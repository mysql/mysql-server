/*
   Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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
LinearSectionIterator::LinearSectionIterator(const Uint32* _data, Uint32 _len)
{
  data= (_len == 0)? NULL:_data;
  len= _len;
  read= false;
}

LinearSectionIterator::~LinearSectionIterator()
{}

void
LinearSectionIterator::reset()
{
  /* Reset iterator */
  read= false;
}

const Uint32*
LinearSectionIterator::getNextWords(Uint32& sz)
{
  if (likely(!read))
  {
    read= true;
    sz= len;
    return data;
  }
  sz= 0;
  return NULL;
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
SignalSectionIterator::SignalSectionIterator(NdbApiSignal* signal)
{
  firstSignal= currentSignal= signal;
}

SignalSectionIterator::~SignalSectionIterator()
{}

void
SignalSectionIterator::reset()
{
  /* Reset iterator */
  currentSignal= firstSignal;
}

const Uint32*
SignalSectionIterator::getNextWords(Uint32& sz)
{
  if (likely(currentSignal != NULL))
  {
    NdbApiSignal* signal= currentSignal;
    currentSignal= currentSignal->next();
    sz= signal->getLength();
    return signal->getDataPtrSend();
  }
  sz= 0;
  return NULL;
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
FragmentedSectionIterator::FragmentedSectionIterator(GenericSectionPtr ptr)
{
  realIterator= ptr.sectionIter;
  realIterWords= ptr.sz;
  realCurrPos= 0;
  rangeStart= 0;
  rangeLen= rangeRemain= realIterWords;
  lastReadPtr= NULL;
  lastReadPtrLen= 0;
  moveToPos(0);

  assert(checkInvariants());
}

bool
FragmentedSectionIterator::checkInvariants()
{
  assert( (realIterator != NULL) || (realIterWords == 0) );
  assert( realCurrPos <= realIterWords );
  assert( rangeStart <= realIterWords );
  assert( (rangeStart+rangeLen) <= realIterWords);
  assert( rangeRemain <= rangeLen );

  /* Can only have a null readptr if nothing is left */
  assert( (lastReadPtr != NULL) || (rangeRemain == 0));

  /* If we have a non-null readptr and some remaining 
   * words the readptr must have some words
   */
  assert( (lastReadPtr == NULL) || 
          ((rangeRemain == 0) || (lastReadPtrLen != 0)));
  return true;
}

void
FragmentedSectionIterator::moveToPos(Uint32 pos)
{
  assert(pos <= realIterWords);

  if (pos < realCurrPos)
  {
    /* Need to reset, and advance from the start */
    realIterator->reset();
    realCurrPos= 0;
    lastReadPtr= NULL;
    lastReadPtrLen= 0;
  }

  if ((lastReadPtr == NULL) && 
      (realIterWords != 0) &&
      (pos != realIterWords))
    lastReadPtr= realIterator->getNextWords(lastReadPtrLen);

  if (pos == realCurrPos)
    return;

  /* Advance until we get a chunk which contains the pos */
  while (pos >= realCurrPos + lastReadPtrLen)
  {
    realCurrPos+= lastReadPtrLen;
    lastReadPtr= realIterator->getNextWords(lastReadPtrLen);
    assert(lastReadPtr != NULL);
  }

  const Uint32 chunkOffset= pos - realCurrPos;
  lastReadPtr+= chunkOffset;
  lastReadPtrLen-= chunkOffset;
  realCurrPos= pos;
}

bool
FragmentedSectionIterator::setRange(Uint32 start, Uint32 len)
{
  assert(checkInvariants());
  if (start+len > realIterWords)
    return false;
  moveToPos(start);

  rangeStart= start;
  rangeLen= rangeRemain= len;

  assert(checkInvariants());
  return true;
}

void
FragmentedSectionIterator::reset()
{
  /* Reset iterator to last specified range */
  assert(checkInvariants());
  moveToPos(rangeStart);
  rangeRemain= rangeLen;
  assert(checkInvariants());
}

const Uint32*
FragmentedSectionIterator::getNextWords(Uint32& sz)
{
  assert(checkInvariants());
  const Uint32* currPtr= NULL;

  if (rangeRemain)
  {
    assert(lastReadPtr != NULL);
    assert(lastReadPtrLen != 0);
    currPtr= lastReadPtr;
  
    sz= MIN(rangeRemain, lastReadPtrLen);
  
    if (sz == lastReadPtrLen)
      /* Will return everything in this chunk, move iterator to 
       * next
       */
      lastReadPtr= realIterator->getNextWords(lastReadPtrLen);
    else
    {
      /* Not returning all of this chunk, just advance within it */
      lastReadPtr+= sz;
      lastReadPtrLen-= sz;
    }
    realCurrPos+= sz;
    rangeRemain-= sz;
  }
  else
  {
    sz= 0;
  }

  assert(checkInvariants());
  return currPtr;
}




//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////// Unit test ///////////////////

#ifdef TEST_SECTIONITERATORS

#include <util/NdbTap.hpp>
#include "NdbApiSignal.hpp"

#define VERIFY(x) if ((x) == 0) { printf("VERIFY failed at Line %u : %s\n",__LINE__, #x);  return -1; }

/* Verify that word[n] == bias + n */
int
verifyIteratorContents(GenericSectionIterator& gsi, int dataWords, int bias)
{
  int pos= 0;

  while (pos < dataWords)
  {
    const Uint32* readPtr=NULL;
    Uint32 len= 0;

    readPtr= gsi.getNextWords(len);
    
    VERIFY(readPtr != NULL);
    VERIFY(len != 0);
    VERIFY(len <= (Uint32) (dataWords - pos));
    
    for (int j=0; j < (int) len; j++)
      VERIFY(readPtr[j] == (Uint32) (bias ++));

    pos += len;
  }

  return 0;
}

int
checkGenericSectionIterator(GenericSectionIterator& iter, int size, int bias)
{
  /* Verify contents */
  VERIFY(verifyIteratorContents(iter, size, bias) == 0);
  
  Uint32 sz;
  
  /* Check that iterator is empty now */
  VERIFY(iter.getNextWords(sz) == NULL);
  VERIFY(sz == 0);
    
  VERIFY(iter.getNextWords(sz) == NULL);
  VERIFY(sz == 0);
  
  iter.reset();
  
  /* Verify reset put us back to the start */
  VERIFY(verifyIteratorContents(iter, size, bias) == 0);
  
  /* Verify no more words available */
  VERIFY(iter.getNextWords(sz) == NULL);
  VERIFY(sz == 0);  
  
  return 0;
}

int
checkIterator(GenericSectionIterator& iter, int size, int bias)
{
  /* Test iterator itself, and then FragmentedSectionIterator
   * adaptation
   */
  VERIFY(checkGenericSectionIterator(iter, size, bias) == 0);
  
  /* Now we'll test the FragmentedSectionIterator on the iterator
   * we were passed
   */
  const int subranges= 20;
  
  iter.reset();
  GenericSectionPtr ptr;
  ptr.sz= size;
  ptr.sectionIter= &iter;
  FragmentedSectionIterator fsi(ptr);

  for (int s=0; s< subranges; s++)
  {
    Uint32 start= 0;
    Uint32 len= 0;
    if (size > 0)
    {
      start= (Uint32) (rand() % size);
      if (0 != (size-start)) 
        len= (Uint32) (rand() % (size-start));
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


int
testLinearSectionIterator()
{
  /* Test Linear section iterator of various
   * lengths with section[n] == bias + n
   */
  const int totalSize= 200000;
  const int bias= 13;

  Uint32 data[totalSize];
  for (int i=0; i<totalSize; i++)
    data[i]= bias + i;

  for (int len= 0; len < 5000; len++)
  {
    LinearSectionIterator something(data, len);

    VERIFY(checkIterator(something, len, bias) == 0);
  }

  return 0;
}

NdbApiSignal*
createSignalChain(NdbApiSignal*& poolHead, int length, int bias)
{
  /* Create signal chain, with word[n] == bias+n */
  NdbApiSignal* chainHead= NULL;
  NdbApiSignal* chainTail= NULL;
  int pos= 0;
  int signals= 0;

  while (pos < length)
  {
    int offset= pos % NdbApiSignal::MaxSignalWords;
    
    if (offset == 0)
    {
      if (poolHead == NULL)
        return 0;

      NdbApiSignal* newSig= poolHead;
      poolHead= poolHead->next();
      signals++;

      newSig->next(NULL);

      if (chainHead == NULL)
      {
        chainHead= chainTail= newSig;
      }
      else
      {
        chainTail->next(newSig);
        chainTail= newSig;
      }
    }
    
    chainTail->getDataPtrSend()[offset]= (bias + pos);
    chainTail->setLength(offset + 1);
    pos ++;
  }

  return chainHead;
}
    
int
testSignalSectionIterator()
{
  /* Create a pool of signals, build
   * signal chains from it, test
   * the iterator against the signal chains
   */
  const int totalNumSignals= 100;
  NdbApiSignal* poolHead= NULL;

  /* Allocate some signals */
  for (int i=0; i < totalNumSignals; i++)
  {
    NdbApiSignal* sig= new NdbApiSignal((BlockReference) 0);

    if (poolHead == NULL)
    {
      poolHead= sig;
      sig->next(NULL);
    }
    else
    {
      sig->next(poolHead);
      poolHead= sig;
    }
  }

  const int bias= 7;
  for (int dataWords= 1; 
       dataWords <= (int)(totalNumSignals * 
                          NdbApiSignal::MaxSignalWords); 
       dataWords ++)
  {
    NdbApiSignal* signalChain= NULL;
    
    VERIFY((signalChain= createSignalChain(poolHead, dataWords, bias)) != NULL );
    
    SignalSectionIterator ssi(signalChain);
    
    VERIFY(checkIterator(ssi, dataWords, bias) == 0);
    
    /* Now return the signals to the pool */
    while (signalChain != NULL)
    {
      NdbApiSignal* sig= signalChain;
      signalChain= signalChain->next();
      
      sig->next(poolHead);
      poolHead= sig;
    }
  }
  
  /* Free signals from pool */
  while (poolHead != NULL)
  {
    NdbApiSignal* sig= poolHead;
    poolHead= sig->next();
    delete(sig);
  }
  
  return 0;
}

int main()
{
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

  plan(2); // Number of tests

  ok1(testLinearSectionIterator() == 0);
  ok1(testSignalSectionIterator() == 0);

  return exit_status();
}


#endif
