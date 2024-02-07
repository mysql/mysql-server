/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "include/my_dbug.h"
#include "sql/sql_class.h"

#include "m_string.h"
#include "sql/auth/auth_acls.h"
#include "sql/derror.h" /* ER_THD */
#include "sql/rpl_async_conn_failover_add_source_udf.h"
#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_group_replication.h"
#include "sql/rpl_io_monitor.h"
#include "string_with_len.h"

const int DEFAULT_WEIGHT_VAL = 50;

bool Rpl_async_conn_failover_add_source::init() {
  DBUG_TRACE;

  Udf_data udf(m_udf_name, STRING_RESULT,
               Rpl_async_conn_failover_add_source::add_source,
               Rpl_async_conn_failover_add_source::add_source_init,
               Rpl_async_conn_failover_add_source::add_source_deinit);

  m_initialized = !register_udf(udf);
  return !m_initialized;
}

bool Rpl_async_conn_failover_add_source::deinit() {
  DBUG_TRACE;

  if (m_initialized && !unregister_udf(m_udf_name)) {
    m_initialized = false;
  }

  return m_initialized;
}

char *Rpl_async_conn_failover_add_source::add_source(UDF_INIT *, UDF_ARGS *args,
                                                     char *result,
                                                     unsigned long *length,
                                                     unsigned char *,
                                                     unsigned char *error) {
  DBUG_TRACE;
  *error = 0;
  Rpl_async_conn_failover_table_operations sql_operations(TL_WRITE);

  auto err_val{false};    // error value
  std::string err_msg{};  // error message

  std::string channel(args->args[0], args->lengths[0]);         // channel name
  std::string host(args->args[1], args->lengths[1]);            // hostname
  uint port = *(reinterpret_cast<long long *>(args->args[2]));  // port

  /* if weight is provided assign that otherwise add default weight value */
  uint weight = (args->arg_count > 4) ? *((long long *)args->args[4])
                                      : DEFAULT_WEIGHT_VAL;

  /* add row */
  std::tie(err_val, err_msg) =
      sql_operations.add_source(channel, host, port, "", weight, "");

  if (err_val) {
    *error = 1;
    my_error(ER_UDF_ERROR, MYF(0), m_udf_name, err_msg.c_str());
  } else {
    err_msg.assign(
        "The UDF asynchronous_connection_failover_add_source() "
        "executed successfully.");

    if (args->arg_count > 3 && args->lengths[3] > 0) {
      push_warning(
          current_thd, Sql_condition::SL_WARNING,
          ER_WARN_ASYNC_CONN_FAILOVER_NETWORK_NAMESPACE,
          ER_THD(current_thd, ER_WARN_ASYNC_CONN_FAILOVER_NETWORK_NAMESPACE));
    }
  }

  strcpy(result, err_msg.c_str());
  *length = err_msg.length();
  return result;
}

bool Rpl_async_conn_failover_add_source::add_source_init(UDF_INIT *init_id,
                                                         UDF_ARGS *args,
                                                         char *message) {
  DBUG_TRACE;
  if (args->arg_count < 3) {
    my_stpcpy(message,
              "Wrong arguments: You need to specify all mandatory "
              "arguments.");
    return true;
  }

  if (args->arg_count > 5) {
    my_stpcpy(message, "Wrong arguments: You must specify max 5 arguments.");
    return true;
  }

  if (args->arg_type[0] != STRING_RESULT) {
    my_stpcpy(message, "Wrong arguments: You need to specify channel name.");
    return true;
  }

  if (args->arg_type[1] != STRING_RESULT || args->lengths[1] == 0) {
    my_stpcpy(message, "Wrong arguments: You need to specify hostname.");
    return true;
  }

  if (args->arg_type[2] != INT_RESULT) {
    my_stpcpy(message, "Wrong arguments: You need to specify value for port.");
    return true;
  }

  if (args->arg_count > 3 && args->arg_type[3] != STRING_RESULT) {
    my_stpcpy(message,
              "Wrong arguments: You need to specify a string value for "
              "network_namespace.");
    return true;
  }

  if (args->arg_count == 5 &&
      (args->arg_type[4] != INT_RESULT || (*((long long *)args->args[4]) < 1) ||
       (*((long long *)args->args[4]) > 100))) {
    my_stpcpy(message,
              "Wrong arguments: The weight argument value should be integer "
              "value between 1-100.");
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

  if (is_group_replication_member_secondary()) {
    my_stpcpy(message,
              "Can't execute the given operation on a Group Replication "
              "secondary member.");
    return true;
  }

  if (Udf_charset_service::set_return_value_charset(init_id) ||
      Udf_charset_service::set_args_charset(args)) {
    return true;
  }

  init_id->maybe_null = false;
  return false;
}

void Rpl_async_conn_failover_add_source::add_source_deinit(UDF_INIT *) {
  DBUG_TRACE;
  return;
}
