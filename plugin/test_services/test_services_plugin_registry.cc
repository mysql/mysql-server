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

#define LOG_COMPONENT_TAG "test_services_plugin_registry"

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/plugin.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <stddef.h>

#include "my_dbug.h"

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

/**
  @file test_services_plugin_registry.cc

  Test daemon plugin for the registry plugin service.

  @sa test_plugin_registry()
*/

/** Declare the interface to our own service. Usually comes from a svc header */
BEGIN_SERVICE_DEFINITION(test_services_plugin_registry_service)
DECLARE_BOOL_METHOD(test1, (int a1, int a2, int *outres));
END_SERVICE_DEFINITION(test_services_plugin_registry_service)

/**
  Implementation of test1 method

  @param a1 first argument to add
  @param a2 second argument to add
  @param[out] outres the result of adding a1 to a2

  @retval 0         success
  @retval non-zero  failure
*/
static DEFINE_BOOL_METHOD(test1, (int a1, int a2, int *outres)) {
  *outres = a1 + a2;
  return 0;
}

/** Our own service definition: a struct of function pointers */
static SERVICE_TYPE(test_services_plugin_registry_service) svc_def = {test1};

using service_type_t =
    SERVICE_TYPE_NO_CONST(test_services_plugin_registry_service);

/** @ref svc_def converted to a @ref my_h_service */
static my_h_service h_my_svc =
    reinterpret_cast<my_h_service>(const_cast<service_type_t *>(&svc_def));

/**
  Tests the plugin registry service

  The test consists of:
    - acquire the registry_registration service default implememtnation
    - register a new service implementation for the mysql_server component
    - acquire the newly registered service's default implementation
    - compare the service handles for equality
    - release the service handle for our service
    - unregister our service
    - release the handle to the registry registration service

  Error messages are written to the server's error log.
  In case of success writes a single information message to the server's log.

  @retval false  success
  @retval true   failure
*/
static bool test_plugin_registry(MYSQL_PLUGIN p [[maybe_unused]]) {
  bool result = false;
  SERVICE_TYPE(registry) *r = mysql_plugin_registry_acquire();
  my_h_service h_reg = nullptr;
  my_h_service h_ret_svc = nullptr;
  int int_result = -1;
  SERVICE_TYPE(registry_registration) *reg = nullptr;
  SERVICE_TYPE(test_services_plugin_registry_service) * ret;

  enum { IDLE, REG_ACQUIRED, MY_SVC_REGISTERED, MY_SVC_ACQUIRED } state = IDLE;

  if (!r) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "mysql_plugin_registry_acquire() returns empty");
    return true;
  }

  if (r->acquire("registry_registration", &h_reg)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "finding registry_register failed");
    result = true;
    goto done;
  }

  if (!h_reg) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "empty registry_query returned");
    result = true;
    goto done;
  }

  reg = reinterpret_cast<SERVICE_TYPE(registry_registration) *>(h_reg);

  state = REG_ACQUIRED;

  if (reg->register_service(
          "test_services_plugin_registry_service.mysql_server", h_my_svc)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't register my new service");
    result = true;
    goto done;
  }

  /* Register an already restistered service: Fail */
  if (reg->register_service(
          "test_services_plugin_registry_service.mysql_server", h_my_svc)) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "new service already registered");
  }

  state = MY_SVC_REGISTERED;

  if (r->acquire("test_services_plugin_registry_service", &h_ret_svc)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find the newly registered service");
    result = true;
    goto done;
  }

  state = MY_SVC_ACQUIRED;

  /* Acquire an already acquired service: Succeed (ignored) */
  if (r->acquire("test_services_plugin_registry_service", &h_ret_svc)) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "newly registered service already aquired");
  }

  state = MY_SVC_ACQUIRED;

  if (h_ret_svc != h_my_svc) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Different service handle returned");
    result = true;
    goto done;
  }

  ret = reinterpret_cast<SERVICE_TYPE(test_services_plugin_registry_service) *>(
      h_ret_svc);

  if (ret->test1(1, 2, &int_result)) {
    LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                    "results don't match: received %d", int_result);
    result = true;
    goto done;
  }

  if (r->release(h_ret_svc)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "can't release my service");
    result = true;
    goto done;
  }

  /* Release an already released service: Succeed (ignored) */
  if (r->release(h_ret_svc)) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "my service already released");
  }

  state = MY_SVC_REGISTERED;

  if (reg->unregister("test_services_plugin_registry_service.mysql_server")) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "can't unregister my service");
    result = true;
    goto done;
  }

  /* Unregister an already unregistered service: Fail */
  if (reg->unregister("test_services_plugin_registry_service.mysql_server")) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "my service aleady unregistered");
  }

  state = REG_ACQUIRED;

  if (r->release(h_reg)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't release registry_registration");
    result = true;
    goto done;
  }

  state = IDLE;

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
               "test_plugin_registry succeeded");

done:
  switch (state) {
    case MY_SVC_ACQUIRED:
      r->release(h_ret_svc);
      [[fallthrough]];
    case MY_SVC_REGISTERED:
      reg->unregister("test_services_plugin_registry_service.mysql_server");
      [[fallthrough]];
    case REG_ACQUIRED:
      r->release(h_reg);
      [[fallthrough]];
    case IDLE:
    default:
      mysql_plugin_registry_release(r);
  }
  return result;
}

/**
  Initialize the test services at server start or plugin installation.

  Call the test service.

  @retval 0 success
  @retval 1 failure
*/

static int test_services_plugin_init(void *p) {
  DBUG_TRACE;
  int rc;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;

  rc = test_plugin_registry(reinterpret_cast<MYSQL_PLUGIN>(p)) ? 1 : 0;

  return rc;
}

/**
  Terminate the test services at server shutdown or plugin deinstallation.

  Does nothing

  @retval 0 success
  @retval 1 failure
*/

static int test_services_plugin_deinit(void *) {
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  DBUG_TRACE;
  return 0;
}

static struct st_mysql_daemon test_services_plugin_registry = {
    MYSQL_DAEMON_INTERFACE_VERSION};

/**
  test_services_plugin_registry descriptor
*/

mysql_declare_plugin(test_services_plugin_registry){
    MYSQL_DAEMON_PLUGIN,
    &test_services_plugin_registry,
    "test_services_plugin_registry",
    PLUGIN_AUTHOR_ORACLE,
    "test the plugin registry services",
    PLUGIN_LICENSE_GPL,
    test_services_plugin_init,   /* Plugin Init */
    nullptr,                     /* Plugin Check uninstall */
    test_services_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    nullptr, /* status variables                */
    nullptr, /* system variables                */
    nullptr, /* config options                  */
    0,       /* flags                           */
} mysql_declare_plugin_end;
