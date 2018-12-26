/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#ifndef CLIENT_AUTHENTICATION_H
#define CLIENT_AUTHENTICATION_H
#include <my_global.h>
#include "mysql.h"
#include "mysql/client_plugin.h"

C_MODE_START
int sha256_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);
int sha256_password_init(char *, size_t, int, va_list);
int sha256_password_deinit(void);
int caching_sha2_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);
int caching_sha2_password_init(char *, size_t, int, va_list);
int caching_sha2_password_deinit(void);
C_MODE_END

#endif

