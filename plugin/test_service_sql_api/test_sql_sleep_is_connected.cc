/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

#define LOG_COMPONENT_TAG "test_session_is_connected"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#include <cstdint>

#include "mysql/components/my_service.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/plugin.h"
#include "mysql/service_srv_session_info.h"

#include "helper/test_context.h"

#include "my_dbug.h"       // NOLINT(build/include_subdir)
#include "my_inttypes.h"   // NOLINT(build/include_subdir)
#include "mysqld_error.h"  // NOLINT(build/include_subdir)

static Test_context *test_context = nullptr;

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static void ensure_api_ok(const char *function, int result) {
  if (result != 0) {
    test_context->log_test_line("ERROR calling %s: returned %i\n", function,
                                result);
  }
}

static void ensure_api_ok(const char *function, MYSQL_SESSION result) {
  if (result == nullptr) {
    test_context->log_test_line("ERROR calling ", function, ": returned ",
                                reinterpret_cast<uintptr_t>(result), "\n");
  }
}

#define ENSURE_API_OK(call) ensure_api_ok(__FUNCTION__, (call));

struct Callback_data {
  bool limit_is_connected{false};
  int is_connected_calls{0};
  int handle_ok_calls{0};
};

static int sql_start_result_metadata(void *, uint, uint,
                                     const CHARSET_INFO *
#ifndef NDEBUG
                                         resultcs
#endif
) {
  DBUG_ENTER("sql_start_result_metadata");
  DBUG_PRINT("info", ("resultcs->number: %d", resultcs->number));
  DBUG_PRINT("info", ("resultcs->csname: %s", resultcs->csname));
  DBUG_PRINT("info", ("resultcs->m_coll_name: %s", resultcs->m_coll_name));
  DBUG_RETURN(false);
}

static int sql_field_metadata(void *, struct st_send_field *field,
                              const CHARSET_INFO *) {
  DBUG_ENTER("sql_field_metadata");
  DBUG_PRINT("info", ("field->db_name: %s", field->db_name));
  DBUG_PRINT("info", ("field->table_name: %s", field->table_name));
  DBUG_PRINT("info", ("field->org_table_name: %s", field->org_table_name));
  DBUG_PRINT("info", ("field->col_name: %s", field->col_name));
  DBUG_PRINT("info", ("field->org_col_name: %s", field->org_col_name));
  DBUG_PRINT("info", ("field->length: %d", static_cast<int>(field->length)));
  DBUG_PRINT("info",
             ("field->charsetnr: %d", static_cast<int>(field->charsetnr)));
  DBUG_PRINT("info", ("field->flags: %d", static_cast<int>(field->flags)));
  DBUG_PRINT("info",
             ("field->decimals: %d", static_cast<int>(field->decimals)));
  DBUG_PRINT("info", ("field->type: %d", static_cast<int>(field->type)));
  test_context->log_test_line(" > sql_field_metadata: ", field->col_name);
  DBUG_RETURN(false);
}

static int sql_end_result_metadata(void *, uint, uint) {
  DBUG_ENTER("sql_end_result_metadata");
  DBUG_RETURN(false);
}

static int sql_start_row(void *) {
  DBUG_ENTER("sql_start_row");
  DBUG_RETURN(false);
}

static int sql_end_row(void *) {
  DBUG_ENTER("sql_end_row");
  DBUG_RETURN(false);
}

static void sql_abort_row(void *) {
  DBUG_ENTER("sql_abort_row");
  DBUG_VOID_RETURN;
}

static ulong sql_get_client_capabilities(void *) {
  DBUG_ENTER("sql_get_client_capabilities");
  DBUG_RETURN(0);
}

static int sql_get_null(void *) {
  DBUG_ENTER("sql_get_null");
  test_context->log_test_line(" > sql_get_null");
  DBUG_RETURN(false);
}

static int sql_get_integer(void *, longlong) {
  DBUG_ENTER("sql_get_integer");
  test_context->log_test_line(" > sql_get_integer");
  DBUG_RETURN(false);
}

static int sql_get_longlong(void *, longlong, uint) {
  DBUG_ENTER("sql_get_longlong");
  test_context->log_test_line(" > sql_get_longlong");
  DBUG_RETURN(false);
}

