/*
 * Config.hpp
 *
 * Created on: January 11, 2020
 * @author Alex Konshin <akonshin@gmail.com>
 */

#define RF_RECEIVER_VERSION "5.0"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>
#include <vector>

#ifdef INCLUDE_MQTT
#include "../utils/MQTT.hpp"
#endif

#include "../protocols/Protocol.hpp"
#include "Config.hpp"
#include "ConfigParser.hpp"
#include "SensorsData.hpp"
#include "../utils/Logger.hpp"
#include "../utils/Utils.hpp"


#define is_cmd(cmd,opt,opt_len) (opt_len == (sizeof(cmd)-1) && strncmp(cmd, opt, opt_len) == 0)

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
#ifdef INCLUDE_HTTPD
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DI:WH:G:a:no";
#else
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DI:WG:a:no";
#endif
#elif defined(INCLUDE_HTTPD)
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DH:G:a:no";
#else
static const char* short_options = "c:g:s:Al:vVt:TCULd256780DG:a:no";
#endif

#define arg_required 1

#define command_def(name, max_num_of_unnamed_args) \
  static const int command_ ## name ## _max_num_of_unnamed_args = max_num_of_unnamed_args; \
  static const struct CmdArgDef COMMAND_ ## name ## _ARGS[]

#define add_command_def(name) add_command_def_(#name, command_ ## name ## _max_num_of_unnamed_args, sizeof(COMMAND_ ## name ## _ARGS)/sizeof(struct CmdArgDef), COMMAND_ ## name ## _ARGS, &Config::command_ ## name)

command_def(config, 1) = {
#define CMD_CONFIG_FILE 0
  { "file", arg_required },
};

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

#ifdef INCLUDE_HTTPD
command_def(httpd, 1) = {
#define CMD_HTTPD_PORT 0
  { "port", arg_required },
#define CMD_HTTPD_WWW_ROOT 1
  { "www_root", 0 },
};
#endif

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
#define CMD_MQTT_BOUNDS_RULE_BOUNDS    7
  { "bounds", arg_required },
#define CMD_MQTT_BOUNDS_RULE_LOCK_LO   8
  { "lock_lo", 0 },
#define CMD_MQTT_BOUNDS_RULE_UNLOCK_LO 9
  { "unlock_lo", 0 },
#define CMD_MQTT_BOUNDS_RULE_LOCK_HI   10
  { "lock_hi", 0 },
#define CMD_MQTT_BOUNDS_RULE_UNLOCK_HI 11
  { "unlock_hi", 0 },
#define CMD_MQTT_BOUNDS_RULE_LOCK_IN   12
  { "lock_in", 0 },
#define CMD_MQTT_BOUNDS_RULE_UNLOCK_IN 13
  { "unlock_in", 0 },
};

#endif

//action_rule id=cool sensor="Room 1" metric=F cmd_hi=on cmd_lo=off bounds=72.5..74.5[22:00]72..75[8:00]

command_def(action_rule, 3) = {
#define CMD_ACTION_RULE_SENSOR    0
  { "sensor", arg_required },
#define CMD_ACTION_RULE_ID        1
  { "id", 0 },
#define CMD_ACTION_RULE_METRIC    2
  { "metric", 0 },
#define CMD_ACTION_RULE_CMD_LO    3
  { "cmd_lo", 0 },
#define CMD_ACTION_RULE_CMD_HI    4
  { "cmd_hi", 0 },
#define CMD_ACTION_RULE_CMD_IN    5
  { "cmd_in", 0 },
#define CMD_ACTION_RULE_BOUNDS    6
  { "bounds", arg_required },
#define CMD_ACTION_RULE_LOCK_LO   7
  { "lock_lo", 0 },
#define CMD_ACTION_RULE_UNLOCK_LO 8
  { "unlock_lo", 0 },
#define CMD_ACTION_RULE_LOCK_HI   9
  { "lock_hi", 0 },
#define CMD_ACTION_RULE_UNLOCK_HI 10
  { "unlock_hi", 0 },
#define CMD_ACTION_RULE_LOCK_IN   11
  { "lock_in", 0 },
#define CMD_ACTION_RULE_UNLOCK_IN 12
  { "unlock_in", 0 },
};


//-------------------------------------------------------------
const char* const VALID_OPTIONS =
    "\nValid options:\n\n"
    "--config, -c\n"
    "    Argument of this option specifies configuration file.\n"
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
    "    Parameter value is additional header to send to InfluxDB.\n"
    "    See https://docs.influxdata.com/influxdb/v2.0/reference/api/influxdb-1x/#token-authentication\n"
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
    "    More verbose output.\n";

