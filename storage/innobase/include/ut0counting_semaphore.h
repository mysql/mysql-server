/*****************************************************************************

Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#pragma once
#include "ut0class_life_cycle.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <type_traits>

namespace ut {

namespace detail {
/* TODO: Simplify this once all platforms support std::atomic<T>::notify_one().
In particular gcc 10.2 which we use on ARM platforms with EL 7, still has
no support for it, thus we provide a workaround which uses std::mutex and
std::condition_variable. */

/** A SFINAE helper type which has value=true if and only if std::atomic<T>
supports wait(T). This is the generic fallback which sets it to false. */
template <typename, typename = void>
struct has_atomic_wait : std::false_type {};

/** A SFINAE specialization which kicks in when std::atomic::wait(T) is
implemented. */
template <typename T>
struct has_atomic_wait<T, std::void_t<decltype(std::atomic<T>().wait(T{}))>>
    : std::true_type {};

/** A SFINAE helper from which ut::counting_semaphore inherits whole behaviour.
The has_atomic_wait_support argument is not meant to be specified explicitly.
Instead it will be deduced to be true or false based on the platform.
Then exactly one of the two variants will be instantiated.
Also, in practice T will always be uint32_t, but it has to be a type variable,
so that `has_atomic_wait_support` can *depend* on it. */
template <typename T, bool has_atomic_wait_support = has_atomic_wait<T>::value>
class counting_semaphore_base;

/** Just like std::counting_semaphore except that:

  - acquire(foo) will call foo() whenever it sees the counter==0, which can be
  used to trigger some kind of wakeup mechanism for whoever is hogging the
  resources.
*/
template <typename T>
class counting_semaphore_base<T, true> : Non_copyable {
 public:
  /** Initializes the counter to initial_value */
  counting_semaphore_base(T initial_value)
      : m_counter{initial_value}, m_waiters{0} {}
  /** Waits until the counter is positive, and decrements it by one. */
  template <typename F>
  void acquire(F &&execute_when_zero) {
    for (auto seen = m_counter.load();;) {
      if (seen == 0) {
        std::forward<F>(execute_when_zero)();
        m_waiters.fetch_add(1);
        /* If this thread goes to sleep here, and is in the kernel at moment T,
        then there will be a moment L after T, at which m_waiters.load() will
        be executed from release(). This is sufficient to prove that as long
        as someone is sleeping in the kernel, someone else will try to wake
        them, as m_waiters.load() is non zero as long as someone sleeps, and
        thus m_counter.notify_one() will be called.

        The proof that such moment L must exist is by contradiction.

        If there is no such moment, then there must be well defined "last
        moment at which m_counter.fetch_add(1) was called", let's call it M.
        Clearly M is before T. The same thread which executed this last
        fetch_add, then faced one of 3 cases:

        A) It saw m_waiters.load() == 0, and did nothing.

        B) It saw m_waiters.load() > 0, called m_counter.notify_one(), but
        nobody was asleep at that moment, so woke up nobody.

        C) It saw m_waiters.load() > 0, called m_counter.notify_one() and woken
        up somebody.

        In Case (A), note that since the moment when the sleeping thread
        incremented the m_waiters, it remained above zero at least until the
        moment T at which it still was asleep. This means this
        m_waiters.fetch_add(1) had to happen after M. The sleeping thread went
        to sleep because m_counter.wait(0) saw m_counter==0, and that
        happened-after M. That means that at least one thread successfully
        performed CAS, and thus acquire()d after moment M. But the contract
        requires that after each acquire() someone calls release(). So there
        must be another call to m_counter.fetch_add(1) after M. The
        contradiction ends the proof.

        In Case (B), note that at moment T the thread is asleep, so the call to
        m_counter.notify_one() must've executed the check for sleepers inside
        notify_one() at a moment which is before the sleeping thread has been
        executing the check for m_counter==0 and going to sleep. So, m_counter
        was equal to 0 at some point after M. But that means someone
        successfully executed CAS after M, and just like in case (A) someone
        will call release() after M, contradicting the definition of M.

        In Case (C), this woken up thread is one of runnable threads which have
        a chance to take a look at value of m_counter which was incremented at
        moment M. Either this thread, or some other thread, will therefore
        succeed to CAS it. */
        m_counter.wait(0);
        m_waiters.fetch_sub(1);
        seen = m_counter.load();
      } else if (m_counter.compare_exchange_weak(seen, seen - 1)) {
        return;
      }
    }
  }
  /** Increments the counter by one */
  void release() {
    m_counter.fetch_add(1);
    if (0 < m_waiters.load()) {
      m_counter.notify_one();
    }
  }