static int sql_get_decimal(void *, const decimal_t *) {
  DBUG_ENTER("sql_get_decimal");
  test_context->log_test_line(" > sql_get_decimal");
  DBUG_RETURN(false);
}

static int sql_get_double(void *, double, uint32) {
  DBUG_ENTER("sql_get_double");
  test_context->log_test_line(" > sql_get_double");
  DBUG_RETURN(false);
}

static int sql_get_date(void *, const MYSQL_TIME *) {
  DBUG_ENTER("sql_get_date");
  test_context->log_test_line(" > sql_get_date");
  DBUG_RETURN(false);
}

static int sql_get_time(void *, const MYSQL_TIME *, uint) {
  DBUG_ENTER("sql_get_time");
  test_context->log_test_line(" > sql_get_time");
  DBUG_RETURN(false);
}

static int sql_get_datetime(void *, const MYSQL_TIME *, uint) {
  DBUG_ENTER("sql_get_datetime");
  test_context->log_test_line(" > sql_get_datetime");
  DBUG_RETURN(false);
}

static int sql_get_string(void *, const char *const, size_t,
                          const CHARSET_INFO *const) {
  DBUG_ENTER("sql_get_string");

  test_context->log_test_line(" > sql_get_string");

  DBUG_RETURN(false);
}

static void sql_handle_ok(void *ctx, uint, uint, ulonglong, ulonglong,
                          const char *const) {
  DBUG_ENTER("sql_handle_ok");
  Callback_data *cbd = static_cast<Callback_data *>(ctx);
  cbd->handle_ok_calls++;

  test_context->log_test_line(" > sql_handle_ok");

  DBUG_VOID_RETURN;
}

static void sql_handle_error(void *, uint sql_errno, const char *const err_msg,
                             const char *const) {
  DBUG_ENTER("sql_handle_error");
  test_context->log_test_line(" > sql_handle_error: ", sql_errno, err_msg);
  DBUG_VOID_RETURN;
}

static void sql_shutdown(void *, int shutdown_server) {
  DBUG_ENTER("sql_shutdown");
  test_context->log_test_line(" > sql_shutdown: ", shutdown_server);

  DBUG_VOID_RETURN;
}

static bool sql_connection_alive(void *ctx) {
  Callback_data *cbd = static_cast<Callback_data *>(ctx);

  if (cbd->limit_is_connected) {
    // Connection is disconnected
    // after concrete number of calls
    cbd->is_connected_calls--;
  } else {
    // Connection is always alive
    // just count number to calls
    cbd->is_connected_calls++;
  }

  const bool result = cbd->is_connected_calls > 0;

  // We can't log each call to is_connection_alive,
  // because on slower/overloaded machines it might log different
  // number of calls.
  //
  // Thus lets log following line in case of limited number of calls.
  if (cbd->limit_is_connected) {
    test_context->log_test_line(" > sql_connection_alive => returns ",
                                result ? "true" : "false");
  }

  return result;
}

const struct st_command_service_cbs sql_cbs = {sql_start_result_metadata,
                                               sql_field_metadata,
                                               sql_end_result_metadata,
                                               sql_start_row,
                                               sql_end_row,
                                               sql_abort_row,
                                               sql_get_client_capabilities,
                                               sql_get_null,
                                               sql_get_integer,
                                               sql_get_longlong,
                                               sql_get_decimal,
                                               sql_get_double,
                                               sql_get_date,
                                               sql_get_time,
                                               sql_get_datetime,
                                               sql_get_string,
                                               sql_handle_ok,
                                               sql_handle_error,
                                               sql_shutdown,
                                               sql_connection_alive};

static void run_cmd(MYSQL_SESSION session, const std::string &query,
                    Callback_data *ctxt) {
  test_context->log_test_line("Execute: ", query);

  COM_DATA com = {};

  com.com_query.query = query.c_str();
  com.com_query.length = query.length();

  int fail = command_service_run_command(
      session, COM_QUERY, &com, &my_charset_utf8mb3_general_ci, &sql_cbs,
      CS_TEXT_REPRESENTATION, ctxt);
  if (fail) {
    test_context->log_error("run_statement code: ", fail);

    return;
  }
}

/******************************************************************************/

