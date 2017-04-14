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

#define PROTOCOL_F007TH    1
#define PROTOCOL_00592TXR  2

#include "SensorsData.hpp"

#define SEND_DATA_BUFFER_SIZE 1024
#define SERVER_RESPONSE_BUFFER_SIZE 8192

#define VERBOSITY_DEBUG            1
#define VERBOSITY_INFO             2
#define VERBOSITY_PRINT_DETAILS    4
#define VERBOSITY_PRINT_STATISTICS 8
#define VERBOSITY_PRINT_UNDECODED 16
#define VERBOSITY_PRINT_JSON      32
#define VERBOSITY_PRINT_CURL      64

#define OPTION_CELSIUS           256
#define OPTION_UTC               512


typedef struct ReceivedData {
  struct ReceivedData *next;
  int16_t* pSequence;
  uint32_t uSequenceStartTime;
  SensorData sensorData;

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

  SensorData* getSensorData() {
    return &data->sensorData;
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
  uint8_t getRollingCode() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return (data->sensorData.nF007TH >> 24) & 255;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return data->sensorData.fields.rolling_code;
    }
    return 0;
  }

  // true => OK
  bool getBatteryStatus() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return (data->sensorData.nF007TH&0x00800000) == 0;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return data->sensorData.fields.status==0x44;
    }
    return false;
  }

  // Channel number 1..8
  static int getChannelF007TH(uint32_t nF007TH) {
    return ((nF007TH>>20)&7)+1;
  }
  int getChannelF007TH() {
    return data == __null ? -1 : ((data->sensorData.nF007TH>>20)&7)+1;
  }
  char getChannel00592TXR() {
    if (data != __null && data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
      switch ((data->sensorData.fields.channel>>6)&3) {
      case 3: return 'A';
      case 2: return 'B';
      case 0: return 'C';
      }
    }
    return '?';
  }
  int getChannel() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return ((data->sensorData.nF007TH>>20)&7)+1;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return ((data->sensorData.fields.channel>>6)&3)^3;
    }
    return -1;
  }
  int getChannelNumber() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return ((data->sensorData.nF007TH>>20)&7)+1;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
        switch ((data->sensorData.fields.channel>>6)&3) {
        case 3: return 1;
        case 2: return 2;
        case 0: return 3;
        }
      }
    }
    return -1;
  }

  int getTemperatureCx10() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH)
        return (((data->sensorData.nF007TH>>8)&4095)-720) * 5 / 9;

      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR)
        return ((((data->sensorData.fields.t_hi)&127)<<7) | (data->sensorData.fields.t_low&127)) - 1000;
    }
    return -2732;
  }

  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH)
        return ((data->sensorData.nF007TH>>8)&4095)-400;

      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
        int c = ((((data->sensorData.fields.t_hi)&127)<<7) | (data->sensorData.fields.t_low&127)) - 1000;
        return (c*9/5)+320;
      }
    }
    return -4597;
  }

  // Relative Humidity, 0..100%
  int getHumidity() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return data->sensorData.nF007TH&255;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return data->sensorData.fields.rh & 127;
    }
    return 0;
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

  bool print(FILE* file, int options) {
    bool print_details = (options&VERBOSITY_PRINT_DETAILS) != 0;
    bool print_undecoded = (options&VERBOSITY_PRINT_UNDECODED) != 0;

    if (data == __null) return false;

    if (file == __null) file = stdout;

    // print timestamp
    char dt[32];
    struct tm tm;
    if ((options&OPTION_UTC) != 0) {
      tm = *gmtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%FT%TZ", &tm); // ISO 8601 format
    } else {
      tm = *localtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm);
    }

    if (print_details) {
      fprintf(file, "%s ", dt);
      printInputSequence(file);
    }

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) == 0 ) {

      if (print_details) {
        if (data->sensorData.fields.protocol == PROTOCOL_F007TH)
          fprintf(file, "  F007TH data       = %08x\n", data->sensorData.nF007TH);
        else if (data->sensorData.fields.protocol == PROTOCOL_00592TXR)
          fprintf(file, "  00592TXR data     = %02x %02x %02x %02x %02x %02x %02x\n",
              data->sensorData.fields.channel, data->sensorData.fields.rolling_code, data->sensorData.fields.status, data->sensorData.fields.rh,
              data->sensorData.fields.t_hi, data->sensorData.fields.t_low, data->sensorData.fields.checksum);
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

      int temperature = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();

      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
        fprintf(file, "  type              = AcuRite 00592TXR\n  channel           = %c\n", getChannel00592TXR());

      } else if (data->sensorData.fields.protocol == PROTOCOL_F007TH) {
        fprintf(file, "  type              = Ambient Weather F007TH\n  channel           = %d\n", getChannelF007TH());
      }

      fprintf( file,
        "  rolling code      = %02x\n"\
        "  temperature       = %d.%c%c\n"\
        "  humidity          = %d%%\n"\
        "  battery           = %s\n",
        getRollingCode(),
        temperature/10, '0'+(temperature%10), (options&OPTION_CELSIUS) != 0 ? 'C' : 'F',
        getHumidity(),
        getBatteryStatus() ? "OK" :"Bad"
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


  bool json(FILE* file, int options) {
    if (data == __null) return false;

    if (file == __null) file = stdout;

    uint16_t decodingStatus = data->decodingStatus;
    if ((decodingStatus & 0x3f) == 0 ) {

      char dt[32];
      struct tm tm;
      if ((options&OPTION_UTC) != 0) {
        tm = *gmtime(&data_time); // convert time_t to struct tm
        strftime(dt, sizeof dt, "%FT%TZ", &tm); // ISO 8601 format
      } else {
        tm = *localtime(&data_time); // convert time_t to struct tm
        strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm);
      }
      fprintf( file, "{\"time\":\"%s\",", dt );

      if ((decodingStatus & 0xc0) != 0 ) {
        fputs("\"valid\":false,", file);
      }

      fprintf( file,
        "\"channel\":%d,"\
        "\"rolling_code\":%d,"\
        "\"temperature\":%d,"\
        "\"humidity\":%d,"\
        "\"battery_ok\":%s}\n",
        getChannel(),
        getRollingCode(),
        (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10(),
        getHumidity(),
        getBatteryStatus() ? "true" :"false"
      );

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
    if ((options&OPTION_UTC) != 0) { // UTC time zone
      tm = *gmtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%FT%TZ", &tm); // ISO format
    } else { // local time zone
      tm = *localtime(&data_time); // convert time_t to struct tm
      strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S %Z", &tm);
    }

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
      getChannel(),
      getRollingCode(),
      (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10(),
      getHumidity(),
      getBatteryStatus() ? "true" :"false"
    );

    if (verbose) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
      fputc('\n', stderr);
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
          fprintf(stderr, "ERROR: Invalid call influxDB(char*,size_t,bool): invalid size (%d) of the buffer.\n", buflen);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if (decodingStatus != 0 ) {
      if (verbose) fprintf(stderr, "Output data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    char id[60];
    int len = snprintf( id, sizeof(id),
      "type=F007TH,channel=%d,rolling_code=%d",
      getChannelNumber(),
      getRollingCode()
    );

    char* output = buffer;
    int remain = buflen;
    if ((changed&TEMPERATURE_IS_CHANGED) != 0) {
      int t = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
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

    if (verbose) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
      fputc('\n', stderr);
    }

    return len*sizeof(unsigned char);
  }

};

#endif /* RECEIVEDMESSAGE_HPP_ */
