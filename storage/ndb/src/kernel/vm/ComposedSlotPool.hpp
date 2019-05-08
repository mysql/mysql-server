/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPOSEDSLOTPOOL_HPP
#define COMPOSEDSLOTPOOL_HPP

#include "blocks/record_types.hpp"
#include "ndb_global.h"
#include "ndb_limits.h"
#include "ndb_types.h"
#include "vm/Pool.hpp"

#define JAM_FILE_ID 510

#define COMPOSED_SLOT_POOL_NO_MAX_COUNT

template <typename Pool1, typename Pool2>
class ComposedSlotPool
{
 public:
  typedef Slot Type;
  ComposedSlotPool();
  Uint32 getUncheckedPtrs(Uint32* from,
                          Ptr<Slot> ptrs[],
                          Uint32 cnt,
                          Uint32 slot_size) const;
  Uint32 getEntrySize() const;
  Uint32 getNoOfFree() const;
  Uint32 getSize() const;
  Uint32 getUsed() const;
  Uint32 getUsedHi() const;
  void resetUsedHi();

  static Uint64 getMemoryNeed(Uint32 slot_size, Uint32 entry_count);
  bool seize(Ptr<Slot>& p, Uint32 slot_size);
  bool seize_pool2(Ptr<Slot>& p, Uint32 slot_size);
  void release(Ptr<Slot> p, Uint32 slot_size);
  void init(Uint32 type_id,
            unsigned int slot_size,
            Uint32* min_recs,
            Uint32 max_recs,
            const Pool_context& pool_ctx);
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  void resetMaxSize();
  void setMaxSize(Uint32 max_recs);
#endif
  bool startup(Uint32 slot_size);
  bool getValidPtr(Ptr<Slot>& p, Uint32 magic, Uint32 slot_size) const;
  bool getValidPtr_pool2(Ptr<Slot>& p, Uint32 magic, Uint32 slot_size) const;
  bool getUncheckedPtrRO(Ptr<Slot>& p, Uint32 slot_size) const;
  bool getUncheckedPtrRO_pool2(Ptr<Slot>& p, Uint32 slot_size) const;
  bool getUncheckedPtrRW(Ptr<Slot>& p, Uint32 slot_size) const;
  bool getUncheckedPtrRW_pool2(Ptr<Slot>& p, Uint32 slot_size) const;
  bool may_shrink() const;
  bool rearrange_free_list_and_shrink(Uint32 max_shrinks);

 private:
  bool inPool1(Uint32 i) const;
  Uint32 toPool2(Uint32 i) const;
  Uint32 fromPool2(Uint32 i) const;

  Pool1 m_pool1;
#if !defined(COMPOSED_SLOT_POOL_NO_MAX_COUNT) || defined(VM_TRACE) || defined(ERROR_INSERT)
  Uint32 m_max_count;
#endif
  Uint32 m_use_count;
  Uint32 m_used_high;
  Uint32 m_shrink_level;
  Pool2 m_pool2;
  Uint32 m_slot_size;
  Uint32 m_pool1_startup_count;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  Uint32 m_orig_max_count;
#endif
};

template <typename Pool1, typename Pool2>
inline ComposedSlotPool<Pool1, Pool2>::ComposedSlotPool()
    :
#if !defined(COMPOSED_SLOT_POOL_NO_MAX_COUNT)
      m_max_count(0),
#elif defined(VM_TRACE) || defined(ERROR_INSERT)
      m_max_count(RNIL),
#endif
      m_use_count(0),
      m_used_high(0),
      m_shrink_level(0),
      m_slot_size(0),
      m_pool1_startup_count(0)
#if defined(VM_TRACE) || defined(ERROR_INSERT)
      ,
      m_orig_max_count(m_max_count)
