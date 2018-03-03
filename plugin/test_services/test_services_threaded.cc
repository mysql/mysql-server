/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define LOG_COMPONENT_TAG "test_services_threaded"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <mysql_version.h>
#include <stdlib.h>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "m_string.h"  // strlen
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"          // my_write, my_malloc
#include "sql/sql_plugin.h"  // st_plugin_int

#define STRING_BUFFER 256

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

File outfile;

struct test_services_context {
  my_thread_handle test_services_thread;
};

/* Shows status of test (Busy/READY) */
enum t_test_status { BUSY = 0, READY = 1 };
static t_test_status test_status;

/* declaration of status variable for plugin */
static SHOW_VAR test_services_status[] = {
    {"test_services_status", (char *)&test_status, SHOW_INT, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_GLOBAL}};

/* SQL variables to control test execution                     */
/* SQL variables to switch on/off test of services, default=on */
/* Only be effective at start od mysqld by setting it as option --loose-...  */
static int with_log_message_val = 0;
static MYSQL_SYSVAR_INT(with_log_message, with_log_message_val,
                        PLUGIN_VAR_RQCMDARG,
                        "Switch on/off test of log message service", NULL, NULL,
                        1, 0, 1, 0);

static SYS_VAR *test_services_sysvars[] = {MYSQL_SYSVAR(with_log_message),
                                           NULL};

/* The test cases for the log_message service. */
static int test_log_plugin_error() {
  DBUG_ENTER("test_log_plugin_error");
  /* Writes to mysqld.1.err: Plugin test_services reports an info text */
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
               "This is the test plugin for services");

  /* Writes to mysqld.1.err: Plugin test_services reports a warning. */
  LogPluginErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG,
               "This is a warning from test plugin for services");

  /* Writes to mysqld.1.err: Plugin test_services reports an error. */
  LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
               "This is an error from test plugin for services");

  DBUG_RETURN(0);
}

/* This fucntion is needed to be called in a thread. */
static void *test_services(void *p MY_ATTRIBUTE((unused))) {
  DBUG_ENTER("test_services");

  int ret = 0;

  test_status = BUSY;
  /* Test of service: LogPluginErr/LogPluginErrMsg */
  /* Log the value of the switch in mysqld.err. */
  LogPluginErrMsg(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                  "Test_services_threaded with_log_message_val: %d",
                  with_log_message_val);
  if (with_log_message_val == 1) {
    ret = test_log_plugin_error();
  } else {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "Test of log_message switched off");
  }

  test_status = READY;

  if (ret != 0) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Test services return code: %d", ret);
  }

  DBUG_RETURN(0);
}

/* Creates the plugin context "con", which holds a pointer to the thread. */
static int test_services_plugin_init(void *p) {
  DBUG_ENTER("test_services_plugin_init");

  int ret = 0;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs))
    DBUG_RETURN(1);
  struct test_services_context *con;
  my_thread_attr_t attr; /* Thread attributes */
  struct st_plugin_int *plugin = (struct st_plugin_int *)p;
  con = (struct test_services_context *)my_malloc(
      PSI_INSTRUMENT_ME, sizeof(struct test_services_context), MYF(0));
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  /* now create the thread and call test_services within the thread. */
  if (my_thread_create(&con->test_services_thread, &attr, test_services, p) !=
      0) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Could not create test services thread!");
    exit(0);
  }
  plugin->data = (void *)con;

  DBUG_RETURN(ret);
}

/* Clean up thread and frees plugin context "con". */
static int test_services_plugin_deinit(void *p) {
  DBUG_ENTER("test_services_plugin_deinit");
  void *dummy_retval;
  struct st_plugin_int *plugin = (struct st_plugin_int *)p;
  struct test_services_context *con =
      (struct test_services_context *)plugin->data;
  my_thread_cancel(&con->test_services_thread);
  my_thread_join(&con->test_services_thread, &dummy_retval);
  my_free(con);
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  DBUG_RETURN(0);
}

/* Mandatory structure describing the properties of the plugin. */
struct st_mysql_daemon test_services_plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_services_plugin,
    "test_services_threaded",
    "Horst Hunger",
    "Test services with thread",
    PLUGIN_LICENSE_GPL,
    test_services_plugin_init,   /* Plugin Init */
    NULL,                        /* Plugin Check uninstall */
    test_services_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    test_services_status,  /* status variables                */
    test_services_sysvars, /* system variables                */
    NULL,                  /* config options                  */
    0,                     /* flags                           */
} mysql_declare_plugin_end;
