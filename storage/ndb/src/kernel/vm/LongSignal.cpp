/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "LongSignal.hpp"
#include "LongSignalImpl.hpp"
#include <EventLogger.hpp>

extern EventLogger * g_eventLogger;

#define JAM_FILE_ID 262

// Static function.
void 
SectionSegmentPool::handleOutOfSegments(SectionSegment_basepool& pool)
{
  g_eventLogger
    ->warning("The long message buffer is out of free elements. This may "
              "cause the data node to crash. Consider increasing the buffer "
              "size via the LongMessageBuffer configuration parameter. The "
              "current size of this pool is %lu bytes. You may also check "
              "the state of this buffer via the ndbinfo.memoryusage table.", 
              static_cast<unsigned long>
              (pool.getSize() * sizeof(SectionSegment)));
};

/**
 * verifySection
 * Assertion method to check that a segmented section is constructed
 * 'properly' where 'properly' is loosly defined.
 */
bool
verifySection(Uint32 firstIVal, SectionSegmentPool& thePool)
{
  if (firstIVal == RNIL)
    return true;

  /* Get first section ptr (With assertions in getPtr) */
  SectionSegment* first= thePool.getPtr(firstIVal);

  assert(first != NULL);
  Uint32 totalSize= first->m_sz;
#ifdef VM_TRACE
  Uint32 lastSegIVal= first->m_lastSegment;
#endif

  /* Hmm, need to be careful of length == 0
   * Nature abhors a segmented section with length 0
   */
  //assert(totalSize != 0);
  assert(lastSegIVal != RNIL); /* Should never be == RNIL */
  /* We ignore m_ownerRef */

  if (totalSize <= SectionSegment::DataLength)
  {
    /* 1 segment */
    assert(first->m_lastSegment == firstIVal);
    // m_nextSegment not always set to RNIL on last segment
    //assert(first->m_nextSegment == RNIL);
  }
  else
  {
    /* > 1 segment */
    assert(first->m_nextSegment != RNIL);
    assert(first->m_lastSegment != firstIVal);
    Uint32 currIVal= firstIVal;
    SectionSegment* curr= first;

    /* Traverse segments to where we think the end should be */
    while (totalSize > SectionSegment::DataLength)
    {
      currIVal= curr->m_nextSegment;
      curr= thePool.getPtr(currIVal);
      totalSize-= SectionSegment::DataLength;
      /* Ignore m_ownerRef, m_sz, m_lastSegment of intermediate
       * Segments
       */
    }

    /* Once we are here, we are on the last Segment of this Section
     * Check that last segment is as stated in the first segment
     */
    assert(currIVal == lastSegIVal);
    // m_nextSegment not always set properly on last segment
    //assert(curr->m_nextSegment == RNIL);
    /* Ignore m_ownerRef, m_sz, m_lastSegment of last segment */
  }

  return true;
}

void
copy(Uint32 * & insertPtr,
     class SectionSegmentPool & thePool, const SegmentedSectionPtr & _ptr){

  Uint32 len = _ptr.sz;
  SectionSegment * ptrP = _ptr.p;

  assert(verifySection(_ptr.i, thePool));

  while(len > 60){
    memcpy(insertPtr, &ptrP->theData[0], 4 * 60);
    len -= 60;
    insertPtr += 60;
    ptrP = thePool.getPtr(ptrP->m_nextSegment);
  }
  memcpy(insertPtr, &ptrP->theData[0], 4 * len);
  insertPtr += len;
}

void
copy(Uint32 * dst, SegmentedSectionPtr src){
  copy(dst, g_sectionSegmentPool, src);
}

/* Copy variant which takes an IVal */
void
copy(Uint32* dst, Uint32 srcFirstIVal)
{
  SegmentedSectionPtr p;
  getSection(p, srcFirstIVal);

  copy(dst, p);
}

