/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Prototypes for the embedded version of MySQL */

C_MODE_START
void lib_connection_phase(NET *net, int phase);
void init_embedded_mysql(MYSQL *mysql, int client_flag);
void *create_embedded_thd(int client_flag);
int check_embedded_connection(MYSQL *mysql, const char *db);
void free_old_query(MYSQL *mysql);
extern MYSQL_METHODS embedded_methods;

/* This one is used by embedded library to gather returning data */
typedef struct embedded_query_result
{
  MYSQL_ROWS **prev_ptr;
  unsigned int warning_count, server_status;
  struct st_mysql_data *next;
  my_ulonglong affected_rows, insert_id;
  char info[MYSQL_ERRMSG_SIZE];
  MYSQL_FIELD *fields_list;
  unsigned int last_errno;
  char sqlstate[SQLSTATE_LENGTH+1];
} EQR;

C_MODE_END
