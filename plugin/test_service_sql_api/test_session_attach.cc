/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include <array>
#include <utility>

#define LOG_COMPONENT_TAG "test_session_attach"

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdlib.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql_com.h"

#include "plugin/test_service_sql_api/helper/test_context.h"

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static Test_context *test_context{nullptr};

static MYSQL_THDVAR_INT(var_int, PLUGIN_VAR_OPCMDARG, "Test variable", nullptr,
                        nullptr, 0, 0, 2147483, 0);

static int expected_session_variable_value(const int session_index) {
  return (session_index + 1) * 10;
}

static void handle_log_error(void *, uint sql_errno, const char *err_msg,
                             const char *) {
  test_context->log_test_line("SQL execution failed with ", sql_errno,
                              " error and message: ", err_msg);
}

const struct st_command_service_cbs sql_cbs = {
    nullptr,           // sql_start_result_metadata,
    nullptr,           // sql_field_metadata,
    nullptr,           // sql_end_result_metadata,
    nullptr,           // sql_start_row,
    nullptr,           // sql_end_row,
    nullptr,           // sql_abort_row,
    nullptr,           // sql_get_client_capabilities,
    nullptr,           // sql_get_null,
    nullptr,           // sql_get_integer,
    nullptr,           // sql_get_longlong,
    nullptr,           // sql_get_decimal,
    nullptr,           // sql_get_double,
    nullptr,           // sql_get_date,
    nullptr,           // sql_get_time,
    nullptr,           // sql_get_datetime,
    nullptr,           // sql_get_string,
    nullptr,           // sql_handle_ok,
    handle_log_error,  // sql_handle_error,
    nullptr,           // sql_shutdown,
    nullptr            // sql_alive
};

static void exec_test_cmd(MYSQL_SESSION session, const char *test_cmd) {
  COM_DATA cmd;

  test_context->log_test_line(test_cmd);

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = test_cmd;
  cmd.com_query.length = strlen(cmd.com_query.query);

  const bool failed =
      0 != command_service_run_command(session, COM_QUERY, &cmd,
                                       &my_charset_utf8mb3_general_ci, &sql_cbs,
                                       CS_BINARY_REPRESENTATION, nullptr);

  if (failed) {
    test_context->log_error("exec_test_cmd: ret code: ", failed);
  }
}

static void test_sql() {
  DBUG_TRACE;

  constexpr int number_of_sessions = 10;
  std::array<MYSQL_SESSION, number_of_sessions> sessions;

  /* Open Sessions */
  for (int i = 0; i < number_of_sessions; ++i) {
    sessions[i] = srv_session_open(nullptr, nullptr);
    test_context->log_test_line("Opening Session ", i + 1);

    if (!sessions[i]) {
      test_context->log_test_line("Opening Session ", i + 1, " failed.");
      test_context->log_error("Open Session failed.");
    }
  }

  test_context->separator();

  for (uint i = 0; i < number_of_sessions; i++) {
    const int buffer_size = 256;
    std::string buffer(buffer_size, '\0');

    auto session_id_text = std::to_string(i + 1);

    if (1 == session_id_text.length()) session_id_text.insert(0, "0");

    test_context->log_test("\nQuery ", session_id_text, ": ");

    snprintf(&(buffer[0]), buffer.length(),
             "SET SESSION test_session_attach_var_int = %i;",
             expected_session_variable_value(i));

    exec_test_cmd(sessions[i], buffer.c_str());
  }

  test_context->separator();

  /* Verify Sessions variables in another
     sequence than "set" was done */
  for (int i = 0; i < number_of_sessions; ++i) {
    const int session_offset = 5;
    /* Go in reverse direction, starting from offset */
    const int session_index =
        number_of_sessions - ((i + session_offset) % number_of_sessions) - 1;

    test_context->log_test_line("Attach Session ", i + 1);

    if (srv_session_attach(sessions[session_index], nullptr)) {
      test_context->log_test_line("Attach Session ", i + 1, " failed.");

      continue;
    }

    test_context->log_test_line("Verify Session ", i + 1, " variable");
    auto session_thd = srv_session_info_get_thd(sessions[session_index]);

    if (expected_session_variable_value(session_index) !=
        THDVAR(session_thd, var_int)) {
      test_context->log_test_line("Verify Session ", i + 1,
                                  " variable failed, actual value is ",
                                  THDVAR(session_thd, var_int));
      test_context->log_error("Verify Session variable failed.");
    }

    test_context->log_test_line("Detach Session ", i + 1);

    if (srv_session_detach(sessions[session_index])) {
      test_context->log_test_line("Detach Session ", i + 1, " failed.");
      test_context->log_error("Detach Session failed.");

      continue;
    }
  }

  test_context->separator();

  /* Close Sessions */
  for (int i = 0; i < number_of_sessions; ++i) {
    auto result = srv_session_close(sessions[i]);
    test_context->log_test_line("Close Session ", i + 1);

    if (result) {
      test_context->log_test_line("Close Session ", i + 1, " failed.");
      test_context->log_error("Close Session failed.");
    }
  }

  test_context->log_test_line("Closed all sessions");
}

