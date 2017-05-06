/*
  RFReceiver

  Copyright (c) 2017 Alex Konshin
*/

#include "RFReceiver.hpp"

bool RFReceiver::isLibInitialized = false;
RFReceiver* RFReceiver::first = NULL;

RFReceiver::RFReceiver(int gpio) {
  this->gpio = gpio;
  protocols = PROTOCOL_F007TH|PROTOCOL_00592TXR;
  isEnabled = false;
  isDecoderStarted = false;
  stopDecoder = false;
  stopMessageReader = false;
  stopped = false;
#ifdef USE_GPIO_TS
  fd = -1;
#else
  interrupted = 0;
  skipped = 0;
  corrected = 0;
#endif
  timerEvent = 0;
  sequences = 0;
  dropped = 0;
  sequence_pool_overflow = 0;
  bad_manchester = 0;
  manchester_OOS = 0;

  firstMessage = NULL;
  lastMessagePtr = &firstMessage;

  resetReceiverBuffer();

  //TODO use atomic CAS
  if (first == NULL) {
    next = NULL;
  } else {
    next = first;
  }
  first = this;
}
RFReceiver::~RFReceiver() {
  stop();
  pthread_mutex_destroy(&messageQueueLock);
}

void RFReceiver::setProtocols(unsigned protocols) {
  this->protocols = protocols;
}

void RFReceiver::initLib() {
#ifdef USE_GPIO_TS
  if (!isLibInitialized) {
    Log->log("RFReceiver " RF_RECEIVER_VERSION " (with gpio-ts kernel module).");
    if( access( "/sys/class/gpio-ts", F_OK|R_OK|X_OK ) == -1 ) {
      Log->error("Kernel module gpio-ts is not loaded.");
      exit(1);
    }

    isLibInitialized = true;
    signal((int) SIGINT, signalHandler);
    signal((int) SIGUSR1, signalHandler);
  }
#else
  if (!isLibInitialized) {
    Log->log("RFReceiver " RF_RECEIVER_VERSION " (with pigpio library).");
    //unsigned version = gpioVersion();
    //printf("pigpio version is %d.\n", version);
    //gpioCfgMemAlloc(PI_MEM_ALLOC_PAGEMAP);
    if (gpioInitialise() < 0) {
      Log->error("Failed to initialize library pigpio.");
      exit(1);
    }
    isLibInitialized = true;
    gpioSetSignalFuncEx(SIGINT, processCtrlBreak, NULL);
  }
#endif
}

void RFReceiver::closeLib() {
  if (isLibInitialized) {
#ifndef USE_GPIO_TS
    gpioTerminate();
#endif
    isLibInitialized = false;
  }
}

void RFReceiver::closeAll() {
  // TODO use atomic CAS?
  RFReceiver* p = first;
  first = NULL;
  while ( p!=NULL ) {
    p->stop();
    p = p->next;
  }
  closeLib();
}

#ifdef USE_GPIO_TS
void RFReceiver::signalHandler(int signum) {

  switch(signum) {
  case SIGUSR1: {
    RFReceiver* p = first;
    while (p != NULL) {
        p->printDebugStatistics();
        p = p->next;
    }
    break;
  }

  case SIGUSR2:
     // TODO debug?
    break;

  case SIGPIPE:
  case SIGWINCH:
    break;

  case SIGCHLD:
    /* Used to notify threads of events */
    break;

  case SIGINT:
    printf("\nGot Ctrl-C. Terminating...\n");
    Log->log("Got Ctrl-C. Terminating...");
    RFReceiver::closeAll();
    exit(0);
    break;

  default:
    printf("\nUnhandled signal %d. Terminating...\n", signum);
    Log->log("Unhandled signal %d. Terminating...", signum);
    RFReceiver::closeAll();
    exit(1);
  }
}
#else
// Handle Ctrl-C
void RFReceiver::processCtrlBreak(int signum, void *userdata) {
  //RFReceiver* receiver = (RFReceiver*)userdata;
  //receiver->stop();
  printf("\nGot Ctrl-C. Terminating...\n");
  RFReceiver::closeAll();
  exit(0);
}
#endif

