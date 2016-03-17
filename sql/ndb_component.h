/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_COMPONENT_H
#define NDB_COMPONENT_H

#include <my_global.h>
#include <thr_cond.h>
#include <thr_mutex.h>

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
    MY_ATTRIBUTE((format(printf, 3, 4)));
  void log_error(const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));
  void log_warning(const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));
  void log_info(const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));

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
  native_mutex_t m_start_stop_mutex;
  native_cond_t m_start_stop_cond;

  const char* m_name;

  void run_impl();
  friend void * Ndb_component_run_C(void *);
};

#endif