struct Test_data {
  void *p{nullptr};
  MYSQL_SESSION session{nullptr};
};

const int k_is_connected_unlimited = -1;

static void test_sql_is_connected_enusre_is_called(const int number_of_calls,
                                                   const int sleep_period,
                                                   const Test_data *test_data) {
  const bool is_limited = k_is_connected_unlimited != number_of_calls;
  Callback_data ctxt;
  test_context->separator('-');

  if (is_limited) {
    ctxt.limit_is_connected = true;
    ctxt.is_connected_calls = number_of_calls;

    test_context->log_test_line(
        "Test interaction between `sleep` and `is_connected`, ",
        "in case when connection break at ", number_of_calls,
        " call to is_connected");
  } else {
    test_context->log_test_line("Test sleep and is_connected interactions, ",
                                "in case when connection never breaks");
  }

  std::string query;
  query.append("SELECT SLEEP(")
      .append(std::to_string(sleep_period))
      .append(");");
  run_cmd(test_data->session, query, &ctxt);

  if (is_limited) {
    if (0 != ctxt.is_connected_calls) {
      test_context->log_test_line(
          "ERROR: Is_connected wasn't called expected number of times, "
          "called: ",
          number_of_calls - ctxt.is_connected_calls,
          ", expected: ", number_of_calls);
      test_context->log_error(
          "The method 'sql_is_connected' should, be called exactly ",
          number_of_calls, " times and after that break the sleep.",
          "Still it was called ", number_of_calls - ctxt.is_connected_calls,
          " times.");
    }
  } else {
    if (0 == ctxt.is_connected_calls) {
      test_context->log_test_line(
          "ERROR: Is_connected wasn't at all ! The test expects at least one "
          "call");
      test_context->log_error(
          "The method sql_is_connected wasn't called, it should be called at "
          "least once.");
      return;
    }

    test_context->log_test_line(" > is_connected was called several times.");
  }
}

static void test_sql_is_connected(void *plugin_ctx) {
  const int k_call_allowed_once = 1;
  const int k_call_allowed_twice = 2;
  const int k_call_allowed_four_times = 4;
  const int k_sleep_for_1_hour = 60 * 60;
  const int k_sleep_for_20_seconds = 20;
  Test_data test_data;
  DBUG_ENTER("test_sql");

  test_context->separator();
  test_context->log_test_line("Opening Session");
  ENSURE_API_OK(test_data.session = srv_session_open(nullptr, plugin_ctx));

  test_sql_is_connected_enusre_is_called(k_call_allowed_once,
                                         k_sleep_for_1_hour, &test_data);
  test_sql_is_connected_enusre_is_called(k_call_allowed_twice,
                                         k_sleep_for_1_hour, &test_data);
  test_sql_is_connected_enusre_is_called(k_call_allowed_four_times,
                                         k_sleep_for_1_hour, &test_data);
  test_sql_is_connected_enusre_is_called(k_is_connected_unlimited,
                                         k_sleep_for_20_seconds, &test_data);

  test_context->separator();
  test_context->log_test_line("Close Session");
  ENSURE_API_OK(srv_session_close(test_data.session));

  DBUG_VOID_RETURN;
}

static int test_session_plugin_init(void *plugin_ctx) {
  DBUG_ENTER("test_sql_service_plugin_init");

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs))
    DBUG_RETURN(1);
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  test_context = new Test_context("test_sql_is_connected", plugin_ctx);

  /* Test of service: sql */
  test_sql_is_connected(plugin_ctx);

  DBUG_RETURN(0);
}

static int test_session_plugin_deinit(void *) {
  DBUG_ENTER("test_sql_service_plugin_deinit");
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");

  delete test_context;
  test_context = nullptr;
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  DBUG_RETURN(0);
}

struct st_mysql_daemon test_session_plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_session_plugin,
    "test_sql_sleep_is_connected",
    "Lukasz Kotula",
    "Test sql service commands",
    PLUGIN_LICENSE_GPL,
    test_session_plugin_init,   /* Plugin Init */
    nullptr,                    /* Plugin Check uninstall */
    test_session_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    nullptr, /* status variables                */
    nullptr, /* system variables                */
    nullptr, /* config options                  */
    0,       /* flags                           */
} mysql_declare_plugin_end;
