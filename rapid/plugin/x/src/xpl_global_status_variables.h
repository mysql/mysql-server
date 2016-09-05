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

    m_sessions_count.store(0);
    m_worker_thread_count.store(0);
    m_active_worker_thread_count.store(0);
    m_closed_sessions_count.store(0);
    m_sessions_fatal_errors_count.store(0);
    m_init_errors_count.store(0);
    m_accepted_connections_count.store(0);
    m_closed_connections_count.store(0);
    m_connection_errors_count.store(0);
    m_connection_reject_count.store(0);
    m_connection_accept_errors_count.store(0);
    m_accepted_sessions_count.store(0);
    m_rejected_sessions_count.store(0);
    m_killed_sessions_count.store(0);
  }


  void increment_sessions_count()
  {
    ++m_sessions_count;
  }


  void decrement_sessions_count()
  {
    --m_sessions_count;
  }


  long long get_sessions_count()
  {
    return m_sessions_count.load();
  }


  void increment_worker_thread_count()
  {
    ++m_worker_thread_count;
  }


  void decrement_worker_thread_count()
  {
    --m_worker_thread_count;
  }


  long long get_worker_thread_count()
  {
    return m_worker_thread_count.load();
  }


  void increment_active_worker_thread_count()
  {
    ++m_active_worker_thread_count;
  }


  void decrement_active_worker_thread_count()
  {
    --m_active_worker_thread_count;
  }


  long long get_active_worker_thread_count()
  {
    return m_active_worker_thread_count.load();
  }


  void increment_closed_sessions_count()
  {
    ++m_closed_sessions_count;
  }


  long long get_closed_sessions_count()
  {
    return m_closed_sessions_count.load();
  }


  void increment_sessions_fatal_errors_count()
  {
    ++m_sessions_fatal_errors_count;
  }


  long long get_sessions_fatal_errors_count()
  {
    return m_sessions_fatal_errors_count.load();
  }


  void increment_init_errors_count()
  {
    ++m_init_errors_count;
  }


  long long get_init_errors_count()
  {
    return m_init_errors_count.load();
  }


  void increment_closed_connections_count()
  {
    ++m_closed_connections_count;
  }


  long long get_closed_connections_count()
  {
    return m_closed_connections_count.load();
  }


  void increment_accepted_connections_count()
  {
    ++m_accepted_connections_count;
  }


  long long get_accepted_connections_count()
  {
    return m_accepted_connections_count.load();
  }


  void increment_connection_errors_count()
  {
    ++m_connection_errors_count;
  }


  long long get_connection_errors_count()
  {
    return m_connection_errors_count.load();
  }


  void increment_connection_accept_errors_count()
  {
    ++m_connection_accept_errors_count;
  }


  long long get_connection_accept_errors_count()
  {
    return m_connection_accept_errors_count.load();
  }


  void increment_connection_reject_count()
  {
    ++m_connection_reject_count;
  }


  long long get_rejected_connections_count()
  {
    return m_connection_reject_count.load();
  }


  void increment_accepted_sessions_count()
  {
    ++m_accepted_sessions_count;
  }


  long long get_accepted_sessions_count()
  {
    return m_accepted_sessions_count.load();
  }


  void increment_rejected_sessions_count()
  {
    ++m_rejected_sessions_count;
  }


  long long get_rejected_sessions_count()
  {
    return m_rejected_sessions_count.load();
  }


  void increment_killed_sessions_count()
  {
    ++m_killed_sessions_count;
  }


  long long get_killed_sessions_count()
  {
    return m_killed_sessions_count.load();
  }


private:
  Global_status_variables()
  {
    reset();
  }


  Global_status_variables(const Global_status_variables &);
  Global_status_variables &operator=(const Global_status_variables &);

  volatile ngs::atomic<int64> m_sessions_count;
  volatile ngs::atomic<int64> m_worker_thread_count;
  volatile ngs::atomic<int64> m_active_worker_thread_count;
  volatile ngs::atomic<int64> m_closed_sessions_count;
  volatile ngs::atomic<int64> m_sessions_fatal_errors_count;
  volatile ngs::atomic<int64> m_init_errors_count;
  volatile ngs::atomic<int64> m_accepted_connections_count;
  volatile ngs::atomic<int64> m_closed_connections_count;
  volatile ngs::atomic<int64> m_connection_errors_count;
  volatile ngs::atomic<int64> m_connection_reject_count;
  volatile ngs::atomic<int64> m_connection_accept_errors_count;
  volatile ngs::atomic<int64> m_accepted_sessions_count;
  volatile ngs::atomic<int64> m_rejected_sessions_count;
  volatile ngs::atomic<int64> m_killed_sessions_count;
};


} // namespace xpl


#endif // _XPL_GLOBAL_STATUS_VARIABLES_H_
