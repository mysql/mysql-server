/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/plugin.h>
#include <mysql_com.h>
#include <mysql_version.h>
#include <stddef.h>

#include "my_compiler.h"
#include "my_inttypes.h"

static int test_udf_registration_init(MYSQL_PLUGIN p);
static int test_udf_registration_deinit(MYSQL_PLUGIN p);

/**
  @file test_udf_services.cc

  This is a test suite plugin to verify that plugins can co-exist with UDFs.
  The file defines one DAEMON plugin @ref test_udf_services_plugin and one
  UDF function: @ref test_udf_services_udf.
  The test then checks if the plugin can be unloaded and loaded while the
  UDF is defined.

  No user-facing functionality in this plugin. Just test material !
*/

static struct st_mysql_daemon test_udf_services_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

static struct st_mysql_daemon test_udf_registration_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(test_udf_services){
    MYSQL_DAEMON_PLUGIN,
    &test_udf_services_plugin,
    "test_udf_services",
    "Georgi Kodinov",
    "MySQL mtr test framework",
    PLUGIN_LICENSE_GPL,
    NULL,   /* Plugin Init          */
    NULL,   /* Plugin Check uninstall */
    NULL,   /* Plugin Deinit        */
    0x0100, /* Plugin version: 1.0  */
    NULL,   /* status variables     */
    NULL,   /* system variables     */
    NULL,   /* config options       */
    0,      /* flags                */
},
    {
        MYSQL_DAEMON_PLUGIN,
        &test_udf_registration_plugin,
        "test_udf_registration",
        "Georgi Kodinov",
        "MySQL mtr test framework",
        PLUGIN_LICENSE_GPL,
        test_udf_registration_init,   /* Plugin Init          */
        NULL,                         /* Plugin Check uninstall */
        test_udf_registration_deinit, /* Plugin Deinit        */
        0x0100,                       /* Plugin version: 1.0  */
        NULL,                         /* status variables     */
        NULL,                         /* system variables     */
        NULL,                         /* config options       */
        0,                            /* flags                */
    } mysql_declare_plugin_end;

#ifdef WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

/**
  Initialization function for @ref test_udf_services_udf

  Must be present otherwise the server refuses to load

  @param      initrd    Return value from xxxx_init
  @param      args      Array of arguments
  @param[out] message   Error message in case of error.
  @retval     false     success
  @retval     true      Failure. Error in the message argument
*/
PLUGIN_EXPORT bool test_udf_services_udf_init(
    UDF_INIT *initid MY_ATTRIBUTE((unused)),
    UDF_ARGS *args MY_ATTRIBUTE((unused)),
    char *message MY_ATTRIBUTE((unused))) {
  return false;
}

/**
  A UDF function returning 0.

  @param      initrd    Return value from xxxx_init
  @param      args      Array of arguments
  @param[out] is_null   If the result is null, store 1 here
  @param[out] error     On error store 1 here
*/
PLUGIN_EXPORT longlong test_udf_services_udf(
    UDF_INIT *initid MY_ATTRIBUTE((unused)),
    UDF_ARGS *args MY_ATTRIBUTE((unused)), char *is_null MY_ATTRIBUTE((unused)),
    char *error MY_ATTRIBUTE((unused))) {
  char buffer[10];
  *is_null = 0;
  *error = 0;
  /* use a plugin service function */
  snprintf(buffer, sizeof(buffer), "test");
  return 0;
}

#include <mysql/components/my_service.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/service_plugin_registry.h>

/** Sample plugin init function that registers a UDF */
static int test_udf_registration_init(MYSQL_PLUGIN /*p */) {
  SERVICE_TYPE(registry) * reg;
  SERVICE_TYPE(udf_registration) * udf;
  bool ret = false;

  reg = mysql_plugin_registry_acquire();
  if (!reg) {
    ret = true;
    goto end;
  }
  reg->acquire("udf_registration", (my_h_service *)&udf);
  if (!udf) {
    ret = true;
    goto end;
  }
  ret = udf->udf_register("test_udf_registration_udf", INT_RESULT,
                          (Udf_func_any)test_udf_services_udf,
                          test_udf_services_udf_init, NULL);

  reg->release((my_h_service)udf);
end:
  if (reg) mysql_plugin_registry_release(reg);
  return ret ? 1 : 0;
}

/** Sample plugin init function that unregisters a UDF */
static int test_udf_registration_deinit(MYSQL_PLUGIN /* p */) {
  SERVICE_TYPE(registry) * reg;
  SERVICE_TYPE(udf_registration) *udf = nullptr;
  bool ret = false;
  int was_present;

  reg = mysql_plugin_registry_acquire();
  if (!reg) {
    ret = true;
    goto end;
  }
  reg->acquire("udf_registration", (my_h_service *)&udf);
  if (!udf) {
    ret = true;
    goto end;
  }

  ret = udf->udf_unregister("test_udf_registration_udf", &was_present);

end:
  if (reg) {
    if (udf) reg->release((my_h_service)udf);
    mysql_plugin_registry_release(reg);
  }
  return ret ? 1 : 0;
}
