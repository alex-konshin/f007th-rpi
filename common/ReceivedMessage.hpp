/*
 * ReceivedMessage.hpp
 *
 *  Created on: January 27, 2017
 *      Author: Alex Konshin
 */

#ifndef RECEIVEDMESSAGE_HPP_
#define RECEIVEDMESSAGE_HPP_

#include "SensorsData.hpp"
#include "Config.hpp"

#define SEND_DATA_BUFFER_SIZE 2048
#define SERVER_RESPONSE_BUFFER_SIZE 8192

typedef struct ReceivedData {
  struct ReceivedData *next;
  int16_t* pSequence;
  uint32_t uSequenceStartTime;
  SensorData sensorData;

  long protocol_tried_manchester;
  uint16_t decodingStatus;
  uint16_t decodedBits;

  uint16_t detailedDecodingStatus[NUMBER_OF_PROTOCOLS];
  uint16_t detailedDecodedBits[NUMBER_OF_PROTOCOLS];

  int16_t iSequenceSize;

} ReceivedData;


class ReceivedMessage {
public:
  ReceivedData* data;
  time_t data_time;

private:
  bool is_sensor_def_set = false;

#define T2D_BUFFER_SIZE 13
#define ID_BUFFER_SIZE 512

  char* t2d(int t, char* buffer) {
    char* p = buffer+T2D_BUFFER_SIZE-1;
    p[0] = '\0';
    bool negative = t<0;
    if (negative) t = -t;
    int d = t%10;
    if (d!=0) {
      *--p = ('0'+d);
      *--p = '.';
    }
    t = t/10;
    do {
      d = t%10;
      *--p = ('0'+d);
      t = t/10;
    } while (t > 0);
    if (negative) *--p = '-';
    return p;
  }


public:
  ReceivedMessage() {
    data_time = time(NULL);
    data = __null;
  }
  ReceivedMessage(ReceivedData* data) {
    data_time = time(NULL);
    this->data = data;
  }

  ~ReceivedMessage() {
    ReceivedData* data = this->data;
    if (data != NULL) {
      this->data = NULL;
      free((void*)data);
    }
  }

  void setData(ReceivedData* data) {
    ReceivedData* oldData = this->data;
    if (oldData != NULL) {
      this->data = NULL;
      free((void*)oldData);
    }
    this->data = data;
    data_time = time(NULL);
    if (data != NULL) data->sensorData.data_time = data_time;
    is_sensor_def_set = false;
  }

  SensorData* getSensorData() {
    return &data->sensorData;
  }

  time_t getTime() {
    return data_time;
  }

  inline bool isEmpty() { return data == __null; }

  inline bool isValid() {
    return data != __null && data->decodingStatus == 0;
  }
  inline bool isUndecoded() {
    return data != __null && (data->decodingStatus & 0x3f) != 0;
  }
  inline uint16_t getDecodingStatus() {
    return data == __null ? -1 : data->decodingStatus;
  }

  const char* getSensorTypeName() {
    return data == __null ? __null : data->sensorData.getSensorTypeName();
  }

  // random number that is changed when battery is changed
  uint8_t getRollingCode() {
    return data == __null ? 0 : data->sensorData.getRollingCode();
  }

  // true => OK
  bool getBatteryStatus() {
    return data != __null && data->sensorData.getBatteryStatus();
  }

  int getChannel() {
    return data == __null ? -1 : data->sensorData.getChannel();
  }
  int getChannelNumber() {
    return data == __null ? -1 : data->sensorData.getChannelNumber();
  }
  const char* const getChannelName() {
    return data == __null ? NULL : data->sensorData.getChannelName();
  }

  bool hasHumidity() {
    return data != __null && data->sensorData.hasHumidity();
  }
  bool hasTemperature() {
    return data != __null && data->sensorData.hasTemperature();
  }
  bool hasBatteryStatus() {
    return data != __null && data->sensorData.hasBatteryStatus();
  }

  int getTemperatureCx10() {
    return data == __null ? -2732 : data->sensorData.getTemperatureCx10();
  }

  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10() {
    return data == __null ? -4597 : data->sensorData.getTemperatureFx10();
  }

  // Relative Humidity, 0..100%
  int getHumidity() {
    return data == __null ? 0 : data->sensorData.getHumidity();
  }

  bool printInputSequence(FILE* file) {
    if (data == __null || data->pSequence ==__null) return false;

    if (file == __null) file = stdout;
    // print input sequence

    fprintf(file, "sequence size=%d:", data->iSequenceSize);
    for (int index=0; index<data->iSequenceSize; index++ ) {
      if (index != 0) fputc(',', file);
      fprintf(file, " %d", data->pSequence[index]);
    }
    fputc('\n', file);

    return true;
  }

  static void printBits(FILE* file, Bits* bits) {
    fputs("   ==> ", file);
    int size = bits->getSize();
    for (int index=0; index<size; index++ ) {
      fputc(bits->getBit(index) ? '1' : '0', file);
    }
    fputc('\n', file);
  }

