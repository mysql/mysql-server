/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <SectionReader.hpp>
#include <TransporterDefinitions.hpp>
#include "LongSignal.hpp"

#if 0
  Uint32 m_len;
  class SectionSegmentPool & m_pool;
  class SectionSegment * m_head;
  class SectionSegment * m_currentPos;
#endif

SectionReader::SectionReader
(struct SegmentedSectionPtr & ptr, class SectionSegmentPool & pool)
  : m_pool(pool)
{
  if(ptr.p == 0){
    m_pos = 0;
    m_len = 0;
    m_headI= RNIL;
    m_head = 0;
    m_currI= RNIL;
    m_currentSegment = 0;
  } else {
    m_pos = 0;
    m_len = ptr.p->m_sz;
    m_headI= ptr.i;
    m_head = ptr.p;
    m_currI= ptr.i;
    m_currentSegment = ptr.p;
  }
}

SectionReader::SectionReader
(Uint32 firstSectionIVal, class SectionSegmentPool& pool)
  : m_pool(pool)
{
  SectionSegment* firstSeg= m_pool.getPtr(firstSectionIVal);
  
  m_pos = 0;
  m_len = firstSeg->m_sz;
  m_headI= m_currI= firstSectionIVal;
  m_head = m_currentSegment = firstSeg;
}

void
SectionReader::reset(){
  m_pos = 0;
  m_currI= m_headI;
  m_currentSegment = m_head;
}

bool
SectionReader::step(Uint32 len){
  if(m_pos + len >= m_len) {
    m_pos++;
    return false;
  }
  while(len > SectionSegment::DataLength){
    m_currI= m_currentSegment->m_nextSegment;
    m_currentSegment = m_pool.getPtr(m_currI);

    len -= SectionSegment::DataLength;
    m_pos += SectionSegment::DataLength;
  }

  Uint32 ind = m_pos % SectionSegment::DataLength;
  while(len > 0){
    len--;
    m_pos++;

    ind++;
    if(ind == SectionSegment::DataLength){
      ind = 0;
      m_currI= m_currentSegment->m_nextSegment;
      m_currentSegment = m_pool.getPtr(m_currI);
    }
  }
  return true;
}

bool
SectionReader::getWord(Uint32 * dst){
  if (peekWord(dst)) {
    step(1);
    return true;
  }
  return false;
}

bool
SectionReader::peekWord(Uint32 * dst) const {
  if(m_pos < m_len){
    Uint32 ind = m_pos % SectionSegment::DataLength;
    * dst = m_currentSegment->theData[ind];
    return true;
  }
  return false;
}

bool
SectionReader::updateWord(Uint32 value) const 
{
  if(m_pos < m_len){
    Uint32 ind = m_pos % SectionSegment::DataLength;
    m_currentSegment->theData[ind] = value;
    return true;
  }
  return false;
}

bool
SectionReader::peekWords(Uint32 * dst, Uint32 len) const {
  if(m_pos + len > m_len)
    return false;

  Uint32 ind = (m_pos % SectionSegment::DataLength);
  Uint32 left = SectionSegment::DataLength - ind;
  SectionSegment * p = m_currentSegment;

  while(len > left){
    memcpy(dst, &p->theData[ind], 4 * left);
    dst += left;
    len -= left;
    ind = 0;
    left = SectionSegment::DataLength;
    p = m_pool.getPtr(p->m_nextSegment);
  }

  memcpy(dst, &p->theData[ind], 4 * len);
  return true;
}

bool
SectionReader::getWords(Uint32 * dst, Uint32 len){
  if(m_pos + len > m_len)
    return false;

  /* Use getWordsPtr to correctly traverse segments */

  while (len > 0)
  {
    const Uint32* readPtr;
    Uint32 readLen;

    if (!getWordsPtr(len,
                     readPtr,
                     readLen))
      return false;
    
    memcpy(dst, readPtr, readLen << 2);
    len-= readLen;
    dst+= readLen;
  }

  return true;
}

