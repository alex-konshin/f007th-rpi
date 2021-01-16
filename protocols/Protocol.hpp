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
#define PROTOCOL_DS18B20   32
#define PROTOCOL_ALL       (unsigned)(-1)

#define PROTOCOL_INDEX_00592TXR  0
#define PROTOCOL_INDEX_TX7U      1
#define PROTOCOL_INDEX_WH2       2
#define PROTOCOL_INDEX_HG02832   3
#define PROTOCOL_INDEX_F007TH    4
#define PROTOCOL_INDEX_DS18B20   5
#define NUMBER_OF_PROTOCOLS      6

#define MAX_PROTOCOL_NAME_LEN   16

#define METRIC_TEMPERATURE       1
#define METRIC_HUMIDITY          2
#define METRIC_BATTERY_STATUS    4

#define FEATURE_RF                   1
#define FEATURE_CHANNEL              2
#define FEATURE_ROLLING_CODE         4
#define FEATURE_ID32                 8
#define FEATURE_TEMPERATURE         16
#define FEATURE_TEMPERATURE_CELSIUS 32
#define FEATURE_HUMIDITY            64
#define FEATURE_BATTERY_STATUS     128

#define MIN_PERIOD 900
#define MAX_PERIOD 1150

#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include "../utils/Bits.hpp"
#include "../utils/Logger.hpp"

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
class Protocol;

struct ProtocolDef {
  const char *name;
  uint32_t protocol_bit;
  uint8_t protocol_index;
  uint8_t variant;
  uint8_t rolling_code_size;
  uint8_t number_of_channels;
  uint8_t channels_numbering_type; // 0 => numbers, 1 => letters

  Protocol* getProtocol();
  uint32_t getFeatures();
  uint64_t getId(uint8_t channel, uint16_t rolling_code);
};

//-------------------------------------------------------------
class Protocol {
protected:
  static const char* channel_names_numeric[];
  virtual ProtocolDef* _getProtocolDef(const char* protocol_name) = 0;

public:

  static Protocol* protocols[NUMBER_OF_PROTOCOLS];
  static uint32_t registered_protocols;
  static uint32_t rf_protocols;

  static ProtocolDef* getProtocolDef(const char* protocol_name) {
    if (protocol_name == NULL || *protocol_name == '\0') return NULL;
    for (int protocol_index = 0; protocol_index<NUMBER_OF_PROTOCOLS; protocol_index++) {
      Protocol* protocol = protocols[protocol_index];
      ProtocolDef* def = protocol == NULL ? NULL : protocol->_getProtocolDef(protocol_name);
      if (def != NULL) return def;
    }
    return NULL;
  }

  static void initialize() {
    registered_protocols = 0;
    rf_protocols = 0;
    for (int protocol_index = 0; protocol_index<NUMBER_OF_PROTOCOLS; protocol_index++) {
      Protocol* protocol = protocols[protocol_index];
      if (protocol != NULL) {
        registered_protocols |= protocol->protocol_bit;
        if ((protocol->features&FEATURE_RF) != 0) rf_protocols |= protocol->protocol_bit;
      }
    }
  }

  uint32_t protocol_bit;
  uint8_t protocol_index;
  const char* protocol_class;
  uint32_t features;

  Protocol(uint8_t protocol_bit, uint8_t protocol_index, const char* protocol_class, uint32_t features) : protocol_bit(protocol_bit), protocol_index(protocol_index), protocol_class(protocol_class), features(features) {
    protocols[protocol_index] = this;
  }
  virtual ~Protocol() {}

  // WARNING: can be called with data==NULL
  virtual uint32_t getFeatures(SensorData* data) { return features; }

  virtual uint64_t getId(SensorData* data) VIRTUAL;
  virtual uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) VIRTUAL;
  virtual int getChannel(SensorData* data) { return -1; }
  virtual int getChannelNumber(SensorData* data) { return -1; }
  virtual const char* getChannelName(SensorData* data) { return NULL; }

  virtual bool hasBatteryStatus() { return false; }
  virtual bool getBatteryStatus(SensorData* data) { return false; }

  virtual bool hasTemperature(SensorData* data) { return true; }
  virtual bool isRawTemperatureCelsius() { return false; }
  virtual int getRawTemperature(SensorData* data) { return getTemperature10(data, isRawTemperatureCelsius()); };
  virtual int getTemperature10(SensorData* data, bool celsius);
  virtual int getTemperatureCx10(SensorData* data) VIRTUAL;
  virtual int getTemperatureFx10(SensorData* data) VIRTUAL;

  virtual bool hasHumidity(SensorData* data) { return false; }
  virtual int getHumidity(SensorData* data) { return 0; }

  virtual int getMetrics(SensorData* data) {
    int metrics = hasTemperature(data) ? METRIC_TEMPERATURE : 0;
    if (hasHumidity(data)) metrics |= METRIC_HUMIDITY;
    if (hasBatteryStatus()) metrics |= METRIC_BATTERY_STATUS;
    return metrics;
  }

  virtual const char* getSensorTypeName(SensorData* data) VIRTUAL;
  virtual const char* getSensorTypeLongName(SensorData* data) VIRTUAL;
  virtual uint16_t getRollingCode(SensorData* data) { return (uint16_t)(-1); };

  virtual bool equals(SensorData* s, SensorData* p) VIRTUAL;
  //virtual bool sameId(SensorData* s, int channel, uint8_t rolling_code = -1) VIRTUAL;
  virtual void copyFields(SensorData* to, SensorData* from);
  virtual int update(SensorData* sensorData, SensorData* p, time_t data_time, time_t max_unchanged_gap) VIRTUAL;

  virtual void printRawData(SensorData* sensorData, FILE* file) VIRTUAL;
  virtual void printUndecoded(ReceivedData* message, FILE* out, FILE* log, int cfg_options) {}

  static void setLimits(unsigned protocol_mask, unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    //max_duration = 0;
    //min_duration = 0;
    //min_sequence_length = 0;

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
protected:
  static bool decodeManchester(ReceivedData* message, Bits& bitSet, int min_duration, int max_half_duration);
  static bool decodeManchester(ReceivedData* message, int startIndex, int endIndex, Bits& bitSet, int max_half_duration);

  bool decodePWM(ReceivedData* message, int startIndex, int size, int minLo, int maxLo, int minHi, int maxHi, int median, Bits& bits);
  uint8_t crc8( Bits& bits, int from, int size, int polynomial, int init );

  virtual void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {};

};

#endif /* PROTOCOL_HPP_ */
