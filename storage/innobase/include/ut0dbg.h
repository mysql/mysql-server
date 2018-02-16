/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/ut0dbg.h
 Debug utilities for Innobase

 Created 1/30/1994 Heikki Tuuri
 **********************************************************************/

#ifndef ut0dbg_h
#define ut0dbg_h

/* Do not include univ.i because univ.i includes this. */

#include "os0thread.h"

/** Report a failed assertion. */
void ut_dbg_assertion_failed(
    const char *expr, /*!< in: the failed assertion */
    const char *file, /*!< in: source file containing the assertion */
    ulint line)       /*!< in: line number of the assertion */
    UNIV_COLD MY_ATTRIBUTE((noreturn));

/** Abort execution if EXPR does not evaluate to nonzero.
@param EXPR assertion expression that should hold */
#define ut_a(EXPR)                                               \
  do {                                                           \
    if (UNIV_UNLIKELY(!(ulint)(EXPR))) {                         \
      ut_dbg_assertion_failed(#EXPR, __FILE__, (ulint)__LINE__); \
    }                                                            \
  } while (0)

/** Abort execution. */
#define ut_error ut_dbg_assertion_failed(0, __FILE__, (ulint)__LINE__)

#ifdef UNIV_DEBUG
/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR) ut_a(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR) EXPR
#else
/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR)
#endif

/** Debug crash point */
#ifdef UNIV_DEBUG
#define DBUG_INJECT_CRASH(prefix, count)            \
  do {                                              \
    char buf[64];                                   \
    snprintf(buf, sizeof buf, prefix "_%u", count); \
    DBUG_EXECUTE_IF(buf, DBUG_SUICIDE(););          \
  } while (0)
#else
#define DBUG_INJECT_CRASH(prefix, count)
#endif

/** Silence warnings about an unused variable by doing a null assignment.
@param A the unused variable */
#define UT_NOT_USED(A) A = A

#if defined(HAVE_SYS_TIME_H) && defined(HAVE_SYS_RESOURCE_H)

#define HAVE_UT_CHRONO_T

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

/** A "chronometer" used to clock snippets of code.
Example usage:
        ut_chrono_t	ch("this loop");
        for (;;) { ... }
        ch.show();
would print the timings of the for() loop, prefixed with "this loop:" */
class ut_chrono_t {
 public:
  /** Constructor.
  @param[in]	name	chrono's name, used when showing the values */
  ut_chrono_t(const char *name) : m_name(name), m_show_from_destructor(true) {
    reset();
  }

  /** Resets the chrono (records the current time in it). */
  void reset() {
    gettimeofday(&m_tv, NULL);

    getrusage(RUSAGE_SELF, &m_ru);
  }

  /** Shows the time elapsed and usage statistics since the last reset. */
  void show() {
    struct rusage ru_now;
    struct timeval tv_now;
    struct timeval tv_diff;

    getrusage(RUSAGE_SELF, &ru_now);

    gettimeofday(&tv_now, NULL);

#ifndef timersub
#define timersub(a, b, r)                       \
  do {                                          \
    (r)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
    (r)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((r)->tv_usec < 0) {                     \
      (r)->tv_sec--;                            \
      (r)->tv_usec += 1000000;                  \
    }                                           \
  } while (0)
#endif /* timersub */

#define CHRONO_PRINT(type, tvp)                            \
  fprintf(stderr, "%s: %s% 5ld.%06ld sec\n", m_name, type, \
          static_cast<long>((tvp)->tv_sec), static_cast<long>((tvp)->tv_usec))

    timersub(&tv_now, &m_tv, &tv_diff);
    CHRONO_PRINT("real", &tv_diff);

    timersub(&ru_now.ru_utime, &m_ru.ru_utime, &tv_diff);
    CHRONO_PRINT("user", &tv_diff);

    timersub(&ru_now.ru_stime, &m_ru.ru_stime, &tv_diff);
    CHRONO_PRINT("sys ", &tv_diff);
  }

  /** Cause the timings not to be printed from the destructor. */
  void end() { m_show_from_destructor = false; }

  /** Destructor. */
  ~ut_chrono_t() {
    if (m_show_from_destructor) {
      show();
    }
  }

 private:
  /** Name of this chronometer. */
  const char *m_name;

  /** True if the current timings should be printed by the destructor. */
  bool m_show_from_destructor;

  /** getrusage() result as of the last reset(). */
  struct rusage m_ru;

  /** gettimeofday() result as of the last reset(). */
  struct timeval m_tv;
};

#endif /* HAVE_SYS_TIME_H && HAVE_SYS_RESOURCE_H */

#endif
