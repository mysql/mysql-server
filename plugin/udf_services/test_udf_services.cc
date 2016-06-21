/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <my_global.h>
#include <mysql_version.h>
#include <mysql/plugin.h>
#include <mysql_com.h>

/**
  @file test_udf_services.cc

  This is a test suite plugin to verify that plugins can co-exist with UDFs.
  The file defines one DAEMON plugin @ref test_udf_services_plugin and one
  UDF function: @ref test_udf_services_udf.
  The test then checks if the plugin can be unloaded and loaded while the
  UDF is defined.

  No user-facing functionality in this plugin. Just test material !
*/

static struct st_mysql_daemon test_udf_services_plugin=
{ MYSQL_DAEMON_INTERFACE_VERSION  };

mysql_declare_plugin(test_udf_services)
{
  MYSQL_DAEMON_PLUGIN,
  &test_udf_services_plugin,
  "test_udf_services",
  "Georgi Kodinov",
  "MySQL mtr test framework",
  PLUGIN_LICENSE_GPL,
  NULL,                       /* Plugin Init          */
  NULL,                       /* Plugin Deinit        */
  0x0100,                     /* Plugin version: 1.0  */
  NULL,                       /* status variables     */
  NULL,                       /* system variables     */
  NULL,                       /* config options       */
  0,                          /* flags                */
}
mysql_declare_plugin_end;

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
  @retval     FALSE     success
  @retval     TRUE      Failure. Error in the message argument
*/
PLUGIN_EXPORT my_bool
test_udf_services_udf_init(UDF_INIT *initid MY_ATTRIBUTE((unused)),
                           UDF_ARGS *args MY_ATTRIBUTE((unused)),
                           char *message MY_ATTRIBUTE((unused)))
{
  return FALSE;
}


/**
  A UDF function returning 0.

  @param      initrd    Return value from xxxx_init
  @param      args      Array of arguments
  @param[out] is_null   If the result is null, store 1 here
  @param[out] error     On error store 1 here
*/
PLUGIN_EXPORT longlong
test_udf_services_udf(UDF_INIT *initid MY_ATTRIBUTE((unused)),
                      UDF_ARGS *args MY_ATTRIBUTE((unused)),
                      char *is_null MY_ATTRIBUTE((unused)),
                      char *error MY_ATTRIBUTE((unused)))
{
  char buffer[10];
  *is_null= 0;
  *error= 0;
  /* use a plugin service function */
  my_snprintf(buffer, sizeof(buffer), "test");
  return 0;
}
