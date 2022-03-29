/*
 * Config.hpp
 *
 * Created on: January 11, 2020
 * @author Alex Konshin <akonshin@gmail.com>
 */

#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#define RF_RECEIVER_VERSION "5.3"

#define VERBOSITY_DEBUG            1
#define VERBOSITY_INFO             2
#define VERBOSITY_PRINT_DETAILS    4
#define VERBOSITY_PRINT_STATISTICS 8
#define VERBOSITY_PRINT_UNDECODED 16
#define VERBOSITY_PRINT_JSON      32
#define VERBOSITY_PRINT_CURL      64
#define DUMP_SEQS_TO_FILE        128
#define DUMP_UNDECODED_SEQS_TO_FILE 256

#define OPTION_CELSIUS           (1<<10)
#define OPTION_UTC               (1<<11)

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>
#include <vector>

#ifdef BPiM3
#include "../mach/bpi-m3.h"
#endif

#ifdef ODROIDC2
#include "../mach/odroid-c2.h"
#endif

#ifdef RPI
#include "../mach/rpi3.h"
#endif

#ifdef __x86_64__
#include "../mach/x86_64.h"
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

#ifdef TEST_DECODING
#define DEFAULT_OPTIONS (VERBOSITY_INFO|VERBOSITY_PRINT_UNDECODED|VERBOSITY_PRINT_DETAILS)
#elif defined(INCLUDE_HTTPD)
#define DEFAULT_OPTIONS 0
#else
#define DEFAULT_OPTIONS 0
#endif

#define MAX_WWW_ROOT 256
#define MAX_SENSOR_NAME_LEN 64

//-------------------------------------------------------------
#ifdef INCLUDE_POLLSTER
#ifdef TEST_DECODING
#define W1_DEVICES_PATH "/mnt/s/Projects/f007th-rpi/test_data/devices"
#else
#define W1_DEVICES_PATH "/sys/bus/w1/devices"
#endif
#define W1_DEVICES_PATH_LEN sizeof(W1_DEVICES_PATH)
#endif
//-------------------------------------------------------------
#ifdef INCLUDE_HTTPD

#ifdef USE_GPIO_TS
#define MIN_HTTPD_PORT 1024
#else
#define MIN_HTTPD_PORT 1
#endif

#endif // INCLUDE_HTTPD
//-------------------------------------------------------------
#ifdef INCLUDE_MQTT
#include "../utils/MQTT.hpp"
#endif
//-------------------------------------------------------------

#include "SensorsData.hpp"

#define is_cmd(cmd,opt,opt_len) (opt_len == (sizeof(cmd)-1) && strncmp(cmd, opt, opt_len) == 0)

enum class ServerType : int {NONE, STDOUT, REST, InfluxDB};

//-------------------------------------------------------------
struct CmdArgDef {
  const char* name;
#define arg_required 128
  uint8_t flags; // arg_required + arg_index
};

class Config;
class ConfigParser;

typedef void (Config::*CommanExecutor)(const char** argv, int number_of_unnamed_args, ConfigParser* configParser);
#define execute_command(cmd_def,argv,number_of_unnamed_args,configParser) (this->*((cmd_def)->command_executor))(argv,number_of_unnamed_args,configParser)

struct CommandDef {
  struct CommandDef* next;
  const char* name;
  int max_num_of_unnamed_args;
  int max_args;
  const struct CmdArgDef* args;
  CommanExecutor command_executor;
};

//-------------------------------------------------------------
typedef struct UnresolvedReferenceToRule {
  struct UnresolvedReferenceToRule* next;
  AbstractRuleWithSchedule** rule_ptr;
  const char* id;
  int linenum;
  const char* configFilePath;
} UresolvedReferenceToRule;

//-------------------------------------------------------------

class Config {
private:
  struct CommandDef* command_defs = NULL;
  bool tz_set = false;

  void init_command_defs();
  void add_command_def_(const char* name, int max_num_of_unnamed_args, int max_args, const struct CmdArgDef* args, CommanExecutor command_executor);

public:
  const char* log_file_path = NULL;
  const char* dump_file_path = NULL;
  const char* server_url = NULL;
  int gpio = DEFAULT_PIN;
  ServerType server_type = ServerType::NONE;
  uint32_t protocols = 0;
  unsigned long min_sequence_length = 0;
  unsigned long max_duration = 0;
  unsigned long min_duration = 0;

  time_t max_unchanged_gap = 0L;
  const char* auth_header = NULL;

  bool protocols_set_explicitly = true;
  bool changes_only = true;
  bool type_is_set = false;

  bool verbosity_set_explicitly = false;
  uint32_t options = 0;
#ifdef INCLUDE_HTTPD
  int httpd_port = 0;
  const char* www_root = NULL;
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
#endif
#ifdef INCLUDE_POLLSTER
  bool w1_enable = false;
#endif

  std::vector<AbstractRuleWithSchedule*> rules;
  UnresolvedReferenceToRule* unresolved_references_to_rules = NULL;

public:
  Config();
  ~Config();

  static const char* getVersion();
  static void printHelpAndExit(const char* version);
  static void help();

  void process_args (int argc, char *argv[]);
  bool process_cmdline_option( int c, const char* option, const char* optarg, ConfigParser* parser);

  void enableProtocols(const char* list, ConfigParser* parser);

private:

  void readConfig(ConfigParser& configParser, const char* baseFilePath, const char* configFileRelativePath);
  void readConfig(const char* configFileRelativePath);
  void adjust_unnamed_args(const char** argv, int& number_of_unnamed_args, int max_num_of_unnamed_args, const struct CmdArgDef* arg_defs);

  void command_config(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
  void command_log(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
  void command_dump(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);

  void command_sensor(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
#ifdef INCLUDE_POLLSTER
  void command_w1(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
#endif

#ifdef INCLUDE_HTTPD
  void command_httpd(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
#endif

#ifdef INCLUDE_MQTT
  void command_mqtt_broker(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
  void command_mqtt_rule(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
  void command_mqtt_bounds_rule(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);
#endif

  void command_action_rule(const char** argv, int number_of_unnamed_args, ConfigParser* errorLogger);

  void parseListOfRuleLocks(RuleLock** last_ptr, bool lock, const char* str, const char* argname, ConfigParser* errorLogger);
  void resolveReferencesToRules();
  bool resolveReferenceToRule(UnresolvedReferenceToRule* reference);

};

#endif /* CONFIG_HPP_ */
