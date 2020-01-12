/*
 * Config.hpp
 *
 * Created on: January 11, 2020
 * @author Alex Konshin <akonshin@gmail.com>
 */

#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>

#ifdef BPiM3
#include "mach/bpi-m3.h"
#endif

#ifdef ODROIDC2
#include "mach/odroid-c2.h"
#endif

#ifdef RPI
#include "mach/rpi3.h"
#endif

#ifdef __x86_64__
#include "mach/x86_64.h"
#endif

#ifndef MAX_GPIO
#define MAX_GPIO 53
#endif
#ifndef DEFAULT_PIN
#define DEFAULT_PIN 27
#endif
#define to_str(x) #x
#ifndef TEST_DECODING
#define DEFAULT_PIN_STR to_str(DEFAULT_PIN)
#endif


enum ServerType {STDOUT, REST, InfluxDB};


static const struct option long_options[] = {
    { "gpio", required_argument, NULL, 'g' },
    { "send-to", required_argument, NULL, 's' },
    { "server-type", required_argument, NULL, 't' },
    { "celsius", no_argument, NULL, 'C' },
    { "utc", no_argument, NULL, 'U' },
    { "local-time", no_argument, NULL, 'L' },
    { "all", no_argument, NULL, 'A' },
    { "log-file", required_argument, NULL, 'l' },
    { "verbose", no_argument, NULL, 'v' },
    { "more_verbose", no_argument, NULL, 'V' },
    { "statistics", no_argument, NULL, 'T' },
    { "debug", no_argument, NULL, 'd' },
    { "any", no_argument, NULL, '0' },
    { "f007th", no_argument, NULL, '7' },
    { "00592txr", no_argument, NULL, '5' },
    { "tx6", no_argument, NULL, '6' },
    { "hg02832", no_argument, NULL, '8' },
    { "wh2", no_argument, NULL, '2' },
    { "DEBUG", no_argument, NULL, 'D' },
#ifdef TEST_DECODING
    { "input-log", required_argument, NULL, 'I' },
    { "wait", no_argument, NULL, 'W' },
#endif
#ifdef INCLUDE_HTTPD
    { "httpd", required_argument, NULL, 'H' },
#endif
    { "max_gap", required_argument, NULL, 'G' },
    { NULL, 0, NULL, 0 }
};

#ifdef TEST_DECODING
  const int DEFAULT_OPTIONS = VERBOSITY_INFO|VERBOSITY_PRINT_UNDECODED|VERBOSITY_PRINT_DETAILS;
#ifdef INCLUDE_HTTPD
  const char* short_options = "c:g:s:Al:vVt:TCULd256780DI:WH:G:";
#else
  const char* short_options = "c:g:s:Al:vVt:TCULd256780DI:WG:";
#endif
#elif defined(INCLUDE_HTTPD)
  const int DEFAULT_OPTIONS = 0;
  const char* short_options = "c:g:s:Al:vVt:TCULd256780DH:G:";
#else
  const int DEFAULT_OPTIONS = 0;
  const char* short_options = "c:g:s:Al:vVt:TCULd256780DG:";
#endif


class Config {
private:
  bool tz_set = false;

public:
  const char* log_file_path = NULL;
  const char* server_url = NULL;
  int gpio = DEFAULT_PIN;
  ServerType server_type = REST;
  unsigned protocols = 0;
  time_t max_unchanged_gap = 0L;

  bool changes_only = true;
  bool type_is_set = false;

  int options = DEFAULT_OPTIONS;
#ifdef INCLUDE_HTTPD
  int httpd_port = 0;
#endif
#ifdef TEST_DECODING
  bool wait_after_reading = false;
  const char* input_log_file_path = NULL;
#endif

public:
  Config() {

  }
  Config(int options) {
    this->options = options;
  }
  ~Config() {}


