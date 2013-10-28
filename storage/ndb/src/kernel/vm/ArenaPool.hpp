/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef ARENA_POOL_HPP
#define ARENA_POOL_HPP

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

  STATIC_CONST( HeaderSize = 2 );

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

class ArenaPool; // forward

class ArenaAllocator
{
  RWPool m_pool;
  Uint32 m_block_size;
  friend class ArenaPool;
public:
  ArenaAllocator() {}
  void init(Uint32 blockSize, Uint32 type_id, const Pool_context& pc);

  bool seize(ArenaHead&);
  void release(ArenaHead&);
};

class ArenaPool
{
public:
  ArenaPool() {}

  void init(ArenaAllocator*, const Record_info& ri, const Pool_context& pc);

  bool seize(Ptr<void>&) { assert(false); return false; } // Not implemented...

  bool seize(ArenaHead&, Ptr<void>&);
  void release(Ptr<void>);
  void * getPtr(Uint32 i);

private:
  void handle_invalid_release(Ptr<void>) ATTRIBUTE_NORETURN;

  Record_info m_record_info;
  ArenaAllocator * m_allocator;
};

class LocalArenaPoolImpl
{
  ArenaHead & m_head;
  ArenaPool & m_pool;
public:
  LocalArenaPoolImpl(ArenaHead& head, ArenaPool & pool)
    : m_head(head), m_pool(pool) {}

  bool seize(Ptr<void> & ptr) { return m_pool.seize(m_head, ptr); }
  void release(Ptr<void> ptr) { m_pool.release(ptr); }
  void * getPtr(Uint32 i) { return m_pool.getPtr(i); }
};

inline
void*
ArenaPool::getPtr(Uint32 i)
{
  return m_allocator->m_pool.getPtr(m_record_info, i);
}

inline
void
ArenaPool::release(Ptr<void> ptr)
{
  Uint32 * record_ptr = static_cast<Uint32*>(ptr.p);
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


#undef JAM_FILE_ID

#endif
