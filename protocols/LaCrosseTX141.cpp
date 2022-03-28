/*
 * LaCrosseTX141.cpp
 *
 * LaCrosse TX141-Bv2, TX141TH-Bv2, TX141-Bv3, TX145wsdth sensor.
 *  Also TFA 30.3221.02 (a TX141TH-Bv2),
 * also TFA 30.3222.02 (a LaCrosse-TX141W).
 * also TFA 30.3251.10 (a LaCrosse-TX141W).
 *
 * This implementation is based on the description of the protocol
 * that was found in rtl_433 project:
 *   https://github.com/merbanan/rtl_433/blob/master/src/devices/lacrosse_tx141x.c
 *   (c) 2017 Robert Fraczkiewicz <aromring@gmail.com>
 *
 *  Created on: March 25, 2022
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

#define MIN_SEQUENCE_TX141 88

// preamble
#define PREAMBLE_MIN_LO_DURATION_TX141 720
#define PREAMBLE_MAX_LO_DURATION_TX141 1000
#define PREAMBLE_MIN_HI_DURATION_TX141 720
#define PREAMBLE_MAX_HI_DURATION_TX141 1000

#define MIN_PERIOD_TX141 650
#define MAX_PERIOD_TX141 820

#define BIT0_MIN_HI_DURATION_TX141 140
#define BIT0_MAX_HI_DURATION_TX141 360
#define BIT0_MIN_LO_DURATION_TX141 340
#define BIT0_MAX_LO_DURATION_TX141 630

#define BIT1_MIN_HI_DURATION_TX141 340
#define BIT1_MAX_HI_DURATION_TX141 560
#define BIT1_MIN_LO_DURATION_TX141 160
#define BIT1_MAX_LO_DURATION_TX141 360

// min of all BIT*_MIN_*_DURATION_TX141 and PREAMBLE_MIN_*_DURATION_TX141
#define MIN_DURATION_TX141 140
// min of all BIT*_MIN_*_DURATION_TX141 and PREAMBLE_MAX_*_DURATION_TX141
#define MAX_DURATION_TX141 1000

//#define DEBUG_TX141

#if defined(NDEBUG)||!defined(DEBUG_TX141)
#define DBG_TX141(format, arg...)     ((void)0)
#else
#define DBG_TX141(format, arg...)  Log->info(format, ## arg)
#endif

static ProtocolDef def_tx141 = {
  name : "tx141",
  protocol_bit: PROTOCOL_TX141,
  protocol_index: PROTOCOL_INDEX_TX141,
  variant: 0,
  rolling_code_size: 8,
  number_of_channels: 4,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};

static uint8_t CRC_TABLE[256] = {
      0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
      0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
      0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4,
      0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
      0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11,
      0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
      0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52,
      0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
      0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA,
      0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
      0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9,
      0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
      0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C,
      0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
      0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F,
      0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
      0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED,
      0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
      0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE,
      0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
      0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B,
      0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
      0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28,
      0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
      0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0,
      0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
      0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93,
      0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
      0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56,
      0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
      0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15,
      0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC
  };

class ProtocolTX141 : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      if (strcasecmp(def_tx141.name, protocol_name) == 0) return &def_tx141;
    }
    return NULL;
  }

public:
  static ProtocolTX141* instance;

  ProtocolTX141() : Protocol(PROTOCOL_TX141, PROTOCOL_INDEX_TX141, "TX141",
      FEATURE_RF | FEATURE_CHANNEL | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS | FEATURE_HUMIDITY | FEATURE_BATTERY_STATUS) {}

  uint64_t getId(SensorData* data) {
    uint64_t channel = (data->u32.low >> 20) & 3UL;
    uint64_t rolling_code = (data->u32.low >> 24) & 255UL;
    return ((uint64_t)protocol_index<<48) | rolling_code | (channel<<8);
  }
  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    if (channel > 0) channel--;
    return ((uint64_t)protocol_index<<48) | (channel<<8) | (rolling_code&255UL);
  }

  int getMetrics(SensorData* data) { return METRIC_TEMPERATURE | METRIC_HUMIDITY; }

  int getTemperatureCx10(SensorData* data) {
    return (int)((data->u32.low>>8)&4095)-500;
  }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) {
    int t = (int)((data->u32.low>>8)&4095)-500;
    return (t*90+25)/50+320;
  }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  bool hasHumidity(SensorData* data) { return true; }
  // Relative Humidity, 0..100%
  int getHumidity(SensorData* data) { return data->u32.low & 255; }

  const char* getSensorTypeName(SensorData* data) { return "TX141"; }
  const char* getSensorTypeLongName(SensorData* data) { return "LaCrosse TX141"; }

  // random number that is changed when battery is changed
  uint16_t getRollingCode(SensorData* data) { return (data->u32.low >> 24) & 255; }

  int getChannel(SensorData* data) { return ((data->u32.low >> 20) & 3) + 1; }
  int getChannelNumber(SensorData* data) { return getChannel(data); }
  const char* getChannelName(SensorData* data) { return channel_names_numeric[getChannel(data)]; }

  bool hasBatteryStatus() { return true; }
  bool getBatteryStatus(SensorData* data) { return (data->u32.low & 0x00800000) == 0; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (((p->u32.low^s->u32.low)&0xff300000) == 0); // compare rolling code
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
      if (((w^new_w)&0x000fff00) != 0) result |= TEMPERATURE_IS_CHANGED;
      if (((w^new_w)&0x000000ff) != 0) result |= HUMIDITY_IS_CHANGED;
      if (((w^new_w)&0x00800000) != 0) result |= BATTERY_STATUS_IS_CHANGED;
    }
    if (result != 0) {
      p->u64 = sensorData->u64;
      p->data_time = data_time;
    }
    return result;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  TX141 data  = %08x\n", sensorData->u32.low);
  }

  void adjustLimits(unsigned long& min_sequence_length, unsigned long& max_duration, unsigned long& min_duration) {
    if ( min_duration==0 || min_duration>MIN_DURATION_TX141 ) min_duration = MIN_DURATION_TX141;
    if ( max_duration==0 || max_duration<MAX_DURATION_TX141 ) max_duration = MAX_DURATION_TX141;
    if ( min_sequence_length==0 || min_sequence_length>MIN_SEQUENCE_TX141 ) min_sequence_length = MIN_SEQUENCE_TX141;
  };

  /*
   * Decoding LaCrosse TX141*
   */