#endif
{
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::getUncheckedPtrs(
    Uint32* from, Ptr<Slot> ptrs[], Uint32 cnt, Uint32 slot_size) const
{
  const Uint32 i = *from;
  if (likely(inPool1(i)) || (i == RNIL && inPool1(0)))
  {
    Uint32 ptr_cnts = m_pool1.getUncheckedPtrs(from, ptrs, cnt, slot_size);
    if (unlikely(*from == RNIL) && m_pool2.getSize() > 0)
    {
      *from = m_pool1.getSize();
    }
    return ptr_cnts;
  }
  Uint32 from2 = toPool2(i);
  Uint32 ptr_cnts = m_pool2.getUncheckedPtrs(&from2, ptrs, cnt, slot_size);
  for (Uint32 i = 0; i < ptr_cnts; i++)
  {
    ptrs[i].i = fromPool2(ptrs[i].i);
  }
  *from = fromPool2(from2);
  return ptr_cnts;
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::getEntrySize() const
{
  return m_slot_size;
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::getNoOfFree() const
{
  return getSize() - getUsed();
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::getSize() const
{
  return m_pool1.getSize() + m_pool2.getSize();
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::getUsed() const
{
  return m_use_count;
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::getUsedHi() const
{
  return m_used_high;
}

template <typename Pool1, typename Pool2>
inline void ComposedSlotPool<Pool1, Pool2>::resetUsedHi()
{
  m_used_high = m_use_count;
}

template <typename Pool1, typename Pool2>
inline Uint64 ComposedSlotPool<Pool1, Pool2>::getMemoryNeed(
    Uint32 slot_size, Uint32 entry_count)
{
  return Pool1::getMemoryNeed(slot_size, entry_count) +
         Pool2::getMemoryNeed(slot_size, entry_count);
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::seize(Ptr<Slot>& p,
                                                  Uint32 slot_size)
{
#if !defined(COMPOSED_SLOT_POOL_NO_MAX_COUNT) || defined(VM_TRACE) || defined(ERROR_INSERT)
  const Uint32 max_count = m_max_count;
#endif
  const Uint32 use_count = m_use_count;
  const Uint32 new_use_count = use_count + 1;
  const Uint32 used_high = m_used_high;
#if !defined(COMPOSED_SLOT_POOL_NO_MAX_COUNT)
  if (unlikely(use_count == max_count))
  {
    return false;
  }
#elif defined(VM_TRACE) || defined(ERROR_INSERT)
  if (unlikely(use_count >= max_count))
  {
    return false;
  }
#endif
  if (unlikely(!m_pool1.seize(p, slot_size)))
  {
    if (!seize_pool2(p, slot_size))
    {
      return false;
    }
  }
  m_use_count = new_use_count;
  if (used_high < new_use_count)
  {
    m_used_high = new_use_count;
  }
  return true;
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::seize_pool2(Ptr<Slot>& p,
                                                  Uint32 slot_size)
{
    if (!m_pool2.seize(p, slot_size))
    {
      return false;
    }
    p.i = fromPool2(p.i);
  return true;
}

template <typename Pool1, typename Pool2>
inline void ComposedSlotPool<Pool1, Pool2>::release(Ptr<Slot> p,
                                                    Uint32 slot_size)
{
  const Uint32 use_count = m_use_count;
  const Uint32 new_use_count = use_count - 1;
  if (likely(inPool1(p.i)))
  {
    m_pool1.release(p, slot_size);
  }
  else
  {
    p.i = toPool2(p.i);
    m_pool2.release(p, slot_size);
  }
  m_use_count = new_use_count;
}

template <typename Pool1, typename Pool2>
inline void ComposedSlotPool<Pool1, Pool2>::init(Uint32 type_id,
                                                 unsigned int slot_size,
                                                 Uint32* min_recs,
                                                 Uint32 max_recs,
                                                 const Pool_context& pool_ctx)
{
#if !defined(COMPOSED_SLOT_POOL_NO_MAX_COUNT)
  m_max_count = max_recs;
#else
  (void) max_recs;
#endif
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  m_orig_max_count = m_max_count;
#endif
  const Uint32 req_recs = *min_recs;
  Uint32 pool1_recs = req_recs;
  m_pool1.init(type_id, slot_size, &pool1_recs, pool_ctx);
  Uint32 pool2_recs = req_recs - pool1_recs;
  m_pool2.init(type_id, slot_size, &pool2_recs, pool_ctx);
  *min_recs = pool1_recs + pool2_recs;

  m_slot_size = slot_size;
  m_shrink_level = (16384 / slot_size); /* slots on a half page */
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::startup(Uint32 slot_size)
{
  return m_pool1.startup(&m_pool1_startup_count, slot_size);
}

#if defined(VM_TRACE) || defined(ERROR_INSERT)
template <typename Pool1, typename Pool2>
inline void ComposedSlotPool<Pool1, Pool2>::resetMaxSize()
{
  m_max_count = m_orig_max_count;
}

template <typename Pool1, typename Pool2>
inline void ComposedSlotPool<Pool1, Pool2>::setMaxSize(Uint32 max_recs)
{
  m_max_count = max_recs;
}
#endif

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::getValidPtr(
    Ptr<Slot>& p, Uint32 magic, Uint32 slot_size) const
{
  if (likely(m_pool1.getValidPtr(p, magic, slot_size)))
  {
    return true;
  }
  if (likely(inPool1(p.i)))
  {
    return false;
  }
  return getValidPtr_pool2(p,magic, slot_size);
}

template<typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::getValidPtr_pool2(
    Ptr<Slot>& p, Uint32 magic, Uint32 slot_size) const
{
  p.i = toPool2(p.i);
  bool ok = m_pool2.getValidPtr(p, magic, slot_size);
  p.i = fromPool2(p.i);
  return ok;
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::getUncheckedPtrRO(
    Ptr<Slot>& p, Uint32 slot_size) const
{
  if (likely(m_pool1.getUncheckedPtrRO(p, slot_size)))
  {
    return true;
  }
  if (likely(inPool1(p.i)))
  {
    return false;
  }
  return getUncheckedPtrRO_pool2(p, slot_size);
}

template<typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::getUncheckedPtrRO_pool2(
    Ptr<Slot>& p, Uint32 slot_size) const
{
  p.i = toPool2(p.i);
  bool ok = m_pool2.getUncheckedPtrRO(p, slot_size);
  p.i = fromPool2(p.i);
  return ok;
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::getUncheckedPtrRW(
    Ptr<Slot>& p, Uint32 slot_size) const
{
  if (likely(m_pool1.getUncheckedPtrRW(p, slot_size)))
  {
    return true;
  }
  if (likely(inPool1(p.i)))
  {
    return false;
  }
  return getUncheckedPtrRW_pool2(p, slot_size);
}

template<typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::getUncheckedPtrRW_pool2(
    Ptr<Slot>& p, Uint32 slot_size) const
{
  p.i = toPool2(p.i);
  bool ok = m_pool2.getUncheckedPtrRW(p, slot_size);
  p.i = fromPool2(p.i);
  return ok;
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::may_shrink() const
{
  return unlikely(m_pool1.may_shrink() || m_pool2.may_shrink()) &&
         (getNoOfFree() > m_shrink_level);
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::rearrange_free_list_and_shrink(
    Uint32 max_shrinks)
{
  bool more = false;
  Uint32 pool1_shrinks = max_shrinks;
  more |= m_pool1.rearrange_free_list_and_shrink(&pool1_shrinks, m_slot_size);
  Uint32 pool2_shrinks = max_shrinks - pool1_shrinks;
  more |= m_pool2.rearrange_free_list_and_shrink(&pool2_shrinks, m_slot_size);
  return more;
}

template <typename Pool1, typename Pool2>
inline bool ComposedSlotPool<Pool1, Pool2>::inPool1(Uint32 i) const
{
  return i < m_pool1.getSize();
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::toPool2(Uint32 i) const
{
  if (unlikely(i == RNIL)) return RNIL;
  require(i >= m_pool1.getSize());
  return i - m_pool1.getSize();
}

template <typename Pool1, typename Pool2>
inline Uint32 ComposedSlotPool<Pool1, Pool2>::fromPool2(Uint32 i) const
{
  if (unlikely(i == RNIL)) return RNIL;
  assert(i < RNIL - m_pool1.getSize());
  return (i + m_pool1.getSize());
}

#undef JAM_FILE_ID

#endif
