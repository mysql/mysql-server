/*****************************************************************************

Copyright (c) 1994, 2023, Oracle and/or its affiliates.

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

/** @file include/ut0ut.h
 Various utilities

 Created 1/20/1994 Heikki Tuuri
 ***********************************************************************/

/**************************************************/ /**
 @page PAGE_INNODB_UTILS Innodb utils

 Useful data structures:
 - @ref Link_buf - to track concurrent operations
 - @ref Sharded_rw_lock - sharded rw-lock (very fast s-lock, slow x-lock)

 *******************************************************/

#ifndef ut0ut_h
#define ut0ut_h

/* Do not include univ.i because univ.i includes this. */

#include <string.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <ostream>
#include <sstream>
#include <type_traits>

#include "db0err.h"

#ifndef UNIV_HOTBACKUP
#include "os0atomic.h"
#endif /* !UNIV_HOTBACKUP */

#include <time.h>

#include <ctype.h>

#include <stdarg.h>
#include "ut/ut.h"
#include "ut0dbg.h"

#ifndef UNIV_NO_ERR_MSGS
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/derror.h"
#endif /* !UNIV_NO_ERR_MSGS */

/** Index name prefix in fast index creation, as a string constant */
#define TEMP_INDEX_PREFIX_STR "\377"

#ifndef UNIV_HOTBACKUP
#if defined(HAVE_PAUSE_INSTRUCTION)
/* According to the gcc info page, asm volatile means that the
instruction has important side-effects and must not be removed.
Also asm volatile may trigger a memory barrier (spilling all registers
to memory). */
#define UT_RELAX_CPU() __asm__ __volatile__("pause")

#elif defined(HAVE_FAKE_PAUSE_INSTRUCTION)
#define UT_RELAX_CPU() __asm__ __volatile__("rep; nop")
#elif defined _WIN32
/* In the Win32 API, the x86 PAUSE instruction is executed by calling
the YieldProcessor macro defined in WinNT.h. It is a CPU architecture-
independent way by using YieldProcessor. */
#define UT_RELAX_CPU() YieldProcessor()
#elif defined(__aarch64__)
/* A "yield" instruction in aarch64 is essentially a nop, and does not cause
enough delay to help backoff. "isb" is a barrier that, especially inside a
loop, creates a small delay without consuming ALU resources.
Experiments shown that adding the isb instruction improves stability and reduces
result jitter. Adding more delay to the UT_RELAX_CPU than a single isb reduces
performance. */
#define UT_RELAX_CPU() __asm__ __volatile__("isb" ::: "memory")
#else
#define UT_RELAX_CPU() __asm__ __volatile__("" ::: "memory")
#endif

#if defined(HAVE_HMT_PRIORITY_INSTRUCTION)
#define UT_LOW_PRIORITY_CPU() __asm__ __volatile__("or 1,1,1")
#define UT_RESUME_PRIORITY_CPU() __asm__ __volatile__("or 2,2,2")
#else
#define UT_LOW_PRIORITY_CPU() ((void)0)
#define UT_RESUME_PRIORITY_CPU() ((void)0)
#endif

#else                  /* !UNIV_HOTBACKUP */
#define UT_RELAX_CPU() /* No op */
#endif                 /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP

/** Calculate the minimum of two pairs.
@param[out]     min_hi  MSB of the minimum pair
@param[out]     min_lo  LSB of the minimum pair
@param[in]      a_hi    MSB of the first pair
@param[in]      a_lo    LSB of the first pair
@param[in]      b_hi    MSB of the second pair
@param[in]      b_lo    LSB of the second pair */
static inline void ut_pair_min(ulint *min_hi, ulint *min_lo, ulint a_hi,
                               ulint a_lo, ulint b_hi, ulint b_lo);
#endif /* !UNIV_HOTBACKUP */

/** Compares two ulints.
@param[in]      a       ulint
@param[in]      b       ulint
@return 1 if a > b, 0 if a == b, -1 if a < b */
static inline int ut_ulint_cmp(ulint a, ulint b);

/** Compare two pairs of integers.
@param[in]      a_h     more significant part of first pair
@param[in]      a_l     less significant part of first pair
@param[in]      b_h     more significant part of second pair
@param[in]      b_l     less significant part of second pair
@return comparison result of (a_h,a_l) and (b_h,b_l)
@retval -1 if (a_h,a_l) is less than (b_h,b_l)
@retval 0 if (a_h,a_l) is equal to (b_h,b_l)
@retval 1 if (a_h,a_l) is greater than (b_h,b_l) */
[[nodiscard]] static inline int ut_pair_cmp(ulint a_h, ulint a_l, ulint b_h,
                                            ulint b_l);

/** Calculates fast the remainder of n/m when m is a power of two.
 @param n in: numerator
 @param m in: denominator, must be a power of two
 @return the remainder of n/m */
