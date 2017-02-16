/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/client_plugin.h>
#include <mysql.h>
#include <string.h>

/**
  The main function of the mysql_clear_password authentication plugin.
*/

static int clear_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  int res;

  /* send password in clear text */
  res= vio->write_packet(vio, (const unsigned char *) mysql->passwd, 
                              strlen(mysql->passwd) + 1);

  return res ? CR_ERROR : CR_OK;
}

mysql_declare_client_plugin(AUTHENTICATION)
  "mysql_clear_password",
  "Georgi Kodinov",
  "Clear password authentication plugin",
  {0,1,0},
  "GPL",
  NULL,
  NULL,
  NULL,
  NULL,
  clear_password_auth_client
mysql_end_client_plugin;

