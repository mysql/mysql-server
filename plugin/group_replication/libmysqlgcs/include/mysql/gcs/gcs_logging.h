/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_LOGGING_INCLUDED
#define GCS_LOGGING_INCLUDED

#include <atomic>
#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

/**
  @class Common_interface

  Common interface that defines the sink, logger and debugger.
*/
class Common_interface {
 public:
  /**
    The purpose of this method is to force any implementing classes to define
    a destructor, since it will be used by Gcs_log_manager and Gcs_debug_manager
    in their finalize methods.
  */

  virtual ~Common_interface() {}

  /**
    The purpose of this method is to initialize any resources used in the
    logging and debugging system. It is invoked by the
    Gcs_log_manager::initialize and Gcs_debug_manager::initialize methods.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error initialize() = 0;

  /**
    The purpose of this method is to free any resources used in the logging
    and debugging system.

    It is invoked by the Gcs_log_manager::finalize and
    Gcs_debug_manager::finalize methods during the GCS interface termination
    procedure, and also by the Gcs_log_manager::initialize and
    Gcs_debug_manager::initialize methods in case a logging system has been
    previously set up.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error finalize() = 0;
};

/**
  @class Sink_interface

  Common sink that may be shared by the logging and debugging systems.

  A typical usage of this interface is the initialization and injection of a
  logging system:

  @code{.cpp}
    Sink_interface *sink= new MyGCS_Sink();

    Logger_interface *logger= new MyGCS_Logger(sink);
    group_if->set_logger(logger);
  @endcode
*/

class Sink_interface : public Common_interface {
 public:
  /**
    The purpose of this method is to force any implementing classes to define
    a destructor.
  */

  virtual ~Sink_interface() {}

  /**
    The purpose of this method is to effectively log the information.

    It should be invoked by the logger or debugger objects after the
    information to be logged has been pre-processed.

    @param[in] message  the message to log
  */

  virtual void log_event(const std::string &message) = 0;

  /**
    The purpose of this method is to effectively log the information.

    It should be invoked by the logger or debugger objects after the
    information to be logged has been pre-processed.

    @param[in] message the message to log
    @param[in] message_size message size to log
  */

  virtual void log_event(const char *message, size_t message_size) = 0;

  /**
    The purpose of this method is to return information on the sink such
    as its location.
  */

  virtual const std::string get_information() const = 0;
};

typedef enum {
  GCS_FATAL = 0,
  GCS_ERROR = 1,
  GCS_WARN = 2,
  GCS_INFO = 3,
} gcs_log_level_t;

static const char *const gcs_log_levels[] = {
    "[MYSQL_GCS_FATAL] ", "[MYSQL_GCS_ERROR] ", "[MYSQL_GCS_WARN] ",
    "[MYSQL_GCS_INFO] "};

/**
  @class Logger_interface

  Sink that must be implemented and used by any logging systems and inserted
  in the MySQL GCS logging infrastructure. The actual implementation can be
  shared between the debugging systems.

  A typical usage of this interface is the initialization and injection of a
  logging system:

  @code{.cpp}
    Logger_interface *logger= new MyGCS_Logger();
    group_if->set_logger(logger);
  @endcode

  Since the default logging system is initialized in the
  Gcs_interface::initialize method, this injection should be performed after
  that step. Otherwise, the injected logger will be finalized and replaced
  by the default logger.
*/

class Logger_interface : public Common_interface {
 public:
  /**
    The purpose of this method is to force any implementing classes to define
    a destructor, since it will be used by Gcs_log_manager in its finalize
    method.
  */

  virtual ~Logger_interface() {}

  /**
    The purpose of this method is to deliver to the logging system any event
    to be logged.

    It shouldn't be invoked directly in the code, as it is wrapped by the
    MYSQL_GCS_LOG_[LEVEL] macros which deal with the rendering of the logging
    message into a final string that is then handed alongside with the level to
    this method.

    @param[in] level  logging level of message
    @param[in] message  the message to log
  */

