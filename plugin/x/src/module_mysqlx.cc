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

#include "plugin/x/src/module_mysqlx.h"

#include <memory>
#include <string>
#include <utility>

#include "my_dbug.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/helper/multithread/xsync_point.h"
#include "plugin/x/src/module_cache.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/server/builder/server_builder.h"
#include "plugin/x/src/services/mysqlx_group_member_status_listener.h"
#include "plugin/x/src/services/mysqlx_group_membership_listener.h"
#include "plugin/x/src/services/mysqlx_maintenance.h"
#include "plugin/x/src/services/registrator.h"
#include "plugin/x/src/sha256_password_cache.h"
#include "plugin/x/src/udf/mysqlx_error.h"
#include "plugin/x/src/udf/mysqlx_generate_document_id.h"
#include "plugin/x/src/udf/mysqlx_get_prepared_statement_id.h"
#include "plugin/x/src/udf/registry.h"
#include "plugin/x/src/variables/status_variables.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_performance_schema.h"

#include "scope_guard.h"  // NOLINT(build/include_subdir)

namespace modules {
namespace details {

std::shared_ptr<xpl::iface::Client> get_client_by_thd(THD *thd) {
  auto server = Module_mysqlx::get_instance_server();

  if (!server.container()) return {};

  return server->get_client(thd);
}

}  // namespace details

xpl::RWLock Module_mysqlx::m_instance_rwl{KEY_rwlock_x_xpl_server_instance};
xpl::Services *Module_mysqlx::m_services = nullptr;
xpl::Notice_input_queue *Module_mysqlx::m_input_queue = nullptr;
xpl::udf::Registry *Module_mysqlx::m_udf_register = nullptr;
xpl::iface::Server *Module_mysqlx::m_server = nullptr;
xpl::iface::SHA256_password_cache *Module_mysqlx::m_sha256_password_cache =
    nullptr;

void Module_mysqlx::require_services() {
  Module_mysqlx::m_services = new xpl::Services();

  if (!Module_mysqlx::m_services->is_valid())
    throw std::runtime_error("One of \"mysqlx_server\" services was not found");
}

void Module_mysqlx::unrequire_services() {
  delete Module_mysqlx::m_services;
  Module_mysqlx::m_services = nullptr;
}

void Module_mysqlx::provide_services() {
  xpl::Service_registrator r;

  r.register_service(SERVICE(mysql_server, mysqlx_maintenance));
  r.register_service(SERVICE(mysqlx, group_membership_listener));
  r.register_service(SERVICE(mysqlx, group_member_status_listener));
}

void Module_mysqlx::unprovide_services() {
  const char *service_names[] = {
      SERVICE_ID(mysql_server, mysqlx_maintenance),
      SERVICE_ID(mysqlx, group_membership_listener),
      SERVICE_ID(mysqlx, group_member_status_listener)};

  xpl::Service_registrator r;

  for (const auto name : service_names) {
    try {
      r.unregister_service(name);
    } catch (const std::exception &e) {
      log_error(ER_XPLUGIN_FAILED_TO_STOP_SERVICES, e.what());
    }
  }
}

void Module_mysqlx::provide_udfs() {
  m_udf_register = new xpl::udf::Registry();
  m_udf_register->insert({
      UDF(mysqlx_error),
      UDF(mysqlx_generate_document_id),
      UDF(mysqlx_get_prepared_statement_id),
  });
}

void Module_mysqlx::unregister_udfs() {
  if (m_udf_register) m_udf_register->drop();

  delete m_udf_register;
  m_udf_register = nullptr;
}

int Module_mysqlx::initialize(MYSQL_PLUGIN plugin_handle) {
  xpl::plugin_handle = plugin_handle;

  try {
    DBUG_EXECUTE_IF("xplugin_shutdown_unixsocket", {
      XSYNC_POINT_ENABLE({"xacceptor_stop_wait", "xacceptor_pre_loop_wait",
                          "xacceptor_post_loop_wait"});
    });
    xpl::init_performance_schema();

    provide_udfs();
    require_services();
    provide_services();

    if (mysqld::get_initialize()) {
      return 0;
    }

    xpl::Server_builder builder(plugin_handle);

    auto update_plugin_vars = builder.get_result_reconfigure_server_callback();
    auto sys_var_service = m_services->m_system_variable_register.get();

    xpl::Global_status_variables::initialize();
    xpl::Plugin_system_variables::initialize(
        sys_var_service, update_plugin_vars, details::get_client_by_thd);

    auto acceptor_task = builder.get_result_acceptor_task();

    if (!acceptor_task) return 1;

    xpl::RWLock_writelock guard_lock(&m_instance_rwl);
    auto guard_of_server_start = create_scope_guard([]() {
      if (m_server) m_server->start_failed();
    });

    m_sha256_password_cache =
        ngs::allocate_object<xpl::SHA256_password_cache>();
    m_input_queue = ngs::allocate_object<xpl::Notice_input_queue>();
    m_server = builder.get_result_server_instance(
        {acceptor_task, m_input_queue->create_broker_task()});

    // Cache cleaning plugin started before the X plugin so cache was not
    // enabled yet
    //
    // The `plugin` should be constructed in a way that `module_cache` is
    // initialized before `module_mysqlx`. Thus is almost all cases
    // `if` below should be evaluated as true.
    //
    // The problematic case is when module_cache was disabled by the user.
    // In this case we should execute delayed startup.
    if (Module_cache::m_is_sha256_password_cache_enabled) {
      m_sha256_password_cache->enable();
    }

    if (!m_server->prepare()) {
      // This is startup error, still we would like to keep
      // X Plugin loaded.
      return 0;
    }

    // Module_cache is not loaded, this means that it won't
    // be able start the server. We must do "delayed start".
    if (!Module_cache::m_is_sha256_password_cache_enabled) {
      m_server->delayed_start_tasks();
    }

    guard_of_server_start.commit();
  } catch (const std::exception &e) {
    log_error(ER_XPLUGIN_STARTUP_FAILED, e.what());
    return 1;
  }

  return 0;
}

int Module_mysqlx::deinitialize(MYSQL_PLUGIN) {
  // this flag will trigger the on_verify_server_state() timer to trigger an
  // acceptor thread exit
  if (m_server) m_server->stop();

  xpl::Plugin_system_variables::cleanup();

  {
    xpl::RWLock_writelock slock(&m_instance_rwl);
    ngs::free_object(m_server);
    m_server = nullptr;

    ngs::free_object(m_input_queue);
    m_input_queue = nullptr;

    ngs::free_object(m_sha256_password_cache);
    m_sha256_password_cache = nullptr;
  }

  unrequire_services();
  unprovide_services();
  unregister_udfs();

  xpl::plugin_handle = nullptr;

  return 0;
}

bool Module_mysqlx::reset() {
  auto server = get_instance_server();

  if (!server.container()) return false;

  if (!server->reset()) return false;

  const int64 worker_thread_count =
      xpl::Global_status_variables::instance().m_worker_thread_count.load();
  xpl::Global_status_variables::initialize(worker_thread_count);

  return true;
}

struct SYS_VAR **Module_mysqlx::get_plugin_variables() {
  return xpl::Plugin_system_variables::m_plugin_system_variables;
}

struct SHOW_VAR *Module_mysqlx::get_status_variables() {
  return xpl::Plugin_status_variables::m_plugin_status_variables;
}

Module_mysqlx::Server_with_lock Module_mysqlx::get_instance_server() {
  return Server_with_lock(m_server, &m_instance_rwl);
}

Module_mysqlx::Services_with_lock Module_mysqlx::get_instance_services() {
  return Services_with_lock(m_services, &m_instance_rwl);
}

Module_mysqlx::Notice_queue_with_lock
Module_mysqlx::get_instance_notice_queue() {
  return Notice_queue_with_lock(m_input_queue, &m_instance_rwl);
}

Module_mysqlx::Sha245_cache_with_lock
Module_mysqlx::get_instance_sha256_password_cache() {
  return Sha245_cache_with_lock(m_sha256_password_cache, &m_instance_rwl);
}

}  // namespace modules
