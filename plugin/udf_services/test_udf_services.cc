/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

static int test_udf_extension_init(MYSQL_PLUGIN p);
static int test_udf_extension_deinit(MYSQL_PLUGIN p);
/**
  @file test_udf_services.cc

  This is a test suite plugin to verify :
  (1) Plugins can co-exist with UDFs
      The file defines one DAEMON plugin @ref test_udf_services_plugin and one
      UDF function: @ref test_udf_services_udf.
      The test then checks if the plugin can be unloaded and loaded while the
      UDF is defined.
  (2) UDF extension attributes
      The file defines one DAEMON plugin @ref test_udf_extension_services and
      a few UDF functions to test the UDF extension arguments. UDF functions
      tests for character set and collation extension arguments right now.

  No user-facing functionality in this plugin. Just test material !
*/

static struct st_mysql_daemon test_udf_services_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

static struct st_mysql_daemon test_udf_registration_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

static struct st_mysql_daemon test_udf_extension_services_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(test_udf_services){
    MYSQL_DAEMON_PLUGIN,
    &test_udf_services_plugin,
    "test_udf_services",
    PLUGIN_AUTHOR_ORACLE,
    "MySQL mtr test framework",
    PLUGIN_LICENSE_GPL,
    nullptr, /* Plugin Init          */
    nullptr, /* Plugin Check uninstall */
    nullptr, /* Plugin Deinit        */
    0x0100,  /* Plugin version: 1.0  */
    nullptr, /* status variables     */
    nullptr, /* system variables     */
    nullptr, /* config options       */
    0,       /* flags                */
},
    {
        MYSQL_DAEMON_PLUGIN,
        &test_udf_registration_plugin,
        "test_udf_registration",
        PLUGIN_AUTHOR_ORACLE,
        "MySQL mtr test framework",
        PLUGIN_LICENSE_GPL,
        test_udf_registration_init,   /* Plugin Init          */
        nullptr,                      /* Plugin Check uninstall */
        test_udf_registration_deinit, /* Plugin Deinit        */
        0x0100,                       /* Plugin version: 1.0  */
        nullptr,                      /* status variables     */
        nullptr,                      /* system variables     */
        nullptr,                      /* config options       */
        0,                            /* flags                */
    },
    {
        MYSQL_DAEMON_PLUGIN,
        &test_udf_extension_services_plugin,
        "test_udf_extension_services",
        PLUGIN_AUTHOR_ORACLE,
        "MySQL mtr test framework",
        PLUGIN_LICENSE_GPL,
        test_udf_extension_init,   /* Plugin Init          */
        nullptr,                   /* Plugin Check uninstall */
        test_udf_extension_deinit, /* Plugin Deinit        */
        0x0100,                    /* Plugin version: 1.0  */
        nullptr,                   /* status variables     */
        nullptr,                   /* system variables     */
        nullptr,                   /* config options       */
        0,                         /* flags                */
    } mysql_declare_plugin_end;

#ifdef WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

/**
  Initialization function for @ref test_udf_services_udf

  Must be present otherwise the server refuses to load

  @param      initid    Return value from xxxx_init
  @param      args      Array of arguments
  @param[out] message   Error message in case of error.
  @retval     false     success
  @retval     true      Failure. Error in the message argument
*/
PLUGIN_EXPORT bool test_udf_services_udf_init(UDF_INIT *initid [[maybe_unused]],
                                              UDF_ARGS *args [[maybe_unused]],
                                              char *message [[maybe_unused]]) {
  return false;
}

/**
  A UDF function returning 0.

  @param      initid    Return value from xxxx_init
  @param      args      Array of arguments
  @param[out] is_null   If the result is null, store 1 here
  @param[out] error     On error store 1 here
*/
PLUGIN_EXPORT longlong test_udf_services_udf(UDF_INIT *initid [[maybe_unused]],
                                             UDF_ARGS *args [[maybe_unused]],
                                             unsigned char *is_null
                                             [[maybe_unused]],
                                             unsigned char *error
                                             [[maybe_unused]]) {
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

using udf_registration_t = SERVICE_TYPE_NO_CONST(udf_registration);

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
  reg->acquire("udf_registration",
               reinterpret_cast<my_h_service *>(
                   const_cast<udf_registration_t **>(&udf)));
  if (!udf) {
    ret = true;
    goto end;
  }
  ret = udf->udf_register("test_udf_registration_udf", INT_RESULT,
                          (Udf_func_any)test_udf_services_udf,
                          test_udf_services_udf_init, nullptr);

  reg->release(
      reinterpret_cast<my_h_service>(const_cast<udf_registration_t *>(udf)));
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
  reg->acquire("udf_registration",
               reinterpret_cast<my_h_service *>(
                   const_cast<udf_registration_t **>(&udf)));
  if (!udf) {
    ret = true;
    goto end;
  }

  ret = udf->udf_unregister("test_udf_registration_udf", &was_present);

end:
  if (reg) {
    if (udf)
      reg->release(reinterpret_cast<my_h_service>(
          const_cast<udf_registration_t *>(udf)));
    mysql_plugin_registry_release(reg);
  }
  return ret ? 1 : 0;
}

