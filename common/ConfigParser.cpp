/*
 * ConfigParser.cpp
 *
 *  Created on: December 27, 2020
 *      Author: Alex Konshin
 */

#include "ConfigParser.hpp"
#include "SensorsData.hpp"
#include "../utils/Logger.hpp"
#include "../utils/Utils.hpp"

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>


//-------------------------------------------------------------
ConfigParser::ConfigParser(const char* const valid_options_text) : ErrorLogger(LOGGER_FLAG_STDERR) {
  this->valid_options_text = valid_options_text;
  buffers[0].ptr = NULL;
  buffers[0].size = 0;
  buffers[1].ptr = NULL;
  buffers[1].size = 0;
}

//-------------------------------------------------------------
ConfigParser::~ConfigParser() {
  close_file();
  line = NULL;
  next_line = NULL;
  if (configFilePath != NULL) {
    free((void*)configFilePath);
    configFilePath = NULL;
  }
}

//-------------------------------------------------------------
void ConfigParser::print_error_vargs(const char* fmt, va_list vargs) {
  char buffer[BUFFER_SIZE];
  format_message(buffer, NULL, fmt, vargs);
  fprintf(stderr, "ERROR: %s (line #%d of file \"%s\").\n", buffer, linenum, configFilePath);
}

void ConfigParser::error_vargs(const char* fmt, va_list vargs) {
  print_error_vargs(fmt, vargs);
  exit(1);
}

void ConfigParser::error(const char* fmt, ...) {
  va_list vargs;
  va_start(vargs, fmt);
  print_error_vargs(fmt, vargs);
  va_end(vargs);
  exit(1);
}

void ConfigParser::print_error(const char* fmt, ...) {
  va_list vargs;
  va_start(vargs, fmt);
  print_error_vargs(fmt, vargs);
  va_end(vargs);
}

void ConfigParser::print_valid_options() {
  fputs( valid_options_text, stderr );
}
void ConfigParser::errorWithHelp(const char* fmt, ...) {
  va_list vargs;
  va_start(vargs, fmt);
  print_error_vargs(fmt, vargs);
  va_end(vargs);
  print_valid_options();
  fflush(stderr);
  fclose(stderr);
  exit(1);
}

//-------------------------------------------------------------
// Returns normalized absolute path to file/directory.
const char* ConfigParser::resolveFilePath(const char* relativePath, int options) {
  if (relativePath == NULL || *relativePath == '\0') error("File/directory path should not be empty");

  const char* basefile= this->configFilePath;
  if (basefile == NULL || *basefile == '\0') error("Program error: the path of configuration file is not set yet");

  const char* basedir = getParentFolderPath(basefile, 1);
  if (basedir == NULL) error("File/directory path should not be empty");

  const char* result_path = buildFilePath(basedir, relativePath);
  free((void*)basedir);
  if (result_path == NULL) error("Cannot resolve file/directory path \"%s\"", relativePath);
  if (options != 0) {
    struct stat file_stat;
    int fd = open(result_path, O_RDONLY);
    bool exist = fd != -1 && fstat(fd, &file_stat) == 0;
    if (fd != -1) close(fd);
    if (!exist) {
      if ((options&MUST_EXIST) != 0) {
        if ((options&CAN_BE_FILE) != 0) error("File \"%s\" does not exist or is not accessible", result_path);
        if ((options&CAN_BE_DIRECTORY) != 0) error("Directory \"%s\" does not exist or is not accessible", result_path);
        error("File/directory \"%s\" does not exist or is not accessible", result_path);
      }
    } else if ((options&(CAN_BE_FILE_OR_DIRECTORY)) != 0) {
      if ((options&(CAN_BE_FILE_OR_DIRECTORY)) == CAN_BE_FILE_OR_DIRECTORY) {
        if (!S_ISREG(file_stat.st_mode) && !S_ISDIR(file_stat.st_mode)) error("File system entry \"%s\" neither a file nor a directory", result_path);
      } else if ((options&CAN_BE_FILE) != 0) {
        if (!S_ISREG(file_stat.st_mode)) error("File system entry \"%s\" is not a regular file", result_path);
      } else if ((options&CAN_BE_DIRECTORY) != 0) {
        if (!S_ISDIR(file_stat.st_mode)) error("File system entry \"%s\" is not a directory", result_path);
      }
    }
  }
  return result_path;
}