  static void printBits(Bits& bits) {
    char buffer[512];
    int size = bits.getSize();
    if (size>500) size = 500;
    int out = 0;
    for (int index=0; index<size; index++ ) {
      if (index!=0 && (index&3)==0) buffer[out++] = ' ';
      buffer[out++] = bits.getBit(index) ? '1' : '0';
    }
    buffer[out++] = '\0';
    Log->info( "   ==> %s", &buffer );
  }

  SensorDef* getSensorDef() {
    if (data == NULL) return NULL;
    if (!is_sensor_def_set) {
      is_sensor_def_set = true;
      return data->sensorData.def = SensorDef::find(data->sensorData.getId());
    }
    return data->sensorData.def;
  }

  bool print(FILE* file, int options) {
    if (data == NULL) return false;

    bool print_details = (options&VERBOSITY_PRINT_DETAILS) != 0;
    bool print_undecoded = (options&VERBOSITY_PRINT_UNDECODED) != 0;

    if (file == NULL) file = stdout;

    // print timestamp
    char dt[TIME2STR_BUFFER_SIZE];
    convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);

    uint16_t decodingStatus = data->decodingStatus;

    if (print_details || (print_undecoded && (decodingStatus & 0x3f) != 0)) {
      fprintf(file, "%s ", dt);
      printInputSequence(file);
    }

    if ((decodingStatus & 0x3f) == 0 ) {

      if (print_details) {
        data->sensorData.printRawData(file);
      } else {
        fputs(dt, file);
        fputc('\n', file);
      }

      if ((decodingStatus & 0xc0) != 0 ) {
        if ((decodingStatus & 0x80) != 0 )
          fprintf(file, "  *** failed checksum (error %04x) ***\n", decodingStatus);
        else
          fprintf(file, "  *** no checksum (error %04x) ***\n", decodingStatus);
      }

      SensorDef* sensor_def = getSensorDef();
      if (sensor_def != NULL && sensor_def->name != NULL) {
        fprintf(file, "  name              = %s\n", sensor_def->name);
      }
      //fprintf(file, "  id                = %08x\n", data->sensorData.getId());
      fprintf(file, "  type              = %s\n", data->sensorData.getSensorTypeLongName());

      const char* channel = getChannelName();
      if (channel != NULL) fprintf(file, "  channel           = %s\n", channel);

      fprintf(file, "  rolling code      = %02x\n", getRollingCode());
      if (hasTemperature()) {
        int temperature = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
        char t2d_buffer[T2D_BUFFER_SIZE];
        fprintf(file, "  temperature       = %s%c\n", t2d(temperature, t2d_buffer), (options&OPTION_CELSIUS) != 0 ? 'C' : 'F');
      }
      if (hasHumidity()) {
        fprintf(file, "  humidity          = %d%%\n", getHumidity());
      }
      if (hasBatteryStatus()) {
        fprintf(file, "  battery           = %s\n", getBatteryStatus() ? "OK" :"Bad");
      }

    } else if (print_undecoded) {
      fputs(dt, file);
      fputc('\n', file);
      fprintf(file, "  decoding status = %04x(", decodingStatus );
      for (int proto_index = 0; proto_index<NUMBER_OF_PROTOCOLS; proto_index++) {
        if (proto_index != 0) fputc(',', file);
        fprintf(file, "%04x", data->detailedDecodingStatus[proto_index] );
      }
      fprintf(file, ")\n  decoded bits = %d\n", data->decodedBits);
    }

