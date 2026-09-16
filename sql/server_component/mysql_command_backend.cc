/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.
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
#include "include/sql_common.h"
#include "my_dbug.h"
#include "mysql_command_delegates.h"
#include "sql/server_component/mysql_command_services_imp.h"
#include "sql/server_component/security_context_imp.h"
#include "sql/session_tracker.h"
#include "sql/sql_class.h"
#include "sql/srv_session.h"
#include "sql_string.h"

extern SERVICE_TYPE_NO_CONST(registry) * srv_registry;
extern SERVICE_TYPE_NO_CONST(registry) * srv_registry_no_lock;

namespace cs {

namespace {

bool has_session_track_info(MYSQL *mysql) {
  if (mysql == nullptr || mysql->extension == nullptr) return false;

  STATE_INFO *info = &(MYSQL_EXTENSION_PTR(mysql)->state_change);
  for (int i = SESSION_TRACK_BEGIN; i <= SESSION_TRACK_END; ++i) {
    if (info->info_list[i].head_node != nullptr) return true;
  }
  return false;
}

bool can_cache_session_track_info(MYSQL *mysql) {
  if (mysql == nullptr || mysql->extension == nullptr) return false;

  return (MYSQL_COMMAND_SERVICE_EXTN(mysql) != nullptr);
}

ulong get_default_client_capabilities(MYSQL *mysql) {
  if (mysql == nullptr) return 0;

  ulong client_capabilities = mysql->server_capabilities;
  if (mysql->extension == nullptr) return client_capabilities;

  auto *mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  if (mcs_extn != nullptr) {
    client_capabilities |= mcs_extn->mcs_client_flag;
  }
  return client_capabilities;
}

void set_command_service_error(MYSQL *mysql, uint sql_errno,
                               const char *err_msg, const char *sqlstate) {
  if (sql_errno == 0) sql_errno = ER_COMMAND_SERVICE_BACKEND_FAILED;
  if (sqlstate == nullptr || sqlstate[0] == '\0')
    sqlstate = mysql_errno_to_sqlstate(sql_errno);

  set_mysql_extended_error(mysql, sql_errno, sqlstate, "%s",
                           err_msg != nullptr ? err_msg : "");
}

bool store_session_track_info(MYSQL *mysql) {
  if (mysql == nullptr) return false;

  THD *thd =
      (mysql->thd != nullptr) ? reinterpret_cast<THD *>(mysql->thd) : nullptr;

  mysql_clear_session_track_info(mysql);

  if (thd == nullptr || !thd->session_tracker.enabled_any() ||
      !thd->session_tracker.changed_any()) {
    return false;
  }

  String encoded;
  encoded.set_charset(thd->variables.collation_database);
  thd->session_tracker.store(thd, encoded);

  if (encoded.length() == 0) return false;

  const auto *pos = reinterpret_cast<const uchar *>(encoded.ptr());
  const auto *end = pos + encoded.length();

  const size_t encoded_length_size = net_field_length_size(pos);
  if (static_cast<size_t>(end - pos) < encoded_length_size) {
    mysql_clear_session_track_info(mysql);
    return false;
  }

  auto *mutable_pos = const_cast<uchar *>(pos);
  const auto total_length =
      static_cast<size_t>(net_field_length_ll(&mutable_pos));
  pos = mutable_pos;

  // Session_tracker::store() uses the OK-packet inner payload format
  if (static_cast<size_t>(end - pos) < total_length ||
      mysql_decode_session_track_payload(mysql, pos, total_length)) {
    mysql_clear_session_track_info(mysql);
    return false;
  }

  return has_session_track_info(mysql);
}

void discard_session_track_info(THD *thd) {
  if (thd == nullptr || !thd->session_tracker.enabled_any() ||
      !thd->session_tracker.changed_any()) {
    return;
  }

  // store() serializes and clears pending tracker state. Use a temporary buffer
  // on error so failed commands do not leak tracker data into a later
  // successful one.
  String discarded;
  discarded.set_charset(thd->variables.collation_database);
  thd->session_tracker.store(thd, discarded);
}

/*
  Mirrors THD session-tracker state into the MYSQL handle cache at command
  boundaries so command-service consumers can read it through MYSQL_H
*/
class Backend_callback_command_delegate : public Callback_command_delegate {
 public:
  Backend_callback_command_delegate(void *srv, SRV_CTX_H srv_ctx_h,
                                    MYSQL *mysql)
      : Callback_command_delegate(srv, srv_ctx_h),
        m_mysql(mysql),
        m_client_capabilities(resolve_client_capabilities()) {}

