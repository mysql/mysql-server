/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include <my_inttypes.h>
#include <my_sys.h>
#include <mysql.h>
#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/bits/psi_memory_bits.h>
#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/service_mysql_alloc.h>
#include <sql/server_component/mysql_command_backend.h>
#include <sql/server_component/mysql_command_consumer_imp.h>
#include <sql/server_component/mysql_command_delegates.h>
#include <sql/server_component/mysql_command_services_imp.h>
#include "sql/mysqld.h"  // srv_registry
#include "sql/server_component/security_context_imp.h"
#include "sql/srv_session.h"

PSI_memory_key key_memory_query_service;
struct Error_handler {
  int m_last_sql_errno;
  const char *m_last_sql_error;
  static void handler(void *ctx, unsigned int sql_errno, const char *err_msg) {
    assert(ctx);
    Error_handler *self = reinterpret_cast<Error_handler *>(ctx);
    self->m_last_sql_errno = sql_errno;
    self->m_last_sql_error = err_msg;
  }
} default_error_h;

namespace {

constexpr const char *k_default_text_consumer_factory_service =
    "mysql_text_consumer_factory_v1.mysql_server";
constexpr const char *k_default_text_consumer_client_capabilities_service =
    "mysql_text_consumer_client_capabilities_v1.mysql_server";

Dom_ctx *get_error_dom_ctx(MYSQL *mysql) {
  if (mysql == nullptr || mysql->extension == nullptr) return nullptr;

  auto *mcs_ext = MYSQL_COMMAND_SERVICE_EXTN(mysql);
  if (mcs_ext == nullptr || mcs_ext->consumer_srv_data == nullptr ||
      mcs_ext->has_custom_consumer_factory) {
    return nullptr;
  }

  return reinterpret_cast<Dom_ctx *>(mcs_ext->consumer_srv_data);
}

}  // namespace

/**
  Calls mysql_init() api to Gets or initializes a MYSQL structure

  @param[out] mysql_h Prepared mysql object from mysql_init call.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::init, (MYSQL_H * mysql_h)) {
  bool ret = false;
  try {
    THD *thd = current_thd;
    /* Not supposed to work recursively */
    if (thd != nullptr && thd->is_a_srv_session()) {
      my_error(ER_CANT_RUN_COMMAND_SERVICES_RECURSIVELY, MYF(0));
      return true;
    }
    Mysql_handle *mysql_handle = (Mysql_handle *)my_malloc(
        key_memory_query_service, sizeof(Mysql_handle),
        MYF(MY_WME | MY_ZEROFILL));
    if (mysql_handle == nullptr) return true;
    mysql_handle->connection_mode = mysql_command_connection_mode::kEmbedded;
    auto mysql = &mysql_handle->mysql;
    *mysql = (mysql_init(nullptr));
    if (*mysql == nullptr) {
      my_free(mysql_handle);
      return true;
    }
    mysql_handle->client_methods = (*mysql)->methods;
    *mysql_h = reinterpret_cast<MYSQL_H>(mysql_handle);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    ret = true;
  }
  return ret;
}

static MYSQL_LEX_CSTRING dummy_plugin{"server_service",
                                      sizeof("server_service")};
/**
  Calls session init_thread() to initialize a physical thread to use the session
  service.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::init, ()) {
  return Srv_session::init_thread(&dummy_plugin);
}

/**
  Calls session deinit_thread() to deinitialize a physical thread that has been
  using the session service.
*/
DEFINE_METHOD(void, mysql_command_services_imp::end, ()) {
  Srv_session::deinit_thread();
}

bool mysql_command_services_imp::bind_address_accepts_loopback(
    const char *bind_address, bool use_ipv4) {
  if (bind_address == nullptr) return false;

  const char *loopback_address = use_ipv4 ? "127.0.0.1" : "::1";
  const char *wildcard_address = use_ipv4 ? "0.0.0.0" : "::";
  return strcmp(bind_address, loopback_address) == 0 ||
         strcmp(bind_address, "*") == 0 ||
         strcmp(bind_address, wildcard_address) == 0;
}

