/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mysql/plugin.h"

/*
  Test plugin to examine registration of a dynamic system variable which name
  conflicts with a name of static system variable (@@sql_mode).

  Externally visible names of plugin-registered variables consist of two
  parts separated by the underscore symbol>

  To check a registration of variables
*/

/**
  Plugin type-specific descriptor
*/
static struct st_mysql_daemon test_plugin_descriptor = {
    0x0001  // interface version
};

static long value_storage;

/**
  Descriptor of a system variable @@sql_mode2 (the should not conflict with
  existent system variable names).
*/
static MYSQL_SYSVAR_LONG(
    mode2,                // name part of the externally visible variable name
    value_storage,        // associated value
    PLUGIN_VAR_RQCMDARG,  // flags
    "Forces to register a variable with name \"sql_mode2\"",  // comment
    nullptr,                                                  // check function
    nullptr,  // on-update function
    0L,       // default value
    0L,       // minimal allowed value
    0L,       // maximal allowed value
    0);       // blk_sz(?)

/**
  Descriptor of a system variable with a conflicting name: @@sql_mode.
*/
static MYSQL_SYSVAR_LONG(
    mode,  // name part of the externally visible variable name (i.e. sql_mode)
    value_storage,                                          // associated value
    PLUGIN_VAR_RQCMDARG,                                    // flags
    "Forces to register a conflicting name: \"sql_mode\"",  // comment
    nullptr,                                                // check function
    nullptr,  // on-update function
    0L,       // default value
    0L,       // minimal allowed value
    0L,       // maximal allowed value
    0);       // blk_sz(?)

static SYS_VAR *system_variables[] = {
    MYSQL_SYSVAR(mode2),  // should be successfully registered
    MYSQL_SYSVAR(mode),   // should be rejected with a warning in a server log
    nullptr               // end of array
};

/**
  Plugin library descriptor
*/
mysql_declare_plugin(ftexample){
    MYSQL_UDF_PLUGIN,         // type
    &test_plugin_descriptor,  // descriptor
    "sql",                    // plugin name/head of registered variables
    PLUGIN_AUTHOR_ORACLE,     // author
    "Test plugin",            // description
    PLUGIN_LICENSE_GPL,       // license
    nullptr,                  // init function (when loaded)
    nullptr,                  // check uninstall function
    nullptr,                  // de-init function (when unloaded)
    0x0001,                   // version
    nullptr,                  // status variables
    system_variables,         // system variables
    nullptr,
    0,
} mysql_declare_plugin_end;
