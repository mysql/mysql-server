/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef TRANSSLOTPOOL_HPP
#define TRANSSLOTPOOL_HPP

#include "portlib/ndb_prefetch.h"
#include "vm/Slot.hpp"
#include "vm/StaticSlotPool.hpp"
#include "vm/TransientPagePool.hpp"

#define JAM_FILE_ID 504

#define REARRANGE_ON_SEIZE 1

#define INLINE_TRANSIENT_SLOT_POOL 1

/**
 * TransientSlotPool
 *
 * TransientSlotPool may be used for records that have a maximum life time.
 *
 * This since pool can only shrink from top, and the maximum life time of
 * records roughly determines the time it takes for pool to shrink to half
 * when it becomes empty.
 *
 * Pool should *not* be used for record with no known upper limit of life time.
 *
 * Pages are initialized lazily, at most one slot at a time.  This to
 * ensure no seize takes extra long time due to initializing a whole page.
 *
 * Free list for slots in dynamic pages are two ended.  Then releasing records
 * with low id they are put at head (LIFO), while records with high id are put
 * at tail.  This to put some pressure for prefer reusing low id records and
 * free high id records.
 *
 * Pool can only be shrunk from top one page at a time.
 *
 * First slot is aligned to 8 words within page.
 */
class TransientSlotPool
{
public:
  TransientSlotPool();
  ~TransientSlotPool();

  bool may_shrink() const;
  bool rearrange_free_list_and_shrink(Uint32* max_shrinks, Uint32 slot_size);
  Uint32 getNoOfFree() const { return m_free_list.getCount(); }
  Uint32 getSize() const { return m_use_count + m_free_list.getCount(); }
  Uint32 getUsed() const { return m_use_count; }
  typedef Slot Type;

  void init(Uint32 type_id,
            Uint32 slot_size,
            Uint32* min_recs,
            const Pool_context &pool_ctx);

  bool seize(Ptr<Type> &p, Uint32 slot_size);
  void release(Ptr<Type> p, Uint32 slot_size);
  Type *getPtr(Uint32 i, Uint32 slot_size) const;
  void getPtr(Ptr<Type> &p, Uint32 slot_size) const;
  bool getValidPtr(Ptr<Type> &p, Uint32 magic, Uint32 slot_size) const;
  bool getUncheckedPtrRO(Ptr<Type> &p, Uint32 slot_size) const;
  bool getUncheckedPtrRW(Ptr<Type> &p, Uint32 slot_size) const;

  Uint32 getUncheckedPtrs(Uint32* from, Ptr<Type> ptrs[], Uint32 cnt, Uint32 slot_size) const;
  bool checkPtr(Type* p) const;

  static Uint64 getMemoryNeed(Uint32 slot_size, Uint32 entry_count);
private:
  friend class LocalSlotPool<TransientSlotPool>;
  class Page;
  typedef LocalDLCFifoList<LocalSlotPool<TransientSlotPool> > Slot_list;

  bool expand(Uint32 slot_size);
  bool shrink(Uint32 slot_size);

  Page* get_page_from_slot(Ptr<Type> p, Uint32 slot_size) const;

  TransientPagePool* m_page_pool;
  Slot_list::Head m_free_list;
  Uint32 m_type_id; // Needed for page type when allocating new pages (in seize/expand)
  Uint32 m_use_count; // Needed to release into right end of free list (approx).
  /**
   * m_may_shrink - set when top page is unused.
   * Cleared by seize() when first slot on top page is seized.
   * Cleared by shrink() if new top page have some used slots.
   * Set by release() when last used slot is released on top page.
   * Set by expand() if succeeded to add new (top) page.
   */
  bool m_may_shrink;
};

class TransientSlotPool::Page
{
  friend class TransientSlotPool;
private:
  static constexpr Uint32 WORDS_PER_PAGE = 8192;
  static constexpr Uint32 HEADER_WORDS = 8;
  static constexpr Uint32 DATA_WORDS_PER_PAGE = (WORDS_PER_PAGE - HEADER_WORDS);
  static constexpr Uint32 DATA_BYTE_OFFSET = HEADER_WORDS * sizeof(Uint32);
  Uint32 m_magic;
  Uint32 m_page_id;
  Uint32 m_use_count; // use count for dynamic page, to know when it is empty
  Uint32 m_first_in_free_array; // for lazy initialization of dynamic page
  Uint32 m_reserved[4];
  Uint32 m_data[DATA_WORDS_PER_PAGE];

  static void static_asserts()
  {
    static_assert(sizeof(Page) == WORDS_PER_PAGE * sizeof(Uint32));
  }
};

#if INLINE_TRANSIENT_SLOT_POOL
#include "TransientSlotPool.inline.hpp.inc"
#endif

#undef JAM_FILE_ID

#endif