//-------------------------------------------------------------
bool ConfigParser::open_file(const char* baseFilePath, const char* configFileRelativePath) { // FIXME add base path
  if (configFileStream != NULL) {
    fprintf(stderr, "ERROR: Program error - attempt to open configuration file \"%s\" when another file is already opened in this parser.\n", configFilePath);
    return false;
  }

  const char* configFilePath;
  if (baseFilePath == NULL || *baseFilePath == '\0') {
    configFilePath = realpath(configFileRelativePath, NULL);
    if (configFilePath == NULL) {
      fprintf(stderr, "ERROR: Configuration file \"%s\" does not exist.\n", configFileRelativePath);
      return false;
    }
  } else {
    const char* basedir = getParentFolderPath(baseFilePath, 1);
    if (basedir == NULL) {
      fprintf(stderr, "ERROR: Program error - failed to get parent folder path of configuration file \"%s\".\n", baseFilePath);
      return false;
    }
    configFilePath = buildFilePath(basedir, configFileRelativePath);
    free((void*)basedir);
  }

  this->configFilePath = configFilePath;

  configFileStream = fopen(configFilePath, "r");
  if (configFileStream == NULL) {
    fprintf(stderr, "ERROR: Cannot open configuration file \"%s\".\n", configFilePath);
    return false;
  }

  fprintf(stderr, "Reading configuration file \"%s\"...\n", configFilePath);
  linenum = read_linenum = 0;
  line = NULL;
  next_line = NULL;
  return true;
}

//-------------------------------------------------------------
void ConfigParser::close_file() {
  if (configFileStream != NULL) {
    fclose(configFileStream);
    configFileStream = NULL;
  }
}

//-------------------------------------------------------------
const char* ConfigParser::read_line() {
  if (next_line != NULL) {
    line = next_line;
    next_line = NULL;
    linenum++;
  } else {
    line = NULL;
    if (configFileStream == NULL) return NULL;

    Buffer& buffer = buffers[next_buffer_index];
    ssize_t bytesread;
    if ((bytesread = getline(&buffer.ptr, &buffer.size, configFileStream)) == -1) {
      close_file();
      return NULL;
    }
    next_buffer_index ^= 1;
    line = buffer.ptr;
    removeTralingCRLF(line);
    linenum = ++read_linenum;
  }
  return line;
}

//-------------------------------------------------------------
const char* ConfigParser::skipBlanks(const char*& p) {
  if (p == NULL) return NULL;
  char ch;
  while ((ch=*p) ==' ' || ch == '\t') p++;
  if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') p = NULL;
  return p;
}

//-------------------------------------------------------------
// skip blanks but may switch to the next line if it starts with blank
const char* ConfigParser::skipBlanksInCommand(const char*& p) {
  char ch;
  if (p != NULL) {
    while ((ch=*p) ==' ' || ch == '\t') p++;
  }
  if (p == NULL || ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') {
    p = NULL;
    // check next line: if it starts with blank then it is continuation line
    if (next_line == NULL) {
      if (configFileStream == NULL) return NULL;

      Buffer& buffer = buffers[next_buffer_index];
      ssize_t bytesread;
      if ((bytesread = getline(&buffer.ptr, &buffer.size, configFileStream)) == -1) {
        close_file();
        return NULL;
      }
      next_buffer_index ^= 1;
      read_linenum++;
      next_line = buffer.ptr;
      removeTralingCRLF(next_line);
    }

    if ((ch=*next_line) ==' ' || ch == '\t') {
      line = p = next_line;
      next_line = NULL;
      while ((ch=*p) ==' ' || ch == '\t') p++;
      if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') {
        // next line is empty
        return p = NULL;
      }
      linenum++;
    }
  }
  return p;
}

