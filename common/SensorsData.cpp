/*
 * SensorsData.cpp
 *
 *  Created on: April 11, 2020
 *      Author: Alex Konshin
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cmath>
#include <vector>

#include "../utils/Logger.hpp"
#include "../utils/Utils.hpp"
#include "SensorsData.hpp"
#include "Config.hpp"

SensorDef* SensorDef::sensorDefs = NULL;

//-------------------------------------------------------------
void ActionRule::execute(const char* message, class Config& cfg) {
  bool debug = (cfg.options&VERBOSITY_DEBUG) != 0;
  //bool verbose = (cfg.options&VERBOSITY_INFO) != 0;
  if (debug) Log->info("====> %s \"%s\" => command: '%s'\n", getTypeName(), id, message);

  //system(message);
  const char* p = message;
  Logger* logger = Logger::defaultLogger;

  std::vector<const char*> vargs;

  char* arg_buffer = NULL;
  size_t arg_bufsize = 0;
  const char* raw_arg;
  size_t arg_len = 0;
  bool failed = false;
  while ( (raw_arg=getString(p, arg_buffer, arg_bufsize, arg_len, NULL, NULL, &failed, logger)) !=NULL && !failed) {
    const char* arg = make_str(raw_arg, arg_len);
    vargs.push_back(arg);
  }
  if (arg_buffer != NULL) free(arg_buffer);
  if (failed) {
    logger->error("Failed to parse system command: %s", message);
  } else {

    int argc = vargs.size();
    if (argc == 0) {
      logger->error("Empty command");
    } else {

      const char** args = (const char**)calloc(argc+1, sizeof(const char*));

      int iarg = 0;
      for (const char* arg : vargs) {
        if (debug) logger->info("   args[%d] = \"%s\"",iarg,arg);
        args[iarg++] = arg;
      }
      args[iarg] = NULL;

      pid_t pid;

      if ((pid = fork()) == -1) {
         perror("fork() error");
      } else if (pid == 0) {
         execvp(args[0], (char *const*)args);
         logger->error("Error on execvp().");
         exit(2);
      }

      free((void*)args);
    }
  }
  if (!vargs.empty()) {
    for (const char* arg : vargs) {
      free((void*)arg);
    }
    vargs.clear();
  }

}

//-------------------------------------------------------------

void MessageInsert::append(char*& output, uint32_t& remain, const char* str, uint32_t len) {
  if (len == 0 || remain <= 0) return;
  if (len > remain) len = remain;
  memcpy(output, str, len);
  output += len;
  remain -= len;
}

void MessageInsert::append(char*& output, uint32_t& remain, const char* str) {
  if (remain <= 0) return;
  uint32_t len = str==NULL ? 0 : strlen(str);
  if (len == 0) return;
  if (len > remain) len = remain;
  memcpy(output, str, len);
  output += len;
  remain -= len;
}

void MessageInsert::append(char*& output, uint32_t& remain, struct SensorData* data, const char* referenceId) {
  uint32_t len;
  int intValue;
  const char* str;
  char t2d_buffer[T2D_BUFFER_SIZE];

  switch(type) {
  case MessageInsertType::Constant:
    append(output, remain, stringArg);
    break;

  case MessageInsertType::ReferenceId:
    append(output, remain, referenceId);
    break;

  case MessageInsertType::SensorName:
    if (data != NULL && data->def != NULL) append(output, remain, data->def->name);
    break;

  case MessageInsertType::TemperatureF:
    if (remain < 5 || data == NULL) break;
    intValue = data->getTemperatureFx10();
    str = t2d(intValue, t2d_buffer, len);
    append(output, remain, str, len);
    break;

  case MessageInsertType::TemperatureC:
    if (remain < 5 || data == NULL) break;
    intValue = data->getTemperatureCx10();
    str = t2d(intValue, t2d_buffer, len);
    append(output, remain, str, len);
    break;

  case MessageInsertType::TemperaturedF:
    if (remain < 5 || data == NULL) break;
    intValue = data->getTemperatureFx10();
    str = i2a(intValue, t2d_buffer, len);
    append(output, remain, str, len);
    break;

  case MessageInsertType::TemperaturedC:
    if (remain < 5 || data == NULL) break;
    intValue = data->getTemperatureCx10();
    str = i2a(intValue, t2d_buffer, len);
    append(output, remain, str, len);
    break;

  case MessageInsertType::Humidity:
    if (remain < 5 || data == NULL) break;
    intValue = data->getHumidity();
    str = i2a(intValue, t2d_buffer, len);
    append(output, remain, str, len);
    break;

  case MessageInsertType::BatteryStatusInt:
    if (remain < 3 || data == NULL) break;
    intValue = data->getBatteryStatus();
    append(output, remain, intValue == 0 ? "0" : "1");
    break;

  case MessageInsertType::BatteryStatusStr:
    if (remain < 4 || data == NULL) break;
    intValue = data->getBatteryStatus();
    append(output, remain, intValue == 0 ? "Low" : "OK");
    break;

  case MessageInsertType::Percent:
    if (remain >=2) append(output, remain, "%");
    break;
  }
}

uint32_t MessageInsert::formatMessage(char* buffer, uint32_t buffer_size, const char* messageFormat, struct SensorData* data, const char* referenceId) {
  char* output = buffer;
  uint32_t remain = buffer_size-1;
  for ( MessageInsert* insert = this; insert->type!=MessageInsertType::Constant || insert->stringArg!=NULL; insert++) {
    insert->append(output, remain, data, referenceId);
  }
  *output = '\0';
  return output-buffer;
}

static bool check_buffer(size_t remain, int len, const char* caller) {
  if (len >= 0 && remain > (size_t)len+1) return true;
  Log->error("Buffer overflow (%s)", caller);
  return false;
}

void SensorData::print(FILE* file, int options) {
  uint32_t features = getFeatures();

  if (def != NULL && def->name != NULL) {
    fprintf(file, "  name              = %s\n", def->name);
  }
  fprintf(file, "  type              = %s\n", getSensorTypeLongName());

  if ((features&(FEATURE_CHANNEL|FEATURE_ROLLING_CODE)) != 0) {
    if ((features&FEATURE_CHANNEL) != 0) {
      const char* channel = getChannelName();
      if (channel != NULL) fprintf(file, "  channel           = %s\n", channel);
    }
    if ((features&FEATURE_ROLLING_CODE) != 0) {
      uint16_t rc = getRollingCode();
      fprintf(file, "  rolling code      = 0x%04x(%u)\n", rc, rc);
    }
  } else if ((features&FEATURE_ID32) != 0) {
    fprintf(file, "  id                = %08x\n", getId());
  }

  if ((features&FEATURE_TEMPERATURE) != 0) {
    int temperature = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
    char t2d_buffer[T2D_BUFFER_SIZE];
    fprintf(file, "  temperature       = %s%c\n", t2d(temperature, t2d_buffer), (options&OPTION_CELSIUS) != 0 ? 'C' : 'F');
  }
  if ((features&FEATURE_HUMIDITY) != 0) {
    fprintf(file, "  humidity          = %d%%\n", getHumidity());
  }
  if ((features&FEATURE_BATTERY_STATUS) != 0) {
    fprintf(file, "  battery           = %s\n", getBatteryStatus() ? "OK" : "Low");
  }
}

size_t SensorData::generateJsonContent(int start, void*& buffer, size_t& buffer_size, int options) {
  uint32_t features = getFeatures();

  char* ptr = (char*)buffer+start;
  size_t remain = buffer_size-start;

  char dt[TIME2STR_BUFFER_SIZE];
  const char* timestr = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);

  int len;
  if (timestr != NULL)
    len = snprintf(ptr, remain, "{\"time\":\"%s\",\"type\":\"%s\"", timestr, getSensorTypeName() );
  else
    len = snprintf(ptr, remain, "{\"type\":\"%s\"", getSensorTypeName() );
  if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;

  if ((features&(FEATURE_CHANNEL|FEATURE_ROLLING_CODE)) != 0) {
    if ((features&FEATURE_CHANNEL) != 0) {
      const char* channel = getChannelName();
      if (channel != NULL) {
        len += snprintf(ptr+len, remain-len, ",\"channel\":\"%s\"", channel);
        if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
      }
    }
    if ((features&FEATURE_ROLLING_CODE) != 0) {
      len += snprintf(ptr+len, remain-len, ",\"rolling_code\":%d", getRollingCode());
      if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
    }
  } else if ((features&FEATURE_ID32) != 0) {
    len += snprintf(ptr+len, remain-len, ",\"id\":\"%08x\"", getId());
    if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
  }
  if (def != NULL && def->quoted != NULL) {
    len += snprintf(ptr+len, remain-len, ",\"name\":%s", def->quoted);
    if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
  }
  if ((features&FEATURE_TEMPERATURE) != 0) {
    len += snprintf(ptr+len, remain-len, ",\"temperature\":%d", getTemperature10((options&OPTION_CELSIUS) != 0));
    if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
  }
  if ((features&FEATURE_HUMIDITY) != 0) {
    len += snprintf(ptr+len, remain-len, ",\"humidity\":%d", getHumidity());
    if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
  }
  if ((features&FEATURE_BATTERY_STATUS) != 0) {
    len += snprintf(ptr+len, remain-len, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false");
    if (!check_buffer(remain, len, "SensorData::generateJson")) return 0;
  }

  return (size_t)len;
}

size_t SensorData::generateJson(int start, void*& buffer, size_t& buffer_size, int options) {
  size_t required_buffer_size = start + JSON_SIZE_PER_ITEM_ALLDATA + 2;
  char* ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size)+start;
  if (buffer == NULL) {
    Log->error("Out of memory (%s)", "SensorData::generateJson");
    return 0;
  }

  size_t size = generateJsonContent(start, buffer, buffer_size, options);

  size_t remain = buffer_size-start-size;
  if (remain < size+2) {
    Log->error("Buffer overflow (%s)", "SensorData::generateJson");
    return 0;
  }
  *(ptr+size) = '}';
  size++;
  *(ptr+size) = '\0';

  return size;
}

size_t SensorData::generateInfluxData(int start, void*& buffer, size_t& buffer_size, int changed, int options) {
  uint32_t features = getFeatures();

  char t2d_buffer[T2D_BUFFER_SIZE];
#define ID_BUFFER_SIZE 512
  char id[ID_BUFFER_SIZE];
  int len;
  size_t remain = ID_BUFFER_SIZE;

  len = snprintf(id, sizeof(id), "type=%s", getSensorTypeName());
  if ((features&(FEATURE_CHANNEL|FEATURE_ROLLING_CODE)) != 0) {
    if ((features&FEATURE_CHANNEL) != 0) {
      len += snprintf(id+len, remain-len, ",channel=%d", getChannelNumber());
      if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
    }
    if ((features&FEATURE_ROLLING_CODE) != 0) {
      len += snprintf(id+len, remain-len, ",rolling_code=%d", getRollingCode());
      if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
    }
  } else if ((features&FEATURE_ID32) != 0) {
    len += snprintf(id+len, remain-len, ",id=%08x", getId());
    if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
  }
  if (def != NULL && def->influxdb_quoted != NULL) {
    len += snprintf(id+len, remain-len, ",name=%s", def->influxdb_quoted);
    if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
  }

  size_t required_buffer_size = start + (40+len)*3 + 2;
  char* ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size)+start;
  if (buffer == NULL) {
    Log->error("Out of memory (%s required_buffer_size=%ld)", "SensorData::generateInfluxData", required_buffer_size);
    return 0;
  }

  remain = buffer_size - start;
  if ((features&FEATURE_TEMPERATURE) != 0 && (changed&TEMPERATURE_IS_CHANGED) != 0) {
    int t = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
    int len = snprintf(ptr, remain, "temperature,%s value=%s\n", id, t2d(t, t2d_buffer));
    if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
    remain -= len;
    ptr += len;
  }
  if ((features&FEATURE_HUMIDITY) != 0 && (changed&HUMIDITY_IS_CHANGED) != 0) {
    int len = snprintf(ptr, remain, "humidity,%s value=%d\n", id, getHumidity());
    if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
    remain -= len;
    ptr += len;
  }
  if ((features&FEATURE_BATTERY_STATUS) != 0 && (changed&BATTERY_STATUS_IS_CHANGED) != 0) {
    int len = snprintf(ptr, remain, "sensor_battery_status,%s value=%s\n", id, getBatteryStatus() ? "true" :"false");
    if (!check_buffer(remain, len, "SensorData::generateInfluxData")) return 0;
    remain -= len;
    ptr += len;
  }

  if ((options&VERBOSITY_INFO) != 0) {
    fputs((char*)buffer, stderr);
    fputc('\n', stderr);
  }

  return buffer_size - remain;
}

size_t SensorDataStored::generateJsonEx(int start, void*& buffer, size_t& buffer_size, int options) {
  size_t size = generateJsonContent(start, buffer, buffer_size, options);
  char* ptr = (char*)buffer+start;
  size_t remain = buffer_size-start;

#ifdef INCLUDE_HTTPD
  uint32_t features = getFeatures();
  if ((features&FEATURE_TEMPERATURE) != 0) {
    unsigned count = temperatureHistory.getCount();
    if (count != 0) {
      size += snprintf(ptr+size, remain-size, ",\"t_hist\":%d", count);
      if (!check_buffer(remain, size, "SensorDataStored::generateJsonEx")) return 0;
    }
  }
  if ((features&FEATURE_HUMIDITY) != 0) {
    unsigned count = humidityHistory.getCount();
    if (count != 0) {
      size += snprintf(ptr+size, remain-size, ",\"h_hist\":%d", count);
      if (!check_buffer(remain, size, "SensorDataStored::generateJsonEx")) return 0;
    }
  }
#endif
  if (remain < size+2) {
    Log->error("Buffer overflow (%s)", "SensorData::generateJsonEx");
    return 0;
  }
  *(ptr+size) = '}';
  size++;
  *(ptr+size) = '\0';

  return size;
}

size_t SensorDataStored::generateJsonLine(int start, void*& buffer, size_t& buffer_size, RestRequestType requestType, int options) {
  if (def == NULL || def->quoted == NULL) return 0;

  if (requestType == RestRequestType::Brief) {
    Log->error("Invalid call of %s", "SensorDataStored::generateJsonLine");
    return 0;
  }

  int len = 0;
  char t2d_buffer[T2D_BUFFER_SIZE];

  size_t required_buffer_size = (start+strlen(def->quoted)+12)*sizeof(unsigned char);
  char* ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size) + start;
  if (ptr == NULL) {
    Log->error("Out of memory (%s)", "SensorDataStored::generateJsonLine");
    return 0;
  }

  size_t remain = buffer_size-start;

  switch (requestType) {
  case RestRequestType::TemperatureF10:
  case RestRequestType::TemperatureC10:
    if (!hasTemperature()) return 0;
    len = snprintf(ptr, remain, "%s:%d", def->quoted, getTemperature10(requestType==RestRequestType::TemperatureC10));
    break;

  case RestRequestType::TemperatureF:
  case RestRequestType::TemperatureC:
    if (!hasTemperature()) return 0;
    len = snprintf(ptr, remain, "%s:%s", def->quoted, t2d(getTemperature10(requestType==RestRequestType::TemperatureC), t2d_buffer));

    break;

  case RestRequestType::Humidity:
    if (!hasHumidity()) return 0;
    len = snprintf(ptr, remain, "%s:%d", def->quoted, getHumidity());
    break;

  case RestRequestType::Battery:
    if (!hasBatteryStatus()) return 0;
    len = snprintf(ptr, remain, "%s:%s", def->quoted, getBatteryStatus() ? "true" :"false");
    break;

  default:
    ;
  }
  return (size_t)len;
}

size_t SensorDataStored::generateJsonLineBrief(int start, void*& buffer, size_t& buffer_size, time_t current_time, int options) {
  if (def == NULL || def->quoted == NULL) return 0;

  const char* type_name = getSensorTypeLongName();
  if (type_name == NULL) return 0;

  // {"name":"Backyard","last":99999,"temperature":-33.6,"humidity":66,"battery_ok":true,"t_hist":10000,"h_hist":10000}
  size_t required_buffer_size = start+strlen(type_name)+strlen(def->quoted)+125;
  char* ptr = (char*)resize_buffer(required_buffer_size*sizeof(unsigned char), buffer, buffer_size);
  if (ptr == NULL) {
    Log->error("Out of memory (%s)", "SensorDataStored::generateJsonLineBrief");
    return 0;
  }

  size_t remain = buffer_size-start;
  ptr += start;

  int len = snprintf(ptr, remain, "{\"name\":%s", def->quoted);
  if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;

  if (data_time != 0) {
    long diff_minutes = lround( difftime(current_time, data_time)/60 );
    len += snprintf(ptr+len, remain-len, ",\"last\":%ld", diff_minutes);
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }

  bool hasT = hasTemperature();
  if (hasT) {
    char t2d_buffer[T2D_BUFFER_SIZE];
    int t = getTemperature10((options&OPTION_CELSIUS) != 0);
    len += snprintf(ptr+len, remain-len, ",\"temperature\":%s", t2d(t, t2d_buffer));
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }
  bool hasH = hasHumidity();
  if (hasH) {
    len += snprintf(ptr+len, remain-len, ",\"humidity\":%d", getHumidity());
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }
  if (hasBatteryStatus()) {
    len += snprintf(ptr+len, remain-len, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false");
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }
#ifdef INCLUDE_HTTPD
  if (hasT) {
    len += snprintf(ptr+len, remain-len, ",\"t_hist\":%d", temperatureHistory.getCount());
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }
  if (hasH)
    len += snprintf(ptr+len, remain-len, ",\"h_hist\":%d", humidityHistory.getCount());
#endif

  if (remain<(size_t)len+2) {
    Log->error("Buffer overflow (%s)", "SensorDataStored::generateJsonLineBrief");
    return 0;
  }
  *(ptr+len) = '}';
  len++;
  *(ptr+len) = '\0';

  return (size_t)len;
}

size_t SensorsData::generateJson(void*& buffer, size_t& buffer_size, RestRequestType requestType, int options) {
  int nItems = 0;
  SensorDataStored** snapshot = getSnapshot(nItems);
  if (snapshot == NULL || nItems <= 0) return 0;

  size_t result;
  if (requestType == RestRequestType::AllData) {

    result = generateJsonAllData(snapshot, nItems, buffer, buffer_size, options);

  } else {

    size_t required_line_size;
    if (requestType == RestRequestType::Brief) {
      // {"name":"Backyard","last":99999,"temperature":-33.6,"humidity":66,"battery_ok":true,"t_history_size":10000,"h_history_size":10000}
      //size_t required_buffer_size = start+strlen(type_name)+strlen(def->quoted)+150;
      required_line_size = 50+MAX_SENSOR_NAME_LEN+125+2;
    } else {
      required_line_size = JSON_SIZE_PER_ITEM+2;
    }
    size_t required_buffer_size = nItems*required_line_size-2+5;

    time_t current_time = requestType == RestRequestType::Brief ? time(NULL) : 0;

    char* ptr = (char*)resize_buffer(required_buffer_size*sizeof(unsigned char), buffer, buffer_size);
    size_t len = 2;
    size_t total_len = 2;
    ptr[0] = requestType == RestRequestType::Brief ? '[' : '{';
    ptr[1] = '\n';
    ptr[2] = '\0';
    ptr += 2;

    for (int index=0; index<nItems; index++) {
      required_buffer_size = total_len + (nItems-index)*required_line_size+1;
      char* ptr = (char*)resize_buffer(required_buffer_size*sizeof(unsigned char), buffer, buffer_size);
      SensorDataStored* sensorData = snapshot[index];
      if (sensorData == NULL) continue;
      if (total_len > 2) {
        ptr[total_len++] = ',';
        ptr[total_len++] = '\n';
        ptr += 2;
      }
      if (requestType == RestRequestType::Brief) {
        len = sensorData->generateJsonLineBrief(total_len, buffer, buffer_size, current_time, options);
      } else {
        len = sensorData->generateJsonLine(total_len, buffer, buffer_size, requestType, options);
      }
      if (len > 0) {
        total_len += len;
        ptr += len;
      } else if (total_len > 2) { // remove last comma
        total_len -= 2;
        ptr -= 2;
      }
    }
    if (buffer_size-total_len > 3) {
      ptr = (char*)buffer;
      ptr[total_len++] = '\n';
      ptr[total_len++] = requestType == RestRequestType::Brief ? ']' : '}';
      ptr[total_len] = '\0';
    }
    result = total_len;
  }

  free(snapshot);
  return result;
}

size_t SensorsData::generateJsonAllData(SensorDataStored** items, int nItems, void*& buffer, size_t& buffer_size, int options) {
  size_t required_buffer_size = nItems*(JSON_SIZE_PER_ITEM_ALLDATA+2)-2+5;
  char* ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size);
  size_t len;
  size_t total_len = 2;
  ptr[0] = '[';
  ptr[1] = '\n';
  ptr[2] = '\0';
  ptr += 2;

  for (int index=0; index<nItems; index++) {
    required_buffer_size = total_len + (nItems-index)*(JSON_SIZE_PER_ITEM_ALLDATA+2);
    ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size)+total_len;
    SensorDataStored* sensorData = items[index];
    if (sensorData == NULL) continue;
    if (total_len > 2) {
      ptr[0] = ',';
      ptr[1] = '\n';
      ptr += 2;
      total_len += 2;
    }
    len = sensorData->generateJsonEx(total_len, buffer, buffer_size, options);
    if (len > 0) {
      total_len += len;
      ptr += len;
    } else if (total_len > 2) { // remove last comma
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

