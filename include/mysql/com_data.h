/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */
#ifndef PLUGIN_PROTOCOL_INCLUDED
#define PLUGIN_PROTOCOL_INCLUDED

#ifndef MYSQL_ABI_CHECK
#include "my_global.h" /* Needed for my_bool in mysql_com.h */
#include "mysql_com.h" /* mysql_enum_shutdown_level */
#endif


/**
@file
  Definition of COM_DATA to be used with the Command service as data input
  structure.
*/


typedef struct st_com_init_db_data
{
  const char *db_name;
  unsigned long length;
} COM_INIT_DB_DATA;

typedef struct st_com_refresh_data
{
  unsigned char options;
} COM_REFRESH_DATA;

typedef struct st_com_shutdown_data
{
  enum mysql_enum_shutdown_level level;
} COM_SHUTDOWN_DATA;

typedef struct st_com_kill_data
{
  unsigned long id;
} COM_KILL_DATA;

typedef struct st_com_set_option_data
{
  unsigned int opt_command;
} COM_SET_OPTION_DATA;

typedef struct st_com_stmt_execute_data
{
  unsigned long stmt_id;
  unsigned long flags;
  unsigned char *params;
  unsigned long params_length;
} COM_STMT_EXECUTE_DATA;

typedef struct st_com_stmt_fetch_data
{
  unsigned long stmt_id;
  unsigned long num_rows;
} COM_STMT_FETCH_DATA;

typedef struct st_com_stmt_send_long_data_data
{
  unsigned long stmt_id;
  unsigned int  param_number;
  unsigned char *longdata;
  unsigned long length;
} COM_STMT_SEND_LONG_DATA_DATA;

typedef struct st_com_stmt_prepare_data
{
  const char *query;
  unsigned int length;
} COM_STMT_PREPARE_DATA;

typedef struct st_stmt_close_data
{
  unsigned int stmt_id;
} COM_STMT_CLOSE_DATA;

typedef struct st_com_stmt_reset_data
{
  unsigned int stmt_id;
} COM_STMT_RESET_DATA;

typedef struct st_com_query_data
{
  const char *query;
  unsigned int length;
} COM_QUERY_DATA;

typedef struct st_com_field_list_data
{
  unsigned char   *table_name;
  unsigned int    table_name_length;
  const unsigned char *query;
  unsigned int        query_length;
} COM_FIELD_LIST_DATA;

union COM_DATA {
  COM_INIT_DB_DATA com_init_db;
  COM_REFRESH_DATA com_refresh;
  COM_SHUTDOWN_DATA com_shutdown;
  COM_KILL_DATA com_kill;
  COM_SET_OPTION_DATA com_set_option;
  COM_STMT_EXECUTE_DATA com_stmt_execute;
  COM_STMT_FETCH_DATA com_stmt_fetch;
  COM_STMT_SEND_LONG_DATA_DATA com_stmt_send_long_data;
  COM_STMT_PREPARE_DATA com_stmt_prepare;
  COM_STMT_CLOSE_DATA com_stmt_close;
  COM_STMT_RESET_DATA com_stmt_reset;
  COM_QUERY_DATA com_query;
  COM_FIELD_LIST_DATA com_field_list;
};

#endif /* PLUGIN_PROTOCOL_INCLUDED */
