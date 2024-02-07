/*  Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

/**
  @file

  mysql_no_login authentication plugin.

  This plugin exists to support system user accounts, which
  cannot be accessed externally.  This is useful for privileged
  stored programs, views and events.  Such objects can be created
  with DEFINER = [sys account] SQL SECURITY DEFINER.
*/

#include <mysql/plugin_auth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_compiler.h"

static int mysql_no_login(MYSQL_PLUGIN_VIO *vio [[maybe_unused]],
                          MYSQL_SERVER_AUTH_INFO *info [[maybe_unused]]) {
  return CR_ERROR;
}

static int generate_auth_string_hash(char *outbuf [[maybe_unused]],
                                     unsigned int *buflen,
                                     const char *inbuf [[maybe_unused]],
                                     unsigned int inbuflen [[maybe_unused]]) {
  *buflen = 0;
  return 0;
}

static int validate_auth_string_hash(char *const inbuf [[maybe_unused]],
                                     unsigned int buflen [[maybe_unused]]) {
  return 0;
}

static int set_salt(const char *password [[maybe_unused]],
                    unsigned int password_len [[maybe_unused]],
                    unsigned char *salt [[maybe_unused]],
                    unsigned char *salt_len) {
  *salt_len = 0;
  return 0;
}

static struct st_mysql_auth mysql_no_login_handler = {
    MYSQL_AUTHENTICATION_INTERFACE_VERSION,
    nullptr,
    mysql_no_login,
    generate_auth_string_hash,
    validate_auth_string_hash,
    set_salt,
    AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE,
    nullptr};

mysql_declare_plugin(mysql_no_login){
    MYSQL_AUTHENTICATION_PLUGIN,      /* type constant    */
    &mysql_no_login_handler,          /* type descriptor  */
    "mysql_no_login",                 /* Name             */
    PLUGIN_AUTHOR_ORACLE,             /* Author           */
    "No login authentication plugin", /* Description      */
    PLUGIN_LICENSE_GPL,               /* License          */
    nullptr,                          /* Init function    */
    nullptr,                          /* Check uninstall function */
    nullptr,                          /* Deinit function  */
    0x0101,                           /* Version (1.0)    */
    nullptr,                          /* status variables */
    nullptr,                          /* system variables */
    nullptr,                          /* config options   */
    0,                                /* flags            */
} mysql_declare_plugin_end;
