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

#ifndef FS_BUFFER_HPP
#define FS_BUFFER_HPP

#include <ndb_global.h>

#define DEBUG(x)

/**
 * A circular data buffer to be used together with the FS
 * 
 * One writer - Typically your block
 *   getWritePtr()
 *   updateWritePtr()
 *
 * One reader - Typically "thread" in your block sending stuff to NDBFS
 *   getReadPtr()
 *   updateReadPtr()
 */
class FsBuffer {
public:  
  /**
   * Default constructor
   */
  FsBuffer();

  /**
   * setup FsBuffer
   *
   * @param Buffer    - Ptr to continuous memory
   * @param Size      - Buffer size in 32-bit words
   * @param BlockSize - Size of block in 32-bit words
   * @param MinRead   - Min read size in 32-bit words
   *                    Get rounded(down) to nearest multiple of block size.
   * @param MaxRead   - Max read size in 32-bit words
   *                    Get rounded(down) to nearest multiple of block size.
   * @param MaxWrite  - Maximum write (into buffer) in 32-bit words
   *
   * @return NULL if everything is OK
   *    else A string describing problem
   */
  const char * setup(Uint32 * Buffer,
		     Uint32 Size, 
		     Uint32 BlockSize = 128,   // 512 bytes
		     Uint32 MinRead   = 1024,  // 4k
		     Uint32 MaxRead   = 1024,  // 4k
		     Uint32 MaxWrite  = 1024); // 4k
  /*  
   * @return NULL if everything is OK
   *    else A string describing problem
   */
  const char * valid() const;
  
  Uint32 getBufferSize() const;
  Uint32 getUsableSize() const; 
  Uint32 * getStart() const;
  
  /**
   * getReadPtr - Get pointer and size of data to send to FS
   *
   * @param ptr - Where to fetch data
   * @param sz  - How much data in 32-bit words
   * @param eof - Is this the last fetch (only if return false)
   *                                             
   * @return true  - If there is data of size >= minread
   *         false - If there is can be data be if it is is < minread
   *               - else eof = true
   */
  bool getReadPtr(Uint32 ** ptr, Uint32 * sz, bool * eof);
  
  /**
   * @note: sz must be equal to sz returned by getReadPtr
   */
  void updateReadPtr(Uint32 sz);

  /**
   * 
   * @note Must be followed by a updateWritePtr(no of words used)
   */
  bool getWritePtr(Uint32 ** ptr, Uint32 sz);
  
  void updateWritePtr(Uint32 sz);

  /**
   * There will be no more writing to this buffer
   */
  void eof();

  /**
   * Getters for varibles
   */
  Uint32 getMaxWrite() const { return m_maxWrite;}
  Uint32 getMinRead() const { return m_minRead;}
  
  Uint32 getFreeSize() const { return m_free; }

  /**
   * reset
   */
  void reset();

private:
  
  Uint32 m_free;
  Uint32 m_readIndex;
  Uint32 m_writeIndex;
  Uint32 m_eof;
  Uint32 * m_start; 
  Uint32 m_minRead;
  Uint32 m_maxRead;
  Uint32 m_maxWrite;
  Uint32 m_size;

  Uint32 * m_buffer;
  Uint32 m_bufSize;
  Uint32 m_blockSize;

  void clear();
};

inline
FsBuffer::FsBuffer() 
{
  clear();
}

inline
void
FsBuffer::clear(){
  m_minRead = m_maxRead = m_maxWrite = m_size = m_bufSize = m_free = 0;
  m_buffer = m_start = 0;
}

static 
Uint32 * 
align(Uint32 * ptr, Uint32 alignment, bool downwards){
  
  const UintPtr a = (UintPtr)ptr;
  const UintPtr b = a % alignment;
  
  if(downwards){
    return (Uint32 *)(a - b);
  } else {
    return (Uint32 *)(a + (b == 0 ? 0 : (alignment - b)));
  }
}

inline
const char * 
FsBuffer::setup(Uint32 * Buffer,
		Uint32 Size, 
		Uint32 Block,
		Uint32 MinRead,
		Uint32 MaxRead,
		Uint32 MaxWrite)
{
  clear();
  m_buffer    = Buffer;
  m_bufSize   = Size;
  m_blockSize = Block;
  if(Block == 0){
    return valid();
  }
  
  m_minRead  = (MinRead / Block) * Block;
  m_maxRead  = (MaxRead / Block) * Block;
  m_maxWrite = MaxWrite;

  m_start = align(Buffer, Block*4, false);
  Uint32 * stop = align(Buffer + Size - MaxWrite, Block*4, true);
  if(stop > m_start){
    m_size = stop - m_start;
  } else {
    m_size = 0;
  }
  
  if(m_minRead == 0)
    m_size = 0;
  else
    m_size = (m_size / m_minRead) * m_minRead;
  
#if 0
  ndbout_c("Block = %d MinRead = %d -> %d", Block*4, MinRead*4, m_minRead*4);
  ndbout_c("Block = %d MaxRead = %d -> %d", Block*4, MaxRead*4, m_maxRead*4);
  
  ndbout_c("Buffer = %d -> %d", Buffer, m_start);
  ndbout_c("Buffer = %d Size = %d MaxWrite = %d -> %d",
	   Buffer, Size*4, MaxWrite*4, m_size*4);
#endif

  m_readIndex = m_writeIndex = m_eof = 0;
  m_free = m_size;
  return valid();
}

