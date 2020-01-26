/*
 * Logger.hpp
 *
 *  Created on: Apr 17, 2017
 *      Author: Alex Konshin
 */

#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 2048

#define LOGGER_FLAG_STDERR   1
#define LOGGER_FLAG_TIME     2
#define LOGGER_FLAG_TIME_UTC 4

#define Log (Logger::defaultLogger)

class Logger {
public:
  static Logger* defaultLogger;

  Logger() {
    flags = LOGGER_FLAG_STDERR | LOGGER_FLAG_TIME;
    logFile = NULL;
  }

  Logger(FILE* logFile) {
    flags = LOGGER_FLAG_TIME;
    this->logFile = logFile;
  }

  ~Logger() {
    if (logFile != NULL) fclose(logFile);
    if ((flags&LOGGER_FLAG_STDERR) != 0) fflush(stderr);
  }

  void flush() {
    if (logFile != NULL) fflush(logFile);
    if ((flags&LOGGER_FLAG_STDERR) != 0) fflush(stderr);
  }

  void setLogFile(FILE* logFile) {
    this->logFile = logFile;
  }

  void setFlags(unsigned flags) {
    this->flags |= flags;
  }
  void resetFlags(unsigned flags) {
    this->flags &= ~flags;
  }

  void info_vargs(const char* fmt, va_list vargs) {
    if ((flags&LOGGER_FLAG_STDERR) != 0 || logFile != NULL) {
      format_message(NULL, fmt, vargs);
      if ((flags&LOGGER_FLAG_STDERR) != 0) output(stderr);
      if (logFile != NULL) {
        output(logFile);
        fflush(logFile);
      }
    }
  }

  void info(const char* fmt, ...) {
    if ((flags&LOGGER_FLAG_STDERR) != 0 || logFile != NULL) {
      va_list vargs;
      va_start(vargs, fmt);
      info_vargs(fmt, vargs);
      va_end(vargs);
    }
  }

  void log_vargs(const char* fmt, va_list vargs) {
    if (logFile != NULL) {
      format_message(NULL, fmt, vargs);
      if (logFile != NULL) {
        output(logFile);
        fflush(logFile);
      }
    }
  }

  void log(const char* fmt, ...) {
    if (logFile != NULL) {
      va_list vargs;
      va_start(vargs, fmt);
      log_vargs(fmt, vargs);
      va_end(vargs);
    }
  }

  void warning_vargs(const char* fmt, va_list vargs) {
    if ((flags&LOGGER_FLAG_STDERR) != 0 || logFile != NULL) {
      format_message("WARNING: ", fmt, vargs);
      if ((flags&LOGGER_FLAG_STDERR) != 0) output(stderr);
      if (logFile != NULL) {
        output(logFile);
        fflush(logFile);
      }
    }
  }

  void warning(const char* fmt, ...) {
    if ((flags&LOGGER_FLAG_STDERR) != 0 || logFile != NULL) {
      va_list vargs;
      va_start(vargs, fmt);
      warning_vargs(fmt, vargs);
      va_end(vargs);
    }
  }

  void error_vargs(const char* fmt, va_list vargs) {
    if ((flags&LOGGER_FLAG_STDERR) != 0 || logFile != NULL) {
      format_message("ERROR: ", fmt, vargs);
      if ((flags&LOGGER_FLAG_STDERR) != 0) output(stderr);
      if (logFile != NULL) {
        output(logFile);
        fflush(logFile);
      }
    }
  }

  void error(const char* fmt, ...) {
    if ((flags&LOGGER_FLAG_STDERR) != 0 || logFile != NULL) {
      va_list vargs;
      va_start(vargs, fmt);
      error_vargs(fmt, vargs);
      va_end(vargs);
    }
  }

private:

  FILE* logFile;
  unsigned flags;
  char buffer[BUFFER_SIZE];

  void output(FILE* log) {
    int len = strlen(buffer);
    if (len != 0) {
      fputs(buffer, log);
      if (buffer[len-1] != '\n') fputc('\n', log);
    } else {
      fputc('\n', log);
    }
  }

  void format_message(const char* prefix, const char* fmt, va_list vargs) {
    buffer[0] = '\0';
    if (fmt != NULL) {
      if ((flags&LOGGER_FLAG_TIME) != 0) add_time();

      if (prefix != NULL) strcat(buffer, prefix);
      int len = strlen(buffer);

      vsnprintf(buffer+len, BUFFER_SIZE-len, fmt, vargs);
    }
  }

  void add_time() {
    struct tm tm;
    time_t data_time = time(NULL);

    if ((flags&LOGGER_FLAG_TIME_UTC) != 0) {
      tm = *gmtime(&data_time); // convert time_t to struct tm
      strftime(buffer, BUFFER_SIZE, "%FT%TZ ", &tm); // ISO 8601 format
    } else {
      tm = *localtime(&data_time); // convert time_t to struct tm
      strftime(buffer, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S %Z ", &tm);
    }
  }

};

#endif /* LOGGER_HPP_ */
