/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#define LOG_COMPONENT_TAG "test_x_sessions_init"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdlib.h>
#include <sys/types.h>

#include "mysql/components/my_service.h"
#include "mysql/components/services/log_builtins.h"

#include "my_dbug.h"         // NOLINT(build/include_subdir)
#include "my_inttypes.h"     // NOLINT(build/include_subdir)
#include "my_io.h"           // NOLINT(build/include_subdir)
#include "my_sys.h"          // NOLINT(build/include_subdir)
#include "mysqld_error.h"    // NOLINT(build/include_subdir)
#include "template_utils.h"  // NOLINT(build/include_subdir)

static const char *log_filename = "test_x_sessions_init";

#define MAX_SESSIONS 128

#define STRING_BUFFER_SIZE 512

#define WRITE_STR(format)                         \
  snprintf(buffer, sizeof(buffer), "%s", format); \
  my_write(outfile, reinterpret_cast<uchar *>(buffer), strlen(buffer), MYF(0))

#define WRITE_VAL(format, value)                   \
  snprintf(buffer, sizeof(buffer), format, value); \
  my_write(outfile, reinterpret_cast<uchar *>(buffer), strlen(buffer), MYF(0))

static const char *sep =
    "========================================================================"
    "\n";

#define WRITE_SEP() \
  my_write(outfile, pointer_cast<const uchar *>(sep), strlen(sep), MYF(0))

/* SQL (system) variable to control number of sessions                    */
/* Only effective at start od mysqld by setting it as option --loose-...  */
int nb_sessions;
static MYSQL_SYSVAR_INT(nb_sessions, nb_sessions, PLUGIN_VAR_RQCMDARG,
                        "number of sessions", nullptr, nullptr, 1, 1, 500, 0);

static SYS_VAR *test_services_sysvars[] = {MYSQL_SYSVAR(nb_sessions), nullptr};

struct st_plugin_ctx {
  char message[1024];
  uint sql_errno;
  char err_msg[1024];
  char sqlstate[6];
  st_plugin_ctx() { reset(); }

  void reset() {
    memset(&message, 0, sizeof(message));
    sql_errno = 0;
    memset(&err_msg, 0, sizeof(err_msg));
    memset(&sqlstate, 0, sizeof(sqlstate));
  }
};

const struct st_command_service_cbs sql_cbs = {
    nullptr,  // sql_start_result_metadata,
    nullptr,  // sql_field_metadata,
    nullptr,  // sql_end_result_metadata,
    nullptr,  // sql_start_row,
    nullptr,  // sql_end_row,
    nullptr,  // sql_abort_row,
    nullptr,  // sql_get_client_capabilities,
    nullptr,  // sql_get_null,
    nullptr,  // sql_get_integer,
    nullptr,  // sql_get_longlong,
    nullptr,  // sql_get_decimal,
    nullptr,  // sql_get_double,
    nullptr,  // sql_get_date,
    nullptr,  // sql_get_time,
    nullptr,  // sql_get_datetime,
    nullptr,  // sql_get_string,
    nullptr,  // sql_handle_ok,
    nullptr,  // sql_handle_error,
    nullptr,  // sql_shutdown,
    nullptr,  // sql_alive,
};

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static File outfile;

