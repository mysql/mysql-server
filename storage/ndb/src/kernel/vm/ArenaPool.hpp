/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#ifndef ARENA_POOL_HPP
#define ARENA_POOL_HPP

#include "my_config.h"
#include "util/require.h"
#include <ndbd_exit_codes.h>
#include <NdbOut.hpp> // For template ArenaPool former cpp-file
#include "Pool.hpp"
#include "RWPool.hpp"

#define JAM_FILE_ID 289


struct ArenaBlock
{
  Uint32 m_magic;
  union {
    Uint32 m_next_block;
    Uint32 nextPool;
  };

  Uint32 m_data[1];

  static constexpr Uint32 HeaderSize = 2;

  static Uint32 computeBlockSizeInWords(Uint32 datasz) {
    return 16 * (((datasz + 2) + 8) / 16);
  }
};

struct ArenaHead
{
  ArenaHead() {
    m_first_free = ~(Uint16)0;
    m_block_size = 0;
    m_first_block = RNIL;
    m_current_block = RNIL;
    m_current_block_ptr = 0;
  }

  ArenaBlock * m_current_block_ptr;
  Uint32 m_first_block;
  Uint32 m_current_block;
  Uint16 m_first_free;
  Uint16 m_block_size;
};

template<typename T>
class ArenaPool; // forward

class ArenaAllocator
{
  RWPool<void> m_pool;
  Uint32 m_block_size;
  template<typename T> friend class ArenaPool;
public:
  ArenaAllocator() {}
  void init(Uint32 blockSize, Uint32 type_id, const Pool_context& pc);

  bool seize(ArenaHead&);
  void release(ArenaHead&);
};

template<typename T>
class ArenaPool
{
public:
  typedef T Type;

  ArenaPool() {}

  void init(ArenaAllocator*, const Record_info& ri, const Pool_context& pc);

  bool seize(Ptr<T>&) { assert(false); return false; } // Not implemented...

  bool seize(ArenaHead&, Ptr<T>&);
  void release(Ptr<T>);
  T * getPtr(Uint32 i) const;

private:
  [[noreturn]] void handle_invalid_release(Ptr<T>);

  Record_info m_record_info;
  ArenaAllocator * m_allocator;
};

template<typename T>
class LocalArenaPool
{
  ArenaHead & m_head;
  ArenaPool<T> & m_pool;
public:
  LocalArenaPool(ArenaHead& head, ArenaPool<T> & pool)
    : m_head(head), m_pool(pool) {}

  bool seize(Ptr<T> & ptr) { return m_pool.seize(m_head, ptr); }
  void release(Ptr<T> ptr) { m_pool.release(ptr); }
  T * getPtr(Uint32 i) const { return m_pool.getPtr(i); }
};

template<typename T>
inline
T*
ArenaPool<T>::getPtr(Uint32 i) const
{
  void* const p = m_allocator->m_pool.getPtr(m_record_info, i);
  return static_cast<T*>(p);
}

template<typename T>
inline
void
ArenaPool<T>::release(Ptr<T> ptr)
{
  // TODO add trait extracting magic for type T
  Uint32 * record_ptr = reinterpret_cast<Uint32*>(ptr.p);
  Uint32 off = m_record_info.m_offset_magic;
  Uint32 type_id = m_record_info.m_type_id;
  Uint32 magic_val = * (record_ptr + off);

  if (likely(magic_val == ~type_id))
  {
    * (record_ptr + off) = 0;
    return;
  }
  handle_invalid_release(ptr);
}

////////////////////////////////

template<typename T>
void
ArenaPool<T>::init(ArenaAllocator * alloc,
                   const Record_info& ri, const Pool_context&)
{
  m_record_info = ri;
require(ri.m_size == sizeof(T));
#if SIZEOF_CHARP == 4
  m_record_info.m_size = ((ri.m_size + 3) >> 2); // Align to word boundary
#else
  m_record_info.m_size = ((ri.m_size + 7) >> 3) << 1; // align 8-byte
#endif
  m_record_info.m_offset_magic = ((ri.m_offset_magic + 3) >> 2);
  m_record_info.m_offset_next_pool = ((ri.m_offset_next_pool + 3) >> 2);
  m_allocator = alloc;
}

template<typename T>
bool
ArenaPool<T>::seize(ArenaHead & ah, Ptr<T>& ptr)
{
  Uint32 pos = ah.m_first_free;
  Uint32 bs = ah.m_block_size;
  Uint32 ptrI = ah.m_current_block;
  ArenaBlock * block = ah.m_current_block_ptr;

  Uint32 sz = m_record_info.m_size;
require(sizeof(T) <= sz*sizeof(Uint32));
  Uint32 off = m_record_info.m_offset_magic;

  if (0)
    g_eventLogger->info("pos: %u sz: %u (sum: %u) bs: %u", pos, sz, (pos + sz),
                        bs);

  if (pos + sz <= bs)
  {
    /**
     * Alloc in this block
     */
    ptr.i =
      ((ptrI >> POOL_RECORD_BITS) << POOL_RECORD_BITS) +
      (ptrI & POOL_RECORD_MASK) + pos + ArenaBlock::HeaderSize;
    Uint32* const p = block->m_data + pos;
    ptr.p = reinterpret_cast<T*>(p); // TODO dynamic_cast?
    block->m_data[pos+off] = ~(Uint32)m_record_info.m_type_id;

    ah.m_first_free = pos + sz;
    return true;
  }
  else
  {
    Ptr<void> tmp;
    if (ah.m_first_block == RNIL)
    { // ArenaPool is empty, seize a new ArenaHead
      if (!m_allocator->seize(ah))
        return false;
    }
    // Extend pool with new block
    else if (m_allocator->m_pool.seize(tmp))
    {
      assert(ah.m_block_size == m_allocator->m_block_size);
      ah.m_first_free = 0;
      ah.m_current_block = tmp.i;
      ah.m_current_block_ptr->m_next_block = tmp.i;
      ah.m_current_block_ptr = static_cast<ArenaBlock*>(tmp.p);
      ah.m_current_block_ptr->m_next_block = RNIL;
    }
    else
      return false;

    // Re-seize object from created / extended Pool
    const bool ret = seize(ah, ptr);
    (void)ret;
    assert(ret == true);
    return true;
  }
  return false;
}

template<typename T>
void
ArenaPool<T>::handle_invalid_release(Ptr<T> ptr)
{
  char buf[255];

  //Uint32 pos = ptr.i & POOL_RECORD_MASK;
  //Uint32 pageI = ptr.i >> POOL_RECORD_BITS;
  Uint32 * record_ptr_p = (Uint32*)ptr.p;

  Uint32 magic = * (record_ptr_p + m_record_info.m_offset_magic);
  BaseString::snprintf(buf, sizeof(buf),
                       "Invalid memory release: ptr (%x %p) magic: (%.8x %.8x)",
                       ptr.i, ptr.p, magic, m_record_info.m_type_id);

  m_allocator->m_pool.m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}
////////////////////////////////

#undef JAM_FILE_ID

#endif
