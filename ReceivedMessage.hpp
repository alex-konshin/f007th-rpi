/*
 * ReceivedMessage.hpp
 *
 *  Created on: January 27, 2017
 *      Author: Alex Konshin
 */

#ifndef RECEIVEDMESSAGE_HPP_
#define RECEIVEDMESSAGE_HPP_

#include <stdio.h>
#include <time.h>

#include "SensorsData.hpp"

#define SEND_DATA_BUFFER_SIZE 1024
#define SERVER_RESPONSE_BUFFER_SIZE 8192

#define VERBOSITY_INFO             1
#define VERBOSITY_PRINT_DETAILS    2
#define VERBOSITY_PRINT_STATISTICS 4
#define VERBOSITY_PRINT_UNDECODED  8
#define VERBOSITY_PRINT_JSON      16
#define VERBOSITY_PRINT_CURL      32

typedef struct ReceivedData {
  struct ReceivedData *next;
  int16_t* pSequence;
  uint32_t uSequenceStartTime;
  uint32_t nF007TH;

  uint16_t decodingStatus;
  uint16_t decodedBits;

  int16_t iSequenceSize;

} ReceivedData;


class ReceivedMessage {
private:
  ReceivedData* data;
  time_t data_time;

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
    if (data != __null) {
      this->data = __null;
      free((void*)data);
    }
  }

  void setData(ReceivedData* data) {
    ReceivedData* oldData = this->data;
    if (oldData != __null) {
      this->data = __null;
      free((void*)oldData);
    }
    this->data = data;
    data_time = time(NULL);
  }

  uint32_t getData() {
    return data->nF007TH;
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

  // random number that is changed when battery is changed
  static uint8_t getRollingCodeF007TH(uint32_t nF007TH) {
    return (nF007TH >> 24) & 255;
  }
  uint8_t getRollingCodeF007TH() {
    return data == __null ? 0 : (data->nF007TH >> 24) & 255;
  }

  // true => OK
  static bool getBatteryStatusF007TH(uint32_t nF007TH) {
    return (nF007TH&0x00800000) == 0;
  }
  bool getBatteryStatusF007TH() {
    return data != __null && (data->nF007TH&0x00800000) == 0;
  }

  // Channel number 1..8
  static int getChannelF007TH(uint32_t nF007TH) {
    return ((nF007TH>>20)&7)+1;
  }
  int getChannelF007TH() {
    return data == __null ? -1 : ((data->nF007TH>>20)&7)+1;
  }

  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  static int getTemperatureF007TH(uint32_t nF007TH) {
    return ((nF007TH>>8)&4095)-400;
  }
  int getTemperatureF007TH() {
    return data == __null ? -4597 : ((data->nF007TH>>8)&4095)-400;
  }

  // Relative Humidity, 0..100%
  static int getHumidityF007TH(uint32_t nF007TH) {
    return nF007TH&255;
  }
  int getHumidityF007TH() {
    return data == __null ? 0 : data->nF007TH&255;
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

  bool print(FILE* file, int verbosity) {
    bool print_details = (verbosity&VERBOSITY_PRINT_DETAILS) != 0;
    bool print_undecoded = (verbosity&VERBOSITY_PRINT_UNDECODED) != 0;

    if (data == __null) return false;

    if (file == __null) file = stdout;

    // print timestamp

    char dt[32];
    struct tm tm;
    tm = *localtime(&data_time); // convert time_t to struct tm
    strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm); // format

    if (print_details) {
      fprintf(file, "%s ", dt);
      printInputSequence(file);
    }

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) == 0 ) {
      uint32_t nF007TH = data->nF007TH;

      if (print_details) {
        fprintf(file, "  F007TH data       = %08x\n", nF007TH);
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

      fprintf( file,
        "  channel           = %d\n"\
        "  rolling code      = %02x\n"\
        "  temperature*10 F  = %d\n"\
        "  humidity          = %d%%\n"\
        "  battery           = %s\n",
        getChannelF007TH(nF007TH),
        getRollingCodeF007TH(nF007TH),
        getTemperatureF007TH(nF007TH),
        getHumidityF007TH(nF007TH),
        getBatteryStatusF007TH(nF007TH) ? "OK" :"Bad"
      );

    } else if (print_undecoded) {
      fputs(dt, file);
      fputc('\n', file);
      fprintf(file,
        "  decoding status = %04x\n"\
        "  decoded bits = %d\n",
        decodingStatus,
        data->decodedBits
      );
      if ( (decodingStatus & 7)==0 ) {
        // Manchester decoding was successful
/*
        Bits bits(data->iSequenceSize+1);
        if (receiver.decodeManchester(data, bits)) {
          printf("   ==> ");
          int size = bits.getSize();
          for (int index=0; index<size; index++ ) {
            fputc(bits.getBit(index) ? '1' : '0', file);
          }
          fputc('\n', file);
        }
*/
      }
    }

    return true;
  }


  bool json(FILE* file, bool utc) {
    if (data == __null) return false;

    if (file == __null) file = stdout;

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) == 0 ) {

      char dt[32];
      struct tm tm;
      if (utc) {
        tm = *gmtime(&data_time); // convert time_t to struct tm
        strftime(dt, sizeof dt, "%FT%TZ", &tm); // ISO format
      } else {
        tm = *localtime(&data_time); // convert time_t to struct tm
        strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm);
      }
      fprintf( file, "{\"time\":\"%s\",", dt );

      uint32_t nF007TH = data->nF007TH;

      if ((decodingStatus & 0xc0) != 0 ) {
        fputs("\"valid\":false,", file);
      }

      fprintf( file,
        "\"channel\":%d,"\
        "\"rolling_code\":%d,"\
        "\"temperature\":%d,"\
        "\"humidity\":%d,"\
        "\"battery_ok\":%s}\n",
        getChannelF007TH(nF007TH),
        getRollingCodeF007TH(nF007TH),
        getTemperatureF007TH(nF007TH),
        getHumidityF007TH(nF007TH),
        getBatteryStatusF007TH(nF007TH) ? "true" :"false"
      );

    }

    return true;
  }

  int json(char* buffer, size_t buflen, bool utc, bool verbose) {
    if (data == __null || buffer == __null || buflen < SEND_DATA_BUFFER_SIZE) {
      if (verbose) {
        if (data == __null)
          fputs("JSON data was not generated because no data was decoded.\n", stderr);
        else if (buffer == __null)
          fputs("ERROR: Invalid call json(char*,size_t,bool,bool): buffer is not specified.\n", stderr);
        else
          fprintf(stderr, "ERROR: Invalid call json(char*,size_t,bool,bool): invalid size (%d) of the buffer.\n", buflen);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) != 0 ) {
      if (verbose) fprintf(stderr, "JSON data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    char dt[32];
    struct tm tm;
    if (utc) { // UTC time zone
      tm = *gmtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%FT%TZ", &tm); // ISO format
    } else { // local time zone
      tm = *localtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm);
    }

    uint32_t nF007TH = data->nF007TH;

    int len = snprintf( buffer, buflen,
      "{\"time\":\"%s\","\
      "\"valid\":%s,"\
      "\"channel\":%d,"\
      "\"rolling_code\":%d,"\
      "\"temperature\":%d,"\
      "\"humidity\":%d,"\
      "\"battery_ok\":%s}\n",
      dt,
      decodingStatus == 0 ? "true" : "false",
      getChannelF007TH(nF007TH),
      getRollingCodeF007TH(nF007TH),
      getTemperatureF007TH(nF007TH),
      getHumidityF007TH(nF007TH),
      getBatteryStatusF007TH(nF007TH) ? "true" :"false"
    );

    if (verbose) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
      fputc('\n', stderr);
    }

    return len*sizeof(unsigned char);
  }

  int influxDB(char* buffer, size_t buflen, int changed, bool verbose) {
    if (data == __null || buffer == __null || buflen < SEND_DATA_BUFFER_SIZE) {
      if (verbose) {
        if (data == __null)
          fputs("Output data was not generated because no data was decoded.\n", stderr);
        else if (buffer == __null)
          fputs("ERROR: Invalid call influxDB(char*,size_t,bool): buffer is not specified.\n", stderr);
        else
          fprintf(stderr, "ERROR: Invalid call influxDB(char*,size_t,bool): invalid size (%d) of the buffer.\n", buflen);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if (decodingStatus != 0 ) {
      if (verbose) fprintf(stderr, "Output data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    uint32_t nF007TH = data->nF007TH;

    char id[60];
    int len = snprintf( id, sizeof(id),
      "type=F007TH,channel=%d,rolling_code=%d",
      getChannelF007TH(nF007TH),
      getRollingCodeF007TH(nF007TH)
    );

    char* output = buffer;
    int remain = buflen;
    if ((changed&TEMPERATURE_IS_CHANGED) != 0) {
      int t = getTemperatureF007TH(nF007TH);
      int tF = t/10;
      char dF = '0'+(t%10);
      int len = snprintf( output, remain,
        "temperature,%s value=%d.%c\n",
        id, tF, dF
      );
      remain -= len;
      output += len;
    }
    if ((changed&HUMIDITY_IS_CHANGED) != 0) {
      int len = snprintf( output, remain,
        "humidity,%s value=%d\n",
        id, getHumidityF007TH(nF007TH)
      );
      remain -= len;
      output += len;
    }
    if ((changed&BATTERY_STATUS_IS_CHANGED) != 0) {
      int len = snprintf( output, remain,
        "sensor_battery_status,%s value=%s\n",
        id, getBatteryStatusF007TH(nF007TH) ? "true" :"false"
      );
      remain -= len;
      output += len;
    }
    len = buflen - remain;

    if (verbose) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
      fputc('\n', stderr);
    }

    return len*sizeof(unsigned char);
  }

};

#endif /* RECEIVEDMESSAGE_HPP_ */