#define ut_2pow_remainder(n, m) ((n) & ((m)-1))
/** Calculates the biggest multiple of m that is not bigger than n
 when m is a power of two.  In other words, rounds n down to m * k.
 @param n in: number to round down
 @param m in: alignment, must be a power of two
 @return n rounded down to the biggest possible integer multiple of m */
#define ut_2pow_round(n, m) ((n) & ~((m)-1))
/** Align a number down to a multiple of a power of two.
@param n in: number to round down
@param m in: alignment, must be a power of two
@return n rounded down to the biggest possible integer multiple of m */
#define ut_calc_align_down(n, m) ut_2pow_round(n, m)
/** Calculates the smallest multiple of m that is not smaller than n
 when m is a power of two.  In other words, rounds n up to m * k.
 @param n in: number to round up
 @param m in: alignment, must be a power of two
 @return n rounded up to the smallest possible integer multiple of m */
#define ut_calc_align(n, m) (((n) + ((m)-1)) & ~((m)-1))
/** Calculates fast the 2-logarithm of a number, rounded upward to an
 integer.
 @return logarithm in the base 2, rounded upward */
constexpr ulint ut_2_log(ulint n); /*!< in: number */

/** Calculates 2 to power n.
@param[in]      n       power of 2
@return 2 to power n */
static inline uint32_t ut_2_exp(uint32_t n);

/** Calculates fast the number rounded up to the nearest power of 2.
@param[in]  n   number != 0
@return first power of 2 which is >= n */
ulint ut_2_power_up(ulint n);

/** Determine how many bytes (groups of 8 bits) are needed to
store the given number of bits.
@param b in: bits
@return number of bytes (octets) needed to represent b */
#define UT_BITS_IN_BYTES(b) (((b) + 7UL) / 8UL)

/** Determines if a number is zero or a power of two.
@param[in]      n       number
@return nonzero if n is zero or a power of two; zero otherwise */
#define ut_is_2pow(n) UNIV_LIKELY(!((n) & ((n)-1)))

/** Functor that compares two C strings. Can be used as a comparator for
e.g. std::map that uses char* as keys. */
struct ut_strcmp_functor {
  bool operator()(const char *a, const char *b) const {
    return (strcmp(a, b) < 0);
  }
};

namespace ut {
/** The current value of @@innodb_spin_wait_pause_multiplier. Determines
how many PAUSE instructions to emit for each requested unit of delay
when calling `ut_delay(delay)`. The default value of 50 causes `delay*50` PAUSES
which was equivalent to `delay` microseconds on 100 MHz Pentium + Visual C++.
Useful on processors which have "non-standard" duration of a single PAUSE
instruction - one can compensate for longer PAUSES by setting the
spin_wait_pause_multiplier to a smaller value on such machine */
extern ulong spin_wait_pause_multiplier;
}  // namespace ut

/** Runs an idle loop on CPU. The argument gives the desired delay
 in microseconds on 100 MHz Pentium + Visual C++.
 The actual duration depends on a product of `delay` and the current value of
 @@innodb_spin_wait_pause_multiplier.
 @param[in]   delay   delay in microseconds on 100 MHz Pentium, assuming
                      spin_wait_pause_multiplier is 50 (default).
 @return dummy value */
ulint ut_delay(ulint delay);

/* Forward declaration of transaction handle */
struct trx_t;

/** Get a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier.
 @param         [in]    trx             transaction (NULL=no quotes).
 @param         [in]    name            table name.
 @retval        String quoted as an SQL identifier.
*/
std::string ut_get_name(const trx_t *trx, const char *name);

/** Outputs a fixed-length string, quoted as an SQL identifier.
 If the string contains a slash '/', the string will be
 output as two identifiers separated by a period (.),
 as in SQL database_name.identifier. */
void ut_print_name(FILE *f,           /*!< in: output stream */
                   const trx_t *trx,  /*!< in: transaction */
                   const char *name); /*!< in: table name to print */

/** Format a table name, quoted as an SQL identifier.
If the name contains a slash '/', the result will contain two
identifiers separated by a period (.), as in SQL
database_name.table_name.
@see table_name_t
@param[in]      name            table or index name
@param[out]     formatted       formatted result, will be NUL-terminated
@param[in]      formatted_size  size of the buffer in bytes
@return pointer to 'formatted' */
char *ut_format_name(const char *name, char *formatted, ulint formatted_size);

/** Catenate files.
@param[in] dest Output file
@param[in] src Input file to be appended to output */
void ut_copy_file(FILE *dest, FILE *src);

/** Convert byte value to string with unit
@param[in]      data_bytes      byte value
@param[out]     data_str        formatted string */
void ut_format_byte_value(uint64_t data_bytes, std::string &data_str);