void RFReceiver::close() {
  disableReceive();

  // TODO use atomic CAS
  RFReceiver** pp = &first;
  RFReceiver* p = first;
  while (p != NULL && p != this) {
      pp = &(p->next);
      p = p->next;
  }
  if (p == this) {
    *pp = next;
    next = NULL;
    stop();
    if (first == NULL) closeLib();
  }
}

bool RFReceiver::enableReceive() {
  if (!isEnabled) {
    resetReceiverBuffer();
    initLib();

    if ((protocols & PROTOCOL_00592TXR) != 0) {
      min_duration = MIN_DURATION_00592TXR;
    } else {
      min_duration = MIN_DURATION_F007TH;
    }
    if ((protocols & PROTOCOL_F007TH) != 0) {
      max_duration = MAX_DURATION_F007TH;
    } else {
      max_duration = MAX_DURATION_00592TXR;
    }

#ifdef USE_GPIO_TS

    char filepath[40];
//    sprintf(filepath, "/sys/class/gpio-ts/gpiots%d", gpio);
    snprintf(filepath, 40, "/dev/gpiots%d", gpio);
    //DBG("enableReceive() opening \"%s\"...", filepath);
    if ((fd = open(filepath, O_RDONLY|O_NONBLOCK)) < 0) {
      Log->error("Failed to open gpio-ts file \"%s\": %s.", filepath, strerror(errno));
      exit(2);
    }

    // setup noise filter

    long rc = ioctl(fd, GPIOTS_IOCTL_SET_MAX_DURATION, max_duration);
    if (rc != 0) {
      Log->error("Failed to set maximum duration to %ld.", max_duration);
      exit(2);
    }
    rc = ioctl(fd, GPIOTS_IOCTL_SET_MIN_DURATION, min_duration);
    if (rc != 0) {
      Log->error("Failed to set minimum duration to %ld.", min_duration);
      exit(2);
    }
    rc = ioctl(fd, GPIOTS_IOCTL_SET_MIN_SEQ_LEN, MIN_SEQUENCE_LENGTH);
    if (rc != 0) {
      Log->error("Failed to set minimum sequence length to %d.", MIN_SEQUENCE_LENGTH);
      exit(2);
    }
    ioctl(fd, GPIOTS_IOCTL_SET_MIN_SEQ_LEN, MIN_SEQUENCE_LENGTH);

    startDecoder();

#else
    startDecoder();
    gpioSetMode(gpio, PI_INPUT);
    //int rc = gpioSetAlertFuncEx(gpio, interruptCallback, (void *)this);
    // see http://abyz.co.uk/rpi/pigpio/cif.html#gpioSetISRFuncEx
    int rc = gpioSetISRFuncEx(gpio, EITHER_EDGE, 0, interruptCallback, (void*)this);
    if (rc < 0) {
      Log->error("Error code %d from gpioSetISRFuncEx().\n", rc);
      exit(2);
    }
#endif
    isEnabled = true;
  }
  return true;
}

void RFReceiver::disableReceive() {
  if (isEnabled) {
#ifdef USE_GPIO_TS
    int fd = this->fd;
    fd = -1;
    if (fd != -1) {
      if (::close(fd) != 0) {
        Log->warning("Failed to close gpio-ts file: %s.", strerror(errno));
      }
      // TODO close
    }

#else
    //gpioSetAlertFuncEx(gpio, NULL, (void *)this);
    gpioSetISRFuncEx(gpio, EITHER_EDGE, 0, NULL, NULL);
#endif
    isEnabled = false;
  }
}

void RFReceiver::stop() {
  stopped = true;
  if (isLibInitialized && isEnabled) {
    pause();
    stopTimer();
    stopMessageReader = true;
    if (isDecoderStarted) {
/* FIXME
      pthread_mutex_lock(&messageQueueLock);
      pthread_cond_broadcast(&messageReady);
      pthread_mutex_unlock(&messageQueueLock);
*/
    }
    close();
  }
}

bool RFReceiver::isStopped() {
  return stopped;
}

void RFReceiver::pause() {
  if (isEnabled) disableReceive();
  stopDecoder = true;
}

void RFReceiver::resetReceiverBuffer() {
  iPoolWrite = 0;
  iPoolRead = 0;
  iSequenceWrite = 0;
  iSequenceReady = 0;
  iCurrentSequenceStart = 0;
  iCurrentSequenceSize = 0;
  nLastTime = 0;
  lastLevel = -1;
}

