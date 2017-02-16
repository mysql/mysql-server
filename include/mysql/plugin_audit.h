/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _my_audit_h
#define _my_audit_h

/*************************************************************************
  API for Audit plugin. (MYSQL_AUDIT_PLUGIN)
*/

#include "plugin.h"

#define MYSQL_AUDIT_CLASS_MASK_SIZE 1

#define MYSQL_AUDIT_INTERFACE_VERSION 0x0302


/*************************************************************************
  AUDIT CLASS : GENERAL
  
  LOG events occurs before emitting to the general query log.
  ERROR events occur before transmitting errors to the user. 
  RESULT events occur after transmitting a resultset to the user.
  STATUS events occur after transmitting a resultset or errors
  to the user.
*/

#define MYSQL_AUDIT_GENERAL_CLASS 0
#define MYSQL_AUDIT_GENERAL_CLASSMASK (1 << MYSQL_AUDIT_GENERAL_CLASS)
#define MYSQL_AUDIT_GENERAL_LOG 0
#define MYSQL_AUDIT_GENERAL_ERROR 1
#define MYSQL_AUDIT_GENERAL_RESULT 2
#define MYSQL_AUDIT_GENERAL_STATUS 3

struct mysql_event_general
{
  unsigned int event_subclass;
  int general_error_code;
  unsigned long general_thread_id;
  const char *general_user;
  unsigned int general_user_length;
  const char *general_command;
  unsigned int general_command_length;
  const char *general_query;
  unsigned int general_query_length;
  struct charset_info_st *general_charset;
  unsigned long long general_time;
  unsigned long long general_rows;
  /* Added in version 0x302 */
  unsigned long long query_id;
  const char *database;
  unsigned int database_length;
};


/*
  AUDIT CLASS : CONNECTION
  
  CONNECT occurs after authentication phase is completed.
  DISCONNECT occurs after connection is terminated.
  CHANGE_USER occurs after COM_CHANGE_USER RPC is completed.
*/

#define MYSQL_AUDIT_CONNECTION_CLASS 1
#define MYSQL_AUDIT_CONNECTION_CLASSMASK (1 << MYSQL_AUDIT_CONNECTION_CLASS)
#define MYSQL_AUDIT_CONNECTION_CONNECT 0
#define MYSQL_AUDIT_CONNECTION_DISCONNECT 1
#define MYSQL_AUDIT_CONNECTION_CHANGE_USER 2

struct mysql_event_connection
{
  unsigned int event_subclass;
  int status;
  unsigned long thread_id;
  const char *user;
  unsigned int user_length;
  const char *priv_user;
  unsigned int priv_user_length;
  const char *external_user;
  unsigned int external_user_length;
  const char *proxy_user;
  unsigned int proxy_user_length;
  const char *host;
  unsigned int host_length;
  const char *ip;
  unsigned int ip_length;
  const char *database;
  unsigned int database_length;
};

/*
  AUDIT CLASS : TABLE
  
  LOCK occurs when a connection "locks" (this does not necessarily mean a table
  lock and also happens for row-locking engines) the table at the beginning of
  a statement. This event is generated at the beginning of every statement for
  every affected table, unless there's a LOCK TABLES statement in effect (in
  which case it is generated once for LOCK TABLES and then is suppressed until
  the tables are unlocked).

  CREATE/DROP/RENAME occur when a table is created, dropped, or renamed.
*/

#define MYSQL_AUDIT_TABLE_CLASS 15
#define MYSQL_AUDIT_TABLE_CLASSMASK (1 << MYSQL_AUDIT_TABLE_CLASS)
#define MYSQL_AUDIT_TABLE_LOCK   0
#define MYSQL_AUDIT_TABLE_CREATE 1
#define MYSQL_AUDIT_TABLE_DROP   2
#define MYSQL_AUDIT_TABLE_RENAME 3
#define MYSQL_AUDIT_TABLE_ALTER  4

struct mysql_event_table
{
  unsigned int event_subclass;
  unsigned long thread_id;
  const char *user;
  const char *priv_user;
  const char *priv_host;
  const char *external_user;
  const char *proxy_user;
  const char *host;
  const char *ip;
  const char *database;
  unsigned int database_length;
  const char *table;
  unsigned int table_length;
  /* for MYSQL_AUDIT_TABLE_LOCK, true if read-only, false if read/write */
  int read_only;
  /* for MYSQL_AUDIT_TABLE_RENAME */
  const char *new_database;
  unsigned int new_database_length;
  const char *new_table;
  unsigned int new_table_length;
  /* Added in version 0x302 */
  unsigned long long query_id;
};

/*************************************************************************
  Here we define the descriptor structure, that is referred from
  st_mysql_plugin.

  release_thd() event occurs when the event class consumer is to be
  disassociated from the specified THD. This would typically occur
  before some operation which may require sleeping - such as when
  waiting for the next query from the client.
  
  event_notify() is invoked whenever an event occurs which is of any
  class for which the plugin has interest. The second argument
  indicates the specific event class and the third argument is data
  as required for that class.
  
  class_mask is an array of bits used to indicate what event classes
  that this plugin wants to receive.
*/

struct st_mysql_audit
{
  int interface_version;
  void (*release_thd)(MYSQL_THD);
  void (*event_notify)(MYSQL_THD, unsigned int, const void *);
  unsigned long class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
};


#endif