//-------------------------------------------------------------
Config::Config() {
  options = DEFAULT_OPTIONS;
  init_command_defs();
}
Config::Config(int options) {
  this->options = options;
  init_command_defs();
}
Config::~Config() {}

//-------------------------------------------------------------
void Config::init_command_defs() {
  add_command_def(config);
  add_command_def(sensor);
  add_command_def(action_rule);
#ifdef INCLUDE_HTTPD
  add_command_def(httpd);
#endif
#ifdef INCLUDE_MQTT
  add_command_def(mqtt_broker);
  add_command_def(mqtt_rule);
  add_command_def(mqtt_bounds_rule);
#endif
}

//-------------------------------------------------------------
void Config::add_command_def_(const char* name, int max_num_of_unnamed_args, int max_args, const struct CmdArgDef* args, CommanExecutor command_executor) {
  CommandDef* def = new CommandDef;
  def->name = name;
  def->max_num_of_unnamed_args = max_num_of_unnamed_args;
  def->max_args = max_args;
  def->args = args;
  def->command_executor = command_executor;
  def->next = command_defs;
  command_defs = def;
}

//-------------------------------------------------------------
const char* Config::getVersion() {
  return RF_RECEIVER_VERSION;
}

//-------------------------------------------------------------
void Config::printHelpAndExit(const char* version) {
  fputs(
    "(c) 2017-2020 Alex Konshin\n"
#ifdef TEST_DECODING
    "Test decoding of received data for f007th* utilities.\n"
#else
    "Receive data from thermometers then print it to stdout or send it to a remote server.\n"
#endif
      ,stderr
    );
  fprintf(stderr, "Version %s\n\n", version);
  fputs( VALID_OPTIONS, stderr );
  fflush(stderr);
  fclose(stderr);
  exit(1);
}

//-------------------------------------------------------------
void Config::help() {
  printHelpAndExit(getVersion());
}

//-------------------------------------------------------------
void Config::process_args (int argc, char *argv[]) {

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
  resolveReferencesToRules();
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
      if (rules.empty()) server_type = ServerType::STDOUT;
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
bool Config::process_cmdline_option( int c, const char* option, const char* optarg ) {
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

/*-------------------------------------------------------------
 * Read configuration file.
 */
void Config::readConfig(const char* configFileRelativePath) {
  ConfigParser configParser(VALID_OPTIONS);
  readConfig(configParser, NULL, configFileRelativePath);
}

void Config::readConfig(ConfigParser& configParser, const char* baseFilePath, const char* configFileRelativePath) {
  if (!configParser.open_file(baseFilePath, configFileRelativePath)) exit(1);

  char* optarg_buffer = NULL;
  size_t optarg_bufsize = 0;
  const char* p;

  while ((p = configParser.read_line()) != NULL) {
#ifndef NDEBUG
//      fprintf(stderr, "DEBUG: line #%d of file \"%s\": %s\n", linenum, configFilePath, line);
#endif
    size_t opt_len = 0;
    size_t key_len = 0;
    const char* opt = configParser.getFirstWord(p, opt_len);
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
        if (configParser.skipBlanksInCommand(p) != NULL) configParser.error("No arguments expected for command \"%s\"", cmd_def->name);
      } else {
        argv = (const char**)calloc(cmd_def->max_args, sizeof(const char*));

        int iarg = 0;
        bool has_named_args = false;
        const char* arg;
        size_t arg_len = 0;
        while ( (arg=configParser.getString(p, optarg_buffer, optarg_bufsize, arg_len, &key_len)) !=NULL ) {
          const struct CmdArgDef* arg_def = NULL;
          const char* arg_value;
          int iarg_def = 0;
          if (key_len == 0) { // positional argument
            if (has_named_args) configParser.error("Unnamed argument is not allowed after named argument");
            if (iarg >=cmd_def->max_num_of_unnamed_args) configParser.error("Too many unnamed arguments of command");
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
            if (iarg_def>=cmd_def->max_args) configParser.error("Unrecognized argument #%d of command \"%s\"", iarg+1, cmd_def->name);
            arg_value = arg+key_len+1;
          }
          if (argv[iarg_def] != NULL) {
            if (arg_def != NULL) configParser.error("Duplicate argument \"%s\" in command", arg_def->name);
            configParser.error("Program error arg_def==NULL for argument #%d", iarg+1);
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
            if (arg_def->name != NULL)
              configParser.error("Missing argument \"%s\" of command \"%s\"", arg_def->name, cmd_def->name);
            configParser.error("Missing argument #%d of command \"%s\"", i+1, cmd_def->name);
          }
        }
      }
#ifndef NDEBUG
      fprintf(stderr, "DEBUG: Command \"%s\" (line #%d of file \"%s\").\n", cmd_def->name, configParser.linenum, configParser.configFilePath);
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
      execute_command(cmd_def, argv, &configParser);

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
        if (configParser.skipBlanksInCommand(p) != NULL) configParser.errorWithHelp("Unexpected argument of option \"%s\"", struct_opt->name);
      } else {
        size_t optarg_len = 0;
        optarg = configParser.getString(p, optarg_buffer, optarg_bufsize, optarg_len, &key_len);
        if (optarg == NULL && struct_opt->has_arg == required_argument)
          configParser.errorWithHelp("Missing argument of option \"%s\"", struct_opt->name);
      }

      //fprintf(stderr, ">>> option \"%s\": val='%c' optarg=\"%s\"\n", struct_opt->name, struct_opt->val, optarg );

      if (!process_cmdline_option(struct_opt->val, struct_opt->name, optarg))
        configParser.error("Unexpected error when processing option \"%s\"('%c') ", struct_opt->name, struct_opt->val);
      continue;
    }

    // unknown command or command-line option

    configParser.print_error("Unrecognized command or option \"%s\"", make_str(opt, opt_len));
    fputs("\nValid commands:\n\n", stderr);
    for (cmd_def = command_defs; cmd_def != NULL; cmd_def = cmd_def->next) {
      fprintf(stderr, "  %s\n", cmd_def->name );
    }
    configParser.print_valid_options();
    fflush(stderr);
    fclose(stderr);
    exit(1);
  }

  if (optarg_buffer != NULL) free(optarg_buffer);
}

