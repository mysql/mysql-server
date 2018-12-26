/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_LOGGING_INCLUDED
#define	GCS_LOGGING_INCLUDED

#include <sstream>
#include "gcs_types.h"

typedef enum
{
  GCS_TRACE= 5,
  GCS_DEBUG= 4,
  GCS_INFO= 3,
  GCS_WARN= 2,
  GCS_ERROR= 1,
  GCS_FATAL= 0
} gcs_log_level_t;


static const char* const gcs_log_levels[]=
{
  "[MYSQL_GCS_FATAL] ",
  "[MYSQL_GCS_ERROR] ",
  "[MYSQL_GCS_WARN] ",
  "[MYSQL_GCS_INFO] ",
  "[MYSQL_GCS_DEBUG] ",
  "[MYSQL_GCS_TRACE] "
};


/**
  @interface Ext_logger_interface

  This interface must be implemented by all the logging systems to be inserted
  in the MySQL GCS logging infrastructure.

  A typical usage of this interface is the initialization and injection of a
  logging system:

  @code{.cpp}
    Ext_logger_interface *logger= new My_GCS_Ext_logger_interface();
    group_if->set_logger(logger);
  @endcode

  Since the default logging system is initialized in the
  Gcs_interface::initialize method, this injection should be performed after
  that step. Otherwise, the injected logging system will be finalized and
  replaced by the default logger.
*/

class Ext_logger_interface
{
public:
  /**
    The purpose of this method is to force any implementing classes to define
    a destructor, since it will be used by Gcs_logger in its finalize method.
  */

  virtual ~Ext_logger_interface() {}


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

  virtual void log_event(gcs_log_level_t level, const char *message)= 0;


  /**
    The purpose of this method is to initialize any resources used in the
    logging system.
    It is invoked by the Gcs_logger::initialize method.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error initialize()= 0;


  /**
    The purpose of this method is to free any resources used in the
    logging system.
    It is invoked by the Gcs_logger::finalize method during the GCS interface
    termination procedure, and also by the Gcs_logger::initialize method in
    case a logging system was set previously.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error finalize()= 0;
};


/**
  @class Gcs_logger

  This class implements the logging infrastructure, storing the logging system
  to be used by the application as a singleton.
*/

class Gcs_logger
{
private:
  static Ext_logger_interface *log;

public:
  /**
    The purpose of this static method is to set the received logging system on
    the log singleton, and to initialize it, by invoking its implementation of
    the Ext_logger_interface::initialize method.
    This allows any resources needed by the logging system to be initialized,
    and ensures its usage throughout the lifecycle of the current GCS
    application.

    @param[in] logger logging system
    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  static enum_gcs_error initialize(Ext_logger_interface *logger);


  /**
    This static method retrieves the currently set logging system, allowing the
    logging macros to invoke its log_event method.

    @return The current logging system singleton.
  */

  static Ext_logger_interface *get_logger();


  /**
    The purpose of this static method is to free any resources used in the
    logging system.
    It is invoked by the Gcs_logger::finalize method during the GCS interface
    termination procedure, and also by the Gcs_logger::initialize method in
    case a logging system was set previously.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  static enum_gcs_error finalize();
};


// Logging macros

#define GCS_LOG_PREFIX "[GCS] "

#define MYSQL_GCS_LOG(l,x)                      \
  do \
  { \
      std::ostringstream temp; \
      temp << GCS_LOG_PREFIX; \
      temp << x; \
      Gcs_logger::get_logger()->log_event(l, temp.str().c_str()); \
  } \
  while (0); \

#ifdef WITH_LOG_TRACE
#define MYSQL_GCS_LOG_TRACE(x) MYSQL_GCS_LOG(GCS_TRACE, x)
#else
#define MYSQL_GCS_LOG_TRACE(x)
#endif

#ifdef WITH_LOG_DEBUG
#define MYSQL_GCS_DEBUG_EXECUTE(x) x
#define MYSQL_GCS_LOG_DEBUG(x) MYSQL_GCS_LOG(GCS_DEBUG, x)
#else
#define MYSQL_GCS_DEBUG_EXECUTE(x) do { } while (0);
#define MYSQL_GCS_LOG_DEBUG(x)
#endif

#define MYSQL_GCS_LOG_INFO(x) MYSQL_GCS_LOG(GCS_INFO, x)
#define MYSQL_GCS_LOG_WARN(x) MYSQL_GCS_LOG(GCS_WARN, x)
#define MYSQL_GCS_LOG_ERROR(x) MYSQL_GCS_LOG(GCS_ERROR, x)
#define MYSQL_GCS_LOG_FATAL(x) MYSQL_GCS_LOG(GCS_FATAL, x)

#endif	/* GCS_LOGGING_INCLUDED */
