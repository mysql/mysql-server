/*****************************************************************************

Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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

/** @file os/os0event.cc
 The interface to the operating system condition variables.

 Created 2012-09-23 Sunny Bains
 *******************************************************/

#include "os0event.h"

#include <errno.h>
#include <time.h>

#include "ha_prototypes.h"
#include "ut0mutex.h"
#include "ut0new.h"

#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

/** The number of microseconds in a second. */
static const uint64_t MICROSECS_IN_A_SECOND = 1000000;

/** The number of nanoseconds in a second. */
static const uint64_t NANOSECS_IN_A_SECOND [[maybe_unused]] =
    1000 * MICROSECS_IN_A_SECOND;

#ifdef _WIN32
/** Native condition variable. */
typedef CONDITION_VARIABLE os_cond_t;
#else
/** Native condition variable */
typedef pthread_cond_t os_cond_t;
#endif /* _WIN32 */

/** InnoDB condition variable. */
struct os_event {
  os_event() UNIV_NOTHROW;

  ~os_event() UNIV_NOTHROW;

  friend void os_event_global_init();
  friend void os_event_global_destroy();

  /**
  Destroys a condition variable */
  void destroy() UNIV_NOTHROW {
#ifndef _WIN32
    int ret = pthread_cond_destroy(&cond_var);
    ut_a(ret == 0);
#endif /* !_WIN32 */

    mutex.destroy();

    ut_ad(n_objects_alive.fetch_sub(1) != 0);
  }

  /** Set the event */
  void set() UNIV_NOTHROW {
    mutex.enter();

    if (!m_set) {
      broadcast();
    }

    mutex.exit();
  }

  bool try_set() UNIV_NOTHROW {
    if (mutex.try_lock()) {
      if (!m_set) {
        broadcast();
      }

      mutex.exit();

      return (true);
    }

    return (false);
  }

  int64_t reset() UNIV_NOTHROW {
    mutex.enter();

    if (m_set) {
      m_set = false;
    }

    int64_t ret = signal_count;

    mutex.exit();

    return (ret);
  }

  /**
  Waits for an event object until it is in the signaled state.

  Typically, if the event has been signalled after the os_event_reset()
  we'll return immediately because event->m_set == true.
  There are, however, situations (e.g.: sync_array code) where we may
  lose this information. For example:

  thread A calls os_event_reset()
  thread B calls os_event_set()   [event->m_set == true]
  thread C calls os_event_reset() [event->m_set == false]
  thread A calls os_event_wait()  [infinite wait!]
  thread C calls os_event_wait()  [infinite wait!]

  Where such a scenario is possible, to avoid infinite wait, the
  value returned by reset() should be passed in as
  reset_sig_count. */
  void wait_low(int64_t reset_sig_count) UNIV_NOTHROW;

  /** Waits for an event object until it is in the signaled state or
  a timeout is exceeded.
  @param  timeout         Timeout in microseconds, or
  std::chrono::microseconds::max()
  @param  reset_sig_count Zero or the value returned by previous call of
  os_event_reset().
  @return       0 if success, OS_SYNC_TIME_EXCEEDED if timeout was exceeded */
  ulint wait_time_low(std::chrono::microseconds timeout,
                      int64_t reset_sig_count) UNIV_NOTHROW;

  /** @return true if the event is in the signalled state. */
  bool is_set() const UNIV_NOTHROW { return (m_set); }

 private:
  /**
  Initialize a condition variable */
  void init() UNIV_NOTHROW {
    mutex.init();

#ifdef _WIN32
    InitializeConditionVariable(&cond_var);
#else
    {
      int ret;

      ret = pthread_cond_init(&cond_var, &cond_attr);
      ut_a(ret == 0);
    }
#endif /* _WIN32 */

    ut_d(n_objects_alive.fetch_add(1));
  }

  /**
  Wait on condition variable */
  void wait() UNIV_NOTHROW {
#ifdef _WIN32
    if (!SleepConditionVariableCS(&cond_var, mutex, INFINITE)) {
      ut_error;
    }
#else
    {
      int ret;

      ret = pthread_cond_wait(&cond_var, mutex);
      ut_a(ret == 0);
    }
#endif /* _WIN32 */
  }

  /**
  Wakes all threads waiting for condition variable */
  void broadcast() UNIV_NOTHROW {
    m_set = true;
    ++signal_count;

#ifdef _WIN32
    WakeAllConditionVariable(&cond_var);
#else
    {
      int ret;

      ret = pthread_cond_broadcast(&cond_var);
      ut_a(ret == 0);
    }
#endif /* _WIN32 */
  }