void
print(SectionSegment * s, Uint32 len, FILE* out){
  for(Uint32 i = 0; i<len; i++){
    fprintf(out, "H\'0x%.8x ", s->theData[i]);
    if(((i + 1) % 6) == 0)
      fprintf(out, "\n");
  }
}

void
print(SegmentedSectionPtr ptr, FILE* out){

  ptr.p = g_sectionSegmentPool.getPtr(ptr.i);
  Uint32 len = ptr.p->m_sz;

  fprintf(out, "ptr.i = %d(%p) ptr.sz = %d(%d)\n", ptr.i, ptr.p, len, ptr.sz);
  while(len > SectionSegment::DataLength){
    print(ptr.p, SectionSegment::DataLength, out);

    len -= SectionSegment::DataLength;
    fprintf(out, "ptr.i = %d\n", ptr.p->m_nextSegment);
    ptr.p = g_sectionSegmentPool.getPtr(ptr.p->m_nextSegment);
  }

  print(ptr.p, len, out);
  fprintf(out, "\n");
}

bool
dupSection(SPC_ARG Uint32& copyFirstIVal, Uint32 srcFirstIVal)
{
  assert(verifySection(srcFirstIVal));

  SectionSegment* p= g_sectionSegmentPool.getPtr(srcFirstIVal);
  Uint32 sz= p->m_sz;
  copyFirstIVal= RNIL;
  bool ok= true;

  /* Duplicate bulk of section */
  while (sz > SectionSegment::DataLength)
  {
    ok= appendToSection(SPC_CACHE_ARG copyFirstIVal, &p->theData[0],
                        SectionSegment::DataLength);
    if (!ok)
      break;

    sz-= SectionSegment::DataLength;
    srcFirstIVal= p->m_nextSegment;
    p= g_sectionSegmentPool.getPtr(srcFirstIVal);
  }

  /* Duplicate last segment */
  if (ok && (sz > 0))
    ok= appendToSection(SPC_CACHE_ARG copyFirstIVal, &p->theData[0], sz);

  if (unlikely(!ok))
  {
    releaseSection(SPC_CACHE_ARG copyFirstIVal);
    copyFirstIVal= RNIL;
    return false;
  }

  assert(verifySection(copyFirstIVal));
  return true;
}

bool ErrorImportActive = false;
extern Uint32 ErrorMaxSegmentsToSeize;

/**
 * appendToSection
 * Append supplied words to the chain of section segments
 * indicated by the first section passed.
 * If the passed IVal == RNIL then a section will be seized
 * and the IVal will be updated to indicate the first IVal
 * section in the chain
 * Sections are made up of linked SectionSegment objects
 * where :
 *   - The first SectionSegment's m_sz is the size of the
 *     whole section
 *   - The first SectionSegment's m_lastSegment refers to
 *     the last segment in the section
 *   - Each SectionSegment's m_nextSegment refers to the
 *     next segment in the section, *except* for the last
 *     SectionSegment's which is RNIL
 *   - Each SectionSegment except the first does not use
 *     its m_sz or m_nextSegment members.
 */