void Config::adjust_unnamed_args(const char** argv, int& number_of_unnamed_args, int max_num_of_unnamed_args, const struct CmdArgDef* arg_defs) {
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
 * Command "config":
 *   config <file_path>
 */
void Config::command_config(const char** argv, ConfigParser* parser) {
  const char* currentConfigPath = parser->configFilePath;

  const char* config_file_path = argv[CMD_CONFIG_FILE];
  if (config_file_path != NULL && *config_file_path != '\0') parser->error("Parameter must be specified");

#ifndef NDEBUG
  fprintf(stderr, "command \"config\" in line #%d of file \"%s\": file=\"%s\"\n",
      parser->linenum, currentConfigPath, config_file_path);
#endif

  ConfigParser configParser(VALID_OPTIONS);
  readConfig(configParser, currentConfigPath, config_file_path);

}

/*-------------------------------------------------------------
 * Command "sensor":
 *   sensor <protocol> [<channel>] <rolling_code> <name>
 */
void Config::command_sensor(const char** argv, ConfigParser* errorLogger) {
  const char* protocol_name = argv[CMD_SENSOR_PROTOCOL];
  const struct ProtocolDef* protocol_def = Protocol::getProtocolDef(protocol_name);
  if ( protocol_def == NULL ) errorLogger->error("Unrecognized sensor protocol");

  const char* name = argv[CMD_SENSOR_NAME];

  protocol_name = protocol_def->name;
  uint8_t channel_number = 0;
  if (protocol_def->number_of_channels != 0) {
    const char* channel_str = argv[CMD_SENSOR_CHANNEL];
    if (channel_str == NULL) errorLogger->error("Missed channel in the descriptor of sensor");
    if (strlen(channel_str) != 1) errorLogger->error("Invalid value \"%s\" of channel argument in the descriptor of sensor", channel_str);
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
      errorLogger->error("Program error - invalid value %d of channels_numbering_type for protocol %s", protocol_def->channels_numbering_type, protocol_def->name);
    }
    if (channel_number<=0 || channel_number>protocol_def->number_of_channels)
      errorLogger->error("Invalid channel '%c' in the descriptor of sensor", ch);
  }

  const char* rc_str = argv[CMD_SENSOR_RC];
  if (rc_str == NULL) errorLogger->error("Missed rolling code in the descriptor of sensor");
  uint32_t rolling_code = getUnsigned(rc_str, errorLogger);
  if (rolling_code >= (1U<<protocol_def->rolling_code_size))
    errorLogger->error("Invalid value %d for rolling code in the descriptor of sensor", rolling_code);
  uint32_t sensor_id = SensorData::getId(protocol_def->protocol, protocol_def->variant, channel_number, rolling_code);
  if (sensor_id == 0) errorLogger->error("Invalid descriptor of sensor");

  size_t name_len = name == NULL ? 0 : strlen(name);
  if (name_len > MAX_SENSOR_NAME_LEN) errorLogger->error("The name of the sensor is too long (max length is %d", MAX_SENSOR_NAME_LEN);

  SensorDef* def = NULL;
  switch (SensorDef::add(sensor_id, name, name_len, def)) {
  case SENSOR_DEF_WAS_ADDED:
    break;
  case SENSOR_DEF_DUP:
    errorLogger->error("Duplicate descriptor of sensor");
    break;
  case SENSOR_DEF_DUP_NAME:
    errorLogger->error("Duplicate name \"%s\" of sensor", name);
    break;
  case SENSOR_NAME_MISSING:
    errorLogger->error("Sensor name/id must be specified in the descriptor of sensor");
    break;
  case SENSOR_NAME_TOO_LONG:
    errorLogger->error("Sensor name is too long in the descriptor of sensor");
    break;
  case SENSOR_NAME_INVALID:
    errorLogger->error("Invalid sensor name in the descriptor of sensor");
    break;
  default:
    errorLogger->error("Programming error - unrecognized error code from SensorDef::add()");
  }
#ifndef NDEBUG
  fprintf(stderr, "command \"sensor\" in line #%d of file \"%s\": channel=%d rolling_code=%d id=%08x name=%s ixdb_name=%s\n",
      errorLogger->linenum, errorLogger->configFilePath, channel_number, rolling_code, sensor_id, def->quoted, def->influxdb_quoted);
#endif
}


