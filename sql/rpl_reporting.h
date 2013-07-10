/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef RPL_REPORTING_H
#define RPL_REPORTING_H

#include "my_sys.h"                             /* loglevel */

/**
   Maximum size of an error message from a slave thread.
 */
#define MAX_SLAVE_ERRMSG      1024

// todo: consider to remove rpl_reporting.cc,h from building embedded
#if !defined(EMBEDDED_LIBRARY)
class THD;
#endif

/**
   Mix-in to handle the message logging and reporting for relay log
   info and master log info structures.

   By inheriting from this class, the class is imbued with
   capabilities to do slave reporting.
 */
class Slave_reporting_capability
{
public:
  /** lock used to synchronize m_last_error on 'SHOW SLAVE STATUS' **/
  mutable mysql_mutex_t err_lock;
  /**
     Constructor.

     @param thread_name Printable name of the slave thread that is reporting.
   */
  Slave_reporting_capability(char const *thread_name);

  /**
     Writes a message and, if it's an error message, to Last_Error
     (which will be displayed by SHOW SLAVE STATUS).

     @param level       The severity level
     @param err_code    The error code
     @param msg         The message (usually related to the error
                        code, but can contain more information), in
                        printf() format.
  */
  virtual void report(loglevel level, int err_code, const char *msg, ...) const
    ATTRIBUTE_FORMAT(printf, 4, 5);
  void va_report(loglevel level, int err_code, const char *prefix_msg,
                 const char *msg, va_list v_args) const;

  /**
     Clear errors. They will not show up under <code>SHOW SLAVE
     STATUS</code>.
   */
  void clear_error() {
    mysql_mutex_lock(&err_lock);
    m_last_error.clear();
    mysql_mutex_unlock(&err_lock);
  }

#if !defined(EMBEDDED_LIBRARY)
  /**
     Check if the current error is of temporary nature or not.
  */
  int has_temporary_error(THD *thd, uint error_arg= 0, bool* silent= 0) const;
#endif // EMBEDDED_LIBRARY
  
  /**
     Error information structure.
   */
  class Error {
    friend class Slave_reporting_capability;
  public:
    Error()
    {
      clear();
    }

    void clear()
    {
      number= 0;
      message[0]= '\0';
      timestamp[0]= '\0';

    }

    void update_timestamp()
    {
      time_t skr;
      struct tm tm_tmp;
      struct tm *start;

      skr= my_time(0);
      localtime_r(&skr, &tm_tmp);
      start=&tm_tmp;

      sprintf(timestamp, "%02d%02d%02d %02d:%02d:%02d",
              start->tm_year % 100,
              start->tm_mon+1,
              start->tm_mday,
              start->tm_hour,
              start->tm_min,
              start->tm_sec);
      timestamp[15]= '\0';
    }

    /** Error code */
    uint32 number;
    /** Error message */
    char message[MAX_SLAVE_ERRMSG];
    /** Error timestamp as string */
    char timestamp[16];
  };

  Error const& last_error() const { return m_last_error; }
  bool is_error() const { return last_error().number != 0; }

  virtual ~Slave_reporting_capability()= 0;

protected:

  virtual void do_report(loglevel level, int err_code,
                 const char *msg, va_list v_args) const
  {
    va_report(level, err_code, NULL, msg, v_args);
  }

private:
  /**
     Last error produced by the I/O or SQL thread respectively.
   */
  mutable Error m_last_error;

  char const *const m_thread_name;

  // not implemented
  Slave_reporting_capability(const Slave_reporting_capability& rhs);
  Slave_reporting_capability& operator=(const Slave_reporting_capability& rhs);
};

#endif // RPL_REPORTING_H

