/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.
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

#include "mysql_command_backend.h"
#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_admin_session.h>
#include "include/mysql.h"
#include "include/mysqld_errmsg.h"
#include "include/sql_common.h"
#include "mysql/service_srv_session.h"
#include "mysql_command_delegates.h"
#include "sql/current_thd.h"
#include "sql/server_component/mysql_command_consumer_imp.h"
#include "sql/server_component/mysql_command_services_imp.h"
#include "sql/server_component/security_context_imp.h"
#include "sql/srv_session.h"

extern SERVICE_TYPE_NO_CONST(registry) * srv_registry;
extern SERVICE_TYPE_NO_CONST(registry) * srv_registry_no_lock;

namespace cs {

MYSQL_METHODS mysql_methods = {
    csi_connect,       csi_read_query_result, csi_advanced_command,
    csi_read_rows,     csi_use_result,        csi_fetch_row,
    csi_fetch_lengths, csi_flush_use_result,  csi_read_change_user_result,
#if !defined(MYSQL_SERVER) && !defined(MYSQL_COMPONENT)
    nullptr,  // csi_list_fields,
    nullptr,  // csi_read_prepare_result,
    nullptr,  // csi_stmt_execute,
    nullptr,  // csi_read_binary_rows,
    nullptr,  // csi_unbuffered_fetch,
    nullptr,  // csi_free_embedded_thd,
    nullptr,  // csi_read_statistics,
    nullptr,  // csi_next_result,
    nullptr,  // csi_read_rows_from_cursor
#endif        // ! MYSQL_SERVER
    nullptr,  /* read_query_result_nonblocking */
    nullptr,  /* advanced_command_nonblocking */
    nullptr,  /* read_rows_nonblocking */
    nullptr,  /* flush_use_result_nonblocking */
    nullptr,  /* next_result_nonblocking */
    nullptr,  /* read_change_user_result_nonblocking */
};

static mysql_state_machine_status acquire_services(
    mysql_command_consumer_refs *consumer_refs,
    mysql_service_registry_t *srv_registry) {
  my_h_service h_command_consumer = nullptr;
  my_h_service h_command_consumer_srv = nullptr;
  if (consumer_refs->factory_srv == nullptr) {
    if (srv_registry->acquire("mysql_text_consumer_factory_v1.mysql_server",
                              &h_command_consumer))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->factory_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_factory_v1) *>(h_command_consumer);
  }

