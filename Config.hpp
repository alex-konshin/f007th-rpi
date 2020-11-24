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
#include <vector>

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

#define is_cmd(cmd,opt,opt_len) (opt_len == (sizeof(cmd)-1) && strncmp(cmd, opt, opt_len) == 0)

static const char* EMPTY_STRING = "";

enum class ServerType : int {NONE, STDOUT, REST, InfluxDB};

// Command line options
static const struct option long_options[] = {
    { "config", required_argument, NULL, 'c' },
    { "gpio", required_argument, NULL, 'g' },
    { "send-to", required_argument, NULL, 's' },
    { "server-type", required_argument, NULL, 't' },
    { "no-server", no_argument, NULL, 'n' },
    { "stdout", no_argument, NULL, 'o' },
    { "celsius", no_argument, NULL, 'C' },
    { "utc", no_argument, NULL, 'U' },
    { "local-time", no_argument, NULL, 'L' },
    { "all-changes", no_argument, NULL, 'A' },
    { "log-file", required_argument, NULL, 'l' },
    { "verbose", no_argument, NULL, 'v' },
    { "more_verbose", no_argument, NULL, 'V' },
    { "statistics", no_argument, NULL, 'T' },
    { "debug", no_argument, NULL, 'd' },
    { "any-protocol", no_argument, NULL, '0' },
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
    { "max-gap", required_argument, NULL, 'G' },
    { "auth-header", required_argument, NULL, 'a'},
    { NULL, 0, NULL, 0 }
};

#ifdef TEST_DECODING
static const int DEFAULT_OPTIONS = VERBOSITY_INFO|VERBOSITY_PRINT_UNDECODED|VERBOSITY_PRINT_DETAILS;
#ifdef INCLUDE_HTTPD
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DI:WH:G:a:no";
#else
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DI:WG:a:no";
#endif
#elif defined(INCLUDE_HTTPD)
static const int DEFAULT_OPTIONS = 0;
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DH:G:a:no";
#else
static const int DEFAULT_OPTIONS = 0;
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DG:a:no";
#endif


struct ProtocolDef {
  const char *name;
  uint8_t protocol;
  uint8_t variant;
  uint8_t rolling_code_size;
  uint8_t number_of_channels;
  uint8_t channels_numbering_type; // 0 => numbers, 1 => letters
};

static const struct ProtocolDef protocol_defs[] = {
  {"f007th",   PROTOCOL_F007TH,   0, 8, 8, 0},
  {"f007tp",   PROTOCOL_F007TH,   1, 8, 8, 0},
  {"00592txr", PROTOCOL_00592TXR, 0, 8, 3, 1},
  {"hg02832",  PROTOCOL_HG02832,  0, 8, 3, 0},
  {"tx6",      PROTOCOL_TX7U,     0, 7, 0, 0},
  {"wh2",      PROTOCOL_WH2,   0x40, 8, 0, 0},
  {"ft007th",  PROTOCOL_WH2,   0x41, 8, 0, 0},
  {NULL,       0,                 0, 0, 0, 0}
};

#define arg_required 1

struct CmdArgDef {
  const char* name;
  uint8_t flags; // arg_required
};

class Config;
typedef void (Config::*CommanExecutor)(const char** argv, int linenum, const char* configFilePath);
#define execute_command(cmd_def,argv,linenum,configFilePath) (this->*((cmd_def)->command_executor))(argv,linenum,configFilePath)

struct CommandDef {
  struct CommandDef* next;
  const char* name;
  int max_num_of_unnamed_args;
  int max_args;
  const struct CmdArgDef* args;
  CommanExecutor command_executor;
};

#define command_def(name, max_num_of_unnamed_args) \
  static const int command_ ## name ## _max_num_of_unnamed_args = max_num_of_unnamed_args; \
  static const struct CmdArgDef COMMAND_ ## name ## _ARGS[]

#define add_command_def(name) add_command_def_(#name, command_ ## name ## _max_num_of_unnamed_args, sizeof(COMMAND_ ## name ## _ARGS)/sizeof(struct CmdArgDef), COMMAND_ ## name ## _ARGS, &Config::command_ ## name)


command_def(sensor, 4) = {
#define CMD_SENSOR_PROTOCOL 0
  { "protocol", arg_required },
#define CMD_SENSOR_CHANNEL 1
  { "ch", 0 },
#define CMD_SENSOR_RC 2
  { "rc", arg_required },
#define CMD_SENSOR_NAME 3
  { "name", arg_required },
};

#ifdef INCLUDE_MQTT

command_def(mqtt_broker, 3) = {
#define CMD_MQTT_BROKER_HOST 0
  { "host", 0 },
#define CMD_MQTT_BROKER_PORT 1
  { "port", 0 },
#define CMD_MQTT_CLIENT_ID 2
  { "client_id", 0 },
#define CMD_MQTT_USER 3
  { "user", 0 },
#define CMD_MQTT_PASSWORD 4
  { "password", 0 },
#define CMD_MQTT_KEEPALIVE 5
  { "keepalive", 0 },
//  { "protocol", 0 },
//  { "certificate", 0 },
//  { "tls_insecure", 0 },
//  { "tls_version", 0 },
};


// mqtt_rule id=temp_room1 sensor="Room 1" metric=F topic="/temperature/room1" msg="%F"
// mqtt_rule id=cool_high sensor="Room 1" metric=F hi=75 topic=/hvac/cooling msg=on  unlock=cool_low
// mqtt_rule id=cool_low  sensor="Room 1" metric=F lo=72 topic=/hvac/cooling msg=off unlock=cool_high

command_def(mqtt_rule, 3) = {
#define CMD_MQTT_RULE_SENSOR  0
  { "sensor", arg_required },
#define CMD_MQTT_RULE_TOPIC   1
  { "topic", arg_required },
#define CMD_MQTT_RULE_MESSAGE 2
  { "msg", 0 },
#define CMD_MQTT_RULE_ID      3
  { "id", 0 },
#define CMD_MQTT_RULE_METRIC  4
  { "metric", 0 },
#define CMD_MQTT_RULE_LO      5
  { "lo", 0 },
#define CMD_MQTT_RULE_HI      6
  { "hi", 0 },
#define CMD_MQTT_RULE_LOCK    7
  { "lock", 0 },
#define CMD_MQTT_RULE_UNLOCK  8
  { "unlock", 0 },
};


//mqtt_bounds_rule id=cool sensor="Room 1" metric=F topic=hvac/cooling msg_hi=on msg_lo=off bounds=72.5..74.5[22:00]72..75[8:00]

command_def(mqtt_bounds_rule, 3) = {
#define CMD_MQTT_BOUNDS_RULE_SENSOR    0
  { "sensor", arg_required },
#define CMD_MQTT_BOUNDS_RULE_TOPIC     1
  { "topic", arg_required },
#define CMD_MQTT_BOUNDS_RULE_ID        2
  { "id", 0 },
#define CMD_MQTT_BOUNDS_RULE_METRIC    3
  { "metric", 0 },
#define CMD_MQTT_BOUNDS_RULE_MSG_LO    4
  { "msg_lo", 0 },
#define CMD_MQTT_BOUNDS_RULE_MSG_HI    5
  { "msg_hi", 0 },
#define CMD_MQTT_BOUNDS_RULE_MSG_IN    6
  { "msg_in", 0 },
#define CMD_MQTT_RULE_BOUNDS_BOUNDS    7
  { "bounds", arg_required },
#define CMD_MQTT_RULE_BOUNDS_LOCK_LO   8
  { "lock_lo", 0 },
#define CMD_MQTT_RULE_BOUNDS_UNLOCK_LO 9
  { "unlock_lo", 0 },
#define CMD_MQTT_RULE_BOUNDS_LOCK_HI   10
  { "lock_hi", 0 },
#define CMD_MQTT_RULE_BOUNDS_UNLOCK_HI 11
  { "unlock_hi", 0 },
#define CMD_MQTT_RULE_BOUNDS_LOCK_IN   12
  { "lock_in", 0 },
#define CMD_MQTT_RULE_BOUNDS_UNLOCK_IN 13
  { "unlock_in", 0 },
};