#include "services_required.h"
#include "test_udf_extension.h"
#include "udf_extension_test_functions.h"

/**
  Plugin init function that registers a UDF.
  A newly created UDF must be registered here.

  @retval false UDF registered successfully.
  @retval true  Otherwise.
*/
static int test_udf_extension_init(MYSQL_PLUGIN /*p */) {
  bool ret = true;
  if (Registry_service::acquire() || Udf_registration::acquire()) {
    goto end;
  }
  udf_ext::Test_udf_charset_base::udf_charset_base_init();
  /*
    Demonstrates how to set and get the charset extension argument of
    return value. It also demonstrate how to perforn the charset
    conversion on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set of the second argument.
  */
  if (Udf_registration::add("test_result_charset", STRING_RESULT,
                            (Udf_func_any)test_result_charset,
                            test_result_charset_init,
                            test_result_charset_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected charset of a UDF argument.
    Users sets the charset of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the charset of first UDF
    argument as charset of second argument.
  */
  if (Udf_registration::add("test_args_charset", STRING_RESULT,
                            (Udf_func_any)test_args_charset,
                            test_args_charset_init, test_args_charset_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set and get the collation extension argument of
    return value. It also demonstrate how to perforn the charset
    conversion on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set of the second argument. It determines
    the charset of first argument from the collation name as it was set
    during init() time.
  */
  if (Udf_registration::add("test_result_collation", STRING_RESULT,
                            (Udf_func_any)test_result_collation,
                            test_result_collation_init,
                            test_result_collation_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected collation of a UDF argument.
    Users sets the collation of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the collation of first UDF
    argument as collation of second argument.
  */
  if (Udf_registration::add("test_args_collation", STRING_RESULT,
                            (Udf_func_any)test_args_collation,
                            test_args_collation_init,
                            test_args_collation_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set and get the charset extension argument of
    return value. It also demonstrate how to perforn the charset conversion
    on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set as it was specified by the user in the second
    argument.
  */
  if (Udf_registration::add("test_result_charset_with_value", STRING_RESULT,
                            (Udf_func_any)test_result_charset_with_value,
                            test_result_charset_with_value_init,
                            test_result_charset_with_value_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected charset of a UDF argument.
    Users sets the charset of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the charset of first UDF
    argument as charset provided by the user in the second argument.
  */
  if (Udf_registration::add("test_args_charset_with_value", STRING_RESULT,
                            (Udf_func_any)test_args_charset_with_value,
                            test_args_charset_with_value_init,
                            test_args_charset_with_value_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set and get the collation extension argument of
    return value. It also demonstrate how to perforn the charset
    conversion on return value.

    This UDF takes two STRING arguments. It returns the value of first
    argument. But before returning the value, it converts the return
    value into the character set of the second argument. It determines
    the charset of first argument from the collation name as provided
    by the user in the second argument.
  */
  if (Udf_registration::add("test_result_collation_with_value", STRING_RESULT,
                            (Udf_func_any)test_result_collation_with_value,
                            test_result_collation_with_value_init,
                            test_result_collation_with_value_deinit)) {
    goto end;
  }
  /*
    Demonstrates how to set the expected collation of a UDF argument.
    Users sets the collation of a UDF argument at the init() time, server
    detects that and provided the converted value at the UDF() time.

    This UDF takes two STRING arguments. It sets the collation of first UDF
    argument as collation provided by the user in the second argument.
  */
  if (Udf_registration::add("test_args_collation_with_value", STRING_RESULT,
                            (Udf_func_any)test_args_collation_with_value,
                            test_args_collation_with_value_init,
                            test_args_collation_with_value_deinit)) {
    goto end;
  }
  ret = false;  // Successfully initialized the plugin
end:
  if (ret) {
    Udf_registration::release();
    Registry_service::release();
  }
  return ret ? 1 : 0;
}

/**
  Plugin deinit function that unregisters a UDF

  @retval false UDF unregistered successfully.
  @retval true  Otherwise.
*/
static int test_udf_extension_deinit(MYSQL_PLUGIN /* p */) {
  bool ret = true;
  int was_present;
  if (Registry_service::acquire() || Udf_registration::acquire()) {
    goto end;
  }
  udf_ext::Test_udf_charset_base::udf_charset_base_deinit();
  if (Udf_registration::remove("test_result_charset", &was_present) ||
      Udf_registration::remove("test_args_charset", &was_present) ||
      Udf_registration::remove("test_result_collation", &was_present) ||
      Udf_registration::remove("test_args_collation", &was_present) ||
      Udf_registration::remove("test_result_charset_with_value",
                               &was_present) ||
      Udf_registration::remove("test_args_charset_with_value", &was_present) ||
      Udf_registration::remove("test_result_collation_with_value",
                               &was_present) ||
      Udf_registration::remove("test_args_collation_with_value",
                               &was_present)) {
    goto end;
  }
  ret = false;
end:
  Udf_registration::release();
  Registry_service::release();
  return ret ? 1 : 0;
}