#ifdef INCLUDE_HTTPD
/*-------------------------------------------------------------
 * Command "httpd":
 *   httpd <port> [<www_root>]
 */
void Config::command_httpd(const char** argv, ConfigParser* parser) {

  const char* port_str = argv[CMD_HTTPD_PORT];
  const char* www_root = argv[CMD_HTTPD_WWW_ROOT];

  uint32_t long_value = getUnsigned(port_str, parser);
  if (long_value<MIN_HTTPD_PORT || long_value>65535) parser->error("Invalid HTTPD port number \"%s\"", port_str);
  httpd_port = (int)long_value;

  if (www_root != NULL && *www_root != '\0') {
    size_t len = strlen(www_root);
    if (len > MAX_WWW_ROOT) parser->error("The value of parameter www_root is too long (max length is %d)", MAX_WWW_ROOT);

    const char* resolved_path = parser->resolveFilePath(www_root, CAN_BE_DIRECTORY|MUST_EXIST);
    const char* www_root_path = canonicalize_file_name(resolved_path);
    if (www_root_path == NULL) parser->error("Cannot canonicalize path of directory \"%s\"", resolved_path);
    this->www_root = www_root_path;
  }

#ifndef NDEBUG
  fprintf(stderr, "command \"httpd\" in line #%d of file \"%s\": port=%d www_root=\"%s\"\n",
      parser->linenum, parser->configFilePath, httpd_port, www_root);
#endif
}
#endif


#ifdef INCLUDE_MQTT
/*-------------------------------------------------------------
 * Command "mqtt_broker":
 *   mqtt_broker [host=<host>] [port=<port>] [client_id=<client_id>] [user=<user> password=<password>] [keepalive=<keepalive>]
 *
 * TODO Options "protocol", "certificate", "tls_insecure", "tls_version" are not implemented yet.
 *
 */
void Config::command_mqtt_broker(const char** argv, ConfigParser* errorLogger) {
  const char* host = argv[CMD_MQTT_BROKER_HOST];
  if (host == NULL) host = "localhost";
  mqtt_broker_host = clone(host);

  const char* str = argv[CMD_MQTT_BROKER_PORT];
  if (str != NULL) {
    uint32_t port = getUnsigned(str, errorLogger);
    if (port==0 || port>65535) errorLogger->error("Invalid MQTT broker port \"%s\"", str);
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
    uint32_t keepalive = getUnsigned(str, errorLogger);
    mqtt_keepalive = keepalive;
  }

  mqtt_enable = true;
}

/*-------------------------------------------------------------
 * Command "mqtt_rule':
 *   mqtt_rule id=cool_high sensor="Room 1" metric=F hi=75 topic=/hvac/cooling message=on  unlock=cool_low
 */
