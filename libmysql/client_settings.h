/* Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
   
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

#ifndef CLIENT_SETTINGS_INCLUDED
#define CLIENT_SETTINGS_INCLUDED
#else
#error You have already included an client_settings.h and it should not be included twice
#endif /* CLIENT_SETTINGS_INCLUDED */

extern uint		mysql_port;
extern char *	mysql_unix_port;

/*
 Note: CLIENT_CAPABILITIES is also defined in sql/client_settings.h.
 When adding capabilities here, consider if they should be also added to
 the server's version.
*/
#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | \
                             CLIENT_LONG_FLAG |     \
                             CLIENT_TRANSACTIONS |  \
                             CLIENT_PROTOCOL_41 | \
                             CLIENT_SECURE_CONNECTION | \
                             CLIENT_MULTI_RESULTS | \
                             CLIENT_PS_MULTI_RESULTS | \
                             CLIENT_PLUGIN_AUTH | \
                             CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA | \
                             CLIENT_CONNECT_ATTRS)

sig_handler my_pipe_sig_handler(int sig);
void read_user_name(char *name);
my_bool handle_local_infile(MYSQL *mysql, const char *net_filename);

void mysql_read_default_options(struct st_mysql_options *options,
				const char *filename,const char *group);
void mysql_detach_stmt_list(LIST **stmt_list, const char *func_name);
MYSQL * STDCALL
cli_mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag);

void cli_mysql_close(MYSQL *mysql);

MYSQL_FIELD * cli_list_fields(MYSQL *mysql);
my_bool cli_read_prepare_result(MYSQL *mysql, MYSQL_STMT *stmt);
MYSQL_DATA * cli_read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
				   uint fields);
int cli_stmt_execute(MYSQL_STMT *stmt);
int cli_read_binary_rows(MYSQL_STMT *stmt);
int cli_unbuffered_fetch(MYSQL *mysql, char **row);
const char * cli_read_statistics(MYSQL *mysql);
int cli_read_change_user_result(MYSQL *mysql);

#ifdef EMBEDDED_LIBRARY
int init_embedded_server(int argc, char **argv, char **groups);
void end_embedded_server();
#endif /*EMBEDDED_LIBRARY*/

C_MODE_START
extern int mysql_init_character_set(MYSQL *mysql);
C_MODE_END
