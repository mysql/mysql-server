/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_command_consumer.h>
#include <mysql/components/services/mysql_command_services.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/security_context.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/plugin.h>
#include "include/mysql.h"

REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_account_database_security_context_lookup) =
    nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(udf_registration) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_command_factory) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_command_options) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_command_query) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_command_query_result) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_command_field_info) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(mysql_command_error_info) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(registry) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(log_builtins) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string) = nullptr;

MYSQL_H mysql_h = nullptr;
static bool udf_registered = false;
#define MYSQL_SESSION_USER "mysql.session"
#define MYSQL_HOST "localhost"

static char *test_command_service_udf(UDF_INIT *, UDF_ARGS *args, char *result,
                                      unsigned long *length, unsigned char *,
                                      unsigned char *error) {
  MYSQL_RES_H mysql_res = nullptr;
  MYSQL_ROW_H row = nullptr;
  uint64_t row_count = 0;
  unsigned int num_column = 0;
  std::string result_set;
  MYSQL_H mysql_h = nullptr;

  *error = 1;
  if (args->arg_count == 0) {
    return nullptr;
  }

  std::string query(args->args[0], args->lengths[0]);

  // Query by attaching to the THD to the session service.
  MYSQL_SESSION mysql_session = srv_session_open(NULL, NULL);
  auto loc_thd = srv_session_info_get_thd(mysql_session);
  if (loc_thd) {
    // Fill the secruity context of session we got.
    Security_context_handle sctx;
    if (mysql_service_mysql_thd_security_context->get(loc_thd, &sctx)) {
      goto session_err;
    }
    if (mysql_service_mysql_account_database_security_context_lookup->lookup(
            sctx, MYSQL_SESSION_USER, MYSQL_HOST, nullptr, nullptr)) {
      goto session_err;
    }
  }

  if (mysql_service_mysql_command_factory->init(&mysql_h)) {
    goto session_err;
  }

  if (loc_thd && mysql_service_mysql_command_options->set(
                     mysql_h, MYSQL_COMMAND_LOCAL_THD_HANDLE, loc_thd)) {
    goto session_err;
  }

  if (mysql_service_mysql_command_factory->connect(mysql_h)) {
    goto session_err;
  }

  /* setting the mysql_text_consumer_factory into the mysql object */
  mysql_service_mysql_command_options->set(mysql_h, MYSQL_TEXT_CONSUMER_FACTORY,
                                           nullptr);

  /* setting the mysql_text_consumer_metadata into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_METADATA, nullptr);

  /* setting the mysql_text_consumer_row_factory into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_ROW_FACTORY, nullptr);

  /* setting the mysql_text_consumer_error into the mysql object */
  mysql_service_mysql_command_options->set(mysql_h, MYSQL_TEXT_CONSUMER_ERROR,
                                           nullptr);

  /* setting the mysql_text_consumer_get_null into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_NULL, nullptr);

  /* setting the mysql_text_consumer_get_integer into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_INTEGER, nullptr);

  /* setting the mysql_text_consumer_get_longlong into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_LONGLONG, nullptr);

  /* setting the mysql_text_consumer_get_decimal into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_DECIMAL, nullptr);

  /* setting the mysql_text_consumer_get_double into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_DOUBLE, nullptr);

  /* setting the mysql_text_consumer_get_Data_time into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_DATE_TIME, nullptr);

  /* setting the mysql_text_consumer_get_string into the mysql object */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_GET_STRING, nullptr);

  /* setting the mysql_text_consumer_client_capabilities into the mysql object
   */
  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_TEXT_CONSUMER_CLIENT_CAPABILITIES, nullptr);

  /* setting the default protocol(local) into the mysql object */
  mysql_service_mysql_command_options->set(mysql_h, MYSQL_COMMAND_PROTOCOL,
                                           nullptr);

  /* setting the default user name (MYSQL_SESSION_USER) into the mysql object */
  mysql_service_mysql_command_options->set(mysql_h, MYSQL_COMMAND_USER_NAME,
                                           nullptr);

  /* setting the default host name (MYSQL_SYS_HOST) into the mysql object */
  mysql_service_mysql_command_options->set(mysql_h, MYSQL_COMMAND_HOST_NAME,
                                           nullptr);

  /* setting the default TCP/IP port number (0) into the mysql object */
  mysql_service_mysql_command_options->set(mysql_h, MYSQL_COMMAND_TCPIP_PORT,
                                           nullptr);

  mysql_service_mysql_command_options->set(
      mysql_h, MYSQL_COMMAND_LOCAL_THD_HANDLE, nullptr);

  if (mysql_service_mysql_command_query->query(mysql_h, query.data(),
                                               query.length())) {
    mysql_service_mysql_command_factory->close(mysql_h);
    goto session_err;
  }

  mysql_service_mysql_command_query_result->store_result(mysql_h, &mysql_res);
  if (mysql_res) {
    if (mysql_service_mysql_command_query->affected_rows(mysql_h, &row_count)) {
      goto err;
    }
    if (mysql_service_mysql_command_field_info->num_fields(mysql_res,
                                                           &num_column)) {
      goto err;
    }

    for (uint64_t i = 0; i < row_count; i++) {
      if (mysql_service_mysql_command_query_result->fetch_row(mysql_res,
                                                              &row)) {
        goto err;
      }
      ulong *length = nullptr;
      mysql_service_mysql_command_query_result->fetch_lengths(mysql_res,
                                                              &length);
      for (unsigned int j = 0; j < num_column; j++) {
        result_set += row[j];
      }
    }
    /* The caller has the buffer limit, and the size is of MAX_FIELD_WIDTH size
       so we are truncating the result of the query output if it has more date
    */
    strncpy(
        result,
        reinterpret_cast<char *>(const_cast<char *>(result_set.c_str())),
        (result_set.length() < *length) ? result_set.length() : (*length - 1));
    *length =
        (result_set.length() < *length) ? result_set.length() : (*length - 1);
    result[*length] = '\0';
  }