void Config::command_mqtt_rule(const char** argv, ConfigParser* errorLogger) {
  const char* sensor_name = argv[CMD_MQTT_RULE_SENSOR];
  if (sensor_name == NULL || *sensor_name == '\0')  errorMissingArg("mqtt_rule", "sensor", errorLogger);

  SensorDef* sensor_def = SensorDef::find(sensor_name);
  if (sensor_def == NULL) errorLogger->error("Undefined sensor name \"%s\"", sensor_name);

  Metric metric = Metric::TemperatureF;
  int scale = 0;
  bool allow_negative = false;
  const char* str = argv[CMD_MQTT_RULE_METRIC];
  if (str == NULL || *str == '\0') {
    metric = (options&OPTION_CELSIUS)!=0 ? Metric::TemperatureC : Metric::TemperatureF;
    scale = 1;
    allow_negative = true;
  } else if (str[1] != '\0') {
    errorInavidArg(str, "metric", errorLogger);
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
    errorInavidArg(str, "metric", errorLogger);
  }

  int lo = 0;
  bool is_lo_specified = convertDecimalArg(argv[CMD_MQTT_RULE_LO], lo, scale, allow_negative, "lo", errorLogger);
  int hi = 0;
  bool is_hi_specified = convertDecimalArg(argv[CMD_MQTT_RULE_HI], hi, scale, allow_negative, "hi", errorLogger);
  if (is_lo_specified && is_hi_specified) {
    errorLogger->error("MQTT rule arguments \"hi\" and \"lo\" are mutually exclusive");
    exit(1);
  }

  const char* topic = argv[CMD_MQTT_RULE_TOPIC];
  if (topic == NULL || *topic == '\0')  errorMissingArg("mqtt_rule", "topic", errorLogger);
  topic = clone(topic);

  const char* id = argv[CMD_MQTT_RULE_ID];
  if (id != NULL) id = *id == '\0'? NULL : clone(id);
  if (id != NULL) {
    for (AbstractRuleWithSchedule* existing_rule: rules) {
      if (existing_rule->id != NULL && strcmp(id,existing_rule->id) == 0)
        errorLogger->error("Duplicated id \"%s\" in the definition of rule", id);
    }
  }

  MessageInsert* compiledMessageFormat = NULL;
  const char* messageFormat = argv[CMD_MQTT_RULE_MESSAGE];
  if (messageFormat != NULL) {
    messageFormat = *messageFormat == '\0' ? NULL : clone(messageFormat);
    if (messageFormat != NULL) compiledMessageFormat = compileMessage(messageFormat, errorLogger);
  }

  RuleLock* lock_list = NULL;
  str = argv[CMD_MQTT_RULE_LOCK];
  if (str != NULL) parseListOfRuleLocks(&lock_list, true, str, "lock", errorLogger);
  str = argv[CMD_MQTT_RULE_UNLOCK];
  if (str != NULL) parseListOfRuleLocks(&lock_list, false, str, "unlock", errorLogger);

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
  sensor_def->addRule(rule);
  rules.push_back(rule);

}

/*-------------------------------------------------------------
 * Command "mqtt_bounds_rule':
 *   mqtt_bounds_rule id=cool sensor="Room 1" metric=F topic=hvac/cooling msg_hi=on msg_lo=off bounds=72.5..74.5[22:00]72..75[8:00]
 */