#ifdef USE_GPIO_TS

#define N_ITEMS 512


// returns: 1 - sequence is ready, 0 - timeout, (-1) - error reading
int RFReceiver::readSequences() {
  struct timeval timeout;
  fd_set fdset;
  int rv;

  uint32_t buffer[N_ITEMS];
  int length = N_ITEMS*sizeof(uint32_t);

  //DBG("readSequences() iSequenceReady=%d iSequenceWrite=%d", iSequenceReady, iSequenceWrite);

  while (!stopDecoder && iSequenceReady == iSequenceWrite ) {
    FD_ZERO(&fdset);    // clear the set
    FD_SET(fd, &fdset); // add our file descriptor to the set
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    rv = select(fd+1, &fdset, NULL, NULL, &timeout);
    //DBG("select() => rv=%d", rv);
    if (rv <= 0) {
      //if (rv == 0) return 0; // timeout
      if (rv == -1) Log->error("Failed call select: %s.", strerror(errno));
      return rv;
    }

    int bytes_read = read( fd, buffer, length );
    //DBG("read() => bytes_read=%d", bytes_read);
    if (bytes_read <= 0) {
      if (bytes_read == 0) continue;
      int err = errno;
      if (errno == EAGAIN) continue;
      Log->error("Failed read: %s.", strerror(err));
      return -1;
    }

    // process data
    int n_items = bytes_read>>2;
    //DBG("readSequences() n_items=%d", n_items);

    for (int index=0; index<n_items; index++) {
      uint32_t item = buffer[index];
      int status = (int)item_to_status(item);
      if ((status & ~1) == 0) {
        int16_t duration = (int16_t)item_to_duration(item);
        int nextIndex = (iPoolWrite+1)&(POOL_SIZE-1);

        if (iPoolRead != nextIndex) { // checking free space in pool
          pool[iPoolWrite] = duration;
          iPoolWrite = nextIndex;

          if (++iCurrentSequenceSize < MAX_SEQUENCE_LENGTH) continue; // done with updating the current sequence

          // the current sequence is already too long
        }
      }

      // End of sequence

      if (iCurrentSequenceSize<MIN_SEQUENCE_LENGTH) {
/* debug
        if (iCurrentSequenceSize > 0) {
          DBG("readSequences() => dropped %d iPoolRead=%d iPoolWrite=%d item=%d:%lu",
              iCurrentSequenceSize, iPoolRead, iPoolWrite, status, item_to_duration(item));
          for (int i = 0; i < n_items; i++) {
            uint32_t item = buffer[i];
            uint32_t duration = item_to_duration(item);
            status = item_to_status(item);
            if (i > 0) fputs(", ", stdout);
            if (duration == GPIO_TS_MAX_DURATION) {
                printf("%d:MAX", status);
            } else {
                printf("%d:%u", status, duration);
            }
          }
          putchar('\n');
        }
*/

        // drop the current sequence because it is too short
        dropped++;

        iPoolWrite = iCurrentSequenceStart;

        iCurrentSequenceSize = 0;
        continue;
      }

      int nextSequenceIndex = (iSequenceWrite + 1)&(MAX_CHAINS-1);

      if (iSequenceReady == nextSequenceIndex) {
        DBG("readSequences() => sequence_pool_overflow %d", sequence_pool_overflow);
        // overflow
        sequence_pool_overflow++;
        return 1;
      }

      // Put new sequence into queue

      iSequenceSize[iSequenceWrite] = (int16_t)iCurrentSequenceSize;
      iSequenceStart[iSequenceWrite] = (int16_t)iCurrentSequenceStart;
      uSequenceStartTime[iSequenceWrite] = uCurrentSequenceStartTime;

      iSequenceWrite = nextSequenceIndex;

      iCurrentSequenceStart = iPoolWrite;
      iCurrentSequenceSize = 0;
      sequences++;
    }
  }
  return 1;
}

#else // use pigpio

void RFReceiver::interruptCallback(int gpio, int level, uint32_t tick, void* userdata) {
  RFReceiver* receiver = (RFReceiver*)userdata;
  receiver->handleInterrupt(level, tick);
}

