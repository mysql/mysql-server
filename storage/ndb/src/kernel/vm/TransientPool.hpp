/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TRANSPOOL_HPP
#define TRANSPOOL_HPP

#include "TransientSlotPool.hpp"
#include "debugger/EventLogger.hpp"
#include "util/require.h"
#include "vm/ComposedSlotPool.hpp"
#include "vm/Slot.hpp"

#define JAM_FILE_ID 506

#define SIZEOF_IN_WORDS(T) ((sizeof(T) + sizeof(Uint32) - 1) / sizeof(Uint32))

typedef ComposedSlotPool<StaticSlotPool, TransientSlotPool>
    TransientFastSlotPool;

template <typename T, Uint32 Slot_size = SIZEOF_IN_WORDS(T)>
class TransientPool : public TransientFastSlotPool {
 public:
  typedef T Type;
  void init(Uint32 type_id, const Pool_context &pool_ctx, Uint32 min_recs,
            Uint32 max_recs);
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  void resetMaxSize();
  void setMaxSize(Uint32 max_recs);
#endif
  bool startup();

  [[nodiscard]] bool seize(Ptr<T> &p);
  void release(Ptr<T> p);
  [[nodiscard]] T *getPtr(Uint32 i) const;
  void getPtr(Ptr<T> &p) const;
  /**
   * getValidPtr is often called on an operation record from a thread that
   * doesn't own the operation record. A few examples are:
   * 1) The current operation changing the record has its i-value written
   * into the row header. This write only happens in LDM threads since
   * Query threads are not allowed to change row data. In addition a linked
   * list of operation records are used on this operation on all other
   * changes or locked reads happening on this row.
   *
   * Even though the query thread cannot insert its own operation records
   * into this list, it must be able to read those operation records
   * efficiently to quickly find the correct version to use in the index
   * or to know which record to read in key lookup based on the transaction
   * id.
   *
   * see tuxReadAttrsOpt, tuxReadPk and before calling find_savepoint.
   *
   * 2) getValidPtr is also called on scan records in the TUX index. Each
   *    scan operation record in TUX insert its scan operation record into
   *    the index pages to ensure that it can resume its scan operation
   *    after returning from a real-time break. This writing happens both
   *    from Query threads and from LDM threads and is protected by a
   *    mutex on the index fragment record.
   *
   * 3) In DBACC we need to get the lock owners operation record when
   *    accessing a key in the hash table, this is only written by LDM thread,
   *    but needs to be readable from all query threads.
   *
   * All these accesses are in a sense insecure since the owner of the
   * pool object is allowed to continue inserting and removing objects from
   * pool concurrently with our call to getValidPtr.
   *
   * However the caller knows that the operation record that we want to get
   * a valid pointer is not released. This means that we rely on that the
   * translation of i-value is not changed as long as at least one operation
   * record remains on a page. This must hold also for any intermediate
   * pages used to find the page that houses the operation record.
   *
   * This principle must be upheld by the TransientPool. If this is no longer
   * true one must use real pointers between all operation records in those
   * lists. In addition the i-value stored in the row must be translated
   * by a special map index that maps from a 32-bit value to a pointer to
   * an operation record.
   */
  [[nodiscard]] bool getValidPtr(Ptr<T> &p) const;
  [[nodiscard]] bool getUncheckedPtrRO(Ptr<T> &p) const;
  [[nodiscard]] bool getUncheckedPtrRW(Ptr<T> &p) const;
  [[nodiscard]] Uint32 getUncheckedPtrs(Uint32 *from, Ptr<T> ptrs[],
                                        Uint32 cnt) const;

  Uint32 getEntrySize() const { return sizeof(Type); }
  Uint32 getNoOfFree() const { return SlotPool::getNoOfFree(); }
  Uint32 getSize() const { return SlotPool::getSize(); }
  Uint32 getUsed() const { return SlotPool::getUsed(); }
  Uint32 getUsedHi() const { return SlotPool::getUsedHi(); }
  void resetUsedHi() { SlotPool::resetUsedHi(); }

  static Uint64 getMemoryNeed(Uint32 entry_count);

 private:
  typedef TransientFastSlotPool SlotPool;
  // typedef TransientSlotPool SlotPool;
  typedef typename SlotPool::Type SlotType;

  static void static_asserts() {
    static_assert(offsetof(T, m_magic) == 0);
    static_assert(sizeof(T::m_magic) == 4);
  }
};

