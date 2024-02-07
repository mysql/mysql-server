/*
 * Copyright (c) 2016, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_IO_XPL_LISTENER_TCP_H_
#define PLUGIN_X_SRC_IO_XPL_LISTENER_TCP_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/listener.h"
#include "plugin/x/src/interface/operations_factory.h"
#include "plugin/x/src/interface/socket.h"
#include "plugin/x/src/interface/socket_events.h"

namespace xpl {

class Listener_tcp : public iface::Listener {
 public:
  using Socket_ptr = std::shared_ptr<iface::Socket>;
  using Factory_ptr = std::shared_ptr<iface::Operations_factory>;

  Listener_tcp(Factory_ptr operations_factory, const std::string &bind_address,
               const std::string &network_namespace, const uint16_t port,
               const uint32_t port_open_timeout, iface::Socket_events &event,
               const uint32_t backlog);
  ~Listener_tcp() override;

  const Sync_variable_state &get_state() const override;
  std::string get_configuration_variable() const override;

  bool setup_listener(On_connection on_connection) override;
  void close_listener() override;
  void pre_loop() override;
  void loop() override;
  void report_properties(On_report_properties on_status) override;
  bool report_status() const override;

 private:
  std::string choose_property_value(const std::string &value) const;
  Socket_ptr create_socket();

  Factory_ptr m_operations_factory;
  Sync_variable_state m_state;
  std::string m_bind_address;
  std::string m_network_namespace;
  const uint16_t m_port;
  const uint32_t m_port_open_timeout;
  const uint32_t m_backlog;
  Socket_ptr m_tcp_socket;
  iface::Socket_events &m_event;
  std::string m_last_error;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_IO_XPL_LISTENER_TCP_H_
