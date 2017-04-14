/*
  RFReceiver

  Copyright (c) 2017 Alex Konshin
*/
#ifndef _RFReceiver_h
#define _RFReceiver_h

#define RaspberryPi

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#ifdef USE_GPIO_TS
//FIXME
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "gpio-ts.h"
#else
//#include <wiringPi.h>
#include <pigpio.h>
#endif

#include "Bits.hpp"
#include "ReceivedMessage.hpp"

#define POOL_SIZE 4096
#define MAX_CHAINS 64
#define MIN_SEQUENCE_LENGTH 85
#define MAX_SEQUENCE_LENGTH 400
#define MANCHESTER_BUFFER_SIZE 25

// AcuRite 00592TXR - 200...600
#define MIN_DURATION_00592TXR 140
#define MAX_DURATION_00592TXR 660

// Ambient Weather F007TH
#define MIN_DURATION_F007TH 400
#define MAX_DURATION_F007TH 1150
#define MAX_HALF_DURATION 600
#define MIN_PERIOD 900
#define MAX_PERIOD 1150

// Noise filter
#define IGNORABLE_SKIP 60
#define MAX_IGNORD_SKIPS 2


#ifdef BPiM3
#define MAX_GPIO 362
#else
#define MAX_GPIO 53
#endif


#ifndef ASSERT
#ifdef  NDEBUG
#define ASSERT(expr)     ((void)0)
#define DBG(format, arg...)     ((void)0)
#else
#define PRINT_ASSERT(expr,file,line)  fputs("ASSERTION: ("#expr") in "#file" : "#line"\n", stdout)
#define ASSERT(expr)  if(!(expr)) { PRINT_ASSERT(expr,__FILE__,__LINE__); /*assert(expr);*/ }
#define DBG(format, arg...)  fprintf(stderr, format"\n", ## arg)
#endif // NDEBUG
#endif //ASSERT

class RFReceiver {

public:
  RFReceiver(int gpio);
  ~RFReceiver();

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

  bool decodeManchester(ReceivedData* message, Bits& bitSet);
  bool decodeF007TH(ReceivedData* message, uint32_t& nF007TH);
  bool decode00592TXR(ReceivedData* message);

private:
  static void initLib();
  static void closeLib();
  static void closeAll();
#ifdef USE_GPIO_TS
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

#ifdef USE_GPIO_TS
  int readSequences();
#else
  void handleInterrupt(int level, uint32_t tick);
#endif
  void endOfSequence();
  void decoder();
  void startDecoder();
  void resetReceiverBuffer();
  //void sequenceIsReady(int nextSequenceIndex);

  void addBit(bool bit);

  RFReceiver* next;
  static RFReceiver* first;
  static bool isLibInitialized;

  uint32_t nLastTime;
  int gpio;
  int lastLevel;

  uint32_t protocols;
  unsigned long min_duration;
  unsigned long max_duration;

#ifdef USE_GPIO_TS
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

  ReceivedData* firstMessage;
  ReceivedData** lastMessagePtr;

  // decoder thread and synchronization

  pthread_t decoderThreadId;
  //pthread_mutex_t sequencePoolLock;
  //pthread_cond_t sequenceReadyForDecoding;

  // timer

  // statistics

  uint32_t interrupted;
  uint32_t sequences;
  uint32_t skipped;
  uint32_t dropped;
  uint32_t corrected;
  uint32_t sequence_pool_overflow;
  uint32_t bad_manchester;
  uint32_t manchester_OOS;

  // control

  bool isEnabled;
  bool isDecoderStarted;
  bool stopDecoder;
  bool stopMessageReader;
  bool stopped;
  int timerEvent;
};

#endif
