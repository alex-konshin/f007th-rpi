/*
 * SensorsData.hpp
 *
 *  Created on: February 5, 2017
 *      Author: Alex Konshin
 */

#ifndef SENSORSDATA_HPP_
#define SENSORSDATA_HPP_

#define SENSOR_UID_MASK  0xff700000L
#define SENSOR_TEMPERATURE_MASK 0x000fff00L
#define SENSOR_HUMIDITY_MASK    0x000000ffL
#define SENSOR_BATTERY_MASK     0x00800000L
#define SENSOR_DATA_MASK        (SENSOR_TEMPERATURE_MASK|SENSOR_HUMIDITY_MASK|SENSOR_BATTERY_MASK)

#define JSON_SIZE_PER_ITEM_ALLDATA  256
#define JSON_SIZE_PER_ITEM  128

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <mutex>
#include "../utils/Logger.hpp"
#include "../utils/Utils.hpp"
#include "../protocols/Protocol.hpp"

#define TEMPERATURE_IS_CHANGED       METRIC_TEMPERATURE
#define HUMIDITY_IS_CHANGED          METRIC_HUMIDITY
#define BATTERY_STATUS_IS_CHANGED    METRIC_BATTERY_STATUS
#define DATA_IS_CHANGED              (TEMPERATURE_IS_CHANGED|HUMIDITY_IS_CHANGED)

#define NEW_UID                      8
#define TIME_NOT_CHANGED            16

#define HISTORY_DEPTH_HOURS         24

//-------------------------------------------------------------
// Compiled message format

enum class MessageInsertType{ Constant, ReferenceId, SensorName, TemperatureF, TemperatureC, TemperaturedF, TemperaturedC, Humidity, BatteryStatusInt, BatteryStatusStr, Percent };

typedef struct MessageInsert {
  MessageInsertType type;
  const char* stringArg;
private:
  void append(char*& output, uint32_t& remain, const char* str, uint32_t len);
  void append(char*& output, uint32_t& remain, const char* str);

  void append(char*& output, uint32_t& remain, struct SensorData* data, const char* referenceId);

public:
  uint32_t formatMessage(char* buffer, uint32_t buffer_size, const char* messageFormat, struct SensorData* data, const char* referenceId);

} MessageInsert;


//-------------------------------------------------------------
enum class BoundCheckResult : int { Lower=0, Inside=1, Higher=2, NotApplicable=-2, Locked=-1 };

enum class RestRequestType : int { AllData=0, TemperatureF=1, TemperatureF10=2, TemperatureC=3, TemperatureC10=4, Humidity=5, Battery=6, Brief=7 };

//-------------------------------------------------------------
#define NO_BOUND 0x00008000
typedef struct RuleBounds {
  union {
    uint32_t both;
    struct {
      int16_t low, high;
    } bound;
  };
  static struct RuleBounds make(int low, int high) {
    RuleBounds result;
    result.bound.low = (int16_t)low;
    result.bound.high = (int16_t)high;
    return result;
  }
  int32_t getHi() {
    return adjust(bound.high);
  }
  int32_t getLo() {
    return adjust(bound.low);
  }
private:
  static int32_t adjust(int16_t value) {
    if ( (0x0000ffff & (int32_t)value) == NO_BOUND) return NO_BOUND;
    return value;
  }
} RuleBounds;

typedef struct RuleBoundsSheduleItem {
  struct RuleBoundsSheduleItem* prev;
  struct RuleBoundsSheduleItem* next;
  RuleBounds bounds;
  uint32_t time_offset;
} RuleBoundsSheduleItem;

//-------------------------------------------------------------
class AbstractRuleBoundSchedule {
public:
  virtual ~AbstractRuleBoundSchedule() {}
  virtual BoundCheckResult checkBounds(int value, uint32_t day_time_offset, uint32_t week_time_offset) = 0;

  static BoundCheckResult checkBounds(RuleBounds bounds, int value) {
    int32_t lo = bounds.getLo();
    if (lo != NO_BOUND && value < lo) return BoundCheckResult::Lower;
    int32_t hi = bounds.getHi();
    return hi == NO_BOUND || value <= hi ? BoundCheckResult::Inside : BoundCheckResult::Higher;
  }
};

