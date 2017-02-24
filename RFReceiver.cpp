/*
  RFReceiver

  Copyright (c) 2017 Alex Konshin
*/

#include "RFReceiver.hpp"


bool RFReceiver::isLibInitialized = false;


RFReceiver::RFReceiver(int gpio) {
  this->gpio = gpio;
  isEnabled = false;
  stopDecoder = false;
  stopMessageReader = false;
  stopped = false;

  timerEvent = 0;
  interrupted = 0;
  sequences = 0;
  skipped = 0;
  dropped = 0;
  corrected = 0;
  sequence_pool_overflow = 0;
  bad_manchester = 0;
  manchester_OOS = 0;

  firstMessage = NULL;
  lastMessagePtr = &firstMessage;

  resetReceiverBuffer();
}
RFReceiver::~RFReceiver() {
  stop();
  pthread_mutex_destroy(&messageQueueLock);
}

void RFReceiver::initLib() {
  if (!isLibInitialized) {
    //unsigned version = gpioVersion();
    //printf("pigpio version is %d.\n", version);
    //gpioCfgMemAlloc(PI_MEM_ALLOC_PAGEMAP);
    if (gpioInitialise() < 0) {
      puts("ERROR: Failed to initialize library pigpio.\n");
      exit(1);
    }
  }
  gpioSetSignalFuncEx(SIGINT, processCtrlBreak, (void*)this);
  isLibInitialized = true;
}
void RFReceiver::closeLib() {
  if (isLibInitialized) {
    gpioTerminate();
    isLibInitialized = false;
    isEnabled = false;
  }
}
// Handle Ctrl-C
void RFReceiver::processCtrlBreak(int signum, void *userdata) {
  RFReceiver* receiver = (RFReceiver*)userdata;
  receiver->stop();
}


void RFReceiver::interruptCallback(int gpio, int level, uint32_t tick, void* userdata) {
  RFReceiver* receiver = (RFReceiver*)userdata;
  receiver->handleInterrupt(level, tick);
}

bool RFReceiver::enableReceive() {
  if (!isEnabled) {
    resetReceiverBuffer();
    initLib();
    startDecoder();
    gpioSetMode(gpio, PI_INPUT);
    //int rc = gpioSetAlertFuncEx(gpio, interruptCallback, (void *)this);
    // see http://abyz.co.uk/rpi/pigpio/cif.html#gpioSetISRFuncEx
    int rc = gpioSetISRFuncEx(gpio, EITHER_EDGE, 0, interruptCallback, (void*)this);
    if (rc < 0) {
      printf("ERROR: Error code %d from gpioSetISRFuncEx().\n", rc);
      exit(2);
    }
    isEnabled = true;
  }
  return true;
}

