/*
  Receiver

  Copyright (c) 2017 Alex Konshin
*/
#ifndef _Receiver_h
#define _Receiver_h

#define RaspberryPi

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>

#ifdef TEST_DECODING
#include <unistd.h>
#elif defined(USE_GPIO_TS)
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "gpio-ts.h"
#else
//#include <wiringPi.h>
#include <pigpio.h>
#endif

#include "../utils/Logger.hpp"
#include "../utils/Bits.hpp"
#include "ReceivedMessage.hpp"

#define POOL_SIZE 4096
#define MAX_CHAINS 64
#define MIN_SEQUENCE_LENGTH 85
#define MAX_SEQUENCE_LENGTH 400
#define MANCHESTER_BUFFER_SIZE 25

// Noise filter
#define IGNORABLE_SKIP 60
#define MAX_IGNORD_SKIPS 2


#ifdef BPiM3
#include "../mach/bpi-m3.h"
#endif

#ifdef ODROIDC2
#include "../mach/odroid-c2.h"
#endif

#ifdef RPI
#include "../mach/rpi3.h"
#endif

#ifdef __x86_64__
#include "../mach/x86_64.h"
#endif

#ifndef MAX_GPIO
#define MAX_GPIO 53
#endif
#ifndef DEFAULT_PIN
#define DEFAULT_PIN 27
#endif

class Receiver {

public:
  Receiver(Config* cfg);
  ~Receiver();

  bool enableReceive();
  void disableReceive();
  void stop();
  void pause();
  bool isStopped();

  bool available();
  bool waitForMessage(ReceivedMessage& message);

  bool checkAndResetTimerEvent();
  void printStatistics();
  void printStatisticsPeriodically(uint32_t millis);
  void printDebugStatistics();

  void setProtocols(unsigned protocols);

  void setLogger(Logger logger);
#ifdef TEST_DECODING
  void setInputLogFile(const char* inputLogFilePath);
  void setWaitAfterReading(bool waitAfterReading);
#endif
  bool printManchesterBits(ReceivedMessage& message, FILE* file);

private:
  static void initLib();
  static void closeLib();
  static void closeAll();
#if defined(USE_GPIO_TS)||defined(TEST_DECODING)
  static void signalHandler(int signum);
#else
  static void processCtrlBreak(int signum, void *userdata);
  static void interruptCallback(int gpio, int level, uint32_t tick, void *userdata);
#endif
  static void* decoderThreadFunction(void *context);
  static void timerHandler(void *context);

  void close();
  void setTimer(uint32_t millis);
  void stopTimer();
  void raiseTimerEvent();

  static void destroyMessage(ReceivedData* message);
  ReceivedData* createNewMessage();

#if defined(USE_GPIO_TS)||defined(TEST_DECODING)
  int readSequences();
#else
  void handleInterrupt(int level, uint32_t tick);
#endif
  void endOfSequence();
  void decoder();
  void startDecoder();
  void initMessageQueue();
  void resetReceiverBuffer();

#ifdef INCLUDE_POLLSTER
  static void* pollsterThreadFunction(void *context);
  void startPollster();
  void pollster();
  void pollW1();
#endif

  void addBit(bool bit);

  Receiver* next;
  static Receiver* first;
  static bool isLibInitialized;

  Config* cfg;
  uint32_t nLastTime;
  int gpio;
  int lastLevel;

  uint32_t protocols;
  unsigned long min_sequence_length;
  unsigned long min_duration;
  unsigned long max_duration;

#ifdef TEST_DECODING
  const char* inputLogFilePath;
  FILE* inputLogFileStream;
#elif defined(USE_GPIO_TS)
  int fd; // gpiots file
#else
  int nNoiseFilterCounter;
  uint32_t nLastGoodTime;
#endif

  uint32_t uCurrentStatisticsTimer;

  uint32_t manchester[MANCHESTER_BUFFER_SIZE];

  volatile int iPoolWrite;
  volatile int iPoolRead;
  volatile int iSequenceWrite;
  volatile int iSequenceReady;
  volatile int iCurrentSequenceStart;
  volatile int iCurrentSequenceSize;
  uint32_t uCurrentSequenceStartTime;

  // cyclic buffer for durations
  volatile int16_t pool[POOL_SIZE];

  // cyclic buffer for sequences
  volatile uint32_t uSequenceStartTime[MAX_CHAINS];
  volatile int16_t iSequenceStart[MAX_CHAINS];
  volatile int16_t iSequenceSize[MAX_CHAINS];

  // output queue
  pthread_mutex_t messageQueueLock;
  pthread_cond_t messageReady;
#ifdef INCLUDE_POLLSTER
  pthread_mutex_t pollsterLock;
  pthread_cond_t pollsterCondition;
#endif

  ReceivedData* firstMessage;
  ReceivedData** lastMessagePtr;

  //pthread_mutex_t sequencePoolLock;
  //pthread_cond_t sequenceReadyForDecoding;

  pthread_t decoderThreadId;

#ifdef INCLUDE_POLLSTER
  pthread_t pollsterThreadId;

  char* line_buffer = NULL;
  size_t line_buffer_len = 0;

  bool isPollsterInitialized;
  bool isPollsterStarted;
  volatile bool stopPollster;
#endif

  // control

  bool isEnabled;
  bool isPollsterEnabled;
  bool isDecoderStarted;
  bool isMessageQueueInitialized;
  bool stopDecoder;
  bool stopMessageReader;
  volatile bool stopped;
#ifdef TEST_DECODING
  bool waitAfterReading;
#endif
  int timerEvent;
};

#endif