  /**
  Wakes one thread waiting for condition variable */
  void signal() UNIV_NOTHROW {
#ifdef _WIN32
    WakeConditionVariable(&cond_var);
#else
    {
      int ret;

      ret = pthread_cond_signal(&cond_var);
      ut_a(ret == 0);
    }
#endif /* _WIN32 */
  }

  /**
  Do a timed wait on condition variable.
  @return true if timed out, false otherwise */
  bool timed_wait(
#ifndef _WIN32
      const timespec *abstime /*!< Timeout. */
#else
      DWORD time_in_ms /*!< Timeout in milliseconds. */
#endif /* !_WIN32 */
  );

#ifndef _WIN32
  /** Returns absolute time until which we should wait if
  we wanted to wait for timeout since now.
  This method could be removed if we switched to the usage
  of std::condition_variable. */
  struct timespec get_wait_timelimit(std::chrono::microseconds timeout);
#endif /* !_WIN32 */

 private:
  bool m_set;           /*!< this is true when the
                        event is in the signaled
                        state, i.e., a thread does
                        not stop if it tries to wait
                        for this event */
  int64_t signal_count; /*!< this is incremented
                        each time the event becomes
                        signaled */
  EventMutex mutex;     /*!< this mutex protects
                        the next fields */

  os_cond_t cond_var; /*!< condition variable is
                      used in waiting for the event */

#ifndef _WIN32
  /** Attributes object passed to pthread_cond_* functions.
  Defines usage of the monotonic clock if it's available.
  Initialized once, in the os_event::global_init(), and
  destroyed in the os_event::global_destroy(). */
  static pthread_condattr_t cond_attr;

  /** True iff usage of the monotonic clock has been successfully
  enabled for the cond_attr object. */
  static bool cond_attr_has_monotonic_clock;
#endif /* !_WIN32 */
  static bool global_initialized;

#ifdef UNIV_DEBUG
  static std::atomic_size_t n_objects_alive;
#endif /* UNIV_DEBUG */

 protected:
  // Disable copying
  os_event(const os_event &);
  os_event &operator=(const os_event &);
};

bool os_event::timed_wait(
#ifndef _WIN32
    const timespec *abstime
#else
    DWORD time_in_ms
#endif /* !_WIN32 */
) {
#ifdef _WIN32
  BOOL ret;

  ret = SleepConditionVariableCS(&cond_var, mutex, time_in_ms);

  if (!ret) {
    DWORD err = GetLastError();

    /* FQDN=msdn.microsoft.com
    @see http://$FQDN/en-us/library/ms686301%28VS.85%29.aspx,

    "Condition variables are subject to spurious wakeups
    (those not associated with an explicit wake) and stolen wakeups
    (another thread manages to run before the woken thread)."
    Check for both types of timeouts.
    Conditions are checked by the caller.*/
    if (err == WAIT_TIMEOUT || err == ERROR_TIMEOUT) {
      return (true);
    }
  }

  ut_a(ret);

  return (false);
#else
  int ret;

  ret = pthread_cond_timedwait(&cond_var, mutex, abstime);

  switch (ret) {
    case 0:
    case ETIMEDOUT:
      /* We play it safe by checking for EINTR even though
      according to the POSIX documentation it can't return EINTR. */
    case EINTR:
      break;

    default:
#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_742)
#endif /* !UNIV_NO_ERR_MSGS */
          << "pthread_cond_timedwait() returned: " << ret << ": abstime={"
          << abstime->tv_sec << "," << abstime->tv_nsec << "}";
      ut_error;
  }

  return (ret == ETIMEDOUT);
#endif /* _WIN32 */
}

/**
Waits for an event object until it is in the signaled state.

Typically, if the event has been signalled after the os_event_reset()
we'll return immediately because event->m_set == true.
There are, however, situations (e.g.: sync_array code) where we may
lose this information. For example:

thread A calls os_event_reset()
thread B calls os_event_set()   [event->m_set == true]
thread C calls os_event_reset() [event->m_set == false]
thread A calls os_event_wait()  [infinite wait!]
thread C calls os_event_wait()  [infinite wait!]

Where such a scenario is possible, to avoid infinite wait, the
value returned by reset() should be passed in as
reset_sig_count. */
void os_event::wait_low(int64_t reset_sig_count) UNIV_NOTHROW {
  mutex.enter();

  if (!reset_sig_count) {
    reset_sig_count = signal_count;
  }

  while (!m_set && signal_count == reset_sig_count) {
    wait();

    /* Spurious wakeups may occur: we have to check if the
    event really has been signaled after we came here to wait. */
  }

  mutex.exit();
}

