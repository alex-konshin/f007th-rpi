/*
 * F007TH_push_to_rest.cpp
 *
 * Push sensors readings to remote server via REST API
 *
 * Created on: February 4, 2017
 * @author Alex Konshin <akonshin@gmail.com>
 */

#include "common/Receiver.hpp"
#include <curl/curl.h>

#include "common/SensorsData.hpp"
#include "common/Config.hpp"

#ifdef INCLUDE_HTTPD
#include "utils/HTTPD.hpp"
#endif

static bool send(ReceivedMessage& message, Config& cfg, int changed, void*& data_buffer, size_t& buffer_size, char* response_buffer, FILE* log);


int main(int argc, char *argv[]) {

  if ( argc==1 ) Config::help();

  Config cfg(DEFAULT_OPTIONS);
  cfg.process_args(argc, argv);

  FILE* log;
  { // setup log file
    const char* log_file_path;
    if (cfg.log_file_path == NULL || cfg.log_file_path[0]=='\0') {

#ifdef TEST_DECODING
#define LOG_FILE_OPEN_MODE "w"
      log_file_path = "f007th-test-decoding.log";
#else
#define LOG_FILE_OPEN_MODE "a"
      log_file_path = "f007th-send.log";
#endif
      cfg.log_file_path = log_file_path = realpath(log_file_path, NULL);
    } else {
      log_file_path = cfg.log_file_path;
    }
    log = openFileForWriting(cfg.log_file_path, LOG_FILE_OPEN_MODE);
    if (log == NULL) {
      fprintf(stderr, "Failed to open log file \"%s\".\n", cfg.log_file_path);
      exit(1);
    }
    fprintf(stderr, "Log file is \"%s\".\n", cfg.log_file_path);
  }

  FILE* dump_file = NULL;
  if ((cfg.options&DUMP_SEQS_TO_FILE) != 0 && cfg.dump_file_path != NULL && *cfg.dump_file_path != '\0') {
    dump_file = openFileForWriting(cfg.dump_file_path, "w");
    if (dump_file == NULL) {
      fprintf(stderr, "Failed to open dump file \"%s\" for writing.\n", cfg.dump_file_path);
      exit(1);
    }
    fprintf(stderr, "Dump file is \"%s\".", cfg.dump_file_path);
  }
  fflush(stderr);

  char* response_buffer = NULL;
  size_t buffer_size = SEND_DATA_BUFFER_SIZE*sizeof(char);
  void* data_buffer = malloc(buffer_size);
  if (data_buffer == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }

  if (cfg.server_type!=ServerType::STDOUT && cfg.server_type!=ServerType::NONE) {
    curl_global_init(CURL_GLOBAL_ALL);
    response_buffer = (char*)malloc(SERVER_RESPONSE_BUFFER_SIZE*sizeof(char));
  }

  SensorsData sensorsData(cfg.options);

  Receiver receiver(&cfg);
  Log->setLogFile(log);
#ifdef TEST_DECODING
  receiver.setInputLogFile(cfg.input_log_file_path);
  receiver.setWaitAfterReading(cfg.wait_after_reading);
#endif

  ReceivedMessage message;

  receiver.enableReceive();

#ifdef INCLUDE_HTTPD
  HTTPD* httpd = NULL;

  if (cfg.httpd_port >= MIN_HTTPD_PORT && cfg.httpd_port<65535) {
    // start HTTPD server
    Log->log("Starting HTTPD server on port %d...", cfg.httpd_port);
    httpd = HTTPD::start(&sensorsData, &cfg);
    if (httpd == NULL) {
      Log->error("Could not start HTTPD server on port %d.", cfg.httpd_port);
      fclose(log);
      exit(1);
    }
  }
#endif

#ifdef INCLUDE_MQTT
  if (!MqttPublisher::create(cfg)) {
    fclose(log);
    exit(1);
  }
#endif

  if ((cfg.options&VERBOSITY_PRINT_STATISTICS) != 0) receiver.printStatisticsPeriodically(1000); // print statistics every second

#define RULE_MESSAGE_MAX_SIZE 4096
  char rule_message_buffer[RULE_MESSAGE_MAX_SIZE];

  if ((cfg.options&(VERBOSITY_INFO|VERBOSITY_DEBUG)) != 0) fputs("Receiving data...\n", stderr);
  while(!receiver.isStopped()) {
    bool got_data = receiver.waitForMessage(message);
    if (receiver.isStopped()) break;
    if (got_data) {
      if (message.isEmpty()) {
        fputs("ERROR: Missing data.\n", stderr);
        continue;
      }

      bool is_message_printed = false;
      bool verbose = (cfg.options&(VERBOSITY_INFO|VERBOSITY_DEBUG)) != 0;

      if (dump_file != NULL && (cfg.options&(DUMP_SEQS_TO_FILE|DUMP_UNDECODED_SEQS_TO_FILE)) == DUMP_SEQS_TO_FILE) { // write the received sequence (if any) to the dump file
        message.printInputSequence(dump_file, cfg.options);
        fflush(dump_file);
      }

      if (verbose || ((cfg.options&VERBOSITY_PRINT_UNDECODED) != 0 && message.isUndecoded())) {
        message.print(stdout, log, cfg.options);
        is_message_printed = true;
      }
      if (message.isUndecoded()) {
        if (verbose) fputs("Could not decode the received data.\n", stderr);
        if (dump_file != NULL && (cfg.options&(DUMP_SEQS_TO_FILE|DUMP_UNDECODED_SEQS_TO_FILE)) == (DUMP_SEQS_TO_FILE|DUMP_UNDECODED_SEQS_TO_FILE)) {
          // write undecoded received sequence to the dump file
          message.printInputSequence(dump_file, cfg.options);
          fflush(dump_file);
        }
      } else {
        bool isValid = message.isValid();
        int changed = isValid ? message.update(sensorsData, cfg.max_unchanged_gap) : 0;
        if (changed != TIME_NOT_CHANGED) {
          int really_changed = changed;
          if (changed == 0 && !cfg.changes_only && (isValid || (cfg.server_type != ServerType::InfluxDB && cfg.server_type != ServerType::NONE)))
            changed = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
          if (changed != 0) {
            if (cfg.server_type == ServerType::STDOUT) {
              if (!is_message_printed) // already printed
                message.print(stdout, NULL, cfg.options);
            } else if (cfg.server_type != ServerType::NONE) {
              if (!send(message, cfg, changed, data_buffer, buffer_size, response_buffer, log) && verbose)
                Log->info("No data was sent to server.");
            }
          } else {
            if (verbose) {
              if (cfg.server_type == ServerType::STDOUT) {
                if (!isValid)
                  fputs("Data is corrupted.\n", stderr);
                else
                  fputs("Data is not changed.\n", stderr);
              } else if (cfg.server_type != ServerType::NONE) {
                if (!isValid)
                  fputs("Data is corrupted and is not sent to server.\n", stderr);
                else
                  fputs("Data is not changed and is not sent to server.\n", stderr);
              }
            }
          }

          if (isValid) {
            SensorData* sensorData = &message.data->sensorData;
            SensorDef* sensorDef = sensorData->def;
            if (sensorDef != NULL) {
              bool debug = (cfg.options&VERBOSITY_DEBUG) != 0;
              AbstractRuleWithSchedule* rule = sensorDef->getRules();
              while (rule != NULL) {
                BoundCheckResult checkResult = sensorData->checkRule(rule, really_changed);
                if (checkResult != BoundCheckResult::Locked && checkResult != BoundCheckResult::NotApplicable) {
                  if (debug) Log->info("%s \"%s\" => MATCHED.", rule->getTypeName(), rule->id);
                  uint32_t size = rule->formatMessage(rule_message_buffer, RULE_MESSAGE_MAX_SIZE, checkResult, sensorData);
                  if (size > 0) rule->execute(rule_message_buffer, cfg);
                  rule->applyLocks(checkResult);
                }
                rule = rule->next;
              }
            }
          }

        }
      }

    }

    if (receiver.checkAndResetTimerEvent()) {
      if ((cfg.options&VERBOSITY_PRINT_STATISTICS) != 0) receiver.printStatistics();
    }
  }

#ifdef INCLUDE_MQTT
  MqttPublisher::destroy();
#endif

#ifdef INCLUDE_HTTPD
  HTTPD::destroy(httpd);
#endif

  if ((cfg.options&VERBOSITY_INFO) != 0) fputs("\nExiting...\n", stderr);

  // finally
  if (dump_file != NULL) fclose(dump_file);
  free(data_buffer);
  if (response_buffer != NULL) free(response_buffer);
  Log->log("Exiting...");
  fclose(log);
  if (cfg.server_type != ServerType::STDOUT && cfg.server_type != ServerType::NONE) curl_global_cleanup();

  exit(0);
}

