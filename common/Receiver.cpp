/*
  Receiver

  Copyright (c) 2017 Alex Konshin
*/

#include "Receiver.hpp"
#include "../protocols/Protocol.hpp"
#include "Config.hpp"
#include <mutex>

#ifdef INCLUDE_POLLSTER
#include "dirent.h"
#include <sys/types.h>
#endif

bool Receiver::isLibInitialized = false;
Receiver* Receiver::first = NULL;
std::mutex receivers_chain_mutex;

pthread_mutex_t receiversLock;


Receiver::Receiver(Config* cfg) {
  this->cfg = cfg;
  this->gpio = cfg->gpio;
  protocols = cfg->protocols;
  min_sequence_length = cfg->min_sequence_length;
  max_duration = cfg->max_duration;
  min_duration = cfg->min_duration;
  isEnabled = false;
  isMessageQueueInitialized = false;
  isDecoderStarted = false;
  stopDecoder = false;
  stopMessageReader = false;
  stopped = false;
#ifdef INCLUDE_POLLSTER
  isPollsterEnabled = cfg->w1_enable;
  isPollsterStarted = false;
  isPollsterInitialized = false;
  stopPollster = false;
#endif
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
Receiver::~Receiver() {
  stop();
  if (isMessageQueueInitialized) {
    isMessageQueueInitialized = false;
    pthread_mutex_destroy(&messageQueueLock);
  }

#ifdef INCLUDE_POLLSTER
  if (isPollsterInitialized) {
    isPollsterInitialized = false;
    pthread_mutex_destroy(&pollsterLock);
  }
#endif
}

void Receiver::setProtocols(unsigned protocols) {
  this->protocols = protocols;
}

void Receiver::initLib() {
#ifdef TEST_DECODING
  if (!isLibInitialized) {
    isLibInitialized = true;
    signal((int) SIGINT, signalHandler);
    signal((int) SIGUSR1, signalHandler);
  }
#elif defined(USE_GPIO_TS)
  if (!isLibInitialized) {
    Log->log("Receiver " RF_RECEIVER_VERSION " (with gpio-ts kernel module).");
    if ( access("/sys/class/gpio-ts", F_OK|R_OK|X_OK ) == -1) {
      Log->error("Kernel module gpio-ts is not loaded.");
      exit(1);
    }

    isLibInitialized = true;
    signal((int) SIGINT, signalHandler);
    signal((int) SIGUSR1, signalHandler);
  }
#else
  if (!isLibInitialized) {
    Log->log("Receiver " RF_RECEIVER_VERSION " (with pigpio library).");
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

void Receiver::closeLib() {
  if (isLibInitialized) {
#if !defined(USE_GPIO_TS) && !defined(TEST_DECODING)
    gpioTerminate();
#endif
    isLibInitialized = false;
  }
}

void Receiver::closeAll() {
  do {
    receivers_chain_mutex.lock();
    Receiver* p = Receiver::first;
    if (p != NULL) {
      Receiver* next = p->next;
      p->next = NULL;
      Receiver::first = next;
    }
    receivers_chain_mutex.unlock();
    if (p == NULL) break;
    p->stop();
  } while (true);

  closeLib();
}


#if defined(USE_GPIO_TS)||defined(TEST_DECODING)
void Receiver::signalHandler(int signum) {

  switch(signum) {
  case SIGUSR1: {
    receivers_chain_mutex.lock();
    Receiver* p = first;
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
    Receiver::closeAll();
    //exit(0);
    break;

  case SIGTERM:
    printf("\nTerminating...\n");
    Log->log("Terminating...");
    Receiver::closeAll();
    //exit(0);
    break;

  default:
    printf("\nUnhandled signal %d. Terminating...\n", signum);
    Log->log("Unhandled signal %d. Terminating...", signum);
    Receiver::closeAll();
    exit(1);
  }
}
#else
// Handle Ctrl-C
void Receiver::processCtrlBreak(int signum, void *userdata) {
  //Receiver* receiver = (Receiver*)userdata;
  //receiver->stop();
  printf("\nGot Ctrl-C. Terminating...\n");
  Receiver::closeAll();
  exit(0);
}
#endif

void Receiver::close() {
  disableReceive();

#ifdef TEST_DECODING
  if (inputLogFileStream != NULL) {
    fclose(inputLogFileStream);
    inputLogFileStream = NULL;
  }
#endif

  Receiver** pp = &first;
  receivers_chain_mutex.lock();
  Receiver* p = first;
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

bool Receiver::enableReceive() {
  if (!isEnabled) {
    resetReceiverBuffer();
    initLib();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    Protocol::setLimits(protocols, min_sequence_length, max_duration, min_duration);
    if (min_sequence_length <= 16) min_sequence_length = MIN_SEQUENCE_LENGTH;
    if (max_duration > 10000) max_duration = 10000;
    if (min_duration < 30) min_duration = 30;
    else if (min_duration > 8000) min_duration = 8000;
    if (min_duration >= max_duration) {
      max_duration = min_duration+2000;
    }
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

#ifdef INCLUDE_POLLSTER
    if (isPollsterEnabled) startPollster();
#endif
    isEnabled = true;
  }
  return true;
}

void Receiver::disableReceive() {
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

void Receiver::stop() {
  if (stopped) return;
  Log->info("Stopping...");
  stopped = true;
#ifdef INCLUDE_POLLSTER
  stopPollster = true;
  if (isPollsterStarted) {
    pthread_mutex_lock(&pollsterLock);
    pthread_cond_broadcast(&pollsterCondition);
    pthread_mutex_unlock(&pollsterLock);
  }
#endif
  if (isLibInitialized && isEnabled) {
    pause();
    stopTimer();
    stopMessageReader = true;
    close();
    if (isDecoderStarted) {
      isDecoderStarted = false;
      stopDecoder = true;
      // TODO ???
    }
  }
  if (isMessageQueueInitialized) {
    // notify all message processors
    pthread_mutex_lock(&messageQueueLock);
    pthread_cond_broadcast(&messageReady);
    pthread_mutex_unlock(&messageQueueLock);
  }
}

bool Receiver::isStopped() {
  return stopped;
}

void Receiver::pause() {
  if (isEnabled) disableReceive();
  stopDecoder = true;
  // stopPollster = true; // FIXME
}

void Receiver::resetReceiverBuffer() {
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
void Receiver::setWaitAfterReading(bool waitAfterReading) {
  this->waitAfterReading = waitAfterReading;
}

void Receiver::setInputLogFile(const char* inputLogFilePath) {
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

//#define WAIT_BEFORE_NET_READ 500000
#define WAIT_BEFORE_NET_READ 100000

int Receiver::readSequences() {
  char* line = NULL;
  size_t bufsize = 0;
  ssize_t bytesread;
  while (!stopDecoder && inputLogFileStream != NULL && iSequenceReady == iSequenceWrite ) {
    usleep(WAIT_BEFORE_NET_READ);
    while ((bytesread = getline(&line, &bufsize, inputLogFileStream)) != -1) {
      size_t len = strlen(line);
      if (len < 80) continue;
      const char* p = strstr(line, "sequence size=");
      if (p == NULL || (p-line) > 60) continue;
      p += sizeof("sequence size=");
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

      if (iCurrentSequenceSize<(int)min_sequence_length) {
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
      usleep(WAIT_BEFORE_NET_READ);
      break;
    }

  }

  if (line != NULL) free(line);

  return 1;
}

#elif defined(USE_GPIO_TS)

#define N_ITEMS 512

// returns: 1 - sequence is ready, 0 - timeout, (-1) - error reading
int Receiver::readSequences() {
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

      if (iCurrentSequenceSize<(int)min_sequence_length) {
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

void Receiver::interruptCallback(int gpio, int level, uint32_t tick, void* userdata) {
  Receiver* receiver = (Receiver*)userdata;
  receiver->handleInterrupt(level, tick);
}

void Receiver::handleInterrupt(int level, uint32_t time) {
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

  if (iCurrentSequenceSize<(int)min_sequence_length) {
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


void* Receiver::decoderThreadFunction(void *context) {
     ((Receiver*)context)->decoder();
    return NULL;
}

void Receiver::startDecoder() {
  //DBG("startDecoder()");
  if (!isDecoderStarted) {
    isDecoderStarted = true;
    //pthread_mutex_init(&sequencePoolLock, NULL);
    //pthread_cond_init(&sequenceReadyForDecoding, NULL);

    initMessageQueue();

    int rc = pthread_create(&decoderThreadId, NULL, decoderThreadFunction, (void*)this);
    if (rc != 0) {
      printf("Error code %d from pthread_create()\n", rc);
      exit(3);
    }
  }
}

void Receiver::decoder() {
  Log->log("Decoder thread has been started");
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

    bool decoded = false;
    for (int protocol_index = 0; protocol_index<NUMBER_OF_PROTOCOLS; protocol_index++) {
      Protocol* protocol = Protocol::protocols[protocol_index];
      if (protocol != NULL && (protocol->protocol_bit&protocols) != 0 && (protocol->getFeatures(NULL)&FEATURE_RF) != 0) {
        message->decodingStatus = 0;
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


#ifdef INCLUDE_POLLSTER
void* Receiver::pollsterThreadFunction(void *context) {
  ((Receiver*)context)->pollster();
  return NULL;
}

void Receiver::startPollster() {
  //DBG("Receiver::startPollster()");
  if (isPollsterEnabled && !isPollsterStarted) {

    pthread_mutex_init(&pollsterLock, NULL);
    isPollsterInitialized = true;
    pthread_cond_init(&pollsterCondition, NULL);

    initMessageQueue();

    isPollsterStarted = true;
    int rc = pthread_create(&pollsterThreadId, NULL, pollsterThreadFunction, (void*)this);
    if (rc != 0) {
      printf("Error code %d from pthread_create()\n", rc);
      exit(3);
    }
  }
}

// Pollster thread
void Receiver::pollster() {
  Log->log("Pollster thread has been started");

  unsigned waittime = 15; // Poll interval in seconds TODO calculate

  struct timespec timeToWait;

  while (!stopPollster) {
    pollW1();

    clock_gettime(CLOCK_REALTIME, &timeToWait);
    timeToWait.tv_sec += waittime;

    int rc = pthread_cond_timedwait(&pollsterCondition, &pollsterLock, &timeToWait);
    //DBG("Receiver::pollster() pthread_cond_timedwait() => %d", rc);
    if (rc == 0) {
      if (stopPollster || stopped) break;
      continue;
    }
    if (rc != ETIMEDOUT) {
      printf("Error code %d from pthread_cond_timedwait()\n", rc);
      break;
    }
  }
  Log->log("Pollster thread has been stopped");
  isDecoderStarted = false;
}

static uint32_t get_W1_id(const char* p) {
  uint32_t result = 0;
  for(int i=0; i<8; i++) {
    result <<= 4;
    char ch = *p++;
    if (ch >= '0' && ch <= '9') {
      result |= ch-'0';
    } else if (ch >= 'a' && ch <= 'f') {
      result |= ch-'a'+10;
    } else return 0;
  }
  return result;
}


struct ReceivedDataDS18B20 : public ReceivedData {
  char name[16];
};

// Poll DS18B20 devices
void Receiver::pollW1() {

  Protocol* protocol = Protocol::protocols[PROTOCOL_INDEX_DS18B20];
  if (protocol == NULL || (protocol->protocol_bit&protocols) == 0) return;

  ReceivedDataDS18B20* messages = NULL;
  ReceivedDataDS18B20** last_msg_ptr = &messages;

  struct dirent *ep;
  DIR* dirp = opendir(W1_DEVICES_PATH);
  if (dirp != NULL) {
    while ((ep = readdir(dirp)) != NULL) {
      // Ex: 28-000004ce62c7
      const char* name = ep->d_name;
      if (*name == '.') continue;
      size_t namelen = strlen(name);
      if (namelen != 15 || strncmp(name, "28-0000", 7) != 0) continue;
      uint32_t id = get_W1_id(name+7);
      if (id == 0) continue;

      ReceivedDataDS18B20* message = (ReceivedDataDS18B20*)malloc(sizeof(ReceivedDataDS18B20));
      memset((void*)message, 0, sizeof(ReceivedData));
      memcpy(message->name, name, 15*sizeof(char));

      message->sensorData.u32.hi = id;
      message->sensorData.protocol = protocol;
      message->sensorData.def = NULL;
      *last_msg_ptr = message;
      last_msg_ptr = (ReceivedDataDS18B20**)&message->next;
    }
    (void)closedir(dirp);
  }

  if (messages != NULL) {
    char filepath[W1_DEVICES_PATH_LEN+26]; // /sys/bus/w1/devices/28-000004ce62c7/w1_slave
    strcpy(filepath, W1_DEVICES_PATH);
    char* name_p = filepath+strlen(W1_DEVICES_PATH);
    *name_p++ = '/';

    ReceivedDataDS18B20* message = messages;
    while (message != NULL) {
      uint16_t decodingStatus = 1;
      strcpy(name_p, message->name);
      strcpy(name_p+15, "/w1_slave");
        decodingStatus = 1;
      FILE* file = fopen(filepath, "r");
      if (file != NULL) {
        ssize_t bytesread;
        // 3e 01 4b 46 7f ff 02 10 6c : crc=6c YES
        // 3e 01 4b 46 7f ff 02 10 6c t=19875
        decodingStatus = 2;
        if((bytesread=getline(&line_buffer, &line_buffer_len, file)) != -1 && bytesread >= 39 && strncmp(line_buffer+36, "YES", 3) == 0) {
          decodingStatus = 3;
          if((bytesread=getline(&line_buffer, &line_buffer_len, file)) != -1 && bytesread >= 30 && strncmp(line_buffer+27, "t=", 2) == 0) {
            decodingStatus = 4;
            const char* p = line_buffer+29;
            long n = 0;
            char ch = *p++;
            if (ch>='0' && ch<='9') {
              do {
                n = n*10+(ch-'0');
                ch = *p++;
              } while (ch>='0' && ch<='9');
              if (ch == '\0' || ch == '\n' || ch == '\r') {
                message->sensorData.u32.low = (uint32_t)n;
                decodingStatus = 0;
              }
            }
          }
        }
        fclose(file);
      } else {
        message->decodingStatus |= 1;
        //DBG("Receiver::pollW1() Could not open file\"%s\".", filepath);
      }
      message->decodingStatus = decodingStatus;
      message->detailedDecodingStatus[PROTOCOL_INDEX_DS18B20] = decodingStatus;
      message = (ReceivedDataDS18B20*)message->next;
    }

    // Put new messages into output queue
    pthread_mutex_lock(&messageQueueLock);

    *lastMessagePtr = messages;
    lastMessagePtr = (ReceivedData**)last_msg_ptr;

    pthread_cond_broadcast(&messageReady);
    pthread_mutex_unlock(&messageQueueLock);
  }
}

#endif


void Receiver::initMessageQueue() {
  //DBG("Receiver::initMessageQueue");
  if (!isMessageQueueInitialized) {
    pthread_mutex_init(&messageQueueLock, NULL);
    pthread_cond_init(&messageReady, NULL);
    isMessageQueueInitialized = true;
  }
}

ReceivedData* Receiver::createNewMessage() {
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
  message->sensorData.protocol = NULL;
  message->sensorData.def = NULL;
  message->decodingStatus = 0;
  message->protocol_tried_manchester = 0;
  message->decodedBits = 0;
  memset(message->detailedDecodingStatus, 0x8000, sizeof(uint16_t)*NUMBER_OF_PROTOCOLS);
  memset(message->detailedDecodedBits, 0x8000, sizeof(uint16_t)*NUMBER_OF_PROTOCOLS);

  message->next = NULL;

  return message;
}

void Receiver::destroyMessage(ReceivedData* message) {
  free((void*)message);
}

bool Receiver::waitForMessage(ReceivedMessage& message) {
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

bool Receiver::available() {
  return firstMessage != NULL;
}

void Receiver::timerHandler(void *context) {
  Receiver* receiver = (Receiver*)context;
  receiver->raiseTimerEvent();
}

void Receiver::raiseTimerEvent() {
  timerEvent = 1;
  pthread_mutex_lock(&messageQueueLock);
  pthread_cond_broadcast(&messageReady);
  pthread_mutex_unlock(&messageQueueLock);

  if ((cfg->options&VERBOSITY_PRINT_STATISTICS) != 0) printStatistics();
}

bool Receiver::checkAndResetTimerEvent() {
  return __sync_lock_test_and_set(&timerEvent, 0) != 0;
}

void Receiver::setTimer(uint32_t millis) {
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
    Log->info("Stopped timer");
  else
    Log->info("Started timer %dms", millis);
}

void Receiver::stopTimer() {
  setTimer(0);
}

void Receiver::printStatisticsPeriodically(uint32_t millis) {
  setTimer(millis);
}

void Receiver::printStatistics() {
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
void Receiver::printDebugStatistics() {
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
