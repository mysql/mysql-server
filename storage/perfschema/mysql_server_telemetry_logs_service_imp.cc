/* Copyright (c) 2023, 2024 Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/mysql_server_telemetry_logs_service_imp.cc
  The performance schema implementation of server telemetry logs service.
*/

#include "storage/perfschema/mysql_server_telemetry_logs_service_imp.h"

#include <mysql/components/services/mysql_server_telemetry_logs_service.h>
#include <list>
#include <string>

#include "sql/auth/sql_security_ctx.h"
#include "sql/field.h"
#include "sql/pfs_priv_util.h"
#include "sql/sql_class.h"  // THD

#include "pfs_global.h"
#include "pfs_instr_class.h"

/* clang-format off */
/**

  @page PAGE_MYSQL_SERVER_TELEMETRY_LOGS_SERVICE Server telemetry logs service
  Performance Schema server telemetry logs service is a mechanism which provides
  plugins/components a way to register its callback to receive each telemetry
  log record being generated in MySQL.
  This way plugin/component can implement its custom log emit protocol.

  @subpage TELEMETRY_LOGS_SERVICE_INTRODUCTION

  @subpage TELEMETRY_LOGS_SERVICE_INTERFACE

  @subpage TELEMETRY_LOGS_EXAMPLE_PLUGIN_COMPONENT


  @page TELEMETRY_LOGS_SERVICE_INTRODUCTION Service Introduction

  This service is named <i>mysql_server_telemetry_logs</i> and it exposes two major
  methods:\n
  - @c register_logger   : plugin/component to register logger callback
  - @c unregister_logger : plugin/component to unregister logger callback

  @section TELEMETRY_LOGS_SERVICE_BLOCK_DIAGRAM Block Diagram
  Following diagram shows the block diagram of PFS services functionality, to
  register/unregister logger callback, exposed via mysql-server component.

@startuml

  actor client as "Plugin/component"
  box "Performance Schema Storage Engine" #LightBlue
  participant pfs_service as "mysql_server_telemetry_logs Service\n(mysql-server component)"
  endbox

  == Initialization ==
  client -> pfs_service :
  note right: Register notification callbacks \nPFS Service Call \n[register_logger()].

  == Cleanup ==
  client -> pfs_service :
  note right: Unregister notification callbacks \nPFS Service Call \n[unregister_logger()].

  @enduml

  @page TELEMETRY_LOGS_SERVICE_INTERFACE Service Interface

  This interface is provided to plugins/components, using which they can receive notifications
  related to each log produced by MySQL.

  Note that, at any given time, there can be only one user of this service.
  There is no support for multiple telemetry log callbacks
  being registered at the same time.

  @page TELEMETRY_LOGS_EXAMPLE_PLUGIN_COMPONENT  Example component

  Component/plugin that implements telemetry log export, typically also uses other services
  within the callback to inspect and filter out the logs according to its needs.
  For example, you can skip logging records based on log metadata such as log level (severity).
  As an example, see "components/test_server_telemetry_logs" test component source code,
  used to test this service.

*/
/* clang-format on */

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, mysql_server_telemetry_logs)
pfs_register_logger_v1, pfs_unregister_logger_v1, pfs_notify_logger_v1,
    END_SERVICE_IMPLEMENTATION();

#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
bool server_telemetry_logs_service_initialized = false;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */

// currently registered telemetry logs callback
std::atomic<log_delivery_callback_t> g_telemetry_log = nullptr;

// locking for callback register/unregister
mysql_mutex_t LOCK_pfs_logging_callback;
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
static PSI_mutex_key key_LOCK_pfs_logging_callback;
static PSI_mutex_info info_LOCK_pfs_logging_callback = {
    &key_LOCK_pfs_logging_callback, "LOCK_pfs_logging_callback",
    PSI_VOLATILITY_PERMANENT, PSI_FLAG_SINGLETON,
    "This lock protects telemetry logs callback function."};
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */

void initialize_mysql_server_telemetry_logs_service() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  g_telemetry_log = nullptr;

  assert(!server_telemetry_logs_service_initialized);

  /* This is called once at startup */
  mysql_mutex_register("pfs", &info_LOCK_pfs_logging_callback, 1);
  mysql_mutex_init(key_LOCK_pfs_logging_callback, &LOCK_pfs_logging_callback,
                   MY_MUTEX_INIT_FAST);
  server_telemetry_logs_service_initialized = true;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

void cleanup_mysql_server_telemetry_logs_service() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  if (server_telemetry_logs_service_initialized) {
    mysql_mutex_destroy(&LOCK_pfs_logging_callback);
    server_telemetry_logs_service_initialized = false;
  }
  g_telemetry_log = nullptr;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

bool pfs_register_logger_v1(log_delivery_callback_t logger [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  if (!server_telemetry_logs_service_initialized) return true;
  // allow overwriting existing callbacks to avoid possible time gap with no
  // telemetry available, if we would need to uninstall previous component using
  // this before installing new one
  mysql_mutex_lock(&LOCK_pfs_logging_callback);
  g_telemetry_log = logger;
  mysql_mutex_unlock(&LOCK_pfs_logging_callback);

  // update effective log level on backend registered
  for (size_t i = 0; i < logger_class_max; ++i) {
    if (logger_class_array[i].m_key > 0) {
      logger_class_array[i].m_effective_level = logger_class_array[i].m_level;
    }
  }
  // Success
  return false;
#else
  // Failure
  return true;
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}

bool pfs_unregister_logger_v1(log_delivery_callback_t logger [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  if (!server_telemetry_logs_service_initialized) return true;
  mysql_mutex_lock(&LOCK_pfs_logging_callback);
  if (g_telemetry_log == logger) {
    g_telemetry_log = nullptr;
    mysql_mutex_unlock(&LOCK_pfs_logging_callback);

    // update effective log level on backend unregistered
    for (size_t i = 0; i < logger_class_max; ++i) {
      if (logger_class_array[i].m_key > 0) {
        logger_class_array[i].m_effective_level = TLOG_NONE;
      }
    }
    // Success
    return false;
  }
  mysql_mutex_unlock(&LOCK_pfs_logging_callback);
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
  // Failure
  return true;
}

void pfs_notify_logger_v1(PSI_logger *logger [[maybe_unused]],
                          OTELLogLevel level [[maybe_unused]],
                          const char *message [[maybe_unused]],
                          time_t timestamp [[maybe_unused]],
                          const log_attribute_t *attr_array [[maybe_unused]],
                          size_t attr_count [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
  if (!server_telemetry_logs_service_initialized) return;
  if (g_telemetry_log != nullptr) {
    auto *entry = reinterpret_cast<PFS_logger_class *>(logger);
    if (entry == nullptr) return;

    mysql_mutex_lock(&LOCK_pfs_logging_callback);
    log_delivery_callback_t delivery = g_telemetry_log.load();
    if (delivery != nullptr) {
      const char *logger_name = entry->m_name.str();
      delivery(logger_name, level, message, timestamp, attr_array, attr_count);
    }
    mysql_mutex_unlock(&LOCK_pfs_logging_callback);
  }
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */
}
