/*
 * CoinfigParser.hpp
 *
 *  Created on: December 27, 2020
 *      Author: Alex Konshin
 */

#ifndef COMMON_CONFIGPARSER_HPP_
#define COMMON_CONFIGPARSER_HPP_

#include <stdlib.h>
#include <stdint.h>
//#include "SensorsData.hpp"
#include "../utils/Logger.hpp"

//-------------------------------------------------------------
typedef struct Buffer {
  char* ptr = NULL;
  size_t size = 0;
} Buffer;


//-------------------------------------------------------------
class ConfigParser : public ErrorLogger {
private:
  FILE* configFileStream = NULL;

  Buffer buffers[2];
  const char* line = NULL;
  const char* next_line = NULL;
  int next_buffer_index = 0;
  int read_linenum = 0;
  const char* valid_options_text;

  void print_error_vargs(const char* fmt, va_list vargs);
  int getHex(char ch);
  char getHex(const char*& p);

public:
  int linenum = -1;
  const char* configFilePath = NULL;

  ConfigParser(const char* const valid_options_text);
  ~ConfigParser();

  void error_vargs(const char* fmt, va_list vargs);
  void error(const char* fmt, ...);
  void print_error(const char* fmt, ...);
  void print_valid_options();
  void errorWithHelp(const char* fmt, ...);

#define CAN_BE_FILE      1
#define CAN_BE_DIRECTORY 2
#define CAN_BE_FILE_OR_DIRECTORY (CAN_BE_FILE|CAN_BE_DIRECTORY)
#define MUST_EXIST       4
  const char* resolveFilePath(const char* relativePath, int options);

  bool open_file(const char* baseFilePath, const char* configFileRelativePath);
  void close_file();
  const char* read_line();
  const char* skipBlanks(const char*& p);
  const char* skipBlanksInCommand(const char*& p);
  const char* getWord(const char*& p, size_t& length);
  void removeTralingCRLF(const char* p);
  const char* getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, size_t* pkey_len);
  const char* getFirstWord(const char*& p, size_t& length);
};

//-------------------------------------------------------------
void errorMissingArg(const char* command, const char* argname, ConfigParser* errorLogger);
void errorInavidArg(const char* str, const char* argname, ConfigParser* errorLogger);
void errorInavidArg(const char* str, const char* p, const char* argname, ConfigParser* errorLogger);

bool convertDecimalArg(const char* str, int& result, int scale, bool allow_negative, const char* argname, ConfigParser* errorLogger);
bool convertDecimalArg(const char* str, const char*& p, int& result, int scale, bool allow_negative, char stop_char, const char* argname, ConfigParser* errorLogger);
uint32_t getDayTimeOffset(const char* arg, const char*& p, const char* argname, ConfigParser* errorLogger);

class AbstractRuleBoundSchedule;
AbstractRuleBoundSchedule* compileBoundSchedule(const char* arg, int scale, bool allow_negative, const char* argname, ConfigParser* errorLogger);

struct MessageInsert;
MessageInsert* compileMessage(const char* messageFormat, ConfigParser* errorLogger);

#endif /* COMMON_CONFIGPARSER_HPP_ */
