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

#include <SimpleProperties.hpp>
#include <TransporterDefinitions.hpp>
#include "LongSignal.hpp"

SimplePropertiesSectionReader::SimplePropertiesSectionReader
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
  first();
}
  
void
SimplePropertiesSectionReader::reset(){
  m_pos = 0;
  m_currentSegment = m_head;
}

bool
SimplePropertiesSectionReader::step(Uint32 len){
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
SimplePropertiesSectionReader::getWord(Uint32 * dst){
  if (peekWord(dst)) {
    step(1);
    return true;
  }
  return false;
}

bool
SimplePropertiesSectionReader::peekWord(Uint32 * dst) const {
  if(m_pos < m_len){
    Uint32 ind = m_pos % SectionSegment::DataLength; 
    * dst = m_currentSegment->theData[ind];
    return true;
  }
  return false;
}

bool
SimplePropertiesSectionReader::peekWords(Uint32 * dst, Uint32 len) const {
  if(m_pos + len > m_len){
    return false;
  }
  Uint32 ind = (m_pos % SectionSegment::DataLength);
  Uint32 left = (SectionSegment::DataLength - ind);
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
SimplePropertiesSectionReader::getWords(Uint32 * dst, Uint32 len){
  if(peekWords(dst, len)){
    step(len);
    return true;
  }
  return false;
}

SimplePropertiesSectionWriter::SimplePropertiesSectionWriter(class SectionSegmentPool & pool)
  : m_pool(pool)
{
  Ptr<SectionSegment> first;
  if(m_pool.seize(first)){
    ;
  } else {
    m_pos = -1;
    m_head = 0;
    m_currentSegment = 0;
    m_prevPtrI = RNIL;
    return;
  }
  m_sz = 0;
  m_pos = 0;
  m_head = first.p;
  m_head->m_lastSegment = first.i;
  m_currentSegment = first.p;
  m_prevPtrI = RNIL;
}
  
bool
SimplePropertiesSectionWriter::reset(){
  if(m_pos >= 0){
    m_pos = 0;
    return true;
  }
  return false;
}

bool
SimplePropertiesSectionWriter::putWord(Uint32 val){
  return putWords(&val, 1);
}

bool
SimplePropertiesSectionWriter::putWords(const Uint32 * src, Uint32 len){
  Uint32 left = SectionSegment::DataLength - m_pos;
  
  while(len >= left){
    memcpy(&m_currentSegment->theData[m_pos], src, 4 * left);
    Ptr<SectionSegment> next;    
    if(m_pool.seize(next)){

      m_prevPtrI = m_currentSegment->m_lastSegment;
      m_currentSegment->m_nextSegment = next.i;
      next.p->m_lastSegment = next.i;
      m_currentSegment = next.p;
      
      len -= left;
      src += left;
      m_sz += left;
      
      left = SectionSegment::DataLength;
      m_pos = 0;
    } else {
      abort();
      return false;
    }
  }

  memcpy(&m_currentSegment->theData[m_pos], src, 4 * len);
  m_sz += len;
  m_pos += len;
  
  assert(m_pos < (int)SectionSegment::DataLength);

  return true;
}

void
SimplePropertiesSectionWriter::getPtr(struct SegmentedSectionPtr & dst){
  // Set last ptr and size
  if(m_pos >= 0){
    dst.p = m_head;
    dst.i = m_head->m_lastSegment;
    dst.sz = m_sz;
    m_head->m_sz = m_sz;
    m_head->m_lastSegment = m_currentSegment->m_lastSegment;

    if((m_pos % SectionSegment::DataLength) == 0){
      m_pool.release(m_currentSegment->m_lastSegment);
      m_head->m_lastSegment = m_prevPtrI;
    }

    m_sz = 0;
    m_pos = -1;
    m_head = m_currentSegment = 0;
    m_prevPtrI = RNIL;
    return ;
  }
  dst.p = 0;
  dst.sz = 0;
  dst.i = RNIL;

  m_pool.release(m_head->m_lastSegment);
  
  m_sz = 0;
  m_pos = -1;
  m_head = m_currentSegment = 0;
  m_prevPtrI = RNIL;
}
