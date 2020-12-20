/*
 * Ambient Weather F007TH
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/RFReceiver.hpp"

// Ambient Weather F007TH
#define MIN_DURATION_F007TH 380
#define MAX_DURATION_F007TH 1150
#define MAX_HALF_DURATION 600


class ProtocolF007TH : public Protocol {

  ProtocolF007TH() : Protocol(PROTOCOL_F007TH, PROTOCOL_INDEX_F007TH, "F007TH") {
  }

  uint32_t getId(SensorData* data) {
    uint32_t variant = data->u32.hi==1 ? 1 : 0; // 0 = F007TH, 1 = F007TP
    uint32_t channel_bits = (data->nF007TH>>20)&7;
    uint32_t rolling_code = (data->nF007TH >> 24) & 255;
    return (protocol_bit<<24) | (variant<<16) | (channel_bits<<8) | rolling_code;
  }
  static ProtocolF007TH* instance;

  int getChannel(SensorData* data) { return ((data->nF007TH>>20)&7)+1; }
  int getChannelNumber(SensorData* data) { return getChannel(data); }
  const char* getChannelName(SensorData* data) { return channel_names_numeric[getChannel(data)]; }

  int getMetrics(SensorData* data) {
    return (data->u32.hi&1) == 0 ? METRIC_TEMPERATURE | METRIC_HUMIDITY | METRIC_BATTERY_STATUS : METRIC_TEMPERATURE | METRIC_BATTERY_STATUS;
  }

  bool hasBatteryStatus() { return true; }
  bool getBatteryStatus(SensorData* data) { return (data->nF007TH&0x00800000) == 0; }

  int getTemperatureCx10(SensorData* data) { return (int)(((data->nF007TH>>8)&4095)-720) * 5 / 9; }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) { return (int)((data->nF007TH>>8)&4095)-400; }
  int getRawTemperature(SensorData* data) { return (int)((data->nF007TH>>8)&4095)-400; };

  bool hasHumidity(SensorData* data) { return (data->u32.hi&1) == 0; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return (data->u32.hi&1) == 0 ? data->nF007TH&255 : 0; }

  const char* getSensorTypeName(SensorData* data) { return data->u32.hi==1 ? "F007TP" : "F007TH"; }
  const char* getSensorTypeLongName(SensorData* data) { return data->u32.hi==1 ? "Ambient Weather F007TP" : "Ambient Weather F007TH"; }

  // random number that is changed when battery is changed
  uint8_t getRollingCode(SensorData* data) { return (data->nF007TH >> 24) & 255; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && ((p->nF007TH ^ s->nF007TH) & SENSOR_UID_MASK) == 0 && p->u32.hi == s->u32.hi; // u32.hi -- F007TP flag
  }

  bool sameId(SensorData* s, int channel, uint8_t rolling_code = -1) {
    if (rolling_code != -1 && rolling_code != getRollingCode(s) ) return false;
    return channel == getChannel(s);
  }

  int update(SensorData* sensorData, SensorData* p, time_t data_time, time_t max_unchanged_gap) {
    uint32_t nF007TH = sensorData->nF007TH;
    uint32_t item = p->nF007TH;
    sensorData->def = p->def;
    time_t gap = data_time - p->data_time;
    if (gap < 2L) return TIME_NOT_CHANGED;
    int result = 0;
    if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
      result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
    } else {
      uint32_t changed = (item ^ nF007TH)& SENSOR_DATA_MASK;
      if (changed == 0) return 0;
      if ((changed&SENSOR_TEMPERATURE_MASK) != 0) result |= TEMPERATURE_IS_CHANGED;
      if ((sensorData->u32.hi&1) == 0 && (changed&SENSOR_HUMIDITY_MASK) != 0) result |= HUMIDITY_IS_CHANGED;
      if ((changed&BATTERY_STATUS_IS_CHANGED) != 0) result |= BATTERY_STATUS_IS_CHANGED;
    }
    if (result != 0) {
      p->u64 = sensorData->u64;
      p->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  F007TH data       = %08x\n", sensorData->nF007TH);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_DURATION_F007TH ) min_duration = MIN_DURATION_F007TH;
    if ( max_duration==0 || max_duration<MAX_DURATION_F007TH ) max_duration = MAX_DURATION_F007TH;
  };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
  bool decode(ReceivedData* message) {
    if (message->sensorData.protocol != NULL) {
      return (message->sensorData.protocol == this);
    }

    Bits bits(message->iSequenceSize+1);

    if (!decodeManchester(message, bits)) {
      message->decodingStatus |= 4;
      return false;
    }
    int size = bits.getSize();
    message->decodedBits = (uint16_t)size;
    if (size < 56) {
      message->decodingStatus |= 8;
      return false;
    }

    bool f007TP = false;
    int dataIndex; // index of the first bit of data after preamble and fixed ID (0x45)
    int index = bits.findBits( 0x00007d45, 15 ); // 1111 1101 0100 0101 shortened preamble + fixed ID (0x45)
    if (index<0) {
      index = bits.findBits( 0x00007d46, 15 ); // F007TP fixed ID = 0x46
      if (index<0) {
        message->decodingStatus |= 16; // could not find preamble
        return false;
      }
      f007TP = true;
    }
    index--;

    if (index+56<size) {
      dataIndex = index+16;
    } else if (index>49 && bits.getInt(index-9, 9) == 0x1f) {
      // a valid data from previous message before preamble(11 bits) and 4 bits 0000
      dataIndex = index-49;
    } else if (index+48<size) {
      // hash code is missing but it is better than nothing
      dataIndex = index+16;
    } else {
      message->decodingStatus |= 32; // not enough data
      return false;
    }

    if (dataIndex+40>size) {
      message->decodingStatus |= 64; // hash code is missing - cannot check it
    } else {
      // Checking of hash for Ambient Weather F007TH.
      // See https://eclecticmusingsofachaoticmind.wordpress.com/2015/01/21/home-automation-temperature-sensors/

      int bit;
      uint32_t t;
      bool good = false;
      int checking_data = dataIndex-8;
      do {
        int mask = 0x7C;
        int calculated_hash = 0x64;
        for (int i = checking_data; i < checking_data+40; ++i) {
          bit = mask & 1;
          mask = (((mask&0xff) >> 1 ) | (mask << 7)) & 0xff;
          if ( bit ) mask ^= 0x18;
          if ( bits.getBit(i) ) calculated_hash ^= mask;
        }
        int expected_hash = bits.getInt(checking_data+40, 8);
        if (((expected_hash^calculated_hash) & 255) == 0) {
          good = true;
          dataIndex = checking_data+8;
          break;
        }
        // Bad hash. But message is repeated up to 3 times. Try the next message.
        checking_data += 65;
      } while(checking_data+48 < size && ((t=bits.getInt(checking_data-17, 21) == 0x1ffd45) || t == 0x1ffd46));
      if (!good) {
        message->decodingStatus |= 128; // failed hash code check
      }
    }

    uint32_t data = bits.getInt(dataIndex, 32);
    message->sensorData.nF007TH = data;
    message->sensorData.protocol = this;
    message->sensorData.u32.hi = f007TP ? 1 : 0;
    return true;
  }
#pragma GCC diagnostic pop

  bool decodeManchester(ReceivedData* message, Bits& bitSet) {
    DBG("ProtocolF007TH::decodeManchester()");
    message->protocol_tried_manchester |= 1<<PROTOCOL_INDEX_F007TH;
    return Protocol::decodeManchester(message, bitSet, MIN_DURATION_F007TH, MAX_HALF_DURATION);
  }

};

ProtocolF007TH* ProtocolF007TH::instance = new ProtocolF007TH();