  virtual void log_event(const gcs_log_level_t level,
                         const std::string &message) = 0;
};

/**
  @class Gcs_log_manager

  This class sets up and configures the debugging infrastructure, storing the
  debugger to be used by the application as a singleton.
*/

class Gcs_log_manager {
 private:
  static Logger_interface *m_logger;

 public:
  /**
    The purpose of this static method is to set the received logging system on
    the log singleton, and to initialize it, by invoking its implementation of
    the Logger_interface::initialize method.

    This allows any resources needed by the logging system to be initialized,
    and ensures its usage throughout the lifecycle of the current GCS
    application.

    @param[in] logger logging system
    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  static enum_gcs_error initialize(Logger_interface *logger);

  /**
    This static method retrieves the currently set logging system, allowing the
    logging macros to invoke its log_event method.

    @return The current logging system singleton.
  */

  static Logger_interface *get_logger();

  /**
    The purpose of this static method is to free any resources used in the
    logging system.

    It is invoked by the Gcs_log_manager::finalize method during the GCS
    interface termination procedure, and also by the Gcs_log_manager::initialize
    method in case a logging system was set previously.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  static enum_gcs_error finalize();
};

/*
  These two definitions contain information on the valid debug options. If
  one wants to add a new option both definitions must be changed.

  Any new option must be added before the GCS_DEBUG_ALL and gaps in the
  numbers are not allowed. Adding a new debug option, e.g. GCS_DEBUG_THREAD,
  would result in the following definitions:

  typedef enum
  {
    GCS_DEBUG_NONE    = 0x00000000,
    GCS_DEBUG_BASIC   = 0x00000001,
    GCS_DEBUG_TRACE   = 0x00000002,
    XCOM_DEBUG_BASIC  = 0x00000004,
    XCOM_DEBUG_TRACE  = 0x00000008,
    GCS_INVALID_DEBUG = ~(0x7FFFFFFF),
    GCS_DEBUG_ALL     = ~(GCS_DEBUG_NONE)
  } gcs_xcom_debug_option_t;


  static const char* const gcs_xcom_debug_strings[]=
  {
    "GCS_DEBUG_BASIC",
    "GCS_DEBUG_TRACE",
    "XCOM_DEBUG_BASIC",
    "XCOM_DEBUG_TRACE",
    "GCS_DEBUG_ALL",
    "GCS_DEBUG_NONE"
  };
*/
#if !defined(GCS_XCOM_DEBUG_INFORMATION)
#define GCS_XCOM_DEBUG_INFORMATION
/*
This code is duplicated in the xcom_logger.h file. So if you make any change
here, remember to update the aforementioned file. Note that XCOM is written
in C which uses a 32 bit integer as the type for the enumeration and although
the code is prepared to handle 64 bits, the enumeration in both the GCS (i.e.
C++) and XCOM (i.e. C) is restricted to 32 bits.

Assuming that we are not using all bits available to define a debug level,
GCS_INVALID_DEBUG will give us the last possible entry that is not used.

The GCS_DEBUG_NONE, GCS_DEBUG_ALL and GCS_INVALID_DEBUG are options that apply
to both GCS and XCOM but we don't prefix it with XCOM to avoid big names.
*/
typedef enum {
  GCS_DEBUG_NONE = 0x00000000,
  GCS_DEBUG_BASIC = 0x00000001,
  GCS_DEBUG_TRACE = 0x00000002,
  XCOM_DEBUG_BASIC = 0x00000004,
  XCOM_DEBUG_TRACE = 0x00000008,
  GCS_INVALID_DEBUG = ~(0x7FFFFFFF),
  GCS_DEBUG_ALL = ~(GCS_DEBUG_NONE)
} gcs_xcom_debug_option_t;

static const char *const gcs_xcom_debug_strings[] = {
    "GCS_DEBUG_BASIC",  "GCS_DEBUG_TRACE", "XCOM_DEBUG_BASIC",
    "XCOM_DEBUG_TRACE", "GCS_DEBUG_ALL",   "GCS_DEBUG_NONE",
};

/*
  Assuming that we are not using all bits available to define a debug
  level, this will give us the last possible entry that is not used.
*/
#endif

/**
  @class Gcs_debug_manager

  This class sets up and configures the debugging infrastructure, storing the
  debugger to be used by the application as a singleton.
*/

class Gcs_debug_options {
 private:
  /*
    The debug level enabled which is by default GCS_DEBUG_NONE;
  */
  static std::atomic<std::int64_t> m_debug_options;

