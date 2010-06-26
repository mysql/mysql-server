#ifndef MYSQL_PLUGIN_AUTH_INCLUDED
/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

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

/**
  @file

  Authentication Plugin API.

  This file defines the API for server authentication plugins.
*/

#define MYSQL_PLUGIN_AUTH_INCLUDED

#include <mysql/plugin.h>

#define MYSQL_AUTHENTICATION_INTERFACE_VERSION 0x0100

#include <mysql/plugin_auth_common.h>

/**
  Provides server plugin access to authentication information
*/
typedef struct st_mysql_server_auth_info
{
  /**
    User name as sent by the client and shown in USER().
    NULL if the client packet with the user name was not received yet.
  */
  const char *user_name;
  /**
    A corresponding column value from the mysql.user table for the
    matching account name
  */
  const char *auth_string;

  /**
    Matching account name as found in the mysql.user table.
    A plugin can override it with another name that will be
    used by MySQL for authorization, and shown in CURRENT_USER()
  */
  char authenticated_as[MYSQL_USERNAME_LENGTH+1]; 
  /**
    This only affects the "Authentication failed. Password used: %s"
    error message. If set, %s will be YES, otherwise - NO.
    Set it as appropriate or ignore at will.
  */
  int  password_used;
} MYSQL_SERVER_AUTH_INFO;

/**
  Server authentication plugin descriptor
*/
struct st_mysql_auth
{
  int interface_version;                        /**< version plugin uses */
  /**
    A plugin that a client must use for authentication with this server
    plugin. Can be NULL to mean "any plugin".
  */
  const char *client_auth_plugin;
  /**
    Function provided by the plugin which should perform authentication (using
    the vio functions if necessary) and return 0 if successful. The plugin can
    also fill the info.authenticated_as field if a different username should be
    used for authorization.
  */
  int (*authenticate_user)(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info);
};
#endif