void Config::command_mqtt_bounds_rule(const char** argv, ConfigParser* errorLogger) {
  const char* sensor_name = argv[CMD_MQTT_BOUNDS_RULE_SENSOR];
  if (sensor_name == NULL || *sensor_name == '\0')  errorMissingArg("mqtt_bounds_rule", "sensor", errorLogger);
  SensorDef* sensor_def = SensorDef::find(sensor_name);
  if (sensor_def == NULL) errorLogger->error("Undefined sensor name \"%s\"", sensor_name);

  Metric metric = Metric::TemperatureF;
  int scale = 0;
  bool allow_negative = false;
  const char* str = argv[CMD_MQTT_BOUNDS_RULE_METRIC];
  if (str == NULL || *str == '\0') {
    metric = (options&OPTION_CELSIUS)!=0 ? Metric::TemperatureC : Metric::TemperatureF;
    scale = 1;
    allow_negative = true;
  } else if (str[1] != '\0') {
    errorInavidArg(str, "metric", errorLogger);
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
    errorInavidArg(str, "metric", errorLogger);
    return;
  }

  const char* topic = argv[CMD_MQTT_BOUNDS_RULE_TOPIC];
  if (topic == NULL || *topic == '\0')  errorMissingArg("mqtt_bounds_rule", "topic", errorLogger);
  topic = clone(topic);

  const char* id = argv[CMD_MQTT_BOUNDS_RULE_ID];
  if (id != NULL) id = *id == '\0'? NULL : clone(id);
  if (id != NULL) {
    for (AbstractRuleWithSchedule* existing_rule: rules) {
      if (existing_rule->id != NULL && strcmp(id,existing_rule->id) == 0)
        errorLogger->error("Duplicated id \"%s\" in the definition of rule", id);
    }
  }

  MessageInsert* compiledMessageFormatHi = NULL;
  const char* messageFormatHi = argv[CMD_MQTT_BOUNDS_RULE_MSG_HI];
  if (messageFormatHi != NULL && *messageFormatHi != '\0') {
    messageFormatHi = clone(messageFormatHi);
    compiledMessageFormatHi = compileMessage(messageFormatHi, errorLogger);
  } else {
    messageFormatHi = NULL;
  }

  MessageInsert* compiledMessageFormatLo = NULL;
  const char* messageFormatLo = argv[CMD_MQTT_BOUNDS_RULE_MSG_LO];
  if (messageFormatLo != NULL && *messageFormatLo != '\0') {
    messageFormatLo = clone(messageFormatLo);
    compiledMessageFormatLo = compileMessage(messageFormatLo, errorLogger);
  } else {
    messageFormatLo = NULL;
  }

  MessageInsert* compiledMessageFormatIn = NULL;
  const char* messageFormatIn = argv[CMD_MQTT_BOUNDS_RULE_MSG_IN];
  if (messageFormatIn != NULL && *messageFormatIn != '\0') {
    messageFormatIn = clone(messageFormatIn);
    compiledMessageFormatIn = compileMessage(messageFormatIn, errorLogger);
  } else {
    messageFormatIn = NULL;
  }

  RuleLock* lock_list_lo = NULL;
  str = argv[CMD_MQTT_BOUNDS_RULE_LOCK_LO];
  if (str != NULL) parseListOfRuleLocks(&lock_list_lo, true, str, "lock_lo", errorLogger);
  str = argv[CMD_MQTT_BOUNDS_RULE_UNLOCK_LO];
  if (str != NULL) parseListOfRuleLocks(&lock_list_lo, false, str, "unlock_lo", errorLogger);

  RuleLock* lock_list_hi = NULL;
  str = argv[CMD_MQTT_BOUNDS_RULE_LOCK_HI];
  if (str != NULL) parseListOfRuleLocks(&lock_list_hi, true, str, "lock_hi", errorLogger);
  str = argv[CMD_MQTT_BOUNDS_RULE_UNLOCK_HI];
  if (str != NULL) parseListOfRuleLocks(&lock_list_hi, false, str, "unlock_hi", errorLogger);

  RuleLock* lock_list_in = NULL;
  str = argv[CMD_MQTT_BOUNDS_RULE_LOCK_IN];
  if (str != NULL) parseListOfRuleLocks(&lock_list_in, true, str, "lock_in", errorLogger);
  str = argv[CMD_MQTT_BOUNDS_RULE_UNLOCK_IN];
  if (str != NULL) parseListOfRuleLocks(&lock_list_in, false, str, "unlock_in", errorLogger);

  if (compiledMessageFormatHi == NULL && compiledMessageFormatLo == NULL && compiledMessageFormatIn == NULL && lock_list_lo == NULL && lock_list_hi == NULL && lock_list_in == NULL)
    errorLogger->error("At least one of options msg_*, lock_*, unlock_* must be specified for command mqtt_bounds_rule");

  const char* bounds_string = argv[CMD_MQTT_BOUNDS_RULE_BOUNDS];
  if (bounds_string == NULL || *bounds_string == '\0') errorMissingArg("mqtt_bounds_rule", "bounds", errorLogger);

  // Compile bounds=72.5..74.5[22:00]72..75[8:00]
  AbstractRuleBoundSchedule* bounds = compileBoundSchedule(bounds_string, scale, allow_negative, "bounds", errorLogger);

  MqttRule* rule = new MqttRule(sensor_def, metric, topic);
  rule->setBound(bounds);
  rule->id = id;
  if (compiledMessageFormatHi != NULL) rule->setMessage(BoundCheckResult::Higher, messageFormatHi, compiledMessageFormatHi);
  if (compiledMessageFormatLo != NULL) rule->setMessage(BoundCheckResult::Lower, messageFormatLo, compiledMessageFormatLo);
  if (compiledMessageFormatIn != NULL) rule->setMessage(BoundCheckResult::Inside, messageFormatIn, compiledMessageFormatIn);
  if (lock_list_hi != NULL) rule->setLocks(lock_list_hi, BoundCheckResult::Higher);
  if (lock_list_lo != NULL) rule->setLocks(lock_list_lo, BoundCheckResult::Lower);
  if (lock_list_in != NULL) rule->setLocks(lock_list_in, BoundCheckResult::Lower);
  sensor_def->addRule(rule);
  rules.push_back(rule);

}