void RFReceiver::handleInterrupt(int level, uint32_t time) {
  interrupted++;

  uint32_t duration = time - nLastTime;
  nLastTime = time;

  if (iCurrentSequenceSize == 0) { // it was noise so far
    if (level == lastLevel) {
      lastLevel = level;
      return;
    }
    lastLevel = level;
    if (level == 1) return; // sequence must start with high level
    if (duration <= min_duration) return; // interval is too short

    int nextIndex = (iPoolWrite+1)&(POOL_SIZE-1);
    if (iPoolRead == nextIndex) return;	// no free space in pool

    // Start new sequence

    pool[iPoolWrite] = (int16_t)duration;
    iCurrentSequenceStart = iPoolWrite;
    iPoolWrite = nextIndex;
    iCurrentSequenceSize = 1;
    uCurrentSequenceStartTime = time;
    nNoiseFilterCounter = 0;
    nLastGoodTime = time;
    return;
  }

  int nextIndex = 0;
  int oldLevel = lastLevel;
  lastLevel = level;
  if (nNoiseFilterCounter>0) {

    if ((nNoiseFilterCounter&1) == 1) {
      if (duration > IGNORABLE_SKIP) goto end_of_sequence; // noise signal is too long to be ignored

      if (level == oldLevel) {
        // skipped interrupt => very short spike then up again => 2 fronts
        if ( (nNoiseFilterCounter += 2) > MAX_IGNORD_SKIPS*2 ) goto end_of_sequence;
      } else {
        // end of short spike
        if (++nNoiseFilterCounter > MAX_IGNORD_SKIPS*2 ) goto end_of_sequence;
      }

      uint32_t corrected_duration = time - nLastGoodTime;
      if (corrected_duration > MAX_PERIOD) goto end_of_sequence; // corrected duration is too long
      return; // continue filtering
    }

    if (duration < IGNORABLE_SKIP || level == oldLevel) goto end_of_sequence; // very close spikes => noise

    // odd front

    uint32_t corrected_duration = time - nLastGoodTime;
    if (corrected_duration > MAX_PERIOD) goto end_of_sequence; // corrected duration is too long
    if (corrected_duration < min_duration) {
      if (++nNoiseFilterCounter > MAX_IGNORD_SKIPS*2 ) goto end_of_sequence;
      return; // continue filtering
    }

    // corrected duration is good
    duration = corrected_duration;
    corrected++;

  } else if (duration < min_duration) {

    if (level == 0) {
      if (duration < IGNORABLE_SKIP) {
        // very short spike
        // TODO Theoretically we can adjust nLastGoodTime and previous interval
        // but decrementing of iPoolWrite is not safe
      }
      // short spike
      goto end_of_sequence;
    }

    // up front

    if (duration < IGNORABLE_SKIP) goto end_of_sequence; // very short down. don't know how to interpret it.

    // Try to filter noise
    nNoiseFilterCounter = 1;
    return;
  }

  nextIndex = (iPoolWrite+1)&(POOL_SIZE-1);
  if (iPoolRead != nextIndex) {	// checking free space in pool
    nNoiseFilterCounter = 0;
    nLastGoodTime = time;

    int oldLevel = lastLevel;
    lastLevel = level;

    if (level != oldLevel) {
      // skipped interruption
      skipped++;
    } else if (duration <= max_duration) {
      // good interval

      pool[iPoolWrite] = (int16_t)duration;
      iPoolWrite = nextIndex;

      if (++iCurrentSequenceSize < MAX_SEQUENCE_LENGTH) return; // done with updating the current sequence

      // the current sequence is already too long
    }
  }

  // End of sequence
  end_of_sequence:

  if (iCurrentSequenceSize<MIN_SEQUENCE_LENGTH) {
    // drop the current sequence because it is too short
    dropped++;

    iPoolWrite = iCurrentSequenceStart;

    iCurrentSequenceSize = 0;
    return;
  }

  int nextSequenceIndex = (iSequenceWrite + 1)&(MAX_CHAINS-1);

  if ((iSequenceReady&(MAX_CHAINS-1)) == nextSequenceIndex) {
    // overflow
    sequence_pool_overflow++;
    return;
  }

  // Put new sequence into queue

  iSequenceSize[iSequenceWrite] = (int16_t)iCurrentSequenceSize;
  iSequenceStart[iSequenceWrite] = (int16_t)iCurrentSequenceStart;
  uSequenceStartTime[iSequenceWrite] = uCurrentSequenceStartTime;

//  sequenceIsReady(nextSequenceIndex);
  iSequenceWrite = nextSequenceIndex;

  iCurrentSequenceSize = 0;
  sequences++;

}
#endif