typedef struct PutData {
  uint8_t* data;
  size_t len;
  uint8_t* response_write;
  size_t response_remain;
  bool verbose;
} PutData;
/*
static size_t read_callback(void *ptr, size_t size, size_t nmemb, struct PutData* data) {
  size_t curl_size = nmemb * size;
  size_t to_copy = (data->len < curl_size) ? data->len : curl_size;
  memcpy(ptr, data->data, to_copy);
  if (data->verbose) fprintf(stderr, "sending %d bytes...\n", to_copy);
  data->len -= to_copy;
  data->data += to_copy;
  return to_copy;
}
*/
static size_t write_callback(void *ptr, size_t size, size_t nmemb, struct PutData* data) {
  size_t curl_size = nmemb * size;
  size_t to_copy = (data->response_remain < curl_size) ? data->response_remain : curl_size;
  if (data->verbose) fprintf(stderr, "receiving %d bytes...\n", (int)to_copy);
  if (to_copy > 0) {
    memcpy(data->response_write, ptr, to_copy);
    data->response_remain -= to_copy;
    data->response_write += to_copy;
    return to_copy;
  }
  return size;
}

bool send(ReceivedMessage& message, Config& cfg, int changed, void*& data_buffer, size_t& buffer_size, char* response_buffer, FILE* log) {
  bool verbose = (cfg.options&VERBOSITY_PRINT_DETAILS) != 0;
  if (verbose) fputs("===> called send()\n", stderr);

  *(char*)data_buffer = '\0';
  ssize_t data_size;
  if (cfg.server_type == ServerType::InfluxDB) {
    data_size = message.influxDB(data_buffer, buffer_size, changed, cfg.options);
  } else {
    data_size = message.json(data_buffer, buffer_size, cfg.options);
  }
  if (data_size <= 0) {
    if (verbose) fputs("===> return from send() without sending because output data was not generated\n", stderr);
    return false;
  }

  struct PutData data;
  data.data = (uint8_t*)data_buffer;
  data.len = data_size;
  data.response_write = (uint8_t*)response_buffer;
  data.response_remain = SERVER_RESPONSE_BUFFER_SIZE;
  data.verbose = verbose;
  response_buffer[0] = '\0';

  CURL* curl = curl_easy_init();  // get a curl handle
  if (!curl) {
    Log->error("Failed to get curl handle.", stderr);
    if (verbose) fputs("===> return from send() without sending because failed to initialize curl\n", stderr);
    return false;
  }
  if ((cfg.options&VERBOSITY_PRINT_CURL) != 0) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  struct curl_slist *headers = NULL;
  if (cfg.server_type == ServerType::REST) {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "Connection: close");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  } if (cfg.server_type == ServerType::InfluxDB) {
    headers = curl_slist_append(headers, "Content-Type:"); // do not set content type
    headers = curl_slist_append(headers, "Accept:");
    if (cfg.auth_header != NULL) {
      headers = curl_slist_append(headers, cfg.auth_header);
      if (verbose) {
        fputs(cfg.auth_header, stderr);
        fputc('\n', stderr);
      }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }

  curl_easy_setopt(curl, CURLOPT_URL, cfg.server_url);
//  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  if (cfg.server_type == ServerType::InfluxDB)
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  else
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
//  curl_easy_setopt(curl, CURLOPT_READFUNCTION, &read_callback);
//  curl_easy_setopt(curl, CURLOPT_READDATA, &data);
//  curl_off_t size = json_size;
//  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, size);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_buffer);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK)
    Log->error("Sending data to %s failed: %s", cfg.server_url, curl_easy_strerror(rc));

  if (verbose && response_buffer[0] != '\0') {
    if (data.response_remain <= 0) data.response_remain = 1;
    response_buffer[SERVER_RESPONSE_BUFFER_SIZE-data.response_remain] = '\0';
    fputs(response_buffer, stderr);
    fputc('\n', stderr);
  }
  bool success = rc == CURLE_OK;
  long http_code = 0;
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code != (cfg.server_type == ServerType::InfluxDB ? 204 : 200)) {
    success = false;
    if (http_code == 0)
      Log->error("Failed to connect to server %s", cfg.server_url);
    else
      Log->error("Got HTTP status code %ld.", http_code);
    if (data.response_remain <= 0) data.response_remain = 1;
    response_buffer[SERVER_RESPONSE_BUFFER_SIZE-data.response_remain] = '\0';
    fputs(response_buffer, log);
    fputc('\n', log);
    fflush(log);
  } else if (rc == CURLE_ABORTED_BY_CALLBACK) {
    Log->error("HTTP request was aborted.");
  }
  curl_easy_cleanup(curl);
  if (headers != NULL) curl_slist_free_all(headers);
  if (verbose) fputs("===> return from send()\n", stderr);
  return success;
}



