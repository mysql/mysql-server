/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef NGS_LISTENER_INTERFACE_H_
#define NGS_LISTENER_INTERFACE_H_

#include "violite.h"
#include "ngs/thread.h"
#include <vector>


namespace ngs {

class Connection_acceptor_interface;

enum State_listener {
  State_listener_initializing,
  State_listener_prepared,
  State_listener_running,
  State_listener_stopped
};

class Listener_interface {
public:
  typedef Sync_variable<State_listener> Sync_variable_state;
  typedef ngs::function<void(Connection_acceptor_interface &)> On_connection;

  virtual ~Listener_interface() {};

  virtual Sync_variable_state &get_state() = 0;
  virtual std::string get_last_error() = 0;
  virtual std::string get_name_and_configuration() const = 0;
  virtual std::vector<std::string> get_configuration_variables() const = 0;
  virtual bool is_handled_by_socket_event() = 0;

  virtual bool setup_listener(On_connection) = 0;
  virtual void close_listener() = 0;

  virtual void loop() = 0;
};

} // namespace ngs

#endif // NGS_LISTENER_INTERFACE_H_
