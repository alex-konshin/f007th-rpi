/*
 * AcuRite00592TXR.cpp
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

// AcuRite 00592TXR - 200...600
#define MIN_DURATION_00592TXR 140
#define MAX_DURATION_00592TXR 660

//#define DEBUG_00592TXR

#if defined(NDEBUG)||!defined(DEBUG_00592TXR)
#define DBG_00592TXR(format, arg...)     ((void)0)
#else
#define DBG_00592TXR(format, arg...)  Log->info(format, ## arg)
#endif


static ProtocolDef def_00592txr = {
  name : "00592txr",
  protocol_bit: PROTOCOL_00592TXR,
  protocol_index: PROTOCOL_INDEX_00592TXR,
  variant: 0x04,
  rolling_code_size: 12,
  number_of_channels: 3,
  channels_numbering_type: 1 // 0 => numbers, 1 => letters
};


class Protocol00592TXR : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      if (strcasecmp(def_00592txr.name, protocol_name) == 0) return &def_00592txr;
    }
    return NULL;
  }

public:
  static Protocol00592TXR* instance;

  Protocol00592TXR() : Protocol(PROTOCOL_00592TXR, PROTOCOL_INDEX_00592TXR, "00592TXR",
    FEATURE_RF | FEATURE_CHANNEL | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS | FEATURE_HUMIDITY | FEATURE_BATTERY_STATUS ) {}

  uint64_t getId(SensorData* data) {
    uint64_t channel_bits = (data->fields.channel >> 6) & 3UL;
    uint64_t rolling_code = (data->u32.hi >> 8) & 0xfffUL;
    uint64_t variant = data->fields.status & 0x3fUL;
    return ((uint64_t)protocol_index<<48) | (variant<<16) | (channel_bits<<14) | rolling_code;
  }

  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    uint64_t channel_bits = 0;
    switch (channel) {
    case 1: channel_bits = 3; break;
    case 2: channel_bits = 2; break;
    case 3: channel_bits = 0; break;
    }
    return ((uint64_t)protocol_index<<48) | ((protocol_def->variant&0x0ffffUL)<<16) | (channel_bits<<14) | (rolling_code&0x0fff);
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
  bool getBatteryStatus(SensorData* data) { return (data->fields.status&0x40) != 0; }

  int getTemperatureCx10(SensorData* data) { return (int)((((data->fields.t_hi)&127)<<7) | (data->fields.t_low&127)) - 1000; }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) {
    int t = (int)((((data->fields.t_hi)&127)<<7) | (data->fields.t_low&127)) - 1000;
    return (int)((t*90+25)/50+320);
  }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return true; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return data->fields.rh & 127; }

  const char* getSensorTypeName(SensorData* data) { return "00592TXR"; }
  const char* getSensorTypeLongName(SensorData* data) { return "AcuRite 00592TXR"; }

  // random number that is changed when battery is changed
  uint16_t getRollingCode(SensorData* data) { return (data->u32.hi >> 8) & 0xfff; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (p->fields.rolling_code == s->fields.rolling_code) && (p->fields.channel == s->fields.channel);
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
    message->sensorData.protocol = NULL;
    Bits bits(56);

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < 121) {
      message->decodingStatus = 8;
      return false;
    }

    uint16_t decodingStatus = 0;
    int startIndex = 0;

    while ( !try_decode(message, startIndex, bits, decodingStatus) ) {
      if (startIndex < 0 || startIndex+121 > iSequenceSize) { // it was the last attempt
        if (message->decodingStatus == 0 || (decodingStatus&128) != 0) {
          message->decodingStatus = decodingStatus;
          if ((decodingStatus&128) != 0) {
            message->decodedBits = bits.getSize();
          } else {
            bits.clear();
          }
        }
        return false;
      }
      // cleanup before next attempt
      if ((decodingStatus&128) != 0) {
        message->decodingStatus = decodingStatus;
        message->decodedBits = bits.getSize();
      } else if ((decodingStatus&0x20) != 0 || (message->decodingStatus&128) == 0) { // bits are lost or the previous status had no bits
        message->decodingStatus = decodingStatus;
        message->decodedBits = 0;
        bits.clear();
      }
      decodingStatus = 0;
    }
    return true;
  }

private:
  bool try_decode(ReceivedData* message, int& startIndex, Bits& bits, uint16_t& decodingStatus) {
    decodingStatus = 0;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < startIndex+121) {
      decodingStatus = 8;
      return false;
    }

    int16_t* pSequence = message->pSequence;

    // check sync sequence. Expecting 8 items with values around 600

    DBG_00592TXR("try_decode() startIndex=%d",startIndex);
    int dataStartIndex = -1;
    int failedIndex = 0;
    for ( int index = startIndex; index<=(iSequenceSize-121); index+=2 ) {
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
          failedIndex++;
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
        dataStartIndex = index+8; // data should start from this index
        startIndex = index+2; // set start index for the next attempt if this one fails
        break;
      }
    }
    if (dataStartIndex < 0) {
      DBG_00592TXR("try_decode() pSequence[%d]=%d bad sync sequence",failedIndex,pSequence[failedIndex]);
      decodingStatus = (16 | (failedIndex<<8));
      startIndex = -1;
      return false;
    }

    DBG_00592TXR("try_decode() dataStartIndex=%d",dataStartIndex);
    //printf("decode00592TXR(): detected sync sequence\n");

    bits.clear();

    int decoded_bits_count = 0;
    for ( int index = dataStartIndex; index<iSequenceSize-1; index+=2 ) {
      int16_t item1 = pSequence[index];
      if (item1 < 120 || item1 > 680) {
        if (decoded_bits_count >= 56) break; // We got enough bits for 00592TXR variant
        decodingStatus = (0x20 | (index<<8));
        return false;
      }
      int16_t item2 = pSequence[index+1];
      if (item2 < 120 || item2 > 680) {
        if (decoded_bits_count >= 56) break; // We got enough bits for 00592TXR variant
        decodingStatus = (0x21 | ((index+1)<<8));
        return false;
      }
      if (item1 < 290 && item2 > 310) {
        bits.addBit(0);
      } else if (item2 < 290 && item1 > 310) {
        bits.addBit(1);
      } else {
        if (decoded_bits_count >= 56) break; // We got enough bits for 00592TXR variant
        //printf("decode00592TXR(): bad items %d: %d %d\n", index, item1, item2);
        decodingStatus = (0x22 | (index<<8));
        return false;
      }
      if (++decoded_bits_count >= 64) break;
    }
    if (decoded_bits_count < 56) {
      //printf("decode00592TXR(): bits.getSize() != 56\n");
      decodingStatus = 0x23;
      return false;
    }

    unsigned checksum = 0;
    uint8_t p = 0;
    for (int i = 0; i < 48; i+=8) {
      uint8_t b = bits.getInt(i, 8);
      checksum += b;
      if (i >= 16) p ^= b;
    }

    p = (p ^ (p>>4)) & 15;
    if (((1 << p) & 0b0110100110010110) != 0) {
      decodingStatus = 0x0080;
      return false;
    }

    if (((bits.getInt(48,8)^checksum)&255) != 0) {
      //printf("decode00592TXR(): bad checksum\n");
      decodingStatus = 0x0081;
      return false;
    }

    message->decodedBits = decoded_bits_count;
    uint64_t n00592TXR = bits.getInt64(0, 56);
    message->sensorData.u64 = n00592TXR;
    message->sensorData.protocol = this;
    return true;
  }

};

Protocol00592TXR* Protocol00592TXR::instance = new Protocol00592TXR();