template <typename T, Uint32 Slot_size>
inline bool TransientPool<T, Slot_size>::seize(Ptr<T> &p) {
  Ptr<SlotType> slot;
  if (unlikely(!SlotPool::seize(slot, Slot_size))) {
    return false;
  }
  p.i = slot.i;
#if defined VM_TRACE || defined ERROR_INSERT
  memset(reinterpret_cast<void *>(slot.p), 0xF4, Slot_size * sizeof(Uint32));
#endif
  p.p = new (slot.p) T;
  if (unlikely(!Magic::match(p.p->m_magic, T::TYPE_ID))) {
    g_eventLogger->info(
        "Magic::match failed in %s: "
        "type_id %08x rg %u tid %u: "
        "slot_size %u: ptr.i %u: ptr.p %p: "
        "magic %08x expected %08x",
        __func__, T::TYPE_ID, GET_RG(T::TYPE_ID), GET_TID(T::TYPE_ID),
        Slot_size, p.i, p.p, p.p->m_magic, Magic::make(T::TYPE_ID));
    require(Magic::match(p.p->m_magic, T::TYPE_ID));
  }
  return true;
}

template <typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::release(Ptr<T> p) {
  Ptr<SlotType> slot;
  slot.i = p.i;
  p.p->~T();
#if defined VM_TRACE || defined ERROR_INSERT
  memset(reinterpret_cast<void *>(p.p), 0xF4, Slot_size * sizeof(Uint32));
#endif
  slot.p = new (p.p) SlotType;
  SlotPool::release(slot, Slot_size);
}

template <typename T, Uint32 Slot_size>
inline T *TransientPool<T, Slot_size>::getPtr(Uint32 i) const {
  Ptr<SlotType> p;
  p.i = i;
  require(SlotPool::getValidPtr(p, Magic::make(T::TYPE_ID), Slot_size));
  return reinterpret_cast<T *>(p.p);
}

template <typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::getPtr(Ptr<T> &p) const {
  p.p = getPtr(p.i);
}

template <typename T, Uint32 Slot_size>
inline bool TransientPool<T, Slot_size>::getValidPtr(Ptr<T> &p) const {
  Ptr<SlotType> slot;
  slot.i = p.i;
  if (unlikely(
          !SlotPool::getValidPtr(slot, Magic::make(T::TYPE_ID), Slot_size))) {
    return false;
  }
  p.p = reinterpret_cast<T *>(slot.p);
  return true;
}

template <typename T, Uint32 Slot_size>
inline bool TransientPool<T, Slot_size>::getUncheckedPtrRO(Ptr<T> &p) const {
  Ptr<SlotType> slot;
  slot.i = p.i;
  if (unlikely(!SlotPool::getUncheckedPtrRO(slot, Slot_size))) {
    return false;
  }
  p.p = reinterpret_cast<T *>(slot.p);
  return true;
}

template <typename T, Uint32 Slot_size>
inline bool TransientPool<T, Slot_size>::getUncheckedPtrRW(Ptr<T> &p) const {
  Ptr<SlotType> slot;
  slot.i = p.i;
  if (unlikely(!SlotPool::getUncheckedPtrRW(slot, Slot_size))) {
    return false;
  }
  p.p = reinterpret_cast<T *>(slot.p);
  return true;
}

template <typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::init(Uint32 type_id,
                                              const Pool_context &pool_ctx,
                                              Uint32 min_recs,
                                              Uint32 max_recs) {
  SlotPool::init(type_id, Slot_size, &min_recs, max_recs, pool_ctx);
}

template <typename T, Uint32 Slot_size>
inline bool TransientPool<T, Slot_size>::startup() {
  return SlotPool::startup(Slot_size);
}

template <typename T, Uint32 Slot_size>
inline Uint64 TransientPool<T, Slot_size>::getMemoryNeed(Uint32 entry_count) {
  return SlotPool::getMemoryNeed(Slot_size, entry_count);
}

template <typename T, Uint32 Slot_size>
inline Uint32 TransientPool<T, Slot_size>::getUncheckedPtrs(Uint32 *from,
                                                            Ptr<T> ptrs[],
                                                            Uint32 cnt) const {
  Ptr<SlotType> *slots = reinterpret_cast<Ptr<SlotType> *>(ptrs);
  return SlotPool::getUncheckedPtrs(from, slots, cnt, Slot_size);
}

#if defined(VM_TRACE) || defined(ERROR_INSERT)
template <typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::resetMaxSize() {
  SlotPool::resetMaxSize();
}

template <typename T, Uint32 Slot_size>
inline void TransientPool<T, Slot_size>::setMaxSize(Uint32 max_recs) {
  SlotPool::setMaxSize(max_recs);
}
#endif

#undef JAM_FILE_ID

#endif