bool
SectionReader::getWordsPtr(Uint32 maxLen,
                           const Uint32*& readPtr,
                           Uint32& actualLen)
{
  if(m_pos >= m_len)
    return false;

  /* We return a pointer to the current position,
   * with length the minimum of
   *  - significant words remaining in the whole section
   *  - space remaining in the current segment
   *  - maxLen from caller
   */
  const Uint32 sectionRemain= m_len - m_pos;
  const Uint32 startInd = (m_pos % SectionSegment::DataLength);
  const Uint32 segmentSpace = SectionSegment::DataLength - startInd;
  SectionSegment * p = m_currentSegment;

  const Uint32 remain= MIN(sectionRemain, segmentSpace);
  actualLen= MIN(remain, maxLen);
  readPtr= &p->theData[startInd];

  /* If we've read everything in this segment, and
   * there's another one, move onto it ready for 
   * next time
   */
  m_pos += actualLen;

  if (((startInd + actualLen) == SectionSegment::DataLength) &&
      (m_pos < m_len))
  {
    m_currI= p->m_nextSegment;
    m_currentSegment= m_pool.getPtr(m_currI);
  }

  return true;
}

bool
SectionReader::getWordsPtr(const Uint32*& readPtr,
                           Uint32& actualLen)
{
  /* Cannot have more than SectionSegment::DataLength
   * contiguous words
   */
  return getWordsPtr(SectionSegment::DataLength,
                     readPtr,
                     actualLen);
}


SectionReader::PosInfo
SectionReader::getPos()
{
  PosInfo pi;
  pi.currPos= m_pos;
  pi.currIVal= m_currI;
  
  return pi;
}

bool
SectionReader::setPos(PosInfo posInfo)
{
  if (posInfo.currPos > m_len)
    return false;
  
  if (posInfo.currIVal == RNIL)
  {
    if (posInfo.currPos > 0)
      return false;
    m_currentSegment= 0;
  }
  else
  {
    assert(segmentContainsPos(posInfo));
    
    m_currentSegment= m_pool.getPtr(posInfo.currIVal);
  }

  m_pos= posInfo.currPos;
  m_currI= posInfo.currIVal;

  return true;
}

bool
SectionReader::segmentContainsPos(PosInfo posInfo)
{
  /* This is a check that the section referenced 
   * by this SectionReader contains the position 
   * given at the section given.
   * It should not be run in-production
   */
  Uint32 IVal= m_headI;
  Uint32 pos= posInfo.currPos;

  while (pos >= SectionSegment::DataLength)
  {
    /* Get next segment */
    SectionSegment* seg= m_pool.getPtr(IVal);

    IVal= seg->m_nextSegment;
    pos-= SectionSegment::DataLength;
  }

  return (IVal == posInfo.currIVal);
}


#ifdef UNIT_TEST

#define VERIFY(x) if ((x) == 0) { printf("VERIFY failed at Line %u : %s\n",__LINE__, #x);  return -1; }

/* Redefine ArrayPool dependencies to enable standalone Unit-test compile */
void ErrorReporter::handleAssert(const char* message, const char* file, int line, int ec)
{
  printf("Error :\"%s\" at file : %s line %u ec %u\n",
         message, file, line, ec);
  abort();
}

void* ndbd_malloc(size_t size)
{
  return malloc(size);
}

void ndbd_free(void* p, size_t size)
{
  free(p);
}

SectionSegmentPool g_sectionSegmentPool;

/* Create a section of word length given from the
 * Segment Pool supplied
 */
Uint32 createSection(SectionSegmentPool* pool,
                     Uint32 length)
{
  Uint32 pos= 0; 
  
  VERIFY(length > 0);

  Ptr<SectionSegment> first, current;

  VERIFY(pool->seize(first));

  first.p->m_sz= length;
  current= first;
  
  while (length > SectionSegment::DataLength)
  {
    for (Uint32 i=0; i<SectionSegment::DataLength; i++)
      current.p->theData[i]= pos+i;
    
    pos+= SectionSegment::DataLength;
    length-= SectionSegment::DataLength;
    
    SectionSegment* prev= current.p;
    VERIFY(pool->seize(current));
    prev->m_nextSegment= current.i;
  }
  
  if (length > 0)
  {
    for (Uint32 i=0; i< length; i++)
      current.p->theData[i]= pos+i;
  };

  first.p->m_lastSegment= current.i;

  return first.i;
};

