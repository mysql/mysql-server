/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/** @file include/ut0log.h Logging facilities. */

#ifndef ut0log_h
#define ut0log_h

#include "my_loglevel.h"
#include "mysql/components/services/log_shared.h"
#include "ut0core.h"

/** Get the format string for the logger.
@param[in]      errcode         The error code from share/errmsg-*.txt
@return the message string or nullptr */
const char *srv_get_server_errmsgs(int errcode);

namespace ib {

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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  @param[in]    buf             the buffer contents to log.
  @param[in]    count           the length of the buffer buf.
  @return the output stream into which buffer was written. */
  std::ostream &write(const char *buf, std::streamsize count) {
    return (m_oss.write(buf, count));
  }

  /** Write the given buffer to the internal string stream object.
  @param[in]    buf             the buffer contents to log
  @param[in]    count           the length of the buffer buf.
  @return the output stream into which buffer was written. */
  std::ostream &write(const unsigned char *buf, std::streamsize count) {
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
  @param[in]    err     Error code from errmsg-*.txt.
  @param[in]    args    Variable length argument list */
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
  @param[in]    level           Logging level
  @param[in]    err             Error message code. */
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
  @param[in]    level           Logging level
  @param[in]    err             Error message code.
  @param[in]    args            Variable length argument list */
  template <class... Args>
  explicit logger(loglevel level, int err, Args &&... args)
      : m_err(err), m_level(level) {
    m_oss << msg(err, std::forward<Args>(args)...);
  }

  /** Constructor
  @param[in]    level           Log error level */
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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  /** Default constructor uses ER_IB_MSG_0
  @param[in]    location                Location that creates the fatal message.
*/
  fatal(ut::Location location) : logger(ERROR_LEVEL), m_location(location) {}

  /** Constructor.
  @param[in]    location                Location that creates the fatal message.
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
  template <class... Args>
  explicit fatal(ut::Location location, int err, Args &&... args)
      : logger(ERROR_LEVEL, err, std::forward<Args>(args)...),
        m_location(location) {}
#else
  /** Constructor
  @param[in]    location                Location that creates the fatal message.
  */
  fatal(ut::Location location) : m_location(location) {}
#endif /* !UNIV_NO_ERR_MSGS */

  /** Destructor. */
  ~fatal() override;

 private:
  /** Location of the original caller to report to assertion failure */
  ut::Location m_location;
};

/** Emit an error message if the given predicate is true, otherwise emit a
warning message */
class error_or_warn : public logger {
 public:
#ifndef UNIV_NO_ERR_MSGS

  /** Default constructor uses ER_IB_MSG_0
  @param[in]    pred            True if it's a warning. */
  error_or_warn(bool pred) : logger(pred ? ERROR_LEVEL : WARNING_LEVEL) {}

  /** Constructor.
  @param[in]    pred            True if it's a warning.
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  @param[in]    fatal           true if it's a fatal message
  @param[in] location Location that creates the fatal */
  fatal_or_error(bool fatal, ut::Location location)
      : logger(ERROR_LEVEL), m_fatal(fatal), m_location(location) {}

  /** Constructor.
  @param[in]    fatal           true if it's a fatal message
  @param[in] location Location that creates the fatal
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
  template <class... Args>
  explicit fatal_or_error(bool fatal, ut::Location location, int err,
                          Args &&... args)
      : logger(ERROR_LEVEL, err, std::forward<Args>(args)...),
        m_fatal(fatal),
        m_location(location) {}

  /** Destructor */
  ~fatal_or_error() override;
#else
  /** Constructor
  @param[in] location Location that creates the fatal */
  fatal_or_error(bool fatal, ut::Location location)
      : m_fatal(fatal), m_location(location) {}

  /** Destructor */
  ~fatal_or_error() override;

#endif /* !UNIV_NO_ERR_MSGS */
 private:
  /** If true then assert after printing an error message. */
  const bool m_fatal;
  /** Location of the original caller to report to assertion failure */
  ut::Location m_location;
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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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
  @param[in]    err             Error code from errmsg-*.txt.
  @param[in]    args            Variable length argument list */
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

/* Convenience functions that ease the usage of logging facilities throughout
   the code.

   Logging facilities are designed such so that they differentiate between the
   case when UNIV_NO_ERR_MSGS is defined and when it is not. In particular, end
   user code must take into account when code is built with  UNIV_NO_ERR_MSGS
   because not the same set of ib::logger constructors will be available in such
   setting. Design of the logging facility therefore imposes that every possible
   usage of it in the end user code will result with sprinkling the #ifdefs all
   around.

   So, what these convenience wrappers do is that they provide somewhat better
   alternative to the following code, which without the wrapper look like:
     #ifdef UNIV_NO_ERR_MSGS
       ib::info();
     #else
       ib::info(ER_IB_MSG_1158);
     #endif
         << "Some message";

   Same applies for any other ib:: logging facility, e.g.:
     #ifdef UNIV_NO_ERR_MSGS
       ib::fatal(UT_LOCATION_HERE)
     #else
       ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_1157)
     #endif
         << "Some message";

   With the convenience wrapper these two usages become:
     log_info(ER_IB_MSG_1158) << "Some message";
     log_fatal(UT_LOCATION_HERE, ER_IB_MSG_1157) << "Some message";
*/

static inline auto log_info() { return ib::info(); }
static inline auto log_warn() { return ib::warn(); }
static inline auto log_error() { return ib::error(); }
static inline auto log_fatal(ut::Location location) {
  return ib::fatal(location);
}
static inline auto log_error_or_warn(bool pred) {
#ifdef UNIV_NO_ERR_MSGS
  return ib::error_or_warn();
#else
  return ib::error_or_warn(pred);
#endif
}
static inline auto log_fatal_or_error(bool fatal, ut::Location location) {
  return ib::fatal_or_error(fatal, location);
}

template <typename... Args>
static inline auto log_info(int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_info();
#else
  return ib::info(err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_warn(int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_warn();
#else
  return ib::warn(err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_error(int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_error();
#else
  return ib::error(err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_fatal(ut::Location location, int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_fatal(location);
#else
  return ib::fatal(location, err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_error_or_warn(bool pred, int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_error_or_warn(pred);
#else
  return ib::error_or_warn(pred, err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_fatal_or_error(bool fatal, ut::Location location,
                                      int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_fatal_or_error(fatal, location);
#else
  return ib::fatal_or_error(fatal, location, err, std::forward<Args>(args)...);
#endif
}

#ifdef UNIV_HOTBACKUP
static inline auto log_trace_1() { return ib::trace_1(); }
static inline auto log_trace_2() { return ib::trace_2(); }
static inline auto log_trace_3() { return ib::trace_3(); }

template <typename... Args>
static inline auto log_trace_1(int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_trace_1();
#else
  return ib::trace_1(err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_trace_2(int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_trace_2();
#else
  return ib::trace_2(err, std::forward<Args>(args)...);
#endif
}
template <typename... Args>
static inline auto log_trace_3(int err, Args &&... args) {
#ifdef UNIV_NO_ERR_MSGS
  return log_trace_3();
#else
  return ib::trace_3(err, std::forward<Args>(args)...);
#endif
}
#endif /* UNIV_HOTBACKUP */

}  // namespace ib

#endif
