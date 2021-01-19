/*
 * Protocol.cpp
 *
 *  Created on: December 6, 2020
 *      Author: akonshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/Receiver.hpp"

//#define DEBUG_MANCHESTER
#if defined(NDEBUG)||!defined(DEBUG_MANCHESTER)
#define DBG_MANCHESTER(format, arg...)     ((void)0)
#else
#define DBG_MANCHESTER(format, arg...)  Log->info(format, ## arg)
#endif // NDEBUG


const char* Protocol::channel_names_numeric[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8"};
Protocol* Protocol::protocols[NUMBER_OF_PROTOCOLS];
uint32_t Protocol::registered_protocols = 0;
uint32_t Protocol::rf_protocols = 0;

Statistics* statistics = new Statistics();

Protocol* ProtocolDef::getProtocol() {
  return Protocol::protocols[protocol_index];
}
uint32_t ProtocolDef::getFeatures() {
  Protocol* protocol = getProtocol();
  if (protocol == NULL) {
    fprintf(stderr, "Program error - protocol \"%s\" is not initialized", name);
    exit(2);
  }
  return protocol->features;
}

uint64_t ProtocolDef::getId(uint8_t channel, uint16_t rolling_code) {
  Protocol* protocol = getProtocol();
  if (protocol == NULL) {
    fprintf(stderr, "Program error - protocol \"%s\" is not initialized", name);
    exit(2);
  }
  return protocol->getId(this, channel, rolling_code);
}

void Protocol::copyFields(SensorData* to, SensorData* from) {
  to->data_time = from->data_time;
  to->u64 = from->u64;
}

int Protocol::getTemperature10(SensorData* data, bool celsius) { return celsius ? data->getTemperatureCx10() : data->getTemperatureFx10(); }

bool Protocol::decodeManchester(ReceivedData* message, Bits& bitSet, ProtocolThresholds& limits) {
  int iSequenceSize = message->iSequenceSize;
  int16_t* pSequence = message->pSequence;

  int min_sequence_length = limits.min_sequence_length;

  // Find subsequences that matches provided criteria.
  // Try to decode bits from found subsequences with Manchester decoder.
  // Call virtual method processDecodedBits to process bits and validate the data.

  int subsequence_size;
  int startIndex = 0;
  int firstShortIndex = -1;

  bool even = true;
  for ( int index = 0; index<iSequenceSize-min_sequence_length; index++ ) {
    if (index < startIndex) continue;

    DurationThresholds& thresolds = (index&1)==0 ? limits.high : limits.low;
    int16_t item = pSequence[index];
    bool short_item = item <= thresolds.short_max && item >= thresolds.short_min;
    bool long_item = !short_item && item <= thresolds.long_max && item >= thresolds.long_min;

    if (!short_item && !long_item) {
      subsequence_size = index-startIndex;
      DBG_MANCHESTER(">>> 1 >>> decodeManchester pSequence[%d]=%d short=%d..%d long=%d..%d size=%d min_size=%d", index, item, thresolds.short_min, thresolds.short_max, thresolds.long_min, thresolds.long_max, subsequence_size, min_sequence_length);
      if (subsequence_size >= min_sequence_length) {
        bool success = decodeManchester(message, startIndex, subsequence_size, bitSet, limits);
        DBG_MANCHESTER(">>> 1 >>> decodeManchester start=%d size=%d bits=%d decodingStatus=%04x", startIndex, subsequence_size, bitSet.getSize(), message->decodingStatus);
        if (success && bitSet.getSize() >= limits.min_bits) {
          success = processDecodedBits(message, bitSet);
          DBG_MANCHESTER(">>> 1 >>> processDecodedBits start=%d size=%d decodingStatus=%04x", startIndex, subsequence_size, message->decodingStatus);
          if (success) return true;
        }
      }
      startIndex = (index+2) & ~1u;
      firstShortIndex = -1;
      even = true;
      DBG_MANCHESTER(">>> 1 >>> decodeManchester set startIndex=%d", startIndex);
    } else if (long_item) {
      if (!even) { // OOS
        if (firstShortIndex < 0) { // should not happen
          startIndex = (index+2) & ~1u;
        } else {
          startIndex = (firstShortIndex+2) & ~1u;
        }
        DBG_MANCHESTER(">>> 1 >>> decodeManchester pSequence[%d]=%d OOS firstShortIndex=%d => startIndex=%d", index, item, firstShortIndex, startIndex);
        even = true;
        firstShortIndex = -1;
      }
    } else {
      if (firstShortIndex < 0) firstShortIndex = index;
      even = !even;
    }
  }

  subsequence_size = iSequenceSize-startIndex;
  if (subsequence_size >= min_sequence_length) {
    bool success = decodeManchester(message, startIndex, subsequence_size, bitSet, limits);
    DBG_MANCHESTER(">>> 2 >>> decodeManchester start=%d size=%d bits=%d decodingStatus=%04x", startIndex, subsequence_size, bitSet.getSize(), message->decodingStatus);
    if (success && bitSet.getSize() >= limits.min_bits) {
      bool success = processDecodedBits(message, bitSet);
      DBG_MANCHESTER(">>> 2 >>> decodeManchester start=%d size=%d decodingStatus=%04x", startIndex, subsequence_size, message->decodingStatus);
      if (success) return true;
    }
  }

  if (message->decodingStatus == 0 ) message->decodingStatus = 4;
  return false;
}

bool Protocol::decodeManchester(ReceivedData* message, int startIndex, int size, Bits& bitSet, ProtocolThresholds& limits) {
  DBG_MANCHESTER(">>> 0 >>> decodeManchester start=%d size=%d decodingStatus=%04x", startIndex, size, message->decodingStatus);
  int16_t* pSequence = message->pSequence;

  message->decodingStatus = 0;

  // find the start of Manchester frame
  int adjastment = -1;
  int16_t max_half_duration = limits.low.short_max;
  for ( int index = 0; index<32; index++ ) {
    if (pSequence[index+startIndex] > max_half_duration) {
      if ( index==0 ) {
        // first interval is long
        adjastment = 1;
      } else if ((index&1) == 1) {
        // first probe is good
        adjastment = 0;
      } else {
        // first bit is 0
        adjastment = 2;
      }
      break;
    }
  }
  if ( adjastment==-1 ) {
    statistics->bad_manchester++;
    message->decodingStatus = 1;
    return false;
  }

  message->protocol_tried_manchester |= protocol_bit;

  bitSet.clear();

  // first bits
  int intervalIndex = -1;
  switch(adjastment) {
  case 0: // good
    bitSet.addBit( true );
    intervalIndex = 1;
    break;
  case 1: // first interval is long
    bitSet.addBit( false );
    bitSet.addBit( true );
    intervalIndex = 1;
    break;
  case 2: // first bit is 0
    bitSet.addBit( false );
    intervalIndex = 0;
    break;
  }

  // decoding
  do {
    int interval = pSequence[startIndex+intervalIndex];
    bool half = interval<max_half_duration;
    bitSet.addBit( ((intervalIndex&1)==0) != half ); // level = (intervalIndex&1)==0

    if ( half ) {
      if ( ++intervalIndex>=size ) return true;
      interval = pSequence[startIndex+intervalIndex];
      if ( interval>=max_half_duration ) {
        if (bitSet.getSize() >= limits.min_bits) return true;
        statistics->manchester_OOS++;
        DBG_MANCHESTER( "    Bad sequence at index %d: %d.", startIndex+intervalIndex, interval );
        message->decodingStatus = 2;
        return true;
      }
    }

  } while ( ++intervalIndex<(size-1) );

  return true;
}

bool Protocol::decodePWM(ReceivedData* message, int startIndex, int size, int minLo, int maxLo, int minHi, int maxHi, int median, Bits& bits) {
  int16_t* pSequence = message->pSequence;
  int end = startIndex+size;
  for ( int index=startIndex; index<end; index+=2 ) {
    int duration = pSequence[index];
    if ( duration>maxHi || duration<minHi ) {
      //DBG("decodePWM() pSequence[%d]=%d hi (expected %d..%d)",index,duration,minHi,maxHi);
      message->decodingStatus = 4;
      return false;
    }

    bits.addBit( duration<median );

    if ( index+1<end ) {
      duration = pSequence[index+1];
      if ( duration>maxLo || duration<minLo ) {
        //DBG("decodePWM() pSequence[%d]=%d lo (expected %d..%d)",index,duration,minLo,maxLo);
        message->decodingStatus = 4;
        return false;
      }
    }
  }
  return true;
}

static inline bool is_good(int actual_width, int expected_width, int tolerance) {
  int delta = actual_width - expected_width;
  if (delta < 0) delta = -delta;
  return delta <= tolerance;
}

bool Protocol::decodePPM(ReceivedData* message, int startIndex, int size, int pulse_width, int pulse_tolerance, int lo0, int lo1, int lo_tolerance, Bits& bits) {
  int16_t* pSequence = message->pSequence;
  int end = startIndex+size;
  for ( int index=startIndex; index<end; index+=2 ) {
    int duration = pSequence[index];
    if (!is_good(duration, pulse_width, pulse_tolerance)) {
      //DBG("decodePPM() pSequence[%d]=%d hi (expected %d..%d)",index,duration,pulse_width-pulse_tolerance,pulse_width+pulse_tolerance);
      message->decodingStatus = 4;
      return false;
    }

    if ( index+1<end ) {
      duration = pSequence[index+1];
      bool bit = false;
      if (is_good(duration, lo1, lo_tolerance)) {
        bit = true;
      } else if (!is_good(duration, lo0, lo_tolerance)) {
        //DBG("decodePPM() pSequence[%d]=%d lo (expected %d or %d with tolerance %d)",index,duration,lo0,lo1,lo_tolerance);
        message->decodingStatus = 4;
        return false;
      }
      bits.addBit(bit);
    }

  }
  return true;
}

uint8_t Protocol::crc8( Bits& bits, int from, int size, int polynomial, int init ) {
  int result = init;
  for ( int index = from; index<from+size; index += 8 ) {
    result ^= bits.getInt( index, 8 );
    for ( int bit = 0; bit<8; ++bit ) {
      if ( (result&0x80)!=0 ) {
        result = (result<<1)^polynomial;
      } else {
        result = (result<<1);
      }
    }
  }
  return (uint8_t)(result & 255);
}

