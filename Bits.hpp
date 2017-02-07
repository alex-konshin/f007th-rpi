/*
 * Bits.hpp
 *
 *  Created on: January 25, 2017
 *      Author: Alex Konshin
 */

#ifndef BITS_HPP_
#define BITS_HPP_


class Bits {
private:
  int size;
  int capacity;
  uint64_t* words;

public:
  Bits(int capacity) {
    if ( capacity<0 ) capacity = 64;
    int initialNumberOfWords = (capacity+63)>>6;
    if ( initialNumberOfWords==0 ) initialNumberOfWords = 1;
    words = (uint64_t*)calloc(initialNumberOfWords, sizeof(uint64_t));
    this->capacity = initialNumberOfWords;
    size = 0;
  }

  ~Bits() {
    size = 0;
    capacity = 0;
    free(words);
    words = __null;
  }

  inline int getSize() { return size; }

  void addBit(bool value) {
    int index = size++;
    int new_capacity = (size+63)>>6;
    if (new_capacity > capacity) {
      uint64_t* new_words = (uint64_t*)realloc(words, new_capacity*sizeof(uint64_t));
      //if (new_words == NULL){}
      words = new_words;
      capacity = new_capacity;
      words[new_capacity-1] = 0LL;
    }
    //if ( value ) words[index>>6] |= (1ULL<<(index&63));
    if ( value )
      ((uint32_t*)words)[index>>5] |= (1<<(index&31));
  }

  bool getBit(int index) {
    if ( index>=size ) return false;
    //return (words[index>>6] & (1ULL<<(index&63))) != 0L;
    return ((((uint32_t*)words)[index>>5]) & (1<<(index&31))) != 0;
  }

  uint32_t getInt(int from, int size) {
    uint32_t result = 0;
    for (int index = from; index<from+size; index++) {
      result <<= 1;
      if (getBit(index)) result |= 1L;
    }
    return result;
  }

  int findBits(uint32_t bits, int bitsLength) {
    int shift = 64-bitsLength;

    // reverse order of bits
    uint32_t inv = 0;
    uint32_t b = 1L << (bitsLength-1);
    while ( b != 0 ) {
      if ( (bits&1L)!=0 ) inv |= b;
      bits >>= 1;
      b >>= 1;
    }
    uint64_t bb = inv & 0xffffffffLL;
    uint64_t mask = (1LL << bitsLength) - 1;
    bb &= mask;

    int endIndex = size - bitsLength;
    int index = 0;

    for ( int wordIndex = 0; wordIndex<capacity; wordIndex++ ) {
      uint64_t word = words[wordIndex];
      if ( (word&mask)==bb ) return index;
      if ( ++index > endIndex ) return -1;

      for ( int i=0; i<shift; i++ ) {
        word >>= 1;
        if ( (word&mask)==bb ) return index;
        if ( ++index > endIndex ) return -1;
      }

      word |= (words[wordIndex+1] << bitsLength);
      for ( int i=0; i<bitsLength-1; i++ ) {
        word >>= 1;
        if ( (word&mask)==bb ) return index;
        if ( ++index > endIndex ) return -1;
      }
    }
    return -1;
  }

};



#endif /* BITS_HPP_ */
