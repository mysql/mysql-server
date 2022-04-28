/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file os/os0thread.cc
 The interface to the operating system thread control primitives

 Created 9/8/1995 Heikki Tuuri
 *******************************************************/

#include "univ.i"

#include <atomic>
#include <ios>
#include <sstream>
#include <thread>

#ifdef UNIV_LINUX
/* include defs for CPU time priority settings */
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#endif /* UNIV_LINUX */

#include "ut0ut.h"

/** We are prepared for a situation that we have this many threads waiting for
a semaphore inside InnoDB. innodb_init_params() sets the value. */
uint32_t srv_max_n_threads = 0;

/** Number of threads active. */
std::atomic_int os_thread_count;

void IB_thread::start() {
  ut_a(state() == State::NOT_STARTED);
  m_state->store(State::ALLOWED_TO_START);

#ifdef _WIN32
  unsigned int cnt = 0;
#endif /* _WIN32 */

  while (state() == State::ALLOWED_TO_START) {
    UT_RELAX_CPU();

#ifdef _WIN32
    /* When the number of threads to be spawned exceeds the number of cores of
    a machine, it's seen that we cannot just rely on UT_RELAX_CPU(). So in such
    a case, allow the thread to release its time slice to any thread wanting
    control. */
    if (++cnt > 500) {
      std::this_thread::yield();
    }
#endif /* _WIN32 */
  }
  const auto state_after_start = state();

  ut_a(state_after_start == State::STARTED ||
       state_after_start == State::STOPPED);
}

void IB_thread::wait(State state_to_wait_for) {
  ut_a(state() != State::INVALID);
  ut_a(m_shared_future.valid());
  if (state_to_wait_for >= State::STOPPED) {
    m_shared_future.wait();
  }
  while (state() < state_to_wait_for) {
    UT_RELAX_CPU();
  }
}

void IB_thread::join() { wait(State::STOPPED); }

void IB_thread::init(std::promise<void> &promise) {
  m_shared_future = promise.get_future();
  m_state.reset(new std::atomic<State>(State::NOT_STARTED));
}

void IB_thread::set_state(State new_state) {
  ut_a(state() != State::INVALID);
  m_state->store(new_state);
}

std::string to_string(std::thread::id thread_id, bool hex_value) {
  try {
    std::stringstream ss;
    if (hex_value) {
      ss << std::hex << std::showbase;
    }
    /* The documentation for this says "May throw implementation-defined
    exceptions." - we catch anything that might be thrown here. */
    ss << thread_id;
    return ss.str();
  } catch (...) {
    return "invalid_thread_id";
  }
}