typedef struct UresolvedReferenceToMqttRule {
  struct UresolvedReferenceToMqttRule* next;
  MqttRule** rule_ptr;
  const char* id;
  int linenum;
  const char* configFilePath;
} UresolvedReferenceToMqttRule;

#endif


//-------------------------------------------------------------

class Config {
private:
  struct CommandDef* command_defs = NULL;

  void init_command_defs() {
    add_command_def(sensor);
#ifdef INCLUDE_MQTT
    add_command_def(mqtt_broker);
    add_command_def(mqtt_rule);
    add_command_def(mqtt_bounds_rule);
#endif
  }

  void add_command_def_(const char* name, int max_num_of_unnamed_args, int max_args, const struct CmdArgDef* args, CommanExecutor command_executor) {
    CommandDef* def = new CommandDef;
    def->name = name;
    def->max_num_of_unnamed_args = max_num_of_unnamed_args;
    def->max_args = max_args;
    def->args = args;
    def->command_executor = command_executor;
    def->next = command_defs;
    command_defs = def;
  }

  bool tz_set = false;

public:
  const char* log_file_path = NULL;
  const char* server_url = NULL;
  int gpio = DEFAULT_PIN;
  ServerType server_type = ServerType::NONE;
  unsigned protocols = 0;
  time_t max_unchanged_gap = 0L;
  const char* auth_header = NULL;

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
#ifdef INCLUDE_MQTT
  bool mqtt_enable = false;
  const char* mqtt_broker_host = NULL;
  uint16_t mqtt_broker_port = 1833;
  const char* mqtt_client_id = NULL;
  const char* mqtt_username = NULL;
  const char* mqtt_password = NULL;
  uint16_t mqtt_keepalive = 60;

  std::vector<MqttRule*> mqtt_rules;
  UresolvedReferenceToMqttRule* unresolved_references_to_mqtt_rules = NULL;
#endif

public:
  Config() {
    init_command_defs();
  }
  Config(int options) {
    this->options = options;
    init_command_defs();
  }
  ~Config() {}

