/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_LISTENER_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_LISTENER_INTERFACE_H_

#include <vector>

#include "plugin/x/ngs/include/ngs/server_properties.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"
#include "plugin/x/src/helper/multithread/sync_variable.h"
#include "violite.h"

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
  using Sync_variable_state = xpl::Sync_variable<State_listener>;
  using On_connection = ngs::function<void(Connection_acceptor_interface &)>;
  using On_report_properties = ngs::function<void(
      const Server_property_ids status_id, const std::string &status_value)>;

  virtual ~Listener_interface() = default;

  virtual Sync_variable_state &get_state() = 0;
  virtual std::string get_last_error() = 0;
  virtual std::string get_name_and_configuration() const = 0;
  virtual std::vector<std::string> get_configuration_variables() const = 0;
  virtual void report_properties(On_report_properties on_status) = 0;

  virtual bool setup_listener(On_connection) = 0;
  virtual void close_listener() = 0;

  virtual void loop() = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_LISTENER_INTERFACE_H_
