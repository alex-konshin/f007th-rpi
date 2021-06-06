/*
 * TFATwinPlus.cpp
 *
 * TFA Twin Plus 30.3049, Conrad KW9010, Ea2 BL999.
 *
 *  Created on: January 16, 2021
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

#define DURATION_TFA303049_LO_0    2000
#define DURATION_TFA303049_LO_1    4000
#define TOLERANCE_TFA303049_LO     200
#define DURATION_TFA303049_HI      500
#define TOLERANCE_TFA303049_HI     125
#define MIN_SEQUENCE_TFA303049 73

#define MIN_LO_DURATION_TFA303049 (DURATION_TFA303049_LO_0 - TOLERANCE_TFA303049_LO)
#define MAX_LO_DURATION_TFA303049 (DURATION_TFA303049_LO_1 + TOLERANCE_TFA303049_LO)
#define MIN_HI_DURATION_TFA303049 (DURATION_TFA303049_HI - TOLERANCE_TFA303049_HI)
#define MAX_HI_DURATION_TFA303049 (DURATION_TFA303049_HI + TOLERANCE_TFA303049_HI)

static ProtocolDef def_tfa303049 = {
  name : "tfa303049",
  protocol_bit: PROTOCOL_TFA303049,
  protocol_index: PROTOCOL_INDEX_TFA303049,
  variant: 0,
  rolling_code_size: 6,
  number_of_channels: 4,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};

static const char* const protocol_aliases[] = {
    "tfa303049", "tfatwinplus", "TFA Twin Plus 30.3049",
    "kw9010", "Conrad KW9010", "conradkw9010",
    "ea2bl999", "Ea2 BL999", "bl999"
};
#define NUMBER_OF_ALIASES (sizeof(protocol_aliases)/sizeof(const char*))

//static uint8_t reverse_4bits[] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
static uint8_t reverse_2bits[] = { 0, 2, 1, 3 };

class ProtocolTFA303049 : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      for (unsigned i = 0; i<NUMBER_OF_ALIASES; i++) {
        if (strcasecmp(protocol_aliases[i], protocol_name) == 0) return &def_tfa303049;
      }
    }
    return NULL;
  }

public:
  static ProtocolTFA303049* instance;

  ProtocolTFA303049() : Protocol(PROTOCOL_TFA303049, PROTOCOL_INDEX_TFA303049, "TFA303049",
      FEATURE_RF | FEATURE_CHANNEL | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS | FEATURE_HUMIDITY | FEATURE_BATTERY_STATUS) {}

  uint64_t getId(SensorData* data) {
    uint64_t rolling_code = ((data->u32.low & 0x0f) | ((data->u32.low>>2) & 0x30)) & 0x3fUL;
    uint64_t channel_bits = (data->u32.low >> 4) & 3UL;
    return ((uint64_t)protocol_index<<48) | (channel_bits<<16) | rolling_code;
  }
  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    uint64_t channel_bits = reverse_2bits[channel&3];
    return ((uint64_t)protocol_index<<48) | (channel_bits<<16) | (rolling_code&63);
  }

  int getChannel(SensorData* data) { return reverse_2bits[(data->u32.low >> 4) & 3]; }
  int getChannelNumber(SensorData* data) { return getChannel(data); }
  const char* getChannelName(SensorData* data) { return channel_names_numeric[getChannel(data)]; }

  int getMetrics(SensorData* data) { return METRIC_TEMPERATURE | METRIC_HUMIDITY | METRIC_BATTERY_STATUS; }

  bool hasBatteryStatus() { return true; }
  bool getBatteryStatus(SensorData* data) { return (data->u32.low&0x00000100) == 0; }

  int getTemperatureCx10(SensorData* data) {
    int t = (int)((data->u32.low >> 12) & 0x0fff);
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
  int getHumidity(SensorData* data) { return ((int)(data->u32.low >> 24) & 0x7f) - 28; }

  const char* getSensorTypeName(SensorData* data) { return "TFA303049"; }
  const char* getSensorTypeLongName(SensorData* data) { return "TFA Twin Plus 30.3049"; }

  // random number that is changed when battery is changed
  uint16_t getRollingCode(SensorData* data) { return (data->u32.low & 0x0f) | ((data->u32.low>>2) & 0x30); }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (((p->u32.low^s->u32.low)&0x000000ff) == 0); // compare rolling code & channel
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
      if (((w^new_w)&0x7f000000) != 0) result |= HUMIDITY_IS_CHANGED;
    }
    if (((w^new_w)&0x00000100) != 0) result |= BATTERY_STATUS_IS_CHANGED;
    if (result != 0) {
      p->u64 = sensorData->u64;
      p->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  TFA303049 data  = %08x\n", sensorData->u32.low);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_HI_DURATION_TFA303049 ) min_duration = MIN_HI_DURATION_TFA303049;
    if ( max_duration==0 || max_duration<MAX_LO_DURATION_TFA303049 ) max_duration = MAX_LO_DURATION_TFA303049;
    if ( min_sequence_length==0 || min_sequence_length>MIN_SEQUENCE_TFA303049 ) min_sequence_length = MIN_SEQUENCE_TFA303049;
  };

  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = NULL;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < MIN_SEQUENCE_TFA303049) {
      message->decodingStatus = 8;
      return false;
    }

    // Decoding bits

    Bits bits(40);
    if ( !decodePPM(message, 0, MIN_SEQUENCE_TFA303049, DURATION_TFA303049_HI, TOLERANCE_TFA303049_HI, DURATION_TFA303049_LO_0, DURATION_TFA303049_LO_1, TOLERANCE_TFA303049_LO, bits) ) return false;
#ifndef NDEBUG // FIXME
    ReceivedMessage::printBits(bits);
#endif

    uint64_t data = bits.getReverse64(0, 36);
//    DBG("data = %016llx", data);

    uint32_t n = (uint32_t)data;
//    DBG("n = %08x", n);
    uint8_t calculated_checksum = 0;
    for (int i=0; i<8; i++) {
      calculated_checksum += n & 15;
      n = n>>4;
    }
    calculated_checksum &= 15;
    uint8_t checksum = (data>>32) & 15;
    if (checksum != calculated_checksum) {
//      DBG("TFA303049 bad checksum: checksum=0x%02x calculated_checksum=0x%02x",checksum,calculated_checksum);
      message->decodingStatus = 0x0080; // bad checksum
      return false;
    }

    n = (uint32_t)data;
    if ((n & 0x80000000) == 0) {
      message->decodingStatus = 0x0180; // 32 bit must be 1
      return false;
    }
    n = (n >> 21) & 7;
    if (n != 0 && n != 7) {
      message->decodingStatus = 0x0280; // sign bits must be 000 or 111
      return false;
    }

    message->sensorData.u64 = data;
    message->sensorData.protocol = this;
    message->decodedBits = (uint16_t)bits.getSize();

    return true;
  }

};

ProtocolTFA303049* ProtocolTFA303049::instance = new ProtocolTFA303049();