err:
  *error = 0;
  mysql_service_mysql_command_query_result->free_result(mysql_res);
  mysql_service_mysql_command_factory->close(mysql_h);
session_err:
  if (mysql_session) srv_session_close(mysql_session);
  mysql_session = nullptr;
  return (*error) ? nullptr : result;
}

static int plugin_deinit(void *p);

static int plugin_init(void *p) {
  int rc = 0;
  my_h_service h_thd_security_ctx_srv = nullptr;
  my_h_service h_security_ctx_srv = nullptr;
  my_h_service h_command_factory_srv = nullptr;
  my_h_service h_command_srv = nullptr;

  if (init_logging_service_for_plugin(&mysql_service_registry,
                                      &mysql_service_log_builtins,
                                      &mysql_service_log_builtins_string))
    return 1;

  if (mysql_service_registry->acquire("mysql_thd_security_context",
                                      &h_thd_security_ctx_srv))
    rc = 1;
  else
    mysql_service_mysql_thd_security_context =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_thd_security_context) *>(
            h_thd_security_ctx_srv);

  if (mysql_service_registry->acquire_related(
          "mysql_account_database_security_context_lookup",
          h_thd_security_ctx_srv, &h_security_ctx_srv))
    rc = 1;
  else
    mysql_service_mysql_account_database_security_context_lookup =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(
            mysql_account_database_security_context_lookup) *>(
            h_security_ctx_srv);

  if (mysql_service_registry->acquire_related("mysql_security_context_options",
                                              h_thd_security_ctx_srv,
                                              &h_security_ctx_srv))
    rc = 1;
  else
    mysql_service_mysql_security_context_options =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(
            mysql_security_context_options) *>(h_security_ctx_srv);

  if (mysql_service_registry->acquire("mysql_command_factory",
                                      &h_command_factory_srv))
    rc = 1;
  else
    mysql_service_mysql_command_factory =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_command_factory) *>(
            h_command_factory_srv);

  if (mysql_service_registry->acquire_related(
          "mysql_command_options", h_command_factory_srv, &h_command_srv))
    rc = 1;
  else
    mysql_service_mysql_command_options =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_command_options) *>(
            h_command_srv);

  if (mysql_service_registry->acquire_related(
          "mysql_command_query", h_command_factory_srv, &h_command_srv))
    rc = 1;
  else
    mysql_service_mysql_command_query =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_command_query) *>(
            h_command_srv);

  if (mysql_service_registry->acquire_related(
          "mysql_command_query_result", h_command_factory_srv, &h_command_srv))
    rc = 1;
  else
    mysql_service_mysql_command_query_result =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_command_query_result) *>(
            h_command_srv);

  if (mysql_service_registry->acquire_related(
          "mysql_command_field_info", h_command_factory_srv, &h_command_srv))
    rc = 1;
  else
    mysql_service_mysql_command_field_info =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_command_field_info) *>(
            h_command_srv);

  if (mysql_service_registry->acquire_related(
          "mysql_command_error_info", h_command_factory_srv, &h_command_srv))
    rc = 1;
  else
    mysql_service_mysql_command_error_info =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_command_error_info) *>(
            h_command_srv);

  if (mysql_service_registry->acquire("udf_registration", &h_command_srv))
    rc = 1;
  else
    mysql_service_udf_registration =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(udf_registration) *>(
            h_command_srv);

  if (mysql_service_udf_registration) {
    if (mysql_service_udf_registration->udf_register(
            "test_command_service_udf", STRING_RESULT,
            reinterpret_cast<Udf_func_any>(test_command_service_udf), nullptr,
            nullptr))
      rc = 1;
    else
      udf_registered = true;
  }

  if (rc) plugin_deinit(p);
  return rc;
}

