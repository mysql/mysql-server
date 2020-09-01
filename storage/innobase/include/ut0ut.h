/*****************************************************************************

Copyright (c) 1994, 2020, Oracle and/or its affiliates.

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

/** Get the format string for the logger.
@param[in]	errcode		The error code from share/errmsg-*.txt
@return the message string or nullptr */
const char *srv_get_server_errmsgs(int errcode);

/** Time stamp */
typedef time_t ib_time_t;

/** Time stamp read from the monotonic clock (returned by ut_time_monotonic()).
 */
typedef int64_t ib_time_monotonic_t;

/** Number of milliseconds read from the monotonic clock (returned by
 * ut_time_monotonic_ms()). */
typedef int64_t ib_time_monotonic_ms_t;

/** Number of microseconds read from the monotonic clock (returned by
 * ut_time_monotonic_us()). */
typedef int64_t ib_time_monotonic_us_t;

#ifndef UNIV_HOTBACKUP
#if defined(HAVE_PAUSE_INSTRUCTION)
/* According to the gcc info page, asm volatile means that the
instruction has important side-effects and must not be removed.
Also asm volatile may trigger a memory barrier (spilling all registers
to memory). */
#ifdef __SUNPRO_CC
#define UT_RELAX_CPU() asm("pause")
#else
#define UT_RELAX_CPU() __asm__ __volatile__("pause")
#endif /* __SUNPRO_CC */

#elif defined(HAVE_FAKE_PAUSE_INSTRUCTION)
#define UT_RELAX_CPU() __asm__ __volatile__("rep; nop")
#elif defined _WIN32
/* In the Win32 API, the x86 PAUSE instruction is executed by calling
the YieldProcessor macro defined in WinNT.h. It is a CPU architecture-
independent way by using YieldProcessor. */
#define UT_RELAX_CPU() YieldProcessor()
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

/** Delays execution for at most max_wait_us microseconds or returns earlier
 if cond becomes true.
 @param cond in: condition to wait for; evaluated every 2 ms
 @param max_wait_us in: maximum delay to wait, in microseconds */
#define UT_WAIT_FOR(cond, max_wait_us)                                        \
  do {                                                                        \
    const auto start_us = ut_time_monotonic_us();                             \
    while (!(cond)) {                                                         \
      const auto diff = ut_time_monotonic_us() - start_us;                    \
      const auto limit = max_wait_us;                                         \
      if (limit <= 0 || (diff > 0 && ((uint64_t)diff) > ((uint64_t)limit))) { \
        break;                                                                \
      }                                                                       \
      os_thread_sleep(2000 /* 2 ms */);                                       \
    }                                                                         \
  } while (0)
#else                  /* !UNIV_HOTBACKUP */
#define UT_RELAX_CPU() /* No op */
#endif                 /* !UNIV_HOTBACKUP */

#define ut_max std::max
#define ut_min std::min

/** Calculate the minimum of two pairs.
@param[out]	min_hi	MSB of the minimum pair
@param[out]	min_lo	LSB of the minimum pair
@param[in]	a_hi	MSB of the first pair
@param[in]	a_lo	LSB of the first pair
@param[in]	b_hi	MSB of the second pair
@param[in]	b_lo	LSB of the second pair */
UNIV_INLINE
void ut_pair_min(ulint *min_hi, ulint *min_lo, ulint a_hi, ulint a_lo,
                 ulint b_hi, ulint b_lo);

/** Compares two ulints.
@param[in]	a	ulint
@param[in]	b	ulint
@return 1 if a > b, 0 if a == b, -1 if a < b */
UNIV_INLINE
int ut_ulint_cmp(ulint a, ulint b);

/** Compare two pairs of integers.
@param[in]	a_h	more significant part of first pair
@param[in]	a_l	less significant part of first pair
@param[in]	b_h	more significant part of second pair
@param[in]	b_l	less significant part of second pair
@return comparison result of (a_h,a_l) and (b_h,b_l)
@retval -1 if (a_h,a_l) is less than (b_h,b_l)
@retval 0 if (a_h,a_l) is equal to (b_h,b_l)
@retval 1 if (a_h,a_l) is greater than (b_h,b_l) */
UNIV_INLINE
int ut_pair_cmp(ulint a_h, ulint a_l, ulint b_h, ulint b_l)
    MY_ATTRIBUTE((warn_unused_result));

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
UNIV_INLINE
ulint ut_2_log(ulint n); /*!< in: number */

