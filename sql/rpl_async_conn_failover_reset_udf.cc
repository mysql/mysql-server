/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "include/m_string.h"
#include "include/my_dbug.h"
#include "sql/sql_class.h"

#include "sql/auth/auth_acls.h"
#include "sql/derror.h" /* ER_THD */
#include "sql/rpl_async_conn_failover_reset_udf.h"
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_channel_service_interface.h"
#include "sql/rpl_group_replication.h"

bool Rpl_async_conn_failover_reset::init() {
  DBUG_TRACE;

  Udf_data udf(m_udf_name, STRING_RESULT, Rpl_async_conn_failover_reset::reset,
               Rpl_async_conn_failover_reset::reset_init,
               Rpl_async_conn_failover_reset::reset_deinit);

  m_initialized = !register_udf(udf);
  return !m_initialized;
}

bool Rpl_async_conn_failover_reset::deinit() {
  DBUG_TRACE;
  if (m_initialized) {
    return unregister_udf(m_udf_name);
  }
  return false;
}

char *Rpl_async_conn_failover_reset::reset(UDF_INIT *, UDF_ARGS *, char *result,
                                           unsigned long *length,
                                           unsigned char *is_null,
                                           unsigned char *error) {
  DBUG_TRACE;
  *is_null = 0;  // result is not null
  *error = 0;

  std::string err_msg{};  // error message returned during delete row operation

  Rpl_async_conn_failover_table_operations sql_operations(TL_WRITE);
  if (sql_operations.reset()) {
    err_msg.assign(
        "The UDF asynchronous_connection_failover_reset() "
        "was unable to reset the configuration.");
    *error = 1;
    my_error(ER_UDF_ERROR, MYF(0), m_udf_name, err_msg.c_str());
  } else {
    err_msg.assign(
        "The UDF asynchronous_connection_failover_reset() "
        "executed successfully.");
  }

  my_stpcpy(result, err_msg.c_str());
  *length = err_msg.length();
  return result;
}

bool Rpl_async_conn_failover_reset::reset_init(UDF_INIT *init_id,
                                               UDF_ARGS *args, char *message) {
  DBUG_TRACE;

  if (args->arg_count > 0) {
    my_stpcpy(message, "Wrong arguments: The function takes no arguments.");
    return true;
  }

  THD *thd{current_thd};
  if (thd == nullptr) {
    my_stpcpy(message,
              "Error checking the user privileges. Check the log for "
              "more details or restart the server.");
    return true;
  }

  Security_context *sctx = thd->security_context();
  if (!sctx->check_access(SUPER_ACL) &&
      !sctx->has_global_grant(STRING_WITH_LEN("REPLICATION_SLAVE_ADMIN"))
           .first) {
    my_stpcpy(message,
              "Access denied; you need (at least one of) the SUPER or "
              "REPLICATION_SLAVE_ADMIN privilege for this operation");
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "SUPER or REPLICATION_SLAVE_ADMIN");
    return true;
  }

  if (thd && thd->locked_tables_mode) {
    my_stpcpy(message,
              "Can't execute the given operation because you have"
              " active locked tables.");
    return true;
  }

  if (Udf_charset_service::set_return_value_charset(init_id) ||
      Udf_charset_service::set_args_charset(args))
    return true;

  if (is_group_replication_running()) {
    my_stpcpy(message,
              "Can't execute the given operation while Group Replication is "
              "running.");
    return true;
  }

  if (is_any_slave_channel_running_with_failover_enabled(
          CHANNEL_RECEIVER_THREAD | CHANNEL_APPLIER_THREAD)) {
    my_stpcpy(message,
              "Can't execute the given operation while Replication channels "
              "configured with SOURCE_CONNECTION_AUTO_FAILOVER "
              "are running.");
    return true;
  }

  init_id->maybe_null = false;
  return false;
}

void Rpl_async_conn_failover_reset::reset_deinit(UDF_INIT *) {
  DBUG_TRACE;
  return;
}