public:
  bool decode(ReceivedData* message) {

    /*
     * The TX141TH-Bv2 protocol is OOK modulated PWM with fixed period of 625 us
     * for data bits, preambled by four long startbit pulses of fixed period equal
     * to ~1666 us. Hence, it is similar to Bresser Thermo-/Hygro-Sensor 3CH.
     * A single data packet looks as follows:
     * 1) preamble - 833 us high followed by 833 us low, repeated 4 times:
     *    ----      ----      ----      ----
     *   |    |    |    |    |    |    |    |
     *         ----      ----      ----      ----
     * 2) a train of 40 data pulses with fixed 625 us period follows immediately:
     *    ---    --     --     ---    ---    --     ---
     *   |   |  |  |   |  |   |   |  |   |  |  |   |   |
     *        --    ---    ---     --     --    ---     -- ....
     * A logical 1 is 417 us of high followed by 208 us of low.
     * A logical 0 is 208 us of high followed by 417 us of low.
     * Thus, in the example pictured above the bits are 1 0 0 1 1 0 1 ....
     * The TX141TH-Bv2 sensor sends 12 of identical packets, one immediately following
     * the other, in a single burst. These 12-packet bursts repeat every 50 seconds. At
     * the end of the last packet there are two 833 us pulses ("post-amble"?).
     *
     * The TX141-Bv3 has a revision which only sends 4 packets per transmission.
     *
     * The data is grouped in 5 bytes / 10 nybbles
     *   [id] [id] [flags] [temp] [temp] [temp] [humi] [humi] [chk] [chk]
     * The "id" is an 8 bit random integer generated when the sensor powers up for the
     * first time; "flags" are 4 bits for battery low indicator, test button press,
     * and channel; "temp" is 12 bit unsigned integer which encodes temperature in degrees
     * Celsius as follows:
     * temp_c = temp/10 - 50
     * to account for the -40 C -- 60 C range; "humi" is 8 bit integer indicating
     * relative humidity in %.*
     *
     */

    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = NULL;

    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < MIN_SEQUENCE_TX141) {
      message->decodingStatus = 8;
      return false;
    }

    uint16_t decodingStatus = 0;
    int startIndex = 0;
    Bits bits(40);

    while (startIndex+MIN_SEQUENCE_TX141 <= iSequenceSize) {
      int preambleStart = find_preamble(message, startIndex, decodingStatus);
      if (preambleStart < 0) {
        if (message->decodingStatus == 0) message->decodingStatus = 8;
        break;
      }

      int dataStartIndex = preambleStart+8;

      if ( try_decode(message, dataStartIndex, bits, decodingStatus) ) {
        message->decodingStatus = decodingStatus;
        return true;
      }

      if ((decodingStatus&128) != 0) {
        message->decodingStatus = decodingStatus;
        message->decodedBits = bits.getSize();
      } else if ((decodingStatus&0x20) != 0 || (message->decodingStatus&128) == 0) { // bits are lost or the previous status had no bits
        message->decodingStatus = decodingStatus;
        message->decodedBits = 0;
      }
      decodingStatus = 0;

      startIndex = preambleStart+2;
    }

    return false;
  }


