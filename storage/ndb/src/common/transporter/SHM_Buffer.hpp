/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef SHM_BUFFER_HPP
#define SHM_BUFFER_HPP

#include "util/require.h"
#include <ndb_global.h>
#include "portlib/mt-asm.h"
#include "transporter/TransporterRegistry.hpp"

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
  SHM_Reader() :
    m_startOfBuffer(nullptr),
    m_readIndex(0)
  {
  }
  SHM_Reader(char * const _startOfBuffer,
	     Uint32 _sizeOfBuffer,
	     Uint32 _slack,
	     Uint32 * _readIndex,
	     Uint32 * _writeIndex) :
    m_startOfBuffer(_startOfBuffer),
    m_totalBufferSize(_sizeOfBuffer),
    m_bufferSize(_sizeOfBuffer - _slack),
    m_readIndex(0),
    m_sharedReadIndex(_readIndex),
    m_sharedWriteIndex(_writeIndex)
  {
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
  inline void getReadPtr(Uint32 * & ptr,
                         Uint32 * & eod,
                         Uint32 * & end);

  /**
   * Update read ptr
   * Return number of bytes read
   */
  inline Uint32 updateReadPtr(Uint32 *ptr);
  inline Uint32 getReadIndex()
  {
    return m_readIndex;
  }
  inline Uint32 getWriteIndex()
  {
    return *m_sharedWriteIndex;
  }
  
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
SHM_Reader::empty() const
{
  rmb();
  bool ret = (m_readIndex == * m_sharedWriteIndex);
  return ret;
}

/**
 * Get read pointer
 *
 *  returns ptr - where to start reading
 */
inline 
void
SHM_Reader::getReadPtr(Uint32 * & ptr,
                       Uint32 * & eod,
                       Uint32 * & end)
{
  rmb();
  Uint32 tReadIndex  = m_readIndex;
  Uint32 tWriteIndex = * m_sharedWriteIndex;

  ptr = (Uint32*)&m_startOfBuffer[tReadIndex];

  /**
   * When reading we move the tail forward and we can
   * read until tail meets the head.
   *
   * Read index and write index equal to each other means there
   * is nothing to read and will be found since readPtr will be
   * equal to endPtr and eodPtr.
   */
  assert(tWriteIndex < m_bufferSize);
  if (tReadIndex <= tWriteIndex)
  {
    eod = (Uint32*)&m_startOfBuffer[tWriteIndex];
    end = (Uint32*)&m_startOfBuffer[tWriteIndex];
  }
  else
  {
    eod = (Uint32*)&m_startOfBuffer[m_totalBufferSize];
    end = (Uint32*)&m_startOfBuffer[m_bufferSize];
  }
}

/**
 * Update read ptr
 */
inline
Uint32
SHM_Reader::updateReadPtr(Uint32 *ptr)
{
  Uint32 prevReadIndex = m_readIndex;
  Uint32 tReadIndex = ((char*)ptr) - m_startOfBuffer;
  Uint32 size_read = tReadIndex - prevReadIndex;

  assert((size_read % 4) == 0);
  assert(tReadIndex < m_totalBufferSize);

  if (unlikely(tReadIndex >= m_bufferSize))
  {
    tReadIndex = 0;
  }

  wmb();
  m_readIndex = tReadIndex;
  * m_sharedReadIndex = tReadIndex;
  return size_read;
}

class SHM_Writer {
public:
  SHM_Writer() :
    m_startOfBuffer(nullptr),
    m_totalBufferSize(0),
    m_bufferSize(0),
    m_sharedWriteIndex(nullptr)
  {
  }
  SHM_Writer(char * const _startOfBuffer,
	     Uint32 _sizeOfBuffer,
	     Uint32 _slack,
	     Uint32 * _readIndex,
	     Uint32 * _writeIndex) :
    m_startOfBuffer(_startOfBuffer),
    m_totalBufferSize(_sizeOfBuffer),
    m_bufferSize(_sizeOfBuffer - _slack),
    m_writeIndex(0),
    m_sharedReadIndex(_readIndex),
    m_sharedWriteIndex(_writeIndex)
  {
  }

  inline Uint32 getWriteIndex() const { return m_writeIndex;}
  inline Uint32 getReadIndex()
  {
    return *m_sharedReadIndex;
  }
  inline Uint32 getBufferSize() const { return m_bufferSize;}
  inline Uint32 get_free_buffer() const;
  
  inline void copyIndexes(SHM_Writer * standbyWriter);

  /* Write struct iovec into buffer. */
  inline Uint32 writev(const struct iovec *vec, int count);

private:
  char * const m_startOfBuffer;
  const Uint32 m_totalBufferSize;
  const Uint32 m_bufferSize;
  
  Uint32 m_writeIndex;
  
  const Uint32 * m_sharedReadIndex;
  Uint32 * const m_sharedWriteIndex;
};

inline
Uint32
SHM_Writer::get_free_buffer() const
{
  rmb();
  Uint32 tReadIndex  = * m_sharedReadIndex;
  Uint32 tWriteIndex = m_writeIndex;
  
  Uint32 free;
  if (tReadIndex <= tWriteIndex)
  {
    assert(tWriteIndex < m_bufferSize);
    free = m_bufferSize + tReadIndex - tWriteIndex;
  }
  else
  {
    free = tReadIndex - tWriteIndex;
  }
  assert(free >= 4);
  /**
   * We cannot write the last word, so remove that from
   * free area.
   */
  return free - 4;
}
 
inline
Uint32
SHM_Writer::writev(const struct iovec *vec, int count)
{
  rmb();
  Uint32 tReadIndex  = * m_sharedReadIndex;
  Uint32 tWriteIndex = m_writeIndex;

  if (unlikely(tReadIndex == 0))
  {
    /**
     * When writing it is important to avoid that tail meets the
     * head. If read index is 0 it means that we cannot write
     * such that we need to set write index to 0. To avoid this
     * we set read index to the buffer size. This means that the
     * writer will not write beyond the buffer size and thus will
     * not attempt to set write index to 0.
     *
     * This will work fine also at first write and when the buffer
     * is empty and both read and write index is at 0. In this
     * case we can write the entire buffer, but we cannot write
     * from 0 to 0. This would not work.
     */
    tReadIndex = m_bufferSize;
  }
  assert(tWriteIndex < m_bufferSize);
  assert((tWriteIndex % 4) == 0); /* Index always on word boundaries */
  assert((tReadIndex % 4) == 0);
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
      /**
       * Free buffer is split in two.
       * We will start by writing as if the end is at m_bufferSize.
       * If there is more remaining to be written we will continue
       * writing from 0 still ensuring that we don't write so much
       * head meets tail.
       */
      bool extra = false;
      if (likely(tWriteIndex + remain <= m_bufferSize))
      {
        maxBytes = remain;
      }
      else
      {
        maxBytes = (m_bufferSize - tWriteIndex);
        extra = true;
      }
      segment = 4*TransporterRegistry::unpack_length_words((Uint32 *)ptr,
                                                           maxBytes/4,
                                                           extra);
      memcpy(m_startOfBuffer + tWriteIndex, ptr, segment);
      require(remain >= segment);
      remain -= segment;
      total += segment;
      ptr += segment;
      tWriteIndex += segment;
      if (unlikely(tWriteIndex >= m_bufferSize))
      {
        tWriteIndex = 0;
        if (remain > 0)
        {
          if (remain < tReadIndex)
          {
            maxBytes = remain;
          }
          else
          {
            maxBytes = (tReadIndex - 4);
          }
          segment = 4*TransporterRegistry::unpack_length_words((Uint32 *)ptr,
                                                               maxBytes/4,
                                                               false);
          memcpy(m_startOfBuffer, ptr, segment);
          total += segment;
          tWriteIndex = segment;
          assert(tWriteIndex < tReadIndex);
          if (remain > segment)
            break;                                // No more room
        }
      }
      else
      {
        assert(remain == 0);
      }
    }
    else
    {
      if ((tWriteIndex + remain) < tReadIndex)
      {
        maxBytes = remain;
      }
      else
      {
        assert(tReadIndex >= (tWriteIndex + 4));
        maxBytes = (tReadIndex - tWriteIndex) - 4;
      }
      segment = 4*TransporterRegistry::unpack_length_words((Uint32 *)ptr,
                                                           maxBytes/4,
                                                           false);
      memcpy(m_startOfBuffer + tWriteIndex, ptr, segment);
      total += segment;
      tWriteIndex += segment;
      assert(tWriteIndex < tReadIndex);
      if (remain > segment)
        break;                                  // No more room
    }
  }
  wmb();
  m_writeIndex = tWriteIndex;
  *m_sharedWriteIndex = tWriteIndex;

  return total;
}

#endif