#ifdef _WIN32
/** A substitute for vsnprintf(3), formatted output conversion into
 a limited buffer. Note: this function DOES NOT return the number of
 characters that would have been printed if the buffer was unlimited because
 VC's _vsnprintf() returns -1 in this case and we would need to call
 _vscprintf() in addition to estimate that but we would need another copy
 of "ap" for that and VC does not provide va_copy(). */
void ut_vsnprintf(char *str,       /*!< out: string */
                  size_t size,     /*!< in: str size */
                  const char *fmt, /*!< in: format */
                  va_list ap);     /*!< in: format values */
#else
/** A wrapper for vsnprintf(3), formatted output conversion into
 a limited buffer. Note: this function DOES NOT return the number of
 characters that would have been printed if the buffer was unlimited because
 VC's _vsnprintf() returns -1 in this case and we would need to call
 _vscprintf() in addition to estimate that but we would need another copy
 of "ap" for that and VC does not provide va_copy(). */
#define ut_vsnprintf(buf, size, fmt, ap) ((void)vsnprintf(buf, size, fmt, ap))
#endif /* _WIN32 */

/** Convert an error number to a human readable text message. The
 returned string is static and should not be freed or modified.
 @return string, describing the error */
const char *ut_strerr(dberr_t num); /*!< in: error number */

namespace ib {

/** For measuring time elapsed. Since std::chrono::high_resolution_clock
may be influenced by a change in system time, it might not be steady.
So we use std::chrono::steady_clock for elapsed time. */
class Timer {
 public:
  using SC = std::chrono::steady_clock;

 public:
  /** Constructor. Starts/resets the timer to the current time. */
  Timer() noexcept { reset(); }

  /** Reset the timer to the current time. */
  void reset() { m_start = SC::now(); }

  /** @return the time elapsed in milliseconds. */
  template <typename T = std::chrono::milliseconds>
  int64_t elapsed() const noexcept {
    return std::chrono::duration_cast<T>(SC::now() - m_start).count();
  }

  /** Print time elapsed since last reset (in milliseconds) to the stream.
  @param[in,out] out  Stream to write to.
  @param[in] timer Timer to write to the stream.
  @return stream instance that was passed in. */
  template <typename T, typename Traits>
  friend std::basic_ostream<T, Traits> &operator<<(
      std::basic_ostream<T, Traits> &out, const Timer &timer) noexcept {
    return out << timer.elapsed();
  }

 private:
  /** High resolution timer instance used for timimg. */
  SC::time_point m_start;
};

}  // namespace ib

#ifdef UNIV_HOTBACKUP
/** Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'.
@param[in]      buf     buffer where to sprintf */
void meb_sprintf_timestamp_without_extra_chars(char *buf);
#endif /* UNIV_HOTBACKUP */

struct Wait_stats {
  uint64_t wait_loops;

  explicit Wait_stats(uint64_t wait_loops = 0) : wait_loops(wait_loops) {}

  Wait_stats &operator+=(const Wait_stats &rhs) {
    wait_loops += rhs.wait_loops;
    return (*this);
  }

  Wait_stats operator+(const Wait_stats &rhs) const {
    return (Wait_stats{wait_loops + rhs.wait_loops});
  }

  bool any_waits() const { return (wait_loops != 0); }
};

namespace ib {

/** Allows to monitor an event processing times, allowing to throttle the
processing to one per THROTTLE_DELAY_SEC. */
class Throttler {
 public:
  Throttler() : m_last_applied_time(0) {}

  /** Checks if the item should be processed or ignored to not process them more
  frequently than one per THROTTLE_DELAY_SEC. */
  bool apply() {
    const auto current_time = std::chrono::steady_clock::now();
    const auto current_time_in_sec =
        std::chrono::duration_cast<std::chrono::seconds>(
            current_time.time_since_epoch())
            .count();
    auto last_apply_time = m_last_applied_time.load();
    if (last_apply_time + THROTTLE_DELAY_SEC <
        static_cast<uint64_t>(current_time_in_sec)) {
      if (m_last_applied_time.compare_exchange_strong(last_apply_time,
                                                      current_time_in_sec)) {
        return true;
      }
      /* Any race condition with other threads would mean someone just changed
      the `m_last_apply_time` and will print the message. We don't want
      to retry the operation again. */
    }
    return false;
  }

 private:
  /* Time when the last item was not throttled. Stored as number of seconds
  since epoch. */
  std::atomic<uint64_t> m_last_applied_time;

  /** Throttle all items within that amount seconds from the last non throttled
  one. */
  static constexpr uint64_t THROTTLE_DELAY_SEC = 10;
};
}  // namespace ib

#include "ut0ut.ic"

#endif /* !ut0ut_h */
