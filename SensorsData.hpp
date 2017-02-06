/*
 * Channels.hpp
 *
 *  Created on: Feb 5, 2017
 *      Author: Alex Konshin
 */

#ifndef SENSORSDATA_HPP_
#define SENSORSDATA_HPP_

#define SENSOR_UID_MASK  0xff700000L
#define SENSOR_DATA_MASK 0x008fffffL

class SensorsData {
private:
  int size;
  int capacity;
  uint32_t* items;

public:
  SensorsData() {
    items = (uint32_t*)calloc(8, sizeof(uint32_t));
    capacity = 8;
    size = 0;
  }
  SensorsData(int capacity) {
    if (capacity <= 0) capacity = 8;
    items = (uint32_t*)calloc(capacity, sizeof(uint32_t));
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

  uint32_t getItem(int index) {
    return index<0 || index>=size ? 0 : items[index];
  }

  uint32_t getData(int channel, int rolling_code = -1) {
    if (channel < 1 || channel > 8) return 0;
    if (rolling_code > 255 || (rolling_code < 0 && rolling_code != -1)) return 0;

    uint32_t mask;
    uint32_t uid = ((uint32_t)channel-1L) << 20;
    if (rolling_code == -1) {
      mask = 0x00700000L;
    } else {
      uid |= (((uint32_t)rolling_code) << 24);
      mask = SENSOR_UID_MASK;
    }
    for (int index = size-1; index>=0; index--) {
      uint32_t item = items[index];
      if ((item & mask) == uid) return item;
    }
    return 0;
  }

  bool update(uint32_t nF007TH) {
    uint32_t uid = nF007TH & SENSOR_UID_MASK;
    for (int index = 0; index<size; index++) {
      uint32_t item = items[index];
      if ((item & SENSOR_UID_MASK) == uid) {
        if (((item ^ nF007TH)& SENSOR_DATA_MASK) == 0) return false;
        items[index] = nF007TH;
        return true;
      }
    }
    add(nF007TH);
    return true;
  }

private:
  void add(uint32_t item) {
    int index = size;
    int new_size = size+1;
    if (new_size > capacity) {
      int new_capacity = capacity + 8;
      uint32_t* new_data = (uint32_t*)realloc(items, new_capacity*sizeof(uint32_t));
      if (new_data ==__null) return; // FIXME throw exception?
      items = new_data;
      capacity = new_capacity;
    }
    size = new_size;
    items[index] = item;
  }

};


#endif /* SENSORSDATA_HPP_ */
