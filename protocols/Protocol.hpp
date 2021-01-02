/*
 * Protocol.hpp
 *
 *  Created on: December 6, 2020
 *      Author: Alex Konshin
 */

#ifndef PROTOCOL_HPP_
#define PROTOCOL_HPP_

#define PROTOCOL_00592TXR  1
#define PROTOCOL_TX7U      2
#define PROTOCOL_WH2       4
#define PROTOCOL_HG02832   8
#define PROTOCOL_F007TH    16
#define PROTOCOL_ALL       (unsigned)(-1)

#define PROTOCOL_INDEX_00592TXR  0
#define PROTOCOL_INDEX_TX7U      1
#define PROTOCOL_INDEX_WH2       2
#define PROTOCOL_INDEX_HG02832   3
#define PROTOCOL_INDEX_F007TH    4
#define NUMBER_OF_PROTOCOLS      5

#define METRIC_TEMPERATURE       1
#define METRIC_HUMIDITY          2
#define METRIC_BATTERY_STATUS    4

#define MIN_PERIOD 900
#define MAX_PERIOD 1150

#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include "../utils/Bits.hpp"

struct SensorData;
struct ReceivedData;
class ReceivedMessage;
enum class BoundCheckResult;

//-------------------------------------------------------------
typedef struct Statistics {
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
} Statistics;

extern Statistics* statistics;

struct ProtocolDef {
  const char *name;
  uint8_t protocol;
  uint8_t protocol_index;
  uint8_t variant;
  uint8_t rolling_code_size;
  uint8_t number_of_channels;
  uint8_t channels_numbering_type; // 0 => numbers, 1 => letters
};

//-------------------------------------------------------------
class Protocol {
protected:
  static const char* channel_names_numeric[];
  virtual ProtocolDef* _getProtocolDef(const char* protocol_name) = 0;

public:
  static Protocol* protocols[NUMBER_OF_PROTOCOLS];

  static void registerProtocol(Protocol* protocol) {
    protocols[protocol->protocol_index] = protocol;
  }
  static ProtocolDef* getProtocolDef(const char* protocol_name) {
    if (protocol_name == NULL || *protocol_name == '\0') return NULL;
    for (int protocol_index = 0; protocol_index<NUMBER_OF_PROTOCOLS; protocol_index++) {
      Protocol* protocol = protocols[protocol_index];
      ProtocolDef* def = protocol == NULL ? NULL : protocol->_getProtocolDef(protocol_name);
      if (def != NULL) return def;
    }
    return NULL;
  }

  uint8_t protocol_bit;
  uint8_t protocol_index;
  const char* protocol_class;

  Protocol(uint8_t protocol_bit, uint8_t protocol_index, const char* protocol_class) : protocol_bit(protocol_bit), protocol_index(protocol_index), protocol_class(protocol_class) {
    registerProtocol(this);
  }
  virtual ~Protocol() {}

  virtual uint32_t getId(SensorData* data) = 0;
  virtual int getChannel(SensorData* data) { return -1; }
  virtual int getChannelNumber(SensorData* data) { return -1; }
  virtual const char* getChannelName(SensorData* data) { return NULL; }

  virtual bool hasBatteryStatus() { return false; }
  virtual bool getBatteryStatus(SensorData* data) { return false; }

  virtual bool hasTemperature(SensorData* data) { return true; }
  virtual bool isRawTemperatureCelsius() { return false; }
  virtual int getRawTemperature(SensorData* data) { return getTemperature10(data, isRawTemperatureCelsius()); };
  virtual int getTemperature10(SensorData* data, bool celsius);
  virtual int getTemperatureCx10(SensorData* data) = 0;
  virtual int getTemperatureFx10(SensorData* data) = 0;

  virtual bool hasHumidity(SensorData* data) { return false; }
  virtual int getHumidity(SensorData* data) { return 0; }

  virtual int getMetrics(SensorData* data) {
    int metrics = hasTemperature(data) ? METRIC_TEMPERATURE : 0;
    if (hasHumidity(data)) metrics |= METRIC_HUMIDITY;
    if (hasBatteryStatus()) metrics |= METRIC_BATTERY_STATUS;
    return metrics;
  }

  virtual const char* getSensorTypeName(SensorData* data) = 0;
  virtual const char* getSensorTypeLongName(SensorData* data) = 0;
  virtual uint8_t getRollingCode(SensorData* data) = 0;

  virtual bool equals(SensorData* s, SensorData* p) = 0;
  virtual bool sameId(SensorData* s, int channel, uint8_t rolling_code = -1) = 0;
  virtual void copyFields(SensorData* to, SensorData* from);
  virtual int update(SensorData* sensorData, SensorData* p, time_t data_time, time_t max_unchanged_gap) = 0;

  virtual void printRawData(SensorData* sensorData, FILE* file) = 0;


  static void setLimits(unsigned protocol_mask, unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    max_duration = 0;
    min_duration = 0;
    min_sequence_length = 0;

    if (protocol_mask != PROTOCOL_ALL) {
      for (int protocol_index = 0; protocol_index < NUMBER_OF_PROTOCOLS; protocol_index++) {
        Protocol* protocol = protocols[protocol_index];
        if (protocol != NULL && (protocol->protocol_bit&protocol_mask) != 0) {
          protocol->adjustLimits(min_sequence_length, max_duration, min_duration);
        }
      }
    }

    if ( min_duration==0 ) min_duration = 50;
    if ( max_duration==0 ) max_duration = 2000;
    if ( min_sequence_length==0 ) min_sequence_length = 32;
  }

  virtual bool decode(ReceivedData* message) { return false; }

  virtual bool decodeManchester(ReceivedData* message, Bits& bitSet) { return false; }
  static bool printManchesterBits(ReceivedMessage& message, FILE* file, FILE* file2);
protected:
  static bool decodeManchester(ReceivedData* message, Bits& bitSet, int min_duration, int max_half_duration);
  static bool decodeManchester(ReceivedData* message, int startIndex, int endIndex, Bits& bitSet, int max_half_duration);

  bool decodePWM(ReceivedData* message, int startIndex, int size, int minLo, int maxLo, int minHi, int maxHi, int median, Bits& bits);
  uint8_t crc8( Bits& bits, int from, int size, int polynomial, int init );

  virtual void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {};

};

#endif /* PROTOCOL_HPP_ */