void* RFReceiver::decoderThreadFunction(void *context) {
     ((RFReceiver*)context)->decoder();
    return NULL;
}

void RFReceiver::startDecoder() {
  //DBG("startDecoder()");
  if (!isDecoderStarted) {
    isDecoderStarted = true;
    //pthread_mutex_init(&sequencePoolLock, NULL);
    //pthread_cond_init(&sequenceReadyForDecoding, NULL);

    pthread_mutex_init(&messageQueueLock, NULL);
    pthread_cond_init(&messageReady, NULL);

    int rc = pthread_create(&decoderThreadId, NULL, decoderThreadFunction, (void*)this);
    if (rc != 0) {
      printf("Error code %d from pthread_create()\n", rc);
      exit(3);
    }
  }
}

void RFReceiver::decoder() {
  Log->log("Decoder thread has been started.\n");
  while (!stopDecoder) {
#ifdef USE_GPIO_TS

    if (iSequenceReady == iSequenceWrite) {
      int rc = readSequences();
      //DBG("readSequences() => rc=%d", rc);
      if (rc <= 0) {
        if (rc < 0) {
          stop();
          break;
        }
        // timeout
        if (uCurrentStatisticsTimer != 0) raiseTimerEvent();
        continue;
      }
    }
#else
    //pthread_mutex_lock(&sequencePoolLock);
    while (!stopDecoder && iSequenceReady == iSequenceWrite) {
      //pthread_cond_wait(&sequenceReadyForDecoding, &sequencePoolLock);
      gpioSleep(PI_TIME_RELATIVE, 0, 500000); // = 0.5 second
    }
    //pthread_mutex_unlock(&sequencePoolLock);
#endif

    ReceivedData* message = createNewMessage();
    if (message == NULL) continue;

    bool decoded = (protocols&PROTOCOL_00592TXR) != 0 && decode00592TXR(message);
    if (!decoded && (protocols&PROTOCOL_F007TH) != 0) {
      uint32_t nF007TH;
      decoded = decodeF007TH(message, nF007TH);
    }

    // TODO do not queue the message if it is not decoded and no need to print undecoded messages.

    // put new message into output queue
    pthread_mutex_lock(&messageQueueLock);
    *lastMessagePtr = message;
    lastMessagePtr = &message->next;
    pthread_cond_broadcast(&messageReady);
    pthread_mutex_unlock(&messageQueueLock);
  }
  Log->log("Decoder stopped");
  isDecoderStarted = false;
}

bool RFReceiver::decodeManchester(ReceivedData* message, Bits& bitSet) {
  int iSequenceSize = message->iSequenceSize;
  int16_t* pSequence = message->pSequence;

  int startIndex = 0;
  int endIndex = 0;
  for ( int index = 0; index<iSequenceSize-MIN_SEQUENCE_LENGTH; index++ ) {
    if ( pSequence[index]<MIN_DURATION_F007TH ) {
      if ((index-startIndex) >= MIN_SEQUENCE_LENGTH) {
        endIndex = index;
        if (decodeManchester(message, startIndex, endIndex, bitSet)) {
          if (bitSet.getSize() >= 56) return true;
        }
      }
      startIndex = (index+2) & ~1u;
    }
  }

  if ((iSequenceSize-startIndex) >= MIN_SEQUENCE_LENGTH) {
    endIndex = iSequenceSize;
    if (decodeManchester(message, startIndex, endIndex, bitSet)) {
      if (bitSet.getSize() >= 56) return true;
    }
  }

  return false;
}