inline
void
FsBuffer::reset() 
{
  m_readIndex = m_writeIndex = 0;
  m_free = m_size;
  m_eof = 0;
}

inline
const char *
FsBuffer::valid() const {
  if(m_buffer  == 0) return "Null pointer buffer";
  if(m_bufSize == 0) return "Zero size buffer";
  if(m_blockSize == 0) return "Zero block size";
  if(m_minRead < m_blockSize) return "Min read less than block size";
  if(m_maxRead < m_blockSize) return "Max read less than block size";
  if(m_maxRead < m_minRead) return "Max read less than min read";
  if(m_size == 0) return "Zero usable space";
  return 0;
}

inline
Uint32 
FsBuffer::getBufferSize() const {
  return m_bufSize;
}

inline
Uint32
FsBuffer::getUsableSize() const {
  return m_size;
}

inline
Uint32 *
FsBuffer::getStart() const {
  return m_start;
}

inline
bool 
FsBuffer::getReadPtr(Uint32 ** ptr, Uint32 * sz, bool * _eof){

  Uint32 * Tp = m_start;
  const Uint32 Tr = m_readIndex;
  const Uint32 Tm = m_minRead;
  const Uint32 Ts = m_size;
  const Uint32 Tmw = m_maxRead;

  Uint32 sz1 = m_size - m_free; // Used
  
  if(sz1 >= Tm){
    if(Tr + sz1 > Ts)
      sz1 = (Ts - Tr);
    
    if(sz1 > Tmw)
      * sz = Tmw;
    else
      * sz = sz1 - (sz1 % Tm);
    
    * ptr = &Tp[Tr];

    DEBUG(ndbout_c("getReadPtr() Tr: %d Tw: %d Ts: %d Tm: %d sz1: %d -> %d",
		   Tr, Tw, Ts, Tm, sz1, * sz));

    return true;
  }
  
  if(!m_eof){
    * _eof = false;
    
    DEBUG(ndbout_c("getReadPtr() Tr: %d Tw: %d Ts: %d Tm: %d sz1: %d -> false",
		   Tr, Tw, Ts, Tm, sz1));
    
    return false;
  }
  
  * sz = sz1;
  * _eof = true;
  * ptr = &Tp[Tr];

  DEBUG(ndbout_c("getReadPtr() Tr: %d Tw: %d Ts: %d Tm: %d sz1: %d -> %d eof",
		 Tr, Tw, Ts, Tm, sz1, * sz));
  
  return false;
}

inline
void
FsBuffer::updateReadPtr(Uint32 sz){
  const Uint32 Tr = m_readIndex;
  const Uint32 Ts = m_size;
  
  m_free += sz;
  m_readIndex = (Tr + sz) % Ts;
}

inline
bool
FsBuffer::getWritePtr(Uint32 ** ptr, Uint32 sz){
  assert(sz <= m_maxWrite);
  Uint32 * Tp = m_start;
  const Uint32 Tw = m_writeIndex;
  const Uint32 sz1 = m_free;

  if(sz1 > sz){ // Note at least 1 word of slack
    * ptr = &Tp[Tw];

    DEBUG(ndbout_c("getWritePtr(%d) Tr: %d Tw: %d Ts: %d sz1: %d -> true",
		   sz, Tr, Tw, Ts, sz1));
    return true;
  }

  DEBUG(ndbout_c("getWritePtr(%d) Tr: %d Tw: %d Ts: %d sz1: %d -> false",
		 sz, Tr, Tw, Ts, sz1));

  return false;
}

inline
void 
FsBuffer::updateWritePtr(Uint32 sz){
  assert(sz <= m_maxWrite);
  Uint32 * Tp = m_start;
  const Uint32 Tw = m_writeIndex;
  const Uint32 Ts = m_size;
  
  const Uint32 Tnew = (Tw + sz);
  m_free -= sz;
  if(Tnew < Ts){
    m_writeIndex = Tnew;
    return;
  }

  memcpy(Tp, &Tp[Ts], (Tnew - Ts) << 2);
  m_writeIndex = Tnew - Ts;
}

inline
void
FsBuffer::eof(){
  m_eof = 1;
}

#endif
