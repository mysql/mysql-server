/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef NGS_SOCKET_EVENTS_H_
#define NGS_SOCKET_EVENTS_H_

#include <vector>

#include "plugin/x/ngs/include/ngs/interface/socket_events_interface.h"

struct event_base;

namespace ngs {

class Socket_events: public Socket_events_interface {
public:
  Socket_events();
  ~Socket_events();

  bool listen(Socket_interface::Shared_ptr s, ngs::function<void (Connection_acceptor_interface &)> callback);

  void add_timer(const std::size_t delay_ms, ngs::function<bool ()> callback);
  void loop();
  void break_loop();

private:
  static void timeout_call(int sock, short which, void *arg);
  static void socket_data_avaiable(int sock, short which, void *arg);

  struct Timer_data;
  struct Socket_data;
  struct event_base *m_evbase;
  std::vector<Socket_data *> m_socket_events;
  std::vector<Timer_data*> m_timer_events;
  Mutex m_timers_mutex;
};

} // namespace ngs

#endif // NGS_SOCKET_EVENTS_H_
