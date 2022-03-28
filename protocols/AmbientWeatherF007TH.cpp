/*
 * Ambient Weather F007TH
 *
 *  Created on: December 9, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

// Ambient Weather F007TH
#define MIN_DURATION_F007TH 340
#define MAX_DURATION_F007TH 1150
#define MAX_HALF_DURATION 600

static ProtocolDef def_f007th = {
  name : "f007th",
  protocol_bit: PROTOCOL_F007TH,
  protocol_index: PROTOCOL_INDEX_F007TH,
  variant: 0,
  rolling_code_size: 8,
  number_of_channels: 8,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};
static ProtocolDef def_f007tp = {
  name : "f007tp",
  protocol_bit: PROTOCOL_F007TH,
  protocol_index: PROTOCOL_INDEX_F007TH,
  variant: 1,
  rolling_code_size: 8,
  number_of_channels: 8,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};


static ProtocolThresholds limits = {
  min_sequence_length: 85,
  min_bits: 56,
  low: {
    425,  // short_min
    660,  // short_max
    950, // long_min
    1150  // long_max
  },
  high: {
    320,  // short_min
    600,  // short_max
    820,  // long_min
    1010  // long_max
  }
};


class ProtocolF007TH : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      if (strcasecmp(def_f007th.name, protocol_name) == 0) return &def_f007th;
      if (strcasecmp(def_f007tp.name, protocol_name) == 0) return &def_f007tp;
    }
    return NULL;
  }

public:
  static ProtocolF007TH* instance;

  ProtocolF007TH() : Protocol(PROTOCOL_F007TH, PROTOCOL_INDEX_F007TH, "F007TH",
      FEATURE_RF | FEATURE_CHANNEL | FEATURE_ROLLING_CODE | FEATURE_TEMPERATURE | FEATURE_HUMIDITY | FEATURE_BATTERY_STATUS ) {}

  uint64_t getId(SensorData* data) {
    uint64_t variant = data->u32.hi==1 ? 1 : 0; // 0 = F007TH, 1 = F007TP
    uint64_t channel_bits = (data->nF007TH>>20)&7UL;
    uint64_t rolling_code = (data->nF007TH >> 24) & 255UL;
    return ((uint64_t)protocol_index<<48) | (variant<<16) | (channel_bits<<8) | rolling_code;
  }
  uint64_t getId(ProtocolDef *protocol_def, uint8_t channel, uint16_t rolling_code) {
    uint64_t channel_bits = (channel-1)&7UL;
    return ((uint64_t)protocol_index<<48) | (((uint64_t)protocol_def->variant&0x0ffff)<<16) | (channel_bits<<8) | (rolling_code&255UL);
  }

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
  uint16_t getRollingCode(SensorData* data) { return (data->nF007TH >> 24) & 255; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && ((p->nF007TH ^ s->nF007TH) & SENSOR_UID_MASK) == 0 && p->u32.hi == s->u32.hi; // u32.hi -- F007TP flag
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

  bool decode(ReceivedData* message) {
    if (message->sensorData.protocol != NULL) {
      return (message->sensorData.protocol == this);
    }
    message->decodingStatus = 0;

    Bits bits(message->iSequenceSize+1);
    if (!decodeManchester(message, bits, limits)) {
      if (message->decodingStatus == 0) message->decodingStatus = 4;
      return false;
    }
    return true;
  }

  bool processDecodedBits(ReceivedData* message, Bits& bits) {
    int size = bits.getSize();
    message->decodedBits = (uint16_t)size;
    if (size < 56) {
      message->decodingStatus = 8;
      return false;
    }
#define PREAMBLE_MIN_LEN 14
    bool f007TP = false;
    int dataIndex; // index of the first bit of data after preamble and fixed ID (0x45)
    int index = bits.findBits( 0x0000fd45, PREAMBLE_MIN_LEN ); // 1111 1101 0100 0101 shortened preamble + fixed ID (0x45)
    if (index<0) {
      index = bits.findBits( 0x0000fd46, PREAMBLE_MIN_LEN ); // F007TP fixed ID = 0x46
      if (index<0) {
        message->decodingStatus = 0x10; // could not find preamble
        return false;
      }
      f007TP = true;
    }
    index -= 16-PREAMBLE_MIN_LEN;

    if (index+56<size) {
      dataIndex = index+16;
    } else if (index>49 && bits.getInt(index-9, 9) == 0x1f) {
      // a valid data from previous message before preamble(11 bits) and 4 bits 0000
      dataIndex = index-49;
    } else if (index+48<size) {
      // hash code is missing but it is better than nothing
      dataIndex = index+16;
    } else {
      message->decodingStatus = 0x20; // not enough data
      return false;
    }

    if (dataIndex+40>size) {
      message->decodingStatus = 0x40; // hash code is missing - cannot check it
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
        message->decodingStatus = 0x80; // failed hash code check
        return false;
      }
    }

    uint32_t data = bits.getInt(dataIndex, 32);
    message->sensorData.nF007TH = data;
    message->sensorData.protocol = this;
    message->sensorData.u32.hi = f007TP ? 1 : 0;
    return true;
  }

  void printUndecoded(ReceivedData* message, FILE *out, FILE *log, int cfg_options) {
    if ((message->detailedDecodingStatus[PROTOCOL_INDEX_F007TH] & 7)!=0 ) return;

    uint16_t savedDecodingStatus = message->decodingStatus;
    Bits bits(message->iSequenceSize+1);
    bool is_manchester_successful = decodeManchester(message, bits, limits);
    message->decodingStatus = savedDecodingStatus;
    if (is_manchester_successful) {

      FILE* file = out;
      FILE* file2 = log;
      do {
        fprintf(file, "  Manchester decoding was successful\n");
        ReceivedMessage::printBits(file, &bits);

        // Repeat for log file
        file = file2;
        file2 = NULL;
      } while (file != NULL);
    }
  }

};

ProtocolF007TH *ProtocolF007TH::instance = new ProtocolF007TH();

