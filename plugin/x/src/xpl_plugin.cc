/*
   Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql/plugin.h"
#include "mysqlx_version.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/module_cache.h"
#include "plugin/x/src/module_mysqlx.h"

static struct st_mysql_daemon mysqlx_deamon_plugin_descriptor = {
    MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(mysqlx){
    MYSQL_AUDIT_PLUGIN, /* plugin type                   */
    /* type specific descriptor */
    modules::Module_cache::get_audit_plugin_descriptor(),
    "mysqlx_cache_cleaner", /* plugin name                   */
    PLUGIN_AUTHOR_ORACLE,   /* author                        */
    "Cache cleaner for sha2 authentication in X plugin", /* description */
    PLUGIN_LICENSE_GPL,                  /* license                       */
    modules::Module_cache::initialize,   /* plugin initializer            */
    nullptr,                             /* Uninstall notifier            */
    modules::Module_cache::deinitialize, /* plugin deinitializer          */
    0x0100,                              /* version (1.0)                 */
    nullptr,                             /* status variables              */
    nullptr,                             /* system variables              */
    nullptr,                             /* reserverd                     */
    0                                    /* flags                         */
},
    {
        MYSQL_DAEMON_PLUGIN,
        &mysqlx_deamon_plugin_descriptor,
        MYSQLX_PLUGIN_NAME,
        PLUGIN_AUTHOR_ORACLE,
        "X Plugin for MySQL",
        PLUGIN_LICENSE_GPL,
        &modules::Module_mysqlx::initialize,            /* init       */
        nullptr,                                        /* check uninstall */
        &modules::Module_mysqlx::deinitialize,          /* deinit     */
        MYSQLX_PLUGIN_VERSION,                          /* version    */
        modules::Module_mysqlx::get_status_variables(), /* status var */
        modules::Module_mysqlx::get_plugin_variables(), /* system var */
        nullptr,                                        /* options    */
        0                                               /* flags      */
    } mysql_declare_plugin_end;