bool freeSection(SectionSegmentPool* pool,
                 Uint32 firstIVal)
{
  Ptr<SectionSegment> p;

  p.i= firstIVal;
  pool->getPtr(p);

  const Uint32 segs= (p.p->m_sz + SectionSegment::DataLength -1) / 
    SectionSegment::DataLength;
  
  pool->releaseList(segs, p.i, p.p->m_lastSegment);

  return true;
}

int checkBuffer(const Uint32* buffer,
                Uint32 start,
                Uint32 len)
{
  for (Uint32 i=0; i<len; i++)
  {
    if (buffer[i] != start + i)
      printf("i=%u buffer[i]=%u, start=%u\n",
             i, buffer[i], start);
    VERIFY(buffer[i] == start + i);
  }
  return 0;
};

#include <random.h>

int testSR(Uint32 iVal, SectionSegmentPool* ssp, Uint32 len)
{
  SectionReader srStepPeek(iVal, *ssp);
  SectionReader srGetWord(iVal, *ssp);
  SectionReader srPosSource(iVal, *ssp);
  SectionReader srPosDest(iVal, *ssp);
  SectionReader srPtrWord(iVal, *ssp);

  VERIFY(srStepPeek.getSize() == len);

  /* Reset the section readers at a random position */
  const Uint32 noResetPos= 9999999;
  Uint32 resetAt= len> 10 ? myRandom48(len) : noResetPos;

  /* Read from the section readers, 1 word at a time */
  for (Uint32 i=0; i < len; i++)
  {
    Uint32 peekWord;
    Uint32 getWord;
    const Uint32* ptrWord;
    Uint32 ptrReadSize;

    /* Check that peek, getWord and getWordsPtr return
     * the same, correct value
     */
    VERIFY(srStepPeek.peekWord(&peekWord));
    if (i < (len -1))
        VERIFY(srStepPeek.step(1));
    VERIFY(srGetWord.getWord(&getWord));
    VERIFY(srPtrWord.getWordsPtr(1, ptrWord, ptrReadSize));
    VERIFY(ptrReadSize == 1);
    //printf("PeekWord=%u, i=%u\n",
    //       peekWord, i);
    VERIFY(peekWord == i);
    VERIFY(peekWord == getWord);
    VERIFY(peekWord == *ptrWord);

    /* Check that one sectionReader with it's position
     * set from the first returns the same, correct word
     */
    SectionReader::PosInfo p= srPosSource.getPos();
    srPosDest.setPos(p);
    
    Uint32 srcWord, destWord;

    VERIFY(srPosSource.getWord(&srcWord));
    VERIFY(srPosDest.getWord(&destWord));
    
    VERIFY(srcWord == peekWord);
    VERIFY(srcWord == destWord);

    /* Reset the readers */
    if (i == resetAt)
    {
      //printf("Resetting\n");
      resetAt= noResetPos;
      i= (Uint32) -1;
      srStepPeek.reset();
      srGetWord.reset();
      srPosSource.reset();
      srPosDest.reset();
      srPtrWord.reset();
    }
    else
    {
      if ((myRandom48(400) == 1) &&
          (i < len -1))
      {
        /* Step all readers forward by some amount */
        Uint32 stepSize= myRandom48((len - i) -1 );
        //printf("Stepping %u words\n", stepSize);
        VERIFY(srStepPeek.step(stepSize));
        VERIFY(srGetWord.step(stepSize));
        VERIFY(srPosSource.step(stepSize));
        VERIFY(srPosDest.step(stepSize));
        VERIFY(srPtrWord.step(stepSize));
        i+= stepSize;
      }
    }
  }

  /* Check that there's nothing left in any reader */
  VERIFY(!srStepPeek.step(1));
  VERIFY(!srGetWord.step(1));
  VERIFY(!srPosSource.step(1));
  VERIFY(!srPosDest.step(1));
  VERIFY(!srPtrWord.step(1));

  srStepPeek.reset();
  srGetWord.reset();
  srPosSource.reset();
  srPosDest.reset();
  srPtrWord.reset();
  
  /* Now read larger chunks of words */
  Uint32 pos= 0;
  Uint32* buffer= (Uint32*) malloc(len * 4);

  VERIFY(buffer != NULL);

  while (pos < len)
  {
    const Uint32 remain= len-pos;
    const Uint32 readSize= remain == 1 ? 1 : myRandom48(remain);
    //printf("Pos=%u Len=%u readSize=%u \n", pos, len, readSize);
    /* Check that peek + step get the correct words */
    VERIFY(srStepPeek.peekWords(buffer, readSize));
    if (len > pos + readSize)
    {
      VERIFY(srStepPeek.step(readSize));
    }
    else
      VERIFY(srStepPeek.step((len - pos) - 1));
    
    VERIFY(checkBuffer(buffer, pos, readSize) == 0);

    /* Check that getWords gets the correct words */
    VERIFY(srGetWord.getWords(buffer, readSize));
    VERIFY(checkBuffer(buffer, pos, readSize) == 0);

    /* Check that using getPos + setPos gets the correct words */
    VERIFY(srPosDest.setPos(srPosSource.getPos()));
    VERIFY(srPosSource.getWords(buffer, readSize));
    VERIFY(checkBuffer(buffer, pos, readSize) == 0);

    VERIFY(srPosDest.getWords(buffer, readSize));
    VERIFY(checkBuffer(buffer, pos, readSize) == 0);

    /* Check that getWordsPtr gets the correct words */
    Uint32 ptrWordsRead= 0;
    
    //printf("Reading from ptr\n");
    while (ptrWordsRead < readSize)
    {
      const Uint32* ptr= NULL;
      Uint32 readLen;
      VERIFY(srPtrWord.getWordsPtr((readSize - ptrWordsRead), 
                                   ptr, 
                                   readLen));
      VERIFY(readLen <= readSize);
      //printf("Read %u words, from pos %u, offset %u\n",
      //       readLen, pos, ptrWordsRead);
      VERIFY(checkBuffer(ptr, pos+ ptrWordsRead, readLen) == 0);
      ptrWordsRead+= readLen;
    }

    pos += readSize;
  }
  
  /* Check that there's no more words in any reader */
  VERIFY(!srStepPeek.step(1));
  VERIFY(!srGetWord.step(1));
  VERIFY(!srPosSource.step(1));
  VERIFY(!srPosDest.step(1));
  VERIFY(!srPtrWord.step(1));

  /* Verify that ptr-fetch variants do not fetch beyond
   * the length, even if we ask for more
   */
  srPtrWord.reset();
  Uint32 readWords= 0;
  while (readWords < len)
  {
    const Uint32* readPtr;
    Uint32 wordsRead= 0;
    VERIFY(srPtrWord.getWordsPtr(20, readPtr, wordsRead));
    readWords+= wordsRead;
    VERIFY(readWords <= len);
  }

  free(buffer);
  return 0;
}


