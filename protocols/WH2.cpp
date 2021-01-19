/*
 * WH2.cpp
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

// Fine Offset Electronics WH2
#define MIN_LO_DURATION_WH2 810
#define MAX_LO_DURATION_WH2 1020
#define MIN_HI_DURATION_WH2 450
#define MAX_HI_DURATION_WH2 1550
#define PWM_MEDIAN_WH2 1000
#define MIN_SEQUENCE_WH2 95

static ProtocolDef def_wh2 = {
  name : "wh2",
  protocol_bit: PROTOCOL_WH2,
  protocol_index: PROTOCOL_INDEX_WH2,
  variant: 0x40,
  rolling_code_size: 8,
  number_of_channels: 0,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};
static ProtocolDef def_ft007th = {
  name : "ft007th",
  protocol_bit: PROTOCOL_WH2,
  protocol_index: PROTOCOL_INDEX_WH2,
  variant: 0x41,
  rolling_code_size: 8,
  number_of_channels: 0,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};

class ProtocolWH2 : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      if (strcasecmp(def_wh2.name, protocol_name) == 0) return &def_wh2;
      if (strcasecmp(def_ft007th.name, protocol_name) == 0) return &def_ft007th;
    }
    return NULL;
  }

public:
  static ProtocolWH2* instance;

  ProtocolWH2() : Protocol(PROTOCOL_WH2, PROTOCOL_INDEX_WH2, "WH2",
      FEATURE_RF | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS | FEATURE_HUMIDITY ) {}

  uint64_t getId(SensorData* data) {
    uint64_t variant = ((data->u32.hi&0x80000000) != 0 ? 1 : 0) | ((data->u32.low>>24)&0x00f0); // 0x41 = FT007TH, 0x40 = WH2
    uint64_t rolling_code = (data->u32.low >> 20) & 255UL;
    return ((uint64_t)protocol_index<<48) | (variant<<16) | rolling_code;
  }
  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    return ((uint64_t)protocol_index<<48) | (((uint64_t)protocol_def->variant&0x0ffff)<<16) | (rolling_code&255);
  }

  int getMetrics(SensorData* data) { return METRIC_TEMPERATURE | METRIC_HUMIDITY; }

  int getTemperatureCx10(SensorData* data) {
    int t = (int)((data->u32.low>>8)&1023);
    if ( (t&0x0800)!=0 ) t = -(t&0x07ff);
    return t;
  }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) {
    int t = (int)((data->u32.low>>8)&1023);
    if ( (t&0x0800)!=0 ) t = -(t&0x07ff);
    return (t*90+25)/50+320;
  }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return true; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return data->u32.low & 127; }

  const char* getSensorTypeName(SensorData* data) { return (data->u32.hi&0x80000000) != 0 ? "FT007TH" : "WH2"; }
  const char* getSensorTypeLongName(SensorData* data) { return (data->u32.hi&0x80000000) != 0 ? "Telldus FT007TH" : "Fine Offset Electronics WH2"; }

  // random number that is changed when battery is changed
  uint16_t getRollingCode(SensorData* data) { return (data->u32.low >> 20) & 255; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (((p->u32.low^s->u32.low)&0x0ff00000) == 0); // compare rolling code
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
      if (((w^new_w)&0x0003ff00) != 0) result |= TEMPERATURE_IS_CHANGED;
      if (((w^new_w)&0x000000ff) != 0) result |= HUMIDITY_IS_CHANGED;
      //if (((w^new_w)&0x00080000) != 0) result |= BATTERY_STATUS_IS_CHANGED; TODO
    }
    if (result != 0) {
      p->u64 = sensorData->u64;
      p->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  WH2/FT007TH data  = %08x%02x\n", sensorData->u32.low, sensorData->u32.hi&255);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>150 ) min_duration = 150;
    if ( max_duration==0 || max_duration<MAX_HI_DURATION_WH2 ) max_duration = MAX_HI_DURATION_WH2;
    if ( min_sequence_length==0 || min_sequence_length>MIN_SEQUENCE_WH2 ) min_sequence_length = MIN_SEQUENCE_WH2;
  };

  /*
   * Decoding Fine Offset Electronics WH2/Telldus FT007TH
   */
  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = NULL;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < MIN_SEQUENCE_WH2) {
      message->decodingStatus |= 8;
      return false;
    }

    int16_t* pSequence = message->pSequence;
    int16_t item;

    // skip and check preamble
    bool ft007th = false;
    int dataStartIndex = -1;
    for ( int preambleIndex = 0; preambleIndex<=(iSequenceSize-MIN_SEQUENCE_WH2); preambleIndex+=2 ) {
      ft007th = false;
      int preambleStart = preambleIndex;
      for (int index = 0; index<16; index +=2) {
        item = pSequence[preambleStart+index];
        if (index==0 && (iSequenceSize-preambleIndex>=97) && item>=180 && item<=220) {
          item = pSequence[preambleIndex+1];
          if (item<=MIN_LO_DURATION_WH2 || item>=MAX_LO_DURATION_WH2) break;
          ft007th = true;
          preambleStart += 2;
          item = pSequence[preambleStart+index];
        }
        if (item<=MIN_HI_DURATION_WH2 || item>=MAX_HI_DURATION_WH2) continue;
        item = pSequence[preambleStart+index+1];
        if (item<=MIN_LO_DURATION_WH2 || item>=MAX_LO_DURATION_WH2) continue;
        if (index == 14) dataStartIndex = preambleStart+16;
      }
      if ( dataStartIndex!=-1 ) break;
    }
    if ( dataStartIndex==-1 ) {
      message->decodingStatus |= 16;
      return false;
    }

    // Decoding bits

    Bits bits(40);
    if ( !decodePWM(message, dataStartIndex, iSequenceSize-dataStartIndex, MIN_LO_DURATION_WH2, MAX_LO_DURATION_WH2, MIN_HI_DURATION_WH2, MAX_HI_DURATION_WH2, PWM_MEDIAN_WH2, bits) ) return false;
  #ifndef NDEBUG
    ReceivedMessage::printBits(bits);
  #endif

    uint64_t data = bits.getInt64(0, 32);
    uint8_t checksum = bits.getInt64(32, 8);
    uint8_t calculated_checksum = crc8(bits, 0, 32, 0x31, 0);
    if (checksum != calculated_checksum) {
      //DBG("decodeWH2() bad checksum: checksum=0x%02x calculated_checksum=0x%02x",checksum,calculated_checksum);
      message->decodingStatus |= 0x0080; // bad checksum
      return false;
    }

    int type = bits.getInt(0, 4);
    if (type != 4) {
      message->decodingStatus |= 0x0180; // unknown type
      return false;
    }

    message->sensorData.u64 = data;
    uint32_t hi = 255&checksum;
    if ( ft007th ) hi |= 0x80000000; // Telldus
    message->sensorData.u32.hi = hi;
    message->sensorData.protocol = this;
    message->decodedBits = (uint16_t)bits.getSize();

    return true;
  }

};

ProtocolWH2* ProtocolWH2::instance = new ProtocolWH2();
