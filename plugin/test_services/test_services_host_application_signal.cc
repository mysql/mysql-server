/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#define LOG_COMPONENT_TAG "test_services_host_application_signal"

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/my_host_application_signal.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/plugin.h>
#include <mysql_version.h>
#include <stddef.h>

REQUIRES_SERVICE_PLACEHOLDER(host_application_signal) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(udf_registration) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(registry) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(log_builtins) = nullptr;
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string) = nullptr;
static bool udf_registered = false;

static bool shutdown_at_init = false;

static MYSQL_SYSVAR_BOOL(shutdown_at_init, shutdown_at_init,
                         PLUGIN_VAR_OPCMDARG, "Call shutdown at init if true",
                         nullptr, nullptr, false);

static SYS_VAR *system_variables[] = {MYSQL_SYSVAR(shutdown_at_init), nullptr};

static long long test_shutdown_signal_udf(UDF_INIT *, UDF_ARGS *args,
                                          unsigned char *,
                                          unsigned char *error) {
  if (args->arg_count > 0 && args->arg_type[0] == INT_RESULT) {
    switch (*(reinterpret_cast<long long *>(args->args[0]))) {
      case 1:
        my_host_application_signal_shutdown(mysql_service_registry);
        break;
      case 0:
        mysql_service_host_application_signal->signal(
            HOST_APPLICATION_SIGNAL_SHUTDOWN, nullptr);
        break;
      case 2:
        mysql_service_host_application_signal->signal(
            HOST_APPLICATION_SIGNAL_LAST, nullptr);
        break;
    }
  } else
    *error = 1;
  return 0;
}

static int plugin_deinit(void *p);

static int plugin_init(void *p) {
  int rc = 0;
  my_h_service h;

  if (init_logging_service_for_plugin(&mysql_service_registry,
                                      &mysql_service_log_builtins,
                                      &mysql_service_log_builtins_string))
    return 1;

  if (shutdown_at_init &&
      my_host_application_signal_shutdown(mysql_service_registry))
    rc = 1;

  if (mysql_service_registry->acquire("host_application_signal", &h))
    rc = 1;
  else
    mysql_service_host_application_signal =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(host_application_signal) *>(h);

  if (mysql_service_registry->acquire("udf_registration", &h))
    rc = 1;
  else
    mysql_service_udf_registration =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(udf_registration) *>(h);

  if (mysql_service_udf_registration) {
    if (mysql_service_udf_registration->udf_register(
            "test_shutdown_signal_udf", INT_RESULT,
            reinterpret_cast<Udf_func_any>(test_shutdown_signal_udf), nullptr,
            nullptr))
      rc = 1;
    else
      udf_registered = true;
  }

  if (rc) plugin_deinit(p);
  return rc;
}

static int plugin_deinit(void *) {
  if (mysql_service_host_application_signal)
    mysql_service_registry->release(reinterpret_cast<my_h_service>(
        const_cast<SERVICE_TYPE_NO_CONST(host_application_signal) *>(
            mysql_service_host_application_signal)));
  if (mysql_service_udf_registration) {
    if (udf_registered)
      mysql_service_udf_registration->udf_unregister("test_shutdown_signal_udf",
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

mysql_declare_plugin(test_services_host_application_signal){
    MYSQL_DAEMON_PLUGIN,
    &plugin,
    "test_services_host_application_signal",
    PLUGIN_AUTHOR_ORACLE,
    "test the plugin host application signal service",
    PLUGIN_LICENSE_GPL,
    plugin_init,   /* Plugin Init */
    nullptr,       /* Plugin Check uninstall */
    plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    nullptr, /* status variables                */
    system_variables,
    nullptr, /* config options                  */
    0,       /* flags                           */
} mysql_declare_plugin_end;