//-------------------------------------------------------------
class RuleBoundFixed : public AbstractRuleBoundSchedule {
protected:
  RuleBounds _bounds;
public:
  RuleBoundFixed(RuleBounds bounds) {
    _bounds = bounds;
  }
  RuleBoundFixed(int low, int high) {
    _bounds = RuleBounds::make(low, high);
  }

  ~RuleBoundFixed() {}

  RuleBounds getBounds() { return _bounds; }

  BoundCheckResult checkBounds(int value, uint32_t day_time_offset, uint32_t week_time_offset) {
    return AbstractRuleBoundSchedule::checkBounds(_bounds, value);
  }
};

//-------------------------------------------------------------
class RuleBoundSchedule : public AbstractRuleBoundSchedule {
protected:
  RuleBoundsSheduleItem* firstScheduleItem;
public:
  RuleBoundSchedule(RuleBoundsSheduleItem* schedule) {
    firstScheduleItem = schedule;
  }

  ~RuleBoundSchedule() {}

  BoundCheckResult checkBounds(int value, uint32_t day_time_offset, uint32_t week_time_offset) {
    uint32_t time_offset = day_time_offset;
    RuleBoundsSheduleItem* first = firstScheduleItem;
    RuleBoundsSheduleItem* current = first;
    if (current == NULL) return BoundCheckResult::NotApplicable; // it should not happen
    if (current->time_offset > time_offset)  {
      RuleBoundsSheduleItem* prev = current;
      while ((prev = prev->prev) != first && prev->time_offset > time_offset) current = prev;
    } else {
      RuleBoundsSheduleItem* next = current;
      while ((next = next->next) != first && next->time_offset <= time_offset) current = next;
    }
    return AbstractRuleBoundSchedule::checkBounds(current->bounds, value);
  }

};

//-------------------------------------------------------------
enum class Metric : int { TemperatureF, TemperatureC, Humidity, BatteryStatus };

// Reference to another rule that will be locked/unlocked when the current rule is applied.
typedef struct RuleLock {
  struct RuleLock* next;
  class AbstractRuleWithSchedule* rule;
  bool lock; // lock vs unlock
} RuleLock;

//-------------------------------------------------------------
class AbstractRuleWithSchedule {
private:
  // the list of rules to be locked/unlocked when this rule is applied
  RuleLock* locks[3] = {NULL,NULL,NULL};

  void freeLocks(RuleLock* locks) {
    while (locks != NULL) {
      RuleLock* lock = locks;
      locks = lock ->next;
      free(lock);
    }
  }

public:
  AbstractRuleWithSchedule* next = NULL;
  struct SensorDef* sensor_def;
  const char* id = NULL;
  const char* messageFormat[3] = {NULL, NULL, NULL};
  struct MessageInsert* compiledMessageFormat[3] = {NULL, NULL, NULL};
  Metric metric;
  AbstractRuleBoundSchedule* boundSchedule = NULL;
  bool isLocked = false;
  uint8_t selfLocks = 0;

  AbstractRuleWithSchedule(SensorDef* sensor_def, Metric metric) {
    this->sensor_def = sensor_def;
    this->metric = metric;
  }
  virtual ~AbstractRuleWithSchedule() {
    if (locks[0] != NULL) freeLocks(locks[0]);
    if (locks[1] != NULL) freeLocks(locks[1]);
    if (locks[2] != NULL) freeLocks(locks[2]);
    // TODO free messages, topic, bounds ?
  }

  void setBound(AbstractRuleBoundSchedule* boundSchedule) {
    this->boundSchedule = boundSchedule;
  }
  void setBound(int low, int high) {
    this->boundSchedule = new RuleBoundFixed(RuleBounds::make(low, high));
  }

  void setLocks(RuleLock* list, BoundCheckResult bound) {
    int index = (int)bound;
    if (index >= 0 && index < 3) this->locks[index] = list;
  }

  void linkTo(AbstractRuleWithSchedule*& rules) {
    next = NULL;
    AbstractRuleWithSchedule** link = &rules;
    AbstractRuleWithSchedule* rule;
    while ((rule = *link) != NULL) link = &rule->next;
    *link = this;
  }

  void setLock(bool lock) {
    isLocked = lock;
  }