/** Calculates 2 to power n.
@param[in]	n	power of 2
@return 2 to power n */
UNIV_INLINE
uint32_t ut_2_exp(uint32_t n);

/** Calculates fast the number rounded up to the nearest power of 2.
@param[in]  n   number != 0
@return first power of 2 which is >= n */
ulint ut_2_power_up(ulint n);

/** Determine how many bytes (groups of 8 bits) are needed to
store the given number of bits.
@param b in: bits
@return number of bytes (octets) needed to represent b */
#define UT_BITS_IN_BYTES(b) (((b) + 7) / 8)

/** Returns system time. We do not specify the format of the time returned:
 the only way to manipulate it is to use the function ut_difftime.
 @return system time */
ib_time_t ut_time(void);

/** Returns the number of microseconds since epoch. Uses the monotonic clock.
 @return us since epoch or 0 if failed to retrieve */
ib_time_monotonic_us_t ut_time_monotonic_us(void);

/** Returns the number of milliseconds since epoch. Uses the monotonic clock.
 @return ms since epoch */
ib_time_monotonic_ms_t ut_time_monotonic_ms(void);

/** Returns the number of seconds since epoch. Uses the monotonic clock.
 @return us since epoch or 0 if failed to retrieve */
ib_time_monotonic_t ut_time_monotonic(void);

/** Returns the difference of two times in seconds.
 @return time2 - time1 expressed in seconds */
double ut_difftime(ib_time_t time2,  /*!< in: time */
                   ib_time_t time1); /*!< in: time */

/** Determines if a number is zero or a power of two.
@param[in]	n	number
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
 @param		[in]	trx		transaction (NULL=no quotes).
 @param		[in]	name		table name.
 @retval	String quoted as an SQL identifier.
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
@param[in]	name		table or index name
@param[out]	formatted	formatted result, will be NUL-terminated
@param[in]	formatted_size	size of the buffer in bytes
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

#ifdef UNIV_DEBUG
/** Finds the first format specifier in `fmt` format string
@param[in]   fmt  The format string
@return Either the longest suffix of `fmt` which starts with format specifier,
or `nullptr` if could not find any format specifier inside `fmt`.
*/
static inline const char *get_first_format(const char *fmt) {
  const char *pos = strchr(fmt, '%');
  if (pos != nullptr && pos[1] == '%') {
    return (get_first_format(pos + 2));
  }
  return (pos);
}

/** Verifies that the `fmt` format string does not require any arguments
@param[in]   fmt  The format string
@return true if and only if there is no format specifier inside `fmt` which
requires passing an argument */
static inline bool verify_fmt_match(const char *fmt) {
  return (get_first_format(fmt) == nullptr);
}