//-------------------------------------------------------------
const char* ConfigParser::getWord(const char*& p, size_t& length) {
  length = 0;
  const char* start = skipBlanksInCommand(p);
  if (start == NULL) return p = NULL;

  char ch;
  while ((ch=*p) !=' ' && ch != '\t') {
    if (ch == '\0' || ch == '\n' || ch == '\r') {
      length = p-start;
      p = NULL;
      return start;
    }
    p++;
  }

  length = p-start;
  return start;
}

//-------------------------------------------------------------
const char* ConfigParser::getFirstWord(const char*& p, size_t& length) {
  length = 0;
  char ch;
  if (p==NULL || (ch=*p)=='\0' || ch=='\n' || ch =='\r' || ch=='#') return p = NULL;
  if (ch==' ' || ch=='\t') { // line starts with blank
    error("First character of command should not be blank");
  }

  const char* start = p;
  do {
    p++;
    if ((ch=*p)=='\0' || ch=='\n' || ch=='\r' || ch=='#') {
      length = p-start;
      p = NULL;
      return start;
    }
  } while (ch!=' ' && ch!='\t');

  length = p-start;
  return start;
}

//-------------------------------------------------------------
void ConfigParser::removeTralingCRLF(const char* p) {
  if (p != NULL) {
    int len = strlen(p);
    char* s = (char*)p;
    char ch;
    while (len > 0 && ((ch=s[len-1])=='\n' || ch=='\r') ) s[--len] = '\0';
  }
}

//-------------------------------------------------------------
const char* ConfigParser::getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, size_t* pkey_len) {
  length = 0;
  if (pkey_len != NULL) *pkey_len = 0;
  const char* start = skipBlanksInCommand(p);
  if (start == NULL) return p = NULL;

  char ch;
  if (*start == '"') {
    // quoted string

    int value_len = 0;
    const char* ptr = start;
    while ((ch=*++ptr) !='"') {
      if (ch == '\\') {
        ch = *++ptr;
        if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f') error("Unmatched quote ('\"')");
        switch ( ch) {
        case '\\':
        case '"':
        case '\'':
        case 'n':
        case 't':
        case 'r':
        case 'f':
          break;
        case 'x':
          if (!isHex(*++ptr) || !isHex(*++ptr)) error("Two digits are expected after \\x");
          break;
        case '\0':
        case '\n':
        case '\r':
        case '\f':
          error("Backslash is not allowed at the end of line");
          break;
        default:
          error("Unexpected character '%c' after backslash", ch);
        }
      } else if (ch == '\0' || ch == '\n' || ch == '\r') {
        error("Unmatched quote ('\"')");
      }
      value_len++;
    }

    ch = *++ptr;
    if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '#') {
      p = NULL;
    } else if (ch == ' ' || ch == '\t') {
      p = ptr;
    } else {
      error("Invalid quoted string");
    }

    size_t new_size = sizeof(char)*(value_len+1);
    if (buffer == NULL) {
      buffer = (char*)malloc(new_size);
      bufsize = new_size;
    } else if (bufsize<=new_size) {
      buffer = (char*)realloc(buffer, new_size);
      bufsize = new_size;
    }
    if (buffer == NULL) {
      fprintf(stderr, "ERROR: Out of memory\n");
      exit(1);
    }

    char* out = buffer;
    ptr = start;
    while ((ch=*++ptr) !='"') {
      if (ch == '\\') {
        ch = *++ptr;
        if (ch == '\0' || ch == '\n' || ch == '\r') error("Unmatched quote ('\"')");
        switch ( ch) {
        case '\\':
        case '"':
        case '\'':
          break;
        case 'n':
          ch = '\n';
          break;
        case 't':
          ch = '\t';
          break;
        case 'r':
          ch = '\r';
          break;
        case 'f':
          ch = '\f';
          break;
        case 'x':
          ch = getHex(ptr);
          break;
        }
      }
      *out++ = ch;
    }
    *out++ = '\0';
    length = value_len;
    return buffer;
  }

  // started as unquoted
  bool include_quotes = false;
  bool inside_quotes = false;
  int value_len = 0;
  while ((ch=*p) != '\0' && ch != '\r' && ch != '\n' && ch!='\f') {
    if (!inside_quotes) {
      if (ch == ' ' || ch == '\t' || ch == '#') break;
      if (pkey_len != NULL && ch == '=' && !include_quotes && *pkey_len == 0) {
        *pkey_len = p-start;
        if (*pkey_len == 0) *pkey_len = -1; // missing key but the argument still may be valid
      }
    }
    p++;

    if (ch == '"')  {
      inside_quotes = !inside_quotes;
      include_quotes = true;
    } else {
      if (ch == '\\') {
        if (!inside_quotes) error("Backslash is not allowed in unquoted strings");
        ch = *p++;
        switch ( ch) {
        case '\\':
        case '"':
        case '\'':
        case 'n':
        case 't':
        case 'r':
        case 'f':
          break;
        case 'x':
          if (!isHex(*p++) || !isHex(*p++)) error("Two digits are expected after \\x");
          break;
        case '\0':
        case '\n':
        case '\r':
        case '\f':
          error("Backslash is not allowed at the end of line");
          break;
        default:
          error("Unexpected character '%c' after backslash", ch);
        }
      }
      value_len++;
    }
  }
  if (inside_quotes) error("Unmatched quote ('\"')");
  if (!include_quotes && ch == '\0') {
    length = p-start;
    p = NULL;
    return start;
  }
  size_t new_size = sizeof(char)*(value_len+1);
  if (buffer == NULL) {
    buffer = (char*)malloc(new_size);
    bufsize = new_size;
  } else if (bufsize<=new_size) {
    buffer = (char*)realloc(buffer, new_size);
    bufsize = new_size;
  }
  if (buffer == NULL) {
    fprintf(stderr, "ERROR: Out of memory\n");
    exit(1);
  }
  if (!include_quotes) {
    strncpy(buffer, start, value_len);
    buffer[value_len] = '\0';
  } else {
    inside_quotes = false;
    p = start;
    char* pout = buffer;
    while ((ch=*p++) != '\0' && ch != '\r' && ch != '\n' && ch!='\f') {
      if (!inside_quotes && (ch == ' ' || ch == '\t' || ch == '#')) break;
      if (ch == '"')  {
        inside_quotes = !inside_quotes;
      } else {
        if (ch == '\\') {
          ch = *p++;
          switch ( ch) {
          case '\\':
          case '"':
          case '\'':
            break;
          case 'n':
            ch = '\n';
            break;
          case 't':
            ch = '\t';
            break;
          case 'r':
            ch = '\r';
            break;
          case 'f':
            ch = '\f';
            break;
          case 'x':
            ch = getHex(p);
            break;
          }
        }
        *pout++ = ch;
      }
    }
    *pout = '\0';
    value_len = pout-buffer;
  }

  if (ch=='\0' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '#') p = NULL;
  length = value_len;
  return buffer;
}

