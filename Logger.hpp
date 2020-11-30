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

#define BUFFER_SIZE 4096

#define LOGGER_FLAG_STDERR   1
#define LOGGER_FLAG_TIME     2
#define LOGGER_FLAG_TIME_UTC 4

#define Log (Logger::defaultLogger)

#ifndef ASSERT
#ifdef  NDEBUG
#define ASSERT(expr)     ((void)0)
#define DBG(format, arg...)     ((void)0)
#else
#define PRINT_ASSERT(expr,file,line)  fputs("ASSERTION: ("#expr") in "#file" : " #line "\n", stderr)
#define ASSERT(expr)  if(!(expr)) { PRINT_ASSERT(expr,__FILE__,__LINE__); /*assert(expr);*/ }
#define DBG(format, arg...)  Log->info(format, ## arg)
#endif // NDEBUG
#endif //ASSERT


class ErrorLogger {
protected:

  unsigned flags = 0;
  char buffer[BUFFER_SIZE];

public:
  ErrorLogger() {
    flags = 0;
  }
  ErrorLogger(unsigned flags) {
    this->flags = flags;
  }

  virtual ~ErrorLogger() {}

  virtual void error_vargs(const char* fmt, va_list vargs) {
    format_message("ERROR: ", fmt, vargs);
    output(stderr);
  }

  virtual void error(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    error_vargs(fmt, vargs);
    va_end(vargs);
  }

  void setFlags(unsigned flags) {
    this->flags |= flags;
  }
  void resetFlags(unsigned flags) {
    this->flags &= ~flags;
  }

protected:
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

  void format_message(const char* prefix, const char* fmt, va_list vargs) {
    buffer[0] = '\0';
    if (fmt != NULL) {
      if ((flags&LOGGER_FLAG_TIME) != 0) add_time();

      if (prefix != NULL) strcat(buffer, prefix);
      int len = strlen(buffer);

      vsnprintf(buffer+len, BUFFER_SIZE-len, fmt, vargs);
    }
  }

  void output(FILE* log) {
    int len = strlen(buffer);
    if (len != 0) {
      fputs(buffer, log);
      if (buffer[len-1] != '\n') fputc('\n', log);
    } else {
      fputc('\n', log);
    }
  }

};


class Logger : public ErrorLogger {
private:

  FILE* logFile;

public:
  static Logger* defaultLogger;

  Logger() : ErrorLogger(LOGGER_FLAG_STDERR | LOGGER_FLAG_TIME) {
    logFile = NULL;
  }

  Logger(FILE* logFile) : ErrorLogger(LOGGER_FLAG_TIME) {
    flags = LOGGER_FLAG_TIME;
    this->logFile = logFile;
  }

  virtual ~Logger() {
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

};

#endif /* LOGGER_HPP_ */
