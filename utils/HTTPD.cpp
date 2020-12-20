/*
 * HTTPD.cpp
 *
 *  Created on: December 2, 2020
 *      Author: Alex Konshin <akonshin@gmail.com>
 */

/*
 * Handling of HTTP requests
 */

#include <microhttpd.h>

// required for downloading files
#include <sys/stat.h>
#include <fcntl.h>
/*
#ifdef MHD_HAVE_LIBMAGIC
#include <magic.h>
#endif
*/

#include "../common/SensorsData.hpp"
#include "../common/Config.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include "HTTPD.hpp"

#define request_params(request_name) REQUEST_ ## request_name ## _ARGS
#define max_number_of_params(request_name) (sizeof(request_params(request_name))/sizeof(const char*))

static const char* request_params(temperature)[] = {
#define REQ_TEMPERATURE_PARAM_SCALE 0
  "scale"
};

static const char* request_params(temperature_history)[] = {
#define REQ_TEMPERATURE_HISTORY_PARAM_SCALE 0
  "scale",
#define REQ_TEMPERATURE_HISTORY_PARAM_UTC 1
  "utc",
//#define REQ_TEMPERATURE_HISTORY_PARAM_FROM 2
//  "from"
//#define REQ_TEMPERATURE_HISTORY_PARAM_TO 3
//  "to"
};

static const char* request_params(humidity_history)[] = {
#define REQ_HUMIDITY_HISTORY_PARAM_UTC 0
  "utc",
//#define REQ_HUMIDITY_HISTORY_PARAM_FROM 1
//  "from"
//#define REQ_HUMIDITY_HISTORY_PARAM_TO 2
//  "to"
};

// Max total length of request (without arguments)
#define MAX_URL 256
// Max length of request name
#define MAX_REQ_LEN 12
// Max number of defined parameters in all requests
#define MAX_NUMBER_OF_PARAMS 4


static MHD_Response* make_html_response(const char* html_text) {
  MHD_Response* response = MHD_create_response_from_buffer(strlen(html_text), (void*)html_text, MHD_RESPMEM_PERSISTENT);
  (void) MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
  return response;
}

#define html_error_response(name,error_code,html_text) \
static struct MHD_Response* name ## _response = NULL; \
static int error_ ## name (struct MHD_Connection* connection) { \
  if (name ## _response == NULL) name ## _response = make_html_response(html_text); \
  return MHD_queue_response(connection, error_code, name ## _response); \
} \

#define destroy_html_error_response(name) { if (name ## _response != NULL) { MHD_destroy_response(name ## _response); name ## _response = NULL; } }

/**
 * Response returned for refused uploads.
 */
html_error_response(request_refused, MHD_HTTP_NOT_FOUND, "<html><head><title>Request refused</title></head><body>Request refused</body></html>");
html_error_response(bad_request, MHD_HTTP_BAD_REQUEST, "<html><head><title>Bad request</title></head><body>400: Bad request</body></html>");
html_error_response(data_not_found, MHD_HTTP_NOT_FOUND, "<html><head><title>Data not found</title></head><body>404: Data not found</body></html>");
html_error_response(not_supported, MHD_HTTP_NOT_FOUND, "<html><head><title>Not supported</title></head><body>404: Requested metric is not supported by the sensor</body></html>");
html_error_response(out_of_memory, MHD_HTTP_INSUFFICIENT_STORAGE, "<html><head><title>Out of memory</title></head><body>507: Out of memeory</body></html>");

static void destroy_html_responses() {
  destroy_html_error_response(request_refused);
  destroy_html_error_response(bad_request);
  destroy_html_error_response(data_not_found);
  destroy_html_error_response(not_supported);
}

//-------------------------------------------------------------
HTTPD::HTTPD(SensorsData* sensorsData, Config* cfg) {
  this->port = cfg->httpd_port;
  this->sensorsData = sensorsData;
  this->cfg = cfg;
  daemon = NULL;
  no_home_page = false;
}

HTTPD::~HTTPD() {
  stop();
  destroy_html_responses();
}

//-------------------------------------------------------------
struct ReqParamProcessingData {
  HTTPD* httpd;
  const char** param_names;
  const char** param_values;
  int max_params;
  bool success;
};

static int param_iterator(void* cls, enum MHD_ValueKind kind, const char* key, const char* value) {

  ReqParamProcessingData* req_param_processing_data = (ReqParamProcessingData*)cls;
  if ((req_param_processing_data->httpd->cfg->options&VERBOSITY_INFO) != 0) Log->log(" \"%s\" = \"%s\"", key, value);

  bool found = false;
  if (key != NULL && req_param_processing_data->param_names != NULL && req_param_processing_data->max_params > 0 ) {
    for (int param_index = 0; param_index<req_param_processing_data->max_params; param_index++) {
      const char* param_name = req_param_processing_data->param_names[param_index];
      if (key != NULL && strcmp(param_name, key) == 0) {
        found = true;
        req_param_processing_data->param_values[param_index] = value;
        break;
      }
    }
  }
  if (!found) req_param_processing_data->success = false;
  return MHD_YES;
}

static int process_params(HTTPD* httpd, struct MHD_Connection* connection, const char* p, const char** param_names, int max_params, const char** param_values, bool& success) {

  struct ReqParamProcessingData req_param_processing_data;
  req_param_processing_data.httpd = httpd;
  req_param_processing_data.param_names = param_names;
  req_param_processing_data.max_params = max_params;
  req_param_processing_data.param_values = param_values;
  req_param_processing_data.success = true;

  MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, &param_iterator, (void*)&req_param_processing_data);

  if (!req_param_processing_data.success) return error_bad_request(connection);

  success = true;
  return MHD_YES;
}