  /*
    String that represents the GCS_DEBUG_NONE;
  */
  static const std::string m_debug_none;

  /*
    String that represents the GCS_DEBUG_ALL;
  */
  static const std::string m_debug_all;

 public:
  /*
    Atomically load information on debug options.
  */

  static inline int64_t load_debug_options() {
    return m_debug_options.load(std::memory_order_relaxed);
  }

  /*
    Atomically store information on debug options.
  */

  static inline void store_debug_options(int64_t debug_options) {
    m_debug_options.store(debug_options, std::memory_order_relaxed);
  }

  /**
    Verify whether any of the debug options are defined.

    @param debug_options Set of debug options to be verified.
  */

  static inline bool test_debug_options(const int64_t debug_options) {
    return load_debug_options() & debug_options;
  }

  /**
    Get the current set of debug options.
  */

  static int64_t get_current_debug_options();

  /**
    Get the current set of debug options as a string separated by comma.

    Although, a boolean value is returned in this case it will always
    return false.

    @param[out] res_debug_options String containing the result
  */

  static int64_t get_current_debug_options(std::string &res_debug_options);

  /**
     Get the set of valid debug options excluding GCS_DEBUG_NONE and
    GCS_DEBUG_ALL.
  */

  static int64_t get_valid_debug_options();

  /**
    Check whether the set of debug options is valid or not including
    GCS_DEBUG_NONE and GCS_DEBUG_ALL.

    @param[in] debug_options Set of debug options
  */

  static bool is_valid_debug_options(const int64_t debug_options);

  /**
    Check whether the set of debug options is valid or not including
    GCS_DEBUG_NONE and GCS_DEBUG_ALL.

    @param[in] debug_options Set of debug options
  */

  static bool is_valid_debug_options(const std::string &debug_options);

  /**
    Get the set of debug options passed as paramter as an unsigned integer.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options
    @param[out] res_debug_options Unsigned integer that contains the result
  */

  static bool get_debug_options(const std::string &debug_options,
                                int64_t &res_debug_options);

  /**
    Get the set of debug options passed as paramter as a string.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options
    @param[out] res_debug_options String that contains the result
  */
  static bool get_debug_options(const int64_t debug_options,
                                std::string &res_debug_options);

  /**
    Get the the number of possible debug options.
  */

  static unsigned int get_number_debug_options();

  /**
    Extend the current set of debug options with new debug options expressed as
    an unsigned integer parameter.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options to be added
  */

  static bool set_debug_options(const int64_t debug_options);

  /**
    Change the current set of debug options by the new debug options expressed
    as an unsigned integer parameter.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options to be added
  */

  static bool force_debug_options(const int64_t debug_options);

  /**
    Extend the current set of debug options with new debug options expressed
    as a string.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options to be added
  */

  static bool set_debug_options(const std::string &debug_options);

  /**
    Changed the current set of debug options by the new debug options expressed
    as a string.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options to be added
  */

  static bool force_debug_options(const std::string &debug_options);

  /**
    Reduce the current set of debug options by disabling the debug options
    expressed as an unsigned integer parameter.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options to be disabled
  */

  static bool unset_debug_options(const int64_t debug_options);

  /**
    Reduce the current set of debug options by disabling the debug options
    expressed as a string.

    If there is any invalid debug option in the debug_options parameter, true
    is returned.

    @param[in] debug_options Set of debug options to be disabled
  */

  static bool unset_debug_options(const std::string &debug_options);
};
#endif /* GCS_LOGGING_INCLUDED */