#endif

/*-------------------------------------------------------------
 * Command "action_rule':
 *   action_rule id=cool sensor="Room 1" metric=F cmd_hi=on cmd_lo=off bounds=72.5..74.5[22:00]72..75[8:00]
 */
void Config::command_action_rule(const char** argv, ConfigParser* errorLogger) {
  const char* sensor_name = argv[CMD_ACTION_RULE_SENSOR];
  if (sensor_name == NULL || *sensor_name == '\0')  errorMissingArg("action_rule", "sensor", errorLogger);
  SensorDef* sensor_def = SensorDef::find(sensor_name);
  if (sensor_def == NULL) errorLogger->error("Undefined sensor name \"%s\"", sensor_name);

  Metric metric = Metric::TemperatureF;
  int scale = 0;
  bool allow_negative = false;
  const char* str = argv[CMD_ACTION_RULE_METRIC];
  if (str == NULL || *str == '\0') {
    metric = (options&OPTION_CELSIUS)!=0 ? Metric::TemperatureC : Metric::TemperatureF;
    scale = 1;
    allow_negative = true;
  } else if (str[1] != '\0') {
    errorInavidArg(str, "metric", errorLogger);
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
    errorInavidArg(str, "metric", errorLogger);
  }

  const char* id = argv[CMD_ACTION_RULE_ID];
  if (id != NULL) id = *id == '\0'? NULL : clone(id);
  if (id != NULL) {
    for (AbstractRuleWithSchedule* existing_rule: rules) {
      if (existing_rule->id != NULL && strcmp(id,existing_rule->id) == 0)
        errorLogger->error("Duplicated id \"%s\" in the definition of rule", id);
    }
  }

  MessageInsert* compiledMessageFormatHi = NULL;
  const char* messageFormatHi = argv[CMD_ACTION_RULE_CMD_HI];
  if (messageFormatHi != NULL && *messageFormatHi != '\0') {
    messageFormatHi = clone(messageFormatHi);
    compiledMessageFormatHi = compileMessage(messageFormatHi, errorLogger);
  } else {
    messageFormatHi = NULL;
  }

  MessageInsert* compiledMessageFormatLo = NULL;
  const char* messageFormatLo = argv[CMD_ACTION_RULE_CMD_LO];
  if (messageFormatLo != NULL && *messageFormatLo != '\0') {
    messageFormatLo = clone(messageFormatLo);
    compiledMessageFormatLo = compileMessage(messageFormatLo, errorLogger);
  } else {
    messageFormatLo = NULL;
  }

  MessageInsert* compiledMessageFormatIn = NULL;
  const char* messageFormatIn = argv[CMD_ACTION_RULE_CMD_IN];
  if (messageFormatIn != NULL && *messageFormatIn != '\0') {
    messageFormatIn = clone(messageFormatIn);
    compiledMessageFormatIn = compileMessage(messageFormatIn, errorLogger);
  } else {
    messageFormatIn = NULL;
  }

  RuleLock* lock_list_lo = NULL;
  str = argv[CMD_ACTION_RULE_LOCK_LO];
  if (str != NULL) parseListOfRuleLocks(&lock_list_lo, true, str, "lock_lo", errorLogger);
  str = argv[CMD_ACTION_RULE_UNLOCK_LO];
  if (str != NULL) parseListOfRuleLocks(&lock_list_lo, false, str, "unlock_lo", errorLogger);

  RuleLock* lock_list_hi = NULL;
  str = argv[CMD_ACTION_RULE_LOCK_HI];
  if (str != NULL) parseListOfRuleLocks(&lock_list_hi, true, str, "lock_hi", errorLogger);
  str = argv[CMD_ACTION_RULE_UNLOCK_HI];
  if (str != NULL) parseListOfRuleLocks(&lock_list_hi, false, str, "unlock_hi", errorLogger);

  RuleLock* lock_list_in = NULL;
  str = argv[CMD_ACTION_RULE_LOCK_IN];
  if (str != NULL) parseListOfRuleLocks(&lock_list_in, true, str, "lock_in", errorLogger);
  str = argv[CMD_ACTION_RULE_UNLOCK_IN];
  if (str != NULL) parseListOfRuleLocks(&lock_list_in, false, str, "unlock_in", errorLogger);

  if (compiledMessageFormatHi == NULL && compiledMessageFormatLo == NULL && compiledMessageFormatIn == NULL && lock_list_lo == NULL && lock_list_hi == NULL && lock_list_in == NULL)
    errorLogger->error("At least one of options cmd_*, lock_*, unlock_* must be specified for command action_rule");

  const char* bounds_string = argv[CMD_ACTION_RULE_BOUNDS];
  if (bounds_string == NULL || *bounds_string == '\0') errorMissingArg("action_rule", "bounds", errorLogger);

  // Compile bounds=72.5..74.5[22:00]72..75[8:00]
  AbstractRuleBoundSchedule* bounds = compileBoundSchedule(bounds_string, scale, allow_negative, "bounds", errorLogger);

  ActionRule* rule = new ActionRule(sensor_def, metric);
  rule->setBound(bounds);
  rule->id = id;
  if (compiledMessageFormatHi != NULL) rule->setMessage(BoundCheckResult::Higher, messageFormatHi, compiledMessageFormatHi);
  if (compiledMessageFormatLo != NULL) rule->setMessage(BoundCheckResult::Lower, messageFormatLo, compiledMessageFormatLo);
  if (compiledMessageFormatIn != NULL) rule->setMessage(BoundCheckResult::Inside, messageFormatIn, compiledMessageFormatIn);
  if (lock_list_hi != NULL) rule->setLocks(lock_list_hi, BoundCheckResult::Higher);
  if (lock_list_lo != NULL) rule->setLocks(lock_list_lo, BoundCheckResult::Lower);
  if (lock_list_in != NULL) rule->setLocks(lock_list_in, BoundCheckResult::Lower);
  sensor_def->addRule(rule);
  rules.push_back(rule);

}

