/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#define LOG_COMPONENT_TAG "test_services"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <mysql_version.h>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "m_string.h"  // strlen
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"  // my_write, my_malloc
//#include "sql_plugin.h"                         // st_plugin_int

#define STRING_BUFFER 100

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

File outfile;

/* Shows status of test (Busy/READY) */
enum t_test_status { BUSY = 0, READY = 1 };
static volatile t_test_status test_status;

/* declaration of status variable for plugin */
static SHOW_VAR test_services_status[] = {
    {"test_services_status",
     const_cast<char *>(reinterpret_cast<volatile char *>(&test_status)),
     SHOW_INT, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF, SHOW_SCOPE_GLOBAL}};

/* SQL (system) variables to control test execution                     */
/* SQL (system) variables to switch on/off test of services, default=on */
/* Only be effective at start od mysqld by setting it as option --loose-...  */
static int with_log_message_val = 0;
static MYSQL_SYSVAR_INT(with_log_message, with_log_message_val,
                        PLUGIN_VAR_RQCMDARG,
                        "Switch on/off test of log message service", nullptr,
                        nullptr, 1, 0, 1, 0);

static int non_default_variable_value = 0;
static MYSQL_SYSVAR_INT(non_default_variable, non_default_variable_value,
                        PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_NODEFAULT,
                        "A variable that won't accept SET DEFAULT", nullptr,
                        nullptr, 1, 0, 100, 0);

static SYS_VAR *test_services_sysvars[] = {MYSQL_SYSVAR(with_log_message),
                                           MYSQL_SYSVAR(non_default_variable),
                                           nullptr};

/* The test cases for the log_message service. */
static int test_log_plugin_error(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  /* Writes to mysqld.1.err: Plugin test_services reports an info text */
  LogPluginErr(
      INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
      "This is the test plugin for services testing info report output");

  /* Writes to mysqld.1.err: Plugin test_services reports a warning. */
  LogPluginErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG,
               "This is a warning from test plugin for services "
               "testing warning report output");

  /* Writes to mysqld.1.err: Plugin test_services reports an error. */
  LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
               "This is an error from test plugin for services "
               "testing error report output");

  return 0;
}

/* The tests of snprintf and log_message run when INSTALL PLUGIN is called. */
static int test_services_plugin_init(void *p) {
  DBUG_TRACE;

  int ret = 0;
  test_status = BUSY;
  /* Test of service: LogPluginErr/LogPluginErrMsg */
  /* Log the value of the switch in mysqld.err. */

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErrMsg(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                  "Test_services with_log_message_val: %d",
                  with_log_message_val);
  if (with_log_message_val == 1) {
    ret = test_log_plugin_error(p);
  } else {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "Test of log_message switched off");
    ret = 0;
  }

  test_status = READY;

  return ret;
}

/* There is nothing to clean up when UNINSTALL PLUGIN. */
static int test_services_plugin_deinit(void *) {
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  DBUG_TRACE;
  return 0;
}

/* Mandatory structure describing the properties of the plugin. */
struct st_mysql_daemon test_services_plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_services_plugin,
    "test_services",
    PLUGIN_AUTHOR_ORACLE,
    "Test services",
    PLUGIN_LICENSE_GPL,
    test_services_plugin_init,   /* Plugin Init */
    nullptr,                     /* Plugin Check uninstall */
    test_services_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    test_services_status,  /* status variables                */
    test_services_sysvars, /* system variables                */
    nullptr,               /* config options                  */
    0,                     /* flags                           */
} mysql_declare_plugin_end;