/** Verifies that the `fmt` format string contains format specifiers which match
the type and order of the arguments
@param[in]  fmt   The format string
@param[in]  head  The first argument
@param[in]  tail  Others (perhaps none) arguments
@return true if and only if the format specifiers found in `fmt` correspond to
types of head, tail...
*/
template <typename Head, typename... Tail>
static bool verify_fmt_match(const char *fmt, Head &&head, Tail &&... tail) {
  using H =
      typename std::remove_cv<typename std::remove_reference<Head>::type>::type;
  const char *pos = get_first_format(fmt);
  if (pos == nullptr) {
    return (false);
  }
  /* We currently only handle :
  %[-0-9.*]*(d|ld|lld|u|lu|llu|zu|zx|zd|s|x|i|f|c|X|p|lx|llx|lf)
  Feel free to extend the parser, if you need something more, as the parser is
  not intended to be any stricter than real printf-format parser, and if it does
  not handle something, it is only to keep the code simpler. */
  const std::string skipable("-+ #0123456789.*");

  pos++;
  while (*pos != '\0' && skipable.find_first_of(*pos) != std::string::npos) {
    pos++;
  }
  if (*pos == '\0') {
    return (false);
  }
  bool is_ok = true;
  if (pos[0] == 'l') {
    if (pos[1] == 'l') {
      if (pos[2] == 'd') {
        is_ok = std::is_same<H, long long int>::value;
      } else if (pos[2] == 'u' || pos[2] == 'x') {
        is_ok = std::is_same<H, unsigned long long int>::value;
      } else {
        is_ok = false;
      }
    } else if (pos[1] == 'd') {
      is_ok = std::is_same<H, long int>::value;
    } else if (pos[1] == 'u') {
      is_ok = std::is_same<H, unsigned long int>::value;
    } else if (pos[1] == 'x') {
      is_ok = std::is_same<H, unsigned long int>::value;
    } else if (pos[1] == 'f') {
      is_ok = std::is_same<H, double>::value;
    } else {
      is_ok = false;
    }
  } else if (pos[0] == 'd') {
    is_ok = std::is_same<H, int>::value;
  } else if (pos[0] == 'u') {
    is_ok = std::is_same<H, unsigned int>::value;
  } else if (pos[0] == 'x') {
    is_ok = std::is_same<H, unsigned int>::value;
  } else if (pos[0] == 'X') {
    is_ok = std::is_same<H, unsigned int>::value;
  } else if (pos[0] == 'i') {
    is_ok = std::is_same<H, int>::value;
  } else if (pos[0] == 'f') {
    is_ok = std::is_same<H, float>::value;
  } else if (pos[0] == 'c') {
    is_ok = std::is_same<H, char>::value;
  } else if (pos[0] == 'p') {
    is_ok = std::is_pointer<H>::value;
  } else if (pos[0] == 's') {
    is_ok = (std::is_same<H, char *>::value ||
             std::is_same<H, char const *>::value ||
             (std::is_array<H>::value &&
              std::is_same<typename std::remove_cv<
                               typename std::remove_extent<H>::type>::type,
                           char>::value));
  } else if (pos[0] == 'z') {
    if (pos[1] == 'u') {
      is_ok = std::is_same<H, size_t>::value;
    } else if (pos[1] == 'x') {
      is_ok = std::is_same<H, size_t>::value;
    } else if (pos[1] == 'd') {
      is_ok = std::is_same<H, ssize_t>::value;
    } else {
      is_ok = false;
    }
  } else {
    is_ok = false;
  }
  return (is_ok && verify_fmt_match(pos + 1, std::forward<Tail>(tail)...));
}
#endif /* UNIV_DEBUG */

/** This is a wrapper class, used to print any unsigned integer type
in hexadecimal format.  The main purpose of this data type is to
overload the global operator<<, so that we can print the given
wrapper value in hex. */
struct hex {
  explicit hex(uintmax_t t) : m_val(t) {}
  const uintmax_t m_val;
};

/** This is an overload of the global operator<< for the user defined type
ib::hex.  The unsigned value held in the ib::hex wrapper class will be printed
into the given output stream in hexadecimal format.
@param[in,out]	lhs	the output stream into which rhs is written.
@param[in]	rhs	the object to be written into lhs.
@retval	reference to the output stream. */
inline std::ostream &operator<<(std::ostream &lhs, const hex &rhs) {
  std::ios_base::fmtflags ff = lhs.flags();
  lhs << std::showbase << std::hex << rhs.m_val;
  lhs.setf(ff);
  return (lhs);
}

/** The class logger is the base class of all the error log related classes.
It contains a std::ostringstream object.  The main purpose of this class is
to forward operator<< to the underlying std::ostringstream object.  Do not
use this class directly, instead use one of the derived classes. */
class logger {
 public:
  /** Destructor */
  virtual ~logger();

#ifndef UNIV_NO_ERR_MSGS

  /** Format an error message.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  logger &log(int err, Args &&... args) {
    ut_a(m_err == ER_IB_MSG_0);

    m_err = err;

    m_oss << msg(err, std::forward<Args>(args)...);

    return (*this);
  }

#endif /* !UNIV_NO_ERR_MSGS */

  template <typename T>
  logger &operator<<(const T &rhs) {
    m_oss << rhs;
    return (*this);
  }

  /** Write the given buffer to the internal string stream object.
  @param[in]	buf		the buffer contents to log.
  @param[in]	count		the length of the buffer buf.
  @return the output stream into which buffer was written. */
  std::ostream &write(const char *buf, std::streamsize count) {
    return (m_oss.write(buf, count));
  }

  /** Write the given buffer to the internal string stream object.
  @param[in]	buf		the buffer contents to log
  @param[in]	count		the length of the buffer buf.
  @return the output stream into which buffer was written. */
  std::ostream &write(const byte *buf, std::streamsize count) {
    return (m_oss.write(reinterpret_cast<const char *>(buf), count));
  }

 public:
  /** For converting the message into a string. */
  std::ostringstream m_oss;

#ifndef UNIV_NO_ERR_MSGS
  /** Error code in errmsg-*.txt */
  int m_err{};