//-------------------------------------------------------------
int ConfigParser::getHex(char ch) {
  if (ch>='0' && ch<='9') return (ch-'0');
  if (ch>='a' && ch<='f') return (ch-'a')+10;
  if (ch<'A' || ch>'F') error("Invalid hex character '%c'", ch);
  return (ch-'A')+10;
}

char ConfigParser::getHex(const char*& p) {
  int n = getHex(*p++);
  n <<= 4;
  int n2 = getHex(*p++);
  n |= n2;
  return (char)n;
}


//-------------------------------------------------------------
#define MAX_MESSAGE_INSERTS 16

MessageInsert* compileMessage(const char* messageFormat, ConfigParser* errorLogger) {
  const char* p = messageFormat;

  int index = 0;
  MessageInsert compiled[MAX_MESSAGE_INSERTS+1];

  char ch;
  do {
    const char* start = p;
    while ( (ch=*p) != '\0' && ch != '%' ) p++;
    size_t len = p-start;
    if (len > 0) {
      if (index >= MAX_MESSAGE_INSERTS) errorLogger->error("Too complex message/command format - too many inserts");
      compiled[index].type = MessageInsertType::Constant;
      compiled[index].stringArg = make_str(start, len);
      index++;
    }
    if (ch == '\0') break;

    if (index >= MAX_MESSAGE_INSERTS) errorLogger->error("Too complex message/command format - too many inserts");

    ch = *++p;
    MessageInsertType messageInsertType = MessageInsertType::Constant;
    switch (ch) {
    case '\0':
      errorLogger->error("Invalid message/command format: character is expected after %%");
      break;
    case 'F':
      messageInsertType = MessageInsertType::TemperatureF;
      break;
    case 'C':
      messageInsertType = MessageInsertType::TemperatureC;
      break;
    case 'f':
      messageInsertType = MessageInsertType::TemperaturedF;
      break;
    case 'c':
      messageInsertType = MessageInsertType::TemperaturedC;
      break;
    case 'N':
      messageInsertType = MessageInsertType::SensorName;
      break;
    case 'I':
      messageInsertType = MessageInsertType::ReferenceId;
      break;
    case 'H':
      messageInsertType = MessageInsertType::Humidity;
      break;
    case 'B':
      messageInsertType = MessageInsertType::BatteryStatusInt;
      break;
    case 'b':
      messageInsertType = MessageInsertType::BatteryStatusStr;
      break;
    case '%':
      messageInsertType = MessageInsertType::Percent;
      break;
    default:
      errorLogger->error("Invalid message/command format: invalid substitution \"%%%c\"", ch);
    }
    compiled[index].type = messageInsertType;
    compiled[index].stringArg = NULL;
    index++;

    p++;
  } while(true);

  if (index == 0) errorLogger->error("Empty message/command");

  MessageInsert* result = (MessageInsert*)malloc((index+1)*sizeof(MessageInsert));
  memcpy(result, compiled, index*sizeof(MessageInsert));
  result[index].type = MessageInsertType::Constant;
  result[index].stringArg = NULL;

  return result;
}

