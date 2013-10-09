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

#include "ArenaPool.hpp"
#include <ndbd_exit_codes.h>
#include <NdbOut.hpp>

static
Uint32
computeBlockSize(Uint32 blockSz, Uint32 wpp)
{
  Uint32 minspill = wpp % blockSz;
  Uint32 minspill_bs = blockSz;

  for (Uint32 i = 16; i<blockSz/4; i += 16)
  {
    Uint32 spillsz = wpp % (blockSz - i);
    if (spillsz == 0)
    {
      return blockSz - i;
    }
    else if (spillsz < minspill)
    {
      minspill = spillsz;
      minspill_bs = blockSz -i;
    }
  }
  ndbout_c("blockSz: %u, wpp: %u -> %u (%u)",
           blockSz, wpp, minspill_bs, minspill);
  return minspill_bs;
}

void
ArenaAllocator::init(Uint32 sz, Uint32 type_id, const Pool_context& pc)
{
  Uint32 blocksz = ArenaBlock::computeBlockSizeInWords(sz);
  Uint32 wpp = m_pool.WORDS_PER_PAGE;

  Uint32 bs = computeBlockSize(blocksz, wpp);
  Record_info ri;
  ri.m_size = 4 * bs;
  {
    ArenaBlock tmp;
    const char * off_base = (char*)&tmp;
    const char * off_next = (char*)&tmp.nextPool;
    const char * off_magic = (char*)&tmp.m_magic;

    ri.m_offset_next_pool = Uint32(off_next - off_base);
    ri.m_offset_magic = Uint32(off_magic - off_base);
  }
  ri.m_type_id = type_id;
  m_pool.init(ri, pc);
  m_block_size = bs - ArenaBlock::HeaderSize;
}

bool
ArenaAllocator::seize(ArenaHead& ah)
{
  Ptr<void> tmp;
  if (m_pool.seize(tmp))
  {
    ah.m_first_block = tmp.i;
    ah.m_current_block = tmp.i;
    ah.m_block_size = m_block_size;
    ah.m_current_block_ptr = static_cast<ArenaBlock*>(tmp.p);
    ah.m_current_block_ptr->m_next_block = RNIL;
    return true;
  }
  return false;
}

void
ArenaAllocator::release(ArenaHead& ah)
{
  Ptr<void> curr;
  curr.i = ah.m_first_block;
  while (curr.i != RNIL)
  {
    curr.p = m_pool.getPtr(curr.i);
    Uint32 next = static_cast<ArenaBlock*>(curr.p)->m_next_block;
    m_pool.release(curr);
    curr.i = next;
  }

  new (&ah) ArenaHead();
}

void
ArenaPool::init(ArenaAllocator * alloc,
                const Record_info& ri, const Pool_context&)
{
  m_record_info = ri;
#if SIZEOF_CHARP == 4
  m_record_info.m_size = ((ri.m_size + 3) >> 2); // Align to word boundary
#else
  m_record_info.m_size = ((ri.m_size + 7) >> 3) << 1; // align 8-byte
#endif
  m_record_info.m_offset_magic = ((ri.m_offset_magic + 3) >> 2);
  m_record_info.m_offset_next_pool = ((ri.m_offset_next_pool + 3) >> 2);
  m_allocator = alloc;
}

bool
ArenaPool::seize(ArenaHead & ah, Ptr<void>& ptr)
{
  Uint32 pos = ah.m_first_free;
  Uint32 bs = ah.m_block_size;
  Uint32 ptrI = ah.m_current_block;
  ArenaBlock * block = ah.m_current_block_ptr;

  Uint32 sz = m_record_info.m_size;
  Uint32 off = m_record_info.m_offset_magic;

  if (0)
    ndbout_c("pos: %u sz: %u (sum: %u) bs: %u",
             pos, sz, (pos + sz), bs);

  if (pos + sz <= bs)
  {
    /**
     * Alloc in this block
     */
    ptr.i =
      ((ptrI >> POOL_RECORD_BITS) << POOL_RECORD_BITS) +
      (ptrI & POOL_RECORD_MASK) + pos + ArenaBlock::HeaderSize;
    ptr.p = block->m_data + pos;
    block->m_data[pos+off] = ~(Uint32)m_record_info.m_type_id;

    ah.m_first_free = pos + sz;
    return true;
  }
  else
  {
    Ptr<void> tmp;
    if (m_allocator->m_pool.seize(tmp))
    {
      ah.m_first_free = 0;
      ah.m_current_block = tmp.i;
      ah.m_current_block_ptr->m_next_block = tmp.i;
      ah.m_current_block_ptr = static_cast<ArenaBlock*>(tmp.p);
      ah.m_current_block_ptr->m_next_block = RNIL;
      bool ret = seize(ah, ptr);
      (void)ret;
      assert(ret == true);
      return true;
    }
  }
  return false;
}

void
ArenaPool::handle_invalid_release(Ptr<void> ptr)
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
