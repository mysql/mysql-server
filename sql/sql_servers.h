/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "slave.h" // for tables_ok(), rpl_filter

/* structs */
typedef struct st_federated_server
{
  char *server_name;
  long port;
  uint server_name_length;
  char *db, *scheme, *username, *password, *socket, *owner, *host, *sport;
} FOREIGN_SERVER;

/* cache handlers */
my_bool servers_init(bool dont_read_server_table);
my_bool servers_reload(THD *thd);
my_bool get_server_from_table_to_cache(TABLE *table);
void servers_free(bool end=0);

/* insert functions */
int create_server(THD *thd, LEX_SERVER_OPTIONS *server_options);
int insert_server(THD *thd, FOREIGN_SERVER *server_options);
int insert_server_record(TABLE *table, FOREIGN_SERVER *server);
int insert_server_record_into_cache(FOREIGN_SERVER *server);
void store_server_fields_for_insert(TABLE *table, FOREIGN_SERVER *server);
void store_server_fields_for_insert(TABLE *table,
                                    FOREIGN_SERVER *existing,
                                    FOREIGN_SERVER *altered);
int prepare_server_struct_for_insert(LEX_SERVER_OPTIONS *server_options,
                                     FOREIGN_SERVER *server);

/* drop functions */ 
int drop_server(THD *thd, LEX_SERVER_OPTIONS *server_options);
int delete_server_record(TABLE *table,
                         char *server_name,
                         int server_name_length);
int delete_server_record_in_cache(LEX_SERVER_OPTIONS *server_options);

/* update functions */
int alter_server(THD *thd, LEX_SERVER_OPTIONS *server_options);
int prepare_server_struct_for_update(LEX_SERVER_OPTIONS *server_options,
                                     FOREIGN_SERVER *existing,
                                     FOREIGN_SERVER *altered);
int update_server(THD *thd, FOREIGN_SERVER *existing, FOREIGN_SERVER *altered);
int update_server_record(TABLE *table, FOREIGN_SERVER *server);
int update_server_record_in_cache(FOREIGN_SERVER *existing,
                                  FOREIGN_SERVER *altered);
/* utility functions */
void merge_server_struct(FOREIGN_SERVER *from, FOREIGN_SERVER *to);
FOREIGN_SERVER *get_server_by_name(const char *server_name);
my_bool server_exists_in_table(THD *thd, char *server_name);
