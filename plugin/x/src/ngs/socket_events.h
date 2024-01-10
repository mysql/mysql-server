/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_NGS_SOCKET_EVENTS_H_
#define PLUGIN_X_SRC_NGS_SOCKET_EVENTS_H_

#include <memory>
#include <vector>

#include "mysql/harness/net_ts.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/interface/socket_events.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace ngs {

class Socket_events : public xpl::iface::Socket_events {
 private:
  using Socket = net::ip::tcp::socket;

 public:
  Socket_events();
  ~Socket_events() override;

  bool listen(
      std::shared_ptr<xpl::iface::Socket> s,
      std::function<void(xpl::iface::Connection_acceptor &)> callback) override;

  void add_timer(const std::size_t delay_ms,
                 std::function<bool()> callback) override;
  void loop() override;
  void break_loop() override;

 private:
  class EntryTimer;
  class EntryAcceptingSocket;

  void callback_timeout(EntryTimer *timer_entry, std::error_code ec);
  void callback_accept_socket(EntryAcceptingSocket *acceptors_entry,
                              std::error_code ec);

  net::io_context m_io_context;
  std::vector<EntryAcceptingSocket *> m_socket_events;
  std::vector<EntryTimer *> m_timer_events;
  xpl::Mutex m_timers_mutex{KEY_mutex_x_socket_events_timers};
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_SOCKET_EVENTS_H_