//-------------------------------------------------------------
static SensorData* find_requested_sensor_data(const char* name, SensorsData* sensorsData) {
  const char* p = name;
  if (sensorsData == NULL || p == NULL || *p=='\0') return NULL;
/*
  char ch;
  while ((ch=*p) != '\0') {
    if (ch == '?' || ch == '&') return NULL;
    if (ch == '/' ) {
      if (*(p+1) != '\0') return NULL;
      break;
    }
    p++;
  }
  size_t name_len = p-name;
  return sensorsData->find(name, name_len);
*/
  return sensorsData->find(name);
}

//-------------------------------------------------------------
typedef struct FileExt2Mime {
  const char* ext;
  const char* mime;
} FileExt2Mime;

static struct FileExt2Mime ext2mime[] = {
    { "html", "text/html" },
    { "htm", "text/html" },
    { "css", "text/css" },
    { "xml", "application/xml" },
    { "xhtml", "application/xhtml+xml" },
    { "js", "text/javascript" },
    { "json", "application/json" },
    { "txt", "text/plain" },
    { "png", "image/png" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "png", "image/png" },
    { "gif", "image/gif" },
    { "bmp", "image/bmp" },
    { "ttf", "font/ttf" },
    { "woff", "font/woff" },
    { "woff2", "font/woff2" },
    { "mp3", "audio/mpeg" },
    { "aac", "audio/aac" },
    { "mid", "audio/midi" },
    { "midi", "audio/midi" },
    { "wav", "audio/wav" },
    { "weba", "audio/webm" },
    { "webm", "video/webm" },
    { "webp", "image/webp" },
    { "zip", "application/zip" },
    { "gz", "application/gzip" },
    { "7z", "application/x-7z-compressed" },
    { NULL, NULL }
};


static const char* filepath2mime(const char* filepath) {
  if (filepath == NULL) return NULL;
  size_t pathlen = strlen(filepath);
  if (pathlen == 0) return NULL;
  size_t ext_len = 0;
  const char* p = filepath+pathlen;
  char ch;
  while (--p != filepath && (ch=*p) != '.') {
    if (ch == '/') return NULL; // no extension
    ext_len++;
  }
  const char* ext = p+1;

  struct FileExt2Mime* record = ext2mime;
  const char* recext;
  while ((recext=record->ext) != NULL) {
    if (strlen(recext) == ext_len && strcmp(ext,recext) == 0) return record->mime;
    record++;
  }
  return NULL;
}

