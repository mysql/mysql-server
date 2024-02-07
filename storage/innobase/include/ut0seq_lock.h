/*****************************************************************************

Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0seq_lock.h
 Implements a sequential lock structure for non-locking atomic read/write
 operations on a complex structure.

 *******************************************************************/

#ifndef ut0seq_lock_h
#define ut0seq_lock_h

#include <atomic>
#include <thread>

#include "ut0class_life_cycle.h"

namespace ut {
/** A class that allows to read value of variable of some type T atomically and
allows the value to be changed, all using lock-free operations. The type T has
to be composed of std::atomic fields only. That is because read(op_r) might read
it in parallel to write(op_w). Other than that you are allowed to use any type.
Inspired by https://www.hpl.hp.com/techreports/2012/HPL-2012-68.pdf Figure 6. */
template <typename T>
class Seq_lock : private Non_copyable {
 public:
  Seq_lock() = default;

  template <typename... Args>
  Seq_lock(Args... args) : m_value{std::forward<Args>(args)...} {}
  /* This class can be made copyable, but this requires additional constructors.
   */

  /** Writes a new value for the variable of type T. The op can use
  memory_order_relaxed stores.
  NOTE: The user needs to synchronize all calls to this method. */
  template <typename Op>
  void write(Op &&op) {
    const auto old = m_seq.load(std::memory_order_relaxed);
    /* The odd value means someone else is executing the write operation
    concurrently, and this is not allowed. */
    ut_ad((old & 1) == 0);
    m_seq.store(old + 1, std::memory_order_relaxed);
    /* This fence is meant to synchronize with the fence in read(), whenever
    op() in read() happens to load-from any of the values stored by our op().
    Then it would follow that load to seq_after will happen-after our
    first fetch_add(1), which is all we need. See 32.9.1 of C++17 draft. */
    std::atomic_thread_fence(std::memory_order_release);
    op(m_value);
    m_seq.store(old + 2, std::memory_order_release);
  }
  /* Reads the value of the stored value of type T using operation op(). The
  op() can use memory_order_relaxed loads. The op() can't assume the data stored
  inside T is logically consistent. Calls to this method don't need to be
  synchronized. */
  template <typename Op>
  auto read(Op &&op) const {
    int try_count = 0;
    while (true) {
      const auto seq_before = m_seq.load(std::memory_order_acquire);
      if ((seq_before & 1) == 1) {
        /* Someone is currently writing to the stored value. Try a few times to
        read the seq value, if this not help, try to yield execution. */
        if ((++try_count & 7) == 0) {
          std::this_thread::yield();
        }
        continue;
      }
      auto res = op(m_value);
      std::atomic_thread_fence(std::memory_order_acquire);
      const auto seq_after = m_seq.load(std::memory_order_relaxed);
      if (seq_before == seq_after) {
        return res;
      }
      /* The begin and end seq number is different, so the value read from T may
      be invalid. Let's just try again to perform the read. We do not want to
      yield here, as the new seq value is already set. If it is an odd value, we
      will detect it in the next loop and possibly yield. If it is even, we will
      just try to read new value. */
    }
    /* If we got here, then op() has seen a single coherent picture of value.
    We have seq_before == seq_after and it is even.
    Suppose that one of the loads inside op() saw value written by some writer w
    and thus our fence synchronizes-with w's fence, and thus seq_after
    assignment happens-after w's first increment of seq.
    Since seq_after is even, it must mean it comes from some later store in
    seq's modification order, not earlier than w's final increment of seq.
    And as seq_before is equal to it, it comes from the same later store.
    But, seq_before was assigned using acquire load, thus it
    synchronizes-with the store from which it loads, meaning that op() has
    to happen-after w's final increment of seq (one might need to follow
    the mutex sequence if there were other writers in between).
    Thus we see that if op() sees any part of the value written by w, then
    it all the other parts of value it sees are at least as fresh.
    Since writers are sequenced, it is meaningful to focus on "the latest"
    writer whose change our op() detected, and use it as w in above reasoning to
    conclude all fragments seen by op() come from it. */
  }

 private:
  /** Stored value. */
  T m_value;
  /** Sequence count. Even when the value is ready for read, odd when the value
  is being written to. */
  std::atomic<uint64_t> m_seq{0};
};
}  // namespace ut

#endif /* ut0seq_lock_h */