  static void help() {
    fputs(
#ifdef TEST_DECODING
      "(c) 2017-2020 Alex Konshin\n"
      "Test decoding of received data for f007th* utilities.\n"
#else
      "(c) 2017-2020 Alex Konshin\n"
      "Receive data from thermometers then print it to stdout or send it to remote server via REST API.\n"
#endif
      "Version " RF_RECEIVER_VERSION "\n\n"
#ifndef TEST_DECODING
      "--gpio, -g\n"
      "    Value is GPIO pin number (default is " DEFAULT_PIN_STR ") as defined on page http://abyz.co.uk/rpi/pigpio/index.html\n"
#endif
      "--celsius, -C\n"
      "    Output temperature in degrees Celsius.\n"
      "--utc, -U\n"
      "    Timestamps are printed/sent in ISO 8601 format.\n"
      "--local-time, -L\n"
      "    Timestamps are printed/sent in format \"YYYY-mm-dd HH:MM:SS TZ\".\n"
      "--send-to, -s\n"
      "    Parameter value is server URL.\n"
      "--server-type, -t\n"
      "    Parameter value is server type. Possible values are REST (default) or InfluxDB.\n"
      "--all, -A\n"
      "    Send all data. Only changed and valid data is sent by default.\n"
      "--log-file, -l\n"
      "    Parameter is a path to log file.\n"
#ifdef TEST_DECODING
      "--input-log, -I\n"
      "    Parameter is a path to input log file to be processed.\n"
#endif
#ifdef INCLUDE_HTTPD
      "--httpd, -H\n"
      "    Run HTTPD server on the specified port.\n"
#endif
      "--max_gap, -G\n"
      "    Max gap between reported reading. Next reading will be sent to server or printed even it is the same as previous readings.\n"
      "--verbose, -v\n"
      "    Verbose output.\n"
      "--more_verbose, -V\n"
      "    More verbose output.\n",
      stderr
    );
    fflush(stderr);
    fclose(stderr);
    exit(1);
  }

  bool process_cmdline_option( int c, const char* option, const char* optarg ) {
    long long_value;

    switch (c) {

    case 'c':
      readConfig(optarg);
      break;

    case 'g':
      long_value = strtol(optarg, NULL, 10);
      if (long_value<=0 || long_value>MAX_GPIO) {
        fprintf(stderr, "ERROR: Invalid GPIO pin number \"%s\".\n", optarg);
        help();
      }
      gpio = (int)long_value;
      break;

    case 'A':
      changes_only = false;
      break;

    case 'v':
      options |= VERBOSITY_INFO;
      break;

    case 'l':
      log_file_path = optarg;
      break;

  #ifdef TEST_DECODING
    case 'I':
      input_log_file_path = optarg;
      break;

    case 'W':
      wait_after_reading = true;
      break;
  #endif

  #ifdef INCLUDE_HTTPD
    case 'H':
      long_value = strtol(optarg, NULL, 10);

      if (long_value<MIN_HTTPD_PORT || long_value>65535) {
        fprintf(stderr, "ERROR: Invalid HTTPD port number \"%s\".\n", optarg);
        help();
      }
      httpd_port = (int)long_value;
      break;
  #endif

    case 's':
      server_url = optarg;
      break;

    case 't':
      if (strcasecmp(optarg, "REST") == 0) {
        if (type_is_set && server_type!=REST) {
          fputs("ERROR: Server type is specified twice.\n", stderr);
          help();
        }
        server_type = REST;
      } else if (strcasecmp(optarg, "InfluxDB") == 0) {
        if (type_is_set && server_type!=InfluxDB) {
          fputs("ERROR: Server type is specified twice.\n", stderr);
          help();
        }
        server_type = InfluxDB;
      } else {
        fprintf(stderr, "ERROR: Unknown server type \"%s\".\n", optarg);
        help();
      }
      type_is_set = true;
      break;

    case 'T':
      options |= VERBOSITY_PRINT_STATISTICS;
      break;

    case 'd':
      options |= VERBOSITY_DEBUG;
      break;

    case 'C':
      options |= OPTION_CELSIUS;
      break;

    case 'U':
      if (tz_set && (options&OPTION_UTC)==0) {
        fputs("ERROR: Options -L/--local_time and -U/--utc are mutually exclusive\n", stderr);
        help();
      }
      options |= OPTION_UTC;
      tz_set = true;
      break;

    case 'L':
      if (tz_set && (options&OPTION_UTC)!=0) {
        fputs("ERROR: Options -L/--local_time and -U/--utc are mutually exclusive\n", stderr);
        help();
      }
      tz_set = true;
      break;

    case 'V':
      options |= VERBOSITY_INFO | VERBOSITY_PRINT_JSON | VERBOSITY_PRINT_CURL | VERBOSITY_PRINT_UNDECODED | VERBOSITY_PRINT_DETAILS;
      break;

    case '2': // WH2
      protocols |= PROTOCOL_WH2;
      break;
    case '5': // AcuRite 00592TXR
      protocols |= PROTOCOL_00592TXR;
      break;
    case '6': // LaCrosse TX3/TX6/TX7
      protocols |= PROTOCOL_TX7U;
      break;
    case '7': // Ambient Weather F007TH
      protocols |= PROTOCOL_F007TH;
      break;
    case '8': // Auriol HG02832
      protocols |= PROTOCOL_HG02832;
      break;
    case '0': // any protocol
      protocols |= PROTOCOL_ALL;
      break;
    case 'D': // print undecoded messages
      options |= VERBOSITY_PRINT_UNDECODED;
      changes_only = false;
      break;

    case 'G':
      long_value = strtol(optarg, NULL, 10);
      if (long_value<=0 || long_value>1000) {
        fprintf(stderr, "ERROR: Invalid value of argument --max_gap \"%s\".\n", optarg);
        help();
      }
      max_unchanged_gap = long_value*60;
      break;

    case '?':
      help();
      break;

    default:
      //fprintf(stderr, "ERROR: Unknown option \"%s\".\n", option]);
      //help();
      return false;
    }
    return true;
  }

private:

  static int16_t readInt(const char*& p) {
    int16_t n = 0;

    char ch;
    while ((ch=*p)==' ') p++;

    if (ch=='\0' || ch=='\n' || ch=='\r') return -1;

    do {
      n = n*10+(ch-'0');
      ch = *++p;
    } while (ch>='0' && ch<='9');

    if (ch==',') p++;
    return n;
  }


  void readConfig(const char* configFilePath) {
    FILE* configFileStream;

    configFileStream = fopen(configFilePath, "r");
    if (configFileStream == NULL) {
      fprintf(stderr, "Cannot open configuration file \"%s\".", configFilePath);
      exit(1);
    }

    fprintf(stderr, "Reading configuration file \"%s\"...\n", configFilePath);

    int linenum = 0;

    char* optarg_buffer = NULL;
    size_t optarg_bufsize = 0;

    char* line = NULL;
    size_t bufsize = 0;

    ssize_t bytesread;
    while ((bytesread = getline(&line, &bufsize, configFileStream)) != -1) {
      linenum++;
      size_t opt_len = 0;
      const char* p = line;
      const char* opt = getWord(p, opt_len);
      if (opt == NULL) continue;

      const struct option *struct_opt;
      for (struct_opt = long_options; struct_opt->name != NULL; struct_opt++) {
        size_t struct_opt_len = strlen(struct_opt->name);
        if (opt_len == struct_opt_len && strncmp(struct_opt->name, opt, opt_len) == 0) break;
      }
      if ( struct_opt->name != NULL ) { // found command line option

        const char* optarg = NULL;
        if (struct_opt->has_arg == no_argument) {
          if (skipBlanks(p) != NULL) {
            fprintf(stderr, "ERROR: Unexpected argument of option \"%s\" in line #%d of file \"%s\"\n", struct_opt->name, linenum, configFilePath);
            help();
          }
        } else {
          size_t optarg_len = 0;
          optarg = getString(p, optarg_buffer, optarg_bufsize, optarg_len, linenum, configFilePath);
          if (optarg == NULL && struct_opt->has_arg == required_argument) {
            fprintf(stderr, "ERROR: Missing argument of option \"%s\" in line #%d of file \"%s\"\n", struct_opt->name, linenum, configFilePath);
            exit(1);
          }
        }

        //fprintf(stderr, ">>> option \"%s\": val='%c' optarg=%s\n", struct_opt->name, struct_opt->val, optarg );

        if (!process_cmdline_option(struct_opt->val, struct_opt->name, optarg)) {
          fprintf(stderr, "ERROR: Unexpected error when processing option \"%s\"('%c') in line #%d of file \"%s\"\n", struct_opt->name, struct_opt->val, linenum, configFilePath);
          exit(1);
        }
        continue;
      }

      // unknown command-line option

      //TODO implement commands

      fprintf(stderr, "ERROR: Unrecognized option in line #%d of file \"%s\".\n", linenum, configFilePath);
      help();
    }

    if (line != NULL) free(line);
    if (optarg_buffer != NULL) free(optarg_buffer);
    fclose(configFileStream);
  }

