/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef LONG_SIGNAL_HPP
#define LONG_SIGNAL_HPP

#include "pc.hpp"
#include <ArrayPool.hpp>

/**
 * Section handling
 */
struct SectionSegment {

  STATIC_CONST( DataLength = 60 );
  
  Uint32 m_ownerRef;
  Uint32 m_sz;
  Uint32 m_lastSegment;
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
void copy(SegmentedSectionPtr dst, Uint32 * src, Uint32 len);
void copy(Uint32 * dst, SegmentedSectionPtr src);

extern class SectionSegmentPool g_sectionSegmentPool;
void getSection(SegmentedSectionPtr & ptr, Uint32 id);
void linkSegments(Uint32 head, Uint32 tail);

void getSections(Uint32 secCount, SegmentedSectionPtr ptr[3]);
void releaseSections(Uint32 secCount, SegmentedSectionPtr ptr[3]);


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