  /** Error logging level. */
  loglevel m_level{INFORMATION_LEVEL};
#endif /* !UNIV_NO_ERR_MSGS */

#ifdef UNIV_HOTBACKUP
  /** For MEB trace infrastructure. */
  int m_trace_level{};
#endif /* UNIV_HOTBACKUP */

 protected:
#ifndef UNIV_NO_ERR_MSGS
  /** Format an error message.
  @param[in]	err	Error code from errmsg-*.txt.
  @param[in]	args	Variable length argument list */
  template <class... Args>
  static std::string msg(int err, Args &&... args) {
    const char *fmt = srv_get_server_errmsgs(err);

    int ret;
    char buf[LOG_BUFF_MAX];
#ifdef UNIV_DEBUG
    if (get_first_format(fmt) != nullptr) {
      if (!verify_fmt_match(fmt, std::forward<Args>(args)...)) {
        fprintf(stderr, "The format '%s' does not match arguments\n", fmt);
        ut_error;
      }
    }
#endif
    ret = snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);

    std::string str;

    if (ret > 0 && (size_t)ret < sizeof(buf)) {
      str.append(buf);
    }

    return (str);
  }

 protected:
  /** Uses LogEvent to report the log entry, using provided message
  @param[in]    msg    message to be logged
  */
  void log_event(std::string msg);

  /** Constructor.
  @param[in]	level		Logging level
  @param[in]	err		Error message code. */
  logger(loglevel level, int err) : m_err(err), m_level(level) {
    /* Note: Dummy argument to avoid the warning:

    "format not a string literal and no format arguments"
    "[-Wformat-security]"

    The warning only kicks in if the call is of the form:

       snprintf(buf, sizeof(buf), str);
    */

    m_oss << msg(err, "");
  }

  /** Constructor.
  @param[in]	level		Logging level
  @param[in]	err		Error message code.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit logger(loglevel level, int err, Args &&... args)
      : m_err(err), m_level(level) {
    m_oss << msg(err, std::forward<Args>(args)...);
  }

  /** Constructor
  @param[in]	level		Log error level */
  explicit logger(loglevel level) : m_err(ER_IB_MSG_0), m_level(level) {}

#endif /* !UNIV_NO_ERR_MSGS */
};

/** The class info is used to emit informational log messages.  It is to be
used similar to std::cout.  But the log messages will be emitted only when
the dtor is called.  The preferred usage of this class is to make use of
unnamed temporaries as follows:

info() << "The server started successfully.";

In the above usage, the temporary object will be destroyed at the end of the
statement and hence the log message will be emitted at the end of the
statement.  If a named object is created, then the log message will be emitted
only when it goes out of scope or destroyed. */
class info : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS

  /** Default constructor uses ER_IB_MSG_0 */
  info() : logger(INFORMATION_LEVEL) {}

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit info(int err, Args &&... args)
      : logger(INFORMATION_LEVEL, err, std::forward<Args>(args)...) {}
#else
  /** Destructor */
  ~info() override;
#endif /* !UNIV_NO_ERR_MSGS */
};

/** The class warn is used to emit warnings.  Refer to the documentation of
class info for further details. */
class warn : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0 */
  warn() : logger(WARNING_LEVEL) {}

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit warn(int err, Args &&... args)
      : logger(WARNING_LEVEL, err, std::forward<Args>(args)...) {}

#else
  /** Destructor */
  ~warn() override;
#endif /* !UNIV_NO_ERR_MSGS */
};

/** The class error is used to emit error messages.  Refer to the
documentation of class info for further details. */
class error : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0 */
  error() : logger(ERROR_LEVEL) {}

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit error(int err, Args &&... args)
      : logger(ERROR_LEVEL, err, std::forward<Args>(args)...) {}

#else
  /** Destructor */
  ~error() override;
#endif /* !UNIV_NO_ERR_MSGS */
};

/** The class fatal is used to emit an error message and stop the server
by crashing it.  Use this class when MySQL server needs to be stopped
immediately.  Refer to the documentation of class info for usage details. */
class fatal : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0 */
  fatal() : logger(ERROR_LEVEL) {}

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit fatal(int err, Args &&... args)
      : logger(ERROR_LEVEL, err, std::forward<Args>(args)...) {}

  /** Destructor. */
  ~fatal() override;
#else
  /** Destructor. */
  ~fatal() override;
#endif /* !UNIV_NO_ERR_MSGS */
};

