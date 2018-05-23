#ifndef SQL_COMMON_INCLUDED
#define SQL_COMMON_INCLUDED

/* Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.
   
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

#define SQL_COMMON_INCLUDED

#ifdef	__cplusplus
extern "C" {
#endif

#include <mysql.h>
#include <hash.h>

extern const char	*unknown_sqlstate;
extern const char	*cant_connect_sqlstate;
extern const char	*not_error_sqlstate;


/*
  Free all memory allocated in MYSQL handle except the
  current options.
*/
void mysql_close_free(MYSQL *mysql);

/*
  Clear connection options stored in MYSQL handle and
  free memory used by them.
*/
void mysql_close_free_options(MYSQL *mysql);


/**
  The structure is used to hold the state change information
  received from the server. LIST functions are used for manipulation
  of the members of the structure.
*/
typedef struct st_session_track_info_node {
  /** head_node->data is a LEX_STRING which contains the variable name. */
  LIST *head_node;
  LIST *current_node;
} STATE_INFO_NODE;

/**
  Store the change info received from the server in an array of linked lists
  with STATE_INFO_NODE elements (one per state type).
*/
typedef struct st_session_track_info {
  /** Array of STATE_NODE_INFO elements (one per state type). */
  struct st_session_track_info_node info_list[SESSION_TRACK_END + 1];
} STATE_INFO;

/*
  Access to MYSQL::extension member.

  Note: functions mysql_extension_{init,free}() are defined
  in client.c.
*/

struct st_mysql_trace_info;

typedef struct st_mysql_extension {
  struct st_mysql_trace_info *trace_data;
  struct st_session_track_info state_change;
} MYSQL_EXTENSION;

/* "Constructor/destructor" for MYSQL extension structure. */
struct st_mysql_extension* mysql_extension_init(struct st_mysql*);
void mysql_extension_free(struct st_mysql_extension*);

/*
  Note: Allocated extension structure is freed in mysql_close_free()
  called by mysql_close().
*/
#define MYSQL_EXTENSION_PTR(H)                                    \
(                                                                 \
 (struct st_mysql_extension*)                                     \
 ( (H)->extension ?                                               \
   (H)->extension : ((H)->extension= mysql_extension_init(H))     \
 )                                                                \
)


struct st_mysql_options_extention {
  char *plugin_dir;
  char *default_auth;
  char *ssl_crl;				/* PEM CRL file */
  char *ssl_crlpath;				/* PEM directory of CRL-s? */
  HASH connection_attributes;
  char *server_public_key_path;
  size_t connection_attributes_length;
  my_bool enable_cleartext_plugin;
  my_bool unused0;                              /* Former ssl_enforce */
  char *tls_version; /* TLS version option */
  long ssl_ctx_flags; /* SSL ctx options flag */
  unsigned int ssl_mode;
};

typedef struct st_mysql_methods
{
  my_bool (*read_query_result)(MYSQL *mysql);
  my_bool (*advanced_command)(MYSQL *mysql,
			      enum enum_server_command command,
			      const unsigned char *header,
			      size_t header_length,
			      const unsigned char *arg,
			      size_t arg_length,
			      my_bool skip_check,
                              MYSQL_STMT *stmt);
  MYSQL_DATA *(*read_rows)(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
			   unsigned int fields);
  MYSQL_RES * (*use_result)(MYSQL *mysql);
  void (*fetch_lengths)(unsigned long *to, 
			MYSQL_ROW column, unsigned int field_count);
  void (*flush_use_result)(MYSQL *mysql, my_bool flush_all_results);
  int (*read_change_user_result)(MYSQL *mysql);
#if !defined(MYSQL_SERVER) || defined(EMBEDDED_LIBRARY)
  MYSQL_FIELD * (*list_fields)(MYSQL *mysql);
  my_bool (*read_prepare_result)(MYSQL *mysql, MYSQL_STMT *stmt);
  int (*stmt_execute)(MYSQL_STMT *stmt);
  int (*read_binary_rows)(MYSQL_STMT *stmt);
  int (*unbuffered_fetch)(MYSQL *mysql, char **row);
  void (*free_embedded_thd)(MYSQL *mysql);
  const char *(*read_statistics)(MYSQL *mysql);
  my_bool (*next_result)(MYSQL *mysql);
  int (*read_rows_from_cursor)(MYSQL_STMT *stmt);
  void (*free_rows)(MYSQL_DATA *cur);
#endif
} MYSQL_METHODS;

#define simple_command(mysql, command, arg, length, skip_check) \
  ((mysql)->methods \
    ? (*(mysql)->methods->advanced_command)(mysql, command, 0, \
                                            0, arg, length, skip_check, NULL) \
    : (set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate), 1))
#define stmt_command(mysql, command, arg, length, stmt) \
  ((mysql)->methods \
    ? (*(mysql)->methods->advanced_command)(mysql, command, 0,  \
                                           0, arg, length, 1, stmt) \
    : (set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate), 1))

extern CHARSET_INFO *default_client_charset_info;
MYSQL_FIELD *unpack_fields(MYSQL *mysql, MYSQL_ROWS *data,MEM_ROOT *alloc,
                           uint fields, my_bool default_value,
                           uint server_capabilities);
MYSQL_FIELD * cli_read_metadata_ex(MYSQL *mysql, MEM_ROOT *alloc,
                                   unsigned long field_count,
                                   unsigned int fields);
MYSQL_FIELD * cli_read_metadata(MYSQL *mysql, unsigned long field_count,
                               unsigned int fields);
void free_rows(MYSQL_DATA *cur);
void free_old_query(MYSQL *mysql);
void end_server(MYSQL *mysql);
my_bool mysql_reconnect(MYSQL *mysql);
void mysql_read_default_options(struct st_mysql_options *options,
				const char *filename,const char *group);
my_bool
cli_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const unsigned char *header, size_t header_length,
		     const unsigned char *arg, size_t arg_length,
                     my_bool skip_check, MYSQL_STMT *stmt);
unsigned long cli_safe_read(MYSQL *mysql, my_bool *is_data_packet);
unsigned long cli_safe_read_with_ok(MYSQL *mysql, my_bool parse_ok,
                                    my_bool *is_data_packet);
void net_clear_error(NET *net);
void set_stmt_errmsg(MYSQL_STMT *stmt, NET *net);
void set_stmt_error(MYSQL_STMT *stmt, int errcode, const char *sqlstate,
                    const char *err);
void set_mysql_error(MYSQL *mysql, int errcode, const char *sqlstate);
void set_mysql_extended_error(MYSQL *mysql, int errcode, const char *sqlstate,
                              const char *format, ...);
#ifdef EMBEDDED_LIBRARY
int embedded_ssl_check(MYSQL *mysql);
#endif

/* client side of the pluggable authentication */
struct st_plugin_vio_info;
void mpvio_info(Vio *vio, struct st_plugin_vio_info *info);
int run_plugin_auth(MYSQL *mysql, char *data, uint data_len,
                    const char *data_plugin, const char *db);
int mysql_client_plugin_init();
void mysql_client_plugin_deinit();

struct st_mysql_client_plugin;
extern struct st_mysql_client_plugin *mysql_client_builtins[];
uchar * send_client_connect_attrs(MYSQL *mysql, uchar *buf);
extern my_bool libmysql_cleartext_plugin_enabled;
int is_file_or_dir_world_writable(const char *filepath);
void read_ok_ex(MYSQL *mysql, unsigned long len);

#ifdef	__cplusplus
}
#endif

#define protocol_41(A) ((A)->server_capabilities & CLIENT_PROTOCOL_41)

#endif /* SQL_COMMON_INCLUDED */