bool
appendToSection(SPC_ARG Uint32& firstSegmentIVal, const Uint32* src, Uint32 len)
{
  Ptr<SectionSegment> firstPtr, currPtr;

  if (len == 0)
    return true;

  Uint32 remain= SectionSegment::DataLength;
  Uint32 segmentLen= 0;

#ifdef NDB_DEBUG_RES_OWNERSHIP
  const Uint32 owner = getResOwner();
#else
  const Uint32 owner = 0;
#endif

  if (firstSegmentIVal == RNIL)
  {
#ifdef ERROR_INSERT
    /* Simulate running out of segments */
    if (ErrorImportActive)
    {
      if (ErrorMaxSegmentsToSeize == 0)
      {
        ndbout_c("append exhausted on first segment");
        return false;
      }
    }
#endif
    /* First data to be added to this section */
    bool result= g_sectionSegmentPool.seize(SPC_SEIZE_ARG firstPtr);

    if (!result)
      return false;

    firstPtr.p->m_sz= 0;
    firstPtr.p->m_ownerRef= owner;
    firstSegmentIVal= firstPtr.i;

    currPtr= firstPtr;
  }
  else
  {
    /* Section has at least one segment with data already */
    g_sectionSegmentPool.getPtr(firstPtr, firstSegmentIVal);
    g_sectionSegmentPool.getPtr(currPtr, firstPtr.p->m_lastSegment);

    Uint32 existingLen= firstPtr.p->m_sz;
    assert(existingLen > 0);
    segmentLen= existingLen % SectionSegment::DataLength;

    /* If existingLen %  SectionSegment::DataLength == 0
     * we assume that the last segment is full
     */
    segmentLen= segmentLen == 0 ?
      SectionSegment::DataLength :
      segmentLen;

    remain= SectionSegment::DataLength - segmentLen;
  }

  firstPtr.p->m_sz+= len;

#ifdef ERROR_INSERT
  Uint32 remainSegs= (Uint32) ErrorMaxSegmentsToSeize - 1;
#endif

  while(len > remain) {
    /* Fill this segment, and link in another one
     * Note that we can memcpy to a bad address with size 0
     */
    memcpy(&currPtr.p->theData[segmentLen], src, remain << 2);
    src += remain;
    len -= remain;
    Ptr<SectionSegment> prevPtr= currPtr;

#ifdef ERROR_INSERT
    /* Simulate running out of segments */
    if (ErrorImportActive)
    {
      if (0 == remainSegs--)
      {
        ndbout_c("Append exhausted on segment %d", ErrorMaxSegmentsToSeize);
        firstPtr.p->m_lastSegment= prevPtr.i;
        firstPtr.p->m_sz-= len;
        return false;
      }
    }
#endif
    bool result = g_sectionSegmentPool.seize(SPC_SEIZE_ARG currPtr);
    if (!result)
    {
      /* Failed, ensure segment list is consistent for
       * freeing later
       */
      firstPtr.p->m_lastSegment= prevPtr.i;
      firstPtr.p->m_sz-= len;
      return false;
    }
    prevPtr.p->m_nextSegment = currPtr.i;
    currPtr.p->m_sz= 0;
    currPtr.p->m_ownerRef= owner;

    segmentLen= 0;
    remain= SectionSegment::DataLength;
  }

  /* Data fits in the current last segment */
  firstPtr.p->m_lastSegment= currPtr.i;
  currPtr.p->m_nextSegment= RNIL;
  memcpy(&currPtr.p->theData[segmentLen], src, len << 2);

  return true;
}
bool
import(SPC_ARG Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len){

#ifdef ERROR_INSERT
  /* Simulate running out of segments */
  if (ErrorImportActive)
  {
    if (ErrorMaxSegmentsToSeize == 0)
    {
      ndbout_c("Import exhausted on first segment");
      return false;
    }
  }
#endif

#ifdef NDB_DEBUG_RES_OWNERSHIP
  const Uint32 owner = getResOwner();
#else
  const Uint32 owner = 0;
#endif

  first.p = 0;
  if(g_sectionSegmentPool.seize(SPC_SEIZE_ARG first)){
    ;
  } else {
    ndbout_c("No Segmented Sections for import");
    return false;
  }

  first.p->m_sz = len;
  first.p->m_ownerRef = owner;

  Ptr<SectionSegment> currPtr = first;

#ifdef ERROR_INSERT
  Uint32 remainSegs= (Uint32) ErrorMaxSegmentsToSeize - 1;
#endif

  while(len > SectionSegment::DataLength){
    memcpy(&currPtr.p->theData[0], src, 4 * SectionSegment::DataLength);
    src += SectionSegment::DataLength;
    len -= SectionSegment::DataLength;
    Ptr<SectionSegment> prevPtr = currPtr;

#ifdef ERROR_INSERT
    /* Simulate running out of segments */
    if (ErrorImportActive)
    {
      if (0 == remainSegs--)
      {
        ndbout_c("Import exhausted on segment %d", 
                 ErrorMaxSegmentsToSeize);
        first.p->m_lastSegment= prevPtr.i;
        first.p->m_sz-= len;
        prevPtr.p->m_nextSegment = RNIL;
        return false;
      }
    }
#endif

    if(g_sectionSegmentPool.seize(SPC_SEIZE_ARG currPtr)){
      prevPtr.p->m_nextSegment = currPtr.i;
      currPtr.p->m_ownerRef = owner;
      ;
    } else {
      /* Leave segment chain in ok condition for release */
      first.p->m_lastSegment = prevPtr.i;
      first.p->m_sz-= len;
      prevPtr.p->m_nextSegment = RNIL;
      ndbout_c("Not enough Segmented Sections during import");
      return false;
    }
  }

  first.p->m_lastSegment = currPtr.i;
  currPtr.p->m_nextSegment = RNIL;
  memcpy(&currPtr.p->theData[0], src, 4 * len);

  assert(verifySection(first.i));
  return true;
}

