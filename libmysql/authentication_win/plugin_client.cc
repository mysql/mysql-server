/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <mysql.h>
#include <mysql/plugin_auth.h>
#include <mysql/client_plugin.h>

#include "common.h"

/*
  The following MS C++ specific pragma embeds a comment in the resulting
  object file. A "lib" comment tells the linker to use the specified 
  library, thus the dependency is handled automagically.
*/

#ifdef _MSC_VER
#pragma comment(lib, "Secur32")
#endif

static int win_auth_client_plugin_init(char*, size_t, int, va_list)
{
  return 0;
}


static int win_auth_client_plugin_deinit()
{
  return 0;
}


int win_auth_handshake_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);


/*
  Client plugin declaration. This is added to mysql_client_builtins[]
  in sql-common/client.c
*/

extern "C"
st_mysql_client_plugin_AUTHENTICATION win_auth_client_plugin=
{
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN,
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN_INTERFACE_VERSION,
  "authentication_windows_client",
  "Rafal Somla",
  "Windows Authentication Plugin - client side",
  {0,1,0},
  "GPL",
  NULL,
  win_auth_client_plugin_init,
  win_auth_client_plugin_deinit,
  NULL,                            // option handling
  win_auth_handshake_client
};
