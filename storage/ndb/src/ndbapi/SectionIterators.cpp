/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <SectionIterators.hpp>
#include "NdbApiSignal.hpp"

/**
 * LinearSectionIterator
 *
 * This is an implementation of GenericSectionIterator
 * that iterates over one linear section of memory.
 * The iterator is used by the transporter at signal
 * send time to obtain all of the relevant words for the
 * signal section
 */
LinearSectionIterator::LinearSectionIterator(const Uint32 *_data, Uint32 _len) {
  data = (_len == 0) ? nullptr : _data;
  len = _len;
  read = false;
}

LinearSectionIterator::~LinearSectionIterator() {}

void LinearSectionIterator::reset() {
  /* Reset iterator */
  read = false;
}

const Uint32 *LinearSectionIterator::getNextWords(Uint32 &sz) {
  if (likely(!read)) {
    read = true;
    sz = len;
    return data;
  }
  sz = 0;
  return nullptr;
}

/**
 * SignalSectionIterator
 *
 * This is an implementation of GenericSectionIterator
 * that uses chained NdbApiSignal objects to store a
 * signal section.
 * The iterator is used by the transporter at signal
 * send time to obtain all of the relevant words for the
 * signal section
 */
SignalSectionIterator::SignalSectionIterator(NdbApiSignal *signal) {
  firstSignal = currentSignal = signal;
}

SignalSectionIterator::~SignalSectionIterator() {}

void SignalSectionIterator::reset() {
  /* Reset iterator */
  currentSignal = firstSignal;
}

const Uint32 *SignalSectionIterator::getNextWords(Uint32 &sz) {
  if (likely(currentSignal != nullptr)) {
    NdbApiSignal *signal = currentSignal;
    currentSignal = currentSignal->next();
    sz = signal->getLength();
    return signal->getDataPtrSend();
  }
  sz = 0;
  return nullptr;
}

/**
 * FragmentedSectionIterator
 * -------------------------
 * This class acts as an adapter to a GenericSectionIterator
 * instance, providing a sub-range iterator interface.
 * It is used when long sections of a signal are fragmented
 * across multiple actual signals - the user-supplied
 * GenericSectionIterator is then adapted into a
 * GenericSectionIterator that only returns a subset of
 * the contained words for each signal fragment.
 */

FragmentedSectionIterator::FragmentedSectionIterator(GenericSectionPtr ptr) {
  realIterator = ptr.sectionIter;
  realIterWords = ptr.sz;
  realCurrPos = 0;
  rangeStart = 0;
  rangeLen = rangeRemain = realIterWords;
  lastReadPtr = nullptr;
  lastReadTotal = 0;
  lastReadAvail = 0;
  moveToPos(0);

  assert(checkInvariants());
}

bool FragmentedSectionIterator::checkInvariants() {
  assert((realIterator != nullptr) || (realIterWords == 0));
  assert(realCurrPos <= realIterWords);
  assert(rangeStart <= realIterWords);
  assert((rangeStart + rangeLen) <= realIterWords);
  assert(rangeRemain <= rangeLen);

  /* Can only have a null readptr if nothing is left */
  assert((lastReadPtr != nullptr) || (lastReadAvail == 0));
  assert((lastReadPtr != nullptr) || (rangeRemain == 0));

  /* If we have a non-null readptr and some remaining
   * words the readptr must have some words
   */
  assert((lastReadPtr == nullptr) ||
         ((rangeRemain == 0) || (lastReadTotal != 0)));

  assert(lastReadTotal >= lastReadAvail);
  return true;
}

void FragmentedSectionIterator::moveToPos(Uint32 pos) {
  assert(pos <= realIterWords);
  if (realIterWords == 0) {
    /**
     * Iterator is empty, 'realIterator' could even be nullptr.
     * We are allowed to position at the end only. (With nothing available)
     */
    assert(pos == 0);
    assert(lastReadTotal == 0 && lastReadAvail == 0);
    assert(realCurrPos == 0);
    return;
  }
  if (pos < realCurrPos) {
    /* Need to reset, and advance from the start */
    realIterator->reset();
    realCurrPos = 0;
    lastReadPtr = nullptr;
    lastReadTotal = 0;
    lastReadAvail = 0;
  }

  /* Advance until we get a chunk which contains the pos */
  while (pos >= realCurrPos + lastReadTotal) {
    realCurrPos += lastReadTotal;
    lastReadPtr = realIterator->getNextWords(lastReadTotal);
    lastReadAvail = lastReadTotal;
    if (lastReadPtr == nullptr) {
      // Advanced past the end
      assert(pos == realIterWords && realCurrPos == realIterWords);
      assert(lastReadAvail == 0);
      return;
    }
  }

  const Uint32 chunkOffset = pos - realCurrPos;
  lastReadAvail = lastReadTotal - chunkOffset;
}