  void setMessage(BoundCheckResult boundCheckResult, const char* mqttMessageFormat, struct MessageInsert* compiledMessageFormat) {
    int index = (int)boundCheckResult;
    if (index >= 0 && index < 3) {
      this->messageFormat[index] = mqttMessageFormat;
      this->compiledMessageFormat[index] = compiledMessageFormat;
    }
  }

  uint32_t formatMessage(char* buffer, uint32_t buffer_size, BoundCheckResult boundCheckResult, struct SensorData* data) {
    int index = (int)boundCheckResult;
    if (index < 0 || index >= 3) return 0;
    MessageInsert* compiledMessageFormat = this->compiledMessageFormat[index];
    if (compiledMessageFormat == NULL) return 0;
    const char* messageFormat = this->messageFormat[index];
    return compiledMessageFormat->formatMessage(buffer, buffer_size, messageFormat, data, id);
  }

  void applyLocks(BoundCheckResult boundCheckResult) {
    int index = (int)boundCheckResult;
    if (index >= 0 && index < 3) {
      selfLocks = 1<<index;
      RuleLock* lock = locks[index];
      while (lock != NULL) {
        lock->rule->isLocked = lock->lock;
        lock = lock->next;
      }
    }
  }

  bool isSelfLocked(BoundCheckResult boundCheckResult) {
    int index = (int)boundCheckResult;
    return index >= 0 && index < 3 && (selfLocks & (1<<index)) != 0;
  }

  virtual const char* getTypeName() = 0;

  virtual void execute(const char* message, class Config& cfg) = 0;
};


//-------------------------------------------------------------
// Action rule
class ActionRule : public AbstractRuleWithSchedule {
public:

  ActionRule(SensorDef* sensor_def, Metric metric) : AbstractRuleWithSchedule(sensor_def, metric) {
  }

  const char* getTypeName() {
    return "Action rule";
  }

  void execute(const char* message, class Config& cfg);

};


//-------------------------------------------------------------
typedef struct SensorDef {
private:
  static SensorDef* sensorDefs;

  SensorDef* next;

  AbstractRuleWithSchedule* rules;
  size_t name_len;

public:
  uint64_t id;
  unsigned index;
  const char* name;
  const char* quoted;
  const char* influxdb_quoted;
  struct SensorDataStored* data;

  static SensorDef* find(uint64_t id) {
    for (SensorDef* p = sensorDefs; p != NULL; p = p->next) {
      if (p->id == id) return p;
    }
    return NULL;
  }

  static SensorDef* find(const char* name) {
    if (name != NULL) {
      size_t len = strlen(name);
      for (SensorDef* p = sensorDefs; p != NULL; p = p->next) {
        if (p->name_len == len && strcmp(p->name, name) == 0) return p;
      }
    }
    return NULL;
  }

  static SensorDef* find(const char* name, size_t name_len) {
    if (name != NULL) {
      for (SensorDef* p = sensorDefs; p != NULL; p = p->next) {
        if (p->name_len == name_len && strncmp(p->name, name, name_len) == 0) return p;
      }
    }
    return NULL;
  }

#define SENSOR_NAME_MAX_LEN 64

#define SENSOR_DEF_WAS_ADDED 0
#define SENSOR_DEF_DUP       1
#define SENSOR_DEF_DUP_NAME  2
#define SENSOR_NAME_MISSING  3
#define SENSOR_NAME_TOO_LONG 4
#define SENSOR_NAME_INVALID  5

  static int add(uint64_t id, const char* name, size_t name_len, SensorDef*& result) {
    result = NULL;
    if (name_len <= 0 || name == NULL) return SENSOR_NAME_MISSING;
    if (name_len > SENSOR_NAME_MAX_LEN) return SENSOR_NAME_TOO_LONG;

    unsigned new_index = 0;
    SensorDef** pdef = &sensorDefs;
    SensorDef* def = sensorDefs;
    while (def != NULL) {
      if (def->id == id) {
        result = def;
        return SENSOR_DEF_DUP;
      }
      if (def->name != NULL && strcmp(def->name, name)==0 ) {
        result = def;
        return SENSOR_DEF_DUP_NAME;
      }
      new_index++;
      pdef = &(def->next);
      def = def->next;
    }

    char quoted[SENSOR_NAME_MAX_LEN*2+3];
    char influxdb_quoted[SENSOR_NAME_MAX_LEN*2+1];
    char* pq = quoted;
    char* pi = influxdb_quoted;
    *pq++ = '"';
    for (size_t i = 0; i<name_len; i++) {
      const char ch = name[i];

      if (ch=='"' || ch=='\\' || ch=='=' || ch==' ' || ch==',') {
        *pi++ = '\\';
      }
      *pi++ = ch;

      switch (ch) {
      case '\n': case '\r': case '\0':
        return SENSOR_NAME_INVALID;
      case '"': case '\\':
        *pq++ = '\\';
        *pq++ = ch;
        break;
      default:
        *pq++ = ch;
      }
    }
    *pi++ = '\0';
    *pq++ = '"';
    *pq++ = '\0';

    size_t quoted_len = pq-quoted;
    size_t influxdb_quoted_len = pi-influxdb_quoted;

    char* sensor_name = (char*)malloc((name_len+1+quoted_len+influxdb_quoted_len)*sizeof(char));
    memcpy(sensor_name, name, name_len*sizeof(char));
    sensor_name[name_len] = '\0';
    pq = sensor_name+name_len+1;
    memcpy(pq, quoted, quoted_len*sizeof(char));
    pi = pq+quoted_len;
    memcpy(pi, influxdb_quoted, influxdb_quoted_len*sizeof(char));

    def = (SensorDef*)malloc(sizeof(SensorDef));
    def->id = id;
    def->name = sensor_name;
    def->name_len = name_len;
    def->quoted = pq;
    def->influxdb_quoted = pi;
    def->next = NULL;
    def->rules = NULL;
    def->data = NULL;
    def->index = new_index;
    *pdef = def;
    result = def;
    return SENSOR_DEF_WAS_ADDED;
  }

  void addRule(AbstractRuleWithSchedule* rule) {
    if (rule != NULL) rule->linkTo(rules);
  }
  AbstractRuleWithSchedule* getRules() {
    return rules;
  }

} SensorDef;

