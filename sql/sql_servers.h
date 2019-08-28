#ifndef SQL_SERVERS_INCLUDED
#define SQL_SERVERS_INCLUDED

/* Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_global.h"                  /* uint */
#include "rpl_slave.h" // for tables_ok(), rpl_filter

class THD;
typedef struct st_lex_server_options LEX_SERVER_OPTIONS;
typedef struct st_mem_root MEM_ROOT;

/* structs */
typedef struct st_federated_server
{
  char *server_name;
  long port;
  uint server_name_length;
  char *db, *scheme, *username, *password, *socket, *owner, *host, *sport;
} FOREIGN_SERVER;

/* cache handlers */
bool servers_init(bool dont_read_server_table);
bool servers_reload(THD *thd);
void servers_free(bool end=0);

/* insert functions */
bool create_server(THD *thd, LEX_SERVER_OPTIONS *server_options);

/* drop functions */
bool drop_server(THD *thd, LEX_SERVER_OPTIONS *server_options, bool if_exists);

/* update functions */
bool alter_server(THD *thd, LEX_SERVER_OPTIONS *server_options);

/* lookup functions */
FOREIGN_SERVER *get_server_by_name(MEM_ROOT *mem, const char *server_name,
                                   FOREIGN_SERVER *server_buffer);

#endif /* SQL_SERVERS_INCLUDED */
