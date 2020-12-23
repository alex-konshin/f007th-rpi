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
  if (len >= 0 && remain > (size_t)len) return true;
  Log->error("Buffer overflow (%s)", caller);
  return false;
}

size_t SensorData::generateJson(int start, void*& buffer, size_t& buffer_size, int options) {

  char* ptr = (char*)buffer+start;
  size_t remain = buffer_size-start;

  char dt[TIME2STR_BUFFER_SIZE];
  const char* timestr = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);

  int len;
  if (timestr != NULL)
    len = snprintf(ptr, remain, "{\"time\":\"%s\",\"type\":\"%s\"", timestr, getSensorTypeName() );
  else
    len = snprintf(ptr, remain, "{\"type\":\"%s\"", getSensorTypeName() );
  if (!check_buffer(remain, len, "SensorData::generateJson")) return start;

  const char* channel = getChannelName();
  if (channel != NULL) {
    len += snprintf(ptr+len, remain-len, ",\"channel\":\"%s\"", channel);
    if (!check_buffer(remain, len, "SensorData::generateJson")) return start;
  }
  len += snprintf(ptr+len, remain-len, ",\"rolling_code\":%d", getRollingCode());
  if (!check_buffer(remain, len, "SensorData::generateJson")) return start;
  if (def != NULL && def->quoted != NULL) {
    len += snprintf(ptr+len, remain-len, ",\"name\":%s", def->quoted);
    if (!check_buffer(remain, len, "SensorData::generateJson")) return start;
  }
  if (hasTemperature()) {
    len += snprintf(ptr+len, remain-len, ",\"temperature\":%d", getTemperature10((options&OPTION_CELSIUS) != 0));
    if (!check_buffer(remain, len, "SensorData::generateJson")) return start;
  }
  if (hasHumidity()) {
    len += snprintf(ptr+len, remain-len, ",\"humidity\":%d", getHumidity());
    if (!check_buffer(remain, len, "SensorData::generateJson")) return start;
  }
  if (hasBatteryStatus()) {
    len += snprintf(ptr+len, remain-len, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false");
    if (!check_buffer(remain, len, "SensorData::generateJson")) return start;
  }
  if (remain<(size_t)len+2) {
    Log->error("Buffer overflow (%s)", "SensorData::generateJson");
    return start;
  }
  *(ptr+len) = '}';
  len++;
  *(ptr+len) = '\0';

  return (size_t)len;
}

size_t SensorDataStored::generateJsonLine(int start, void*& buffer, size_t& buffer_size, RestRequestType requestType, int options) {
  if (def == NULL || def->quoted == NULL) return 0;

  if (requestType == RestRequestType::Brief)
    return generateJsonLineBrief(start, buffer, buffer_size, options);

  int len = 0;
  uint32_t dummy = 0;
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
    len = snprintf(ptr, remain, "%s:%s", def->quoted, t2d(getTemperature10(requestType==RestRequestType::TemperatureC), t2d_buffer, dummy));

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

size_t SensorDataStored::generateJsonLineBrief(int start, void*& buffer, size_t& buffer_size, int options) {
  if (def == NULL || def->quoted == NULL) return 0;

  const char* type_name = getSensorTypeLongName();
  if (type_name == NULL) return 0;

  // {"name":"Backyard","time":"2020-12-20 11:40:02 EST","type":"Acurite 00592TXR","temperature":-33.6,"humidity":66,"battery_ok":true,"t_history_size":10000,"h_history_size":10000}
  size_t required_buffer_size = start+strlen(type_name)+strlen(def->quoted)+157;
  char* ptr = (char*)resize_buffer(required_buffer_size*sizeof(unsigned char), buffer, buffer_size);
  if (ptr == NULL) {
    Log->error("Out of memory (%s)", "SensorDataStored::generateJsonLineBrief");
    return 0;
  }

  size_t remain = buffer_size-start;
  ptr += start;

  int len = snprintf(ptr, remain, "{\"name\":%s,\"type\":\"%s\"", def->quoted, type_name);
  if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;

  char dt[TIME2STR_BUFFER_SIZE];
  const char* timestr = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);
  if (timestr != NULL) {
    len += snprintf(ptr+len, remain-len, ",\"time\":\"%s\"", timestr);
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }
  bool hasT = hasTemperature();
  if (hasT) {
    uint32_t dummy = 0;
    int t = getTemperature10((options&OPTION_CELSIUS) != 0);
    len += snprintf(ptr+len, remain-len, ",\"temperature\":%s", t2d(t, dt, dummy));
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
    len += snprintf(ptr+len, remain-len, ",\"t_history_size\":%d", temperatureHistory.getCount());
    if (!check_buffer(remain, len, "SensorDataStored::generateJsonLineBrief")) return 0;
  }
  if (hasH)
    len += snprintf(ptr+len, remain-len, ",\"h_history_size\":%d", humidityHistory.getCount());
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
      // {"name":"Backyard","time":"2020-12-20 11:40:02 EST","type":"Acurite 00592TXR","temperature":-33.6,"humidity":66,"battery_ok":true,"t_history_size":10000,"h_history_size":10000}
      //size_t required_buffer_size = start+strlen(type_name)+strlen(def->quoted)+150;
      required_line_size = 50+MAX_SENSOR_NAME_LEN+157+2;
    } else {
      required_line_size = JSON_SIZE_PER_ITEM+2;
    }
    size_t required_buffer_size = nItems*required_line_size-2+5;

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
      len = sensorData->generateJsonLine(total_len, buffer, buffer_size, requestType, options);
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
    len = sensorData->generateJson(total_len, buffer, buffer_size, options);
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