//-------------------------------------------------------------
enum class ValueConversion : int { None=0, F2C=1, C2F=2 };


typedef struct HistoryData {
  time_t time;
  int32_t value;

  size_t generateJson(int start, void*& buffer, size_t& buffer_size, ValueConversion convertion, bool x10, bool time_UTC) {

    char dt[TIME2STR_BUFFER_SIZE];
    convert_time(&time, dt, TIME2STR_BUFFER_SIZE, time_UTC);

    // {"t":"2020-12-31 00:00:00+05:00","y":-12345678900}
    size_t required_buffer_size = (size_t)start+(strlen(dt)+12+14)*sizeof(unsigned char);
    char* ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size)+start;
    size_t remain = buffer_size-start;

    int32_t converted_value = value;
    switch (convertion) {
    case ValueConversion::None:
      break;
    case ValueConversion::F2C:
      converted_value = ((value-320)*5)/9;
      break;
    case ValueConversion::C2F:
      converted_value = (value*9)/5+320;
      break;
    }
    int len;
    if (x10) {
      len = snprintf(ptr, remain, "{\"t\":\"%s\",\"y\":%d}", dt, converted_value );
    } else {
      char t2d_buffer[T2D_BUFFER_SIZE];
      uint32_t dummy = 0;
      len = snprintf(ptr, remain, "{\"t\":\"%s\",\"y\":%s}", dt, t2d(converted_value, t2d_buffer, dummy));
    }

    return (size_t)len;
  }

} HistoryData;


typedef struct HistoryListItem {
  HistoryListItem* next;
  HistoryData data;
} HistoryListItem;