//-------------------------------------------------------------
static const char* get_www_file_path(const char* www_root, const char* url) {
  if (www_root == NULL) return NULL;
  size_t www_root_len = strlen(www_root);
  size_t pathlen = strlen(url)+www_root_len+1;
  char* filepath = (char*)malloc(pathlen);
  if (filepath == NULL) return NULL;
  strcpy(filepath, www_root);
  strcpy(filepath+www_root_len, www_root[www_root_len-1] == '/' ? url+1 : url);
  return filepath;
}

//-------------------------------------------------------------
static int download_file(struct MHD_Connection * connection, const char* filepath) {
  size_t filesize = 0;
  int fd = open(filepath, O_RDONLY);
  if (fd != -1) {
    struct stat file_stat;
    if (fstat(fd, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
      close (fd);
      fd = -1;
    }
    filesize = file_stat.st_size;
  }
  if (fd == -1) return error_data_not_found(connection);

  const char *mime = filepath2mime(filepath);
/*
#ifdef MHD_HAVE_LIBMAGIC
  if (mime == NULL) {
    // read beginning of the file to determine mime type
    char file_data[MAGIC_HEADER_SIZE];
    ssize_t nbytes = read(fd, file_data, sizeof(file_data));
    lseek(fd, 0, SEEK_SET);
    if (nbytes != -1) mime = magic_buffer(magic, file_data, nbytes);
  }
 #endif
*/
  struct MHD_Response* response = MHD_create_response_from_fd(filesize, fd);
  if (response == NULL) {
    Log->error("Error on sending file \"%s\".", filepath);
    // internal error (i.e. out of memory)
    close(fd);
    return MHD_NO;
  }

  if (mime != NULL) MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime);
  int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  return ret;
}