static int plugin_deinit(void *) {
  if (mysql_service_mysql_thd_security_context)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_thd_security_context) *>(
            mysql_service_mysql_thd_security_context)));
  if (mysql_service_mysql_account_database_security_context_lookup)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(
            mysql_account_database_security_context_lookup) *>(
            mysql_service_mysql_account_database_security_context_lookup)));
  if (mysql_service_mysql_security_context_options)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_security_context_options) *>(
            mysql_service_mysql_security_context_options)));
  if (mysql_service_mysql_command_factory)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_command_factory) *>(
            mysql_service_mysql_command_factory)));
  if (mysql_service_mysql_command_options)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_command_options) *>(
            mysql_service_mysql_command_options)));
  if (mysql_service_mysql_command_query)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_command_query) *>(
            mysql_service_mysql_command_query)));
  if (mysql_service_mysql_command_query_result)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_command_query_result) *>(
            mysql_service_mysql_command_query_result)));
  if (mysql_service_mysql_command_field_info)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_command_field_info) *>(
            mysql_service_mysql_command_field_info)));
  if (mysql_service_mysql_command_error_info)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(mysql_command_error_info) *>(
            mysql_service_mysql_command_error_info)));
  if (mysql_service_udf_registration) {
    if (udf_registered)
      mysql_service_udf_registration->udf_unregister("test_command_service_udf",
                                                     nullptr);
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(udf_registration) *>(
            mysql_service_udf_registration)));
  }
  deinit_logging_service_for_plugin(&mysql_service_registry,
                                    &mysql_service_log_builtins,
                                    &mysql_service_log_builtins_string);
  return 0;
}

static struct st_mysql_daemon plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(test_services_command_services){
    MYSQL_DAEMON_PLUGIN,
    &plugin,
    "test_services_command_services",
    PLUGIN_AUTHOR_ORACLE,
    "test the plugin command services",
    PLUGIN_LICENSE_GPL,
    plugin_init,   /* Plugin Init */
    nullptr,       /* Plugin Check uninstall */
    plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    nullptr, /* status variables                */
    nullptr, /* system variables                */
    nullptr, /* config options                  */
    0,       /* flags                           */
} mysql_declare_plugin_end;