void
release(SPC_ARG SegmentedSectionPtr & ptr)
{
  g_sectionSegmentPool.releaseList(SPC_SEIZE_ARG
                                   relSz(ptr.sz),
				   ptr.i,
				   ptr.p->m_lastSegment);
}

void
releaseSection(SPC_ARG Uint32 firstSegmentIVal)
{
  if (firstSegmentIVal != RNIL)
  {
    SectionSegment* p = g_sectionSegmentPool.getPtr(firstSegmentIVal);

    g_sectionSegmentPool.releaseList(SPC_SEIZE_ARG
                                     relSz(p->m_sz),
                                     firstSegmentIVal,
                                     p->m_lastSegment);
  }
}

bool
writeToSection(Uint32 firstSegmentIVal, Uint32 offset,
               const Uint32* src,
               Uint32 len)
{
  Ptr<SectionSegment> segPtr;

  if (len == 0)
    return true;

  if (firstSegmentIVal == RNIL)
  {
    return false;
  }
  else
  {
    /* Section has at least one segment with data already */
    g_sectionSegmentPool.getPtr(segPtr, firstSegmentIVal);

    Uint32 existingLen= segPtr.p->m_sz;

    assert(existingLen > 0);
    if (offset >= existingLen)
      return false;         /* No sparse sections or extension */
    if (len > (existingLen - offset))
      return false;         /* Would be extending beyond current length */

    /* Advance through segments to the one containing the start offset */
    while (offset >= SectionSegment::DataLength)
    {
      g_sectionSegmentPool.getPtr(segPtr, segPtr.p->m_nextSegment);
      offset-= SectionSegment::DataLength;
    }

    /* Now overwrite the words */
    while (true)
    {
      Uint32 wordsToCopy = MIN(len,
                               SectionSegment::DataLength - offset);
      memcpy(&segPtr.p->theData[offset], src, (wordsToCopy << 2));
      src+= wordsToCopy;
      len-= wordsToCopy;

      if (!len)
      {
        return true;
      }

      offset = 0;
      g_sectionSegmentPool.getPtr(segPtr, segPtr.p->m_nextSegment);
    }
  }
}

#ifdef NDB_DEBUG_RES_OWNERSHIP

void setResOwner(Uint32 id)
{
  NDB_THREAD_TLS_RES_OWNER = id;
}

Uint32 getResOwner()
{
  return NDB_THREAD_TLS_RES_OWNER;
}

#endif

/** 
 * #undef is needed since this file is included by LongSignal_nonmt.cpp
 * and LongSignal_mt.cpp
 */
#undef JAM_FILE_ID
