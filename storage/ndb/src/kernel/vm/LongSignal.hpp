/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef LONG_SIGNAL_HPP
#define LONG_SIGNAL_HPP

#include "pc.hpp"
#include <ArrayPool.hpp>
#include "DataBuffer.hpp"

#define JAM_FILE_ID 288


/**
 * Section handling
 */
struct SectionSegment {

  STATIC_CONST( DataLength = NDB_SECTION_SEGMENT_SZ );
  
  union {
    Uint32 m_sz;
    Uint32 chunkSize;
  };
  union {
    Uint32 m_ownerRef;
    Uint32 nextChunk;
  };
  union {
    Uint32 m_lastSegment;
    Uint32 lastChunk;  // 
  };
  union {
    Uint32 m_nextSegment;
    Uint32 nextPool;
  };
  Uint32 theData[DataLength];
};

/**
 * Pool for SectionSegments
 */
class SectionSegmentPool : public ArrayPool<SectionSegment> 
{
private:
  // Print an informative error message.
  static void handleOutOfSegments(ArrayPool<SectionSegment>& pool);
public:
  SectionSegmentPool() : 
    ArrayPool<SectionSegment>(&handleOutOfSegments){}
};

/**
 * And the instance
 */
extern SectionSegmentPool g_sectionSegmentPool;

/**
 * Interface for utils for working with a
 * Section Segment pool - hides details of
 * cache / mt etc.
 */
class SegmentUtils
{
public:
  virtual ~SegmentUtils() {};

  /* 'Provider interface' */
  /* Low level ops needed to build tools */
  virtual SectionSegment* getSegmentPtr(Uint32 iVal) = 0;
  void getSegmentPtr(Ptr<SectionSegment>& ptr, Uint32 iVal);
  virtual bool seizeSegment(Ptr<SectionSegment>& p) = 0;
  virtual void releaseSegment(Uint32 iVal) = 0;

  /* Release a linked list of segments with valid size) */
  virtual void releaseSegmentList(Uint32 iVal) = 0;
};

inline void SegmentUtils::getSegmentPtr(Ptr<SectionSegment>& ptr, Uint32 iVal)
{
  ptr.i = iVal;
  ptr.p = getSegmentPtr(iVal);
}

/* Higher level utils */
/* Currently defined in SegmentList.cpp, should move to somewhere else */
bool sectionAppend(SegmentUtils& su, Uint32& firstSegmentIVal, const Uint32* src, Uint32 len);
bool sectionConsume(SegmentUtils& su, Uint32& firstSegmentIVal, Uint32* dst, Uint32 len);
bool sectionVerify(SegmentUtils& su, Uint32 firstIVal);

/**
 * Function prototypes
 */
void print(SegmentedSectionPtr ptr, FILE* out);
void copy(Uint32 * dst, SegmentedSectionPtr src);
void copy(Uint32 * dst, Uint32 srcFirstIVal);

extern class SectionSegmentPool g_sectionSegmentPool;

/* Defined in SimulatedBlock.cpp */
void getSection(SegmentedSectionPtr & ptr, Uint32 id);
void getSections(Uint32 secCount, SegmentedSectionPtr ptr[3]);
Uint32 getSectionSz(Uint32 id);
Uint32* getLastWordPtr(Uint32 id);

/* Internal verification */
bool verifySection(Uint32 firstIVal, 
                   SectionSegmentPool& thePool= g_sectionSegmentPool);

template<Uint32 sz>
void
append(DataBuffer<sz>& dst, SegmentedSectionPtr ptr, SectionSegmentPool& pool){
  Uint32 len = ptr.sz;
  while(len > SectionSegment::DataLength){
    dst.append(ptr.p->theData, SectionSegment::DataLength);
    ptr.p = pool.getPtr(ptr.p->m_nextSegment);
    len -= SectionSegment::DataLength;
  }
  dst.append(ptr.p->theData, len);
}

#undef JAM_FILE_ID

#endif
