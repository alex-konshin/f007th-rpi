/*
 * Channels.hpp
 *
 *  Created on: Feb 5, 2017
 *      Author: Alex Konshin
 */

#ifndef SENSORSDATA_HPP_
#define SENSORSDATA_HPP_

#define SENSOR_UID_MASK  0xff700000L
#define SENSOR_TEMPERATURE_MASK 0x000fff00L
#define SENSOR_HUMIDITY_MASK    0x000000ffL
#define SENSOR_BATTERY_MASK     0x00800000L
#define SENSOR_DATA_MASK        (SENSOR_TEMPERATURE_MASK|SENSOR_HUMIDITY_MASK|SENSOR_BATTERY_MASK)

#define TEMPERATURE_IS_CHANGED       1
#define HUMIDITY_IS_CHANGED          2
#define BATTERY_STATUS_IS_CHANGED    4
#define NEW_UID                      8

typedef struct SensorData {
  union {
    uint64_t u64;
    uint32_t nF007TH; // F007TH data - 4 bytes
    struct {
      uint32_t low, hi;
    } u32;
    struct {
      // begin 00592TXR - 7 bytes
      uint8_t checksum;
      uint8_t t_low, t_hi;
      uint8_t rh;
      uint8_t status;
      uint8_t rolling_code;
      uint8_t channel;
      // end 00592TXR

      uint8_t protocol;
    } fields;
  };
} SensorData;



class SensorsData {
private:
  int size;
  int capacity;
  struct SensorData *items;

public:
  SensorsData() {
    items = (SensorData*)calloc(8, sizeof(SensorData));
    capacity = 8;
    size = 0;
  }
  SensorsData(int capacity) {
    if (capacity <= 0) capacity = 8;
    items = (SensorData*)calloc(capacity, sizeof(SensorData));
    this->capacity = capacity;
    size = 0;
  }

  ~SensorsData() {
    size = 0;
    capacity = 0;
    free(items);
    items = __null;
  }

  inline int getSize() { return size; }

  SensorData* getItem(int index) {
    return index<0 || index>=size ? __null : &items[index];
  }

  SensorData* getData(uint8_t protocol, int channel, int rolling_code = -1) {
    if (protocol != PROTOCOL_F007TH && protocol != PROTOCOL_00592TXR) return __null;
    if (channel < 1 || channel > 8) return 0;
    if (rolling_code > 255 || (rolling_code < 0 && rolling_code != -1)) return __null;

    if (protocol == PROTOCOL_F007TH) {
      uint32_t mask;
      uint32_t uid = ((uint32_t)channel-1L) << 20;
      if (rolling_code == -1) {
        mask = 0x00700000L;
      } else {
        uid |= (((uint32_t)rolling_code) << 24);
        mask = SENSOR_UID_MASK;
      }
      for (int index = size-1; index>=0; index--) {
        SensorData* p = &items[index];
        if ((p->nF007TH & mask) == uid) return p;
      }
    } else if (protocol == PROTOCOL_00592TXR) {
      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->fields.protocol != PROTOCOL_00592TXR) continue;
        if (rolling_code != -1 && p->fields.rolling_code != rolling_code) continue;
        if (p->fields.channel == channel) return p;
      }
    }
    return __null;
  }

  int update(SensorData* sensorData) {
    if (sensorData->fields.protocol == PROTOCOL_F007TH) {
      uint32_t nF007TH = sensorData->nF007TH;
      uint32_t uid = nF007TH & SENSOR_UID_MASK;
      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->fields.protocol != PROTOCOL_F007TH) continue;
        uint32_t item = p->nF007TH;
        if ((item & SENSOR_UID_MASK) == uid) {
          uint32_t changed = (item ^ nF007TH)& SENSOR_DATA_MASK;
          if (changed == 0) return 0;
          p->u64 = sensorData->u64;
          int result = 0;
          if ((changed&SENSOR_TEMPERATURE_MASK) != 0) result |= TEMPERATURE_IS_CHANGED;
          if ((changed&SENSOR_HUMIDITY_MASK) != 0) result |= HUMIDITY_IS_CHANGED;
          if ((changed&BATTERY_STATUS_IS_CHANGED) != 0) result |= BATTERY_STATUS_IS_CHANGED;
          return result;
        }
      }
      add(sensorData);
      return TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED | NEW_UID;
    }

    if (sensorData->fields.protocol == PROTOCOL_00592TXR) {
      uint8_t channel = sensorData->fields.channel;
      uint8_t rolling_code = sensorData->fields.rolling_code;

      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->fields.protocol != PROTOCOL_00592TXR) continue;
        if (p->fields.rolling_code != rolling_code) continue;
        if (p->fields.channel == channel) {
          int result = 0;
          if (p->fields.t_low != sensorData->fields.t_low || p->fields.t_hi != sensorData->fields.t_hi)
            result |= TEMPERATURE_IS_CHANGED;
          if (p->fields.rh != sensorData->fields.rh) result |= HUMIDITY_IS_CHANGED;
          if (p->fields.status != sensorData->fields.status) result |= BATTERY_STATUS_IS_CHANGED;

          if (result != 0) p->u64 = sensorData->u64;
          return result;
        }
      }
      add(sensorData);
      return TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED | NEW_UID;
    }

    if (sensorData->fields.protocol == PROTOCOL_TX7U) {
      uint16_t id = (sensorData->u64>>25)&0x7ff; // type and rolling_code
      uint16_t value = (sensorData->u32.low>>12)&0x0fff;
      uint8_t type = sensorData->u32.hi&15;

      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if ((p->fields.protocol != PROTOCOL_TX7U) || (((p->u64>>25)&0x7ff) != id)) continue;

        int result = 0;
        if (((p->u32.low>>12)&0x0fff) != value) {
          if (type == 0)
            result |= TEMPERATURE_IS_CHANGED;
          else if (type == 14)
            result |= HUMIDITY_IS_CHANGED;
        }

        if (result != 0) p->u64 = sensorData->u64;
        return result;
      }
      add(sensorData);
      return type == 0 ? TEMPERATURE_IS_CHANGED | NEW_UID : type == 14 ? HUMIDITY_IS_CHANGED | NEW_UID : NEW_UID;
    }

    return 0;
  }

private:
  void add(SensorData* item) {
    int index = size;
    int new_size = size+1;
    if (new_size > capacity) {
      int new_capacity = capacity + 8;
      SensorData* new_data = (SensorData*)realloc(items, new_capacity*sizeof(SensorData));
      if (new_data ==__null) return; // FIXME throw exception?
      items = new_data;
      capacity = new_capacity;
    }
    size = new_size;
    items[index].u64 = item->u64;
  }

};


#endif /* SENSORSDATA_HPP_ */
