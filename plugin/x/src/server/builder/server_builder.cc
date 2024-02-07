/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/server/builder/server_builder.h"

#include <memory>
#include <string>

#include "plugin/x/src/ngs/scheduler.h"
#include "plugin/x/src/ngs/socket_acceptors_task.h"
#include "plugin/x/src/ngs/socket_events.h"
#include "plugin/x/src/ngs/timeout_callback.h"
#include "plugin/x/src/server/scheduler_monitor.h"
#include "plugin/x/src/server/server.h"
#include "plugin/x/src/server/session_scheduler.h"
#include "plugin/x/src/variables/status_variables.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

using Monitor_interface_ptr =
    std::unique_ptr<iface::Scheduler_dynamic::Monitor>;

Server_builder::Server_builder(MYSQL_PLUGIN plugin_handle)
    : m_events(ngs::allocate_shared<ngs::Socket_events>()),
      m_timeout_callback(ngs::allocate_shared<ngs::Timeout_callback>(m_events)),
      m_config(ngs::allocate_shared<ngs::Protocol_global_config>()),
      m_thd_scheduler(ngs::allocate_shared<xpl::Session_scheduler>(
          "work", plugin_handle,
          Monitor_interface_ptr{new xpl::Worker_scheduler_monitor()})) {}

std::shared_ptr<iface::Server_task> Server_builder::get_result_acceptor_task()
    const {
  uint32 listen_backlog =
      50 + xpl::Plugin_system_variables::m_max_connections / 5;
  if (listen_backlog > 900) listen_backlog = 900;

  auto acceptors = ngs::allocate_shared<ngs::Socket_acceptors_task>(
      std::ref(m_listener_factory),
      xpl::Plugin_system_variables::m_bind_address,
      xpl::Plugin_system_variables::m_port,
      xpl::Plugin_system_variables::m_port_open_timeout,
      xpl::Plugin_system_variables::m_socket, listen_backlog, m_events);

  return acceptors;
}

Plugin_system_variables::Value_changed_callback
Server_builder::get_result_reconfigure_server_callback() {
  auto thd_scheduler = m_thd_scheduler;
  auto config = m_config;

  return [thd_scheduler, config](THD *thd) {
    // Update came from THDVAR, we do not need it.
    if (nullptr != thd) return;

    const auto min = thd_scheduler->set_num_workers(
        xpl::Plugin_system_variables::m_min_worker_threads);
    if (min < xpl::Plugin_system_variables::m_min_worker_threads)
      xpl::Plugin_system_variables::m_min_worker_threads = min;

    thd_scheduler->set_idle_worker_timeout(
        xpl::Plugin_system_variables::m_idle_worker_thread_timeout * 1000);

    config->max_message_size =
        xpl::Plugin_system_variables::m_max_allowed_packet;
    config->connect_timeout =
        xpl::chrono::Seconds(xpl::Plugin_system_variables::m_connect_timeout);

    config->m_timeouts = xpl::Plugin_system_variables::get_global_timeouts();
  };
}

iface::Server *Server_builder::get_result_server_instance(
    const Server_task_vector &tasks) const {
  auto net_scheduler = ngs::allocate_shared<ngs::Scheduler_dynamic>(
      "network", KEY_thread_x_acceptor);
  auto result = ngs::allocate_object<ngs::Server>(
      net_scheduler, m_thd_scheduler, m_config,
      &xpl::Plugin_status_variables::m_properties, tasks, m_timeout_callback);

  return result;
}

}  // namespace xpl
