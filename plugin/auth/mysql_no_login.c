/*  Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
    MYSQL_PLUGIN_VIO *vio __attribute__((unused)),
    MYSQL_SERVER_AUTH_INFO *info __attribute__((unused)))
{
  return CR_ERROR;
}

static struct st_mysql_auth mysql_no_login_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  0,
  mysql_no_login
};

mysql_declare_plugin(mysql_no_login)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &mysql_no_login_handler,
  "mysql_no_login",
  "Todd Farmer",
  "No login authentication plugin",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0100,
  NULL,
  NULL,
  NULL
}
mysql_declare_plugin_end;
