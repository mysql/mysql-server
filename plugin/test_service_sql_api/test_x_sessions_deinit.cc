/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#define LOG_COMPONENT_TAG "test_x_sessions_deinit"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdlib.h>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"  // my_write, my_malloc
#include "template_utils.h"

static const char *log_filename = "test_x_sessions_deinit";

#define MAX_SESSIONS 128

#define STRING_BUFFER_SIZE 512

#define WRITE_STR(format)                         \
  snprintf(buffer, sizeof(buffer), "%s", format); \
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0))

#define WRITE_VAL(format, value)                   \
  snprintf(buffer, sizeof(buffer), format, value); \
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0))

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

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static File outfile;

static void test_session_open(void *) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;

  MYSQL_SESSION sessions[MAX_SESSIONS];
  void *plugin_ctx = nullptr;

  WRITE_VAL("nb_sessions = %d\n", nb_sessions);
  /* Open sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_open %d - ", i + 1);
    sessions[i] = srv_session_open(nullptr, plugin_ctx);
    if (!sessions[i]) {
      WRITE_STR("Failed\n");
    } else {
      WRITE_STR("Success\n");
    }
  }
}

static void test_session(void *) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;

  MYSQL_SESSION sessions[MAX_SESSIONS];
  void *plugin_ctx = nullptr;
  bool session_ret = false;

  WRITE_VAL("nb_sessions = %d\n", nb_sessions);
  /* Open sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_open %d - ", i + 1);
    sessions[i] = srv_session_open(nullptr, plugin_ctx);
    if (!sessions[i]) {
      WRITE_STR("Failed\n");
    } else {
      WRITE_STR("Success\n");
    }
  }

  /*  close sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_close %d - ", nb_sessions - 1 - i + 1);  // 1 indexed
    const int idx = nb_sessions - 1 - i;
    session_ret = srv_session_close(sessions[idx]);
    if (session_ret) {
      WRITE_STR("Failed\n");
    } else {
      WRITE_STR("Success\n");
    }
    sessions[idx] = nullptr;
  }

  /*  close sessions: Fail as not opened */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_close %d - ", nb_sessions - 1 - i + 1);  // 1 indexed
    session_ret = srv_session_close(sessions[nb_sessions - 1 - i]);
    if (session_ret) {
      WRITE_STR("Failed\n");
    } else {
      WRITE_STR("Success\n");
    }
  }
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

static int test_session_service_plugin_init(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");
  return 0;
}

static int test_session_service_plugin_deinit(void *p) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  create_log_file(log_filename);

  WRITE_SEP();
  WRITE_STR("Test in a server thread\n");
  test_session(p);

  /* Test in a new thread */
  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_session);

  WRITE_STR("Follows threaded run and leaves open session (Bug#21966621)\n");
  // Bug#21966621 - Leave session open at srv thread exit which is doing the
  // release in following way:
  //                1) Lock LOCK_collection while doing release callback for
  //                every session 2) Second attempt of LOCK_collection fails
  //                while removing session by the callback. While exiting both
  //                function LOCK_collection is left in invalid state because it
  //                was released 2 times, lock 1 time 3) at next attempt of
  //                LOCK_collection usage causes deadlock
  test_in_spawned_thread(p, test_session_open);  // step 1, 2
  test_in_spawned_thread(p, test_session);       // step 3

  WRITE_STR("Follows threaded run and leaves open session (Bug#21983102)\n");
  // Bug#21983102 - iterates through sessions list in which elements are removed
  //                thus the iterator becomes invalid causing random crashes and
  //                hangings
  test_in_spawned_thread(p, test_session_open);

  my_close(outfile, MYF(0));

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
    "test_x_sessions_deinit",
    PLUGIN_AUTHOR_ORACLE,
    "Test session service in deinit",
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
