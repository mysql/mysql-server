/*  Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file

  mysql_no_login authentication plugin.

  This plugin exists to support system user accounts, which
  cannot be accessed externally.  This is useful for privileged
  stored programs, views and events.  Such objects can be created
  with DEFINER = [sys account] SQL SECURITY DEFINER.
*/

#include <my_global.h>
#include <mysql/plugin_auth.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int mysql_no_login(
    MYSQL_PLUGIN_VIO *vio MY_ATTRIBUTE((unused)),
    MYSQL_SERVER_AUTH_INFO *info MY_ATTRIBUTE((unused)))
{
  return CR_ERROR;
}

int generate_auth_string_hash(char *outbuf MY_ATTRIBUTE((unused)),
                              unsigned int *buflen,
                              const char *inbuf MY_ATTRIBUTE((unused)),
                              unsigned int inbuflen MY_ATTRIBUTE((unused)))
{
  *buflen= 0;
  return 0;
}

int validate_auth_string_hash(char* const inbuf  MY_ATTRIBUTE((unused)),
                              unsigned int buflen  MY_ATTRIBUTE((unused)))
{
  return 0;
}

int set_salt(const char* password MY_ATTRIBUTE((unused)),
             unsigned int password_len MY_ATTRIBUTE((unused)),
             unsigned char* salt MY_ATTRIBUTE((unused)),
             unsigned char* salt_len)
{
  *salt_len= 0;
  return 0;
}

static struct st_mysql_auth mysql_no_login_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  0,
  mysql_no_login,
  generate_auth_string_hash,
  validate_auth_string_hash,
  set_salt,
  AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE
};

mysql_declare_plugin(mysql_no_login)
{
  MYSQL_AUTHENTICATION_PLUGIN,                  /* type constant    */
  &mysql_no_login_handler,                      /* type descriptor  */
  "mysql_no_login",                             /* Name             */
  "Todd Farmer",                                /* Author           */
  "No login authentication plugin",             /* Description      */
  PLUGIN_LICENSE_GPL,                           /* License          */
  NULL,                                         /* Init function    */
  NULL,                                         /* Deinit function  */
  0x0101,                                       /* Version (1.0)    */
  NULL,                                         /* status variables */
  NULL,                                         /* system variables */
  NULL,                                         /* config options   */
  0,                                            /* flags            */
}
mysql_declare_plugin_end;