  if (consumer_refs->metadata_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_metadata_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->metadata_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_metadata_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->row_factory_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_row_factory_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->row_factory_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_row_factory_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->error_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_error_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->error_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_error_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_null_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_null_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_null_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_null_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_integer_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_integer_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_integer_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_integer_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_longlong_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_longlong_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_longlong_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_longlong_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_decimal_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_decimal_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_decimal_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_decimal_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_double_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_double_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_double_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_double_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_date_time_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_date_time_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_date_time_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_date_time_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->get_string_srv == nullptr) {
    if (srv_registry->acquire_related("mysql_text_consumer_get_string_v1",
                                      h_command_consumer,
                                      &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->get_string_srv = reinterpret_cast<SERVICE_TYPE_NO_CONST(
          mysql_text_consumer_get_string_v1) *>(h_command_consumer_srv);
  }

  if (consumer_refs->client_capabilities_srv == nullptr) {
    if (srv_registry->acquire_related(
            "mysql_text_consumer_client_capabilities_v1", h_command_consumer,
            &h_command_consumer_srv))
      return STATE_MACHINE_FAILED;
    else
      consumer_refs->client_capabilities_srv =
          reinterpret_cast<SERVICE_TYPE_NO_CONST(
              mysql_text_consumer_client_capabilities_v1) *>(
              h_command_consumer_srv);
  }
  return STATE_MACHINE_DONE;
}

mysql_state_machine_status cssm_begin_connect(mysql_async_connect *ctx) {
  MYSQL *mysql = ctx->mysql;
  Mysql_handle mysql_handle;
  mysql_handle.mysql = mysql;
  auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  assert(mcs_extn);
  const char *host = ctx->host;
  const char *user = ctx->user;
  const char *db = ctx->db;
  MYSQL_THD thd;
  bool no_lock_registry = false;
  MYSQL_SESSION mysql_session = nullptr;

  if (mysql_command_services_imp::get(
          (MYSQL_H)&mysql_handle, MYSQL_NO_LOCK_REGISTRY, &no_lock_registry))
    return STATE_MACHINE_FAILED;
  mysql_service_registry_t *registry_service =
      no_lock_registry ? srv_registry_no_lock : srv_registry;

  if (mcs_extn->mcs_thd == nullptr || mcs_extn->session_svc == nullptr) {
    /*
     Avoid possibility of nested txn in the current thd.
     If it is called, for example from a UDF.
    */
    my_service<SERVICE_TYPE(mysql_admin_session)> service(
        "mysql_admin_session.mysql_server", registry_service);
    if (service.is_valid()) mysql_session = service->open(nullptr, ctx);
    if (mysql_session == nullptr) return STATE_MACHINE_FAILED;
    thd = mysql_session->get_thd();
    mcs_extn->is_thd_associated = false;
    Security_context_handle sc;
    if (mysql_security_context_imp::get(thd, &sc)) return STATE_MACHINE_FAILED;
    if (mysql_security_context_imp::lookup(sc, user, host, nullptr, db))
      return STATE_MACHINE_FAILED;
    mcs_extn->mcs_thd = thd;
    mysql->thd = thd;
    mcs_extn->session_svc = mysql_session;
  } else {
    mysql->thd = reinterpret_cast<void *>(mcs_extn->mcs_thd);
  }
  /*
    These references might be created in mysql_command_services_imp::set api.
    If not, we will create here.
  */
  if (mcs_extn->command_consumer_services == nullptr) {
    /*
      Provide default implementations for mysql command consumer services
      and will be released in close() api.
    */
    mcs_extn->command_consumer_services = new mysql_command_consumer_refs();
  }
  mysql_command_consumer_refs *consumer_refs =
      (mysql_command_consumer_refs *)mcs_extn->command_consumer_services;
  /* The above new allocation failed */
  if (consumer_refs == nullptr) return STATE_MACHINE_FAILED;
  /* If the services are not acquired by mysql_command_services_imp::set api,
     then it will be acquired. */
  auto status = acquire_services(consumer_refs, registry_service);
  if (status == STATE_MACHINE_FAILED) return status;
  mysql->client_flag = 0; /* For handshake */
  mysql->server_status = SERVER_STATUS_AUTOCOMMIT;
  return STATE_MACHINE_DONE;
}

MYSQL *csi_connect(mysql_async_connect *ctx) {
  assert(ctx);
  ctx->state_function = cs::cssm_begin_connect;
  return connect_helper(ctx);
}

bool csi_read_query_result(MYSQL *mysql) {
  mysql->status = MYSQL_STATUS_GET_RESULT;
  mysql->resultset_metadata = RESULTSET_METADATA_FULL;
  return false;
}

bool csi_advanced_command(MYSQL *mysql, enum enum_server_command command,
                          const uchar *, size_t, const uchar *arg,
                          size_t arg_length, bool, MYSQL_STMT *) {
  COM_DATA data;
  memset(&data, 0, sizeof(data));
  data.com_query.query = (const char *)arg;
  data.com_query.length = arg_length;
  char err_msg[1][256];
  SRV_CTX_H srv_ctx_h = nullptr;
  Mysql_handle mysql_handle;
  THD *thd = (THD *)mysql->thd;
  auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  void *command_consumer_srv = nullptr;
  bool ret = true;

  /* mcs_extn->command_consumer_services will be set in connect api */
  if (mcs_extn->command_consumer_services) {
    command_consumer_srv = mcs_extn->command_consumer_services;
  } else {
    return ret;
  }

  mysql_handle.mysql = mysql;
  if (mcs_extn->consumer_srv_data != nullptr)
    srv_ctx_h = reinterpret_cast<SRV_CTX_H>(mcs_extn->consumer_srv_data);
  else if (((class mysql_command_consumer_refs *)(command_consumer_srv))
               ->factory_srv->start(&srv_ctx_h, (MYSQL_H *)&mysql_handle)) {
    sprintf(*err_msg, "Could not create %s service",
            "mysql_text_consumer_factory_v1");
    goto error;
  }

  {
    Callback_command_delegate callback_delegate(command_consumer_srv,
                                                srv_ctx_h);
    if (command_service_run_command(
            mcs_extn->session_svc, command, &data, thd->charset(),
            callback_delegate.callbacks(), callback_delegate.representation(),
            &callback_delegate) ||
        thd->is_error()) {
      uint32_t err_num;
      char **ch_ptr = reinterpret_cast<char **>(&err_msg[0]);
      ((class mysql_command_consumer_refs *)(command_consumer_srv))
          ->error_srv->error(srv_ctx_h, &err_num,
                             const_cast<const char **>(ch_ptr));
      strcpy(*err_msg, *ch_ptr);
      goto error;
    }
  }
  ret = false;
error:
  if (ret) my_error(ER_COMMAND_SERVICE_BACKEND_FAILED, MYF(0), err_msg);
  return ret ? true : false;
}

MYSQL_DATA *csi_read_rows(MYSQL *mysql,
                          MYSQL_FIELD *mysql_fields [[maybe_unused]],
                          unsigned int fields [[maybe_unused]]) {
  auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  return std::exchange(mcs_extn->data, nullptr);
}

MYSQL_RES *csi_use_result(MYSQL *mysql) { return use_result(mysql); }

void csi_fetch_lengths(ulong *to, MYSQL_ROW column, unsigned int field_count) {
  for (unsigned int i = 0; i < field_count; i++) {
    if (!*column) {
      *to = 0; /* Null */
      continue;
    }
    *to = strlen(*column);
    column++;
    to++;
  }
}

void csi_flush_use_result(MYSQL *, bool) {
  // Dummy.
  // We already have entire result set. Therefore, there is
  // no need of the flusing the partial result set.
}

int csi_read_change_user_result(MYSQL *) {
  return static_cast<int> packet_error;
}

MYSQL_ROW csi_fetch_row(MYSQL_RES *res) {
  MYSQL_ROW tmp;
  if (!res->data_cursor) {
    DBUG_PRINT("info", ("end of data"));
    return res->current_row = (MYSQL_ROW) nullptr;
  }
  tmp = res->data_cursor->data;
  res->data_cursor = res->data_cursor->next;
  return res->current_row = tmp;
}
}  // namespace cs
