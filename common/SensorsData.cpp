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

size_t SensorData::generateJsonLine(int start, void*& buffer, size_t& buffer_size, RestRequestType requestType) {
  if (def == NULL || def->quoted == NULL) return start;

  int len = 0;
  uint32_t dummy = 0;
  char t2d_buffer[T2D_BUFFER_SIZE];

  size_t required_buffer_size = strlen(def->quoted)+12;
  char* ptr = (char*)resize_buffer(required_buffer_size, buffer, buffer_size) + start;

  switch (requestType) {
  case RestRequestType::TemperatureF10:
  case RestRequestType::TemperatureC10:
    if (!hasTemperature()) return start;
    len = snprintf(ptr, buffer_size-len, "%s:%d", def->quoted, getTemperature10(requestType==RestRequestType::TemperatureC10));
    break;

  case RestRequestType::TemperatureF:
  case RestRequestType::TemperatureC:
    if (!hasTemperature()) return start;
    len = snprintf(ptr, buffer_size-len, "%s:%s", def->quoted, t2d(getTemperature10(requestType==RestRequestType::TemperatureC), t2d_buffer, dummy));
    break;

  case RestRequestType::Humidity:
    if (!hasHumidity()) return start;
    len = snprintf(ptr, buffer_size-len, "%s:%d", def->quoted, getHumidity());
    break;

  case RestRequestType::Battery:
    if (!hasBatteryStatus()) return start;
    len = snprintf(ptr, buffer_size-len, "%s:%s", def->quoted, getBatteryStatus() ? "true" :"false");
    break;

  default:
    ;
  }
  return len;
}