typedef struct History {
private:
  HistoryListItem* first;
  HistoryListItem* last;
  std::mutex chain_mutex;
  RestRequestType datatype;
  unsigned count;

public:
  HistoryListItem* add(time_t time, int32_t value) {
    HistoryListItem* item = new HistoryListItem();
    item->data.time = time;
    item->data.value = value;
    item->next = NULL;
    chain_mutex.lock();
    if (last != NULL) last->next = item;
    last = item;
    if  (first == NULL) first = item;
    count++;
    chain_mutex.unlock();
    return item;
  }

  bool isEmpty() {
    return first == NULL;
  }

  unsigned getCount() { return count; }

  void truncate() {
    struct tm tm;
    time_t now = time(NULL);
    localtime_r(&now, &tm);
    tm.tm_hour -= HISTORY_DEPTH_HOURS;
    time_t from_time = mktime(&tm);
    truncate(from_time);
  }

  void truncate(time_t from) {
    chain_mutex.lock();
    HistoryListItem* record = first;
    if (record == NULL) {
      last = NULL;
    } else {
      if (from != 0) {
        while (record != NULL && record->data.time < from) {
          HistoryListItem* next = record->next;
          delete record;
          first = record = next;
          if ( record == NULL) last = NULL;
          count--;
        }
      }
    }
    chain_mutex.unlock();
  }

  // Copy data for the requested time range.
  // Return the size of result array in argument count.
  HistoryData* get(time_t from, time_t to, unsigned& count) {
    HistoryData* result = NULL;
    count = 0;

    time_t truncate_time = 0;
    if (from == 0) {
      struct tm tm;
      time_t now = time(NULL);
      localtime_r(&now, &tm);
      tm.tm_hour -= HISTORY_DEPTH_HOURS;
      truncate_time = mktime(&tm);
    }

    chain_mutex.lock();

    HistoryListItem* record = first;
    if (record != NULL) {
      if (truncate_time != 0) {
        // remove outdated history
        while (record != NULL && record->data.time < truncate_time) {
          HistoryListItem* next = record->next;
          delete record;
          first = record = next;
          if ( record == NULL) last = NULL;
          count--;
        }
      }
      if (record != NULL && from != 0) {
        while (record != NULL && record->data.time >= from) record = record->next;
      }

      if (record != NULL) {
        HistoryListItem* r = record;
        int result_size = 0;
        do {
          if (to != 0 && r->data.time > to) break;
          result_size++;
        } while ((r = r->next) != NULL);
        if (result_size > 0) {
          count = result_size;
          HistoryData* pdata = result = (HistoryData*)calloc(result_size, sizeof(HistoryData));
          r = record;
          for (int index = 0; index<result_size; index++) {
            pdata->time = r->data.time;
            pdata->value = r->data.value;
            pdata++;
            r = r->next;
          }
        }
      }
    }
    chain_mutex.unlock();

    return result;
  }

  size_t generateJson(time_t from, time_t to, void*& buffer, size_t& buffer_size, ValueConversion convertion, bool x10, bool time_UTC) {
    unsigned count = 0;

    // Take all requested items safely first then generate JSON
    HistoryData* pdata = get(from, to, count);
    if (pdata == NULL || count == 0) return 0;

// {"t":"2020-12-31 00:00:00+05:00","y":-12345678900}
#define JSON_HISTORY_RECORD_SIZE  54
    // [<record>,<record>]
    size_t required_buffer_size = (count*(JSON_HISTORY_RECORD_SIZE+1)-1+3)*sizeof(unsigned char);
    char* pchars = (char*)resize_buffer(required_buffer_size, buffer, buffer_size);
    size_t len = 1;
    size_t total_len = 1;
    *pchars = '[';
    pchars[1]  = '\0';
    HistoryData* pd = pdata;
    for (unsigned index = 0; index<count; index++) {
      if (total_len > 1) {
        pchars[total_len] = ',';
        pchars[++total_len] = '\0';
      }
      len = pd->generateJson(total_len, buffer, buffer_size, convertion, x10, time_UTC);
      if (len > 0) {
        total_len += len;
      } else if (total_len > 1) { // remove last comma
        pchars[--total_len] = '\0';
      }
      pd++;
    }

    pchars[total_len++] = ']';
    pchars[total_len] = '\0';

    free((void*)pdata);
    return total_len;
  }

} History;


//-------------------------------------------------------------
typedef struct SensorData {
  SensorDef* def;
  Protocol* protocol;
  time_t data_time;

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

    } fields;
  };