#ifndef _WIN32

struct timespec os_event::get_wait_timelimit(
    std::chrono::microseconds timeout) {
  /* We could get rid of this function if we switched to std::condition_variable
  from the pthread_cond_. The std::condition_variable::wait_for relies on the
  steady_clock internally and accepts timeout (not time increased by the
  timeout). */
  for (int i = 0;; i++) {
    ut_a(i < 10);
#ifdef HAVE_CLOCK_GETTIME
    if (cond_attr_has_monotonic_clock) {
      struct timespec tp;
      if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
        const auto errno_clock_gettime = errno;

#ifndef UNIV_NO_ERR_MSGS
        ib::error(ER_IB_MSG_CLOCK_GETTIME_FAILED,
                  strerror(errno_clock_gettime));
#endif /* !UNIV_NO_ERR_MSGS */

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        errno = errno_clock_gettime;

      } else {
        const auto increased =
            tp.tv_nsec +
            std::chrono::duration_cast<std::chrono::nanoseconds>(timeout)
                .count();
        if (increased >= static_cast<std::remove_cv<decltype(increased)>::type>(
                             NANOSECS_IN_A_SECOND)) {
          tp.tv_sec += increased / NANOSECS_IN_A_SECOND;
          tp.tv_nsec = increased % NANOSECS_IN_A_SECOND;
        } else {
          tp.tv_nsec = increased;
        }
        return (tp);
      }

    } else
#endif /* HAVE_CLOCK_GETTIME */
    {
      struct timeval tv;
      if (gettimeofday(&tv, nullptr) == -1) {
        const auto errno_gettimeofday = errno;

#ifndef UNIV_NO_ERR_MSGS
        ib::error(ER_IB_MSG_1213, strerror(errno_gettimeofday));
#endif /* !UNIV_NO_ERR_MSGS */

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        errno = errno_gettimeofday;

      } else {
        const auto increased = tv.tv_usec + timeout.count();

        if (increased >= static_cast<std::remove_cv<decltype(increased)>::type>(
                             MICROSECS_IN_A_SECOND)) {
          tv.tv_sec += increased / MICROSECS_IN_A_SECOND;
          tv.tv_usec = increased % MICROSECS_IN_A_SECOND;
        } else {
          tv.tv_usec = increased;
        }

        struct timespec abstime;
        abstime.tv_sec = tv.tv_sec;
        abstime.tv_nsec = tv.tv_usec * 1000;
        return (abstime);
      }
    }
  }
}

#endif /* !_WIN32 */

