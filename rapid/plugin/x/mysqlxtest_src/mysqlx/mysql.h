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

#ifndef _MYSQLX_H_
#define _MYSQLX_H_

typedef char my_bool;
#if !defined(_WIN32)
#define STDCALL
#else
#define STDCALL __stdcall
#endif

#if defined (_WIN32)
typedef unsigned __int64 my_ulonglong;
#else
typedef unsigned long long my_ulonglong;
#endif


typedef struct st_mysql_field {
  char *name;                 /* Name of column */
  char *org_name;             /* Original column name, if an alias */
  char *table;                /* Table of column if column was a field */
  char *org_table;            /* Org table name, if table was an alias */
  char *db;                   /* Database for table */
  char *catalog;              /* Catalog for table */
  char *def;                  /* Default value (set by mysql_list_fields) */
  unsigned long length;       /* Width of column (create length) */
  unsigned long max_length;   /* Max width for selected set */
  unsigned int name_length;
  unsigned int org_name_length;
  unsigned int table_length;
  unsigned int org_table_length;
  unsigned int db_length;
  unsigned int catalog_length;
  unsigned int def_length;
  unsigned int flags;         /* Div flags */
  unsigned int decimals;      /* Number of decimals in field */
  unsigned int charsetnr;     /* Character set */
  enum enum_field_types type; /* Type of field. See mysql_com.h for types */
  void *extension;
} MYSQL_FIELD;

typedef char **MYSQL_ROW;               /* return data as array of strings */
typedef unsigned int MYSQL_FIELD_OFFSET; /* offset to current field */


enum mysql_option
{
  MYSQL_OPT_CONNECT_TIMEOUT, MYSQL_OPT_COMPRESS, MYSQL_OPT_NAMED_PIPE,
  MYSQL_INIT_COMMAND, MYSQL_READ_DEFAULT_FILE, MYSQL_READ_DEFAULT_GROUP,
  MYSQL_SET_CHARSET_DIR, MYSQL_SET_CHARSET_NAME, MYSQL_OPT_LOCAL_INFILE,
  MYSQL_OPT_PROTOCOL, MYSQL_SHARED_MEMORY_BASE_NAME, MYSQL_OPT_READ_TIMEOUT,
  MYSQL_OPT_WRITE_TIMEOUT, MYSQL_OPT_USE_RESULT,
  MYSQL_OPT_USE_REMOTE_CONNECTION, MYSQL_OPT_USE_EMBEDDED_CONNECTION,
  MYSQL_OPT_GUESS_CONNECTION, MYSQL_SET_CLIENT_IP, MYSQL_SECURE_AUTH,
  MYSQL_REPORT_DATA_TRUNCATION, MYSQL_OPT_RECONNECT,
  MYSQL_OPT_SSL_VERIFY_SERVER_CERT, MYSQL_PLUGIN_DIR, MYSQL_DEFAULT_AUTH,
  MYSQL_OPT_BIND,
  MYSQL_OPT_SSL_KEY, MYSQL_OPT_SSL_CERT,
  MYSQL_OPT_SSL_CA, MYSQL_OPT_SSL_CAPATH, MYSQL_OPT_SSL_CIPHER,
  MYSQL_OPT_SSL_CRL, MYSQL_OPT_SSL_CRLPATH,
  MYSQL_OPT_CONNECT_ATTR_RESET, MYSQL_OPT_CONNECT_ATTR_ADD,
  MYSQL_OPT_CONNECT_ATTR_DELETE,
  MYSQL_SERVER_PUBLIC_KEY,
  MYSQL_ENABLE_CLEARTEXT_PLUGIN,
  MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS,
  MYSQL_OPT_SSL_ENFORCE
};




typedef struct st_mysql_res {
  my_ulonglong  row_count;
  MYSQL_FIELD   *fields;
//  MYSQL_DATA    *data;
  MYSQL_ROWS    *data_cursor;
  unsigned long *lengths;
  MYSQL         *handle;
  MYSQL_ROW     row;
  MYSQL_ROW     current_row;
  unsigned int  field_count, current_field;
  my_bool       eof;
  my_bool       unbuffered_fetch_cancelled;
} MYSQL_RES;






unsigned int STDCALL mysql_num_fields(MYSQL_RES *res);
my_ulonglong STDCALL mysql_num_rows(MYSQL_RES *res);
unsigned int STDCALL mysql_num_fields(MYSQL_RES *res);
my_bool STDCALL mysql_eof(MYSQL_RES *res);
MYSQL_FIELD *STDCALL mysql_fetch_field_direct(MYSQL_RES *res,
                                              unsigned int fieldnr);
MYSQL_FIELD * STDCALL mysql_fetch_fields(MYSQL_RES *res);
MYSQL_ROW_OFFSET STDCALL mysql_row_tell(MYSQL_RES *res);
MYSQL_FIELD_OFFSET STDCALL mysql_field_tell(MYSQL_RES *res);

unsigned int STDCALL mysql_field_count(MYSQL *mysql);
my_ulonglong STDCALL mysql_affected_rows(MYSQL *mysql);
my_ulonglong STDCALL mysql_insert_id(MYSQL *mysql);
unsigned int STDCALL mysql_errno(MYSQL *mysql);
const char * STDCALL mysql_error(MYSQL *mysql);

const char *STDCALL mysql_sqlstate(MYSQL *mysql);
unsigned int STDCALL mysql_warning_count(MYSQL *mysql);
const char * STDCALL mysql_info(MYSQL *mysql);
unsigned long STDCALL mysql_thread_id(MYSQL *mysql);

const char * STDCALL mysql_character_set_name(MYSQL *mysql);
int          STDCALL mysql_set_character_set(MYSQL *mysql, const char *csname);



MYSQL *         STDCALL mysql_init(MYSQL *mysql);
my_bool         STDCALL mysql_ssl_set(MYSQL *mysql, const char *key,
                                      const char *cert, const char *ca,
                                      const char *capath, const char *cipher);
const char *    STDCALL mysql_get_ssl_cipher(MYSQL *mysql);
my_bool         STDCALL mysql_change_user(MYSQL *mysql, const char *user,
                                          const char *passwd, const char *db);
MYSQL *         STDCALL mysql_real_connect(MYSQL *mysql, const char *host,
                                           const char *user,
                                           const char *passwd,
                                           const char *db,
                                           unsigned int port,
                                           const char *unix_socket,
                                           unsigned long clientflag);


int             STDCALL mysql_select_db(MYSQL *mysql, const char *db);
int             STDCALL mysql_query(MYSQL *mysql, const char *q);
int             STDCALL mysql_send_query(MYSQL *mysql, const char *q,
                                         unsigned long length);
int             STDCALL mysql_real_query(MYSQL *mysql, const char *q,
                                         unsigned long length);

MYSQL_RES *     STDCALL mysql_store_result(MYSQL *mysql);
MYSQL_RES *     STDCALL mysql_use_result(MYSQL *mysql);

void        STDCALL mysql_get_character_set_info(MYSQL *mysql,
                                                 MY_CHARSET_INFO *charset);


unsigned long STDCALL mysql_real_escape_string(MYSQL *mysql,
                                               char *to,const char *from,
                                               unsigned long length);
unsigned long STDCALL mysql_real_escape_string_quote(MYSQL *mysql,
                                                     char *to, const char *from,
                                                     unsigned long length, char quote);


#endif
