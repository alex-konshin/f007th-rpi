/*
 * Nexus.cpp
 *
 *  Nexus, FreeTec NC-7345, NX-3980, Solight TE82S, TFA 30.3209 temperature/humidity sensor.
 *
 *  Reference https://github.com/merbanan/rtl_433/blob/master/src/devices/nexus.c
 *
 *  Created on: June 06, 2021
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

#define DURATION_NEXUS_LO_0    900
#define DURATION_NEXUS_LO_1    1850
#define TOLERANCE_NEXUS_LO     150
#define DURATION_NEXUS_HI      525
#define TOLERANCE_NEXUS_HI     125
#define MIN_SEQUENCE_NEXUS     72

#define SYNC_NEXUS             4000
#define TOLERANCE_SYNC_NEXUS   150

#define MIN_LO_DURATION_NEXUS (DURATION_NEXUS_LO_0 - TOLERANCE_NEXUS_LO)
#define MAX_LO_DURATION_NEXUS (DURATION_NEXUS_LO_1 + TOLERANCE_NEXUS_LO)
#define MIN_HI_DURATION_NEXUS (DURATION_NEXUS_HI - TOLERANCE_NEXUS_HI)
#define MAX_HI_DURATION_NEXUS (DURATION_NEXUS_HI + TOLERANCE_NEXUS_HI)

#define DEBUG_NEXUS

#if defined(NDEBUG)||!defined(DEBUG_NEXUS)
#define DBG_NEXUS(format, arg...) ((void)0)
#else
#define DBG_NEXUS(format, arg...)  Log->info(format, ## arg)
#endif


static ProtocolDef def_nexus = {
  name : "nexus",
  protocol_bit: PROTOCOL_NEXUS,
  protocol_index: PROTOCOL_INDEX_NEXUS,
  variant: 0,
  rolling_code_size: 8,
  number_of_channels: 3,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};

static const char* const protocol_aliases[] = {
    "nexus",
    "NC-7345", "nc7345",
    "NX-3980", "nx3980",
    "TE82S",
    "TFA 30.3209", "tfa303209"
};
#define NUMBER_OF_ALIASES (sizeof(protocol_aliases)/sizeof(const char*))

class ProtocolNEXUS : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      for (unsigned i = 0; i<NUMBER_OF_ALIASES; i++) {
        if (strcasecmp(protocol_aliases[i], protocol_name) == 0) return &def_nexus;
      }
    }
    return NULL;
  }

public:
  static ProtocolNEXUS* instance;

  ProtocolNEXUS() : Protocol(PROTOCOL_NEXUS, PROTOCOL_INDEX_NEXUS, "NEXUS",
      FEATURE_RF | FEATURE_CHANNEL | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS | FEATURE_HUMIDITY | FEATURE_BATTERY_STATUS) {}

  uint64_t getId(SensorData* data) {
    return ((uint64_t)protocol_index<<48) | (data->u64 & 0x0ff7000000UL);
  }
  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    return ((uint64_t)protocol_index<<48) | ((uint64_t)channel<<24) | ((uint64_t)rolling_code <<28);
  }

  int getChannel(SensorData* data) { return (int)((data->u32.low >> 24) & 7); }
  int getChannelNumber(SensorData* data) { return getChannel(data); }
  const char* getChannelName(SensorData* data) { return channel_names_numeric[getChannel(data)]; }

  int getMetrics(SensorData* data) { return METRIC_TEMPERATURE | METRIC_HUMIDITY | METRIC_BATTERY_STATUS; }

  bool hasBatteryStatus() { return true; }
  bool getBatteryStatus(SensorData* data) { return (data->u32.low&0x08000000) == 0x08000000; }

  int getTemperatureCx10(SensorData* data) {
    int t = (int)((data->u32.low >> 12) & 0x0fff);
    DBG_NEXUS("NEXUS: getTemperatureCx10 = %d", t);
    if ((t & 0x0800) != 0) t |= 0xfffff000;
    return t;
  }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) {
    long t = (long)((data->u32.low >> 12) & 0x0fff);
    if ((t & 0x0800) != 0) t |= 0xfffff000;
    return (int)((t*90+25)/50+320);
  }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return true; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return (int)(data->u32.low & 255); }

  const char* getSensorTypeName(SensorData* data) { return "NEXUS"; }
  const char* getSensorTypeLongName(SensorData* data) { return "Nexus"; }

  // random number that is changed when battery is changed
  uint16_t getRollingCode(SensorData* data) { return (uint16_t)(data->u64>>28); }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (((p->u64^s->u64)& 0x0ff7000000UL) == 0); // compare rolling code & channel
  }

  int update(SensorData* sensorData, SensorData* p, time_t data_time, time_t max_unchanged_gap) {
    uint32_t new_w = sensorData->u32.low;
    uint32_t w = p->u32.low;
    sensorData->def = p->def;
    time_t gap = data_time - p->data_time;
    if (gap < 2L) return TIME_NOT_CHANGED;

    int result = 0;
    if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
      result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED;
    } else {
      if (w == new_w) return 0; // nothing has changed
      if (((w^new_w)&0x00fff000) != 0) result |= TEMPERATURE_IS_CHANGED;
      if (((w^new_w)&0x000000ff) != 0) result |= HUMIDITY_IS_CHANGED;
    }
    if (((w^new_w)&0x08000000) != 0) result |= BATTERY_STATUS_IS_CHANGED;
    if (result != 0) {
      p->u64 = sensorData->u64;
      p->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  NEXUS data  = %08x\n", sensorData->u32.low);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_HI_DURATION_NEXUS ) min_duration = MIN_HI_DURATION_NEXUS;
    if ( max_duration==0 || max_duration<MAX_LO_DURATION_NEXUS ) max_duration = MAX_LO_DURATION_NEXUS;
    if ( min_sequence_length==0 || min_sequence_length>MIN_SEQUENCE_NEXUS ) min_sequence_length = MIN_SEQUENCE_NEXUS;
  };

  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = NULL;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < MIN_SEQUENCE_NEXUS*3-1) {
      message->decodingStatus = 8;
      return false;
    }

    uint64_t packages[15];
    int n_packages = 0;

    Bits bits(150);
    int startIndex = 0;
    do {
      int gapIndex = findGap(message, startIndex, iSequenceSize, SYNC_NEXUS, TOLERANCE_SYNC_NEXUS);
      if (gapIndex >= startIndex+MIN_SEQUENCE_NEXUS) {
        bits.clear();
        DBG_NEXUS("NEXUS: startIndex = %d, gapIndex = %d", startIndex, gapIndex);
        // Decoding bits
        if ( decodePPM(message, startIndex, MIN_SEQUENCE_NEXUS, DURATION_NEXUS_HI, TOLERANCE_NEXUS_HI, DURATION_NEXUS_LO_0, DURATION_NEXUS_LO_1, TOLERANCE_NEXUS_LO, bits) ) {
#if !defined(NDEBUG)&&defined(DEBUG_NEXUS)
          ReceivedMessage::printBits(bits);
#endif
          uint64_t n = bits.getInt64(0, 36);
          DBG_NEXUS("NEXUS: data = %016llx", n);
          if ((n & 0x0f00) == 0x0f00) { // check constant field
            uint8_t channel = (uint8_t)((n >> 24) & 7);
            if (channel <= 3) {
              // valid package
              packages[n_packages++] = n;
            }
          }
        }
      }
      startIndex = gapIndex+1;
    } while (startIndex+MIN_SEQUENCE_NEXUS <= iSequenceSize);

    if ( n_packages<3 ) {
      message->decodingStatus = 12;
      return false;
    }

    // find matching data
    uint64_t data = 0L;
    startIndex = 0;
    do {
      uint64_t n = packages[startIndex];
      int matched = 1;
      for ( int index = startIndex+1; index < n_packages; index++) {
        if (packages[index] == n) matched++;
      }
      if (matched >= 3) {
        data = n;
        break;
      }
    } while (++startIndex >= n_packages-3);

    if (data == 0L) {
      message->decodingStatus = 13;
      return false;
    }
    DBG_NEXUS("NEXUS: validated data = %016llx", data);

    message->sensorData.u64 = data;
    message->sensorData.protocol = this;
    message->decodedBits = 36; //(uint16_t)bits.getSize();

    return true;
  }

};

ProtocolNEXUS* ProtocolNEXUS::instance = new ProtocolNEXUS();