private:
  int find_preamble(ReceivedData* message, int& startIndex, uint16_t& decodingStatus) {
    int iSequenceSize = message->iSequenceSize;
    if (iSequenceSize < MIN_SEQUENCE_TX141) {
      decodingStatus = 8;
      return -1;
    }

    int16_t* pSequence = message->pSequence;
    int16_t item;

    for ( int preambleIndex = startIndex; preambleIndex<=(iSequenceSize-MIN_SEQUENCE_TX141); preambleIndex+=2 ) {
      bool found = true;
      for (int index = 0; index<8; index +=2) {
        item = pSequence[preambleIndex+index];
        if (item<PREAMBLE_MIN_HI_DURATION_TX141 || item>PREAMBLE_MAX_HI_DURATION_TX141) {
          found = false;
          break;
        }
        item = pSequence[preambleIndex+index+1];
        if (item<PREAMBLE_MIN_LO_DURATION_TX141 || item>PREAMBLE_MAX_LO_DURATION_TX141) {
          found = false;
          break;
        }
      }
      if (found) {
        DBG_TX141("find_preamble() preambleIndex=%d", preambleIndex);
        return preambleIndex;
      }
    }

    return -1;
  }

  bool try_decode(ReceivedData* message, int& dataStartIndex, Bits& bits, uint16_t& decodingStatus) {
    decodingStatus = 0;

    //int iSequenceSize = message->iSequenceSize;
    int16_t* pSequence = message->pSequence;

    DBG_TX141("try_decode() dataStartIndex=%d",dataStartIndex);

    bits.clear();

    for ( int index = dataStartIndex; index<dataStartIndex+80; index+=2 ) {
      int16_t itemHi = pSequence[index];
      int16_t itemLo = pSequence[index+1];
      int16_t period = itemHi+itemLo;
      if (period < MIN_PERIOD_TX141 || period > MAX_PERIOD_TX141) {
        decodingStatus = (0x20 | (index<<8));
        DBG_TX141("  failed at index=%d period=%d hi=%d lo=%d", index, period, itemHi, itemLo);
        return false;
      }
      if (itemHi <= BIT0_MAX_HI_DURATION_TX141 && itemHi >= BIT0_MIN_HI_DURATION_TX141 && itemLo >= BIT0_MIN_LO_DURATION_TX141 && itemLo <= BIT0_MAX_LO_DURATION_TX141) {
        bits.addBit(0);
      } else if (itemHi >= BIT1_MIN_HI_DURATION_TX141 && itemHi <= BIT1_MAX_HI_DURATION_TX141 && itemLo >= BIT1_MIN_LO_DURATION_TX141 && itemLo <= BIT1_MAX_LO_DURATION_TX141) {
        bits.addBit(1);
      } else {
        DBG_TX141("  failed at index=%d hi=%d lo=%d", index, itemHi, itemLo);
        decodingStatus = (0x21 | (index<<8));
        return false;
      }
    }

    if (bits.getSize() != 40) {
      decodingStatus = 0x22;
      return false;
    }

    uint32_t data = bits.getInt(0,32);
    uint8_t crc = bits.getInt(32,8) & 255;
    uint8_t calculated_crc = CRC_TABLE[ ((data>>24) & 255) ];
    calculated_crc = CRC_TABLE[ (calculated_crc ^ (data>>16)) & 255 ];
    calculated_crc = CRC_TABLE[ (calculated_crc ^ (data>>8)) & 255 ];
    calculated_crc = CRC_TABLE[ (calculated_crc ^ data) & 255 ];
    calculated_crc = CRC_TABLE[ calculated_crc & 255 ];
    if (calculated_crc != crc) {
      decodingStatus = 0x0080;
      return false;
    }

    message->decodedBits = 40;
    uint64_t n = bits.getInt64(0, 32);
    message->sensorData.u64 = n;
    message->sensorData.protocol = this;
    return true;
  }

};

ProtocolTX141* ProtocolTX141::instance = new ProtocolTX141();
