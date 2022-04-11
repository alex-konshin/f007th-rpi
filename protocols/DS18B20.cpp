/*
 * DS18B20.cpp
 *
 *  Created on: December 30, 2020
 *      Author: Alex Konshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

//

static ProtocolDef def_ds18b20 = {
  name : "ds18b20",
  protocol_bit: PROTOCOL_DS18B20,
  protocol_index: PROTOCOL_INDEX_DS18B20,
  variant: 0,
  rolling_code_size: 7,
  number_of_channels: 0,
  channels_numbering_type: 0 // 0 => numbers, 1 => letters
};

class ProtocolDS18B20 : public Protocol {
protected:
  ProtocolDef* _getProtocolDef(const char* protocol_name) {
    if (protocol_name != NULL) {
      if (strcasecmp(def_ds18b20.name, protocol_name) == 0) return &def_ds18b20;
    }
    return NULL;
  }

public:
  static ProtocolDS18B20* instance;

  ProtocolDS18B20() : Protocol(PROTOCOL_DS18B20, PROTOCOL_INDEX_DS18B20, "DS18B20",
      FEATURE_ID32 | FEATURE_TEMPERATURE | FEATURE_TEMPERATURE_CELSIUS ) {}

  uint64_t getId(SensorData* data) {
    return ((uint64_t)protocol_index<<48) | data->u32.hi;
  }

  int getMetrics(SensorData* data) {
    return METRIC_TEMPERATURE;
  }

  int getTemperatureCx10(SensorData* data) { return (int)(((long)data->u32.low+50)/100); }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10(SensorData* data) { return (((long)data->u32.low*9+250)/500)+320; }
  bool isRawTemperatureCelsius() { return true; }
  int getRawTemperature(SensorData* data) { return getTemperatureCx10(data); }

  const char* getSensorTypeName(SensorData* data) { return "DS18B20"; }
  const char* getSensorTypeLongName(SensorData* data) { return "1-wire DS18B20"; }

  bool equals(SensorData* s, SensorData* p) {
    return (p->protocol == s->protocol) && (p->u32.hi == s->u32.hi);
  }

  void copyFields(SensorData* to, SensorData* from) {
    to->data_time = from->data_time;
    to->u32.low = from->u32.low;
    to->u32.hi = from->u32.hi;
  }

  int update(SensorData* sensorData, SensorData* item, time_t data_time, time_t max_unchanged_gap) {
    sensorData->def = item->def;
    if (item->u32.low == sensorData->u32.low) { // the value has not changed
      time_t gap = data_time - item->data_time;
      if (gap < 2L) return TIME_NOT_CHANGED;
      if (max_unchanged_gap == 0L || gap < max_unchanged_gap) return 0;
      item->data_time = data_time;
      return TEMPERATURE_IS_CHANGED;
    }
    item->data_time = data_time;
    item->u32.low = sensorData->u32.low;
    return TEMPERATURE_IS_CHANGED;
  }

  void printRawData(SensorData* sensorData, FILE* file) {
    fprintf(file, "  DS18B20 data  = %08x %d\n", sensorData->u32.hi, (int32_t)sensorData->u32.low);
  }

  bool decode(ReceivedData* message) {
    message->decodingStatus = 0;
    message->decodedBits = 0;
    message->sensorData.protocol = this;
    return true;
  }

};

ProtocolDS18B20* ProtocolDS18B20::instance = new ProtocolDS18B20();


