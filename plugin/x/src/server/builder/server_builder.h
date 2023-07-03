/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_SERVER_BUILDER_SERVER_BUILDER_H_
#define PLUGIN_X_SRC_SERVER_BUILDER_SERVER_BUILDER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mysql/plugin.h"
#include "plugin/x/src/interface/server_task.h"
#include "plugin/x/src/interface/socket_events.h"
#include "plugin/x/src/interface/timeout_callback.h"
#include "plugin/x/src/io/xpl_listener_factory.h"
#include "plugin/x/src/ngs/protocol/protocol_config.h"
#include "plugin/x/src/ngs/scheduler.h"

namespace xpl {

class Server_builder {
 public:
  using Server_task_vector = std::vector<std::shared_ptr<iface::Server_task>>;
  using Value_changed_callback = std::function<void(THD *)>;

 public:
  explicit Server_builder(MYSQL_PLUGIN plugin_handle);

  std::shared_ptr<iface::Server_task> get_result_acceptor_task() const;
  Value_changed_callback get_result_reconfigure_server_callback();
  iface::Server *get_result_server_instance(
      const Server_task_vector &tasks) const;

 private:
  xpl::Listener_factory m_listener_factory;
  std::shared_ptr<iface::Socket_events> m_events;
  std::shared_ptr<iface::Timeout_callback> m_timeout_callback;
  std::shared_ptr<ngs::Protocol_global_config> m_config;
  std::shared_ptr<ngs::Scheduler_dynamic> m_thd_scheduler;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVER_BUILDER_SERVER_BUILDER_H_