public:

  // Copies data from received and decoded message to a new object that
  // will be added to collection SensorsData
  void copyFrom(SensorData* data) {
    Protocol* protocol = data->protocol;
    this->protocol = protocol;
    protocol->copyFields(this, data);
    data_time = data->data_time;
    if (def == NULL) def = data->def;
  }

  uint32_t getId() { return protocol == NULL ? 0 : protocol->getId(this); }

  uint32_t getFeatures() { return protocol == NULL ? -1 : protocol->getFeatures(this); }
  int getChannel() { return protocol == NULL ? -1 : protocol->getChannel(this); }
  int getChannelNumber() { return protocol == NULL ? -1 : protocol->getChannelNumber(this); }
  const char* getChannelName() { return protocol == NULL ? NULL : protocol->getChannelName(this); }
  bool hasBatteryStatus() { return protocol != NULL && protocol->hasBatteryStatus(); }
  // true => OK
  bool getBatteryStatus() { return protocol != NULL && protocol->getBatteryStatus(this); }

  bool hasTemperature() { return protocol != NULL && protocol->hasTemperature(this); }
  bool isRawTemperatureCelsius() { return protocol != NULL && protocol->isRawTemperatureCelsius(); }
  int getRawTemperature() { return protocol == NULL ? -99999 : protocol->getRawTemperature(this); };

  int getTemperature10(bool celsius) { return celsius ? getTemperatureCx10() : getTemperatureFx10(); }
  int getTemperatureCx10() { return protocol == NULL ? -2732 : protocol->getTemperatureCx10(this); }
  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10() { return protocol == NULL ? -4597 : protocol->getTemperatureFx10(this); }

  bool hasHumidity() { return protocol != NULL && protocol->hasHumidity(this); }
  // Relative Humidity, 0..100%
  int getHumidity() { return protocol == NULL ? 0 : protocol->getHumidity(this); }

  const char* getSensorTypeName() { return protocol == NULL ? "UNKNOWN" : protocol->getSensorTypeName(this); }
  const char* getSensorTypeLongName() { return protocol == NULL ? "UNKNOWN" : protocol->getSensorTypeLongName(this); }

  // random number that is changed when battery is changed
  uint16_t getRollingCode() { return protocol == NULL ? -1 : protocol->getRollingCode(this); }

  size_t generateJsonContent(int start, void*& buffer, size_t& buffer_size, int options);
  size_t generateJson(int start, void*& buffer, size_t& buffer_size, int options);
  size_t generateInfluxData(int start, void*& buffer, size_t& buffer_size, int changed, int options);

  void print(FILE* file, int options);

  void printRawData(FILE* file) {
    if (protocol != NULL) protocol->printRawData(this, file);
  }

  BoundCheckResult checkRule(AbstractRuleWithSchedule* rule, int changed_fields) {
    if (rule == NULL || changed_fields == 0) return BoundCheckResult::NotApplicable;
    if (rule->isLocked) return BoundCheckResult::Locked;

    struct tm ltm;
    time_t tm = time(NULL);
    localtime_r(&tm, &ltm);
    uint32_t day_time_offset = ltm.tm_hour*60 + ltm.tm_min;
    uint32_t week_time_offset = ltm.tm_wday*24*60 + day_time_offset;

    BoundCheckResult result = BoundCheckResult::NotApplicable;

    switch(rule->metric) {
    case Metric::TemperatureC:
    case Metric::TemperatureF:
      if ((changed_fields&TEMPERATURE_IS_CHANGED) != 0) {
        if (rule->boundSchedule == NULL) return BoundCheckResult::Inside;
        int n = rule->metric==Metric::TemperatureC ? getTemperatureCx10() : getTemperatureFx10();
        result = rule->boundSchedule->checkBounds(n, day_time_offset, week_time_offset);
      }
      break;

    case Metric::Humidity:
      if (!hasHumidity()) return BoundCheckResult::NotApplicable;
      if ((changed_fields&HUMIDITY_IS_CHANGED) != 0) {
        if (rule->boundSchedule == NULL) return BoundCheckResult::Inside;
        result = rule->boundSchedule->checkBounds(getHumidity(), day_time_offset, week_time_offset);
      }
      break;

    case Metric::BatteryStatus:
      if (!hasBatteryStatus()) return BoundCheckResult::NotApplicable;
      if ((changed_fields&BATTERY_STATUS_IS_CHANGED) != 0) {
        if (rule->boundSchedule == NULL) return BoundCheckResult::Inside;
        result = rule->boundSchedule->checkBounds((int)getBatteryStatus(), day_time_offset, week_time_offset);
      }
      break;
    }

    return result != BoundCheckResult::NotApplicable && rule->isSelfLocked(result) ? BoundCheckResult::Locked : result;
  }

} SensorData;