  ulong get_client_capabilities() override { return m_client_capabilities; }

  void handle_ok(unsigned int server_status, unsigned int statement_warn_count,
                 unsigned long long affected_rows,
                 unsigned long long last_insert_id,
                 const char *const message) override {
    // Snapshot before forwarding handle_ok() so consumers can read it there
    THD *thd = (m_mysql != nullptr && m_mysql->thd != nullptr)
                   ? reinterpret_cast<THD *>(m_mysql->thd)
                   : nullptr;
    const bool can_cache = can_cache_session_track_info(m_mysql);
    const bool tracker_changed =
        (thd != nullptr && thd->session_tracker.enabled_any() &&
         thd->session_tracker.changed_any());
    unsigned int callback_server_status = server_status;
    const bool supports_session_track =
        ((get_client_capabilities() & CLIENT_SESSION_TRACK) != 0);

    if (!supports_session_track) {
      // Match libmysql: do not retain tracker state for consumers that did not
      // explicitly advertise CLIENT_SESSION_TRACK
      if (tracker_changed) {
        discard_session_track_info(thd);
      }
      if (can_cache) {
        mysql_clear_session_track_info(m_mysql);
      }
      callback_server_status &= ~SERVER_SESSION_STATE_CHANGED;
    } else if (tracker_changed) {
      callback_server_status |= SERVER_SESSION_STATE_CHANGED;
      if (can_cache) {
        if (!store_session_track_info(m_mysql)) {
          mysql_clear_session_track_info(m_mysql);
        }
      } else {
        // If the handle cannot cache session state, still consule the pending
        // tracker state so it does not leak into a later command executed on
        // the same THD
        discard_session_track_info(thd);
      }
    } else {
      if (can_cache) {
        mysql_clear_session_track_info(m_mysql);
      }
      callback_server_status &= ~SERVER_SESSION_STATE_CHANGED;
    }
    Callback_command_delegate::handle_ok(callback_server_status,
                                         statement_warn_count, affected_rows,
                                         last_insert_id, message);
  }

  void handle_error(uint sql_errno, const char *const err_msg,
                    const char *const sqlstate) override {
    THD *thd = (m_mysql != nullptr && m_mysql->thd != nullptr)
                   ? reinterpret_cast<THD *>(m_mysql->thd)
                   : nullptr;
    // Failed commands must not leave pending tracker state for a later
    // successful boundary on the same THD
    discard_session_track_info(thd);

    if (m_mysql != nullptr) {
      mysql_clear_session_track_info(m_mysql);
      m_mysql->server_status &= ~SERVER_SESSION_STATE_CHANGED;
      set_command_service_error(m_mysql, sql_errno, err_msg, sqlstate);
    }

    Callback_command_delegate::handle_error(sql_errno, err_msg, sqlstate);
  }

 private:
  ulong resolve_client_capabilities() {
    if (m_mysql == nullptr || m_mysql->extension == nullptr) {
      return Callback_command_delegate::get_client_capabilities() &
             ~CLIENT_SESSION_TRACK;
    }

    auto *mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(m_mysql);
    if (mcs_extn == nullptr ||
        !mcs_extn->has_custom_client_capabilities_service) {
      // Keep default command-service consumers aligned with libmysql semantics.
      // Session-tracker data is exposed only when a non-default client
      // capabilities service or MYSQL_COMMAND_CLIENT_FLAGS explicitly
      // advertises CLIENT_SESSION_TRACK.
      ulong client_capabilities = get_default_client_capabilities(m_mysql);
      if (mcs_extn == nullptr ||
          (mcs_extn->mcs_client_flag & CLIENT_SESSION_TRACK) == 0) {
        client_capabilities &= ~CLIENT_SESSION_TRACK;
      }
      return client_capabilities;
    }

    return Callback_command_delegate::get_client_capabilities() |
           mcs_extn->mcs_client_flag;
  }

