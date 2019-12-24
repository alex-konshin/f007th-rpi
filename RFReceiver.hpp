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

#ifdef INCLUDE_HTTPD
#ifdef USE_GPIO_TS
#define MIN_HTTPD_PORT 1024
#else
#define MIN_HTTPD_PORT 1
#endif
#include <microhttpd.h>
#endif

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

#define RF_RECEIVER_VERSION "3.1"

#include "Logger.hpp"
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
#define MIN_DURATION_F007TH 380
#define MAX_DURATION_F007TH 1150
#define MAX_HALF_DURATION 600
#define MIN_PERIOD 900
#define MAX_PERIOD 1150

// LaCrosse TX-6U/TX-7U
#define MIN_DURATION_TX7U 400
#define MAX_DURATION_TX7U 1500

// Auriol HG02832
#define MIN_DURATION_HG02832 150
#define MAX_DURATION_HG02832 1000
#define MIN_SEQUENCE_HG02832 87

// Fine Offset Electronics WH2
#define MIN_LO_DURATION_WH2 810
#define MAX_LO_DURATION_WH2 1020
#define MIN_HI_DURATION_WH2 450
#define MAX_HI_DURATION_WH2 1550
#define PWM_MEDIAN_WH2 1000
#define MIN_SEQUENCE_WH2 95

// Noise filter
#define IGNORABLE_SKIP 60
#define MAX_IGNORD_SKIPS 2


#ifdef BPiM3
#include "mach/bpi-m3.h"
#endif

#ifdef ODROIDC2
#include "mach/odroid-c2.h"
#endif

#ifdef RPI
#include "mach/rpi3.h"
#endif

#ifdef __x86_64__
#include "mach/x86_64.h"
#endif

#ifndef MAX_GPIO
#define MAX_GPIO 53
#endif
#ifndef DEFAULT_PIN
#define DEFAULT_PIN 27
#endif

#ifndef ASSERT
#ifdef  NDEBUG
#define ASSERT(expr)     ((void)0)
#define DBG(format, arg...)     ((void)0)
#else
#define PRINT_ASSERT(expr,file,line)  fputs("ASSERTION: ("#expr") in "#file" : "#line"\n", stderr)
#define ASSERT(expr)  if(!(expr)) { PRINT_ASSERT(expr,__FILE__,__LINE__); /*assert(expr);*/ }
#define DBG(format, arg...)  Log->info(format, ## arg)
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

  bool decodeF007TH(ReceivedData* message, uint32_t& nF007TH);
  bool decode00592TXR(ReceivedData* message);
  bool decodeTX7U(ReceivedData* message);
  bool decodeHG02832(ReceivedData* message);
  bool decodeWH2(ReceivedData* message);

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
  void resetReceiverBuffer();

  bool decodeManchester(ReceivedData* message, Bits& bitSet);
  bool decodeManchester(ReceivedData* message, int startIndex, int endIndex, Bits& bitSet);
  bool decodePWM(ReceivedData* message, int startIndex, int size, int minLo, int maxLo, int minHi, int maxHi, int median, Bits& bits);
  uint8_t crc8( Bits& bits, int from, int size, int polynomial, int init );

  void addBit(bool bit);

  RFReceiver* next;
  static RFReceiver* first;
  static bool isLibInitialized;

  uint32_t nLastTime;
  int gpio;
  int lastLevel;

  unsigned protocols;
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

  ReceivedData* firstMessage;
  ReceivedData** lastMessagePtr;

  // decoder thread and synchronization

  pthread_t decoderThreadId;
  //pthread_mutex_t sequencePoolLock;
  //pthread_cond_t sequenceReadyForDecoding;

  // timer

  // statistics

#ifdef TEST_DECODING
#elif !defined(USE_GPIO_TS)
  uint32_t interrupted;
  uint32_t skipped;
  uint32_t corrected;
#endif
  uint32_t sequences;
  uint32_t dropped;
  uint32_t sequence_pool_overflow;
  uint32_t bad_manchester;
  uint32_t manchester_OOS;

  // control

  bool isEnabled;
  bool isDecoderStarted;
  bool stopDecoder;
  bool stopMessageReader;
  bool stopped;
#ifdef TEST_DECODING
  bool waitAfterReading;
#endif
  int timerEvent;
};

#endif
