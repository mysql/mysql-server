/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HA_NDBCLUSTER_COMPONENT_H
#define HA_NDBCLUSTER_COMPONENT_H

#include <my_global.h>
#include <my_pthread.h>

extern "C" void * Ndb_component_run_C(void *);

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
  Ndb_component();
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
   * For usage in threads main loop
   */
  bool is_stop_requested();

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
  pthread_t m_thread;
  pthread_mutex_t m_start_stop_mutex;
  pthread_cond_t m_start_stop_cond;

  void run_impl();
  friend void * Ndb_component_run_C(void *);
};

#endif
