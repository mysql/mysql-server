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

/** @file include/os0thread.h
 The interface to the operating system
 process and thread control primitives

 Created 9/8/1995 Heikki Tuuri
 *******************************************************/

#ifndef os0thread_h
#define os0thread_h

#include <atomic>
#include <cstring>
#include <functional>
#include <future>
#include <sstream>
#include <thread>
#include "ut0dbg.h"

class IB_thread {
 public:
  enum class State { INVALID, NOT_STARTED, ALLOWED_TO_START, STARTED, STOPPED };

  State state() const {
    return (m_state == nullptr ? State::INVALID : m_state->load());
  }

  void start();
  void wait(State state_to_wait_for = State::STOPPED);
  void join();

 private:
  std::shared_future<void> m_shared_future;
  std::shared_ptr<std::atomic<State>> m_state;

  void init(std::promise<void> &promise);
  void set_state(State state);

  friend class Detached_thread;
};

/** Operating system thread native handle */
using os_thread_id_t = std::thread::native_handle_type;

namespace ut {
/** The hash value of the current thread's id */
const inline thread_local size_t this_thread_hash =
    std::hash<std::thread::id>{}(std::this_thread::get_id());
}  // namespace ut

/** Returns the string representation of the thread ID supplied. It uses the
 only standard-compliant way of printing the thread ID.
 @param thread_id The thread ID to convert to string.
 @param hex_value If true, the conversion will be asked to output in
 hexadecimal format. The support for it is OS-implementation-dependent and may
 be ignored.
*/
std::string to_string(std::thread::id thread_id, bool hex_value = false);

/** A class to allow any trivially copyable object to be XOR'ed. Trivially
copyable according to
https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable means we can
copy the underlying representation to array of chars, and back and consider
it a valid copy. It is thread-safe when changing, but no modifications must be
assured during reading the stored value. */
template <typename T_thing, typename T_digit>
class Atomic_xor_of_things {
 public:
  Atomic_xor_of_things() {
    /* We could just default-initialize the acc with acc{}, if not the
     SunStudio. */
    for (size_t i = 0; i < digits_count; i++) {
      acc[i].store(0);
    }
  }

  void xor_thing(T_thing id) {
    /* A buffer filled with zeros to pad the thing to next sizeof(T_digit)
     bytes. It is thread-safe. */
    char buff[sizeof(T_digit) * digits_count]{};
    memcpy(buff, &id, sizeof(T_thing));
    for (size_t i = 0; i < digits_count; i++) {
      T_digit x;
      memcpy(&x, buff + i * sizeof(T_digit), sizeof(T_digit));
      acc[i].fetch_xor(x);
    }
  }

  /** Returns an object that was XOR'ed odd number of times. This function
   assumes there is exactly one such object, and caller must assure this. This
   method is not thread-safe and caller must ensure no other thread is trying
   to modify the value. */
  T_thing recover_if_single() {
    T_thing res;
    char buff[sizeof(T_digit) * digits_count];
    for (size_t i = 0; i < acc.size(); ++i) {
      T_digit x = acc[i].load(std::memory_order_acquire);
      memcpy(buff + i * sizeof(T_digit), &x, sizeof(T_digit));
    }
    memcpy(&res, buff, sizeof(T_thing));
    return res;
  }

 private:
  static constexpr size_t digits_count =
      (sizeof(T_thing) + sizeof(T_digit) - 1) / sizeof(T_digit);
  /* initial value must be all zeros, as opposed to the representation of
  thing{}, because we care about "neutral element of the XOR operation", and not
  "default value of a thing". */
  std::array<std::atomic<T_digit>, digits_count> acc;
};

#ifdef _WIN32
#include <Windows.h>
#include "ut0class_life_cycle.h"

/** Manages a Windows Event object. Destroys it when leaving a scope. */
class Scoped_event : private ut::Non_copyable {
 public:
  /** Creates a new Windows Event object. It is created in manual-reset mode and
  non-signalled start state. It asserts the Event object is created
  successfully. */
  Scoped_event() : m_event(CreateEvent(nullptr, TRUE, FALSE, nullptr)) {
    /* In case different params are specified, like for example event name, then
    the errors may be possible and could be handled. The m_event stored could be
    NULL, for the application to test successful event creation with the
    get_handle() method, but this is currently not supported (and thus not
    tested) by this implementation. */
    ut_a(m_event != NULL);
  }
  ~Scoped_event() {
    if (m_event != NULL) {
      CloseHandle(m_event);
    }
  }
  /** Returns a HANDLE to managed Event. */
  HANDLE get_handle() const { return m_event; }

 private:
  HANDLE m_event;
};
#endif

/** A type for std::thread::id digit to store XOR efficiently. This will make
 the compiler to optimize the operations hopefully to single instruction. */
using Xor_digit_for_thread_id =
    std::conditional<sizeof(std::thread::id) >= sizeof(uint64_t), uint64_t,
                     uint32_t>::type;

/** A type to store XORed objects of type std::thread::id */
using Atomic_xor_of_thread_id =
    Atomic_xor_of_things<std::thread::id, Xor_digit_for_thread_id>;

#endif /* !os0thread_h */
