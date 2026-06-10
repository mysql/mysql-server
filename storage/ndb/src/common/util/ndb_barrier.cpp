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

#include "util/ndb_barrier.h"
#include <cstddef>
#include <mutex>

ndb::barrier::barrier(std::ptrdiff_t participants)
    : participants(participants), expected(participants) {}

bool ndb::barrier::unblock() {
  --expected;
  if (expected != 0) return false;
  expected = participants;
  phase = !phase;
  cvar.notify_all();
  return true;
}

void ndb::barrier::arrive_and_wait() {
  std::unique_lock<std::mutex> lock(mut);
  if (unblock()) return;
  bool awake_phase = !phase;
  cvar.wait(lock, [this, awake_phase] { return phase == awake_phase; });
}

void ndb::barrier::arrive_and_drop() {
  std::unique_lock<std::mutex> lock(mut);
  --participants;
  unblock();
}

#ifdef TEST_NDB_BARRIER

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

// can not reference to elements in vector<bool> in normal way, use bool8
// instead
using bool8 = std::int8_t;

void thread_func(ndb::barrier *barr, int i, std::atomic<int> *barrier_number,
                 bool8 *fail) {
  ndb::scoped_barrier sbarr(*barr);
  for (int j = 0; j < i; j++) {
    int curr_barr = barrier_number->load(std::memory_order_relaxed);
    fprintf(stderr, "Thread %d: before barrier %d, current barrier %d.\n", i,
            j + 1, curr_barr);
    if (curr_barr != j) {
      fprintf(stderr, "Thread %d: ERROR, barrier out of order.\n", i);
      *fail = true;
      return;
    }
    sbarr.arrive_and_wait();
    curr_barr = barrier_number->exchange(j + 1, std::memory_order_relaxed);
    fprintf(stderr, "Thread %d: after barrier %d, current barrier %d.\n", i,
            j + 1, curr_barr);
    if (curr_barr > j + 1) {
      fprintf(stderr, "Thread %d: ERROR, barrier out of order.\n", i);
      *fail = true;
      return;
    }
  }
  fprintf(stderr, "Thread %d: leaving before barrier %d.\n", i, i + 1);
}

int main() {
  const int thread_count = 3;
  std::vector<std::thread> threads;
  std::vector<bool8> failures(thread_count, false);
  ndb::barrier barr(thread_count);
  std::atomic<int> barrier_number = 0;
  for (int i = 0; i < thread_count; i++) {
    threads.push_back(
        std::thread(thread_func, &barr, i, &barrier_number, &failures[i]));
  }
  for (auto &t : threads) t.join();
  for (bool8 f : failures)
    if (f) return 1;
  return 0;
}

#endif