  //-------------------------------------------------------------
  static void help() {
    fputs(
      "(c) 2017-2020 Alex Konshin\n"
#ifdef TEST_DECODING
      "Test decoding of received data for f007th* utilities.\n"
#else
      "Receive data from thermometers then print it to stdout or send it to a remote server.\n"
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
      "--auth-header, -a\n"
      "    Parameter value is additional header to send to InfluxDB. See https://docs.influxdata.com/influxdb/v2.0/reference/api/influxdb-1x/#token-authentication\n"
      "--stdout, -o\n"
      "    Print data to stdout. This option is not compatible with --server-type and --no-server.\n"
      "--no-server, -n\n"
      "    Do not print data on console or send it to servers. This option is not compatible with --server-type and --stdout.\n"
      "--all-changes, -A\n"
      "    Send/print all data. Only changed and valid data is sent by default.\n"
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

  //-------------------------------------------------------------
  void process_args (int argc, char *argv[]) {

    while (1) {
      int c = getopt_long(argc, argv, short_options, long_options, NULL);
      if (c == -1) break;

      const char* option = argv[optind];
      if (!process_cmdline_option(c, option, optarg)) {
        fprintf(stderr, "ERROR: Unknown option \"%s\".\n", option);
        Config::help();
      }
    }

#ifdef INCLUDE_MQTT
    resolveReferencesToMqttRules();
#endif

    if (optind < argc) {
      if (optind != argc-1) Config::help();
      server_url = argv[optind];
    }

    if (server_url == NULL || server_url[0] == '\0') {
      if (type_is_set) {
        if (server_type != ServerType::STDOUT && server_type != ServerType::NONE) {
          fputs("ERROR: Server URL must be specified (options --send-to or -s).\n", stderr);
          Config::help();
        }
      } else {
        server_type = ServerType::NONE;
#ifdef INCLUDE_MQTT
        // if server type and URL are not specified then
        // if any MQTT rule is specified then server type is set to NONE (no output on cosole)
        // otherwise data will be printed on console.
        if (mqtt_rules.empty()) server_type = ServerType::STDOUT;
#endif
      }
    } else {
      //TODO support UNIX sockets for InfluxDB
      if (strncmp(server_url, "http://", 7) != 0 && strncmp(server_url, "https://", 8)) {
        fputs("ERROR: Server URL must be HTTP or HTTPS.\n", stderr);
        Config::help();
      }
      if (!type_is_set) server_type = ServerType::REST;
    }
  #ifdef TEST_DECODING
    if (input_log_file_path == NULL) {
      fputs("ERROR: Input log file must be specified (option --input-log or -I).\n", stderr);
      exit(1);
    }
  #endif

  }

  //-------------------------------------------------------------
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
      log_file_path = clone(optarg);
      break;

  #ifdef TEST_DECODING
    case 'I':
      input_log_file_path = clone(optarg);
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
      server_url = clone(optarg);
      break;

    case 't':
      if (strcasecmp(optarg, "REST") == 0) {
        if (type_is_set && server_type!=ServerType::REST) {
          fputs("ERROR: Server type is specified twice.\n", stderr);
          help();
        }
        server_type = ServerType::REST;
      } else if (strcasecmp(optarg, "InfluxDB") == 0) {
        if (type_is_set && server_type!=ServerType::InfluxDB) {
          fputs("ERROR: Server type is specified twice.\n", stderr);
          help();
        }
        server_type = ServerType::InfluxDB;
      } else {
        fprintf(stderr, "ERROR: Unknown server type \"%s\".\n", optarg);
        help();
      }
      type_is_set = true;
      break;

    case 'n':
      if (type_is_set && server_type!=ServerType::NONE) {
        fputs("ERROR: Output/server type is specified twice.\n", stderr);
        help();
      }
      server_type = ServerType::NONE;
      type_is_set = true;
      break;

    case 'o':
      if (type_is_set && server_type!=ServerType::STDOUT) {
        fputs("ERROR: Output/server type is specified twice.\n", stderr);
        help();
      }
      server_type = ServerType::STDOUT;
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

    case 'a':
      auth_header = clone(optarg);
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

  /*-------------------------------------------------------------
   * Read configuration file.
   */
  void readConfig(const char* configFileRelativePath) {
    const char* configFilePath = realpath(configFileRelativePath, NULL);
    if (configFilePath == NULL) {
      fprintf(stderr, "Configuration file \"%s\" does not exist.\n", configFileRelativePath);
      exit(1);
    }

    FILE* configFileStream;

    configFileStream = fopen(configFilePath, "r");
    if (configFileStream == NULL) {
      fprintf(stderr, "Cannot open configuration file \"%s\".\n", configFilePath);
      exit(1);
    }

    fprintf(stderr, "Reading configuration file \"%s\"...\n", configFilePath);

    int linenum = 0;

    char* optarg_buffer = NULL;
    size_t optarg_bufsize = 0;
    bool quoted;

    char* line = NULL;
    size_t bufsize = 0;

    ssize_t bytesread;
    while ((bytesread = getline(&line, &bufsize, configFileStream)) != -1) {
      linenum++;
#ifndef NDEBUG
//      fprintf(stderr, "DEBUG: line #%d of file \"%s\": %s\n", linenum, configFilePath, line);
#endif
      size_t opt_len = 0;
      size_t key_len = 0;
      const char* p = line;
      const char* opt = getWord(p, opt_len);
      if (opt == NULL) continue;

      ASSERT(command_defs != NULL);
      const struct CommandDef* cmd_def;
      for (cmd_def = command_defs; cmd_def != NULL; cmd_def = cmd_def->next) {
        if (strlen(cmd_def->name) == opt_len && strncmp(cmd_def->name, opt, opt_len) == 0) break;
      }
      if ( cmd_def != NULL ) { // found definition of the command

        int number_of_unnamed_args = 0;
        const char** argv = NULL;
        if (cmd_def->max_args == 0) {
          if (skipBlanks(p) != NULL) {
            fprintf(stderr, "ERROR: No arguments expected for command \"%s\" (line #%d of file \"%s\").\n", cmd_def->name, linenum, configFilePath);
            exit(1);
          }
        } else {
          argv = (const char**)calloc(cmd_def->max_args, sizeof(const char*));

          int iarg = 0;
          bool has_named_args = false;
          const char* arg;
          size_t arg_len = 0;
          while ( (arg=getString(p, optarg_buffer, optarg_bufsize, arg_len, key_len, quoted, linenum, configFilePath)) !=NULL ) {
            const struct CmdArgDef* arg_def;
            const char* arg_value;
            int iarg_def = 0;
            if (key_len == 0) { // positional argument
              if (has_named_args) {
                fprintf(stderr, "ERROR: Unnamed argument is not allowed after named argument (line #%d of file \"%s\").\n", linenum, configFilePath);
                exit(1);
              }
              if (iarg >=cmd_def->max_num_of_unnamed_args) {
                fprintf(stderr, "ERROR: Too many unnamed arguments of command (line #%d of file \"%s\").\n", linenum, configFilePath);
                exit(1);
              }
              arg_def = &cmd_def->args[iarg];
              arg_value = arg;
              iarg_def = iarg;
              number_of_unnamed_args++;
            } else { // named argument
              if (!has_named_args) {
                has_named_args = true;
                adjust_unnamed_args(argv, number_of_unnamed_args, cmd_def->max_num_of_unnamed_args, cmd_def->args);
              }
              for (iarg_def = 0; iarg_def<cmd_def->max_args; iarg_def++) {
                arg_def = &cmd_def->args[iarg_def];
                if (arg_def->name != NULL && strlen(arg_def->name) == key_len && strncmp(arg_def->name, arg, key_len) == 0) break;
              }
              if (iarg_def>=cmd_def->max_args) {
                fprintf(stderr, "ERROR: Unrecognized argument #%d of command (line #%d of file \"%s\").\n", iarg+1, linenum, configFilePath);
                exit(1);
              }
              arg_value = arg+key_len+1;
            }
            if (argv[iarg_def] != NULL) {
              if (arg_def != NULL)
                fprintf(stderr, "ERROR: Duplicate argument \"%s\" in command (line #%d of file \"%s\").\n", arg_def->name, linenum, configFilePath);
              else
                fprintf(stderr, "PROGRAM ERROR: arg_def==NULL for argument #%d (line #%d of file \"%s\").\n", iarg+1, linenum, configFilePath);
              exit(1);
            }
            argv[iarg_def] = clone(arg_value);
            iarg++;
          }
          if (!has_named_args) {
            has_named_args = true;
            adjust_unnamed_args(argv, number_of_unnamed_args, cmd_def->max_num_of_unnamed_args, cmd_def->args);
          }
        }

        for (int i = 0; i<cmd_def->max_args; i++) {
          const char* value = argv[i];
          if (value == NULL) {
            const struct CmdArgDef* arg_def = &cmd_def->args[i];
            if ((arg_def->flags && arg_required) != 0) {
              if (arg_def->name != NULL) {
                fprintf(stderr, "ERROR: Missing argument \"%s\" of command \"%s\" (line #%d of file \"%s\").\n", arg_def->name, cmd_def->name, linenum, configFilePath);
              } else {
                fprintf(stderr, "ERROR: Missing argument #%d of command \"%s\" (line #%d of file \"%s\").\n", i+1, cmd_def->name, linenum, configFilePath);
              }
              exit(1);
            }
          }
        }
#ifndef NDEBUG
        fprintf(stderr, "DEBUG: Command \"%s\" (line #%d of file \"%s\").\n", cmd_def->name, linenum, configFilePath);
        for (int i = 0; i<cmd_def->max_args; i++) {
          const char* value = argv[i];
          if (value != NULL) {
            const struct CmdArgDef* arg_def = &cmd_def->args[i];
            if (arg_def->name != NULL) {
              fprintf(stderr, "    %s = \"%s\"\n", arg_def->name, value);
            } else {
              fprintf(stderr, "    argv[%d] = \"%s\"\n", i, value);
            }
          }
        }
#endif

        // execute the command
        execute_command(cmd_def, argv, linenum, configFilePath);

        for (int i = 0; i<cmd_def->max_args; i++) {
          const char* value = argv[i];
          if (value != NULL) free((void*)value);
        }
        free((void*)argv);
        continue;
      }

      const struct option *struct_opt;
      for (struct_opt = long_options; struct_opt->name != NULL; struct_opt++) {
        if (strlen(struct_opt->name) == opt_len && strncmp(struct_opt->name, opt, opt_len) == 0) break;
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
          optarg = getString(p, optarg_buffer, optarg_bufsize, optarg_len, key_len, quoted, linenum, configFilePath);
          if (optarg == NULL && struct_opt->has_arg == required_argument) {
            fprintf(stderr, "ERROR: Missing argument of option \"%s\" in line #%d of file \"%s\"\n", struct_opt->name, linenum, configFilePath);
            help();
          }
        }

        //fprintf(stderr, ">>> option \"%s\": val='%c' optarg=\"%s\"\n", struct_opt->name, struct_opt->val, optarg );

        if (!process_cmdline_option(struct_opt->val, struct_opt->name, optarg)) {
          fprintf(stderr, "ERROR: Unexpected error when processing option \"%s\"('%c') in line #%d of file \"%s\"\n", struct_opt->name, struct_opt->val, linenum, configFilePath);
          exit(1);
        }
        continue;
      }

      // unknown command-line option

      fprintf(stderr, "ERROR: Unrecognized option in line #%d of file \"%s\".\n", linenum, configFilePath);
      help();
    }

    if (line != NULL) free(line);
    if (optarg_buffer != NULL) free(optarg_buffer);
    fclose(configFileStream);
  }

  void adjust_unnamed_args(const char** argv, int& number_of_unnamed_args, int max_num_of_unnamed_args, const struct CmdArgDef* arg_defs) {
    if (number_of_unnamed_args>0 && number_of_unnamed_args<max_num_of_unnamed_args) {
      int max_index_missing = -1;
      for (int i = max_num_of_unnamed_args-1; i>=number_of_unnamed_args; i--) {
        const struct CmdArgDef* ad = &arg_defs[i];
        if ((ad->flags && arg_required)!=0) {
          max_index_missing = i;
          break;
        }
      }
      if (max_index_missing>0) { // there are missing required positional arguments
        for (int i = 0; i<number_of_unnamed_args; i++) {
          const struct CmdArgDef* ad = &arg_defs[i];
          if ((ad->flags && arg_required)==0) {
            memmove(argv+i+1, argv+i, (number_of_unnamed_args-i)*sizeof(char*));
            argv[i] = NULL;
            if (++number_of_unnamed_args >= max_index_missing) break;
          }
        }
      }
    }
  }

  /*-------------------------------------------------------------
   * Command "sensor':
   *   sensor <protocol> [<channel>] <rolling_code> <name>
   */
  void command_sensor(const char** argv, int linenum, const char* configFilePath) {
    const char* protocol_name = argv[CMD_SENSOR_PROTOCOL];
    const struct ProtocolDef *protocol_def;
    for (protocol_def = protocol_defs; protocol_def->name != NULL; protocol_def++) {
      if (strcasecmp(protocol_def->name, protocol_name) == 0) break;
    }
    if ( protocol_def->name == NULL ) {
      fprintf(stderr, "ERROR: Unrecognized sensor protocol in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
    }

    const char* name = argv[CMD_SENSOR_NAME];

    protocol_name = protocol_def->name;
    uint8_t channel_number = 0;
    if (protocol_def->number_of_channels != 0) {
      const char* channel_str = argv[CMD_SENSOR_CHANNEL];
      if (channel_str == NULL) {
        fprintf(stderr, "ERROR: Missed channel in the descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
        exit(1);
      }
      if (strlen(channel_str) != 1) {
        fprintf(stderr, "ERROR: Invalid value \"%s\" of channel argument in the descriptor of sensor in line #%d of file \"%s\"\n", channel_str, linenum, configFilePath);
        exit(1);
      }
      char ch = *channel_str;
      channel_number = 255;
      switch (protocol_def->channels_numbering_type) {
      case 0:
        if (ch>='0' && ch<='9') channel_number = ch-'0';
        break;
      case 1:
        if (ch>='a' && ch<='c') channel_number = ch-'a'+1;
        else if (ch>='A' && ch<='C') channel_number = ch-'A'+1;
        break;
      default:
        fprintf(stderr, "PROGRAM ERROR: Invalid value %d of channels_numbering_type for protocol %s\n", protocol_def->channels_numbering_type, protocol_def->name);
        exit(1);
      }
      if (channel_number<=0 || channel_number>protocol_def->number_of_channels) {
        fprintf(stderr, "ERROR: Invalid channel '%c' in the descriptor of sensor in line #%d of file \"%s\"\n", ch, linenum, configFilePath);
        exit(1);
      }
    }

    const char* rc_str = argv[CMD_SENSOR_RC];
    if (rc_str == NULL) {
      fprintf(stderr, "ERROR: Missed rolling code in the descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
    }
    uint32_t rolling_code = getUnsigned(rc_str, linenum, configFilePath);
    if (rolling_code >= (1U<<protocol_def->rolling_code_size)) {
      fprintf(stderr, "ERROR: Invalid value %d for rolling code in the descriptor of sensor in line #%d of file \"%s\"\n", rolling_code, linenum, configFilePath);
      exit(1);
    }
    uint32_t sensor_id = SensorData::getId(protocol_def->protocol, protocol_def->variant, channel_number, rolling_code);
    if (sensor_id == 0) {
      fprintf(stderr, "ERROR: Invalid descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
    }

    size_t name_len = name == NULL ? 0 : strlen(name);
    SensorDef* def = NULL;
    switch (SensorDef::add(sensor_id, name, name_len, def)) {
    case SENSOR_DEF_WAS_ADDED:
      break;
    case SENSOR_DEF_DUP:
      fprintf(stderr, "ERROR: Duplicate descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
      break;
    case SENSOR_DEF_DUP_NAME:
      fprintf(stderr, "ERROR: Duplicate name \"%s\" of sensor in line #%d of file \"%s\"\n", name, linenum, configFilePath);
      exit(1);
      break;
    case SENSOR_NAME_MISSING:
      fprintf(stderr, "ERROR: Sensor name/id must be specified in the descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
      break;
    case SENSOR_NAME_TOO_LONG:
      fprintf(stderr, "ERROR: Sensor name is too long in the descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
      break;
    case SENSOR_NAME_INVALID:
      fprintf(stderr, "ERROR: Invalid sensor name in the descriptor of sensor in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
      break;
    default:
      fprintf(stderr, "ERROR: Programming error - unrecognized error code from SensorDef::add() while processing line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
    }
#ifndef NDEBUG
    fprintf(stderr, "command \"sensor\" in line #%d of file \"%s\": channel=%d rolling_code=%d id=%08x name=%s ixdb_name=%s\n",
        linenum, configFilePath, channel_number, rolling_code, sensor_id, def->quoted, def->influxdb_quoted);
#endif
  }

#ifdef INCLUDE_MQTT
  /*-------------------------------------------------------------
   * Command "mqtt_broker":
   *   mqtt_broker [host=<host>] [port=<port>] [client_id=<client_id>] [user=<user> password=<password>] [keepalive=<keepalive>]
   *
   * TODO Options "protocol", "certificate", "tls_insecure", "tls_version" are not implemented yet.
   *
   */
  void command_mqtt_broker(const char** argv, int linenum, const char* configFilePath) {

    const char* host = argv[CMD_MQTT_BROKER_HOST];
    if (host == NULL) host = "localhost";
    mqtt_broker_host = clone(host);

    const char* str = argv[CMD_MQTT_BROKER_PORT];
    if (str != NULL) {
      uint32_t port = getUnsigned(str, linenum, configFilePath);
      if (port==0 || port>65535) {
        fprintf(stderr, "ERROR: Invalid MQTT broker port \"%s\" in line #%d of file \"%s\"\n", str, linenum, configFilePath);
        exit(1);
      }
      mqtt_broker_port = port;
    }

    const char* client_id = argv[CMD_MQTT_CLIENT_ID];
    if (client_id == NULL) {
      client_id = "f007th"; // FIXME get hostname
    }
    mqtt_client_id = clone(client_id);

    str = argv[CMD_MQTT_USER];
    if (str != NULL) {
      mqtt_username = clone(str);
      str = argv[CMD_MQTT_PASSWORD];
      if (str != NULL) mqtt_password = clone(str);
    }

    str = argv[CMD_MQTT_KEEPALIVE];
    if (str != NULL) {
      uint32_t keepalive = getUnsigned(str, linenum, configFilePath);
      mqtt_keepalive = keepalive;
    }

    mqtt_enable = true;
  }

  /*-------------------------------------------------------------
   * Command "mqtt_rule':
   *   mqtt_rule id=cool_high sensor="Room 1" metric=F hi=75 topic=/hvac/cooling message=on  unlock=cool_low
   */
  void command_mqtt_rule(const char** argv, int linenum, const char* configFilePath) {
    const char* sensor_name = argv[CMD_MQTT_RULE_SENSOR];
    if (sensor_name == NULL || *sensor_name == '\0')  errorMissingArg("mqtt_rule", "sensor", linenum, configFilePath);

    SensorDef* sensor_def = SensorDef::find(sensor_name);
    if (sensor_def == NULL) {
      fprintf(stderr, "ERROR: Undefined sensor name \"%s\" in line #%d of file \"%s\"\n", sensor_name, linenum, configFilePath);
      exit(1);
    }

    Metric metric;
    int scale = 0;
    bool allow_negative = false;
    const char* str = argv[CMD_MQTT_RULE_METRIC];
    if (str == NULL || *str == '\0') {
      metric = (options&OPTION_CELSIUS)!=0 ? Metric::TemperatureC : Metric::TemperatureF;
      scale = 1;
      allow_negative = true;
    } else if (str[1] != '\0') {
      errorInavidArg(str, "metric", linenum, configFilePath);
    } else if (*str == 'F') {
      metric = Metric::TemperatureF;
      scale = 1;
      allow_negative = true;
    } else if (*str == 'C') {
      metric = Metric::TemperatureC;
      scale = 1;
      allow_negative = true;
    } else if (*str == 'H') {
      metric = Metric::Humidity;
    } else if (*str == 'B') {
      metric = Metric::BatteryStatus;
    } else {
      errorInavidArg(str, "metric", linenum, configFilePath);
    }

    int lo = 0;
    bool is_lo_specified = convertDecimalArg(argv[CMD_MQTT_RULE_LO], lo, scale, allow_negative, "lo", linenum, configFilePath);
    int hi = 0;
    bool is_hi_specified = convertDecimalArg(argv[CMD_MQTT_RULE_HI], hi, scale, allow_negative, "hi", linenum, configFilePath);
    if (is_lo_specified && is_hi_specified) {
      fprintf(stderr, "ERROR: MQTT rule arguments \"hi\" and \"lo\" are mutually exclusive (line #%d of file \"%s\")\n", linenum, configFilePath);
      exit(1);
    }

    const char* topic = argv[CMD_MQTT_RULE_TOPIC];
    if (topic == NULL || *topic == '\0')  errorMissingArg("mqtt_rule", "topic", linenum, configFilePath);
    topic = clone(topic);

    const char* id = argv[CMD_MQTT_RULE_ID];
    if (id != NULL) id = *id == '\0'? NULL : clone(id);
    if (id != NULL) {
      for (MqttRule* existing_rule: mqtt_rules) {
        if (existing_rule->id != NULL && strcmp(id,existing_rule->id) == 0) {
          fprintf(stderr, "ERROR: Duplicated id \"%s\" in the definition of MQTT rule in line #%d of file \"%s\"\n", id, linenum, configFilePath);
          exit(1);
        }
      }
    }


    MessageInsert* compiledMessageFormat = NULL;
    const char* messageFormat = argv[CMD_MQTT_RULE_MESSAGE];
    if (messageFormat != NULL) {
      messageFormat = *messageFormat == '\0' ? NULL : clone(messageFormat);
      if (messageFormat != NULL) compiledMessageFormat = compileMqttMessage(messageFormat, linenum, configFilePath);
    }

    MqttRuleLock* lock_list = NULL;
    str = argv[CMD_MQTT_RULE_LOCK];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list, true, str, "lock", linenum, configFilePath);
    str = argv[CMD_MQTT_RULE_UNLOCK];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list, false, str, "unlock", linenum, configFilePath);

    MqttRule* rule = new MqttRule(sensor_def, metric, topic);
    if (is_lo_specified) {
      rule->setBound(lo, NO_BOUND);
      rule->setLocks(lock_list, BoundCheckResult::Lower);
      if (compiledMessageFormat != NULL) rule->setMessage(BoundCheckResult::Lower, messageFormat, compiledMessageFormat);
    } else if (is_hi_specified) {
      rule->setBound(NO_BOUND, hi);
      rule->setLocks(lock_list, BoundCheckResult::Higher);
      if (compiledMessageFormat != NULL) rule->setMessage(BoundCheckResult::Higher, messageFormat, compiledMessageFormat);
    } else {
      rule->setLocks(lock_list, BoundCheckResult::Inside);
      if (compiledMessageFormat != NULL) rule->setMessage(BoundCheckResult::Inside, messageFormat, compiledMessageFormat);
    }
    rule->id = id;
    sensor_def->addMqttRule(rule);
    mqtt_rules.push_back(rule);

  }

  //-------------------------------------------------------------
  void parseListOfMqttRuleLocks(MqttRuleLock** last_ptr, bool lock, const char* str, const char* argname, int linenum, const char* configFilePath) {
    if (str == NULL || *str == '\0') return;

    const char* p = str;

    while (skipBlanks(p) != NULL) {
      if (*p == ',') {
        p++;
        continue;
      }
      const char* id = p;

      char ch;
      while ((ch=*p) !='\0' && ch != ',') p++;
      size_t id_len = p-id;
      while (id_len > 0 && ((ch=id[id_len-1]) ==' ' || ch == '\t')) id_len--;
      if (id_len == 0) continue;

      MqttRuleLock* item = (MqttRuleLock*)malloc(sizeof(MqttRuleLock));
      item->next = NULL;
      item->rule = NULL;
      item->lock = lock;
      *last_ptr = item;
      last_ptr = &item->next;

      // find rule with requested id
      MqttRule* rule = NULL;
      for (MqttRule* r: mqtt_rules) {
        const char* rid = r->id;
        if (rid != NULL && strlen(rid) == id_len && strncmp(rid, id, id_len) == 0) {
          item->rule = rule = r;
          break;
        }
      }
      if ( rule == NULL) { // The rule with this id is not defined yet => delayed resolution
        UresolvedReferenceToMqttRule* reference = (UresolvedReferenceToMqttRule*)malloc(sizeof(UresolvedReferenceToMqttRule));
        reference->id = make_str(id, id_len);
        reference->rule_ptr = &(item->rule);
        reference->linenum = linenum;
        reference->configFilePath = configFilePath;
        reference->next = unresolved_references_to_mqtt_rules;
        unresolved_references_to_mqtt_rules = reference;
      }
    }

  }

  void resolveReferencesToMqttRules() {
    bool success = true;
    UresolvedReferenceToMqttRule* reference = unresolved_references_to_mqtt_rules;
    while (reference != NULL) {
      if (!resolveReferenceToMqttRule(reference)) success = false;
      UresolvedReferenceToMqttRule* next = reference->next;
      free((void*)reference->id);
      free(reference);
      reference = next;
    }
    if (!success) exit(1);
    unresolved_references_to_mqtt_rules = NULL;
  }

  bool resolveReferenceToMqttRule(UresolvedReferenceToMqttRule* reference) {
    for (MqttRule* rule: mqtt_rules) {
      const char* rid = rule->id;
      if (rid != NULL && strcmp(rid, reference->id) == 0) {
        *(reference->rule_ptr) = rule;
        return true;
      }
    }
    fprintf(stderr, "ERROR: Undefined reference to MQTT rule with id=\"%s\" in line #%d of file \"%s\"\n", reference->id, reference->linenum, reference->configFilePath);
    return false;
  }

#define MAX_MESSAGE_INSERTS 16
  static MessageInsert* compileMqttMessage(const char* messageFormat, int linenum, const char* configFilePath) {
    const char* p = messageFormat;

    int index = 0;
    MessageInsert compiled[MAX_MESSAGE_INSERTS+1];

    char ch;
    do {
      const char* start = p;
      while ( (ch=*p) != '\0' && ch != '%' ) p++;
      size_t len = p-start;
      if (len > 0) {
        if (index >= MAX_MESSAGE_INSERTS) {
          fprintf(stderr, "ERROR: Too complex message format in line #%d of file \"%s\": too many inserts.\n", linenum, configFilePath);
          exit(1);
        }
        compiled[index].type = MessageInsertType::Constant;
        compiled[index].stringArg = make_str(start, len);
        index++;
      }
      if (ch == '\0') break;

      if (index >= MAX_MESSAGE_INSERTS) {
        fprintf(stderr, "ERROR: Too complex message format in line #%d of file \"%s\": too many inserts.\n", linenum, configFilePath);
        exit(1);
      }

      ch = *++p;
      MessageInsertType messageInsertType = MessageInsertType::Constant;
      switch (ch) {
      case '\0':
        fprintf(stderr, "ERROR: Invalid message format in line #%d of file \"%s\": character is expected after %%.\n", linenum, configFilePath);
        exit(1);
        break;
      case 'F':
        messageInsertType = MessageInsertType::TemperatureF;
        break;
      case 'C':
        messageInsertType = MessageInsertType::TemperatureC;
        break;
      case 'N':
        messageInsertType = MessageInsertType::SensorName;
        break;
      case 'I':
        messageInsertType = MessageInsertType::ReferenceId;
        break;
      default:
        fprintf(stderr, "ERROR: Invalid message format in line #%d of file \"%s\": invalid substitution \"%%%c\".\n", linenum, configFilePath, ch);
        exit(1);
      }
      compiled[index].type = messageInsertType;
      compiled[index].stringArg = NULL;
      index++;

      p++;
    } while(true);

    if (index == 0) {
      fprintf(stderr, "ERROR: Empty message in line #%d of file \"%s\".\n", linenum, configFilePath);
      exit(1);
    }

    MessageInsert* result = (MessageInsert*)malloc((index+1)*sizeof(MessageInsert));
    memcpy(result, compiled, index*sizeof(MessageInsert));
    result[index].type = MessageInsertType::Constant;
    result[index].stringArg = NULL;

    return result;
  }

  /*-------------------------------------------------------------
   * Command "mqtt_bounds_rule':
   *   mqtt_bounds_rule id=cool sensor="Room 1" metric=F topic=hvac/cooling msg_hi=on msg_lo=off bounds=72.5..74.5[22:00]72..75[8:00]
   */
  void command_mqtt_bounds_rule(const char** argv, int linenum, const char* configFilePath) {
    const char* sensor_name = argv[CMD_MQTT_BOUNDS_RULE_SENSOR];
    if (sensor_name == NULL || *sensor_name == '\0')  errorMissingArg("mqtt_bounds_rule", "sensor", linenum, configFilePath);
    SensorDef* sensor_def = SensorDef::find(sensor_name);
    if (sensor_def == NULL) {
      fprintf(stderr, "ERROR: Undefined sensor name \"%s\" in line #%d of file \"%s\"\n", sensor_name, linenum, configFilePath);
      exit(1);
    }

    Metric metric;
    int scale = 0;
    bool allow_negative = false;
    const char* str = argv[CMD_MQTT_BOUNDS_RULE_METRIC];
    if (str == NULL || *str == '\0') {
      metric = (options&OPTION_CELSIUS)!=0 ? Metric::TemperatureC : Metric::TemperatureF;
      scale = 1;
      allow_negative = true;
    } else if (str[1] != '\0') {
      errorInavidArg(str, "metric", linenum, configFilePath);
    } else if (*str == 'F') {
      metric = Metric::TemperatureF;
      scale = 1;
      allow_negative = true;
    } else if (*str == 'C') {
      metric = Metric::TemperatureC;
      scale = 1;
      allow_negative = true;
    } else if (*str == 'H') {
      metric = Metric::Humidity;
    } else if (*str == 'B') {
      metric = Metric::BatteryStatus;
    } else {
      errorInavidArg(str, "metric", linenum, configFilePath);
    }

    const char* topic = argv[CMD_MQTT_BOUNDS_RULE_TOPIC];
    if (topic == NULL || *topic == '\0')  errorMissingArg("mqtt_bounds_rule", "topic", linenum, configFilePath);
    topic = clone(topic);

    const char* id = argv[CMD_MQTT_BOUNDS_RULE_ID];
    if (id != NULL) id = *id == '\0'? NULL : clone(id);
    if (id != NULL) {
      for (MqttRule* existing_rule: mqtt_rules) {
        if (existing_rule->id != NULL && strcmp(id,existing_rule->id) == 0) {
          fprintf(stderr, "ERROR: Duplicated id \"%s\" in the definition of MQTT rule in line #%d of file \"%s\"\n", id, linenum, configFilePath);
          exit(1);
        }
      }
    }

    MessageInsert* compiledMessageFormatHi = NULL;
    const char* messageFormatHi = argv[CMD_MQTT_BOUNDS_RULE_MSG_HI];
    if (messageFormatHi != NULL && *messageFormatHi != '\0') {
      messageFormatHi = clone(messageFormatHi);
      compiledMessageFormatHi = compileMqttMessage(messageFormatHi, linenum, configFilePath);
    } else {
      messageFormatHi = NULL;
    }

    MessageInsert* compiledMessageFormatLo = NULL;
    const char* messageFormatLo = argv[CMD_MQTT_BOUNDS_RULE_MSG_LO];
    if (messageFormatLo != NULL && *messageFormatLo != '\0') {
      messageFormatLo = clone(messageFormatLo);
      compiledMessageFormatLo = compileMqttMessage(messageFormatLo, linenum, configFilePath);
    } else {
      messageFormatLo = NULL;
    }

    MessageInsert* compiledMessageFormatIn = NULL;
    const char* messageFormatIn = argv[CMD_MQTT_BOUNDS_RULE_MSG_IN];
    if (messageFormatIn != NULL && *messageFormatIn != '\0') {
      messageFormatIn = clone(messageFormatIn);
      compiledMessageFormatIn = compileMqttMessage(messageFormatIn, linenum, configFilePath);
    } else {
      messageFormatIn = NULL;
    }

    MqttRuleLock* lock_list_lo = NULL;
    str = argv[CMD_MQTT_RULE_BOUNDS_LOCK_LO];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list_lo, true, str, "lock_lo", linenum, configFilePath);
    str = argv[CMD_MQTT_RULE_BOUNDS_UNLOCK_LO];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list_lo, false, str, "unlock_lo", linenum, configFilePath);

    MqttRuleLock* lock_list_hi = NULL;
    str = argv[CMD_MQTT_RULE_BOUNDS_LOCK_HI];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list_hi, true, str, "lock_hi", linenum, configFilePath);
    str = argv[CMD_MQTT_RULE_BOUNDS_UNLOCK_HI];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list_hi, false, str, "unlock_hi", linenum, configFilePath);

    MqttRuleLock* lock_list_in = NULL;
    str = argv[CMD_MQTT_RULE_BOUNDS_LOCK_IN];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list_in, true, str, "lock_in", linenum, configFilePath);
    str = argv[CMD_MQTT_RULE_BOUNDS_UNLOCK_IN];
    if (str != NULL) parseListOfMqttRuleLocks(&lock_list_in, false, str, "unlock_in", linenum, configFilePath);

    if (compiledMessageFormatHi == NULL && compiledMessageFormatLo == NULL && compiledMessageFormatIn == NULL && lock_list_lo == NULL && lock_list_hi == NULL && lock_list_in == NULL) {
      fprintf(stderr, "ERROR: At least one of options msg_*, lock_*, unlock_* must be specified for command mqtt_bounds_rule in line #%d of file \"%s\"\n", linenum, configFilePath);
      exit(1);
    }

    const char* bounds_string = argv[CMD_MQTT_RULE_BOUNDS_BOUNDS];
    if (bounds_string == NULL || *bounds_string == '\0') errorMissingArg("mqtt_bounds_rule", "bounds", linenum, configFilePath);

    // Compile bounds=72.5..74.5[22:00]72..75[8:00]
    AbstractMqttRuleBoundSchedule* bounds = compileBoundSchedule(bounds_string, scale, allow_negative, "bounds", linenum, configFilePath);

    MqttRule* rule = new MqttRule(sensor_def, metric, topic);
    rule->setBound(bounds);
    rule->id = id;
    if (compiledMessageFormatHi != NULL) rule->setMessage(BoundCheckResult::Higher, messageFormatHi, compiledMessageFormatHi);
    if (compiledMessageFormatLo != NULL) rule->setMessage(BoundCheckResult::Lower, messageFormatLo, compiledMessageFormatLo);
    if (compiledMessageFormatIn != NULL) rule->setMessage(BoundCheckResult::Inside, messageFormatIn, compiledMessageFormatIn);
    if (lock_list_hi != NULL) rule->setLocks(lock_list_hi, BoundCheckResult::Higher);
    if (lock_list_lo != NULL) rule->setLocks(lock_list_lo, BoundCheckResult::Lower);
    if (lock_list_in != NULL) rule->setLocks(lock_list_in, BoundCheckResult::Lower);
    sensor_def->addMqttRule(rule);
    mqtt_rules.push_back(rule);

  }


  // compile bounds=72.5..74.5[22:00]72..75[8:00]
  AbstractMqttRuleBoundSchedule* compileBoundSchedule(const char* arg, int scale, bool allow_negative, const char* argname, int linenum, const char* configFilePath) {
    const char* p = arg;

    int low, high;
    convertDecimalArg(arg, p, low, scale, allow_negative, '.', argname, linenum, configFilePath);
    convertDecimalArg(arg, p, high, scale, allow_negative, '[', argname, linenum, configFilePath);
    if (high <= low) errorInavidArg(arg, p, argname, linenum, configFilePath);

    if (p == NULL || *p == '\0') return new MqttRuleBoundFixed(low, high);

    uint32_t first_time_offset = getDayTimeOffset(arg, p, argname, linenum, configFilePath);
    MqttRuleBoundsSheduleItem* scheduleItem = (MqttRuleBoundsSheduleItem*)malloc(sizeof(MqttRuleBoundsSheduleItem));
    scheduleItem->bounds = MqttRuleBounds::make(low, high);
    scheduleItem->time_offset = first_time_offset;
    scheduleItem->prev = scheduleItem;
    scheduleItem->next = scheduleItem;
    MqttRuleBoundsSheduleItem* firstScheduleItem = scheduleItem;
    MqttRuleBoundsSheduleItem** pNextItem = &scheduleItem->next;

    bool flipped = false;
    uint32_t prev_time_offset = first_time_offset;
    do {
      convertDecimalArg(arg, p, low, scale, allow_negative, '.', argname, linenum, configFilePath);
      convertDecimalArg(arg, p, high, scale, allow_negative, '[', argname, linenum, configFilePath);
      if (high <= low) errorInavidArg(arg, p, argname, linenum, configFilePath);

      uint32_t time_offset = getDayTimeOffset(arg, p, argname, linenum, configFilePath);

      if (time_offset == prev_time_offset) errorInavidArg(arg, p, argname, linenum, configFilePath);
      if (time_offset < prev_time_offset) {
        if (flipped) errorInavidArg(arg, p, argname, linenum, configFilePath);
        flipped = true;
      }
      prev_time_offset = time_offset;

      MqttRuleBoundsSheduleItem* scheduleItem = (MqttRuleBoundsSheduleItem*)malloc(sizeof(MqttRuleBoundsSheduleItem));
      scheduleItem->bounds = MqttRuleBounds::make(low, high);
      scheduleItem->time_offset = time_offset;
      scheduleItem->next = firstScheduleItem;
      scheduleItem->prev = firstScheduleItem->prev;
      firstScheduleItem->prev = scheduleItem;
      *pNextItem = scheduleItem;
      pNextItem = &scheduleItem->next;

    } while (*p != '\0');

    return new MqttRuleBoundSchedule(firstScheduleItem);
  }

  static uint32_t getDayTimeOffset(const char* arg, const char*& p, const char* argname, int linenum, const char* configFilePath) {
    if (*p != '[') errorInavidArg(arg, p, argname, linenum, configFilePath);
    p++;
    int hour, min;
    convertDecimalArg(arg, p, hour, 0, false, ':', argname, linenum, configFilePath);
    if (hour > 24) errorInavidArg(arg, p, argname, linenum, configFilePath);
    if (*p != ':') errorInavidArg(arg, p, argname, linenum, configFilePath);
    p++;
    convertDecimalArg(arg, p, min, 0, false, ']', argname, linenum, configFilePath);
    if (hour==24 ? min != 0 : min > 60) errorInavidArg(arg, p, argname, linenum, configFilePath);
    if (*p != ']') errorInavidArg(arg, p, argname, linenum, configFilePath);
    p++;
    return (uint32_t)hour*60+min;
  }

#endif

  //-------------------------------------------------------------
  static bool errorMissingArg(const char* command, const char* argname, int linenum, const char* configFilePath) {
    fprintf(stderr, "ERROR: Missing argument \"%s\" in the definition of command \"%s\" in line #%d of file \"%s\"\n", argname, command, linenum, configFilePath);
    exit(1);
  }
  static bool errorInavidArg(const char* str, const char* argname, int linenum, const char* configFilePath) {
    fprintf(stderr, "ERROR: Invalid value \"%s\" of argument \"%s\" in line #%d of file \"%s\".\n", str, argname, linenum, configFilePath);
    exit(1);
  }
  static bool errorInavidArg(const char* str, const char* p, const char* argname, int linenum, const char* configFilePath) {
    int pos = p == NULL ? -1 : p-str;
    if (pos >= 0 && pos < (int)strlen(str))
      fprintf(stderr, "ERROR: Error near index %d in value \"%s\" of argument \"%s\" in line #%d of file \"%s\".\n", pos, str, argname, linenum, configFilePath);
    else
      fprintf(stderr, "ERROR: Invalid value \"%s\" of argument \"%s\" in line #%d of file \"%s\".\n", str, argname, linenum, configFilePath);
    exit(1);
  }

  //-------------------------------------------------------------
  static bool convertDecimalArg(const char* str, int& result, int scale, bool allow_negative, const char* argname, int linenum, const char* configFilePath) {
    const char* p = str;
    return convertDecimalArg(str, p, result, scale, allow_negative, '\0', argname, linenum, configFilePath);
  }

  static bool convertDecimalArg(const char* str, const char*& p, int& result, int scale, bool allow_negative, char stop_char, const char* argname, int linenum, const char* configFilePath) {
    result = 0xffffffff;
    if (skipBlanks(p) == NULL) {
      if (stop_char != '\0') errorInavidArg(str, p, argname, linenum, configFilePath);
      return false;
    }

    bool negative = false;
    char ch = *p;
    if (ch=='-') {
      if (!allow_negative) errorInavidArg(str, p, argname, linenum, configFilePath);
      negative = true;
      ch = *++p;
    } else if (ch=='+') {
      ch = *++p;
    }
    if (ch<'0' || ch>'9') errorInavidArg(str, p, argname, linenum, configFilePath);

    int n = (ch-'0');

    while ((ch=*++p)!='\0' && ch != '.' && ch != stop_char) {
      if (ch<'0' || ch>'9') errorInavidArg(str, p, argname, linenum, configFilePath);
      n = n*10 + (ch-'0');
    }

    if ((ch == '.') && (stop_char != '.' || *(p+1) != '.')) {
      if (scale <= 0) errorInavidArg(str, p, argname, linenum, configFilePath);
      while ((ch=*++p) != '\0' && ch != stop_char && --scale >= 0) {
        if (ch<'0' || ch>'9') errorInavidArg(str, p, argname, linenum, configFilePath);
        n = n*10 + (ch-'0');
      }
    }
    while (scale-- > 0) n = n*10;

    if (ch == stop_char) {
      if (stop_char == '.') {
        if (*++p != '.') errorInavidArg(str, p, argname, linenum, configFilePath);
        p++;
      }
    } else {
      if (ch == '\0' && stop_char != '[') errorInavidArg(str, p, argname, linenum, configFilePath);
    }

    if (negative) n = -n;
    result = n;
    return true;
  }


  //-------------------------------------------------------------
  static const char* skipBlanks(const char*& p) {
    if (p == NULL) return NULL;
    char ch;
    while ((ch=*p) ==' ' || ch == '\t') p++;
    if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') p = NULL;
    return p;
  }

  //-------------------------------------------------------------
  static const char* getWord(const char*& p, size_t& length) {
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

  //-------------------------------------------------------------
  static const char* getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, size_t&key_len, bool& quoted, int linenum, const char* configFilePath) {
    length = 0;
    quoted = false;
    key_len = 0;
    const char* start = skipBlanks(p);
    if (start == NULL) return p = NULL;

    char ch;
    if (*start == '"') {
      // quoted string
      quoted = true;

      int value_len = 0;
      const char* ptr = start;
      while ((ch=*++ptr) !='"') {
        if (ch == '\\') {
          ch = *++ptr;
          if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f') {
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
          case '\0':
          case '\n':
          case '\r':
          case '\f':
            fprintf(stderr, "ERROR: Backslash is not allowed at the end of line (line #%d of file \"%s\").\n", linenum, configFilePath);
            exit(1);
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

      ch = *++ptr;
      if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '#') {
        p = NULL;
      } else if (ch == ' ' || ch == '\t') {
        p = ptr;
      } else {
        fprintf(stderr, "ERROR: Invalid quoted string in line #%d of file \"%s\".\n", linenum, configFilePath);
        exit(1);
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

    // started as unquoted
    bool include_quotes = false;
    bool inside_quotes = false;
    int value_len = 0;
    while ((ch=*p) != '\0' && ch != '\r' && ch != '\n' && ch!='\f') {
      if (!inside_quotes) {
        if (ch == ' ' || ch == '\t' || ch == '#') break;
        if (ch == '=' && !include_quotes && key_len == 0) {
          key_len = p-start;
          if (key_len == 0) key_len = -1; // missing key but the argument still may be valid
        }
      }
      p++;

      if (ch == '"')  {
        inside_quotes = !inside_quotes;
        include_quotes = true;
      } else {
        if (ch == '\\') {
          if (!inside_quotes) {
            fprintf(stderr, "ERROR: Backslash is not allowed in unquoted strings (line #%d of file \"%s\").\n", linenum, configFilePath);
            exit(1);
          }
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
            if (!isHex(*p++) || !isHex(*p++)) {
              fprintf(stderr, "ERROR: Two digits are expected after \\x in line #%d of file \"%s\".\n", linenum, configFilePath);
              exit(1);
            }
            break;
          case '\0':
          case '\n':
          case '\r':
          case '\f':
            fprintf(stderr, "ERROR: Backslash is not allowed at the end of line (line #%d of file \"%s\").\n", linenum, configFilePath);
            exit(1);
          default:
            fprintf(stderr, "ERROR: Unexpected character '%c' after backslash in line #%d of file \"%s\".\n", ch, linenum, configFilePath);
            exit(1);
          }
        }
        value_len++;
      }
    }
    if (inside_quotes) {
      fprintf(stderr, "ERROR: Unmatched quote ('\"') in line #%d of file \"%s\".\n", linenum, configFilePath);
      exit(1);
    }
    if (!include_quotes && ch == '\0') {
      length = p-start;
      p = NULL;
      return start;
    }
    quoted = include_quotes;
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
              ch = getHex(p, linenum, configFilePath);
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
  static inline bool isHex(char ch) {
    return (ch>='0' && ch<='9') || (ch>='a' && ch<='f') || (ch>='A' && ch<='F');
  }

  //-------------------------------------------------------------
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

  //-------------------------------------------------------------
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

  //-------------------------------------------------------------
  static uint32_t getUnsigned(const char*& p, int linenum, const char* configFilePath) {
    if (skipBlanks(p) == NULL) {
      fprintf(stderr, "ERROR: Missed unsigned integer argument in line #%d of file \"%s\".\n", linenum, configFilePath);
      exit(1);
    }

    uint32_t result = 0;

    char ch = *p;
    if (ch>='0' && ch<='9') {
      result = (ch-'0');
    } else {
      fprintf(stderr, "ERROR: Unsigned integer argument was expected in line #%d of file \"%s\".\n", linenum, configFilePath);
      exit(1);
    }

    if ( result == 0 && *(p+1) == 'x') { // hex number
      p += 2;
      result = getHex(*p, linenum, configFilePath); // must be at least one hex digit
      for ( int i=0; i<7; i++) {
        ch = *++p;
        if (ch == ' ' || ch == '\t' ) return result;
        if (ch == '\0' || ch == '#' || ch == '\r' || ch == '\n' ) {
          p = NULL;
          return result;
        }
        result <<= 4;
        result |= getHex(ch, linenum, configFilePath);
      }
      ch = *++p;
      if (ch == ' ' || ch == '\t' ) return result;
      if (ch != '\0' && ch != '#' && ch != '\r' && ch != '\n' ) {
        fprintf(stderr, "ERROR: Invalid hex number in line #%d of file \"%s\".\n", linenum, configFilePath);
        exit(1);
      }
      p = NULL;
      return result;
    }

    while ( (ch=*++p)!='\0' && ch != '#' && ch != '\r' && ch != '\n') {
      if (ch == ' ' || ch == '\t' ) return result;
      if (ch>='0' && ch<='9') {
        result = result*10 + (ch-'0');
      } else {
        fprintf(stderr, "ERROR: Unsigned integer argument was expected in line #%d of file \"%s\".\n", linenum, configFilePath);
        exit(1);
      }
    }
    p = NULL;
    return result;
  }

  //-------------------------------------------------------------
  static const char* clone(const char* str) {
    if (str == NULL) return NULL;
    int length = strlen(str);
    if (length == 0) return EMPTY_STRING;
    char* result = (char*)malloc((length+1)*sizeof(char));
    strcpy(result, str);
    return result;
  }
  static const char* make_str(const char* str, size_t length) {
    if (str == NULL) return NULL;
    if (length == 0) return EMPTY_STRING;
    char* result = (char*)malloc((length+1)*sizeof(char));
    strcpy(result, str);
    return result;
  }

};
#endif /* CONFIG_HPP_ */
