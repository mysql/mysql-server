/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TRANSPOOL_HPP
#define TRANSPOOL_HPP

#include "TransientSlotPool.hpp"
#include "vm/ComposedSlotPool.hpp"
#include "vm/Slot.hpp"

#define JAM_FILE_ID 506

#define SIZEOF_IN_WORDS(T) ((sizeof(T) + sizeof(Uint32) - 1) / sizeof(Uint32))

typedef ComposedSlotPool<StaticSlotPool,TransientSlotPool> TransientFastSlotPool;

template <typename T,
          Uint32 Slot_size = SIZEOF_IN_WORDS(T)>
class TransientPool
: public TransientFastSlotPool
{
public:
  typedef T Type;
  void init(Uint32 type_id, const Pool_context &pool_ctx, Uint32 min_recs, Uint32 max_recs);
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  void resetMaxSize();
  void setMaxSize(Uint32 max_recs);
#endif
  bool startup();

  bool seize(Ptr<T> &p);
  void release(Ptr<T> p);
  T *getPtr(Uint32 i) const;
  void getPtr(Ptr<T> &p) const;
  bool getValidPtr(Ptr<T> &p) const;
  bool getUncheckedPtrRO(Ptr<T> &p) const;
  bool getUncheckedPtrRW(Ptr<T> &p) const;
  Uint32 getUncheckedPtrs(Uint32* from, Ptr<T> ptrs[], Uint32 cnt) const;

  Uint32 getEntrySize() const { return sizeof(Type); }
  Uint32 getNoOfFree() const { return SlotPool::getNoOfFree(); }
  Uint32 getSize() const { return SlotPool::getSize(); }
  Uint32 getUsed() const { return SlotPool::getUsed(); }
  Uint32 getUsedHi() const { return SlotPool::getUsedHi(); }
  void resetUsedHi() { SlotPool::resetUsedHi(); }

  static Uint64 getMemoryNeed(Uint32 entry_count);
private:
  typedef TransientFastSlotPool SlotPool;
  //typedef TransientSlotPool SlotPool;
  typedef typename SlotPool::Type SlotType;

  static void static_asserts()
  {
    NDB_STATIC_ASSERT( offsetof(T, m_magic) == 0);
    NDB_STATIC_ASSERT( sizeof(T::m_magic) == 4);
  }
};

template<typename T, Uint32 Slot_size> inline bool TransientPool<T, Slot_size>::seize(Ptr<T> &p)
{
  Ptr<SlotType> slot;
  if (unlikely(!SlotPool::seize(slot, Slot_size)))
  {
    return false;
  }
  p.i = slot.i;
  p.p = new (slot.p) T;
  require(Magic::match(p.p->m_magic, T::TYPE_ID));
  return true;
}

template<typename T, Uint32 Slot_size> inline void TransientPool<T, Slot_size>::release(Ptr<T> p)
{
  Ptr<SlotType> slot;
  slot.i = p.i;
  p.p->~T();
#if defined VM_TRACE || defined ERROR_INSERT
  memset(reinterpret_cast<void*>(p.p), 0xF4, Slot_size * sizeof(Uint32));
#endif
  slot.p = new (p.p) SlotType;
  SlotPool::release(slot, Slot_size);
}

template<typename T, Uint32 Slot_size> inline T *TransientPool<T, Slot_size>::getPtr(Uint32 i) const
{
  Ptr<SlotType> p;
  p.i = i;
  require(SlotPool::getValidPtr(p, Magic::make(T::TYPE_ID), Slot_size));
  return reinterpret_cast<T*>(p.p);
}

template<typename T, Uint32 Slot_size> inline void TransientPool<T, Slot_size>::getPtr(Ptr<T> &p) const
{
  p.p = getPtr(p.i);
}

template<typename T, Uint32 Slot_size> inline bool TransientPool<T, Slot_size>::getValidPtr(Ptr<T> &p) const
{
  Ptr<SlotType> slot;
  slot.i = p.i;
  if (unlikely(!SlotPool::getValidPtr(slot, Magic::make(T::TYPE_ID), Slot_size)))
  {
    return false;
  }
  p.p = reinterpret_cast<T*>(slot.p);
  return true;
}

template<typename T, Uint32 Slot_size> inline bool TransientPool<T, Slot_size>::getUncheckedPtrRO(Ptr<T> &p) const
{
  Ptr<SlotType> slot;
  slot.i = p.i;
  if (unlikely(!SlotPool::getUncheckedPtrRO(slot, Slot_size)))
  {
    return false;
  }
  p.p = reinterpret_cast<T*>(slot.p);
  return true;
}

template<typename T, Uint32 Slot_size> inline bool TransientPool<T, Slot_size>::getUncheckedPtrRW(Ptr<T> &p) const
{
  Ptr<SlotType> slot;
  slot.i = p.i;
  if (unlikely(!SlotPool::getUncheckedPtrRW(slot, Slot_size)))
  {
    return false;
  }
  p.p = reinterpret_cast<T*>(slot.p);
  return true;
}

template<typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::init(Uint32 type_id,
                                              const Pool_context& pool_ctx,
                                              Uint32 min_recs,
                                              Uint32 max_recs)
{
  SlotPool::init(type_id, Slot_size, &min_recs, max_recs, pool_ctx);
}

template<typename T, Uint32 Slot_size>
inline bool TransientPool<T, Slot_size>::startup()
{
  return SlotPool::startup(Slot_size);
}

template<typename T, Uint32 Slot_size>
inline Uint64 TransientPool<T, Slot_size>::getMemoryNeed(Uint32 entry_count)
{
  return SlotPool::getMemoryNeed(Slot_size, entry_count);
}

template<typename T, Uint32 Slot_size>
inline Uint32 TransientPool<T, Slot_size>::getUncheckedPtrs(Uint32* from, Ptr<T> ptrs[], Uint32 cnt) const
{
  Ptr<SlotType>* slots = reinterpret_cast<Ptr<SlotType>*>(ptrs);
  return SlotPool::getUncheckedPtrs(from, slots, cnt, Slot_size);
}

#if defined(VM_TRACE) || defined(ERROR_INSERT)
template<typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::resetMaxSize()
{
  SlotPool::resetMaxSize();
}

template<typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::setMaxSize(Uint32 max_recs)
{
  SlotPool::setMaxSize(max_recs);
}
#endif

#undef JAM_FILE_ID

#endif
