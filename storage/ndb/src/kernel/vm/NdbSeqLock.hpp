/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_SEQLOCK_HPP
#define NDB_SEQLOCK_HPP

#include <atomic>
#include <cstdint>

#include "ndb_types.h"
#include "portlib/mt-asm.h"

#define JAM_FILE_ID 251

#if defined(NDB_HAVE_RMB) && defined(NDB_HAVE_WMB)
struct NdbSeqLock {
  std::atomic<uint32_t> m_seq = 0;
  static_assert(decltype(m_seq)::is_always_lock_free);

  void write_lock();
  void write_unlock();

  Uint32 read_lock();
  bool read_unlock(Uint32 val) const;
};

inline void NdbSeqLock::write_lock() {
  // atomic read not needed but since m_seq is atomic do it relaxed
  Uint32 val = m_seq.load(std::memory_order_relaxed);
  assert((val & 1) == 0);
  val++;
  m_seq.store(val, std::memory_order_relaxed);
  wmb();
}

inline void NdbSeqLock::write_unlock() {
  // atomic read not needed but since m_seq is atomic do it relaxed
  Uint32 val = m_seq.load(std::memory_order_relaxed);
  assert((val & 1) == 1);
  wmb();
  val++;
  m_seq.store(val, std::memory_order_relaxed);
}

inline Uint32 NdbSeqLock::read_lock() {
loop:
  Uint32 val = m_seq.load(std::memory_order_relaxed);
  rmb();
  if (unlikely(val & 1)) {
#ifdef NDB_HAVE_CPU_PAUSE
    cpu_pause();
#endif
    goto loop;
  }
  return val;
}

inline bool NdbSeqLock::read_unlock(Uint32 val) const {
  rmb();
  return val == m_seq.load(std::memory_order_relaxed);
}
#else /** ! rmb() or wmb() */
/**
 * Only for ndbd...
 */

struct NdbSeqLock {
  NdbSeqLock() {}

  void write_lock() {}
  void write_unlock() {}

  Uint32 read_lock() { return 0; }
  bool read_unlock(Uint32 val) const { return true; }
};

#endif

#undef JAM_FILE_ID

#endif
