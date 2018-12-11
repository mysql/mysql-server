/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef NGS_SOCKET_EVENTS_H_
#define NGS_SOCKET_EVENTS_H_

#include <vector>
#include "ngs/socket_events_interface.h"

struct event_base;

namespace ngs {

class Socket_events: public Socket_events_interface {
public:
#ifdef _WIN32
  // mimick evutil_socket_t in libevent-2.x
  typedef intptr_t socket_type;
#else
  typedef int socket_type;
#endif
  Socket_events();
  ~Socket_events();

  bool listen(Socket_interface::Shared_ptr s, ngs::function<void (Connection_acceptor_interface &)> callback);

  void add_timer(const std::size_t delay_ms, ngs::function<bool ()> callback);
  void loop();
  void break_loop();

private:
  static void timeout_call(socket_type sock, short which, void *arg);
  static void socket_data_avaiable(socket_type sock, short which, void *arg);

  struct Timer_data;
  struct Socket_data;
  struct event_base *m_evbase;
  std::vector<Socket_data *> m_socket_events;
  std::vector<Timer_data*> m_timer_events;
  Mutex m_timers_mutex;
};

} // namespace ngs

#endif // NGS_SOCKET_EVENTS_H_
