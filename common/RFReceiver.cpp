/*
  RFReceiver

  Copyright (c) 2017 Alex Konshin
*/

#include "RFReceiver.hpp"
#include "../protocols/Protocol.hpp"
#include "Config.hpp"
#include <mutex>

bool RFReceiver::isLibInitialized = false;
RFReceiver* RFReceiver::first = NULL;
std::mutex receivers_chain_mutex;

pthread_mutex_t receiversLock;


RFReceiver::RFReceiver(int gpio) {
  this->gpio = gpio;
  protocols = PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_TX7U|PROTOCOL_HG02832|PROTOCOL_WH2;
  isEnabled = false;
  isDecoderStarted = false;
  stopDecoder = false;
  stopMessageReader = false;
  stopped = false;
#ifdef TEST_DECODING
  inputLogFilePath = NULL;
  inputLogFileStream = NULL;
  waitAfterReading = false;
#elif defined(USE_GPIO_TS)
  fd = -1;
#endif
  timerEvent = 0;

  firstMessage = NULL;
  lastMessagePtr = &firstMessage;

  resetReceiverBuffer();

  // Link this new receiver to the chain
  receivers_chain_mutex.lock();
  if (first == NULL) {
    next = NULL;
  } else {
    next = first;
  }
  first = this;
  receivers_chain_mutex.unlock();
}
RFReceiver::~RFReceiver() {
  stop();
  pthread_mutex_destroy(&messageQueueLock);
}

void RFReceiver::setProtocols(unsigned protocols) {
  this->protocols = protocols;
}

void RFReceiver::initLib() {
#ifdef TEST_DECODING
  if (!isLibInitialized) {
    isLibInitialized = true;
    signal((int) SIGINT, signalHandler);
    signal((int) SIGUSR1, signalHandler);
  }
#elif defined(USE_GPIO_TS)
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
    gpioSetSignalFuncEx(SIGTERM, processCtrlBreak, NULL);
  }
#endif
}

void RFReceiver::closeLib() {
  if (isLibInitialized) {
#if !defined(USE_GPIO_TS) && !defined(TEST_DECODING)
    gpioTerminate();
#endif
    isLibInitialized = false;
  }
}

void RFReceiver::closeAll() {
  do {
    receivers_chain_mutex.lock();
    RFReceiver* p = RFReceiver::first;
    if (p != NULL) {
      RFReceiver* next = p->next;
      p->next = NULL;
      RFReceiver::first = next;
    }
    receivers_chain_mutex.unlock();
    if (p == NULL) break;
    p->stop();
  } while (true);

  closeLib();
}