bool FragmentedSectionIterator::setRange(Uint32 start, Uint32 len) {
  assert(checkInvariants());
  if (start + len > realIterWords) return false;
  moveToPos(start);

  rangeStart = start;
  rangeLen = rangeRemain = len;

  assert(checkInvariants());
  return true;
}

void FragmentedSectionIterator::reset() {
  /* Reset iterator to last specified range */
  assert(checkInvariants());
  moveToPos(rangeStart);
  rangeRemain = rangeLen;
  assert(checkInvariants());
}

const Uint32 *FragmentedSectionIterator::getNextWords(Uint32 &sz) {
  assert(checkInvariants());
  const Uint32 *currPtr = nullptr;
  sz = 0;

  if (rangeRemain) {
    assert(lastReadPtr != nullptr);
    assert(lastReadTotal != 0);

    if (lastReadAvail > 0) {
      /* Return remaining unconsumed data in current chunk */
      const Uint32 skip = lastReadTotal - lastReadAvail;
      currPtr = lastReadPtr + skip;
      sz = lastReadAvail;
    } else {
      /* Read a new chunk, return it as consumed */
      realCurrPos += lastReadTotal;
      currPtr = lastReadPtr = realIterator->getNextWords(lastReadTotal);
      sz = lastReadAvail = lastReadTotal;
    }

    if (sz > rangeRemain) {
      /* What we return may be limited by end-of-range */
      sz = rangeRemain;
    }
    rangeRemain -= sz;
  } else {
    sz = 0;
  }

  // Assume all returned is consumed, until optional moveToPos()
  lastReadAvail -= sz;
  assert(checkInvariants());
  return currPtr;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////// Unit test ///////////////////

#ifdef TEST_SECTIONITERATORS

#include <random.h>
#include <util/NdbTap.hpp>
#include "NdbApiSignal.hpp"

#define VERIFY(x)                                            \
  if ((x) == 0) {                                            \
    printf("VERIFY failed at Line %u : %s\n", __LINE__, #x); \
    return -1;                                               \
  }

/* Verify that word[n] == bias + n */
int verifyIteratorContents(GenericSectionIterator &gsi, int dataWords,
                           int bias) {
  int pos = 0;

  while (pos < dataWords) {
    const Uint32 *readPtr = nullptr;
    Uint32 len = 0;

    readPtr = gsi.getNextWords(len);

    VERIFY(readPtr != nullptr);
    VERIFY(len != 0);
    VERIFY(len <= (Uint32)(dataWords - pos));

    for (int j = 0; j < (int)len; j++) VERIFY(readPtr[j] == (Uint32)(bias++));

    pos += len;
  }

  return 0;
}

int checkGenericSectionIterator(GenericSectionIterator &iter, int size,
                                int bias) {
  /* Verify contents */
  VERIFY(verifyIteratorContents(iter, size, bias) == 0);

  Uint32 sz;

  /* Check that iterator is empty now */
  VERIFY(iter.getNextWords(sz) == nullptr);
  VERIFY(sz == 0);

  VERIFY(iter.getNextWords(sz) == nullptr);
  VERIFY(sz == 0);

  iter.reset();

  /* Verify reset put us back to the start */
  VERIFY(verifyIteratorContents(iter, size, bias) == 0);

  /* Verify no more words available */
  VERIFY(iter.getNextWords(sz) == nullptr);
  VERIFY(sz == 0);

  return 0;
}

Uint32 randRange(Uint32 range) { return ((Uint32)rand()) % range; }

int checkIterator(GenericSectionIterator &iter, int size, int bias) {
  /* Test iterator itself, and then FragmentedSectionIterator
   * adaptation
   */
  VERIFY(checkGenericSectionIterator(iter, size, bias) == 0);

  /* Now we'll test the FragmentedSectionIterator on the iterator
   * we were passed
   */
  const int subranges = 20;

  iter.reset();
  GenericSectionPtr ptr;
  ptr.sz = size;
  ptr.sectionIter = &iter;
  FragmentedSectionIterator fsi(ptr);

  for (int s = 0; s < subranges; s++) {
    Uint32 start = 0;
    Uint32 len = 0;
    if (size > 0) {
      start = randRange(size);
      if (0 != (size - start)) len = randRange(size - start);
    }

    /*
      printf("Range (0-%u) = (%u + %u)\n",
              size, start, len);
    */
    fsi.setRange(start, len);
    VERIFY(checkGenericSectionIterator(fsi, len, bias + start) == 0);
  }

  return 0;
}

int testLinearSectionIterator() {
  /* Test Linear section iterator of various
   * lengths with section[n] == bias + n
   */
  const int totalSize = 200000;
  const int bias = 13;

  Uint32 data[totalSize];
  for (int i = 0; i < totalSize; i++) data[i] = bias + i;

  for (int len = 0; len < 1000; len++) {
    LinearSectionIterator something(data, len);

    VERIFY(checkIterator(something, len, bias) == 0);
  }

  return 0;
}

NdbApiSignal *createSignalChain(NdbApiSignal *&poolHead, int length, int bias) {
  /* Create signal chain, with word[n] == bias+n */
  NdbApiSignal *chainHead = nullptr;
  NdbApiSignal *chainTail = nullptr;
  int pos = 0;

  while (pos < length) {
    int offset = pos % NdbApiSignal::MaxSignalWords;

    if (offset == 0) {
      if (poolHead == nullptr) return nullptr;

      NdbApiSignal *newSig = poolHead;
      poolHead = poolHead->next();

      newSig->next(nullptr);

      if (chainHead == nullptr) {
        chainHead = chainTail = newSig;
      } else {
        chainTail->next(newSig);
        chainTail = newSig;
      }
    }

    chainTail->getDataPtrSend()[offset] = (bias + pos);
    chainTail->setLength(offset + 1);
    pos++;
  }

  return chainHead;
}

int testSignalSectionIterator() {
  /* Create a pool of signals, build
   * signal chains from it, test
   * the iterator against the signal chains
   */
  const int totalNumSignals = 100;
  NdbApiSignal *poolHead = nullptr;

  /* Allocate some signals */
  for (int i = 0; i < totalNumSignals; i++) {
    NdbApiSignal *sig = new NdbApiSignal((BlockReference)0);

    if (poolHead == nullptr) {
      poolHead = sig;
      sig->next(nullptr);
    } else {
      sig->next(poolHead);
      poolHead = sig;
    }
  }

  const int bias = 7;
  for (int dataWords = 1;
       dataWords <= (int)(totalNumSignals * NdbApiSignal::MaxSignalWords);
       dataWords++) {
    NdbApiSignal *signalChain = nullptr;

    VERIFY((signalChain = createSignalChain(poolHead, dataWords, bias)) !=
           nullptr);

    SignalSectionIterator ssi(signalChain);

    VERIFY(checkIterator(ssi, dataWords, bias) == 0);

    /* Now return the signals to the pool */
    while (signalChain != nullptr) {
      NdbApiSignal *sig = signalChain;
      signalChain = signalChain->next();

      sig->next(poolHead);
      poolHead = sig;
    }
  }

  /* Free signals from pool */
  while (poolHead != nullptr) {
    NdbApiSignal *sig = poolHead;
    poolHead = sig->next();
    delete (sig);
  }

  return 0;
}

/**
 * Iterator view of a generator
 */
class BufferedGeneratingIterator : public GenericSectionIterator {
 private:
  Uint32 *buffer;
  Uint32 buffWords;
  Uint32 len;
  Uint32 pos;
  Uint32 bias;

 public:
  BufferedGeneratingIterator(Uint32 _size, Uint32 _bias, Uint32 _buffWords) {
    buffWords = _buffWords;
    buffer = (Uint32 *)malloc(_buffWords * sizeof(Uint32));
    len = _size;
    bias = _bias;
    pos = 0;
  }

  ~BufferedGeneratingIterator() override { free(buffer); }

  void reset() override {
    /* Reset iterator */
    pos = 0;
  }

  const Uint32 *getNextWords(Uint32 &sz) override {
    const Uint32 remain = len - pos;
    const Uint32 chunkSize = MIN(remain, buffWords);

    if (chunkSize) {
      /* Generate data in buffer */
      for (Uint32 i = 0; i < chunkSize; i++) {
        buffer[i] = bias + pos + i;
      }

      pos += chunkSize;
      sz = chunkSize;
      return buffer;
    }
    sz = 0;
    return nullptr;
  }
};

int testBufferedGeneratingIterator() {
  const int totalSize = 50000;
  const int bias = 19;

  for (int buffSize = 1; buffSize < 100; buffSize++) {
    BufferedGeneratingIterator bgi(totalSize, bias, buffSize);

    VERIFY(checkIterator(bgi, totalSize, bias) == 0);
  }

  return 0;
}

int main() {
  /* Unit test Section Iterators
   * ----------------------
   * To run this code :
   *  - Config cmake with -DWITH_UNIT_TESTS=ON
   *    -> Build binary SectionIterators-t
   *  - Will be added to unit test tested as part of Pb2
   *  - Or run it manually
   *
   * Will print "OK" in success case
   */

  plan(3);  // Number of tests

  ok1(testLinearSectionIterator() == 0);
  ok1(testSignalSectionIterator() == 0);
  ok1(testBufferedGeneratingIterator() == 0);

  return exit_status();
}

#endif
