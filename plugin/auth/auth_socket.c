/*  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
    
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

  auth_socket authentication plugin.

  Authentication is successful if the connection is done via a unix socket and
  the owner of the client process matches the user name that was used when
  connecting to mysqld.
*/
#define _GNU_SOURCE /* for struct ucred */

#include <mysql/plugin_auth.h>
#include <sys/socket.h>
#include <pwd.h>
#include <string.h>

static int socket_auth(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  MYSQL_PLUGIN_VIO_INFO vio_info;
  struct ucred cred;
  socklen_t cred_len= sizeof(cred);
  struct passwd pwd_buf, *pwd;
  char buf[1024];

  /* no user name yet ? read the client handshake packet with the user name */
  if (info->user_name == 0)
  {
    if (vio->read_packet(vio, &pkt) < 0)
      return CR_ERROR;
  }

  info->password_used= PASSWORD_USED_NO_MENTION;

  vio->info(vio, &vio_info);
  if (vio_info.protocol != MYSQL_VIO_SOCKET)
    return CR_ERROR;

  /* get the UID of the client process */
  if (getsockopt(vio_info.socket, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len))
    return CR_ERROR;

  if (cred_len != sizeof(cred))
    return CR_ERROR;

  /* and find the username for this uid */
  getpwuid_r(cred.uid, &pwd_buf, buf, sizeof(buf), &pwd);
  if (pwd == NULL)
    return CR_ERROR;

  /* now it's simple as that */
  return strcmp(pwd->pw_name, info->user_name) ? CR_ERROR : CR_OK;
}

static struct st_mysql_auth socket_auth_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  0,
  socket_auth
};

mysql_declare_plugin(socket_auth)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &socket_auth_handler,
  "auth_socket",
  "Sergei Golubchik",
  "Unix Socket based authentication",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0100,
  NULL,
  NULL,
  NULL,
  0,
}
mysql_declare_plugin_end;

