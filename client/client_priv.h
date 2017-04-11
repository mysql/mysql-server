/*
   Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* Common defines for all clients */

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <errmsg.h>
#include <my_getopt.h>

#ifndef WEXITSTATUS
# ifdef __WIN__
#  define WEXITSTATUS(stat_val) (stat_val)
# else
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
# endif
#endif

enum options_client
{
  OPT_CHARSETS_DIR=256, OPT_DEFAULT_CHARSET,
  OPT_PAGER, OPT_TEE,
  OPT_LOW_PRIORITY, OPT_AUTO_REPAIR, OPT_COMPRESS,
  OPT_DROP, OPT_LOCKS, OPT_KEYWORDS, OPT_DELAYED, OPT_OPTIMIZE,
  OPT_FTB, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC, OPT_TABLES,
  OPT_MASTER_DATA, OPT_AUTOCOMMIT, OPT_AUTO_REHASH,
  OPT_LINE_NUMBERS, OPT_COLUMN_NAMES, OPT_CONNECT_TIMEOUT,
  OPT_MAX_ALLOWED_PACKET, OPT_NET_BUFFER_LENGTH,
  OPT_SELECT_LIMIT, OPT_MAX_JOIN_SIZE, OPT_SSL_SSL,
  OPT_SSL_KEY, OPT_SSL_CERT, OPT_SSL_CA, OPT_SSL_CAPATH,
  OPT_SSL_CIPHER, OPT_SHUTDOWN_TIMEOUT, OPT_LOCAL_INFILE,
  OPT_DELETE_MASTER_LOGS, OPT_COMPACT,
  OPT_PROMPT, OPT_IGN_LINES,OPT_TRANSACTION,OPT_MYSQL_PROTOCOL,
  OPT_SHARED_MEMORY_BASE_NAME, OPT_FRM, OPT_SKIP_OPTIMIZATION,
  OPT_COMPATIBLE, OPT_RECONNECT, OPT_DELIMITER, OPT_SECURE_AUTH,
  OPT_OPEN_FILES_LIMIT, OPT_SET_CHARSET, OPT_SERVER_ARG,
  OPT_STOP_POSITION, OPT_START_DATETIME, OPT_STOP_DATETIME,
  OPT_SIGINT_IGNORE, OPT_HEXBLOB, OPT_ORDER_BY_PRIMARY, OPT_COUNT,
  OPT_TRIGGERS,
  OPT_MYSQL_ONLY_PRINT,
  OPT_MYSQL_LOCK_DIRECTORY,
  OPT_USE_THREADS,
  OPT_IMPORT_USE_THREADS,
  OPT_MYSQL_NUMBER_OF_QUERY,
  OPT_IGNORE_TABLE,OPT_INSERT_IGNORE,OPT_SHOW_WARNINGS,OPT_DROP_DATABASE,
  OPT_TZ_UTC, OPT_CREATE_SLAP_SCHEMA,
  OPT_MYSQLDUMP_SLAVE_APPLY,
  OPT_MYSQLDUMP_SLAVE_DATA,
  OPT_MYSQLDUMP_INCLUDE_MASTER_HOST_PORT,
  OPT_SLAP_CSV, OPT_SLAP_CREATE_STRING,
  OPT_SLAP_AUTO_GENERATE_SQL_LOAD_TYPE, OPT_SLAP_AUTO_GENERATE_WRITE_NUM,
  OPT_SLAP_AUTO_GENERATE_ADD_AUTO,
  OPT_SLAP_AUTO_GENERATE_GUID_PRIMARY,
  OPT_SLAP_AUTO_GENERATE_EXECUTE_QUERIES,
  OPT_SLAP_AUTO_GENERATE_SECONDARY_INDEXES,
  OPT_SLAP_AUTO_GENERATE_UNIQUE_WRITE_NUM,
  OPT_SLAP_AUTO_GENERATE_UNIQUE_QUERY_NUM,
  OPT_SLAP_PRE_QUERY,
  OPT_SLAP_POST_QUERY,
  OPT_SLAP_PRE_SYSTEM,
  OPT_SLAP_POST_SYSTEM,
  OPT_SLAP_COMMIT,
  OPT_SLAP_DETACH,
  OPT_SLAP_NO_DROP,
  OPT_MYSQL_REPLACE_INTO, OPT_BASE64_OUTPUT_MODE, OPT_SERVER_ID,
  OPT_FIX_TABLE_NAMES, OPT_FIX_DB_NAMES, OPT_SSL_VERIFY_SERVER_CERT,
  OPT_AUTO_VERTICAL_OUTPUT,
  OPT_DEBUG_INFO, OPT_DEBUG_CHECK, OPT_COLUMN_TYPES, OPT_ERROR_LOG_FILE,
  OPT_WRITE_BINLOG, OPT_DUMP_DATE,
  OPT_INIT_COMMAND,
  OPT_PLUGIN_DIR,
  OPT_DEFAULT_AUTH,
  OPT_DEFAULT_PLUGIN,
  OPT_ENABLE_CLEARTEXT_PLUGIN,
  OPT_SSL_MODE,
  OPT_MAX_CLIENT_OPTION
};

