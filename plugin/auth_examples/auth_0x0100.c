/* Copyright (C) 2013 Sergei Golubchik and Monty Program Ab

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

  auth plugin that uses old structures as of
  MYSQL_AUTHENTICATION_INTERFACE_VERSION 0x0100

  To test the old version support.
  It intentionally uses no constants like CR_OK ok PASSWORD_USED_YES.
*/

#include <mysql/plugin.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if 0
#include <mysql/plugin_auth.h>
#else
#define MYSQL_AUTHENTICATION_INTERFACE_VERSION 0x0100
typedef void MYSQL_PLUGIN_VIO; /* we don't use it here */

typedef struct st_mysql_server_auth_info
{
  char *user_name;
  unsigned int user_name_length;
  const char *auth_string;
  unsigned long auth_string_length;
  char authenticated_as[49]; 
  char external_user[512];
  int  password_used;
  const char *host_or_ip;
  unsigned int host_or_ip_length;
} MYSQL_SERVER_AUTH_INFO;

struct st_mysql_auth
{
  int interface_version;
  const char *client_auth_plugin;
  int (*authenticate_user)(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info);
};
#endif

static int do_auth_0x0100(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  info->password_used= 1;
  strcpy(info->authenticated_as, "zzzzzzzzzzzzzzzz");
  memset(info->external_user, 'o', 510);
  info->external_user[510]='.';
  info->external_user[511]=0;
  return vio ? -1 : 0; /* use vio to avoid the 'unused' warning */
}

static struct st_mysql_auth auth_0x0100_struct=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION, 0, do_auth_0x0100
};

maria_declare_plugin(auth_0x0100)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &auth_0x0100_struct,
  "auth_0x0100",
  "Sergei Golubchik",
  "Test for API 0x0100 support",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0100,
  NULL,
  NULL,
  "1.0",
  MariaDB_PLUGIN_MATURITY_EXPERIMENTAL,
}
maria_declare_plugin_end;