typedef struct SensorDataStored : SensorData  {
#ifdef INCLUDE_HTTPD
  History temperatureHistory;
  History humidityHistory;
#endif

  size_t generateJsonEx(int start, void*& buffer, size_t& buffer_size, int options); // includes history counts
  size_t generateJsonLine(int start, void*& buffer, size_t& buffer_size, RestRequestType requestType, int options);
  size_t generateJsonLineBrief(int start, void*& buffer, size_t& buffer_size, time_t current_time, int options);

} SensorDataStored;

//-------------------------------------------------------------

class SensorsData {
private:
  std::mutex items_mutex;
  SensorDataStored** items;
  int size;
#define SENSORS_DATA_INITIAL_CAPACITY 32
  int capacity;
  int options;

  SensorDataStored* add(SensorData* data, time_t& data_time) {
    SensorDataStored* new_item = (SensorDataStored*)calloc(1, sizeof(SensorDataStored));
    if (new_item == NULL) {
      Log->error("Out of memory");
      return NULL;
    }
    new_item->copyFrom(data);

    SensorDef* def = new_item->def;
    if (def == NULL) {
      def = SensorDef::find(data->getId());
      if (def != NULL) new_item->def = def;
    }

    items_mutex.lock();

    int new_index = size;
    int new_size = size+1;
    if (new_size > capacity) {
      int new_capacity = capacity + 8;
      SensorDataStored** new_items = (SensorDataStored**)realloc(items, new_capacity*sizeof(SensorDataStored*));
      if (new_items != NULL) {
        items_mutex.unlock();
        Log->error("Out of memory");
        return NULL;
      }
      items = new_items;
      capacity = new_capacity;
    }

    if (def != NULL && new_index > 0) {
      // Insert new item accordingly to the order of sensor definitions in configuration file.
      // Then any REST requests will return data with respect of the order of definitions in configuration file.

      unsigned def_index = def->index;
      // Bubble insert from the end of the array.
      do {
        SensorDataStored* item = items[new_index-1];
        SensorDef* item_def = item->def;
        if (item_def != NULL && item_def->index <= def_index) break;
        items[new_index] = item;
      } while (--new_index > 0);
    }

    items[new_index] = new_item;
    size = new_size;

    items_mutex.unlock();
#ifdef INCLUDE_HTTPD
    if (new_item->def != NULL) {
      if (new_item->hasTemperature()) new_item->temperatureHistory.add(data_time, new_item->getRawTemperature());
      if (new_item->hasHumidity()) new_item->humidityHistory.add(data_time, new_item->getHumidity());
    }
#endif
    return new_item;
  }

  SensorDataStored** getSnapshot(int& count) {
    SensorDataStored** result = NULL;
    count = 0;
    items_mutex.lock();
    if (size > 0) {
      size_t copy_size = size*sizeof(SensorDataStored*);
      result = (SensorDataStored**)malloc(copy_size);
      if (result != NULL) {
        memcpy(result, items, copy_size);
        count = size;
      }
    }
    items_mutex.unlock();
    return result;
  }

  size_t generateJsonAllData(SensorDataStored** items, int nItems, void*& buffer, size_t& buffer_size, int options);

public:
  SensorsData(int options) {
    items = (SensorDataStored**)calloc(SENSORS_DATA_INITIAL_CAPACITY, sizeof(SensorDataStored*));
    capacity = SENSORS_DATA_INITIAL_CAPACITY;
    size = 0;
    this->options = options;
  }
  SensorsData(int capacity, int options) {
    if (capacity <= 0) capacity = SENSORS_DATA_INITIAL_CAPACITY;
    items = (SensorDataStored**)calloc(capacity, sizeof(SensorDataStored*));
    this->capacity = capacity;
    size = 0;
    this->options = options;
  }

