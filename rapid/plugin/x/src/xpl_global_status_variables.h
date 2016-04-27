/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _XPL_GLOBAL_STATUS_VARIABLES_H_
#define _XPL_GLOBAL_STATUS_VARIABLES_H_

#include "xpl_common_status_variables.h"


namespace xpl
{


class Server;


class Global_status_variables : public Common_status_variables
{
public:
  static Global_status_variables &instance()
  {
    static Global_status_variables singleton;

    return singleton;
  }


  void reset()
  {
    Common_status_variables::reset();

    my_atomic_store64(&m_sessions_count, 0);
    my_atomic_store64(&m_worker_thread_count, 0);
    my_atomic_store64(&m_active_worker_thread_count, 0);
    my_atomic_store64(&m_closed_sessions_count, 0);
    my_atomic_store64(&m_sessions_fatal_errors_count, 0);
    my_atomic_store64(&m_init_errors_count, 0);
    my_atomic_store64(&m_accepted_connections_count, 0);
    my_atomic_store64(&m_closed_connections_count, 0);
    my_atomic_store64(&m_connection_errors_count, 0);
    my_atomic_store64(&m_connection_reject_count, 0);
    my_atomic_store64(&m_connection_accept_errors_count, 0);
    my_atomic_store64(&m_accepted_sessions_count, 0);
    my_atomic_store64(&m_rejected_sessions_count, 0);
    my_atomic_store64(&m_killed_sessions_count, 0);
  }


  void increment_sessions_count()
  {
    my_atomic_add64(&m_sessions_count, 1);
  }


  void decrement_sessions_count()
  {
    my_atomic_add64(&m_sessions_count, -1);
  }


  long long get_sessions_count()
  {
    return my_atomic_load64(&m_sessions_count);
  }


  void increment_worker_thread_count()
  {
    my_atomic_add64(&m_worker_thread_count, 1);
  }


  void decrement_worker_thread_count()
  {
    my_atomic_add64(&m_worker_thread_count, -1);
  }


  long long get_worker_thread_count()
  {
    return my_atomic_load64(&m_worker_thread_count);
  }


  void increment_active_worker_thread_count()
  {
    my_atomic_add64(&m_active_worker_thread_count, 1);
  }


  void decrement_active_worker_thread_count()
  {
    my_atomic_add64(&m_active_worker_thread_count, -1);
  }


  long long get_active_worker_thread_count()
  {
    return my_atomic_load64(&m_active_worker_thread_count);
  }


  void increment_closed_sessions_count()
  {
    my_atomic_add64(&m_closed_sessions_count, 1);
  }


  long long get_closed_sessions_count()
  {
    return my_atomic_load64(&m_closed_sessions_count);
  }


  void increment_sessions_fatal_errors_count()
  {
    my_atomic_add64(&m_sessions_fatal_errors_count, 1);
  }


  long long get_sessions_fatal_errors_count()
  {
    return my_atomic_load64(&m_sessions_fatal_errors_count);
  }


  void increment_init_errors_count()
  {
    my_atomic_add64(&m_init_errors_count, 1);
  }


  long long get_init_errors_count()
  {
    return my_atomic_load64(&m_init_errors_count);
  }


  void increment_closed_connections_count()
  {
    my_atomic_add64(&m_closed_connections_count, 1);
  }


  long long get_closed_connections_count()
  {
    return my_atomic_load64(&m_closed_connections_count);
  }


  void increment_accepted_connections_count()
  {
    my_atomic_add64(&m_accepted_connections_count, 1);
  }


  long long get_accepted_connections_count()
  {
    return my_atomic_load64(&m_accepted_connections_count);
  }


  void increment_connection_errors_count()
  {
    my_atomic_add64(&m_connection_errors_count, 1);
  }


  long long get_connection_errors_count()
  {
    return my_atomic_load64(&m_connection_errors_count);
  }


  void increment_connection_accept_errors_count()
  {
    my_atomic_add64(&m_connection_accept_errors_count, 1);
  }


  long long get_connection_accept_errors_count()
  {
    return my_atomic_load64(&m_connection_accept_errors_count);
  }


  void increment_connection_reject_count()
  {
    my_atomic_add64(&m_connection_reject_count, 1);
  }


  long long get_rejected_connections_count()
  {
    return my_atomic_load64(&m_connection_reject_count);
  }


  void increment_accepted_sessions_count()
  {
    my_atomic_add64(&m_accepted_sessions_count, 1);
  }


  long long get_accepted_sessions_count()
  {
    return my_atomic_load64(&m_accepted_sessions_count);
  }


  void increment_rejected_sessions_count()
  {
    my_atomic_add64(&m_rejected_sessions_count, 1);
  }


  long long get_rejected_sessions_count()
  {
    return my_atomic_load64(&m_rejected_sessions_count);
  }


  void increment_killed_sessions_count()
  {
    my_atomic_add64(&m_killed_sessions_count, 1);
  }


  long long get_killed_sessions_count()
  {
    return my_atomic_load64(&m_killed_sessions_count);
  }


private:
  Global_status_variables()
  {
    reset();
  }


  Global_status_variables(const Global_status_variables &);
  Global_status_variables &operator=(const Global_status_variables &);

  volatile int64 m_sessions_count;
  volatile int64 m_worker_thread_count;
  volatile int64 m_active_worker_thread_count;
  volatile int64 m_closed_sessions_count;
  volatile int64 m_sessions_fatal_errors_count;
  volatile int64 m_init_errors_count;
  volatile int64 m_accepted_connections_count;
  volatile int64 m_closed_connections_count;
  volatile int64 m_connection_errors_count;
  volatile int64 m_connection_reject_count;
  volatile int64 m_connection_accept_errors_count;
  volatile int64 m_accepted_sessions_count;
  volatile int64 m_rejected_sessions_count;
  volatile int64 m_killed_sessions_count;
};


} // namespace xpl


#endif // _XPL_GLOBAL_STATUS_VARIABLES_H_
