/*
   Copyright (c) 2026, Oracle and/or its affiliates.

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
#ifndef NDB_BARRIER_H
#define NDB_BARRIER_H

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace ndb {

/*
 * The ndb::barrier is a simplified version of std::barrier.  It is needed for
 * MySQL Cluster 8.0 where std::barrier is not available. When support for 8.0
 * is dropped this class can be dropped and std::barrier be used instead.
 */
class barrier {
  std::mutex mut;
  std::condition_variable cvar;
  std::ptrdiff_t participants;
  std::ptrdiff_t expected;
  bool phase = false;

  bool unblock();  // return false means caller should wait
 public:
  explicit barrier(std::ptrdiff_t participants);
  // arrive at the barrier and block until all participants have arrived
  void arrive_and_wait();
  // arrive at the barrier and remove self from the participant count
  void arrive_and_drop();
};

/*
 * ndb::scoped_barrier is a wrapper around ndb::barrier or std::barrier to be
 * used in a local scope.  When destructor is called on scope exit it leaves the
 * barrier by calling arrive_and_drop function.
 */
template <typename Barrier = ndb::barrier>
class scoped_barrier {
  Barrier &barr;

 public:
  explicit scoped_barrier(Barrier &barr);
  ~scoped_barrier();
  void arrive_and_wait();
};

template <typename Barrier>
inline scoped_barrier<Barrier>::scoped_barrier(Barrier &barr) : barr(barr) {}
template <typename Barrier>
inline scoped_barrier<Barrier>::~scoped_barrier() {
  barr.arrive_and_drop();
}
template <typename Barrier>
inline void scoped_barrier<Barrier>::arrive_and_wait() {
  barr.arrive_and_wait();
}

}  // namespace ndb

#endif
