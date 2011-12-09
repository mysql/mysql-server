/*
   Copyright (C) 2003-2006, 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

  /* Write struct iovec into buffer. */
  inline Uint32 writev(const struct iovec *vec, int count);

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
 
inline
Uint32
SHM_Writer::writev(const struct iovec *vec, int count)
{
  Uint32 tReadIndex  = * m_sharedReadIndex;
  Uint32 tWriteIndex = m_writeIndex;

  /**
   * Loop over iovec entries, copying into the shared memory buffer.
   *
   * The free buffer space may be split with one part after currently used data
   * and one part before. Dealing with this is complicated by the way that the
   * SHM transporter is designed, it assumes signals are never split. So
   * buffer wrap-over is defined at the end of the first signal to cross
   * m_bufferSize (there is extra slack in the buffer to make this possible).
   *
   * This means that we need to scan the signal data to find the correct place
   * to wrap over in the buffer.
   */
  Uint32 total = 0;
  for (int i = 0; i < count; i++)
  {
    unsigned char *ptr = (unsigned char *)vec[i].iov_base;
    Uint32 remain = vec[i].iov_len;
    Uint32 segment;
    Uint32 maxBytes;
    if (tReadIndex <= tWriteIndex)
    {
      /* Free buffer is split in two. */
      if (tWriteIndex + remain > m_bufferSize)
        maxBytes = (m_bufferSize - tWriteIndex)/4;
      else
        maxBytes = remain/4;
      segment = 4*TransporterRegistry::unpack_length_words((Uint32 *)ptr,
                                                           maxBytes/4);
      if (segment > 0)
        memcpy(m_startOfBuffer + tWriteIndex, ptr, segment);
      remain -= segment;
      total += segment;
      ptr += segment;
      tWriteIndex = 0;
      if (remain > 0)
      {
        if (remain > tReadIndex)
          maxBytes = tReadIndex;
        else
          maxBytes = remain;
        segment = 4*TransporterRegistry::unpack_length_words((Uint32 *)ptr,
                                                             maxBytes/4);
        if (segment > 0)
          memcpy(m_startOfBuffer, ptr, segment);
        total += segment;
        tWriteIndex = segment;
        if (remain > segment)
          break;                                // No more room
      }
    }
    else
    {
      if (tWriteIndex + remain > tReadIndex)
        maxBytes = tReadIndex - tWriteIndex;
      else
        maxBytes = remain;
      segment = 4*TransporterRegistry::unpack_length_words((Uint32 *)ptr,
                                                           maxBytes/4);
      if (segment > 0)
        memcpy(m_startOfBuffer + tWriteIndex, ptr, segment);
      total += segment;
      tWriteIndex += segment;
      if (remain > segment)
        break;                                  // No more room
    }
  }

  m_writeIndex = tWriteIndex;
  *m_sharedWriteIndex = tWriteIndex;

  return total;
}

#endif