    return true;
  }

  bool json(FILE* file, int options) {
    if (data == NULL) return false;

    if (file == NULL) file = stdout;

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) == 0 ) {

      bool first = true;
      char dt[TIME2STR_BUFFER_SIZE];
      const char* timestr = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);
      if (timestr != NULL) {
        fprintf( file, "{\"time\":\"%s\"", dt );
        first = false;
      }

      if ((decodingStatus & 0xc0) != 0 ) {
        fprintf(file, "%c\"valid\":false,", first?'{':',');
        first = false;
      }

      SensorDef* sensor_def = getSensorDef();
      if (sensor_def != NULL && sensor_def->name != NULL) {
        fprintf(file, "%c\"name\":%s", first?'{':',', sensor_def->quoted);
        first = false;
      }

      fprintf(file, "%c\"type\":\"%s\"", first?'{':',', data->sensorData.getSensorTypeLongName());

      int channel = getChannelNumber();
      if (channel >= 0) fprintf(file, ",\"channel\":%d", channel);
      fprintf(file, ",\"rolling_code\":%d", getRollingCode());
      if (hasTemperature()) fprintf(file, ",\"temperature\":%d", (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10());
      if (hasHumidity()) fprintf(file, ",\"humidity\":%d", getHumidity());
      if (hasBatteryStatus()) fprintf(file, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false" );
      fputs("}\n", file);

    }

    return true;
  }

  int json(char* buffer, size_t buflen, int options) {
    bool verbose = (options&VERBOSITY_PRINT_JSON) != 0;
    if (data == __null || buffer == __null || buflen < SEND_DATA_BUFFER_SIZE) {
      if (verbose) {
        if (data == __null)
          fputs("JSON data was not generated because no data was decoded.\n", stderr);
        else if (buffer == __null)
          fputs("ERROR: Invalid call ReceivedMessage::json(char*,size_t,bool,bool): buffer is not specified.\n", stderr);
        else
          fprintf(stderr, "ERROR: Invalid call of ReceivedMessage::json(char*,size_t,bool,bool): invalid size (%d) of the buffer.\n", (int)buflen);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) != 0 ) {
      if (verbose) fprintf(stderr, "JSON data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    char dt[TIME2STR_BUFFER_SIZE];
    const char* timestr = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);

    int len;
    if (timestr == NULL)
      len = snprintf( buffer, buflen, "{\"type\":\"%s\",\"valid\":%s", data->sensorData.getSensorTypeLongName(), decodingStatus == 0 ? "true" : "false");
    else
      len = snprintf( buffer, buflen, "{\"time\":\"%s\",\"type\":\"%s\",\"valid\":%s", timestr, data->sensorData.getSensorTypeLongName(), decodingStatus == 0 ? "true" : "false");
    SensorDef* sensor_def = getSensorDef();
    if (sensor_def != NULL && sensor_def->name != NULL) {
      len += snprintf(buffer+len, buflen-len, ",\"name\":%s", sensor_def->quoted);
    }

    int channel = getChannelNumber();
    if (channel >= 0) len += snprintf(buffer+len, buflen-len, ",\"channel\":%d", channel);
    len += snprintf(buffer+len, buflen-len, ",\"rolling_code\":%d", getRollingCode());
    if (hasTemperature()) len += snprintf(buffer+len, buflen-len, ",\"temperature\":%d", (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10());
    if (hasHumidity()) len += snprintf(buffer+len, buflen-len, ",\"humidity\":%d", getHumidity());
    if (hasBatteryStatus()) len += snprintf(buffer+len, buflen-len, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false");
    len += snprintf(buffer+len, buflen-len, "\n");

    buffer[len++] = '}';
    buffer[len++] = '\n';
    buffer[len] = '\0';

    if (verbose) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
    }

    return len*sizeof(unsigned char);
  }

  int influxDB(char* buffer, size_t buflen, int changed, int options) {
    bool verbose = (options&VERBOSITY_PRINT_JSON) != 0;
    if (data == __null || buffer == __null || buflen < SEND_DATA_BUFFER_SIZE) {
      if (verbose) {
        if (data == __null)
          fputs("Output data was not generated because no data was decoded.\n", stderr);
        else if (buffer == __null)
          fputs("ERROR: Invalid call influxDB(char*,size_t,bool): buffer is not specified.\n", stderr);
        else
          fprintf(stderr, "ERROR: Invalid call influxDB(char*,size_t,bool): invalid size (%d) of the buffer.\n", (int)buflen);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if (decodingStatus != 0 ) {
      if (verbose) fprintf(stderr, "Output data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    char* output;
    int remain;

    char t2d_buffer[T2D_BUFFER_SIZE];
    char id[ID_BUFFER_SIZE];
    int len;

    SensorDef* sensor_def = getSensorDef();
    if (sensor_def != NULL && sensor_def->name != NULL) {
      len = snprintf( id, sizeof(id),
        "name=%s,type=%s,channel=%d,rolling_code=%d",
        sensor_def->influxdb_quoted,
        getSensorTypeName(),
        getChannelNumber(),
        getRollingCode()
      );
    } else {
      len = snprintf( id, sizeof(id),
        "type=%s,channel=%d,rolling_code=%d",
        getSensorTypeName(),
        getChannelNumber(),
        getRollingCode()
      );
    }

    output = buffer;
    remain = buflen;
    if ((changed&TEMPERATURE_IS_CHANGED) != 0) {
      int t = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
      int len = snprintf( output, remain, "temperature,%s value=%s\n", id, t2d(t, t2d_buffer));
      remain -= len;
      output += len;
    }
    if ((changed&HUMIDITY_IS_CHANGED) != 0) {
      int len = snprintf( output, remain,
        "humidity,%s value=%d\n",
        id, getHumidity()
      );
      remain -= len;
      output += len;
    }
    if ((changed&BATTERY_STATUS_IS_CHANGED) != 0) {
      int len = snprintf( output, remain,
        "sensor_battery_status,%s value=%s\n",
        id, getBatteryStatus() ? "true" :"false"
      );
      remain -= len;
      output += len;
    }
    len = buflen - remain;

    if ((options&VERBOSITY_INFO) != 0) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
      fputc('\n', stderr);
    }

    return len*sizeof(unsigned char);
  }

  int update(SensorsData& sensorsData, time_t max_unchanged_gap) {
    getSensorDef(); // sets is_sensor_def_set = true;
    return sensorsData.update(&data->sensorData, data_time, max_unchanged_gap);
  }
};

#endif /* RECEIVEDMESSAGE_HPP_ */