/**
  First mysql version supporting the information schema.
*/
#define FIRST_INFORMATION_SCHEMA_VERSION 50003

/**
  Name of the information schema database.
*/
#define INFORMATION_SCHEMA_DB_NAME "information_schema"

/**
  First mysql version supporting the performance schema.
*/
#define FIRST_PERFORMANCE_SCHEMA_VERSION 50503

/**
  Name of the performance schema database.
*/
#define PERFORMANCE_SCHEMA_DB_NAME "performance_schema"

/**
  Wrapper for mysql_real_connect() that checks if SSL connection is establised.

  The function calls mysql_real_connect() first. Then, if the ssl_required
  argument is TRUE (i.e., the --ssl-mode=REQUIRED option was specified), it
  checks the current SSL cipher to ensure that SSL is used for the current
  connection. Otherwise, it returns NULL and sets errno to
  CR_SSL_CONNECTION_ERROR.

  All clients (except mysqlbinlog, which disregards SSL options) use this
  function instead of mysql_real_connect() to handle the --ssl-mode=REQUIRED
  option.
*/
MYSQL *mysql_connect_ssl_check(MYSQL *mysql_arg, const char *host,
                               const char *user, const char *passwd,
                               const char *db, uint port,
                               const char *unix_socket, ulong client_flag,
                               my_bool ssl_required __attribute__((unused)))
{
  MYSQL *mysql;

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  enum mysql_ssl_mode opt_ssl_mode= SSL_MODE_REQUIRED;
  if (ssl_required &&
      mysql_options(mysql_arg, MYSQL_OPT_SSL_MODE, (char *) &opt_ssl_mode))
  {
    NET *net= &mysql_arg->net;
    net->last_errno= CR_SSL_CONNECTION_ERROR;
    strmov(net->last_error, "Client library doesn't support MYSQL_SSL_REQUIRED option");
    strmov(net->sqlstate, "HY000");
    return NULL;
  }
#endif
  mysql= mysql_real_connect(mysql_arg, host, user, passwd, db, port,
                            unix_socket, client_flag);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if (mysql &&                                   /* connection established. */
      ssl_required &&                            /* --ssl-mode=REQUIRED. */
      !mysql_get_ssl_cipher(mysql))              /* non-SSL connection. */
  {
    NET *net= &mysql->net;
    net->last_errno= CR_SSL_CONNECTION_ERROR;
    strmov(net->last_error, "--ssl-mode=REQUIRED option forbids non SSL connections");
    strmov(net->sqlstate, "HY000");
    return NULL;
  }
#endif
  return mysql;
}
