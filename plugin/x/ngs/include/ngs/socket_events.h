/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_SOCKET_EVENTS_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_SOCKET_EVENTS_H_

#include <vector>

#include "plugin/x/ngs/include/ngs/interface/socket_events_interface.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/xpl_performance_schema.h"

struct event_base;

namespace ngs {

class Socket_events : public Socket_events_interface {
 public:
#ifdef _WIN32
  // mimick evutil_socket_t in libevent-2.x
  using socket_type = intptr_t;
#else
  using socket_type = int;
#endif
  Socket_events();
  ~Socket_events();

  bool listen(Socket_interface::Shared_ptr s,
              std::function<void(Connection_acceptor_interface &)> callback);

  void add_timer(const std::size_t delay_ms, std::function<bool()> callback);
  void loop();
  void break_loop();

 private:
  static void timeout_call(socket_type sock, short which, void *arg);
  static void socket_data_avaiable(socket_type sock, short which, void *arg);

  struct Timer_data;
  struct Socket_data;
  struct event_base *m_evbase;
  std::vector<Socket_data *> m_socket_events;
  std::vector<Timer_data *> m_timer_events;
  xpl::Mutex m_timers_mutex{KEY_mutex_x_socket_events_timers};
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_SOCKET_EVENTS_H_
