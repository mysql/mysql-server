/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "mysqlx/mysql.h"
#include "mysqld_error.h"
#include "ngs_common/protocol_protobuf.h"

class Mysqlx_test_connector;

struct st_mysql_priv
{
  Mysqlx_test_connector *impl;
};

typedef st_mysql_priv MYSQL;


typedef struct st_mysql
{
  struct st_mysql_priv *impl;
  int last_error_no;
  char *last_error;

  my_bool free_me;		/* If free in mysql_close */
#if 0
  NET		net;			/* Communication parameters */
  unsigned char	*connector_fd;		/* ConnectorFd for SSL */
  char		*host,*user,*passwd,*unix_socket,*server_version,*host_info;
  char          *info, *db;
  struct charset_info_st *charset;
  MYSQL_FIELD	*fields;
  MEM_ROOT	field_alloc;
  my_ulonglong affected_rows;
  my_ulonglong insert_id;		/* id if insert on table with NEXTNR */
  my_ulonglong extra_info;		/* Not used */
  unsigned long thread_id;		/* Id for connection in server */
  unsigned long packet_length;
  unsigned int	port;
  unsigned long client_flag,server_capabilities;
  unsigned int	protocol_version;
  unsigned int	field_count;
  unsigned int 	server_status;
  unsigned int  server_language;
  unsigned int	warning_count;
  struct st_mysql_options options;
  enum mysql_status status;

  my_bool	reconnect;		/* set to 1 if automatic reconnect */


  LIST  *stmts;                     /* list of all statements */
  const struct st_mysql_methods *methods;
  void *thd;
  /*
   Points to boolean flag in MYSQL_RES  or MYSQL_STMT. We set this flag
   from mysql_stmt_close if close had to cancel result set of this object.
   */
  my_bool *unbuffered_fetch_owner;
  /* needed for embedded server - no net buffer to store the 'info' */
  char *info_buffer;
  void *extension;
#endif
} MYSQL;




