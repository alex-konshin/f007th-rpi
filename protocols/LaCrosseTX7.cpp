/*
 * LaCrosseTX7.cpp
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/RFReceiver.hpp"

// LaCrosse TX-6U/TX-7U
#define MIN_DURATION_TX7U 400
#define MAX_DURATION_TX7U 1500


class ProtocolTX7U : public Protocol {

  ProtocolTX7U() : Protocol(PROTOCOL_TX7U, PROTOCOL_INDEX_TX7U, "TX7U") {
  }
  static ProtocolTX7U* instance;

  uint32_t getId(SensorData* data) {
    uint32_t rolling_code = data->u32.low >> 25;
    return (protocol_bit<<24) | rolling_code;
  }

  int getMetrics(SensorData* data) {
    uint8_t type = data->u32.hi&15;
    return type == 0 ? METRIC_TEMPERATURE : type == 14 ? METRIC_HUMIDITY : 0;
  }

  bool hasTemperature(SensorData* data) {return (data->u32.hi&15)==0 || (data->u32.hi&0x00800000)!=0; }
  inline int getTX7temperature(SensorData* data) {
    return
        (data->u32.hi&0x00800000)!=0
        ? (data->u32.hi>>12)&0x07ff
        : ((data->u32.low >> 20)&15)*100 + ((data->u32.low >> 16)&15)*10 + ((data->u32.low >> 12)&15);
  }
  inline int getTX7humidity(SensorData* data) {
    return
        (data->u32.hi&0x80000000)!=0
        ? (data->u32.hi>>24)&0x7f
        : ((data->u32.low >> 20)&15)*10 + ((data->u32.low >> 16)&15);
  }
  int getTemperatureCx10(SensorData* data) { return data->hasTemperature() ? getTX7temperature(data)-500 : -2732; }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) { return data->hasTemperature() ? ((getTX7temperature(data)-500)*9/5)+320 : -4597; }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return (data->u32.hi&15)==14 || (data->u32.hi&0x80000000)!=0; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return hasHumidity(data) ? getTX7humidity(data) : 0; }

  const char* getSensorTypeName(SensorData* data) { return "TX7"; }
  const char* getSensorTypeLongName(SensorData* data) { return "LaCrosse TX3/TX6/TX7"; }

  // random number that is changed when battery is changed
  uint8_t getRollingCode(SensorData* data) { return (data->u32.low >> 25); }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && ((p->u64>>25)&0x7f) == ((s->u64>>25)&0x7f);
  }

  bool sameId(SensorData* s, int channel, uint8_t rolling_code = -1) {
    return rolling_code == -1 || ((s->u32.low >> 25)&0x7f) == rolling_code;
  }

  int update(SensorData* sensorData, SensorData* item, time_t data_time, time_t max_unchanged_gap) {
    uint8_t type = sensorData->u32.hi&15;
    uint32_t mask;
    uint32_t new_value;
    int result = 0;
    if (type == 0) {
      new_value = ((sensorData->u32.low >> 20)&15)*100 + ((sensorData->u32.low >> 16)&15)*10 + ((sensorData->u32.low >> 12)&15);
      new_value = 0x00800000 | (new_value << 12);
      mask = 0x00fff000;
      result = TEMPERATURE_IS_CHANGED;
    } else if (type == 14) {
      new_value = (((sensorData->u32.low >> 20)&15)*10 + ((sensorData->u32.low >> 16)&15))&0x7f;
      new_value = 0x80000000 | (new_value << 24);
      mask = 0xff000000;
      result = HUMIDITY_IS_CHANGED;
    } else {
      return 0;
    }

    sensorData->def = item->def;
    if (((item->u32.hi)&mask) == new_value) {
      time_t gap = data_time - item->data_time;
      if (gap < 2L) return TIME_NOT_CHANGED;
      return (max_unchanged_gap > 0L && gap > max_unchanged_gap) ? result : 0;
    }
    item->data_time = data_time;
    item->u32.low = sensorData->u32.low;
    item->u32.hi = (item->u32.hi & ~mask) | new_value;
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  TX3/TX6/TX7 data  = %03x%08x\n", (sensorData->u32.hi&0xfff), sensorData->u32.low); //FIXME
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_DURATION_TX7U ) min_duration = MIN_DURATION_TX7U;
    if ( max_duration==0 || max_duration<MAX_DURATION_TX7U ) max_duration = MAX_DURATION_TX7U;
  };

  /*
   * Decoding LaCrosse TX6U/TX7U
   *
   * zero = 1400us high, 1000 us low
   * one = 550us high, 1000 us low
   *
   * https://web.archive.org/web/20141003000354/http://www.f6fbb.org:80/domo/sensors/tx3_th.php
   *
   */
  #define TX7U_LOW_MIN 800
  #define TX7U_LOW_MAX 1200
  #define TX7U_0_MIN 1100
  #define TX7U_0_MAX 1500
  #define TX7U_1_MIN 400
  #define TX7U_1_MAX 650

  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = 0;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < 87 || iSequenceSize > 240) {
      message->decodingStatus |= 8;
      return false;
    }

    int16_t* pSequence = message->pSequence;
    int16_t item;
    // skip till sync sequence
    // 1400,1000,1400,1000,1400,1000,1400,1000

    int failIndex = 0;
    int dataStartIndex = -1;
    bool good_start = false;
    for ( int index = 0; index<(iSequenceSize-86); index++ ) {
      // bit[0] == 0
      failIndex = index;
      item = pSequence[failIndex];
      if (item<=TX7U_0_MIN || item>=TX7U_0_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      // bit[1] == 0
      item = pSequence[++failIndex];
      if (item<=TX7U_0_MIN || item>=TX7U_0_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      // bit[2] == 0
      item = pSequence[++failIndex];
      if (item<=TX7U_0_MIN || item>=TX7U_0_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      good_start = true;

      // bit[3] == 0
      item = pSequence[++failIndex];
      if (item<=TX7U_0_MIN || item>=TX7U_0_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      // bit[4] == 1
      item = pSequence[++failIndex];
      if (item<=TX7U_1_MIN || item>=TX7U_1_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      // bit[5] == 0
      item = pSequence[++failIndex];
      if (item<=TX7U_0_MIN || item>=TX7U_0_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      // bit[6] == 1
      item = pSequence[++failIndex];
      if (item<=TX7U_1_MIN || item>=TX7U_1_MAX) continue;
      item = pSequence[++failIndex];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) continue;

      // bit[7] == 0
      item = pSequence[++failIndex];
      if (item<=TX7U_0_MIN || item>=TX7U_0_MAX) continue;
      item = pSequence[++failIndex];
      if (item>TX7U_LOW_MIN && item<TX7U_LOW_MAX) {
        dataStartIndex = index;
        break;
      }
    }
    if ( dataStartIndex==-1 ) {
      if (good_start)
        message->decodingStatus |= (16 | 1);
      else
        message->decodingStatus |= (16 | (failIndex<<8));
      return false;
    }

    // Decoding bits

    Bits bits(44);
    for ( int index = dataStartIndex; index<86; index+=2 ) {

      item = pSequence[index+1];
      if (item<=TX7U_LOW_MIN || item>=TX7U_LOW_MAX) {
        message->decodingStatus |= (4 | ((index+1)<<8));
        return false;
      }

      item = pSequence[index];
      if (item>TX7U_0_MIN && item<TX7U_0_MAX) {
        bits.addBit(0);
      } else if (item>TX7U_1_MIN && item<TX7U_1_MAX) {
        bits.addBit(1);
      } else {
        message->decodingStatus |= (4 | (index<<8));
        return false;
      }
    }
    item = pSequence[86];
    if (item>TX7U_0_MIN && item<TX7U_0_MAX) {
      bits.addBit(0);
    } else if (item>TX7U_1_MIN && item<TX7U_1_MAX) {
      bits.addBit(1);
    } else {
      message->decodingStatus |= (4 | (86<<8));
      return false;
    }

    uint64_t nTX7U = bits.getInt64(0, 44);

    message->sensorData.u64 = nTX7U;
    message->sensorData.protocol = this;
    message->decodedBits = (uint16_t)bits.getSize();

    uint32_t n = bits.getInt(8, 32);
    if ( n==0 ) {
      message->decodingStatus |= 0x0080;
      return false;
    }
    if ( (bits.getInt(20,8)&255)!=(bits.getInt(32,8)&255) ) {
      message->decodingStatus |= 0x0180;
      return false;
    }

    // parity check (bit 19)
    uint32_t k = 0;
    for (int i=0; i<8; i++) {
      k ^= (n&15);
      n = n>>4;
    }
    if ( ((1<<k)&0b0110100110010110)!=0 ) {
      message->decodingStatus |= 0x0280;
      return false;
    }

    unsigned checksum = 0;
    for (int i = 0; i < 40; i+=4) {
      checksum += bits.getInt(i, 4);
    }
    if (((bits.getInt(40,4)^checksum)&15) != 0) {
      message->decodingStatus |= 0x0380;
      return false;
    }

    return true;
  }

};

ProtocolTX7U* ProtocolTX7U::instance = new ProtocolTX7U();


