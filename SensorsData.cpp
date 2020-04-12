/*
 * SensorsData.cpp
 *
 *  Created on: April 11, 2020
 *      Author: Alex Konshin
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "SensorsData.hpp"

//-------------------------------------------------------------

#define T2D_BUFFER_SIZE 13
char* MessageInsert::t2d(int t, char* buffer, uint32_t& length) {
  char* p = buffer+T2D_BUFFER_SIZE-1;
  *p = '\0';
  bool negative = t<0;
  if (negative) t = -t;
  int d = t%10;
  if (d!=0) {
    *--p = ('0'+d);
    *--p = '.';
  }
  t = t/10;
  do {
    d = t%10;
    *--p = ('0'+d);
    t = t/10;
  } while (t > 0);
  if (negative) *--p = '-';
  length = buffer+T2D_BUFFER_SIZE-1-p;
  return p;
}

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
