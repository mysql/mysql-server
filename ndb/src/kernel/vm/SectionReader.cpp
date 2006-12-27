/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
    m_head = 0;
    m_currentSegment = 0;
  } else {
    m_pos = 0;
    m_len = ptr.p->m_sz;
    m_head = ptr.p;
    m_currentSegment = ptr.p;
  }
}

void
SectionReader::reset(){
  m_pos = 0;
  m_currentSegment = m_head;
}

bool
SectionReader::step(Uint32 len){
  if(m_pos + len >= m_len) {
    m_pos++;
    return false;
  }
  while(len > SectionSegment::DataLength){
    m_currentSegment = m_pool.getPtr(m_currentSegment->m_nextSegment);

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
      m_currentSegment = m_pool.getPtr(m_currentSegment->m_nextSegment);
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

  m_pos += len;
  m_currentSegment = p;
  return true;
}

