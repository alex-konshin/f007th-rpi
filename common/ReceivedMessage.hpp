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

  uint32_t protocol_tried_manchester;
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
  char dt[TIME2STR_BUFFER_SIZE];

#define T2D_BUFFER_SIZE 13
#define ID_BUFFER_SIZE 512

public:
  ReceivedMessage() {
    data_time = time(NULL);
    data = NULL;
    *dt = '\0';
  }
  ReceivedMessage(ReceivedData* data) {
    data_time = time(NULL);
    this->data = data;
    *dt = '\0';
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
    *dt = '\0';
  }

  SensorData* getSensorData() {
    return &data->sensorData;
  }

  time_t getTime() {
    return data_time;
  }

  inline bool isEmpty() { return data == NULL; }

  inline bool isValid() {
    return data != NULL && data->decodingStatus == 0;
  }
  inline bool isUndecoded() {
    return data != NULL && (data->decodingStatus & 0x3f) != 0;
  }
  inline uint16_t getDecodingStatus() {
    return data == NULL ? -1 : data->decodingStatus;
  }

  char* getTimestampt(int options) {
    if (*dt == '\0')  convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, (options&OPTION_UTC) != 0);
    return dt;
  }

  bool printInputSequence(FILE* file, int options) {
    if (data == NULL || data->pSequence == NULL) return false;

    if (file == NULL) file = stdout;
    // print input sequence

    fputs(getTimestampt(options), file);
    fputc(' ', file);

    fprintf(file, "sequence size=%d:", data->iSequenceSize);
    for (int index=0; index<data->iSequenceSize; index++ ) {
      if (index != 0) fputc(',', file);
      fprintf(file, " %d", data->pSequence[index]);
    }
    fputc('\n', file);
    fflush(file);
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

  bool print(FILE* output, FILE* log, int options) {
    if (data == NULL) return false;

    uint16_t decodingStatus = data->decodingStatus;

    bool debug = (options&VERBOSITY_DEBUG) != 0;
    bool print_details = debug || (options&VERBOSITY_PRINT_DETAILS) != 0;
    bool print_undecoded = debug || (options&VERBOSITY_PRINT_UNDECODED) != 0;
    bool is_undecoded = isUndecoded();

    getTimestampt(options);

    if (!is_undecoded) getSensorDef();

    if (output == NULL) output = stdout;

    FILE* file = output;
    FILE* file2 = log;
    do { // repeat for log_file if it is not NULL

      if (print_details || (print_undecoded && is_undecoded)) printInputSequence(file, options);

      if (!is_undecoded) {
        fputs(dt, file);
        fputc('\n', file);

        if (print_details) data->sensorData.printRawData(file);

        if ((decodingStatus & 0xc0) != 0 ) {
          if ((decodingStatus & 0x80) != 0 )
            fprintf(file, "  *** failed checksum (error %04x) ***\n", decodingStatus);
          else
            fprintf(file, "  *** no checksum (error %04x) ***\n", decodingStatus);
        }

        data->sensorData.print(file, options);

      } else if (print_undecoded) {
        fputs(dt, file);
        fprintf(file, " decoding status = %04x(", decodingStatus );
        for (int proto_index = 0; proto_index<NUMBER_OF_PROTOCOLS; proto_index++) {
          if (proto_index != 0) fputc(',', file);
          fprintf(file, "%04x", data->detailedDecodingStatus[proto_index] );
        }
        if (data->decodedBits != 0)
          fprintf(file, "), decoded bits = %d\n", data->decodedBits);
        else
          fputs(")\n", file);
      }


      // repeat for log_file if it is not NULL
      file = file2;
      file2 = NULL;
    } while (file != NULL);

    if (debug || (print_undecoded && is_undecoded)) {
      for (int protocol_index = 0; protocol_index<NUMBER_OF_PROTOCOLS; protocol_index++) {
        Protocol* protocol = Protocol::protocols[protocol_index];
        if (protocol != NULL && (protocol->protocol_bit&data->protocol_tried_manchester) != 0) {
          protocol->printUndecoded(data, output, log, options);
        }
      }
    }

    fflush(output);
    if (log != NULL) fflush(log);

    return true;
  }

  ssize_t json(void*& buffer, size_t& buffer_size, int options) {
    bool verbose = (options&VERBOSITY_PRINT_JSON) != 0;
    if (data == NULL || buffer == NULL || buffer_size < SEND_DATA_BUFFER_SIZE) {
      if (verbose) {
        if (data == NULL)
          fputs("JSON data was not generated because no data was decoded.\n", stderr);
        else if (buffer == NULL)
          fputs("ERROR: Invalid call ReceivedMessage::json(char*,size_t,bool,bool): buffer is not specified.\n", stderr);
        else
          fprintf(stderr, "ERROR: Invalid call of ReceivedMessage::json(char*,size_t,bool,bool): invalid size (%u) of the buffer.\n", (unsigned)buffer_size);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if (isUndecoded()) {
      if (verbose) fprintf(stderr, "JSON data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    size_t size = data->sensorData.generateJson(0, buffer, buffer_size, options);
    if (size != 0) {
      char* ptr = (char*)buffer + size;
      *ptr++ = '\n';
      size += sizeof(unsigned char);
      *ptr = '\0';

      if (verbose) fputs((char*)buffer, stderr);
    }
    return size;
  }

  int influxDB(void*& buffer, size_t& buffer_size, int changed, int options) {
    bool verbose = (options&VERBOSITY_PRINT_JSON) != 0;
    if (data == NULL || buffer == NULL || buffer_size < SEND_DATA_BUFFER_SIZE) {
      if (verbose) {
        if (data == NULL)
          fputs("Output data was not generated because no data was decoded.\n", stderr);
        else if (buffer == NULL)
          fputs("ERROR: Invalid call influxDB(char*,size_t,bool): buffer is not specified.\n", stderr);
        else
          fprintf(stderr, "ERROR: Invalid call influxDB(char*,size_t,bool): invalid size (%u) of the buffer.\n", (unsigned)buffer_size);
      }
      return -1;
    }

    uint16_t decodingStatus = data->decodingStatus;
    if (decodingStatus != 0 ) {
      if (verbose) fprintf(stderr, "Output data was not generated because decodingStatus = %04x.\n", decodingStatus);
      return -2;
    }

    size_t size = data->sensorData.generateInfluxData(0, buffer, buffer_size, changed, options);

    if ((options&VERBOSITY_INFO) != 0) {
      fputs((char*)buffer, stderr);
      fputc('\n', stderr);
    }

    return size;
  }

  SensorDef* getSensorDef() {
    if (data == NULL) return NULL;
    if (!is_sensor_def_set) {
      is_sensor_def_set = true;
      return data->sensorData.def = SensorDef::find(data->sensorData.getId());
    }
    return data->sensorData.def;
  }

  int update(SensorsData& sensorsData, time_t max_unchanged_gap) {
    getSensorDef(); // sets is_sensor_def_set = true;
    return sensorsData.update(&data->sensorData, data_time, max_unchanged_gap);
  }
};

#endif /* RECEIVEDMESSAGE_HPP_ */