/** Emit an error message if the given predicate is true, otherwise emit a
warning message */
class error_or_warn : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS

  /** Default constructor uses ER_IB_MSG_0
  @param[in]	pred		True if it's a warning. */
  error_or_warn(bool pred) : logger(pred ? ERROR_LEVEL : WARNING_LEVEL) {}

  /** Constructor.
  @param[in]	pred		True if it's a warning.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit error_or_warn(bool pred, int err, Args &&... args)
      : logger(pred ? ERROR_LEVEL : WARNING_LEVEL, err,
               std::forward<Args>(args)...) {}

#endif /* !UNIV_NO_ERR_MSGS */
};

/** Emit a fatal message if the given predicate is true, otherwise emit a
error message. */
class fatal_or_error : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0
  @param[in]	fatal		true if it's a fatal message */
  fatal_or_error(bool fatal) : logger(ERROR_LEVEL), m_fatal(fatal) {}

  /** Constructor.
  @param[in]	fatal		true if it's a fatal message
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit fatal_or_error(bool fatal, int err, Args &&... args)
      : logger(ERROR_LEVEL, err, std::forward<Args>(args)...), m_fatal(fatal) {}

  /** Destructor */
  ~fatal_or_error() override;
#else
  /** Constructor */
  fatal_or_error(bool fatal) : m_fatal(fatal) {}
#endif /* !UNIV_NO_ERR_MSGS */
 private:
  /** If true then assert after printing an error message. */
  const bool m_fatal;
};

#ifdef UNIV_HOTBACKUP
/**  The class trace is used to emit informational log messages. only when
trace level is set in the MEB code */
class trace_1 : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0 */
  trace_1() : logger(INFORMATION_LEVEL) { m_trace_level = 1; }

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit trace_1(int err, Args &&... args)
      : logger(INFORMATION_LEVEL, err, std::forward<Args>(args)...) {
    m_trace_level = 1;
  }

#else
  /** Constructor */
  trace_1();
#endif /* !UNIV_NO_ERR_MSGS */
};

/**  The class trace_2 is used to emit informational log messages only when
trace level 2 is set in the MEB code */
class trace_2 : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0 */
  trace_2() : logger(INFORMATION_LEVEL) { m_trace_level = 2; }

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit trace_2(int err, Args &&... args)
      : logger(INFORMATION_LEVEL, err, std::forward<Args>(args)...) {
    m_trace_level = 2;
  }
#else
  /** Destructor. */
  trace_2();
#endif /* !UNIV_NO_ERR_MSGS */
};

/**  The class trace_3 is used to emit informational log messages only when
trace level 3 is set in the MEB code */
class trace_3 : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS
  /** Default constructor uses ER_IB_MSG_0 */
  trace_3() : logger(INFORMATION_LEVEL) { m_trace_level = 3; }

  /** Constructor.
  @param[in]	err		Error code from errmsg-*.txt.
  @param[in]	args		Variable length argument list */
  template <class... Args>
  explicit trace_3(int err, Args &&... args)
      : logger(INFORMATION_LEVEL, err, std::forward<Args>(args)...) {
    m_trace_level = 3;
  }

#else
  /** Destructor. */
  trace_3();
#endif /* !UNIV_NO_ERR_MSGS */
};
#endif /* UNIV_HOTBACKUP */

/** For measuring time elapsed. Since std::chrono::high_resolution_clock
may be influenced by a change in system time, it might not be steady.
So we use std::chrono::steady_clock for ellapsed time. */
class Timer {
 public:
  using MS = std::chrono::milliseconds;
  using SC = std::chrono::steady_clock;

 public:
  /** Constructor. Starts/resets the timer to the current time. */
  Timer() { reset(); }

  /** Reset the timer to the current time. */
  void reset() { m_start = SC::now(); }

  /** @return the time elapsed in milliseconds. */
  int64_t elapsed() const {
    return (std::chrono::duration_cast<MS>(SC::now() - m_start).count());
  }

  /** Print time elapsed since last reset (in milliseconds) to the stream.
  @param[in,out] out  Stream to write to.
  @param[in] timer Timer to write to the stream.
  @return stream instance that was passed in. */
  template <typename T, typename Traits>
  friend std::basic_ostream<T, Traits> &operator<<(
      std::basic_ostream<T, Traits> &out, const Timer &timer) {
    return (out << timer.elapsed());
  }

 private:
  /** High resolution timer instance used for timimg. */
  SC::time_point m_start;
};

}  // namespace ib

#ifdef UNIV_HOTBACKUP
/** Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'.
@param[in]	buf	buffer where to sprintf */
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

#include "ut0ut.ic"

#endif /* !ut0ut_h */
