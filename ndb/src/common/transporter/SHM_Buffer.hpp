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

#ifndef SHM_BUFFER_HPP
#define SHM_BUFFER_HPP

#include <ndb_global.h>

#include <NdbSleep.h>

/**
 * These classes implement a circular buffer
 *
 * One reader and one writer
 */

/**
 * SHM_Reader
 *
 * Use as follows:
 *  getReadPtr(ptr, sz);
 *  for(int i = 0; i<sz; i++)
 *    printf("%c\n", ptr[i]);
 *  updateReadPtr(sz);
 */
class SHM_Reader {
public:
  SHM_Reader(char * const _startOfBuffer,
	     Uint32 _sizeOfBuffer,
	     Uint32 _slack,
	     Uint32 * _readIndex,
	     Uint32 * _writeIndex) :
    m_startOfBuffer(_startOfBuffer),
    m_totalBufferSize(_sizeOfBuffer),
    m_bufferSize(_sizeOfBuffer - _slack),
    m_sharedReadIndex(_readIndex),
    m_sharedWriteIndex(_writeIndex)
  {
  }
  
  void clear() {
    m_readIndex = 0;
  }
  
  /**
   * 
   */
  inline bool empty() const;
  
  /**
   * Get read pointer
   *
   *  returns ptr - where to start reading
   *           sz - how much can I read
   */
  inline void getReadPtr(Uint32 * & ptr, Uint32 * & eod);

  /**
   * Update read ptr
   */
  inline void updateReadPtr(Uint32 *ptr);
  
private:
  char * const m_startOfBuffer;
  Uint32 m_totalBufferSize;
  Uint32 m_bufferSize;
  Uint32 m_readIndex;

  Uint32 * m_sharedReadIndex;
  Uint32 * m_sharedWriteIndex;
};

inline 
bool
SHM_Reader::empty() const{
  bool ret = (m_readIndex == * m_sharedWriteIndex);
  return ret;
}

/**
 * Get read pointer
 *
 *  returns ptr - where to start reading
 *           sz - how much can I read
 */
inline 
void
SHM_Reader::getReadPtr(Uint32 * & ptr, Uint32 * & eod)
{
  Uint32 tReadIndex  = m_readIndex;
  Uint32 tWriteIndex = * m_sharedWriteIndex;
  
  ptr = (Uint32*)&m_startOfBuffer[tReadIndex];
  
  if(tReadIndex <= tWriteIndex){
    eod = (Uint32*)&m_startOfBuffer[tWriteIndex];
  } else {
    eod = (Uint32*)&m_startOfBuffer[m_bufferSize];
  }
}

/**
 * Update read ptr
 */
inline
void 
SHM_Reader::updateReadPtr(Uint32 *ptr)
{
  Uint32 tReadIndex = ((char*)ptr) - m_startOfBuffer;

  assert(tReadIndex < m_totalBufferSize);

  if(tReadIndex >= m_bufferSize){
    tReadIndex = 0;
  }

  m_readIndex = tReadIndex;
  * m_sharedReadIndex = tReadIndex;
}

#define WRITER_SLACK 4

class SHM_Writer {
public:
  SHM_Writer(char * const _startOfBuffer,
	     Uint32 _sizeOfBuffer,
	     Uint32 _slack,
	     Uint32 * _readIndex,
	     Uint32 * _writeIndex) :
    m_startOfBuffer(_startOfBuffer),
    m_totalBufferSize(_sizeOfBuffer),
    m_bufferSize(_sizeOfBuffer - _slack),
    m_sharedReadIndex(_readIndex),
    m_sharedWriteIndex(_writeIndex)
  {
  }
  
  void clear() {
    m_writeIndex = 0;
  }
    
  inline char * getWritePtr(Uint32 sz);
  inline void updateWritePtr(Uint32 sz);

  inline Uint32 getWriteIndex() const { return m_writeIndex;}
  inline Uint32 getBufferSize() const { return m_bufferSize;}
  inline Uint32 get_free_buffer() const;
  
  inline void copyIndexes(SHM_Writer * standbyWriter);

private:
  char * const m_startOfBuffer;
  Uint32 m_totalBufferSize;
  Uint32 m_bufferSize;
  
  Uint32 m_writeIndex;
  
  Uint32 * m_sharedReadIndex;
  Uint32 * m_sharedWriteIndex;
};

inline
char *
SHM_Writer::getWritePtr(Uint32 sz){
  Uint32 tReadIndex  = * m_sharedReadIndex;
  Uint32 tWriteIndex = m_writeIndex;
  
  char * ptr = &m_startOfBuffer[tWriteIndex];

  Uint32 free;
  if(tReadIndex <= tWriteIndex){
    free = m_bufferSize + tReadIndex - tWriteIndex;
  } else {
    free = tReadIndex - tWriteIndex;
  }
  
  sz += 4;
  if(sz < free){
    return ptr;
  }
  
  return 0;
}

inline
void 
SHM_Writer::updateWritePtr(Uint32 sz){

  assert(m_writeIndex == * m_sharedWriteIndex);

  Uint32 tWriteIndex = m_writeIndex;
  tWriteIndex += sz;
  
  assert(tWriteIndex < m_totalBufferSize);

  if(tWriteIndex >= m_bufferSize){
    tWriteIndex = 0;
  }

  m_writeIndex = tWriteIndex;
  * m_sharedWriteIndex = tWriteIndex;
}

inline
Uint32
SHM_Writer::get_free_buffer() const
{
  Uint32 tReadIndex  = * m_sharedReadIndex;
  Uint32 tWriteIndex = m_writeIndex;
  
  Uint32 free;
  if(tReadIndex <= tWriteIndex){
    free = m_bufferSize + tReadIndex - tWriteIndex;
  } else {
    free = tReadIndex - tWriteIndex;
  }
  return free;
}
 
#endif