  ~SensorsData() {
    items_mutex.lock();
    if ( items != NULL) {
      for (int index = 0; index<size; index++) {
        SensorDataStored* item = items[index];
        if (item != NULL) {
          free(item);
          items[index] = NULL;
        }
      }
      free(items);
      items = NULL;
    }
    size = 0;
    capacity = 0;
    items_mutex.unlock();
  }

  inline int getSize() { return size; }

  int getOptions() { return options; }
/*
  SensorDataStored* getData(Protocol* protocol, int channel, uint8_t rolling_code = -1) {
    if (protocol == NULL) return NULL;
    SensorDataStored* result = NULL;
    items_mutex.lock();
    for (int index = 0; index<size; index++) {
      SensorDataStored* p = items[index];
      if (p != NULL && p->protocol == protocol && protocol->sameId(p, channel, rolling_code)) {
        result = p;
        break;
      }
    }
    items_mutex.unlock();
    return result;
  }
*/
  SensorDataStored* find(SensorData* sensorData) {
    SensorDataStored* result = NULL;
    Protocol* protocol = sensorData->protocol;
    if (protocol != NULL ) {
      items_mutex.lock();
      for (int index = 0; index<size; index++) {
        SensorDataStored* p = items[index];
        if (p != NULL && protocol == p->protocol && protocol->equals(sensorData, p)) {
          result = p;
          break;
        }
      }
      items_mutex.unlock();
    }
    return result;
  }

  // Find sensor data by the name defined in the config
  SensorDataStored* find(const char* name) {
    if (name == NULL || *name == '\0') return NULL;
    SensorDef* def = SensorDef::find(name);
    return def == NULL ? NULL : find(def);
  }

  SensorDataStored* find(const char* name, size_t name_len) {
    if (name == NULL || name_len == 0 || *name == '\0') return NULL;
    SensorDef* def = SensorDef::find(name, name_len);
    return def == NULL ? NULL : find(def);
  }

  SensorDataStored* find(SensorDef* def) {
    if (def == NULL) return NULL;
    SensorDataStored* result = def->data;
    if (result == NULL) {
      items_mutex.lock();
      for (int index = 0; index<size; index++) {
        SensorDataStored* sensorData = items[index];
        if (sensorData != NULL && sensorData->def == def) {
          result = sensorData;
          def->data = sensorData;
          break;
        }
      }
      items_mutex.unlock();
    }
    return result;
  }

  int update(SensorData* sensorData, time_t data_time, time_t max_unchanged_gap) {
    Protocol* protocol = sensorData->protocol;
    if (protocol == NULL) return 0;

    int changed;
    SensorDataStored* item = find(sensorData);
    if (item != NULL) {
      changed = protocol->update(sensorData, item, data_time, max_unchanged_gap);
    } else {
      // A new (unknown) sensor
      item = add(sensorData, data_time);
      changed = protocol->getMetrics(sensorData) | NEW_UID;
    }
#ifdef INCLUDE_HTTPD
    if (item->def != NULL && (changed&DATA_IS_CHANGED) != 0) {
      struct tm tm;
      time_t now = time(NULL);
      localtime_r(&now, &tm);
      tm.tm_hour -= HISTORY_DEPTH_HOURS;
      time_t from_time = mktime(&tm);
      if ((changed&TEMPERATURE_IS_CHANGED) != 0) {
        item->temperatureHistory.truncate(from_time);
        item->temperatureHistory.add(data_time, item->getRawTemperature());
      }
      if ((changed&HUMIDITY_IS_CHANGED) != 0) {
        item->humidityHistory.truncate(from_time);
        item->humidityHistory.add(data_time, item->getHumidity());
      }
    }
#endif
    return changed;
  }

  size_t generateJsonAllData(void*& buffer, size_t& buffer_size) {
    int nItems = 0;
    SensorDataStored** snapshot = getSnapshot(nItems);
    if (snapshot == NULL || nItems <= 0) return 0;

    size_t result = generateJsonAllData(snapshot, nItems, buffer, buffer_size, options);

    free(snapshot);
    return result;
  }

  size_t generateJson(void*& buffer, size_t& buffer_size, RestRequestType requestType, int options);

};

#endif /* SENSORSDATA_HPP_ */