#if defined(USE_GPIO_TS)||defined(TEST_DECODING)
void RFReceiver::signalHandler(int signum) {

  switch(signum) {
  case SIGUSR1: {
    receivers_chain_mutex.lock();
    RFReceiver* p = first;
    while (p != NULL) {
      p->printDebugStatistics();
      p = p->next;
    }
    receivers_chain_mutex.unlock();
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
    //exit(0);
    break;

  case SIGTERM:
    printf("\nTerminating...\n");
    Log->log("Terminating...");
    RFReceiver::closeAll();
    //exit(0);
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

#ifdef TEST_DECODING
  if (inputLogFileStream != NULL) {
    fclose(inputLogFileStream);
    inputLogFileStream = NULL;
  }
#endif

  RFReceiver** pp = &first;
  receivers_chain_mutex.lock();
  RFReceiver* p = first;
  while (p != NULL && p != this) {
    pp = &(p->next);
    p = p->next;
  }
  if (p == this) {
    *pp = next;
    next = NULL;
    receivers_chain_mutex.unlock();
    stop();
    if (first == NULL) closeLib();
  } else {
    receivers_chain_mutex.unlock();
  }
}

bool RFReceiver::enableReceive() {
  if (!isEnabled) {
    resetReceiverBuffer();
    initLib();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    unsigned long min_sequence_length = 0;
    max_duration = 0;
    min_duration = 0;
    Protocol::setLimits(protocols, min_sequence_length, max_duration, min_duration);
    if ( min_sequence_length==0 ) min_sequence_length = MIN_SEQUENCE_LENGTH;
#pragma GCC diagnostic pop

#ifdef TEST_DECODING
    startDecoder();

#elif defined(USE_GPIO_TS)

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
    rc = ioctl(fd, GPIOTS_IOCTL_SET_MIN_SEQ_LEN, min_sequence_length);
    if (rc != 0) {
      Log->error("Failed to set minimum sequence length to %d.", min_sequence_length);
      exit(2);
    }

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
#ifdef TEST_DECODING
    // do nothing for now

#elif defined(USE_GPIO_TS)
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
  if (stopped) return;
  Log->info("Stopping...");
  stopped = true;
  if (isLibInitialized && isEnabled) {
    pause();
    stopTimer();
    stopMessageReader = true;
    close();
    if (isDecoderStarted) {
      isDecoderStarted = false;
      pthread_mutex_lock(&messageQueueLock);
      pthread_cond_broadcast(&messageReady);
      pthread_mutex_unlock(&messageQueueLock);
    }
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

#ifdef TEST_DECODING
void RFReceiver::setWaitAfterReading(bool waitAfterReading) {
  this->waitAfterReading = waitAfterReading;
}

void RFReceiver::setInputLogFile(const char* inputLogFilePath) {
  if (inputLogFilePath == NULL) {
    Log->error("Input log file is not specified.");
    exit(1);
  }
  inputLogFileStream = fopen(inputLogFilePath, "r");
  if (inputLogFileStream == NULL) {
    Log->error("Cannot open input log file \"%s\".", inputLogFilePath);
    exit(1);
  }
  this->inputLogFilePath = inputLogFilePath;
}

int RFReceiver::readSequences() {
  char* line = NULL;
  size_t bufsize = 0;
  ssize_t bytesread;
  while (!stopDecoder && inputLogFileStream != NULL && iSequenceReady == iSequenceWrite ) {
    usleep(500000);
    while ((bytesread = getline(&line, &bufsize, inputLogFileStream)) != -1) {
      size_t len = strlen(line);
      if (len<46 || strncmp(line+23," sequence size=",15)!=0) continue;

      const char* p = line+38;
      while (*p!=' ' && *p!='\0' && *p!='\n') p++;
      if (*p!=' ') continue;

      Log->info("input: %s", p);

      int16_t duration;
      while ((duration=readInt(p)) != -1) {
        int nextIndex = (iPoolWrite+1)&(POOL_SIZE-1);

        if (iPoolRead != nextIndex) { // checking free space in pool
          pool[iPoolWrite] = duration;
          iPoolWrite = nextIndex;
          if (++iCurrentSequenceSize < MAX_SEQUENCE_LENGTH) continue; // done with updating the current sequence
          // the current sequence is already too long
        }
      }
      //DBG("readSequences() => iCurrentSequenceSize=%d iPoolRead=%d iPoolWrite=%d", iCurrentSequenceSize, iPoolRead, iPoolWrite );

      // End of sequence
      if (iCurrentSequenceSize == 0) continue;

      if (iCurrentSequenceSize<MIN_SEQUENCE_LENGTH) {
        //DBG("readSequences() => dropped");
/* debug
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
*/

        // drop the current sequence because it is too short
        statistics->dropped++;
        iPoolWrite = iCurrentSequenceStart;

      } else {
        int nextSequenceIndex = (iSequenceWrite + 1)&(MAX_CHAINS-1);

        if (iSequenceReady == nextSequenceIndex) {
          //DBG("readSequences() => sequence_pool_overflow %d", sequence_pool_overflow);
          // overflow
          statistics->sequence_pool_overflow++;
          return 1;
        }

        // Put new sequence into queue

        iSequenceSize[iSequenceWrite] = (int16_t)iCurrentSequenceSize;
        iSequenceStart[iSequenceWrite] = (int16_t)iCurrentSequenceStart;
        uSequenceStartTime[iSequenceWrite] = uCurrentSequenceStartTime;

        iSequenceWrite = nextSequenceIndex;

        iCurrentSequenceStart = iPoolWrite;
        statistics->sequences++;
        //DBG("readSequences() => put sequence in queue sequences=%d", sequences);
      }

      iCurrentSequenceSize = 0;
      break;
    }

    if (bytesread == -1) {
      if (inputLogFileStream != NULL) {
        fclose(inputLogFileStream);
        inputLogFileStream = NULL;
      }
      Log->info("Finished reading input log file.");
      if (!waitAfterReading) return -1;
      usleep(500000);
      break;
    }

  }

  if (line != NULL) free(line);

  return 1;
}

#elif defined(USE_GPIO_TS)

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
        statistics->dropped++;

        iPoolWrite = iCurrentSequenceStart;

        iCurrentSequenceSize = 0;
        continue;
      }

      int nextSequenceIndex = (iSequenceWrite + 1)&(MAX_CHAINS-1);

      if (iSequenceReady == nextSequenceIndex) {
        //DBG("readSequences() => sequence_pool_overflow %d", sequence_pool_overflow);
        // overflow
        statistics->sequence_pool_overflow++;
        return 1;
      }

      // Put new sequence into queue

      iSequenceSize[iSequenceWrite] = (int16_t)iCurrentSequenceSize;
      iSequenceStart[iSequenceWrite] = (int16_t)iCurrentSequenceStart;
      uSequenceStartTime[iSequenceWrite] = uCurrentSequenceStartTime;

      iSequenceWrite = nextSequenceIndex;

      iCurrentSequenceStart = iPoolWrite;
      iCurrentSequenceSize = 0;
      statistics->sequences++;
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
  statistics->interrupted++;

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
    statistics->corrected++;

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
      statistics->skipped++;
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
    statistics->dropped++;

    iPoolWrite = iCurrentSequenceStart;

    iCurrentSequenceSize = 0;
    return;
  }

  int nextSequenceIndex = (iSequenceWrite + 1)&(MAX_CHAINS-1);

  if ((iSequenceReady&(MAX_CHAINS-1)) == nextSequenceIndex) {
    // overflow
    statistics->sequence_pool_overflow++;
    return;
  }

  // Put new sequence into queue

  iSequenceSize[iSequenceWrite] = (int16_t)iCurrentSequenceSize;
  iSequenceStart[iSequenceWrite] = (int16_t)iCurrentSequenceStart;
  uSequenceStartTime[iSequenceWrite] = uCurrentSequenceStartTime;

//  sequenceIsReady(nextSequenceIndex);
  iSequenceWrite = nextSequenceIndex;

  iCurrentSequenceSize = 0;
  statistics->sequences++;

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
#if defined(USE_GPIO_TS) || defined(TEST_DECODING)

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

    message->sensorData.protocol = 0;
    message->decodedBits = 0;
    message->decodingStatus = 0x8000;
    memset(message->detailedDecodingStatus, 0x8000, sizeof(uint16_t)*NUMBER_OF_PROTOCOLS);
    memset(message->detailedDecodedBits, 0x8000, sizeof(uint16_t)*NUMBER_OF_PROTOCOLS);

    bool decoded = false;
    for (int protocol_index = 0; protocol_index<NUMBER_OF_PROTOCOLS; protocol_index++) {
      Protocol* protocol = Protocol::protocols[protocol_index];
      if (protocol != NULL && (protocol->protocol_bit&protocols) != 0) {
        decoded = protocol->decode(message);
        message->detailedDecodingStatus[protocol_index] = message->decodingStatus;
        message->detailedDecodedBits[protocol_index] = message->decodedBits;
        if (decoded) break;
      }
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
  message->sensorData.protocol = 0;
  message->sensorData.def = NULL;
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
#ifdef TEST_DECODING

#elif defined(USE_GPIO_TS)
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
#ifdef TEST_DECODING
  Log->info("statistics(%d): sequences=%ld dropped=%ld overflow=%ld\n",
      gpio, statistics->sequences, statistics->dropped, statistics->sequence_pool_overflow);

#elif defined(USE_GPIO_TS)
  Log->info("statistics(%d): sequences=%ld dropped=%ld overflow=%ld\n",
      gpio, statistics->sequences, statistics->dropped, statistics->sequence_pool_overflow);
#else
  printf("statistics: sequences=%d skipped=%d dropped=%d corrected=%d overflow=%d\n",
      statistics->sequences, statistics->skipped, statistics->dropped, statistics->corrected, statistics->sequence_pool_overflow);
#endif
}
void RFReceiver::printDebugStatistics() {
#ifdef TEST_DECODING

#elif defined(USE_GPIO_TS)
  long irq_data_overflow_counter = ioctl(fd, GPIOTS_IOCTL_GET_IRQ_OVERFLOW_CNT);
  long buffer_overflow_counter = ioctl(fd, GPIOTS_IOCTL_GET_BUF_OVERFLOW_CNT);
  long isr_counter = ioctl(fd, GPIOTS_IOCTL_GET_ISR_CNT);

  Log->info("statistics(%d): sequences=%ld dropped=%ld overflow=%ld irq_data_overflow_counter=%ld buffer_overflow_counter=%ld isr_counter=%ld\n",
      gpio, statistics->sequences, statistics->dropped, statistics->sequence_pool_overflow, irq_data_overflow_counter, buffer_overflow_counter, isr_counter);
#else
  printf("statistics: interrupted=%d sequences=%d skipped=%d dropped=%d corrected=%d overflow=%d\n",
      statistics->interrupted, statistics->sequences, statistics->skipped, statistics->dropped, statistics->corrected, statistics->sequence_pool_overflow);
#endif
}