  MYSQL *m_mysql;
  ulong m_client_capabilities;
};

}  // namespace

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
    mcs_extn->session_svc = mysql_session;
    if (mysql_security_context_imp::get(thd, &sc)) return STATE_MACHINE_FAILED;
    if (mysql_security_context_imp::lookup(sc, user, host, nullptr, db))
      return STATE_MACHINE_FAILED;
    mcs_extn->mcs_thd = thd;
    mysql->thd = thd;
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
  /*
    Statements with no result have nothing for mysql_use_result() to consume.
    Keep the handle ready so the next command is not rejected as out-of-sync.
  */
  mysql->status =
      mysql->field_count > 0 ? MYSQL_STATUS_GET_RESULT : MYSQL_STATUS_READY;
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

  /*
    Match libmysql command ordering: accept a new command only when the handle
    is ready and no multi-result statement has unread results. A DOM-backed
    mysql_use_result() still owns an active result until EOF or free.
  */
  if (mysql->status != MYSQL_STATUS_READY ||
      (mysql->server_status & SERVER_MORE_RESULTS_EXISTS)) {
    DBUG_PRINT("error", ("state: %d", mysql->status));
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    return true;
  }

  /*
    mysql_send_query() clears session-tracker cache before COM_QUERY reaches
    this backend. Other command-service commands enter through simple_command(),
    so keep cache invalidation at the backend boundary and clear the status bit
    for every new command attempt.
  */
  net_clear_error(&mysql->net);
  mysql_clear_session_track_info(mysql);
  mysql->server_status &= ~SERVER_SESSION_STATE_CHANGED;

  /* mcs_extn->command_consumer_services will be set in connect api */
  if (mcs_extn->command_consumer_services) {
    command_consumer_srv = mcs_extn->command_consumer_services;
  } else {
    return ret;
  }

  mysql_handle.mysql = mysql;
  /*
    The default consumer owns MYSQL::field_alloc, so we must end it before
    clearing the client-visible metadata. Clearing field_count here prevents a
    posterior command with no result from inheriting the previous result shape.
  */
  if (mcs_extn->consumer_srv_data != nullptr) {
    ((class mysql_command_consumer_refs *)(command_consumer_srv))
        ->factory_srv->end(mcs_extn->consumer_srv_data);
    mcs_extn->consumer_srv_data = nullptr;
  }
  free_old_query(mysql);

  if (((class mysql_command_consumer_refs *)(command_consumer_srv))
          ->factory_srv->start(&srv_ctx_h, (MYSQL_H *)&mysql_handle)) {
    sprintf(*err_msg, "Could not create %s service",
            "mysql_text_consumer_factory_v1");
    goto error;
  }
  mcs_extn->consumer_srv_data = srv_ctx_h;

  {
    Backend_callback_command_delegate callback_delegate(command_consumer_srv,
                                                        srv_ctx_h, mysql);
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
      /*
        command_service_run_command() may have delivered an error through
        Backend_callback_command_delegate::handle_error(), which already stores
        the server diagnostic on the MYSQL handle. Preserve that diagnostic and
        only synthesize a fallback backend error if no callback error was set.
      */
      if (mysql->net.last_errno == 0)
        set_command_service_error(mysql, err_num, *err_msg, nullptr);
      goto error;
    }
  }
  ret = false;
error:
  if (ret) {
    // Metadata may already have moved the handle to GET_RESULT, but a failed
    // command has no result that the caller can consume.
    mysql->status = MYSQL_STATUS_READY;
    mysql->server_status &= ~SERVER_MORE_RESULTS_EXISTS;
  }

  // Debug hook to simulate a successful command execution even if an error
  // occurred. Useful for testing how consumers handle error codes without
  // triggering a failure.
  DBUG_EXECUTE_IF("mysql_command_services_component_test_errno", return true;);

  if (ret) {
    if (mysql->net.last_errno == 0)
      set_command_service_error(mysql, ER_COMMAND_SERVICE_BACKEND_FAILED,
                                *err_msg, nullptr);
    my_error(ER_COMMAND_SERVICE_BACKEND_FAILED, MYF(0), *err_msg);
  }
  return ret ? true : false;
}

