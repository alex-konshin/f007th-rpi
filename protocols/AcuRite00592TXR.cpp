/*
 * AcuRite00592TXR.cpp
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/RFReceiver.hpp"

// AcuRite 00592TXR - 200...600
#define MIN_DURATION_00592TXR 140
#define MAX_DURATION_00592TXR 660

class Protocol00592TXR : public Protocol {
public:
  Protocol00592TXR() : Protocol(PROTOCOL_00592TXR,PROTOCOL_INDEX_00592TXR) {
  }

  static Protocol00592TXR* instance;

  uint32_t getId(SensorData* data) {
    uint32_t channel_bits = (data->fields.channel>>6)&3;
    uint32_t rolling_code = data->fields.rolling_code;
    return (protocol_bit<<24) | (channel_bits<<8) | rolling_code;
  }

  int getChannel(SensorData* data) { return ((data->fields.channel>>6)&3)^3; }
  int getChannelNumber(SensorData* data) {
    switch ((data->fields.channel>>6)&3) {
    case 3: return 1;
    case 2: return 2;
    case 0: return 3;
    default:
      return -1;
    }
  }
  const char* getChannelName(SensorData* data) {
    switch ((data->fields.channel>>6)&3) {
    case 3: return "A";
    case 2: return "B";
    case 0: return "C";
    }
    return NULL;
  }

  int getMetrics(SensorData* data) { return METRIC_TEMPERATURE | METRIC_HUMIDITY | METRIC_BATTERY_STATUS; }

  bool hasBatteryStatus() { return true; }
  bool getBatteryStatus(SensorData* data) { return data->fields.status==0x44; }

  int getTemperatureCx10(SensorData* data) { return (int)((((data->fields.t_hi)&127)<<7) | (data->fields.t_low&127)) - 1000; }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) {
    int c = (int)((((data->fields.t_hi)&127)<<7) | (data->fields.t_low&127)) - 1000;
    return (c*9/5)+320;
  }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return true; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return data->fields.rh & 127; }

  const char* getSensorTypeName(SensorData* data) { return "00592TXR"; }
  const char* getSensorTypeLongName(SensorData* data) { return "AcuRite 00592TXR"; }

  // random number that is changed when battery is changed
  uint8_t getRollingCode(SensorData* data) { return data->fields.rolling_code; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (p->fields.rolling_code == s->fields.rolling_code) && (p->fields.channel == s->fields.channel);
  }

  bool sameId(SensorData* s, int channel, uint8_t rolling_code = -1) {
    if (rolling_code != -1 && s->fields.rolling_code != rolling_code) return false;
    return s->fields.channel == channel;
  }

  int update(SensorData* sensorData, SensorData* item, time_t data_time, time_t max_unchanged_gap) {
    sensorData->def = item->def;
    time_t gap = data_time - item->data_time;
    if (gap < 2L) return TIME_NOT_CHANGED;

    int result = 0;
    if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
      result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
    } else {
      if (item->fields.t_low != sensorData->fields.t_low || item->fields.t_hi != sensorData->fields.t_hi) result |= TEMPERATURE_IS_CHANGED;
      if (item->fields.rh != sensorData->fields.rh) result |= HUMIDITY_IS_CHANGED;
      if (item->fields.status != sensorData->fields.status) result |= BATTERY_STATUS_IS_CHANGED;
    }
    if (result != 0) {
      item->u64 = sensorData->u64;
      item->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  00592TXR data     = %02x %02x %02x %02x %02x %02x %02x\n",
        sensorData->fields.channel, sensorData->fields.rolling_code, sensorData->fields.status, sensorData->fields.rh,
        sensorData->fields.t_hi, sensorData->fields.t_low, sensorData->fields.checksum);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_DURATION_00592TXR ) min_duration = MIN_DURATION_00592TXR;
    if ( max_duration==0 || max_duration<MAX_DURATION_00592TXR ) max_duration = MAX_DURATION_00592TXR;
  };

  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = 0;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < 121 || iSequenceSize > 200) {
      message->decodingStatus |= 8;
      return false;
    }

    int16_t* pSequence = message->pSequence;

    // check sync sequence. Expecting 8 items with values around 600

    int dataStartIndex = -1;
    int failedIndex = 0;
    for ( int index = 0; index<=(iSequenceSize-121); index+=2 ) {
      bool good = true;
      for ( int i = 0; i<8; i+=2) {
        failedIndex = index+i;
        int16_t item1 = pSequence[failedIndex];
        if (item1 < 400 || item1 > 1000) {
          good = false;
          break;
        }
        int16_t item2 = pSequence[failedIndex+1];
        if (item1 < 400 || item1 > 1000) {
          good = false;
          failedIndex++;
          break;
        }
        int16_t pair = item1+item2;
        if (pair < 1000 || pair > 1450) {
          good = false;
          break;
        }
      }
      if (good) { // expected around 200 or 400
        failedIndex = index+8;
        int16_t item = pSequence[failedIndex];
        if (item > 680 || item < 120) {
          good = false;
          continue;
        }
        dataStartIndex = index+8;
        break;
      }
    }
    if (dataStartIndex < 0) {
      //printf("decode00592TXR(): bad sync sequence\n");
      message->decodingStatus |= (16 | (failedIndex<<8));
      return false;
    }

    //printf("decode00592TXR(): detected sync sequence\n");

    Bits bits(56);

    for ( int index = dataStartIndex; index<iSequenceSize-1; index+=2 ) {
      int16_t item1 = pSequence[index];
      if (item1 < 120 || item1 > 680) {
        message->decodingStatus |= (4 | (index<<8));
        return false;
      }
      int16_t item2 = pSequence[index+1];
      if (item2 < 120 || item2 > 680) {
        message->decodingStatus |= (5 | ((index+1)<<8));
        return false;
      }
      if (item1 < 290 && item2 > 310) {
        bits.addBit(0);
      } else if (item2 < 290 && item1 > 310) {
        bits.addBit(1);
      } else {
        //printf("decode00592TXR(): bad items %d: %d %d\n", index, item1, item2);
        message->decodingStatus |= (6 | (index<<8));
        return false;
      }
    }
    if (bits.getSize() != 56) {
      //printf("decode00592TXR(): bits.getSize() != 56\n");
      message->decodingStatus |= 32;
      return false;
    }

    unsigned status = bits.getInt(16,8)&255;
    if (status != 0x0044u && status != 0x0084) {
      ///printf("decode00592TXR(): bad status byte 0x%02x\n",status);
      message->decodingStatus |= 128;
      return false;
    }

    unsigned checksum = 0;
    for (int i = 0; i < 48; i+=8) {
      checksum += bits.getInt(i, 8);
    }

    if (((bits.getInt(48,8)^checksum)&255) != 0) {
      //printf("decode00592TXR(): bad checksum\n");
      message->decodingStatus |= 128;
      return false;
    }

    uint64_t n00592TXR = bits.getInt64(0, 56);

    message->sensorData.u64 = n00592TXR;
    message->sensorData.protocol = this;
    return true;
  }


};

Protocol00592TXR* Protocol00592TXR::instance = new Protocol00592TXR();