bool RFReceiver::decodeManchester(ReceivedData* message, int startIndex, int endIndex, Bits& bitSet) {
  int16_t* pSequence = message->pSequence;

  message->decodingStatus = 0;

  // find the start of Manchester frame
  int adjastment = -1;
  for ( int index = 0; index<32; index++ ) {
    if ( pSequence[index+startIndex]>MAX_HALF_DURATION ) {
      if ( index==0 ) {
        // first interval is long
        adjastment = 1;
      } else if ( (index&1)==1 ) {
        // first probe is good
        adjastment = 0;
      } else {
        // first bit is 0
        adjastment = 2;
      }
      break;
    }
  }
  if ( adjastment==-1 ) {
    bad_manchester++;
    message->decodingStatus |= 1;
    return false;
  }

  bitSet.clear();

  // first bits
  int intervalIndex = -1;
  switch(adjastment) {
  case 0: // good
    bitSet.addBit( true );
    intervalIndex = 1;
    break;
  case 1: // first interval is long
    bitSet.addBit( false );
    bitSet.addBit( true );
    intervalIndex = 1;
    break;
  case 2: // first bit is 0
    bitSet.addBit( false );
    intervalIndex = 0;
    break;
  }

  int iSubSequenceSize = endIndex-startIndex;
  // decoding
  do {
    int interval = pSequence[startIndex+intervalIndex];
    bool half = interval<MAX_HALF_DURATION;
    bitSet.addBit( ((intervalIndex&1)==0) != half ); // level = (intervalIndex&1)==0

    if ( half ) {
      if ( ++intervalIndex>=iSubSequenceSize ) return true;
      interval = pSequence[startIndex+intervalIndex];
      if ( interval>=MAX_HALF_DURATION ) {
        manchester_OOS++;
        //Log->info( "    Bad sequence at index %d: %d.", intervalIndex, interval );
        message->decodingStatus |= 2;
        return true;
      }
    }

  } while ( ++intervalIndex<(iSubSequenceSize-1) );

  return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
bool RFReceiver::decodeF007TH(ReceivedData* message, uint32_t& nF007TH) {
  if (message->sensorData.fields.protocol != 0) {
    if (message->sensorData.fields.protocol == PROTOCOL_F007TH) {
      nF007TH = message->sensorData.nF007TH;
      return true;
    }
    return false;
  }
  nF007TH = 0;

  Bits bits(message->iSequenceSize+1);

  if (!decodeManchester(message, bits)) {
    message->decodingStatus |= 4;
    return false;
  }
  int size = bits.getSize();
  message->decodedBits = (uint16_t)size;
  if (size < 56) {
    message->decodingStatus |= 8;
    return false;
  }

  int index = bits.findBits( 0x0000fd45, 16 ); // 1111 1101 0100 0101 shortened preamble + fixed ID (0x45)
  if (index<0) {
    message->decodingStatus |= 16; // could not find preamble
    return false;
  }

  int dataIndex; // index of the first bit of data after preamble and fixed ID (0x45)

  if (index+56<size) {
    dataIndex = index+16;
  } else if (index>49 && bits.getInt(index-9, 9) == 0x1f) {
    // a valid data from previous message before preamble(11 bits) and 4 bits 0000
    dataIndex = index-49;
  } else if (index+48<size) {
    // hash code is missing but it is better than nothing
    dataIndex = index+16;
  } else {
    message->decodingStatus |= 32; // not enough data
    return false;
  }

  if (dataIndex+40>size) {
    message->decodingStatus |= 64; // hash code is missing - cannot check it
  } else {
    // Checking of hash for Ambient Weather F007TH.
    // See https://eclecticmusingsofachaoticmind.wordpress.com/2015/01/21/home-automation-temperature-sensors/

    int bit;
    bool good = false;
    int checking_data = dataIndex-8;
    do {
      int mask = 0x7C;
      int calculated_hash = 0x64;
      for (int i = checking_data; i < checking_data+40; ++i) {
        bit = mask & 1;
        mask = (((mask&0xff) >> 1 ) | (mask << 7)) & 0xff;
        if ( bit ) mask ^= 0x18;
        if ( bits.getBit(i) ) calculated_hash ^= mask;
      }
      int expected_hash = bits.getInt(checking_data+40, 8);
      if (((expected_hash^calculated_hash) & 255) == 0) {
        good = true;
        dataIndex = checking_data+8;
        break;
      }
      // Bad hash. But message is repeated up to 3 times. Try the next message.
      checking_data += 65;
    } while(checking_data+48 < size && bits.getInt(checking_data-17, 25) == 0x1ffd45);
    if (!good) {
      message->decodingStatus |= 128; // failed hash code check
    }
  }

  uint32_t data = bits.getInt(dataIndex, 32);
  message->sensorData.nF007TH = data;
  message->sensorData.fields.protocol = PROTOCOL_F007TH;
  nF007TH = data;
  return true;
}
#pragma GCC diagnostic pop

bool RFReceiver::decode00592TXR(ReceivedData* message) {
  int iSequenceSize = message->iSequenceSize;
  if (iSequenceSize < 121 || iSequenceSize > 200) {
    message->decodingStatus |= 8;
    return false;
  }

  int16_t* pSequence = message->pSequence;

  // check sync sequence. Expecting 8 items with values around 600

  int dataStartIndex = -1;

  for ( int index = 0; index<=(iSequenceSize-121); index+=2 ) {
    bool good = true;
    for ( int i = 0; i<8; i++) {
      int16_t item = pSequence[index+i];
      if (item < 520 || item > 700) {
        good = false;
        break;
      }
    }
    if (good) { // expected around 200 or 400
      int16_t item = pSequence[index+8];
      if (item > 460 || item < 140) {
        good = false;
        continue;
      }
      dataStartIndex = index+8;
      break;
    }
  }
  if (dataStartIndex < 0) {
    //printf("decode00592TXR(): bad sync sequence\n");
    message->decodingStatus |= 16;
    return false;
  }

  //printf("decode00592TXR(): detected sync sequence\n");

  Bits bits(56);

  for ( int index = dataStartIndex; index<iSequenceSize-1; index+=2 ) {
    int16_t item1 = pSequence[index];
    if (item1 < 120 || item1 > 680) {
      message->decodingStatus |= 4;
      return false;
    }
    int16_t item2 = pSequence[index+1];
    if (item2 < 120 || item2 > 680) {
      message->decodingStatus |= 4;
      return false;
    }
    if (item1 < 280 && item2 > 320) {
      bits.addBit(0);
    } else if (item2 < 280 && item1>320) {
      bits.addBit(1);
    } else {
      //printf("decode00592TXR(): bad items %d: %d %d\n", index, item1, item2);
      message->decodingStatus |= 4;
      return false;
    }
  }
  if (bits.getSize() != 56) {
    //printf("decode00592TXR(): bits.getSize() != 56\n");
    message->decodingStatus |= 32;
    return false;
  }

  unsigned status = bits.getInt(16,8)&255;
  if (status != 0x0044u && status != 0x0084) {
    ///printf("decode00592TXR(): bad status byte 0x%02x\n",status);
    message->decodingStatus |= 128;
    return false;
  }

  unsigned checksum = 0;
  for (int i = 0; i < 48; i+=8) {
    checksum += bits.getInt(i, 8);
  }

  if (((bits.getInt(48,8)^checksum)&255) != 0) {
    //printf("decode00592TXR(): bad checksum\n");
    message->decodingStatus |= 128;
    return false;
  }

  uint64_t n00592TXR = bits.getInt64(0, 56);

  message->sensorData.u64 = n00592TXR;
  message->sensorData.fields.protocol = PROTOCOL_00592TXR;

  return true;
}


ReceivedData* RFReceiver::createNewMessage() {
  if (iSequenceReady == iSequenceWrite) return NULL;

  int index = iSequenceReady;
  int iCurrentSequenceSize = iSequenceSize[index];
  int iCurrentSequenceStart = iSequenceStart[index];
  uint32_t uCurrentSequenceStartTime = uSequenceStartTime[index];
  iSequenceReady = (iSequenceReady + 1)&(MAX_CHAINS-1);

  void* ptr = malloc(sizeof(ReceivedData) + iCurrentSequenceSize*sizeof(int16_t));
  ReceivedData* message = (ReceivedData*)ptr;
  message->iSequenceSize = iCurrentSequenceSize;
  message->uSequenceStartTime = uCurrentSequenceStartTime;

  int16_t* pSequence = (int16_t*)((uint8_t*)ptr + sizeof(ReceivedData));
  message->pSequence = pSequence;

  // copy the sequence into message
  int i_end = iCurrentSequenceStart + iCurrentSequenceSize;
  if (i_end <= POOL_SIZE) {
    memcpy(pSequence, (const void*)&pool[iCurrentSequenceStart], iCurrentSequenceSize*sizeof(int16_t));
  } else {
    int chunk1_size = POOL_SIZE-iCurrentSequenceStart;
    memcpy(pSequence, (const void*)&pool[iCurrentSequenceStart], chunk1_size*sizeof(int16_t));
    memcpy(pSequence+chunk1_size, (const void*)&pool[0], (iCurrentSequenceSize-chunk1_size)*sizeof(int16_t));
  }

  iPoolRead = (iCurrentSequenceStart+iCurrentSequenceSize)&(POOL_SIZE-1);

  message->sensorData.u64 = 0LL;
  message->decodingStatus = 0;
  message->decodedBits = 0;

  message->next = NULL;

  return message;
}

void RFReceiver::destroyMessage(ReceivedData* message) {
  free((void*)message);
}

bool RFReceiver::waitForMessage(ReceivedMessage& message) {
  ReceivedData* data = NULL;

  pthread_mutex_lock(&messageQueueLock);
  while (!stopped && !stopMessageReader && timerEvent==0 && (data = firstMessage) == NULL) {
    pthread_cond_wait(&messageReady, &messageQueueLock);
  }
  if (data != NULL) {
    firstMessage = data->next;
    if (firstMessage == NULL) lastMessagePtr = &firstMessage;
  }
  pthread_mutex_unlock(&messageQueueLock);
  if (data != NULL) data->next = NULL;
  message.setData(data);
  return data != NULL;
}

bool RFReceiver::available() {
  return firstMessage != NULL;
}

void RFReceiver::timerHandler(void *context) {
  RFReceiver* receiver = (RFReceiver*)context;
  receiver->raiseTimerEvent();
}

void RFReceiver::raiseTimerEvent() {
  timerEvent = 1;
  pthread_mutex_lock(&messageQueueLock);
  pthread_cond_broadcast(&messageReady);
  pthread_mutex_unlock(&messageQueueLock);

  printStatistics();
}

bool RFReceiver::checkAndResetTimerEvent() {
  return __sync_lock_test_and_set(&timerEvent, 0) != 0;
}

void RFReceiver::setTimer(uint32_t millis) {
  if (uCurrentStatisticsTimer == millis) return; // ignore repeated calls

  if (millis != 0) {
    initLib();
  }
  uCurrentStatisticsTimer = millis;
#ifdef USE_GPIO_TS

    //TODO

#else
  int rc = gpioSetTimerFuncEx(1, millis==0 ? 1 : millis, millis == 0 ? NULL : timerHandler, (void*)this);
  if (rc != 0) {
    printf("ERROR: Error code %d from gpioSetTimerFuncEx().\n", rc);
    exit(4);
  }
#endif

  if (millis == 0)
    printf("timer off\n");
  else
    printf("timer on %dms\n", millis);
}

void RFReceiver::stopTimer() {
  setTimer(0);
}

void RFReceiver::printStatisticsPeriodically(uint32_t millis) {
  setTimer(millis);
}

void RFReceiver::printStatistics() {
#ifdef USE_GPIO_TS
  Log->info("statistics(%d): sequences=%ld dropped=%ld overflow=%ld\n",
      gpio, sequences, dropped, sequence_pool_overflow);
#else
    printf("statistics: sequences=%d skipped=%d dropped=%d corrected=%d overflow=%d\n",
        sequences, skipped, dropped, corrected, sequence_pool_overflow);
#endif
}
void RFReceiver::printDebugStatistics() {
#ifdef USE_GPIO_TS
  long irq_data_overflow_counter = ioctl(fd, GPIOTS_IOCTL_GET_IRQ_OVERFLOW_CNT);
  long buffer_overflow_counter = ioctl(fd, GPIOTS_IOCTL_GET_BUF_OVERFLOW_CNT);
  long isr_counter = ioctl(fd, GPIOTS_IOCTL_GET_ISR_CNT);

  Log->info("statistics(%d): sequences=%ld dropped=%ld overflow=%ld irq_data_overflow_counter=%ld buffer_overflow_counter=%ld isr_counter=%ld\n",
      gpio, sequences, dropped, sequence_pool_overflow, irq_data_overflow_counter, buffer_overflow_counter, isr_counter);
#else
    printf("statistics: interrupted=%d sequences=%d skipped=%d dropped=%d corrected=%d overflow=%d\n",
        interrupted, sequences, skipped, dropped, corrected, sequence_pool_overflow);
#endif
}