//-------------------------------------------------------------
void Config::parseListOfRuleLocks(RuleLock** last_ptr, bool lock, const char* str, const char* argname, ConfigParser* errorLogger) {
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

    RuleLock* item = (RuleLock*)malloc(sizeof(RuleLock));
    item->next = NULL;
    item->rule = NULL;
    item->lock = lock;
    *last_ptr = item;
    last_ptr = &item->next;

    // find rule with requested id
    AbstractRuleWithSchedule* rule = NULL;
    for (AbstractRuleWithSchedule* r: rules) {
      const char* rid = r->id;
      if (rid != NULL && strlen(rid) == id_len && strncmp(rid, id, id_len) == 0) {
        item->rule = rule = r;
        break;
      }
    }
    if (rule == NULL) { // The rule with this id is not defined yet => delayed resolution
      UnresolvedReferenceToRule* reference = (UnresolvedReferenceToRule*)malloc(sizeof(UnresolvedReferenceToRule));
      reference->id = make_str(id, id_len);
      reference->rule_ptr = &(item->rule);
      reference->linenum = errorLogger->linenum;
      reference->configFilePath = errorLogger->configFilePath;
      reference->next = unresolved_references_to_rules;
      unresolved_references_to_rules = reference;
    }
  }

}

//-------------------------------------------------------------
void Config::resolveReferencesToRules() {
  bool success = true;
  UnresolvedReferenceToRule* reference = unresolved_references_to_rules;
  while (reference != NULL) {
    if (!resolveReferenceToRule(reference)) success = false;
    UnresolvedReferenceToRule* next = reference->next;
    free((void*)reference->id);
    free(reference);
    reference = next;
  }
  if (!success) exit(1);
  unresolved_references_to_rules = NULL;
}

//-------------------------------------------------------------
bool Config::resolveReferenceToRule(UnresolvedReferenceToRule* reference) {
  for (AbstractRuleWithSchedule* rule: rules) {
    const char* rid = rule->id;
    if (rid != NULL && strcmp(rid, reference->id) == 0) {
      *(reference->rule_ptr) = rule;
      return true;
    }
  }
  fprintf(stderr, "ERROR: Undefined reference to rule with id=\"%s\" in line #%d of file \"%s\"\n", reference->id, reference->linenum, reference->configFilePath);
  return false;
}