void RFReceiver::disableReceive() {
  if (isEnabled) {
    //gpioSetAlertFuncEx(gpio, NULL, (void *)this);
    gpioSetISRFuncEx(gpio, EITHER_EDGE, 0, NULL, NULL);
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
      pthread_mutex_lock(&messageQueueLock);
      pthread_cond_broadcast(&messageReady);
      pthread_mutex_unlock(&messageQueueLock);
    }
    closeLib();
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
    if (duration <= MIN_DURATION) return; // interval is too short

    int nextIndex = (iPoolWrite+1)&(POOL_SIZE-1); // iPoolWrite = (iPoolWrite+1) % POOL_SIZE
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
    if (corrected_duration < MIN_DURATION) {
      if (++nNoiseFilterCounter > MAX_IGNORD_SKIPS*2 ) goto end_of_sequence;
      return; // continue filtering
    }

    // corrected duration is good
    duration = corrected_duration;
    corrected++;

  } else if (duration < MIN_DURATION) {

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

  nextIndex = (iPoolWrite+1)&(POOL_SIZE-1); // iPoolWrite = (iPoolWrite+1) % POOL_SIZE
  if (iPoolRead != nextIndex) {	// checking free space in pool
    nNoiseFilterCounter = 0;
    nLastGoodTime = time;

    int oldLevel = lastLevel;
    lastLevel = level;

    if (level != oldLevel) {
      // skipped interruption
      skipped++;
    } else if (duration <= MAX_PERIOD) {
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

  // End of sequence

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


/*
void RFReceiver::sequenceIsReady(int nextSequenceIndex) {
  pthread_mutex_lock(&sequencePoolLock);
  iSequenceWrite = nextSequenceIndex;
  pthread_cond_broadcast(&sequenceReadyForDecoding);
  pthread_mutex_unlock(&sequencePoolLock);
}
*/

void* RFReceiver::decoderThreadFunction(void *context) {
     ((RFReceiver*)context)->decoder();
    return NULL;
}

void RFReceiver::startDecoder() {
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
  //printf("started decoder...\n");
  do {
    //pthread_mutex_lock(&sequencePoolLock);
    while (!stopDecoder && iSequenceReady == iSequenceWrite) {
      //pthread_cond_wait(&sequenceReadyForDecoding, &sequencePoolLock);
      gpioSleep(PI_TIME_RELATIVE, 0, 500000); // = 0.5 second
    }
    //pthread_mutex_unlock(&sequencePoolLock);

    while (!stopDecoder && iSequenceReady != iSequenceWrite) {

      ReceivedData* message = createNewMessage();
      uint32_t nF007TH;
      decodeF007TH(message, nF007TH);

      // put new message into output queue
      pthread_mutex_lock(&messageQueueLock);
      *lastMessagePtr = message;
      lastMessagePtr = &message->next;
      pthread_cond_broadcast(&messageReady);
      pthread_mutex_unlock(&messageQueueLock);
    }

  } while (!stopDecoder);
  isDecoderStarted = false;
}

bool RFReceiver::decodeManchester(ReceivedData* message, Bits& bitSet) {
  int iSequenceSize = message->iSequenceSize;
  int16_t* pSequence = message->pSequence;

  // find the start of Manchester frame
  int adjastment = -1;
  for ( int index = 0; index<32; index++ ) {
    if ( pSequence[index]>MAX_HALF_DURATION ) {
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

  // decoding
  do {
    int interval = pSequence[intervalIndex];
    bool half = interval<MAX_HALF_DURATION;
    bitSet.addBit( ((intervalIndex&1)==0) != half ); // level = (intervalIndex&1)==0

    if ( half ) {
      if ( ++intervalIndex>=iSequenceSize ) return true;
      interval = pSequence[intervalIndex];
      if ( interval>=MAX_HALF_DURATION ) {
        manchester_OOS++;
        //Log.info( "    Bad sequence at index %d: %d.", intervalIndex, interval );
        message->decodingStatus |= 2;
        return true;
      }
    }

  } while ( ++intervalIndex<(iSequenceSize-1) );

  return true;
}

bool RFReceiver::decodeF007TH(ReceivedData* message, uint32_t& nF007TH) {
  if (message->nF007TH != 0) {
    nF007TH = message->nF007TH;
    return true;
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
  message->nF007TH = data;
  nF007TH = data;
  return true;
}

ReceivedData* RFReceiver::createNewMessage() {
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
    memcpy(pSequence, &pool[iCurrentSequenceStart], iCurrentSequenceSize*sizeof(int16_t));
  } else {
    int chunk1_size = POOL_SIZE-iCurrentSequenceStart;
    memcpy(pSequence, &pool[iCurrentSequenceStart], chunk1_size*sizeof(int16_t));
    memcpy(pSequence+chunk1_size, &pool[0], (iCurrentSequenceSize-chunk1_size)*sizeof(int16_t));
  }

  iPoolRead = (iPoolRead+iCurrentSequenceSize)&(POOL_SIZE-1);

  message->nF007TH = 0;
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
  receiver->timerEvent = 1;
  pthread_mutex_lock(&receiver->messageQueueLock);
  pthread_cond_broadcast(&receiver->messageReady);
  pthread_mutex_unlock(&receiver->messageQueueLock);

  //receiver->printStatistics();
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

  int rc = gpioSetTimerFuncEx(1, millis==0 ? 1 : millis, millis == 0 ? NULL : timerHandler, (void*)this);
  if (rc != 0) {
    printf("ERROR: Error code %d from gpioSetTimerFuncEx().\n", rc);
    exit(4);
  }

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
    printf("statistics: sequences=%d skipped=%d dropped=%d corrected=%d overflow=%d\n",
        sequences, skipped, dropped, corrected, sequence_pool_overflow);
}
void RFReceiver::printDebugStatistics() {
    printf("statistics: interrupted=%d sequences=%d skipped=%d dropped=%d corrected=%d overflow=%d\n",
        interrupted, sequences, skipped, dropped, corrected, sequence_pool_overflow);
}

