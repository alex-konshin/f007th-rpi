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
#define PROTOCOL_TX7U      4
#define PROTOCOL_HG02832   8
#define PROTOCOL_ALL       (unsigned)(-1)

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

  int getTX7temperature() {
    return
        ((data->sensorData.u32.low >> 20)&15)*100 +
        ((data->sensorData.u32.low >> 16)&15)*10 +
        ((data->sensorData.u32.low >> 12)&15);
  }
  int getTX7humidity() {
    return
        ((data->sensorData.u32.low >> 20)&15)*10 +
        ((data->sensorData.u32.low >> 16)&15);
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

  const char* getSensorTypeName() {
    if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return "F007TH";
    if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return "00592TXR";
    if (data->sensorData.fields.protocol == PROTOCOL_TX7U) return "TX7";
    if (data->sensorData.fields.protocol == PROTOCOL_HG02832) return "HG02832";
    return "UNKNOWN";
  }

  // random number that is changed when battery is changed
  uint8_t getRollingCode() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return (data->sensorData.nF007TH >> 24) & 255;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return data->sensorData.fields.rolling_code;
      if (data->sensorData.fields.protocol == PROTOCOL_TX7U) return (data->sensorData.u32.low >> 25);
      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) return (data->sensorData.u32.low >> 24);
    }
    return 0;
  }

  // true => OK
  bool getBatteryStatus() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return (data->sensorData.nF007TH&0x00800000) == 0;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return data->sensorData.fields.status==0x44;
      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) return (data->sensorData.u32.low&0x00008000) == 0;
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
  int getChannelHG02832() {
    return data == __null ? -1 : ((data->sensorData.u32.low>>12)&3)+1;
  }
  int getChannel() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return ((data->sensorData.nF007TH>>20)&7)+1;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return ((data->sensorData.fields.channel>>6)&3)^3;
      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) return ((data->sensorData.u32.low>>12)&3)+1;
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
      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) return ((data->sensorData.u32.low>>12)&3)+1;
    }
    return -1;
  }

  bool hasHumidity() {
    if ((data->sensorData.fields.protocol&(PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_HG02832)) != 0) return true;
    if (data->sensorData.fields.protocol == PROTOCOL_TX7U) return (data->sensorData.u32.hi&15)==14;
    return false;
  }
  bool hasTemperature() {
    if ((data->sensorData.fields.protocol&(PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_HG02832)) != 0) return true;
    if (data->sensorData.fields.protocol == PROTOCOL_TX7U) return (data->sensorData.u32.hi&15)==0;
    return false;
  }
  bool hasBatteryStatus() {
    if ((data->sensorData.fields.protocol&(PROTOCOL_F007TH|PROTOCOL_00592TXR|PROTOCOL_HG02832)) != 0) return true;
    return false;
  }

  int getTemperatureCx10() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH)
        return (int)(((data->sensorData.nF007TH>>8)&4095)-720) * 5 / 9;

      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR)
        return (int)((((data->sensorData.fields.t_hi)&127)<<7) | (data->sensorData.fields.t_low&127)) - 1000;

      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) {
        int result = (int)((data->sensorData.u32.low)&0x0fff);
        if ((result&0x0800) != 0) result |= 0xfffff000;
        return result;
      }

      if (data->sensorData.fields.protocol == PROTOCOL_TX7U) {
        if (hasTemperature()) return getTX7temperature()-500;
        //return -2732;
      }
    }
    return -2732;
  }

  // Temperature, dF = t*10(F). Ex: 72.5F = 725 dF
  int getTemperatureFx10() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH)
        return (int)((data->sensorData.nF007TH>>8)&4095)-400;

      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
        int c = (int)((((data->sensorData.fields.t_hi)&127)<<7) | (data->sensorData.fields.t_low&127)) - 1000;
        return (c*9/5)+320;
      }
      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) {
        return (getTemperatureCx10()*9/5)+320;
      }
      if (data->sensorData.fields.protocol == PROTOCOL_TX7U) {
        if (hasTemperature()) return ((getTX7temperature()-500)*9/5)+320;
        //return -4597;
      }
    }
    return -4597;
  }

  // Relative Humidity, 0..100%
  int getHumidity() {
    if (data != __null) {
      if (data->sensorData.fields.protocol == PROTOCOL_F007TH) return data->sensorData.nF007TH&255;
      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) return data->sensorData.fields.rh & 127;
      if (data->sensorData.fields.protocol == PROTOCOL_TX7U) return hasHumidity() ? getTX7humidity() : 0;
      if (data->sensorData.fields.protocol == PROTOCOL_HG02832) return (data->sensorData.u32.low>>16) & 255;
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
        else if (data->sensorData.fields.protocol == PROTOCOL_TX7U)
          fprintf(file, "  TX3/TX6/TX7 data  = %03x%08x\n", (data->sensorData.u32.hi&0xfff), data->sensorData.u32.low); //FIXME
        else if (data->sensorData.fields.protocol == PROTOCOL_HG02832)
          fprintf(file, "  HG02832 data  = %08x\n", data->sensorData.u32.low);
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


      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
        fprintf(file, "  type              = AcuRite 00592TXR\n  channel           = %c\n", getChannel00592TXR());

      } else if (data->sensorData.fields.protocol == PROTOCOL_F007TH) {
        fprintf(file, "  type              = Ambient Weather F007TH\n  channel           = %d\n", getChannelF007TH());

      } else if (data->sensorData.fields.protocol == PROTOCOL_TX7U) {
        fprintf(file, "  type              = LaCrosse TX3/TX6/TX7(%s)\n", hasTemperature()?"temperature":hasHumidity()?"humidity":"unknown");

      } else if (data->sensorData.fields.protocol == PROTOCOL_HG02832) {
        fprintf(file, "  type              = Auriol HG02832 (IAN 283582)\n  channel           = %d\n", getChannelHG02832());
      }

      fprintf(file, "  rolling code      = %02x\n", getRollingCode());
      if (hasTemperature()) {
        int temperature = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
        fprintf(file, "  temperature       = %d.%c%c\n", temperature/10, '0'+((temperature<0?-temperature:temperature)%10), (options&OPTION_CELSIUS) != 0 ? 'C' : 'F');
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
      fprintf( file, "{\"time\":\"%s\"", dt );

      if ((decodingStatus & 0xc0) != 0 ) {
        fputs(",\"valid\":false,", file);
      }

      if (data->sensorData.fields.protocol == PROTOCOL_00592TXR) {
        fputs(",\"type\":\"AcuRite 00592TXR\"", file);
      } else if (data->sensorData.fields.protocol == PROTOCOL_F007TH) {
        fputs(",\"type\":\"Ambient Weather F007TH\"", file);
      } else if (data->sensorData.fields.protocol == PROTOCOL_TX7U) {
        fputs(",\"type\":\"LaCrosse TX3/TX6/TX7\"", file);
      } else if (data->sensorData.fields.protocol == PROTOCOL_HG02832) {
        fputs(",\"type\":\"Auriol HG02832 (IAN 283582)\"", file);
      }

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
      "{\"time\":\"%s\",\"type\":\"%s\",\"valid\":%s",
      dt,
      data->sensorData.fields.protocol == PROTOCOL_00592TXR ? "AcuRite 00592TXR" :
      data->sensorData.fields.protocol == PROTOCOL_F007TH ? "Ambient Weather F007TH" :
      data->sensorData.fields.protocol == PROTOCOL_HG02832 ? "Auriol HG02832 (IAN 283582)" :
      data->sensorData.fields.protocol == PROTOCOL_TX7U ? "LaCrosse TX3/TX6/TX7" : "unknown",
      decodingStatus == 0 ? "true" : "false"
    );

    int channel = getChannelNumber();
    if (channel >= 0) len += snprintf(buffer+len, buflen-len, ",\"channel\":%d", channel);
    len += snprintf(buffer+len, buflen-len, ",\"rolling_code\":%d", getRollingCode());
    if (hasTemperature()) len += snprintf(buffer+len, buflen-len, ",\"temperature\":%d", (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10());
    if (hasHumidity()) len += snprintf(buffer+len, buflen-len, ",\"humidity\":%d", getHumidity());
    if (hasBatteryStatus()) len += snprintf(buffer+len, buflen-len, ",\"battery_ok\":%s", getBatteryStatus() ? "true" :"false");
    len += snprintf(buffer+len, buflen-len, "\n");

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

    char id[60];
    int len = snprintf( id, sizeof(id),
      "type=%s,channel=%d,rolling_code=%d",
      getSensorTypeName(),
      getChannelNumber(),
      getRollingCode()
    );

    char* output = buffer;
    int remain = buflen;
    if ((changed&TEMPERATURE_IS_CHANGED) != 0) {
      int t = (options&OPTION_CELSIUS) != 0 ? getTemperatureCx10() : getTemperatureFx10();
      int tF = t/10;
      char dF = '0'+((t<0?-t:t)%10);
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

    if ((options&VERBOSITY_INFO) != 0) {
      //fprintf(stderr, "JSON data size is %d bytes.\n", len*sizeof(unsigned char));
      fputs(buffer, stderr);
      fputc('\n', stderr);
    }

    return len*sizeof(unsigned char);
  }

};

#endif /* RECEIVEDMESSAGE_HPP_ */
