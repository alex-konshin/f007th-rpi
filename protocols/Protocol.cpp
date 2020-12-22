/*
 * Protocol.cpp
 *
 *  Created on: December 6, 2020
 *      Author: akonshin
 */

#include "Protocol.hpp"
#include "../common/SensorsData.hpp"
#include "../common/RFReceiver.hpp"

const char* Protocol::channel_names_numeric[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8"};
Protocol* Protocol::protocols[NUMBER_OF_PROTOCOLS];

Statistics* statistics = new Statistics();

void Protocol::copyFields(SensorData* to, SensorData* from) {
  to->data_time = from->data_time;
  to->u64 = from->u64;
}

int Protocol::getTemperature10(SensorData* data, bool celsius) { return celsius ? data->getTemperatureCx10() : data->getTemperatureFx10(); }

bool Protocol::decodeManchester(ReceivedData* message, Bits& bitSet, int min_duration, int max_half_duration) {
  int iSequenceSize = message->iSequenceSize;
  int16_t* pSequence = message->pSequence;

  int startIndex = 0;
  int endIndex = 0;
  for ( int index = 0; index<iSequenceSize-MIN_SEQUENCE_LENGTH; index++ ) {
    if (pSequence[index] < min_duration) {
      if ((index-startIndex) >= MIN_SEQUENCE_LENGTH) {
        endIndex = index;
        if (decodeManchester(message, startIndex, endIndex, bitSet, max_half_duration)) {
          if (bitSet.getSize() >= 56) return true;
        }
      }
      startIndex = (index+2) & ~1u;
    }
  }

  if ((iSequenceSize-startIndex) >= MIN_SEQUENCE_LENGTH) {
    endIndex = iSequenceSize;
    if (decodeManchester(message, startIndex, endIndex, bitSet, max_half_duration)) {
      if (bitSet.getSize() >= 56) return true;
    }
  }

  return false;
}


bool Protocol::decodeManchester(ReceivedData* message, int startIndex, int endIndex, Bits& bitSet, int max_half_duration) {
  int16_t* pSequence = message->pSequence;

  message->decodingStatus = 0;

  // find the start of Manchester frame
  int adjastment = -1;
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
    message->decodingStatus |= 1;
    return false;
  }

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

  int iSubSequenceSize = endIndex-startIndex;
  // decoding
  do {
    int interval = pSequence[startIndex+intervalIndex];
    bool half = interval<max_half_duration;
    bitSet.addBit( ((intervalIndex&1)==0) != half ); // level = (intervalIndex&1)==0

    if ( half ) {
      if ( ++intervalIndex>=iSubSequenceSize ) return true;
      interval = pSequence[startIndex+intervalIndex];
      if ( interval>=max_half_duration ) {
        statistics->manchester_OOS++;
        //Log->info( "    Bad sequence at index %d: %d.", intervalIndex, interval );
        message->decodingStatus |= 2;
        return true;
      }
    }

  } while ( ++intervalIndex<(iSubSequenceSize-1) );

  return true;
}

bool Protocol::printManchesterBits(ReceivedMessage& message, FILE* file, FILE* file2) {
  // FIXME rewrite it
  Protocol* protocol = protocols[PROTOCOL_INDEX_F007TH];
  if ( protocol == NULL || (message.data->detailedDecodingStatus[PROTOCOL_INDEX_F007TH] & 7)!=0 ) return false;
  if ((message.data->protocol_tried_manchester&(1<<protocol->protocol_index)) == 0) return false;

  // Manchester decoding was successful. Print bits...
  uint16_t decodingStatus = message.data->decodingStatus;
  Bits bits(message.data->iSequenceSize+1);
  if (protocol->decodeManchester(message.data, bits)) {
    fprintf(file, "  Manchester decoding was successful\n");
    if (file2 != NULL) {
      fprintf(file2, "  Manchester decoding was successful\n");
      fflush(file2);
    }

    ReceivedMessage::printBits(file, &bits);
    if (file2 != NULL) {
      ReceivedMessage::printBits(file2, &bits);
      fflush(file2);
    }
  }
  message.data->decodingStatus = decodingStatus;
  return true;
}

bool Protocol::decodePWM(ReceivedData* message, int startIndex, int size, int minLo, int maxLo, int minHi, int maxHi, int median, Bits& bits) {
  int16_t* pSequence = message->pSequence;
  int end = startIndex+size;
  for ( int index=startIndex; index<end; index+=2 ) {
    int duration = pSequence[index];
    if ( duration>maxHi || duration<minHi ) {
      //DBG("decodePWM() pSequence[%d]=%d hi (expected %d..%d)",index,duration,minHi,maxHi);
      message->decodingStatus |= 4;
      return false;
    }

    bits.addBit( duration<median );

    if ( index+1<end ) {
      duration = pSequence[index+1];
      if ( duration>maxLo || duration<minLo ) {
        //DBG("decodePWM() pSequence[%d]=%d lo (expected %d..%d)",index,duration,minLo,maxLo);
        message->decodingStatus |= 4;
        return false;
      }
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

