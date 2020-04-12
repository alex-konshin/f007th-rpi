/*
 * SensorsData.hpp
 *
 *  Created on: February 5, 2017
 *      Author: Alex Konshin
 */

#ifndef SENSORSDATA_HPP_
#define SENSORSDATA_HPP_

#define PROTOCOL_F007TH    1
#define PROTOCOL_00592TXR  2
#define PROTOCOL_TX7U      4
#define PROTOCOL_HG02832   8
#define PROTOCOL_WH2      16
#define PROTOCOL_ALL      (unsigned)(-1)

#define PROTOCOL_INDEX_F007TH    0
#define PROTOCOL_INDEX_00592TXR  1
#define PROTOCOL_INDEX_TX7U      2
#define PROTOCOL_INDEX_HG02832   3
#define PROTOCOL_INDEX_WH2       4
#define NUMBER_OF_PROTOCOLS      5

#define VERBOSITY_DEBUG            1
#define VERBOSITY_INFO             2
#define VERBOSITY_PRINT_DETAILS    4
#define VERBOSITY_PRINT_STATISTICS 8
#define VERBOSITY_PRINT_UNDECODED 16
#define VERBOSITY_PRINT_JSON      32
#define VERBOSITY_PRINT_CURL      64

#define OPTION_CELSIUS           256
#define OPTION_UTC               512

#define SENSOR_UID_MASK  0xff700000L
#define SENSOR_TEMPERATURE_MASK 0x000fff00L
#define SENSOR_HUMIDITY_MASK    0x000000ffL
#define SENSOR_BATTERY_MASK     0x00800000L
#define SENSOR_DATA_MASK        (SENSOR_TEMPERATURE_MASK|SENSOR_HUMIDITY_MASK|SENSOR_BATTERY_MASK)

#define TEMPERATURE_IS_CHANGED       1
#define HUMIDITY_IS_CHANGED          2
#define BATTERY_STATUS_IS_CHANGED    4
#define NEW_UID                      8
#define TIME_NOT_CHANGED            16

#define JSON_SIZE_PER_ITEM  200

#include <time.h>
#include <stdio.h>
#include "Logger.hpp"

//-------------------------------------------------------------
#ifdef INCLUDE_MQTT

//-------------------------------------------------------------
// Compiled message format

enum class MessageInsertType{ Constant, ReferenceId, SensorName, TemperatureF, TemperatureC };

typedef struct MessageInsert {
  MessageInsertType type;
  const char* stringArg;
private:
  char* t2d(int t, char* buffer, uint32_t& length);
  void append(char*& output, uint32_t& remain, const char* str, uint32_t len);
  void append(char*& output, uint32_t& remain, const char* str);

  void append(char*& output, uint32_t& remain, struct SensorData* data, const char* referenceId);

public:
  uint32_t formatMessage(char* buffer, uint32_t buffer_size, const char* messageFormat, struct SensorData* data, const char* referenceId);

} MessageInsert;


//-------------------------------------------------------------
enum class BoundCheckResult : int { Lower=0, Inside=1, Higher=2, NotApplicable=-2, Locked=-1 };

