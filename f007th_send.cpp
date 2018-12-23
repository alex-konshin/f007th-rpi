/*
 * F007TH_push_to_rest.cpp
 *
 * Push sensors readings to remote server via REST API
 *
 * Created on: February 4, 2017
 * @author Alex Konshin <akonshin@gmail.com>
 */

#include "RFReceiver.hpp"
#include <curl/curl.h>

#include "SensorsData.hpp"

#define SERVER_TYPE_REST 0

#define to_str(x) #x
#define DEFAULT_PIN_STR to_str(DEFAULT_PIN)


enum ServerType {STDOUT, REST, InfluxDB};

static bool send(ReceivedMessage& message, const char* url, ServerType server_type, int changed, char* data_buffer, char* response_buffer, int options, FILE* log);

static void help() {
  fputs(
    "(c) 2017-2018 Alex Konshin\n"\
    "Receive data from sensors Ambient Weather F007TH then print it to stdout or send it to remote server via REST API.\n\n"\
    "--gpio, -g\n"\
    "    Value is GPIO pin number (default is "DEFAULT_PIN_STR") as defined on page http://abyz.co.uk/rpi/pigpio/index.html\n"\
    "--celsius, -C\n"\
    "    Output temperature in degrees Celsius.\n"\
    "--utc, -U\n"\
    "    Timestamps are printed/sent in ISO 8601 format.\n"\
    "--local-time, -L\n"\
    "    Timestamps are printed/sent in format \"YYYY-mm-dd HH:MM:SS TZ\".\n"\
    "--send-to, -s\n"\
    "    Parameter value is server URL.\n"\
    "--server-type, -t\n"\
    "    Parameter value is server type. Possible values are REST (default) or InfluxDB.\n"\
    "--all, -A\n"\
    "    Send all data. Only changed and valid data is sent by default.\n"\
    "--log-file, -l\n"\
    "    Parameter is a path to log file.\n"\
    "--verbose, -v\n"\
    "    Verbose output.\n"\
    "--more_verbose, -V\n"\
    "    More verbose output.\n",
    stderr
  );
  fflush(stderr);
  fclose(stderr);
  exit(1);
}

int main(int argc, char *argv[]) {

  if ( argc==1 ) help();

  const char* log_file_path = NULL; // TODO real logging is not implemented yet
  const char* server_url = NULL;
  int gpio = DEFAULT_PIN;
  ServerType server_type = REST;
  int options = 0;
  unsigned protocols = 0;

  bool changes_only = true;
  bool tz_set = false;
  bool type_is_set = false;

  const char* short_options = "g:s:Al:vVt:TCULd7068D";
  const struct option long_options[] = {
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
      { "f007th", no_argument, NULL, '7' },
      { "00592txr", no_argument, NULL, '0' },
      { "tx6", no_argument, NULL, '6' },
      { "hg02832", no_argument, NULL, '8' },
      { "DEBUG", no_argument, NULL, 'D' },
      { NULL, 0, NULL, 0 }
  };

  long long_value;
  while (1) {
    int c = getopt_long(argc, argv, short_options, long_options, NULL);
    if (c == -1) break;

    switch (c) {

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

    case '0': // AcuRite 00592TXR
      protocols |= PROTOCOL_00592TXR;
      break;
    case '3': // LaCrosse TX3/TX6/TX7
      protocols |= PROTOCOL_TX7U;
      break;
    case '7': // Ambient Weather F007TH
      protocols |= PROTOCOL_F007TH;
      break;
    case '8': // Auriol HG02832
      protocols |= PROTOCOL_HG02832;
      break;
    case 'D': // debug receiving
      protocols |= PROTOCOL_ALL;
      break;

    case '?':
      help();
      break;

    default:
      fprintf(stderr, "ERROR: Unknown option \"%s\".\n", argv[optind]);
      help();
    }
  }

  if (optind < argc) {
    if (optind != argc-1) help();
    server_url = argv[optind];
  }

  if (server_url == NULL || server_url[0] == '\0') {
    if (type_is_set && server_type != STDOUT) {
      fputs("ERROR: Server URL must be specified (options --send-to or -s).\n", stderr);
      help();
    }
    server_type = STDOUT;
  } else {
    //TODO support UNIX sockets for InfluxDB
    if (strncmp(server_url, "http://", 7) != 0 && strncmp(server_url, "https://", 8)) {
      fputs("ERROR: Server URL must be HTTP or HTTPS.\n", stderr);
      help();
    }
  }

  if (log_file_path == NULL || log_file_path[0]=='\0') {
    log_file_path = "f007th-send.log";
  }

  FILE* log = fopen(log_file_path, "w+");


  char* response_buffer = NULL;
  char* data_buffer = (char*)malloc(SEND_DATA_BUFFER_SIZE*sizeof(char));

  if (server_type!=STDOUT) {
    curl_global_init(CURL_GLOBAL_ALL);
    response_buffer = (char*)malloc(SERVER_RESPONSE_BUFFER_SIZE*sizeof(char));
  }

  SensorsData sensorsData;

  RFReceiver receiver(gpio);
  Log->setLogFile(log);

  if (protocols != 0) receiver.setProtocols(protocols);

  ReceivedMessage message;

  receiver.enableReceive();

  if ((options&VERBOSITY_PRINT_STATISTICS) != 0)
    receiver.printStatisticsPeriodically(1000); // print statistics every second

  if ((options&VERBOSITY_INFO) != 0) fputs("Receiving data...\n", stderr);
  while(!receiver.isStopped()) {

    if (receiver.waitForMessage(message)) {
      if (receiver.isStopped()) break;

      if ((options&VERBOSITY_INFO) != 0) {
        message.print(stdout, options);
        if (message.print(log, options)) fflush(log);
      }

      if (message.isEmpty()) {
        fputs("ERROR: Missing data.\n", stderr);
      } else if (message.isUndecoded()) {
        if ((options&VERBOSITY_INFO) != 0)
          fprintf(stderr, "Could not decode the received data (error %04x).\n", message.getDecodingStatus());
      } else {
        bool isValid = message.isValid();
        int changed = isValid ? sensorsData.update(message.getSensorData()) : 0;
        if (changed == 0 && !changes_only && (isValid || server_type!=InfluxDB))
          changed = TEMPERATURE_IS_CHANGED | HUMIDITY_IS_CHANGED | BATTERY_STATUS_IS_CHANGED;
        if (changed != 0) {
          if (server_type==STDOUT) {
            if ((options&VERBOSITY_INFO) == 0) // already printed
              message.print(stdout, options);
          } else {
            if (!send(message, server_url, server_type, changed, data_buffer, response_buffer, options, log) && (options&VERBOSITY_INFO) != 0)
              Log->info("No data was sent to server.");
          }
        } else {
          if ((options&VERBOSITY_INFO) != 0) {
            if (server_type==STDOUT) {
              if (!isValid)
                fputs("Data is corrupted.\n", stderr);
              else
                fputs("Data is not changed.\n", stderr);
            } else {
              if (!isValid)
                fputs("Data is corrupted and is not sent to server.\n", stderr);
              else
                fputs("Data is not changed and is not sent to server.\n", stderr);
            }
          }
        }
      }

    }

    if (receiver.checkAndResetTimerEvent()) {
      receiver.printStatistics();
    }
  }
  if ((options&VERBOSITY_INFO) != 0) fputs("\nExiting...\n", stderr);

  // finally
  free(data_buffer);
  if (response_buffer != NULL) free(response_buffer);
  fclose(log);
  if (server_type!=STDOUT) curl_global_cleanup();

  exit(0);
}

