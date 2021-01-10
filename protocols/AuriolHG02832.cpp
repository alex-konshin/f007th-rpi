/*
 * Auriol HG02832
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

// Auriol HG02832
#define MIN_DURATION_HG02832 150
#define MAX_DURATION_HG02832 1000
#define MIN_SEQUENCE_HG02832 87


static ProtocolDef def_hg02832 = {
  name : "hg02832",
  protocol_bit: PROTOCOL_HG02832,
  protocol_index: PROTOCOL_INDEX_HG02832,
  variant: 0,
  rolling_code_size: 8,
  number_of_channels: 3,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};

class ProtocolHG02832 : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      if (strcasecmp(def_hg02832.name, protocol_name) == 0) return &def_hg02832;
    }
    return NULL;
  }

public:
  static ProtocolHG02832* instance;

  ProtocolHG02832() : Protocol(PROTOCOL_HG02832, PROTOCOL_INDEX_HG02832, "HG02832",
      FEATURE_RF | FEATURE_CHANNEL | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS | FEATURE_HUMIDITY | FEATURE_BATTERY_STATUS ) {}

  uint64_t getId(SensorData* data) {
    uint64_t channel_bits = (data->u32.low >> 12) & 7UL;
    uint64_t rolling_code = (data->u32.low >> 24) & 255UL;
    uint64_t variant = 0;
    return ((uint64_t)protocol_index<<48) | (variant<<16) | (channel_bits<<8) | rolling_code;
  }
  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    uint64_t channel_bits = (channel-1) & 7UL;
    return ((uint64_t)protocol_index<<48) | (((uint64_t)protocol_def->variant&0x0ffff)<<16) | (channel_bits<<8) | (rolling_code&255);
  }

  int getChannel(SensorData* data) { return ((data->u32.low>>12)&3)+1; }
  int getChannelNumber(SensorData* data) { return ((data->u32.low>>12)&3)+1; }
  const char* getChannelName(SensorData* data) { return channel_names_numeric[((data->u32.low>>12)&3)+1]; }

  int getMetrics(SensorData* data) { return METRIC_TEMPERATURE | METRIC_HUMIDITY | METRIC_BATTERY_STATUS; }

  bool hasBatteryStatus() { return true; }
  bool getBatteryStatus(SensorData* data) { return (data->u32.low&0x00008000) == 0; }

  int getTemperatureCx10(SensorData* data) {
    int result = (int)((data->u32.low)&0x0fff);
    if ((result&0x0800) != 0) result |= 0xfffff000;
    return result;
  }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) { return (getTemperatureCx10(data)*9/5)+320; }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return true; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return (data->u32.low>>16) & 255; }

  const char* getSensorTypeName(SensorData* data) { return "HG02832"; }
  const char* getSensorTypeLongName(SensorData* data) { return "Auriol HG02832 (IAN 283582)"; }

  // random number that is changed when battery is changed
  uint16_t getRollingCode(SensorData* data) { return (data->u32.low >> 24); }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (((p->u32.low^s->u32.low)&0xff003000) == 0); // compare rolling code and channel
  }
/*
  bool sameId(SensorData* s, int channel, uint8_t rolling_code = -1) {
    if (rolling_code != -1 && (s->u32.low>>24) != rolling_code) return false;
    return ((s->u32.low>>12)&3)+1 == (uint8_t)channel;
  }
*/
  int update(SensorData* sensorData, SensorData* item, time_t data_time, time_t max_unchanged_gap) {
    uint32_t new_w = sensorData->u32.low;
    uint32_t w = item->u32.low;
    if (w == new_w) {
      sensorData->def = item->def;
      return 0; // nothing has changed
    }
    sensorData->def = item->def;
    time_t gap = data_time - item->data_time;
    if (gap < 2L) return TIME_NOT_CHANGED;
    int result = 0;
    if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
      result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
    } else {
      if (((w^new_w)&0x00000fff) != 0) result |= TEMPERATURE_IS_CHANGED;
      if (((w^new_w)&0x00ff0000) != 0) result |= HUMIDITY_IS_CHANGED;
      if (((w^new_w)&0x00008000) != 0) result |= BATTERY_STATUS_IS_CHANGED;
    }
    if (result != 0) {
      item->u64 = sensorData->u64;
      item->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  HG02832 data  = %08x%02x\n", sensorData->u32.low, sensorData->u32.hi&255);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_DURATION_HG02832 ) min_duration = MIN_DURATION_HG02832;
    if ( max_duration==0 || max_duration<MAX_DURATION_HG02832 ) max_duration = MAX_DURATION_HG02832;
    if ( min_sequence_length==0 || min_sequence_length>MIN_SEQUENCE_HG02832 ) min_sequence_length = MIN_SEQUENCE_HG02832;
  };

  /*
   * Decoding Auriol HG02832 (IAN 283582)
   *
   */
  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = NULL;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < 87) {
      message->decodingStatus |= 8;
      return false;
    }

    int16_t* pSequence = message->pSequence;
    int16_t item;

    // skip and check preamble
    int dataStartIndex = -1;
    for ( int preambleIndex = 0; preambleIndex<=(iSequenceSize-87); preambleIndex++ ) {

      item = pSequence[preambleIndex];
      if (item<=300 || item>=450) continue;

      item = pSequence[preambleIndex+1];
      if (item<=700 || item>=850) continue;
      item = pSequence[preambleIndex+2];
      if (item<=850 || item>=MAX_DURATION_HG02832) continue;
      item = pSequence[preambleIndex+3];
      if (item<=700 || item>=850) continue;
      item = pSequence[preambleIndex+4];
      if (item<=850 || item>=MAX_DURATION_HG02832) continue;
      item = pSequence[preambleIndex+5];
      if (item<=700 || item>=850) continue;
      item = pSequence[preambleIndex+6];
      if (item<=850 || item>=MAX_DURATION_HG02832) continue;
      item = pSequence[preambleIndex+7];
      if (item>700 && item<850) {
        dataStartIndex = preambleIndex+8;
        break;
      }
    }
    if ( dataStartIndex==-1 ) {
      message->decodingStatus |= 16;
      return false;
    }

    //DBG("decodeHG02832() after preamble");
    // Decoding bits

    int n;
    Bits bits(40);
    for ( int index = dataStartIndex; index<dataStartIndex+79; index+=2 ) {
      item = pSequence[index];
      if ( item<MIN_DURATION_HG02832 || item>700 ) {
        //DBG("decodeHG02832() pSequence[%d]=%d => (n<%d || n>700)",index,item,MIN_DURATION_HG02832);
        message->decodingStatus |= 4;
        return false;
      }
      if ( index+1<iSequenceSize ) {
        n = pSequence[index+1];
        if ( n<150 || n>700 ) {
          //DBG("decodeHG02832() pSequence[%d]=%d => (n<150 || n>700)",index+1,n);
          message->decodingStatus |= 4;
          return false;
        }
        n += item;
        if ( n<750 || n>950 ) {
          //DBG("decodeHG02832() pair sum=%d => (n<800 || n>900)",n);
          message->decodingStatus |= 4;
          return false;
        }
      }
      bits.addBit(item>400);
    }

    uint64_t data = bits.getInt64(0, 32);
    uint8_t checksum = bits.getInt64(32, 8);

    message->sensorData.u64 = data;
    message->sensorData.u32.hi = checksum;
    message->sensorData.protocol = this;
    message->decodedBits = (uint16_t)bits.getSize();

    if ( (data&0x00ff0fffL)==0 ) {
      message->decodingStatus |= 0x0180;
      return false;
    }

    uint8_t calculated_sum = 0x53 ^ data ^ (data>>8) ^ (data>>16) ^ (data>>24);
    for ( int bit = 0; bit<8; ++bit ) {
      if ( (calculated_sum&0x80)!=0 ) {
        calculated_sum = (calculated_sum<<1)^0x31;
      } else {
        calculated_sum = (calculated_sum<<1);
      }
    }
    if ( ((checksum^calculated_sum)&255) != 0) {
      message->decodingStatus |= 0x0080;
      return false;
    }
    return true;
  }

};

ProtocolHG02832* ProtocolHG02832::instance = new ProtocolHG02832();
