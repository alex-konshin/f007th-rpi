/*
  RFReceiver

  Copyright (c) 2017 Alex Konshin
*/
#ifndef _RFReceiver_h
#define _RFReceiver_h

#define RaspberryPi

#include <string.h> /* memcpy */
#include <stdlib.h> /* abs, malloc */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

//#include <wiringPi.h>
#include <pigpio.h>

#include "Bits.hpp"
#include "ReceivedMessage.hpp"

#define POOL_SIZE 4096
#define MAX_CHAINS 64
#define MIN_SEQUENCE_LENGTH 85
#define MAX_SEQUENCE_LENGTH 400
#define MANCHESTER_BUFFER_SIZE 25

#define MIN_DURATION 400
#define MAX_HALF_DURATION 600
#define MIN_PERIOD 900
#define MAX_PERIOD 1150

// Noise filter
#define IGNORABLE_SKIP 60
#define MAX_IGNORD_SKIPS 2

class RFReceiver {

public:
  RFReceiver(int gpio);

  bool enableReceive();
  void disableReceive();
  void stop();

  bool available();
  bool waitForMessage(ReceivedMessage& message);

  bool checkAndResetTimerEvent();
  void printStatistics();
  void printStatisticsPeriodically(uint32_t millis);
  void printDebugStatistics();

  bool decodeManchester(ReceivedData* message, Bits& bitSet);
  bool decodeF007TH(ReceivedData* message, uint32_t& nF007TH);


private:

  static bool isLibInitialized;

  static void initLib();
  static void interruptCallback(int gpio, int level, uint32_t tick, void *userdata);
  static void* decoderThreadFunction(void *context);
  static void timerHandler(void *context);

  void setTimer(uint32_t millis);

  static void destroyMessage(ReceivedData* message);
  ReceivedData* createNewMessage();

  void handleInterrupt(int level, uint32_t tick);
  void endOfSequence();
  void decoder();
  void startDecoder();
  void resetReceiverBuffer();
  //void sequenceIsReady(int nextSequenceIndex);

  void addBit(bool bit);

  uint32_t nLastTime;
  int gpio;
  int lastLevel;

  int nNoiseFilterCounter;
  uint32_t nLastGoodTime;

  uint32_t uCurrentStatisticsTimer;

  uint32_t manchester[MANCHESTER_BUFFER_SIZE];

  // cyclic buffer for durations

  int iPoolWrite;
  int iPoolRead;
  int iSequenceWrite;
  int iSequenceReady;
  int iCurrentSequenceStart;
  int iCurrentSequenceSize;
  uint32_t uCurrentSequenceStartTime;

  int16_t pool[POOL_SIZE];

  // cyclic buffer for sequences

  uint32_t uSequenceStartTime[MAX_CHAINS];
  int16_t iSequenceStart[MAX_CHAINS];
  int16_t iSequenceSize[MAX_CHAINS];

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
  int timerEvent;
};

#endif