//-------------------------------------------------------------
#define NO_BOUND 0x00008000
typedef struct MqttRuleBounds {
  union {
    uint32_t both;
    struct {
      int16_t low, high;
    } bound;
  };
  static struct MqttRuleBounds make(int low, int high) {
    MqttRuleBounds result;
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
} MqttRuleBounds;

typedef struct MqttRuleBoundsSheduleItem {
  struct MqttRuleBoundsSheduleItem* prev;
  struct MqttRuleBoundsSheduleItem* next;
  MqttRuleBounds bounds;
  uint32_t time_offset;
} MqttRuleBoundsSheduleItem;

//-------------------------------------------------------------
class AbstractMqttRuleBoundSchedule {
public:
  virtual ~AbstractMqttRuleBoundSchedule() {}
  virtual BoundCheckResult checkBounds(int value, uint32_t day_time_offset, uint32_t week_time_offset) = 0;

  static BoundCheckResult checkBounds(MqttRuleBounds bounds, int value) {
    int32_t lo = bounds.getLo();
    if (lo != NO_BOUND && value < lo) return BoundCheckResult::Lower;
    int32_t hi = bounds.getHi();
    return hi == NO_BOUND || value <= hi ? BoundCheckResult::Inside : BoundCheckResult::Higher;
  }
};

//-------------------------------------------------------------
class MqttRuleBoundFixed : public AbstractMqttRuleBoundSchedule {
protected:
  MqttRuleBounds _bounds;
public:
  MqttRuleBoundFixed(MqttRuleBounds bounds) {
    _bounds = bounds;
  }
  MqttRuleBoundFixed(int low, int high) {
    _bounds = MqttRuleBounds::make(low, high);
  }

  ~MqttRuleBoundFixed() {}

  MqttRuleBounds getBounds() { return _bounds; }

  BoundCheckResult checkBounds(int value, uint32_t day_time_offset, uint32_t week_time_offset) {
    return AbstractMqttRuleBoundSchedule::checkBounds(_bounds, value);
  }
};

//-------------------------------------------------------------
class MqttRuleBoundSchedule : public AbstractMqttRuleBoundSchedule {
protected:
  MqttRuleBoundsSheduleItem* firstScheduleItem;
public:
  MqttRuleBoundSchedule(MqttRuleBoundsSheduleItem* schedule) {
    firstScheduleItem = schedule;
  }

  ~MqttRuleBoundSchedule() {}

  BoundCheckResult checkBounds(int value, uint32_t day_time_offset, uint32_t week_time_offset) {
    uint32_t time_offset = day_time_offset;
    MqttRuleBoundsSheduleItem* first = firstScheduleItem;
    MqttRuleBoundsSheduleItem* current = first;
    if (current == NULL) return BoundCheckResult::NotApplicable; // it should not happen
    if (current->time_offset > time_offset)  {
      MqttRuleBoundsSheduleItem* prev = current;
      while ((prev = prev->prev) != first && prev->time_offset > time_offset) current = prev;
    } else {
      MqttRuleBoundsSheduleItem* next = current;
      while ((next = next->next) != first && next->time_offset <= time_offset) current = next;
    }
    return AbstractMqttRuleBoundSchedule::checkBounds(current->bounds, value);
  }

};


//-------------------------------------------------------------
enum class Metric : int { TemperatureF, TemperatureC, Humidity, BatteryStatus };
enum class BoundType { None, Low, High };

// Reference to another MQTT rule that will be locked/unlocked when the current rule is applied.
typedef struct MqttRuleLock {
  struct MqttRuleLock* next;
  class MqttRule* rule;
  bool lock; // lock vs unlock
} MqttRuleLock;


//-------------------------------------------------------------
// MQTT rule definition

class MqttRule {
private:
  // the list of rules to be locked/unlocked when this rule is applied
  MqttRuleLock* locks[3] = {NULL,NULL,NULL};

  void freeLocks(MqttRuleLock* locks) {
    while (locks != NULL) {
      MqttRuleLock* lock = locks;
      locks = lock ->next;
      free(lock);
    }
  }

public:
  MqttRule* next = NULL;
  struct SensorDef* sensor_def;
  const char* id = NULL;
  const char* mqttTopic;
  const char* mqttMessageFormat[3] = {NULL, NULL, NULL};
  struct MessageInsert* compiledMessageFormat[3] = {NULL, NULL, NULL};
  Metric metric;
  AbstractMqttRuleBoundSchedule* boundSchedule = NULL;
  bool isLocked = false;
  uint8_t selfLocks;

  MqttRule(SensorDef* sensor_def, Metric metric, const char* mqttTopic) {
    this->sensor_def = sensor_def;
    this->metric = metric;
    this->mqttTopic = mqttTopic;
    selfLocks = 0;
  }
  ~MqttRule() {
    if (locks[0] != NULL) freeLocks(locks[0]);
    if (locks[1] != NULL) freeLocks(locks[1]);
    if (locks[2] != NULL) freeLocks(locks[2]);
    // TODO free messages, topic, bounds ?
  }

  void setBound(AbstractMqttRuleBoundSchedule* boundSchedule) {
    this->boundSchedule = boundSchedule;
  }
  void setBound(int low, int high) {
    this->boundSchedule = new MqttRuleBoundFixed(MqttRuleBounds::make(low, high));
  }

  void setLocks(MqttRuleLock* list, BoundCheckResult bound) {
    int index = (int)bound;
    if (index >= 0 && index < 3) this->locks[index] = list;
  }

  void linkTo(MqttRule*& rules) {
    next = NULL;
    MqttRule** link = &rules;
    MqttRule* rule;
    while ((rule = *link) != NULL) link = &rule->next;
    *link = this;
  }

  void setLock(bool lock) {
    isLocked = lock;
  }

  void setMessage(BoundCheckResult boundCheckResult, const char* mqttMessageFormat, struct MessageInsert* compiledMessageFormat) {
    int index = (int)boundCheckResult;
    if (index >= 0 && index < 3) {
      this->mqttMessageFormat[index] = mqttMessageFormat;
      this->compiledMessageFormat[index] = compiledMessageFormat;
    }
  }

  uint32_t formatMessage(char* buffer, uint32_t buffer_size, BoundCheckResult boundCheckResult, struct SensorData* data) {
    int index = (int)boundCheckResult;
    if (index < 0 || index >= 3) return 0;
    MessageInsert* compiledMessageFormat = this->compiledMessageFormat[index];
    if (compiledMessageFormat == NULL) return 0;
    const char* messageFormat = this->mqttMessageFormat[index];
    return compiledMessageFormat->formatMessage(buffer, buffer_size, messageFormat, data, id);
  }

  void applyLocks(BoundCheckResult boundCheckResult) {
    int index = (int)boundCheckResult;
    if (index >= 0 && index < 3) {
      selfLocks = 1<<index;
      MqttRuleLock* lock = locks[index];
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

};
#endif

//-------------------------------------------------------------

typedef struct SensorDef {
private:
  static SensorDef* sensorDefs;

  SensorDef* next;

#ifdef INCLUDE_MQTT
  MqttRule* mqtt_rules;
#endif

public:
  uint32_t id;
  const char* name;
  const char* quoted;
  const char* influxdb_quoted;

  static SensorDef* find(uint32_t id) {
    for (SensorDef* p = sensorDefs; p != NULL; p = p->next) {
      if (p->id == id) return p;
    }
    return NULL;
  }

  static SensorDef* find(const char* name) {
    if (name != NULL) {
      for (SensorDef* p = sensorDefs; p != NULL; p = p->next) {
        if (strcmp(p->name, name) == 0) return p;
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

  static int add(uint32_t id, const char* name, size_t name_len, SensorDef*& result) {
    result = NULL;
    if (name_len <= 0 || name == NULL) return SENSOR_NAME_MISSING;
    if (name_len > SENSOR_NAME_MAX_LEN) return SENSOR_NAME_TOO_LONG;

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
    def->quoted = pq;
    def->influxdb_quoted = pi;
    def->next = NULL;
#ifdef INCLUDE_MQTT
    def->mqtt_rules = NULL;
#endif
    *pdef = def;
    result = def;
    return SENSOR_DEF_WAS_ADDED;
  }

#ifdef INCLUDE_MQTT
  void addMqttRule(MqttRule* rule) {
    if (rule != NULL) rule->linkTo(mqtt_rules);
  }
  MqttRule* getMqttRules() {
    return mqtt_rules;
  }
#endif

} SensorDef;

//-------------------------------------------------------------

typedef struct SensorData {
  SensorDef* def;
  time_t data_time;
  uint8_t protocol;
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

  uint32_t getId() {
    uint32_t variant = 0;
    uint32_t channel_bits = 0;
    uint32_t rolling_code;
    switch (protocol) {
    case PROTOCOL_F007TH:
      variant = u32.hi==1 ? 1 : 0; // 0 = F007TH, 1 = F007TP
      channel_bits = (nF007TH>>20)&7;
      rolling_code = (nF007TH >> 24) & 255;
      break;
    case PROTOCOL_00592TXR:
      channel_bits = (fields.channel>>6)&3;
      rolling_code = fields.rolling_code;
      break;
    case PROTOCOL_TX7U:
      rolling_code = u32.low >> 25;
      break;
    case PROTOCOL_HG02832:
      channel_bits = (u32.low>>12)&3;
      rolling_code = u32.low >> 24;
      break;
    case PROTOCOL_WH2:
      variant = ((u32.hi&0x80000000) != 0 ? 1 : 0) | ((u32.low>>24)&0x00f0); // 0x41 = FT007TH
      rolling_code = (u32.low>>20)&255;
      break;
    default:
      return 0;
    }
    return (protocol<<24) | (variant<<16) | (channel_bits<<8) | rolling_code;
  }

  static uint32_t getId(uint8_t protocol, uint8_t variant, uint8_t channel, uint8_t rolling_code) {
    uint32_t channel_bits = 0;
    switch (protocol) {
    case PROTOCOL_F007TH:
      channel_bits = (channel-1)&255;
      break;
    case PROTOCOL_00592TXR:
      switch (channel) {
      case 1: channel_bits = 3; break;
      case 2: channel_bits = 2; break;
      case 3: channel_bits = 0; break;
      }
      break;
    case PROTOCOL_HG02832:
      channel_bits = (channel-1)&255;
      break;
    }
    return (protocol<<24) | ((variant&255)<<16) | (channel_bits<<8) | (rolling_code&255);
  }

  int getChannel() {
    if (protocol == PROTOCOL_F007TH) return ((nF007TH>>20)&7)+1;
    if (protocol == PROTOCOL_00592TXR) return ((fields.channel>>6)&3)^3;
    if (protocol == PROTOCOL_HG02832) return ((u32.low>>12)&3)+1;
    return -1;
  }
  int getChannelNumber() {
      if (protocol == PROTOCOL_F007TH) return ((nF007TH>>20)&7)+1;
      if (protocol == PROTOCOL_00592TXR) {
        switch ((fields.channel>>6)&3) {
        case 3: return 1;
        case 2: return 2;
        case 0: return 3;
        }
      } else if (protocol == PROTOCOL_HG02832) {
        return ((u32.low>>12)&3)+1;
      }
    return -1;
  }
  bool hasBatteryStatus() {
    if ((protocol&(PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_HG02832)) != 0) return true;
    return false;
  }
  // true => OK
  bool getBatteryStatus() {
    if (protocol == PROTOCOL_F007TH) return (nF007TH&0x00800000) == 0;
    if (protocol == PROTOCOL_00592TXR) return fields.status==0x44;
    if (protocol == PROTOCOL_HG02832) return (u32.low&0x00008000) == 0;
    return false;
  }


  bool hasTemperature() {
    if ((protocol&(PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_HG02832|PROTOCOL_WH2)) != 0) return true;
    if (protocol == PROTOCOL_TX7U) return (u32.hi&15)==0 || (u32.hi&0x00800000)!=0;
    return false;
  }
  int getTemperature10(bool celsius) {
    return celsius ? getTemperatureCx10() : getTemperatureFx10();
  }
  int getTemperatureCx10() {
    if (protocol == PROTOCOL_F007TH)
      return (int)(((nF007TH>>8)&4095)-720) * 5 / 9;

    if (protocol == PROTOCOL_00592TXR)
      return (int)((((fields.t_hi)&127)<<7) | (fields.t_low&127)) - 1000;

    if (protocol == PROTOCOL_HG02832) {
      int result = (int)((u32.low)&0x0fff);
      if ((result&0x0800) != 0) result |= 0xfffff000;
      return result;
    }

    if (protocol == PROTOCOL_TX7U) {
      if (hasTemperature()) return getTX7temperature()-500;
      //return -2732;
    }

    if (protocol == PROTOCOL_WH2) {
      int t = (int)((u32.low>>8)&1023);
      if ( (t&0x0800)!=0 ) t = -(t&0x07ff);
      return t;
    }
    return -2732;
  }

  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10() {
    if (protocol == PROTOCOL_F007TH)
      return (int)((nF007TH>>8)&4095)-400;

    if (protocol == PROTOCOL_00592TXR) {
      int c = (int)((((fields.t_hi)&127)<<7) | (fields.t_low&127)) - 1000;
      return (c*9/5)+320;
    }
    if (protocol == PROTOCOL_HG02832) {
      return (getTemperatureCx10()*9/5)+320;
    }
    if (protocol == PROTOCOL_TX7U) {
      if (hasTemperature()) return ((getTX7temperature()-500)*9/5)+320;
      //return -4597;
    }
    if (protocol == PROTOCOL_WH2) {
      int t = (int)((u32.low>>8)&1023);
      if ( (t&0x0800)!=0 ) t = -(t&0x07ff);
      return t*9/5+32;
    }
    return -4597;
  }

  bool hasHumidity() {
    if ((protocol&PROTOCOL_F007TH) != 0) return (u32.hi&1) == 0;
    if ((protocol&(PROTOCOL_00592TXR|PROTOCOL_HG02832)) != 0) return true;
    if (protocol == PROTOCOL_TX7U) return (u32.hi&15)==14 || (u32.hi&0x80000000)!=0;
    if (protocol == PROTOCOL_WH2) return true;
    return false;
  }
  // Relative Humidity, 0..100%
  int getHumidity() {
    if (protocol == PROTOCOL_F007TH) return (u32.hi&1) == 0 ? nF007TH&255 : 0;
    if (protocol == PROTOCOL_00592TXR) return fields.rh & 127;
    if (protocol == PROTOCOL_TX7U) return hasHumidity() ? getTX7humidity() : 0;
    if (protocol == PROTOCOL_HG02832) return (u32.low>>16) & 255;
    if (protocol == PROTOCOL_WH2) return u32.low & 127;
    return 0;
  }

  const char* getSensorTypeName() {
    if (protocol == PROTOCOL_F007TH) return u32.hi==1 ? "F007TP" : "F007TH";
    if (protocol == PROTOCOL_00592TXR) return "00592TXR";
    if (protocol == PROTOCOL_TX7U) return "TX7";
    if (protocol == PROTOCOL_HG02832) return "HG02832";
    if (protocol == PROTOCOL_WH2) {
      if ((u32.hi&0x80000000) != 0) return "FT007TH";
      return "WH2";
    }
    return "UNKNOWN";
  }

  const char* getSensorTypeLongName() {
    if (protocol == PROTOCOL_F007TH)
      return u32.hi==1 ? "Ambient Weather F007TP" : "Ambient Weather F007TH";
    if (protocol == PROTOCOL_00592TXR) return "AcuRite 00592TXR";
    if (protocol == PROTOCOL_TX7U) return "LaCrosse TX3/TX6/TX7";
    if (protocol == PROTOCOL_HG02832) return "Auriol HG02832 (IAN 283582)";
    if (protocol == PROTOCOL_WH2) {
      if ((u32.hi&0x80000000) != 0) return "Telldus FT007TH";
      return "Fine Offset Electronics WH2";
    }
    return "UNKNOWN";
  }

  // random number that is changed when battery is changed
  uint8_t getRollingCode() {
    if (protocol == PROTOCOL_F007TH) return (nF007TH >> 24) & 255;
    if (protocol == PROTOCOL_00592TXR) return fields.rolling_code;
    if (protocol == PROTOCOL_TX7U) return (u32.low >> 25);
    if (protocol == PROTOCOL_HG02832) return (u32.low >> 24);
    if (protocol == PROTOCOL_WH2) return (u32.low >> 20) & 255;
    return 0;
  }


  size_t generateJson(char* ptr, size_t buffer_size, int options) {

    char dt[32];
    struct tm tm;
    if ((options&OPTION_UTC) != 0) { // UTC time zone
      tm = *gmtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%FT%TZ", &tm); // ISO format
    } else { // local time zone
      tm = *localtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm);
    }

    int len = snprintf(ptr, buffer_size, "{\"time\":\"%s\",\"type\":\"%s\"", dt, getSensorTypeName() );

    int channel = getChannelNumber();
    if (channel >= 0) len += snprintf(ptr+len, buffer_size-len, ",\"channel\":%d", channel);
    len += snprintf(ptr+len, buffer_size-len, ",\"rolling_code\":%d", getRollingCode());
    if (def != NULL && def->quoted != NULL)
      len += snprintf(ptr+len, buffer_size-len, ",\"name\":%s", def->quoted);
    if (hasTemperature())
      len += snprintf(ptr+len, buffer_size-len, ",\"temperature\":%d", getTemperature10((options&OPTION_CELSIUS) != 0));
    if (hasHumidity())
      len += snprintf(ptr+len, buffer_size-len, ",\"humidity\":%d", getHumidity());
    if (hasBatteryStatus())
      len += snprintf(ptr+len, buffer_size-len, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false");
    len += snprintf(ptr+len, buffer_size-len, "}");

    return (size_t)(len*sizeof(unsigned char));
  }

#ifdef INCLUDE_MQTT
  BoundCheckResult checkMqqtRule(MqttRule* rule, int changed_fields) {
    if (rule == NULL || changed_fields == 0) return BoundCheckResult::NotApplicable;
    if (rule->isLocked) return BoundCheckResult::Locked;

    time_t tm = time(NULL);
    struct tm* ltm = localtime(&tm);
    uint32_t day_time_offset = ltm->tm_hour*60 + ltm->tm_min;
    uint32_t week_time_offset = ltm->tm_wday*24*60 + day_time_offset;

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
#endif

protected:
  inline int getTX7temperature() {
    return
        (u32.hi&0x00800000)!=0
        ? (u32.hi>>12)&0x07ff
        : ((u32.low >> 20)&15)*100 + ((u32.low >> 16)&15)*10 + ((u32.low >> 12)&15);
  }
  inline int getTX7humidity() {
    return
        (u32.hi&0x80000000)!=0
        ? (u32.hi>>24)&0x7f
        : ((u32.low >> 20)&15)*10 + ((u32.low >> 16)&15);
  }

} SensorData;

//-------------------------------------------------------------

class SensorsData {
private:
  int size;
  int capacity;
  int options;
  struct SensorData *items;

public:
  SensorsData(int options) {
    items = (SensorData*)calloc(8, sizeof(SensorData));
    capacity = 8;
    size = 0;
    this->options = options;
  }
  SensorsData(int capacity, int options) {
    if (capacity <= 0) capacity = 8;
    items = (SensorData*)calloc(capacity, sizeof(SensorData));
    this->capacity = capacity;
    size = 0;
    this->options = options;
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

  SensorData* getData(uint8_t protocol, int channel, uint8_t rolling_code = -1) {
    if ((protocol&(PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_HG02832|PROTOCOL_TX7U|PROTOCOL_WH2)) == 0) return __null;
    if (protocol!=PROTOCOL_TX7U && (channel < 1 || channel > 8)) return __null;
    if (rolling_code > 255 || (rolling_code < 0 && rolling_code != -1)) return __null;

    if (protocol == PROTOCOL_F007TH) { //FIXME this method does not distinguish F007TH and F007TP
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
        if (p->protocol != PROTOCOL_00592TXR) continue;
        if (rolling_code != -1 && p->fields.rolling_code != rolling_code) continue;
        if (p->fields.channel == channel) return p;
      }
    } else if (protocol == PROTOCOL_HG02832) {
      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->protocol != PROTOCOL_HG02832) continue;
        if (rolling_code != -1 && (p->u32.low>>24) != rolling_code) continue;
        if (((p->u32.low>>12)&3)+1 == (uint8_t)channel) return p;
      }
    } else if (protocol == PROTOCOL_TX7U) {
      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->protocol != PROTOCOL_TX7U) continue;
        if (rolling_code == -1 || ((p->u32.low >> 25)&0x7f) == rolling_code) return p;
      }
    } else if (protocol == PROTOCOL_WH2) {
      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->protocol != PROTOCOL_WH2) continue;
        if (rolling_code == -1 || ((p->u32.low >> 20)&255) == rolling_code) return p;
      }
    }
    return __null;
  }

  int update(SensorData* sensorData, time_t data_time, time_t max_unchanged_gap) {
    if (sensorData->protocol == PROTOCOL_F007TH) {
      uint32_t nF007TH = sensorData->nF007TH;
      uint32_t uid = nF007TH & SENSOR_UID_MASK;
      uint32_t f007tp = sensorData->u32.hi;
      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->protocol != PROTOCOL_F007TH) continue;
        uint32_t item = p->nF007TH;
        if ((item & SENSOR_UID_MASK) == uid  && p->u32.hi == f007tp) {
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
            if ((f007tp&1) == 0 && (changed&SENSOR_HUMIDITY_MASK) != 0) result |= HUMIDITY_IS_CHANGED;
            if ((changed&BATTERY_STATUS_IS_CHANGED) != 0) result |= BATTERY_STATUS_IS_CHANGED;
          }
          if (result != 0) {
            p->u64 = sensorData->u64;
            p->data_time = data_time;
          }
          return result;
        }
      }
      add(sensorData, data_time);
      return TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED | NEW_UID;
    }

    if (sensorData->protocol == PROTOCOL_00592TXR) {
      uint8_t channel = sensorData->fields.channel;
      uint8_t rolling_code = sensorData->fields.rolling_code;

      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if (p->protocol != PROTOCOL_00592TXR) continue;
        if (p->fields.rolling_code != rolling_code) continue;
        if (p->fields.channel == channel) {
          sensorData->def = p->def;
          time_t gap = data_time - p->data_time;
          if (gap < 2L) return TIME_NOT_CHANGED;
          int result = 0;
          if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
            result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
          } else {
            if (p->fields.t_low != sensorData->fields.t_low || p->fields.t_hi != sensorData->fields.t_hi) result |= TEMPERATURE_IS_CHANGED;
            if (p->fields.rh != sensorData->fields.rh) result |= HUMIDITY_IS_CHANGED;
            if (p->fields.status != sensorData->fields.status) result |= BATTERY_STATUS_IS_CHANGED;
          }
          if (result != 0) {
            p->u64 = sensorData->u64;
            p->data_time = data_time;
          }
          return result;
        }
      }
      add(sensorData, data_time);
      return TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED | NEW_UID;
    }

    if (sensorData->protocol == PROTOCOL_TX7U) {
      uint16_t id = (sensorData->u64>>25)&0x7f; // type and rolling_code
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

      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        if ((p->protocol == PROTOCOL_TX7U) && (((p->u64>>25)&0x7f) == id)) {
          sensorData->def = p->def;
          if (((p->u32.hi)&mask) == new_value) {
            time_t gap = data_time - p->data_time;
            if (gap < 2L) return TIME_NOT_CHANGED;
            return (max_unchanged_gap > 0L && gap > max_unchanged_gap) ? result : 0;
          }
          p->data_time = data_time;
          p->u32.low = sensorData->u32.low;
          p->u32.hi = (p->u32.hi & ~mask) | new_value;
          return result;
        }
      }

      add(sensorData, data_time);
      return type == 0 ? TEMPERATURE_IS_CHANGED | NEW_UID : type == 14 ? HUMIDITY_IS_CHANGED | NEW_UID : NEW_UID;
    }

    if (sensorData->protocol == PROTOCOL_HG02832) {
      uint32_t new_w = sensorData->u32.low;

      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        uint32_t w;
        if (p->protocol != PROTOCOL_HG02832) continue;
        w = p->u32.low;
        if (w == new_w) {
          sensorData->def = p->def;
          return 0; // nothing has changed
        }
        if (((w^new_w)&0xff003000) == 0) { // compare rolling code and channel
          sensorData->def = p->def;
          time_t gap = data_time - p->data_time;
          if (gap < 2L) return TIME_NOT_CHANGED;
          int result = 0;
          if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
            result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
          } else {
            if (((w^new_w)&0x00000fff) != 0) result |= TEMPERATURE_IS_CHANGED;
            if (((w^new_w)&0x00ff0000) != 0) result |= HUMIDITY_IS_CHANGED;
            if (((w^new_w)&0x00008000) != 0) result |= BATTERY_STATUS_IS_CHANGED;
          }
          if (result != 0) {
            p->u64 = sensorData->u64;
            p->data_time = data_time;
          }
          return result;
        }
      }
      add(sensorData, data_time);
      return TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED | NEW_UID;
    }

    if (sensorData->protocol == PROTOCOL_WH2) {
      uint32_t new_w = sensorData->u32.low;

      for (int index = 0; index<size; index++) {
        SensorData* p = &items[index];
        uint32_t w;
        if (p->protocol != PROTOCOL_WH2) continue;
        w = p->u32.low;
        if (w == new_w) {
          sensorData->def = p->def;
          return 0; // nothing has changed
        }
        if (((w^new_w)&0x0ff00000) == 0) { // compare rolling code
          sensorData->def = p->def;
          time_t gap = data_time - p->data_time;
          if (gap < 2L) return TIME_NOT_CHANGED;
          int result = 0;
          if (max_unchanged_gap > 0L && gap > max_unchanged_gap) {
            result = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED;
          } else {
            if (((w^new_w)&0x0003ff00) != 0) result |= TEMPERATURE_IS_CHANGED;
            if (((w^new_w)&0x000000ff) != 0) result |= HUMIDITY_IS_CHANGED;
            //if (((w^new_w)&0x00080000) != 0) result |= BATTERY_STATUS_IS_CHANGED; TODO
          }
          if (result != 0) {
            p->u64 = sensorData->u64;
            p->data_time = data_time;
          }
          return result;
        }
      }
      add(sensorData, data_time);
      return TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | NEW_UID;
    }

    return 0;
  }

  size_t generateJson(int index, void*& buffer, size_t& buffer_size) {
    SensorData* sensorData = getItem(index);
    if (sensorData == __null) return 0;

    if (buffer == __null || buffer_size < JSON_SIZE_PER_ITEM) {
      buffer = realloc(buffer, JSON_SIZE_PER_ITEM);
      buffer_size = JSON_SIZE_PER_ITEM;
    }

    char* ptr = (char*)buffer;

    return sensorData->generateJson(ptr, buffer_size, options);
  }

  size_t generateJson(void*& buffer, size_t& buffer_size) {
    int nItems = getSize();
    if (nItems <= 0) return 0;

    size_t required_buffer_size = nItems*JSON_SIZE_PER_ITEM;
    if (buffer == __null || buffer_size < required_buffer_size) {
      buffer = realloc(buffer, required_buffer_size);
      buffer_size = required_buffer_size;
    }

    char* ptr = (char*)buffer;
    size_t total_len = 2;
    size_t len = 2;

    ptr[0] = '[';
    ptr[1] = '\n';
    ptr[2] = '\0';
    ptr += 2;

    for (int index=0; index<nItems; index++) {
      if (buffer_size-total_len-3 < JSON_SIZE_PER_ITEM) break;
      SensorData* sensorData = getItem(index);
      if (sensorData == __null) continue;
      if (total_len > 2) {
        ptr[0] = ',';
        ptr[1] = '\n';
        ptr += 2;
        total_len += 2;
      }
      len = sensorData->generateJson(ptr, buffer_size, options);
      if (len > 0) {
        total_len += len;
        ptr += len;
      } else if (total_len > 3) { // remove last comma
        total_len -= 2;
        ptr -= 2;
      }
    }
    if (buffer_size-total_len > 3) {
      ptr = (char*)buffer;
      ptr[total_len++] = '\n';
      ptr[total_len++] = ']';
      ptr[total_len] = '\0';
    }

    return total_len;
  }

private:

  void add(SensorData* item, time_t& data_time) {
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
    SensorData* new_item = &items[index];
    new_item->u64 = item->u64;
    new_item->data_time = data_time;
    new_item->protocol = item->protocol;
    if (item->protocol == PROTOCOL_TX7U) {
      if (item->hasTemperature()) {
        uint32_t raw_t = ((item->u32.low >> 20)&15)*100 + ((item->u32.low >> 16)&15)*10 + ((item->u32.low >> 12)&15);
        new_item->u32.hi |= 0x00800000 | (raw_t << 12);
      } else if (item->hasHumidity()) {
        uint32_t raw_h = (((item->u32.low >> 20)&15)*10 + ((item->u32.low >> 16)&15))&0x7f;
        new_item->u32.hi |= 0x80000000 | (raw_h << 24);
      }
    }
    new_item->def = SensorDef::find(item->getId());
  }

};

#endif /* SENSORSDATA_HPP_ */