 private:
  std::atomic<T> m_counter;
  std::atomic<size_t> m_waiters;
};

/** A specialization to be used on old platforms such as gcc 10.2, which lack
support for atomic wait. The implementation uses mutex and conditional_variable
to achieve same observable behaviour in less optimal way. Still, it tries to use
notify_one() instead of notify_all() and only when m_waiters is non-zero. */
template <typename T>
class counting_semaphore_base<T, false> : Non_copyable {
 public:
  counting_semaphore_base(T initial_value)
      : m_counter{initial_value}, m_waiters{0} {}
  template <typename F>
  void acquire(F &&execute_when_zero) {
    std::unique_lock guard{m_mutex};
    ++m_waiters;
    /* If this thread goes to sleep here, and is in the kernel at moment T,
    then there will be a moment L after T, when release() will be executed.
    This is sufficient to prove that as long as someone is sleeping in the
    kernel, someone else will try to wake them, as m_waiters is non zero,
    as long as someone sleeps, and thus m_is_non_zero.notify_one() will be
    called from release().

    The proof that such moment L must exist is by contradiction.

    If there is no such moment, then there must be well defined "last
    moment at which release() was called", let's call it M.
    Clearly M is before T. The same thread which executed it faced one
    of 3 cases:

    A) It saw m_waiters == 0, and did nothing.

    B) It saw m_waiters > 0, called m_is_non_zero.notify_one(), but
    nobody was asleep at that moment, so woke up nobody.

    C) It saw m_waiters > 0, called m_is_non_zero.notify_one() and woken
    up somebody.

    In Case (A), note that since the moment when the sleeping thread
    did ++m_waiters, it remained above zero at least until the
    moment T at which it still was asleep. But this ++m_waiters happens
    under m_mutex, so had to happen after or before release() at M. It must
    have happened after, given that M saw m_waiters==0. The thread which
    went to sleep saw m_counter == 0, in same critical section, so it
    means some time after release() in M which did ++m_counter, someone
    decremented it again in acquire(). That means someone successfully
    performed acquire(), after moment M. The contract requires that after
    each acquire() one calls release(). So, there must be another call to
    release() after M. The contradiction ends the proof.

    In Case (B), note that at moment T the thread is asleep, so the call to
    m_is_non_zero.notify_one() must've executed the check for sleepers inside
    notify_one() at a moment which is before the sleeping thread has been
    executing the check for m_counter==0 and going to sleep. So, m_counter
    was equal to 0 at some point after M. But that means someone successfully
    executed acquire() after M, and just like in case (A), will call
    release() after M, contradicting the definition of M.

    In Case (C), this woken up thread is one of runnable threads which have a
    chance to take a look at value of m_counter which was incremented at moment
    M. Either this thread, or some other thread, will therefore succeed to call
    acquire(), and then release(), again contradicting the definition of M. */
    m_is_non_zero.wait(guard, [&]() {
      if (0 < m_counter) {
        return true;
      }
      std::forward<F>(execute_when_zero)();
      return false;
    });
    --m_waiters;
    --m_counter;
  }
  void release() {
    std::lock_guard guard{m_mutex};
    ++m_counter;
    if (0 < m_waiters) {
      m_is_non_zero.notify_one();
    }
  }

 private:
  std::mutex m_mutex;
  std::condition_variable m_is_non_zero;
  T m_counter;
  size_t m_waiters;
};

}  // namespace detail

/** Just like std::counting_semaphore except that:

  - acquire(foo) will call foo() whenever it sees the counter==0, which can be
  used to trigger some kind of wakeup mechanism for whoever is hogging the
  resources.

  - compiles on platforms which do not have std::counting_semaphore like
  gcc 10.2

  - implementation is tailored to specific platform, using
  std::atomic<uint32_t>::wait()/notify_one() where possible, and falling back
  to std::mutex + std::conditional_variable on older platforms.

  - The counter is forced to be 32-bit to ensure efficient underlying
  implementation. The std::counting_sempahore<T> relies on
  std::atomic<T>::wait()/notify() mechanism, which in turn is implemented by
  using operating system primitives. On Windows, WaitOnAddress can handle
  64-bit memory locations just fine. But on Linux gcc uses a Futex mechanism
  which supports 32-bit memory locations only. The libstdc++ tries to be more
  abstract and handle even 64-bit integers by hashing their addresses to a
  shared pool of 32-bit integers, and uses Futex on them. But because of
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=115955 it causes contention.
  So, our ut::counting_semaphore does not let you specify the type of the
  counter forcing it to be std::atomic<uint32_t> which guarantees fast
  execution on Linux, too.
*/
using counting_semaphore = detail::counting_semaphore_base<uint32_t>;

}  // namespace ut
