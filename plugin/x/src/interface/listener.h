/*
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_LISTENER_H_
#define PLUGIN_X_SRC_INTERFACE_LISTENER_H_

#include <functional>
#include <string>
#include <vector>

#include "violite.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/helper/multithread/sync_variable.h"
#include "plugin/x/src/ngs/thread.h"
#include "plugin/x/src/server/server_properties.h"

namespace xpl {
namespace iface {

class Connection_acceptor;

class Listener {
 public:
  enum class State { k_initializing, k_prepared, k_running, k_stopped };

  using Sync_variable_state = Sync_variable<State>;
  using On_connection = std::function<void(Connection_acceptor &)>;
  using On_report_properties =
      std::function<void(const ngs::Server_property_ids status_id,
                         const std::string &status_value)>;

  virtual ~Listener() = default;

  virtual const Sync_variable_state &get_state() const = 0;
  virtual void report_properties(On_report_properties on_status) = 0;
  virtual bool report_status() const = 0;
  virtual std::string get_configuration_variable() const = 0;

  virtual bool setup_listener(On_connection) = 0;
  virtual void close_listener() = 0;

  virtual void pre_loop() = 0;
  virtual void loop() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_LISTENER_H_
