/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <mysql/components/services/log_builtins.h>
#include <mysql/service_plugin_registry.h>
#include <mysqld_error.h>
#include <atomic>
#include "my_dbug.h"
#include "required_services.h"
#include "template_utils.h"  // pointer_cast

namespace binlog::service::iterators::tests {

my_h_service h_ret_statvar_svc = nullptr;
SERVICE_TYPE(status_variable_registration) *statvar_register_srv = nullptr;

std::atomic<uint64_t> global_status_var_count_buffer_reallocations{0};
std::atomic<uint64_t> global_status_var_sum_buffer_size_requested{0};

static bool acquire_service_handles() {
  DBUG_TRACE;

  /* Acquire mysql_server's registry service */
  const auto *r = mysql_plugin_registry_acquire();
  if (r == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "mysql_plugin_registry_acquire() returns empty");
    return true;
    /* purecov: end */
  }

  /* Acquire pfs_plugin_table_v1 service */
  if (r->acquire("status_variable_registration", &h_ret_statvar_svc) != 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find status_variable_registration service");
    return true;
    /* purecov: end */
  }

  /* Type cast this handler to proper service handle */
  statvar_register_srv =
      reinterpret_cast<SERVICE_TYPE(status_variable_registration) *>(
          h_ret_statvar_svc);

  mysql_plugin_registry_release(r);
  r = nullptr;

  return false;
}

static void release_service_handles() {
  DBUG_TRACE;
  const auto *r = mysql_plugin_registry_acquire();
  if (r == nullptr) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "mysql_plugin_registry_acquire() returns empty");
    /* purecov: end */
  }

  if (r != nullptr) { /* purecov: inspected */
    if (h_ret_statvar_svc != nullptr) {
      /* Release pfs_plugin_table_v1 services */
      r->release(h_ret_statvar_svc);
      h_ret_statvar_svc = nullptr;
      statvar_register_srv = nullptr;
    }

    /* Release registry service */
    mysql_plugin_registry_release(r);
    r = nullptr;
  }
}

static int show_count_buffer_reallocations(THD * /*unused*/, SHOW_VAR *var,
                                           char *buf) {
  var->type = SHOW_LONG;
  var->value = buf;
  *(pointer_cast<unsigned long long *>(buf)) =
      global_status_var_count_buffer_reallocations;
  return 0;
}

static int show_sum_buffer_size_requested(THD * /*unused*/, SHOW_VAR *var,
                                          char *buf) {
  var->type = SHOW_LONG;
  var->value = buf;
  *(pointer_cast<unsigned long long *>(buf)) =
      global_status_var_sum_buffer_size_requested;
  return 0;
}

SHOW_VAR status_func_var[] = {
    {"test_binlog_storage_iterator.count_buffer_reallocations",
     (char *)show_count_buffer_reallocations, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_binlog_storage_iterator.sum_buffer_size_requested",
     (char *)show_sum_buffer_size_requested, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

bool register_status_variables() {
  DBUG_TRACE;
  if (acquire_service_handles()) return true;
  if (statvar_register_srv->register_variable((SHOW_VAR *)&status_func_var) !=
      0)
    return true; /* purecov: inspected */
  return false;
}

bool unregister_status_variables() {
  DBUG_TRACE;
  statvar_register_srv->unregister_variable((SHOW_VAR *)&status_func_var);
  release_service_handles();
  return false;
}

}  // namespace binlog::service::iterators::tests