/**
  Calls mysql_real_connect api to connect to a MySQL server.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::connect, (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    auto mysql = &m_handle->mysql;
    if (*mysql == nullptr) return true;

    auto invalid_parameter = [mysql] {
      set_mysql_error(*mysql, CR_INVALID_PARAMETER_NO, unknown_sqlstate);
      return true;
    };

    if (m_handle->connection_mode ==
        mysql_command_connection_mode::kAuthenticationAttempted) {
      return invalid_parameter();
    }

    auto mcs_ext = MYSQL_COMMAND_SERVICE_EXTN(*mysql);
    if (mcs_ext == nullptr) return true;

    const char *host = mcs_ext->mcs_host_name;
    const char *user = mcs_ext->mcs_user_name;
    const char *password = mcs_ext->mcs_password;
    const char *protocol = mcs_ext->mcs_protocol;
    const char *db = mcs_ext->mcs_db;
    const int configured_port = mcs_ext->mcs_tcpip_port;
    const uint32_t client_flags = mcs_ext->mcs_client_flag;

    if (m_handle->connection_mode == mysql_command_connection_mode::kEmbedded) {
      const char *socket = nullptr;
      int port = -1;

      // Override the default mysql methods with the embedded backend
      (*mysql)->methods = &cs::mysql_methods;
      if (host == nullptr) host = MYSQL_SYS_HOST;
      if (user == nullptr) user = MYSQL_SESSION_USER;
      if (protocol != nullptr && strcmp(protocol, "local")) socket = protocol;
      if (configured_port != 0) port = configured_port;

      return mysql_real_connect(*mysql, host, user, nullptr, db, port, socket,
                                client_flags)
                 ? false
                 : true;
    }

    // Consume authenticated mode before validation or connection can fail
    m_handle->connection_mode =
        mysql_command_connection_mode::kAuthenticationAttempted;
    mcs_ext->mcs_password = nullptr;

    if (m_handle->client_methods == nullptr) return invalid_parameter();
    (*mysql)->methods = m_handle->client_methods;

    /*
      Client defaults are loaded inside mysql_real_connect(), after the local
      endpoint is selected, and can override its protocol, port, or socket.
    */
    if (password == nullptr || user == nullptr || configured_port < 0 ||
        (*mysql)->options.my_cnf_file != nullptr ||
        (*mysql)->options.my_cnf_group != nullptr ||
        mcs_ext->session_svc != nullptr ||
        mcs_ext->command_consumer_services != nullptr ||
        mcs_ext->no_lock_registry) {
      return invalid_parameter();
    }

    if (host == nullptr || host[0] == '\0') host = my_localhost;

    // Only canonical loopback literals may select authenticated TCP,
    // preventing credentials from being forwarded to an arbitrary endpoint.
    const bool use_tcp =
        strcmp(host, "127.0.0.1") == 0 || strcmp(host, "::1") == 0;
    const bool explicit_socket =
        protocol != nullptr && strcmp(protocol, "local") != 0;

    if ((!use_tcp && strcmp(host, my_localhost) != 0) ||
        (use_tcp && explicit_socket) ||
        (configured_port != 0 &&
         static_cast<uint>(configured_port) != mysqld_port)) {
      return invalid_parameter();
    }

    const char *socket = nullptr;
    uint port = 0;
    uint client_protocol = MYSQL_PROTOCOL_DEFAULT;

    if (use_tcp) {
      if (opt_disable_networking || mysqld_port == 0) {
        return invalid_parameter();
      }
      client_protocol = MYSQL_PROTOCOL_TCP;
      port = mysqld_port;
#ifdef _WIN32
    } else if (explicit_socket) {
      if (!opt_enable_named_pipe || mysqld_unix_port == nullptr ||
          strcmp(protocol, mysqld_unix_port) != 0) {
        return invalid_parameter();
      }
      client_protocol = MYSQL_PROTOCOL_PIPE;
      host = ".";
      socket = mysqld_unix_port;
    } else if (opt_enable_shared_memory) {
      client_protocol = MYSQL_PROTOCOL_MEMORY;
      if (mysql_options(*mysql, MYSQL_SHARED_MEMORY_BASE_NAME,
                        shared_memory_base_name)) {
        return true;
      }
    } else if (opt_enable_named_pipe) {
      client_protocol = MYSQL_PROTOCOL_PIPE;
      host = ".";
      socket = mysqld_unix_port;
    } else if (!opt_disable_networking && mysqld_port != 0) {
      client_protocol = MYSQL_PROTOCOL_TCP;
      host = "127.0.0.1";
      port = mysqld_port;
#else
    } else if (mysqld_unix_port != nullptr && mysqld_unix_port[0] != '\0') {
      if (explicit_socket && strcmp(protocol, mysqld_unix_port) != 0) {
        return invalid_parameter();
      }
      client_protocol = MYSQL_PROTOCOL_SOCKET;
      socket = mysqld_unix_port;
    } else if (!explicit_socket && !opt_disable_networking &&
               mysqld_port != 0) {
      client_protocol = MYSQL_PROTOCOL_TCP;
      host = "127.0.0.1";
      port = mysqld_port;
#endif
    } else {
      return invalid_parameter();
    }

    if (client_protocol == MYSQL_PROTOCOL_TCP) {
      const bool use_ipv4 = strcmp(host, "127.0.0.1") == 0;

      /*
        Accept only a single literal bind value. Re-parsing or resolving the
        configuration here would not prove which sockets were actually opened.
      */
      if (!bind_address_accepts_loopback(my_bind_addr_str, use_ipv4))
        return invalid_parameter();
    }

    if (mysql_options(*mysql, MYSQL_OPT_PROTOCOL, &client_protocol))
      return true;

    if (client_protocol == MYSQL_PROTOCOL_TCP) {
      const bool get_server_public_key = true;
      if (mysql_options(*mysql, MYSQL_OPT_GET_SERVER_PUBLIC_KEY,
                        &get_server_public_key)) {
        return true;
      }
    }

    if ((*mysql)->options.connect_timeout == 0) {
      const uint timeout = static_cast<uint>(connect_timeout);
      if (mysql_options(*mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout))
        return true;
    }

    if ((*mysql)->options.extension != nullptr &&
        (*mysql)->options.extension->ssl_mode == 0) {
      const uint ssl_mode = SSL_MODE_PREFERRED;
      if (mysql_options(*mysql, MYSQL_OPT_SSL_MODE, &ssl_mode)) return true;
    }

    return mysql_real_connect(*mysql, host, user, password, db, port, socket,
                              client_flags) == nullptr;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_reset_connection api to resets the connection to
  clear session state.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::reset, (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /* mysql_reset_connection returns '0' for success */
    if (mysql_reset_connection(m_handle->mysql)) return true;

    mysql_clear_session_track_info(m_handle->mysql);
    m_handle->mysql->server_status &= ~SERVER_SESSION_STATE_CHANGED;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_close api to closes a server connection.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::close, (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle != nullptr) {
      MYSQL *mysql = m_handle->mysql;
      mysql_close(mysql);
      my_free(mysql_h);
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_commit api to commits the transaction.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::commit, (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /* mysql_commit returns '0' for success */
    return mysql_commit(m_handle->mysql) ? true : false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_autocommit api to toggles autocommit mode on/off.

  @param[in] mysql_h A valid mysql object.
  @param[in] mode Sets autocommit mode on if mode is 1, off
          if mode is 0.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::autocommit,
                   (MYSQL_H mysql_h, bool mode)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /* mysql_autocommit returns '0' for success */
    return mysql_autocommit(m_handle->mysql, mode) ? true : false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_rollback api to rolls back the transaction.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::rollback, (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /* mysql_rollback returns '0' for success */
    return mysql_rollback(m_handle->mysql) ? true : false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_options api to sets connect options for connection-establishment
  functions such as real_connect().

  @param[in] mysql_h A valid mysql object.
  @param[in] option The option argument is the option that you
             want to set.
  @param[in] arg The arg argument is the value
             for the option.

--------------+-------------------------------+--------------------------------+
     Type     |     Option                    |Explanation                     |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_CONSUMER_SERVICE |The service (implementation)    |
              |                               |name/prefix to look for in the  |
              |                               |registry and direct all the     |
              |                               |calls to.                       |
--------------+-------------------------------+--------------------------------+
MYSQL_THD     |MYSQL_COMMAND_LOCAL_THD_HANDLE |The THD to run the query in.    |
              |                               |If null a new internal THD will |
              |                               |be created.                     |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_PROTOCOL         |Could be valid socket meaning co|
              |                               |nnect to remote server, could be|
              |                               |"local"(default) meaning connect|
              |                               |to the current server.          |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_USER_NAME        |The user name to send to the    |
              |                               |server/set into the thread's    |
              |                               |security context.               |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_HOST_NAME        |The host name to use to         |
              |                               |connect/set into the thread's   |
              |                               |security context.               |
--------------+-------------------------------+--------------------------------+
int           |MYSQL_COMMAND_TCPIP_PORT       |The port to use to connect.     |
--------------+-------------------------------+--------------------------------+
bool          |MYSQL_NO_LOCK_REGISTRY         |Flag to use the _no_lock        |
              |                               |implementation of registry      |
              |                               |service.                        |
--------------+-------------------------------+--------------------------------+
uint32_t      |MYSQL_COMMAND_CLIENT_FLAGS     |Client flags passed to          |
              |                               |mysql_real_connect() and exposed|
              |                               |by the default capabilities     |
              |                               |consumer.                       |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_PASSWORD         |Password used to authenticate   |
              |                               |against the current server.     |
--------------+-------------------------------+--------------------------------+

  @note For the other mysql client options it calls the mysql_options api from
  this api.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::set,
                   (MYSQL_H mysql_h, int option, const void *arg)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /*
      Replacing the consumer calls its end() method, which may release DOM rows
      still referenced by a mysql_use_result() result.
    */
    if (option == MYSQL_TEXT_CONSUMER_FACTORY &&
        (m_handle->mysql->status != MYSQL_STATUS_READY ||
         (m_handle->mysql->server_status & SERVER_MORE_RESULTS_EXISTS))) {
      set_mysql_error(m_handle->mysql, CR_COMMANDS_OUT_OF_SYNC,
                      unknown_sqlstate);
      return true;
    }

    if (option == MYSQL_COMMAND_PASSWORD &&
        m_handle->connection_mode ==
            mysql_command_connection_mode::kAuthenticationAttempted) {
      set_mysql_error(m_handle->mysql, CR_INVALID_PARAMETER_NO,
                      unknown_sqlstate);
      return true;
    }

    auto mcs_ext = MYSQL_COMMAND_SERVICE_EXTN(m_handle->mysql);
    my_h_service consumer_handle;
    mysql_command_consumer_refs *consumer_refs = nullptr;
    if (option >= MYSQL_TEXT_CONSUMER_FACTORY &&
        option <= MYSQL_TEXT_CONSUMER_CLIENT_CAPABILITIES) {
      if (mcs_ext->command_consumer_services == nullptr) {
        mcs_ext->command_consumer_services = new mysql_command_consumer_refs();
      }
      consumer_refs = reinterpret_cast<mysql_command_consumer_refs *>(
          mcs_ext->command_consumer_services);
    }
    switch (option) {
      case MYSQL_TEXT_CONSUMER_FACTORY: {
        const auto *service_name = static_cast<const char *>(arg);

        if (service_name == nullptr) {
          // default mysql_text_consumer_factory_v1 service
          service_name = k_default_text_consumer_factory_service;
          arg = service_name;
        }
        /*
          Acquire the replacement before releasing the current factory so a
          failed set() leaves the existing factory/context unchanged. If the
          acquisition succeeds, end the cached context with the current factory
          before replacing it.
        */
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        if (mcs_ext->consumer_srv_data != nullptr &&
            consumer_refs->factory_srv != nullptr) {
          consumer_refs->factory_srv->end(mcs_ext->consumer_srv_data);
          mcs_ext->consumer_srv_data = nullptr;
        }

        if (consumer_refs->factory_srv) {
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_factory_v1) *>(
                  consumer_refs->factory_srv)));
        }
        consumer_refs->factory_srv =
            reinterpret_cast<SERVICE_TYPE(mysql_text_consumer_factory_v1) *>(
                consumer_handle);
        mcs_ext->has_custom_consumer_factory =
            (strcmp(service_name, k_default_text_consumer_factory_service) !=
             0);
        break;
      }
      case MYSQL_TEXT_CONSUMER_METADATA: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_metadata_v1 service */
          arg = "mysql_text_consumer_metadata_v1.mysql_server";
        }
        if (consumer_refs->metadata_srv) {
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_metadata_v1) *>(
                  consumer_refs->metadata_srv)));
        }
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->metadata_srv =
            reinterpret_cast<SERVICE_TYPE(mysql_text_consumer_metadata_v1) *>(
                consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_ROW_FACTORY: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_row_factory_v1 service */
          arg = "mysql_text_consumer_row_factory_v1.mysql_server";
        }
        if (consumer_refs->row_factory_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_row_factory_v1) *>(
                  consumer_refs->row_factory_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->row_factory_srv = reinterpret_cast<SERVICE_TYPE(
            mysql_text_consumer_row_factory_v1) *>(consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_ERROR: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_error_v1 service */
          arg = "mysql_text_consumer_error_v1.mysql_server";
        }
        if (consumer_refs->error_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(mysql_text_consumer_error_v1) *>(
                  consumer_refs->error_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->error_srv =
            reinterpret_cast<SERVICE_TYPE(mysql_text_consumer_error_v1) *>(
                consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_NULL: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_null_v1 service */
          arg = "mysql_text_consumer_get_null_v1.mysql_server";
        }
        if (consumer_refs->get_null_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_null_v1) *>(
                  consumer_refs->get_null_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_null_srv =
            reinterpret_cast<SERVICE_TYPE(mysql_text_consumer_get_null_v1) *>(
                consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_INTEGER: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_integer_v1 service */
          arg = "mysql_text_consumer_get_integer_v1.mysql_server";
        }
        if (consumer_refs->get_integer_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_integer_v1) *>(
                  consumer_refs->get_integer_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_integer_srv = reinterpret_cast<SERVICE_TYPE(
            mysql_text_consumer_get_integer_v1) *>(consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_LONGLONG: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_longlong_v1 service */
          arg = "mysql_text_consumer_get_longlong_v1.mysql_server";
        }
        if (consumer_refs->get_longlong_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_longlong_v1) *>(
                  consumer_refs->get_longlong_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_longlong_srv = reinterpret_cast<SERVICE_TYPE(
            mysql_text_consumer_get_longlong_v1) *>(consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_DECIMAL: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_decimal_v1 service */
          arg = "mysql_text_consumer_get_decimal_v1.mysql_server";
        }
        if (consumer_refs->get_decimal_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_decimal_v1) *>(
                  consumer_refs->get_decimal_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_decimal_srv = reinterpret_cast<SERVICE_TYPE(
            mysql_text_consumer_get_decimal_v1) *>(consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_DOUBLE: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_double_v1 service */
          arg = "mysql_text_consumer_get_double_v1.mysql_server";
        }
        if (consumer_refs->get_double_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_double_v1) *>(
                  consumer_refs->get_double_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_double_srv =
            reinterpret_cast<SERVICE_TYPE(mysql_text_consumer_get_double_v1) *>(
                consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_DATE_TIME: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_date_time_v1 service */
          arg = "mysql_text_consumer_get_date_time_v1.mysql_server";
        }
        if (consumer_refs->get_date_time_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_date_time_v1) *>(
                  consumer_refs->get_date_time_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_date_time_srv = reinterpret_cast<SERVICE_TYPE(
            mysql_text_consumer_get_date_time_v1) *>(consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_GET_STRING: {
        if (static_cast<const char *>(arg) == nullptr) {
          /* default mysql_text_consumer_get_string_v1 service */
          arg = "mysql_text_consumer_get_string_v1.mysql_server";
        }
        if (consumer_refs->get_string_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_get_string_v1) *>(
                  consumer_refs->get_string_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->get_string_srv =
            reinterpret_cast<SERVICE_TYPE(mysql_text_consumer_get_string_v1) *>(
                consumer_handle);
        break;
      }
      case MYSQL_TEXT_CONSUMER_CLIENT_CAPABILITIES: {
        const auto *service_name = static_cast<const char *>(arg);
        if (service_name == nullptr) {
          // default mysql_text_consumer_client_capabilities_v1 service
          service_name = k_default_text_consumer_client_capabilities_service;
          arg = service_name;
        }
        // Later code treats a custom capabilities service as an explicit
        // opt-in
        mcs_ext->has_custom_client_capabilities_service =
            (strcmp(service_name,
                    k_default_text_consumer_client_capabilities_service) != 0);
        if (consumer_refs->client_capabilities_srv)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<SERVICE_TYPE_NO_CONST(
                  mysql_text_consumer_client_capabilities_v1) *>(
                  consumer_refs->client_capabilities_srv)));
        if (srv_registry->acquire(static_cast<const char *>(arg),
                                  &consumer_handle)) {
          return true;
        }

        consumer_refs->client_capabilities_srv = reinterpret_cast<SERVICE_TYPE(
            mysql_text_consumer_client_capabilities_v1) *>(consumer_handle);
        break;
      }
      case MYSQL_COMMAND_LOCAL_THD_HANDLE: {
        MYSQL_SESSION mysql_session = nullptr;
        if (static_cast<const char *>(arg) == nullptr) {
          if (mcs_ext->is_thd_associated == true)
            return true;  // already thd assigned
          MYSQL_THD thd;
          const char *host = MYSQL_SYS_HOST;
          const char *user = MYSQL_SESSION_USER;
          const char *db = m_handle->mysql->db;
          my_service<SERVICE_TYPE(mysql_admin_session)> service(
              "mysql_admin_session.mysql_server", srv_registry);
          if (service.is_valid())
            mysql_session = service->open(nullptr, nullptr);
          else
            return true;
          thd = mysql_session->get_thd();
          mcs_ext->is_thd_associated = false;
          Security_context_handle sc;
          if (mysql_security_context_imp::get(thd, &sc)) return true;
          if (mysql_security_context_imp::lookup(sc, user, host, nullptr, db))
            return true;
          mcs_ext->mcs_thd = thd;
          mcs_ext->session_svc = mysql_session;
          m_handle->mysql->thd = thd;
        } else {
          /*
            thd is valid and it is not executing some other SQL statement
            then we can reuse the same.
          */
          mysql_session =
              new Srv_session(&Error_handler::handler, &default_error_h,
                              static_cast<MYSQL_THD>(const_cast<void *>(arg)));
          mcs_ext->is_thd_associated = true;
          mcs_ext->session_svc = mysql_session;
          mcs_ext->mcs_thd = static_cast<MYSQL_THD>(const_cast<void *>(arg));
        }
        break;
      }
      case MYSQL_COMMAND_PROTOCOL:
        if (static_cast<const char *>(arg) == nullptr) {
          arg = "local";  // default value.
        }
        mcs_ext->mcs_protocol = static_cast<const char *>(arg);
        break;
      case MYSQL_COMMAND_USER_NAME:
        if (static_cast<const char *>(arg) == nullptr) {
          mcs_ext->mcs_user_name = MYSQL_SESSION_USER;
        } else {
          mcs_ext->mcs_user_name = static_cast<const char *>(arg);
        }
        break;
      case MYSQL_COMMAND_HOST_NAME:
        if (static_cast<const char *>(arg) == nullptr) {
          mcs_ext->mcs_host_name = MYSQL_SYS_HOST;
        } else {
          mcs_ext->mcs_host_name = static_cast<const char *>(arg);
        }
        break;
      case MYSQL_COMMAND_TCPIP_PORT:
        if (static_cast<const char *>(arg) == nullptr) {
          mcs_ext->mcs_tcpip_port = 0;
        } else {
          mcs_ext->mcs_tcpip_port = *static_cast<const int *>(arg);
        }
        break;
      case MYSQL_COMMAND_CLIENT_FLAGS: {
        const auto *client_flags = static_cast<const uint32_t *>(arg);

        // The default mysql_text_consumer_client_capabilities service reports
        // MYSQL::server_capabilities through its SRV_CTX_H
        if (client_flags == nullptr) {
          mcs_ext->mcs_client_flag = 0;
          m_handle->mysql->server_capabilities = 0;
        } else {
          mcs_ext->mcs_client_flag = *client_flags;

          m_handle->mysql->server_capabilities = *client_flags;
        }
        break;
      }
      case MYSQL_COMMAND_PASSWORD:
        mcs_ext->mcs_password = static_cast<const char *>(arg);
        m_handle->connection_mode =
            arg == nullptr ? mysql_command_connection_mode::kEmbedded
                           : mysql_command_connection_mode::kAuthenticated;
        break;
      case MYSQL_NO_LOCK_REGISTRY:
        mcs_ext->no_lock_registry = static_cast<const bool *>(arg);
        break;
      default:
        if (mysql_options(m_handle->mysql,
                          static_cast<enum mysql_option>(option), arg) != 0)
          return true;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_get_option api to returns the value of a mysql_options() option.

  @param[in] mysql_h A valid mysql object.
  @param[in] option The option argument is the option that you
          want to get.
  @param[out] arg The arg argument is the value
          for the option to store.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::get,
                   (MYSQL_H mysql_h, int option, const void *arg)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr || !arg) return true;
    auto mcs_ext = MYSQL_COMMAND_SERVICE_EXTN(m_handle->mysql);
    switch (option) {
      case MYSQL_COMMAND_CLIENT_FLAGS:
        *(const_cast<uint32_t *>(static_cast<const uint32_t *>(arg))) =
            mcs_ext->mcs_client_flag;
        break;
      case MYSQL_COMMAND_PASSWORD:
        return true;
      case MYSQL_NO_LOCK_REGISTRY:
        *(const_cast<bool *>(static_cast<const bool *>(arg))) =
            mcs_ext->no_lock_registry;
        break;
      default:
        if (mysql_get_option(m_handle->mysql,
                             static_cast<enum mysql_option>(option), arg) != 0)
          return true;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_real_query api to executes an SQL query specified
  as a counted string.

  @param[in] mysql_h A valid mysql object.
  @param[in] stmt_str SQL statement pointed to by the null-terminated
          string. The stmt is in mysql server character set.
  @param[in] length A string length bytes long.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::query,
                   (MYSQL_H mysql_h, const char *stmt_str,
                    unsigned long length)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    return mysql_real_query(m_handle->mysql, stmt_str, length);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_affected_rows api to return the number of rows
  changed/deleted/inserted by the last UPDATE,DELETE or INSERT query.

  @param[in]  mysql_h A valid mysql object.
  @param[out] *rows Number of rows affected, for SELECT stmt it tells
              about number of rows present.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::affected_rows,
                   (MYSQL_H mysql_h, uint64_t *rows)) {
  try {
    int64_t ret;
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    ret = mysql_affected_rows(m_handle->mysql);
    /* mysql_affected_rows returns '-1' in case of error */
    return ((long(*rows = ret) >= 0) ? false : true);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_store_result api to retrieves a complete result set.

  @param[in]  mysql_h A valid mysql object.
  @param[out] *mysql_res An mysql result object to get the result
              set.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::store_result,
                   (MYSQL_H mysql_h, MYSQL_RES_H *mysql_res)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    auto mysql_res_handle = (Mysql_res_handle *)my_malloc(
        key_memory_query_service, sizeof(Mysql_res_handle),
        MYF(MY_WME | MY_ZEROFILL));
    if (mysql_res_handle == nullptr) return true;
    mysql_res_handle->mysql_res = mysql_store_result(m_handle->mysql);
    // null handle indicated no result set was returned
    if (mysql_res_handle->mysql_res == nullptr) {
      my_free(mysql_res_handle);
      *mysql_res = nullptr;
      return true;
    }
    *mysql_res = reinterpret_cast<MYSQL_RES_H>(mysql_res_handle);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_free_result api to frees memory used by a result set.

  @param[in] mysql_res_h An mysql result object to free the result
              set.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::free_result,
                   (MYSQL_RES_H mysql_res_h)) {
  try {
    Mysql_res_handle *res_h = reinterpret_cast<Mysql_res_handle *>(mysql_res_h);
    if (res_h != nullptr) {
      mysql_free_result(res_h->mysql_res);
      my_free(res_h);
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_more_results api to checks whether any more results exist.

  @param[in]  mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::more_results,
                   (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /* mysql_more_results returns true for more results exits. */
    return (mysql_more_results(m_handle->mysql) ? false : true);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_next_result api to returns/initiates the next result
  in multiple-result executions.

  @param[in]  mysql_h A valid mysql object.

  @retval -1 no more results
  @retval >0 error
  @retval 0  if yes more results exits(keep looping)
*/
DEFINE_METHOD(int, mysql_command_services_imp::next_result, (MYSQL_H mysql_h)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    /* more results? -1 = no, >0 = error, 0 = yes (keep looping) */
    const int ret = mysql_next_result(m_handle->mysql);
    return ret;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_result_metadata api to check whether a result set has metadata.

  @param[in]  res_h An mysql result object to get the metadata
              info.
              metadata_info Tells metadata is present or not. 0 for none
              1 for present

  @retval true    failure metadata_info not present.
  @retval false   success metadata_info present.
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::result_metadata,
                   (MYSQL_RES_H res_h)) {
  try {
    Mysql_res_handle *mysql_res_h = reinterpret_cast<Mysql_res_handle *>(res_h);
    if (mysql_res_h == nullptr || mysql_res_h->mysql_res == nullptr) {
      return true;
    }
    return (mysql_result_metadata(mysql_res_h->mysql_res) ? false : true);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/**
  Calls mysql_fetch_row api to fetches the next row from the result set.

  @param[in]  res_h An mysql result object to fetch a row from
              the result set.
  @param[out] *row_h To store the fetched row

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::fetch_row,
                   (MYSQL_RES_H res_h, MYSQL_ROW_H *row_h)) {
  try {
    Mysql_res_handle *mysql_res_h = reinterpret_cast<Mysql_res_handle *>(res_h);
    if (mysql_res_h == nullptr || mysql_res_h->mysql_res == nullptr) {
      return true;
    }
    *row_h = mysql_fetch_row(mysql_res_h->mysql_res);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_fetch_lengths api to Returns the lengths of all columns
  in the current row.

  @param[in]  res_h An mysql result object to fetch a row from
              the result set.
  @param[out] *length lengths of all columns.

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::fetch_lengths,
                   (MYSQL_RES_H res_h, ulong **length)) {
  try {
    Mysql_res_handle *mysql_res_h = reinterpret_cast<Mysql_res_handle *>(res_h);
    if (mysql_res_h == nullptr || mysql_res_h->mysql_res == nullptr) {
      return true;
    }
    *length = mysql_fetch_lengths(mysql_res_h->mysql_res);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_fetch_field api to returns the type of next table field.

  @param[in]  res_h An mysql result object to return the next table
              field.
  @param[out] *field_h Stores the definition of one column of a result
              set as a MYSQL_FIELD structure

  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::fetch_field,
                   (MYSQL_RES_H res_h, MYSQL_FIELD_H *field_h)) {
  try {
    Mysql_res_handle *mysql_res_h = reinterpret_cast<Mysql_res_handle *>(res_h);
    if (mysql_res_h == nullptr || mysql_res_h->mysql_res == nullptr) {
      return true;
    }
    *field_h = reinterpret_cast<MYSQL_FIELD_H>(
        mysql_fetch_field(mysql_res_h->mysql_res));
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_num_fields api to returns the number of columns in a result set.

  @param[in]  res_h A valid mysql result set object.
  @param[out] *num_fields Stores the number of columns in the result set.
  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::num_fields,
                   (MYSQL_RES_H res_h, unsigned int *num_fields)) {
  try {
    Mysql_res_handle *mysql_res_h = reinterpret_cast<Mysql_res_handle *>(res_h);
    if (mysql_res_h == nullptr || mysql_res_h->mysql_res == nullptr) {
      return true;
    }
    *num_fields = mysql_num_fields(mysql_res_h->mysql_res);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_fetch_fields api to returns an array of all field structures.

  @param[in]  res_h A valid mysql result set object.
  @param[out] **fields_h Stores the array of all fields for a result set.
  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::fetch_fields,
                   (MYSQL_RES_H res_h, MYSQL_FIELD_H **fields_h)) {
  try {
    Mysql_res_handle *mysql_res_h = reinterpret_cast<Mysql_res_handle *>(res_h);
    if (mysql_res_h == nullptr || mysql_res_h->mysql_res == nullptr) {
      return true;
    }
    *fields_h = reinterpret_cast<MYSQL_FIELD_H *>(
        mysql_fetch_fields(mysql_res_h->mysql_res));
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_field_count api to returns the number of columns for the most
  resent statement.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *num_fields Stores the number of columns for the last stmt.
  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::field_count,
                   (MYSQL_H mysql_h, unsigned int *num_fields)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr) return true;
    *num_fields = mysql_field_count(m_handle->mysql);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_errno api to return the number of most recently invoked mysql
  function.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *err_no Stores the error number of last mysql function.
  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::sql_errno,
                   (MYSQL_H mysql_h, unsigned int *err_no)) {
  try {
    auto *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr || err_no == nullptr) return true;

    const auto *dom_ctx = get_error_dom_ctx(m_handle->mysql);
    if (dom_ctx == nullptr) {
      *err_no = mysql_errno(m_handle->mysql);
    } else {
      *err_no = static_cast<unsigned int>(dom_ctx->m_sql_errno);
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_error api to return the error message of most recently invoked
  mysql function.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *errmsg Stores the error message of last mysql function.
  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::sql_error,
                   (MYSQL_H mysql_h, char **errmsg)) {
  try {
    auto *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr || errmsg == nullptr || *errmsg == nullptr) {
      return true;
    }

    auto *dom_ctx = get_error_dom_ctx(m_handle->mysql);
    const char *source = (dom_ctx != nullptr) ? dom_ctx->m_err_msg->c_str()
                                              : mysql_error(m_handle->mysql);
    strcpy(*errmsg, source != nullptr ? source : "");
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

/**
  Calls mysql_sqlstate api to return the SQLSTATE error code for the last error.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *sqlstate_errmsg Stores the SQLSTATE error message of the most
              recently executed SQL stmt.
  @retval true    failure
  @retval false   success
*/
DEFINE_BOOL_METHOD(mysql_command_services_imp::sql_state,
                   (MYSQL_H mysql_h, char **sqlstate_errmsg)) {
  try {
    Mysql_handle *m_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (m_handle == nullptr || sqlstate_errmsg == nullptr) return true;

    auto *dom_ctx = get_error_dom_ctx(m_handle->mysql);
    *sqlstate_errmsg = const_cast<char *>(
        (dom_ctx != nullptr) ? dom_ctx->m_sqlstate->c_str()
                             : mysql_sqlstate(m_handle->mysql));
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_services_imp::field_metadata_get,
                   (MYSQL_FIELD_H field_h, int metadata, void *data))
try {
  MYSQL_FIELD *fld = reinterpret_cast<MYSQL_FIELD *>(field_h);

  switch (metadata) {
    case MYSQL_COMMAND_FIELD_METADATA_NAME:
      *reinterpret_cast<const char **>(data) = fld->name;
      return false;
    case MYSQL_COMMAND_FIELD_METADATA_TABLE_NAME:
      *reinterpret_cast<const char **>(data) = fld->table;
      return false;
    case MYSQL_COMMAND_FIELD_METADATA_TABLE_DB_NAME:
      *reinterpret_cast<const char **>(data) = fld->db;
      return false;
  }
  return true;
} catch (...) {
  mysql_components_handle_std_exception(__func__);
  return true;
}