static void test_session(void *p) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;

  MYSQL_SESSION sessions[MAX_SESSIONS];

  /* Open sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_open %d\n", i);
    sessions[i] = srv_session_open(NULL, NULL);
    if (!sessions[i])
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "srv_session_open_%d failed.", i);
  }

  unsigned int thread_count = srv_session_info_thread_count((const void *)p);
  WRITE_VAL("Number of threads of this plugin: %d\n", thread_count);
  thread_count = srv_session_info_thread_count(nullptr);
  WRITE_VAL("Number of threads of all (NULL) plugins: %d\n", thread_count);

  /*  close sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_close %d\n", nb_sessions - 1 - i);
    bool session_ret = srv_session_close(sessions[nb_sessions - 1 - i]);
    if (session_ret)
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "srv_session_close_%d failed.", nb_sessions - 1 - i);
  }
}

static void test_session_non_reverse(void *p [[maybe_unused]]) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;

  /* Session declarations */
  MYSQL_SESSION sessions[MAX_SESSIONS];

  /* Open sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_open %d\n", i);
    sessions[i] = srv_session_open(NULL, NULL);
    if (!sessions[i])
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "srv_session_open_%d failed.", i);
  }

  int session_count = srv_session_info_session_count();
  WRITE_VAL("Number of open sessions: %d\n", session_count);

  /*  close sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_close %d\n", i);
    bool session_ret = srv_session_close(sessions[i]);
    if (session_ret)
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "srv_session_close_%d failed.", i);
  }

  session_count = srv_session_info_session_count();
  WRITE_VAL("Number of open sessions: %d\n", session_count);
}

static void test_session_only_open(void *p [[maybe_unused]]) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;

  MYSQL_SESSION sessions[MAX_SESSIONS];

  /* Disabled open without close as these 5 sessions stay open until server
     shutdow, That violates the rules of a valid regression test. */
  /* Open sessions: Must pass */
  //  for (int i= 0; i < nb_sessions; i++)
  for (int i = 0; i < 0; i++) {
    WRITE_VAL("srv_session_open %d\n", i);
    sessions[i] = srv_session_open(NULL, NULL);
    if (!sessions[i])
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "srv_session_open_%d failed.", i);
  }
  struct st_plugin_ctx *ctx = new st_plugin_ctx();
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  COM_DATA cmd;
  pctx->reset();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = "SELECT * FROM test.t_int";
  cmd.com_query.length = strlen(cmd.com_query.query);
  command_service_run_command(NULL, COM_QUERY, &cmd,
                              &my_charset_utf8mb3_general_ci, &sql_cbs,
                              CS_TEXT_REPRESENTATION, ctx);
  delete ctx;
}

struct test_thread_context {
  my_thread_handle thread;
  void *p;
  bool thread_finished;
  void (*test_function)(void *);
};

static void *test_sql_threaded_wrapper(void *param) {
  char buffer[STRING_BUFFER_SIZE];
  struct test_thread_context *context = (struct test_thread_context *)param;

  WRITE_SEP();
  WRITE_STR("init thread\n");
  if (srv_session_init_thread(context->p))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "srv_session_init_thread failed.");

  context->test_function(context->p);

  WRITE_STR("deinit thread\n");
  srv_session_deinit_thread();

  context->thread_finished = true;
  return nullptr;
}

static void create_log_file(const char *log_name) {
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile = my_open(filename, O_CREAT | O_RDWR, MYF(0));
}

static void test_in_spawned_thread(void *p, void (*test_function)(void *)) {
  my_thread_attr_t attr; /* Thread attributes */
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  struct test_thread_context context;

  context.p = p;
  context.thread_finished = false;
  context.test_function = test_function;

  /* now create the thread and call test_session within the thread. */
  if (my_thread_create(&(context.thread), &attr, test_sql_threaded_wrapper,
                       &context) != 0)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Could not create test session thread");
  else
    my_thread_join(&context.thread, nullptr);
}

static int test_session_service_plugin_init(void *p) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  create_log_file(log_filename);

  /* Test of service: sessions */
  WRITE_SEP();
  WRITE_STR("Test in a server thread\n");
  test_session(p);
  test_session_non_reverse(p);
  test_session_only_open(p);
  unsigned int thread_count = srv_session_info_thread_count((const void *)p);
  WRITE_VAL("Number of threads: %d\n", thread_count);
  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_session);
  test_in_spawned_thread(p, test_session_non_reverse);
  test_in_spawned_thread(p, test_session_only_open);

  my_close(outfile, MYF(0));
  return 0;
}

static int test_session_service_plugin_deinit(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  return 0;
}

struct st_mysql_daemon test_session_service_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_session_service_plugin,
    "test_x_sessions_init",
    PLUGIN_AUTHOR_ORACLE,
    "Test session service in init",
    PLUGIN_LICENSE_GPL,
    test_session_service_plugin_init,   /* Plugin Init      */
    nullptr,                            /* Plugin Check uninstall    */
    test_session_service_plugin_deinit, /* Plugin Deinit    */
    0x0100,                             /* 1.0              */
    nullptr,                            /* status variables */
    test_services_sysvars,              /* system variables */
    nullptr,                            /* config options   */
    0,                                  /* flags            */
} mysql_declare_plugin_end;