int main(int arg, char** argv)
{
  /* Test SectionReader
   * -------------------
   * To run this code : 
   *   cd storage/ndb/src/kernel/vm
   *   make testSectionReader
   *   ./testSectionReader
   *
   * Will print "OK" in success case and return 0
   */

  g_sectionSegmentPool.setSize(1024);
  
  printf("g_sectionSegmentPool size is %u\n",
         g_sectionSegmentPool.getSize());

  const Uint32 Iterations= 2000;
  const Uint32 Sections= 5;
  Uint32 sizes[ Sections ];
  Uint32 iVals[ Sections ];

  for (Uint32 t=0; t < Iterations; t++)
  {
    for (Uint32 i=0; i<Sections; i++)
    {
      Uint32 available= g_sectionSegmentPool.getNoOfFree();
      sizes[i] = available ? 
        myRandom48(SectionSegment::DataLength * 
                   available)
        : 0;

      //if (0 == (sizes[i] % 60))
      //  printf("Iteration %u, section %u, allocating %u words\n",
      //         t, i, sizes[i]);
      if (t % 100 == 0)
        if (i==0)
          printf("\nIteration %u", t);
      
      if (sizes[i] > 0)
      {
        iVals[i]= createSection(&g_sectionSegmentPool, sizes[i]);

        VERIFY(testSR(iVals[i], &g_sectionSegmentPool, sizes[i]) == 0);
      }
      else
        iVals[i]= RNIL;
    }

    
    for (Uint32 i=0; i < Sections; i++)
    {
      if (sizes[i] > 0)
        freeSection(&g_sectionSegmentPool, iVals[i]);
    }
  }
  
  printf("\nOK\n");
  return 0;
}

#endif