  const char* skipBlanks(const char*& p) {
    if (p == NULL) return NULL;
    char ch;
    while ((ch=*p) ==' ' || ch == '\t') p++;
    if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') p = NULL;
    return p;
  }

  const char* getWord(const char*& p, size_t& length) {
    length = 0;
    const char* start = skipBlanks(p);
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

  const char* getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, int linenum, const char* configFilePath) {
    length = 0;
    const char* start = skipBlanks(p);
    if (start == NULL) return p = NULL;

    char ch;
    if (*start == '"') {
      // quoted string

      int value_len = 0;
      const char* ptr = start;
      while ((ch=*++ptr) !='"') {
        if (ch == '\\') {
          ch = *++ptr;
          if (ch == '\0' || ch == '\n' || ch == '\r') {
            fprintf(stderr, "ERROR: Unmatched quote ('\"') in line #%d of file \"%s\".\n", linenum, configFilePath);
            exit(1);
          }
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
            if (!isHex(*++ptr) || !isHex(*++ptr)) {
              fprintf(stderr, "ERROR: Two digits are expected after \\x in line #%d of file \"%s\".\n", linenum, configFilePath);
              exit(1);
            }
            break;
          default:
            fprintf(stderr, "ERROR: Unexpected character '%c' after backslash in line #%d of file \"%s\".\n", ch, linenum, configFilePath);
            exit(1);
          }
        } else if (ch == '\0' || ch == '\n' || ch == '\r') {
          fprintf(stderr, "ERROR: Unmatched quote ('\"') in line #%d of file \"%s\".\n", linenum, configFilePath);
          exit(1);
        }
        value_len++;
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
          if (ch == '\0' || ch == '\n' || ch == '\r') {
            fprintf(stderr, "ERROR: Unmatched quote ('\"') in line #%d of file \"%s\".\n", linenum, configFilePath);
            exit(1);
          }
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
            ch = getHex(ptr, linenum, configFilePath);
            break;
          }
        }
        *out++ = ch;
      }
      *out++ = '\0';
      length = value_len;
      return buffer;
    }

    while ((ch=*p) !=' ' && ch != '\t') {
      if (ch == '\0' || ch == '\n' || ch == '\r') {
        length = p-start;
        p = NULL;
        return start;
      }
      p++;
    }
    int value_len = p-start;
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
    strncpy(buffer, start, value_len);

    length = value_len;
    return buffer;
  }

  static inline bool isHex(char ch) {
    return (ch>='0' && ch<='9') || (ch>='a' && ch<='f') || (ch>='A' && ch<='F');
  }

  static int getHex(char ch, int linenum, const char* configFilePath) {
    if (ch>='0' && ch<='9') return (ch-'0');
    if (ch>='a' && ch<='f') return (ch-'a')+10;
    if (ch<'A' || ch>'F') {
      fprintf(stderr, "ERROR: Invalid hex character '%c' in line #%d of file \"%s\".\n", ch, linenum, configFilePath);
      exit(1);
    }
    return (ch-'A')+10;
  }

  static char getHex(const char*& p, int linenum, const char* configFilePath) {
    int n = getHex(*p++, linenum, configFilePath);
    n <<= 4;
    n |= getHex(*p++, linenum, configFilePath);
    return (char)n;
  }

};
#endif /* CONFIG_HPP_ */