struct test_thread_context {
  my_thread_handle thread;

  bool thread_finished;
  void (*test_function)();
};

static void *test_sql_threaded_wrapper(void *param) {
  struct test_thread_context *thread_context =
      (struct test_thread_context *)param;

  test_context->separator();
  test_context->log_test_line("init thread");
  if (srv_session_init_thread(test_context->get_plugin_handler()))
    test_context->log_error("srv_session_init_thread failed.");

  thread_context->test_function();

  test_context->log_test_line("deinit thread");
  srv_session_deinit_thread();

  thread_context->thread_finished = true;
  return nullptr;
}

static void test_in_spawned_thread(void (*test_function)()) {
  my_thread_attr_t attr; /* Thread attributes */
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  struct test_thread_context thread_context;

  thread_context.thread_finished = false;
  thread_context.test_function = test_function;

  /* now create the thread and call test_session within the thread. */
  if (my_thread_create(&(thread_context.thread), &attr,
                       test_sql_threaded_wrapper, &thread_context) != 0) {
    test_context->log_error("Could not create test session thread");
  } else {
    my_thread_join(&thread_context.thread, nullptr);
  }
}

extern "C" {

bool execute_test_init(UDF_INIT *, UDF_ARGS *, char *);

long long execute_test(UDF_INIT *, UDF_ARGS *, unsigned char *,
                       unsigned char *);
}

/*
  UDF responsible for starting the test-case

  Test case must be run after plugin was installed.
  Plugin API while installing plugin leaves system variables half
  initialized , which can't be used in test_sql_service_plugin_init.
*/
long long execute_test(UDF_INIT *, UDF_ARGS *, unsigned char *,
                       unsigned char *) {
  test_context->separator();
  test_context->log_test_line(
      "Test in a server thread. Attach must fail on non srv_session thread.");

  test_sql();

  /* Test in a new thread */
  test_context->log_test_line("Follows threaded run. Successful scenario.");

  test_in_spawned_thread(test_sql);

  return 0;
}

/*
  UDF initialization function
*/
bool execute_test_init(UDF_INIT *, UDF_ARGS *, char *error_message_buffer) {
  if (nullptr == test_context) {
    snprintf(error_message_buffer, MYSQL_ERRMSG_SIZE,
             "Daemon plugin was not installed.");

    return true;
  }

  return false;
}

/*
  Plugin initialization function

  Required for session variable registration.
*/
static int test_sql_service_plugin_init(void *p) {
  DBUG_TRACE;

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  test_context = new Test_context("test_session_attach", p);

  return 0;
}

/*
  Plugin deinitialization function
*/
static int test_sql_service_plugin_deinit(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");

  delete test_context;
  test_context = nullptr;
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  return 0;
}

static SYS_VAR *plugin_system_variables[] = {MYSQL_SYSVAR(var_int), nullptr};

struct st_mysql_daemon test_sql_service_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/
mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_sql_service_plugin,
    "test_session_attach",
    PLUGIN_AUTHOR_ORACLE,
    "Test session THDVAR",
    PLUGIN_LICENSE_GPL,
    test_sql_service_plugin_init,   /* Plugin Init      */
    nullptr,                        /* Plugin Check uninstall    */
    test_sql_service_plugin_deinit, /* Plugin Deinit    */
    0x0100,                         /* 1.0              */
    nullptr,                        /* status variables */
    plugin_system_variables,        /* system variables */
    nullptr,                        /* config options   */
    0,                              /* flags            */
} mysql_declare_plugin_end;