typedef struct PutData {
  uint8_t* data;
  size_t len;
  uint8_t* response_data;
  size_t response_len;
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
  size_t to_copy = (data->response_len < curl_size) ? data->response_len : curl_size;
  if (data->verbose) fprintf(stderr, "receiving %d bytes...\n", (int)to_copy);
  if (to_copy > 0) {
    memcpy(data->response_data, ptr, to_copy);
    data->response_len -= to_copy;
    data->response_data += to_copy;
    return to_copy;
  }
  return size;
}

bool send(ReceivedMessage& message, const char* url, ServerType server_type, int changed, char* data_buffer, char* response_buffer, int options, FILE* log) {
  bool verbose = (options&VERBOSITY_PRINT_DETAILS) != 0;
  if (verbose) fputs("===> called send()\n", stderr);

  data_buffer[0] = '\0';
  int data_size;
  if (server_type == InfluxDB) {
    data_size = message.influxDB(data_buffer, SEND_DATA_BUFFER_SIZE, changed, options);
  } else {
    data_size = message.json(data_buffer, SEND_DATA_BUFFER_SIZE, options);
  }
  if (data_size <= 0) {
    if (verbose) fputs("===> return from send() without sending because output data was not generated\n", stderr);
    return false;
  }

  struct PutData data;
  data.data = (uint8_t*)data_buffer;
  data.len = data_size;
  data.response_data = (uint8_t*)response_buffer;
  data.response_len = SERVER_RESPONSE_BUFFER_SIZE;
  data.verbose = verbose;
  response_buffer[0] = '\0';

  CURL* curl = curl_easy_init();  // get a curl handle
  if (!curl) {
    Log->error("Failed to get curl handle.", stderr);
    if (verbose) fputs("===> return from send() without sending because failed to initialize curl\n", stderr);
    return false;
  }
  if ((options&VERBOSITY_PRINT_CURL) != 0) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  struct curl_slist *headers = NULL;
  if (server_type == REST) {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "Connection: close");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  } if (server_type == InfluxDB) {
    headers = curl_slist_append(headers, "Content-Type:"); // do not set content type
    headers = curl_slist_append(headers, "Accept:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
//  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  if (server_type == InfluxDB)
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
    Log->error("Sending data to %s failed: %s", url, curl_easy_strerror(rc));

  if (verbose && response_buffer[0] != '\0') {
    if (data.response_len <= 0) data.response_len = data.response_len-1;
    response_buffer[SERVER_RESPONSE_BUFFER_SIZE-data.response_len] = '\0';
    fputs(response_buffer, stderr);
    fputc('\n', stderr);
  }
  bool success = rc == CURLE_OK;
  long http_code = 0;
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code != (server_type == InfluxDB ? 204 : 200)) {
    success = false;
    if (http_code == 0)
      Log->error("Failed to connect to server %s", url);
    else
      Log->error("Got HTTP status code %ld.", http_code);
    if (verbose) {
      fputs(response_buffer, log);
      fputc('\n', log);
    }
    fprintf(log, "ERROR: Got HTTP status code %ld.\n", http_code);
    fputs(response_buffer, log);
    fputc('\n', log);
  } else if (rc == CURLE_ABORTED_BY_CALLBACK) {
    Log->error("HTTP request was aborted.");
  }
  curl_easy_cleanup(curl);
  if (headers != NULL) curl_slist_free_all(headers);
  if (verbose) fputs("===> return from send()\n", stderr);
  return success;
}