MYSQL_DATA *csi_read_rows(MYSQL *mysql,
                          MYSQL_FIELD *mysql_fields [[maybe_unused]],
                          unsigned int fields [[maybe_unused]]) {
  auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  mcs_extn->use_result_cursor = nullptr;
  return std::exchange(mcs_extn->data, nullptr);
}

static void csi_clear_use_result_data(MYSQL *mysql) {
  auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  if (mcs_extn == nullptr) return;

  free_rows(mcs_extn->data);
  mcs_extn->data = nullptr;
  mcs_extn->use_result_cursor = nullptr;
}

MYSQL_RES *csi_use_result(MYSQL *mysql) {
  MYSQL_RES *result = use_result(mysql);
  if (result == nullptr) return nullptr;

  /*
    Keep MYSQL_RES::data null so generic client APIs continue to treat this as
    an unbuffered mysql_use_result() result. The DOM rows stay in
    command-service private state and are released on EOF or
    mysql_free_result().
  */
  auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  if (mcs_extn == nullptr || mcs_extn->data == nullptr) {
    mysql_free_result(result);
    return nullptr;
  }

  mcs_extn->use_result_cursor = mcs_extn->data->data;
  return result;
}

void csi_fetch_lengths(ulong *to, MYSQL_ROW column, unsigned int field_count) {
  /*
    The DOM consumer stores an extra end pointer after the field pointers.
    Non-NULL lengths are derived from the distance to the next non-NULL pointer,
    or to that end pointer for the last non-NULL field.
  */
  MYSQL_ROW row_end = column + field_count;

  for (unsigned int field_index = 0; field_index < field_count; ++field_index) {
    if (column[field_index] == nullptr) {
      to[field_index] = 0;
      continue;
    }

    MYSQL_ROW next = column + field_index + 1;
    while (next != row_end && *next == nullptr) next++;
    to[field_index] = static_cast<ulong>(*next - column[field_index] - 1);
  }
}

void csi_flush_use_result(MYSQL *mysql, bool) {
  // mysql_free_result() reaches here for partially consumed use_result rows
  csi_clear_use_result_data(mysql);
}

int csi_read_change_user_result(MYSQL *) {
  return static_cast<int> packet_error;
}

MYSQL_ROW csi_fetch_row(MYSQL_RES *res) {
  /*
    DOM rows are materialized already, but mysql_use_result() still exposes
    unbuffered-result state. Keep the handle attached until EOF so fetch, free,
    and command ordering follow libmysql.
  */
  if (res->handle != nullptr) {
    MYSQL *mysql = res->handle;
    auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql);
    if (mysql->status != MYSQL_STATUS_USE_RESULT) {
      set_mysql_error(mysql,
                      res->unbuffered_fetch_cancelled ? CR_FETCH_CANCELED
                                                      : CR_COMMANDS_OUT_OF_SYNC,
                      unknown_sqlstate);
    } else if (mcs_extn != nullptr && mcs_extn->use_result_cursor != nullptr) {
      /*
        Keep MYSQL_RES::data/data_cursor empty so generic client APIs do not
        treat this mysql_use_result() result as rewindable buffered data.
      */
      MYSQL_ROW row = mcs_extn->use_result_cursor->data;
      mcs_extn->use_result_cursor = mcs_extn->use_result_cursor->next;
      csi_fetch_lengths(res->lengths, row, res->field_count);
      res->data_cursor = nullptr;
      ++res->row_count;
      return res->current_row = row;
    }

    DBUG_PRINT("info", ("end of data"));
    csi_clear_use_result_data(mysql);
    res->data_cursor = nullptr;
    res->eof = true;
    mysql->status = MYSQL_STATUS_READY;
    if (mysql->unbuffered_fetch_owner == &res->unbuffered_fetch_cancelled)
      mysql->unbuffered_fetch_owner = nullptr;
    res->handle = nullptr;
    return res->current_row = nullptr;
  }

  if (res->data == nullptr || res->data_cursor == nullptr) {
    DBUG_PRINT("info", ("end of data"));
    return res->current_row = nullptr;
  }

  MYSQL_ROW row = res->data_cursor->data;
  res->data_cursor = res->data_cursor->next;
  return res->current_row = row;
}
}  // namespace cs
