/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file include/caching_sha2_passwordopt-vars.h
*/

#ifndef HAVE_YASSL
#include "mysql.h"
static char *opt_server_public_key= 0;
static my_bool opt_get_server_public_key= FALSE;
#endif /* HAVE_YASSL */

inline static void set_server_public_key(MYSQL *mysql)
{
#ifndef HAVE_YASSL
  if (opt_server_public_key && *opt_server_public_key)
    mysql_options(mysql, MYSQL_SERVER_PUBLIC_KEY, opt_server_public_key);
#else
  (void)mysql;
#endif /* HAVE_YASSL */
}

inline static void set_get_server_public_key_option(MYSQL *mysql)
{
#ifndef HAVE_YASSL
  mysql_options(mysql, MYSQL_OPT_GET_SERVER_PUBLIC_KEY,
                &opt_get_server_public_key);
#else
  (void)mysql;
#endif /* HAVE_YASSL */
}
