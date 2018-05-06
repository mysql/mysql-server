/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_COMPONENT_H
#define NDB_COMPONENT_H

#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_thread.h"

extern "C" void * Ndb_component_run_C(void *);

/**
 * Baseclass encapsulating the different components
 * in ndbcluster.
 *
 * NOTE! The intention should be to not correlate to number of
 * threads since that is an implementation detail in each
 * component.
 */

class Ndb_component
{
public:
  virtual int init();
  virtual int start();
  virtual int stop();
  virtual int deinit();

  /*
    Set the server as started, this means that the Ndb_component
    can continue processing and use parts of the MySQL Server which are
    not available unilt it's been fully started
  */
  void set_server_started();

protected:
  /**
   * Con/de-structor is protected...so that sub-class needs to provide own
   */
  Ndb_component(const char* name);
  virtual ~Ndb_component();

  /**
   * Component init function
   */
  virtual int do_init() = 0;

  /**
   * Component run function
   */
  virtual void do_run() = 0;

  /**
   * Component deinit function
   */
  virtual int do_deinit() = 0;

  /**
   * Component wakeup function
   * - called when component is set to stop, should
   *   wakeup component from waiting
   */
  virtual void do_wakeup() = 0;

  /**
   * For usage in threads main loop
   */
  bool is_stop_requested();

protected:
  void log_verbose(unsigned verbose_level, const char* fmt, ...)
    const MY_ATTRIBUTE((format(printf, 3, 4)));
  void log_error(const char *fmt, ...) const
    MY_ATTRIBUTE((format(printf, 2, 3)));
  void log_warning(const char *fmt, ...) const
    MY_ATTRIBUTE((format(printf, 2, 3)));
  void log_info(const char *fmt, ...) const
    MY_ATTRIBUTE((format(printf, 2, 3)));

  /*
    Wait for the server started. The Ndb_component(and its thread(s))
    are normally started before the MySQL Server is fully operational
    and some functionality which the Ndb_component depend on isn't
    yet initialized fully. This function will wait until the server
    has reported started or shutdown has been requested.
   */
  bool wait_for_server_started(void);

private:

  enum ThreadState
  {
    TS_UNINIT   = 0,
    TS_INIT     = 1,
    TS_STARTING = 2,
    TS_RUNNING  = 3,
    TS_STOPPING = 4,
    TS_STOPPED  = 5
  };

  ThreadState m_thread_state;
  my_thread_handle m_thread;
  mysql_mutex_t m_start_stop_mutex;
  mysql_cond_t m_start_stop_cond;
  bool m_server_started; // Protected by m_start_stop_mutex

  const char* m_name;

  void run_impl();
  friend void * Ndb_component_run_C(void *);
};

#endif
