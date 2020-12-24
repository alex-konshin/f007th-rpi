/*
 * Utils.hpp
 *
 *  Created on: November 28, 2020
 *      Author: akonshin
 */

#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <stdint.h>
#include "Logger.hpp"

#define T2D_BUFFER_SIZE 13


#define TIME2STR_BUFFER_SIZE 128

  /*
     char dt[TIME2STR_BUFFER_SIZE];
     const char* str = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, utc);
   */
  const char* convert_time(time_t* data_time, char* buffer, size_t buffer_size, bool utc);

  char* t2d(int t, char* buffer);
  char* t2d(int t, char* buffer, uint32_t& length);
  char* i2a(int n, char* buffer, uint32_t& length);

  const char* skipBlanks(const char*& p);
  int getHex(char ch, ErrorLogger* errorLogger);
  char getHex(const char*& p, ErrorLogger* errorLogger);
  const char* getWord(const char*& p, size_t& length);
  const char* getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, size_t* pkey_len, bool* quoted, bool* failed, ErrorLogger* errorLogger);

  int16_t readInt(const char*& p);
  uint32_t getUnsigned(const char*& p, ErrorLogger* errorLogger);

  const char* clone(const char* str);
  const char* make_str(const char* str, size_t length);

  void* resize_buffer(size_t required_buffer_size, void*& buffer, size_t& buffer_size);

#endif /* UTILS_HPP_ */