//-------------------------------------------------------------
bool convertDecimalArg(const char* str, const char*& p, int& result, int scale, bool allow_negative, char stop_char, const char* argname, ConfigParser* errorLogger) {
  result = 0xffffffff;
  if (skipBlanks(p) == NULL) {
    if (stop_char != '\0') errorInavidArg(str, p, argname, errorLogger);
    return false;
  }

  bool negative = false;
  char ch = *p;
  if (ch=='-') {
    if (!allow_negative) errorInavidArg(str, p, argname, errorLogger);
    negative = true;
    ch = *++p;
  } else if (ch=='+') {
    ch = *++p;
  }
  if (ch<'0' || ch>'9') errorInavidArg(str, p, argname, errorLogger);

  int n = (ch-'0');

  while ((ch=*++p)!='\0' && ch != '.' && ch != stop_char) {
    if (ch<'0' || ch>'9') errorInavidArg(str, p, argname, errorLogger);
    n = n*10 + (ch-'0');
  }

  if ((ch == '.') && (stop_char != '.' || *(p+1) != '.')) {
    if (scale <= 0) errorInavidArg(str, p, argname, errorLogger);
    while ((ch=*++p) != '\0' && ch != stop_char && --scale >= 0) {
      if (ch<'0' || ch>'9') errorInavidArg(str, p, argname, errorLogger);
      n = n*10 + (ch-'0');
    }
  }
  while (scale-- > 0) n = n*10;

  if (ch == stop_char) {
    if (stop_char == '.') {
      if (*++p != '.') errorInavidArg(str, p, argname, errorLogger);
      p++;
    }
  } else {
    if (ch == '\0' && stop_char != '[') errorInavidArg(str, p, argname, errorLogger);
  }

  if (negative) n = -n;
  result = n;
  return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
bool convertDecimalArg(const char* str, int& result, int scale, bool allow_negative, const char* argname, ConfigParser* errorLogger) { // @suppress("Unused static function")
  const char* p = str;
  return convertDecimalArg(str, p, result, scale, allow_negative, '\0', argname, errorLogger);
}
#pragma GCC diagnostic pop

//-------------------------------------------------------------
uint32_t getDayTimeOffset(const char* arg, const char*& p, const char* argname, ConfigParser* errorLogger) {
  if (*p != '[') errorInavidArg(arg, p, argname, errorLogger);
  p++;
  int hour, min;
  convertDecimalArg(arg, p, hour, 0, false, ':', argname, errorLogger);
  if (hour > 24) errorInavidArg(arg, p, argname, errorLogger);
  if (*p != ':') errorInavidArg(arg, p, argname, errorLogger);
  p++;
  convertDecimalArg(arg, p, min, 0, false, ']', argname, errorLogger);
  if (hour==24 ? min != 0 : min > 60) errorInavidArg(arg, p, argname, errorLogger);
  if (*p != ']') errorInavidArg(arg, p, argname, errorLogger);
  p++;
  return (uint32_t)hour*60+min;
}

//-------------------------------------------------------------
// compile bounds=72.5..74.5[22:00]72..75[8:00]
AbstractRuleBoundSchedule* compileBoundSchedule(const char* arg, int scale, bool allow_negative, const char* argname, ConfigParser* errorLogger) {
  const char* p = arg;

  int low, high;
  convertDecimalArg(arg, p, low, scale, allow_negative, '.', argname, errorLogger);
  convertDecimalArg(arg, p, high, scale, allow_negative, '[', argname, errorLogger);
  if (high <= low) errorInavidArg(arg, p, argname, errorLogger);

  if (p == NULL || *p == '\0') return new RuleBoundFixed(low, high);

  uint32_t first_time_offset = getDayTimeOffset(arg, p, argname, errorLogger);
  RuleBoundsSheduleItem* scheduleItem = (RuleBoundsSheduleItem*)malloc(sizeof(RuleBoundsSheduleItem));
  scheduleItem->bounds = RuleBounds::make(low, high);
  scheduleItem->time_offset = first_time_offset;
  scheduleItem->prev = scheduleItem;
  scheduleItem->next = scheduleItem;
  RuleBoundsSheduleItem* firstScheduleItem = scheduleItem;
  RuleBoundsSheduleItem** pNextItem = &scheduleItem->next;

  bool flipped = false;
  uint32_t prev_time_offset = first_time_offset;
  do {
    convertDecimalArg(arg, p, low, scale, allow_negative, '.', argname, errorLogger);
    convertDecimalArg(arg, p, high, scale, allow_negative, '[', argname, errorLogger);
    if (high <= low) errorInavidArg(arg, p, argname, errorLogger);

    uint32_t time_offset = getDayTimeOffset(arg, p, argname, errorLogger);

    if (time_offset == prev_time_offset) errorInavidArg(arg, p, argname, errorLogger);
    if (time_offset < prev_time_offset) {
      if (flipped) errorInavidArg(arg, p, argname, errorLogger);
      flipped = true;
    }
    prev_time_offset = time_offset;

    RuleBoundsSheduleItem* scheduleItem = (RuleBoundsSheduleItem*)malloc(sizeof(RuleBoundsSheduleItem));
    scheduleItem->bounds = RuleBounds::make(low, high);
    scheduleItem->time_offset = time_offset;
    scheduleItem->next = firstScheduleItem;
    scheduleItem->prev = firstScheduleItem->prev;
    firstScheduleItem->prev = scheduleItem;
    *pNextItem = scheduleItem;
    pNextItem = &scheduleItem->next;

  } while (*p != '\0');

  return new RuleBoundSchedule(firstScheduleItem);
}

//-------------------------------------------------------------
void errorMissingArg(const char* command, const char* argname, ConfigParser* errorLogger) {
  errorLogger->error("Missing argument \"%s\" in the definition of command \"%s\"", argname, command);
}
void errorInavidArg(const char* str, const char* argname, ConfigParser* errorLogger) {
  errorLogger->error("Invalid value \"%s\" of argument \"%s\"", str, argname);
}
void errorInavidArg(const char* str, const char* p, const char* argname, ConfigParser* errorLogger) {
  int pos = p == NULL ? -1 : p-str;
  if (pos >= 0 && pos < (int)strlen(str))
    errorLogger->error("Error near index %d in value \"%s\" of argument \"%s\"", pos, str, argname);
  errorLogger->error("Invalid value \"%s\" of argument \"%s\"", str, argname);
}

void errorInavidValueOfArg(const char* arg_name, const char* value, ConfigParser* parser) {
  if (parser != NULL) errorInavidArg(value, "arg_name", parser);
  fprintf(stderr, "ERROR: Invalid value of argument --%s=\"%s\".\n", arg_name, value);
  exit(1);
}