//-------------------------------------------------------------
static int process_request(
    void* cls,
    struct MHD_Connection * connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** ptr
) {

  HTTPD* httpd = (HTTPD*)cls;
  SensorsData* sensorsData = httpd->sensorsData;
  Config* cfg = httpd->cfg;

  static int dummy;
  if (strcmp(method, "GET") != 0) return MHD_NO; // unexpected method
  if (&dummy != *ptr) {
    // The first time only the headers are valid, do not respond in the first round...
    *ptr = &dummy;
    return MHD_YES;
  }
  if (*upload_data_size != 0) return MHD_NO; // upload data in a GET!?

  if (url == NULL || *url != '/') return MHD_NO;

  if ((cfg->options&VERBOSITY_INFO) != 0) Log->log("GET \"%s\"", url);

  if (strlen(url) > MAX_URL) return error_bad_request(connection);

  *ptr = NULL; // clear context pointer

  void* buffer = __null;
  size_t buffer_size = 0;
  size_t data_size = 0;

  const char* p = url+1;
  int len = 0;
  char ch = '\0';
  bool is_query = true;
  bool processed = false;
  while ((ch=*p) >= 'a' && ch <='z') {
    if (++len >= MAX_REQ_LEN) {
      is_query = false;
      break;
    }
    p++;
  }

  if (ch == '?') return error_bad_request(connection);

  struct MHD_Response* response;

  if ( is_query && (ch == '/' || ch == '\0')) {
    const char* params[MAX_NUMBER_OF_PARAMS];
    //memset(params, MAX_NUMBER_OF_PARAMS, sizeof(const char*));
    for (int index=0; index<MAX_NUMBER_OF_PARAMS; index++) {
      params[index] = NULL;
    }

    if (len == 0 && ch =='\0') {
      if (!httpd->no_home_page) {

        const char* www_root = cfg->www_root;
        if (www_root != NULL) {
          // Try to show home page
          const char* home_page_file = get_www_file_path(www_root, "/index.html");
          if(home_page_file != NULL && access(home_page_file, R_OK) == 0) {
            int ret = download_file(connection, home_page_file);
            free((void*)home_page_file);
            return ret;
          }
        }

        httpd->no_home_page = true;
      }

      data_size = sensorsData->generateJson(buffer, buffer_size, RestRequestType::AllData);
      processed = true;

#define is_req(req, str, len) (strlen(req) == len && strncmp(req, str, len) == 0)
    } else if (is_req("temperature", url+1, len)) {


      if (ch == '/' && *++p != '\0') {
        // temperature history

        bool success = false;
        int result = process_params(httpd, connection, p, request_params(temperature_history), max_number_of_params(temperature_history), params, success);
        if (!success) return result;

        bool x10;
        bool requested_celsius;
        const char* value = params[REQ_TEMPERATURE_HISTORY_PARAM_SCALE];
        if (value != NULL) {
          if (strcmp("F10", value) == 0) {
            requested_celsius = false;
            x10 = true;
          } else if (strcmp("C10", value) == 0) {
            requested_celsius = true;
            x10 = true;
          } else if (strcmp("F", value) == 0) {
            requested_celsius = false;
            x10 = false;
          } else if (strcmp("C", value) == 0) {
            requested_celsius = true;
            x10 = false;
          } else {
            return error_bad_request(connection);
          }
        } else {
          requested_celsius = (sensorsData->getOptions()&OPTION_CELSIUS) != 0;
          x10 = false;
        }

        SensorData* sensorData = find_requested_sensor_data(p, sensorsData);
        if (sensorData == NULL) return error_data_not_found(connection);
        if (!sensorData->hasTemperature()) return error_not_supported(connection);

        bool value_is_celcius = sensorData->isRawTemperatureCelsius();

        ValueConversion convertion;
        if (value_is_celcius == requested_celsius) {
          convertion = ValueConversion::None;
        } else if (requested_celsius) {
          convertion = ValueConversion::F2C;
        } else {
          convertion = ValueConversion::C2F;
        }

        bool time_UTC = (sensorsData->getOptions()&OPTION_UTC) != 0;
        value = params[REQ_TEMPERATURE_HISTORY_PARAM_UTC];
        if (value != NULL && *value != '\0') {
          if (strcmp("0", value) == 0) {
            time_UTC = false;
          } else if (strcmp("1", value) == 0) {
            time_UTC = true;
          } else {
            return error_bad_request(connection);
          }
        }

        time_t from = 0; // TODO
        time_t to = 0; // TODO

        data_size = sensorData->temperatureHistory.generateJson(from, to, buffer, buffer_size, convertion, x10, time_UTC);
        processed = true;

      } else {
        // current temperature from all defined sensors

        bool success = false;
        int result = process_params(httpd, connection, p, request_params(temperature), max_number_of_params(temperature), params, success);
        if (!success) return result;

        RestRequestType requestType;
        const char* value = params[REQ_TEMPERATURE_PARAM_SCALE];
        if (value != NULL) {
          if (strcmp("F10", value) == 0) {
            requestType = RestRequestType::TemperatureF10;
          } else if (strcmp("C10", value) == 0) {
            requestType = RestRequestType::TemperatureC10;
          } else if (strcmp("F", value) == 0) {
            requestType = RestRequestType::TemperatureF;
          } else if (strcmp("C", value) == 0) {
            requestType = RestRequestType::TemperatureC;
          } else {
            return error_bad_request(connection);
          }
        } else {
          requestType = (sensorsData->getOptions()&OPTION_CELSIUS) != 0 ? RestRequestType::TemperatureC10 : RestRequestType::TemperatureF10;
        }

        // The current temperature from all defined sensors
        data_size = sensorsData->generateJson(buffer, buffer_size, requestType);
        processed = true;
      }

    } else if (is_req("humidity", url+1, len)) {
      if (ch == '/' && *++p != '\0') {
        // humidity history

        bool success = false;
        int result = process_params(httpd, connection, p, request_params(humidity_history), max_number_of_params(humidity_history), params, success);
        if (!success) return result;

        SensorData* sensorData = find_requested_sensor_data(p, sensorsData);
        if (sensorData == NULL) return error_data_not_found(connection);
        if (!sensorData->hasHumidity()) return error_not_supported(connection);

        bool time_UTC = (sensorsData->getOptions()&OPTION_UTC) != 0;
        const char* value = params[REQ_HUMIDITY_HISTORY_PARAM_UTC];
        if (value != NULL && *value != '\0') {
          if (strcmp("0", value) == 0) {
            time_UTC = false;
          } else if (strcmp("1", value) == 0) {
            time_UTC = true;
          } else {
            return error_bad_request(connection);
          }
        }

        time_t from = 0; // TODO
        time_t to = 0; // TODO

        data_size = sensorData->humidityHistory.generateJson(from, to, buffer, buffer_size, ValueConversion::None, false, time_UTC);
        processed = true;

      } else {
        // The current humidity from all defined sensors that supports this metric

        bool success = false;
        int result = process_params(httpd, connection, p, /*request_params(humidity)*/NULL, /*max_number_of_params(humidity)*/0, params, success);
        if (!success) return result;

        data_size = sensorsData->generateJson(buffer, buffer_size, RestRequestType::Humidity);
        processed = true;
      }

    } else if (is_req("sensors", url+1, len)) {
      if (ch == '\0') {
        bool success = false;
        int result = process_params(httpd, connection, p, /*request_params(sensors)*/NULL, /*max_number_of_params(sensors)*/0, params, success);
        if (!success) return result;

        // current data from all defined sensors
        data_size = sensorsData->generateJson(buffer, buffer_size, RestRequestType::AllData);
        processed = true;
      } else {
        // TODO current data from the specific sensor
        return error_bad_request(connection);
      }

    } else if (is_req("version", url+1, len)) {

      response = MHD_create_response_from_buffer(2, (void*)RF_RECEIVER_VERSION, MHD_RESPMEM_PERSISTENT);
      MHD_add_response_header(response, "Content-Type", "text/plain");

      int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
      MHD_destroy_response(response);
      return ret;
    }
  }
  if (!processed) {
    // This is not REST request but it could be request for some file in folder www_root

    const char* www_root = cfg->www_root;
    if (www_root == NULL) return error_request_refused(connection);

    size_t len = strlen(url);
    if (
      url[1] =='/' ||
      url[len-1] == '/' ||
      (url[len-1] == '.' && url[len-2] =='/') || // ends with "/."
      (len>=3 && url[len-1] == '.' && url[len-2] == '.' && url[len-3] == '/') ||  // ends with "/.."
      strstr(url, "/../") != NULL ||
      strstr(url, "/./") != NULL ||
      strstr(url, "//") != NULL
    ) {
      Log->error("Bad request: %s", url);
      return error_bad_request(connection);
    }
    for (size_t i=0; i<len; i++) {
      if ((unsigned)(ch=url[i]) < 32 || ch == '*' || ch == '?' || ch == '|' || ch == '<' || ch == '>') {
        Log->error("Bad request: %s", url);
        return error_bad_request(connection);
      }
    }
    const char* filepath = get_www_file_path(www_root, url);
    if (filepath == NULL) return error_out_of_memory(connection);

    //Log->info("  file = \"%s\"", filepath);
    int ret = download_file(connection, filepath);

    free((void*)filepath);
    return ret;
  }

  if (data_size == 0)
    response = MHD_create_response_from_buffer(2, (void*)"[]", MHD_RESPMEM_PERSISTENT);
  else
    response = MHD_create_response_from_buffer(data_size, buffer, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(response, "Content-Type", "application/json");

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

//-------------------------------------------------------------
HTTPD* HTTPD::start(SensorsData* sensorsData, Config* cfg) {
  if (sensorsData == NULL || cfg == NULL) return NULL;
  HTTPD* httpd = new HTTPD(sensorsData, cfg);

  int port = cfg->httpd_port;
  if (port >= MIN_HTTPD_PORT && port<65535) {
    httpd->daemon =
      MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION,
        port,
        NULL,
        NULL,
        &process_request,
        (void*)httpd,
        MHD_OPTION_END
      );
  }

  return httpd;
}

void HTTPD::destroy(HTTPD*& httpd) {
  if ( httpd != NULL) {
    if ((httpd->cfg->options&VERBOSITY_INFO) != 0) Log->log("Stopping HTTPD server...");
    httpd->stop();
    delete httpd;
    httpd = NULL;
  }
}

//-------------------------------------------------------------
void HTTPD::stop() {
  if (daemon != NULL) {
    MHD_stop_daemon(daemon);
    daemon = NULL;
    destroy_html_responses();
  }
}

