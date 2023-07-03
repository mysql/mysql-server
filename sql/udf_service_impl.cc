/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/components/services/udf_metadata.h>
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/udf_registration.h"
#include "mysql/service_plugin_registry.h"

#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/udf_service_impl.h"

#include "sql/rpl_async_conn_failover_add_managed_udf.h"
#include "sql/rpl_async_conn_failover_add_source_udf.h"
#include "sql/rpl_async_conn_failover_delete_managed_udf.h"
#include "sql/rpl_async_conn_failover_delete_source_udf.h"
#include "sql/rpl_async_conn_failover_reset_udf.h"

bool Udf_service_impl::register_udf(Udf_data &udf) {
  DBUG_TRACE;

  my_service<SERVICE_TYPE(udf_registration)> udf_registration_handler(
      "udf_registration", srv_registry);

  if (udf_registration_handler.is_valid() &&
      DBUG_EVALUATE_IF("rpl_async_udf_register_service_error", 0, 1)) {
    const char *name = udf.m_name.c_str();
    if (DBUG_EVALUATE_IF("rpl_async_udf_register_error", 1, 0) ||
        udf_registration_handler->udf_register(
            name, udf.m_return_type, reinterpret_cast<Udf_func_any>(udf.m_func),
            udf.m_init_func, udf.m_deinit_func)) {
      /* purecov: begin inspected */ /* Only needed if register fails. */
      LogErr(ERROR_LEVEL, ER_UDF_REGISTER_ERROR, name);
      return true;
      /* purecov: end */
    }

    m_udfs_registered.push_back(name);

  } else {
    /* purecov: begin inspected */
    LogErr(ERROR_LEVEL, ER_UDF_REGISTER_SERVICE_ERROR);
    return true;
    /* purecov: end */
  }

  return false;
}

bool Udf_service_impl::unregister_udf(const std::string udf_name) {
  DBUG_TRACE;

  my_service<SERVICE_TYPE(udf_registration)> udf_registration_handler(
      "udf_registration", srv_registry);

  const char *name = udf_name.c_str();
  if (udf_registration_handler.is_valid()) {
    int existed;
    if (udf_registration_handler->udf_unregister(name, &existed) &&
        DBUG_EVALUATE_IF("rpl_async_udf_unregister_error", 0, 1)) {
      LogErr(ERROR_LEVEL, ER_UDF_UNREGISTER_ERROR);
      return true;
    }
    if (existed) {
      auto entry =
          std::find(m_udfs_registered.begin(), m_udfs_registered.end(), name);
      if (entry != m_udfs_registered.end()) m_udfs_registered.erase(entry);
    }
  }

  return false;
}

bool Udf_service_impl::deinit() {
  DBUG_TRACE;

  for (auto &udf : m_udfs_registered) {
    if (unregister_udf(udf.c_str())) return true;
  }

  return false;
}

Udf_load_service::Udf_load_service() { register_udf(); }

Udf_load_service::~Udf_load_service() { unregister_udf(); }

bool Udf_load_service::init() {
  bool error{false};
  for (auto udf : m_udfs_registered) {
    if (udf->init()) error = true;
  }
  return error;
}

bool Udf_load_service::deinit() {
  bool error{false};
  for (auto udf : m_udfs_registered) {
    if (udf->deinit()) error = true;
  }
  return error;
}

void Udf_load_service::register_udf() {
  add<Rpl_async_conn_failover_add_source>();
  add<Rpl_async_conn_failover_delete_source>();
  add<Rpl_async_conn_failover_add_managed>();
  add<Rpl_async_conn_failover_delete_managed>();
  add<Rpl_async_conn_failover_reset>();
}

void Udf_load_service::unregister_udf() {
  for (auto udf : m_udfs_registered) {
    delete udf;
  }
}