ulint os_event::wait_time_low(std::chrono::microseconds timeout,
                              int64_t reset_sig_count) UNIV_NOTHROW {
  bool timed_out = false;

#ifdef _WIN32
  DWORD time_in_ms;

  if (timeout != std::chrono::microseconds::max()) {
    time_in_ms = static_cast<DWORD>(
        std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
  } else {
    time_in_ms = INFINITE;
  }
#else
  struct timespec abstime;

  if (timeout != std::chrono::microseconds::max()) {
    abstime = os_event::get_wait_timelimit(timeout);
  } else {
    abstime.tv_nsec = 999999999;
    abstime.tv_sec = std::numeric_limits<time_t>::max();
  }

  ut_a(abstime.tv_nsec <= 999999999);

#endif /* _WIN32 */

  mutex.enter();

  if (!reset_sig_count) {
    reset_sig_count = signal_count;
  }

  do {
    if (m_set || signal_count != reset_sig_count) {
      break;
    }

#ifndef _WIN32
    timed_out = timed_wait(&abstime);
#else
    timed_out = timed_wait(time_in_ms);
#endif /* !_WIN32 */

  } while (!timed_out);

  mutex.exit();

  return (timed_out ? OS_SYNC_TIME_EXCEEDED : 0);
}

/** Constructor */
os_event::os_event() UNIV_NOTHROW {
  ut_a(global_initialized);
  init();

  m_set = false;

  /* We return this value in os_event_reset(),
  which can then be be used to pass to the
  os_event_wait_low(). The value of zero is
  reserved in os_event_wait_low() for the case
  when the caller does not want to pass any
  signal_count value. To distinguish between
  the two cases we initialize signal_count
  to 1 here. */

  signal_count = 1;
}

/** Destructor */
os_event::~os_event() UNIV_NOTHROW { destroy(); }

/**
Creates an event semaphore, i.e., a semaphore which may just have two
states: signaled and nonsignaled. The created event is manual reset: it
must be reset explicitly by calling sync_os_reset_event.
@return the event handle */
os_event_t os_event_create() {
  os_event_t ret = (ut::new_withkey<os_event>(UT_NEW_THIS_FILE_PSI_KEY));
/**
 On SuSE Linux we get spurious EBUSY from pthread_mutex_destroy()
 unless we grab and release the mutex here. Current OS version:
 openSUSE Leap 15.0
 Linux xxx 4.12.14-lp150.12.25-default #1 SMP
 Thu Nov 1 06:14:23 UTC 2018 (3fcf457) x86_64 x86_64 x86_64 GNU/Linux */
#if defined(LINUX_SUSE)
  os_event_reset(ret);
#endif
  return ret;
}

/**
Check if the event is set.
@return true if set */
bool os_event_is_set(const os_event_t event) /*!< in: event to test */
{
  return (event->is_set());
}

/**
Sets an event semaphore to the signaled state: lets waiting threads
proceed. */
void os_event_set(os_event_t event) /*!< in/out: event to set */
{
  event->set();
}

bool os_event_try_set(os_event_t event) { return (event->try_set()); }

/**
Resets an event semaphore to the nonsignaled state. Waiting threads will
stop to wait for the event.
The return value should be passed to os_even_wait_low() if it is desired
that this thread should not wait in case of an intervening call to
os_event_set() between this os_event_reset() and the
os_event_wait_low() call. See comments for os_event_wait_low().
@return current signal_count. */
int64_t os_event_reset(os_event_t event) /*!< in/out: event to reset */
{
  return (event->reset());
}

ulint os_event_wait_time_low(os_event_t event,
                             std::chrono::microseconds timeout,
                             int64_t reset_sig_count) {
  return (event->wait_time_low(timeout, reset_sig_count));
}

/**
Waits for an event object until it is in the signaled state.

Where such a scenario is possible, to avoid infinite wait, the
value returned by os_event_reset() should be passed in as
reset_sig_count. */
void os_event_wait_low(os_event_t event,        /*!< in: event to wait */
                       int64_t reset_sig_count) /*!< in: zero or the value
                                                returned by previous call of
                                                os_event_reset(). */
{
  event->wait_low(reset_sig_count);
}

/**
Frees an event object. */
void os_event_destroy(os_event_t &event) /*!< in/own: event to free */

{
  if (event != nullptr) {
    ut::delete_(event);
    event = nullptr;
  }
}

#ifndef _WIN32
pthread_condattr_t os_event::cond_attr;
bool os_event::cond_attr_has_monotonic_clock{false};
#endif /* !_WIN32 */
bool os_event::global_initialized{false};

#ifdef UNIV_DEBUG
std::atomic_size_t os_event::n_objects_alive{0};
#endif /* UNIV_DEBUG */

void os_event_global_init(void) {
  ut_ad(os_event::n_objects_alive.load() == 0);
#ifndef _WIN32
  int ret = pthread_condattr_init(&os_event::cond_attr);
  ut_a(ret == 0);

#ifdef UNIV_LINUX /* MacOS does not have support. */
#ifdef HAVE_CLOCK_GETTIME
  ret = pthread_condattr_setclock(&os_event::cond_attr, CLOCK_MONOTONIC);
  if (ret == 0) {
    os_event::cond_attr_has_monotonic_clock = true;
  }
#endif /* HAVE_CLOCK_GETTIME */

#ifndef UNIV_NO_ERR_MSGS
  if (!os_event::cond_attr_has_monotonic_clock) {
    ib::warn(ER_IB_MSG_CLOCK_MONOTONIC_UNSUPPORTED);
  }
#endif /* !UNIV_NO_ERR_MSGS */

#endif /* UNIV_LINUX */
#endif /* !_WIN32 */
  os_event::global_initialized = true;
}

void os_event_global_destroy(void) {
  ut_a(os_event::global_initialized);
  ut_ad(os_event::n_objects_alive.load() == 0);
#ifndef _WIN32
  os_event::cond_attr_has_monotonic_clock = false;
#ifdef UNIV_DEBUG
  const int ret =
#endif /* UNIV_DEBUG */
      pthread_condattr_destroy(&os_event::cond_attr);
  ut_ad(ret == 0);
#endif /* !_WIN32 */
  os_event::global_initialized = false;
}
