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

#ifndef LONG_SIGNAL_HPP
#define LONG_SIGNAL_HPP

#include "pc.hpp"
#include <ArrayPool.hpp>

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
class SectionSegmentPool : public ArrayPool<SectionSegment> {};

/**
 * And the instance
 */
extern SectionSegmentPool g_sectionSegmentPool;

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

#include "DataBuffer.hpp"

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

#endif
