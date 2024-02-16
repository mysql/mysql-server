/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef OPTIONS_MYSQLD_INCLUDED
#define OPTIONS_MYSQLD_INCLUDED

/**
  only options that need special treatment in get_one_option() deserve
  to be listed below
*/
enum options_mysqld {
  OPT_to_set_the_start_number = 256,
  OPT_BIND_ADDRESS,
  OPT_BINLOG_CHECKSUM,
  OPT_BINLOG_DO_DB,
  OPT_BINLOG_FORMAT,
  OPT_BINLOG_IGNORE_DB,
  OPT_BINLOG_MAX_FLUSH_QUEUE_TIME,
  OPT_BINLOG_EXPIRE_LOGS_SECONDS,
  OPT_BIN_LOG,
  OPT_BOOTSTRAP,
  OPT_CONSOLE,
  OPT_DEBUG_SYNC_TIMEOUT,
  OPT_DELAY_KEY_WRITE_ALL,
  OPT_ISAM_LOG,
  OPT_IGNORE_DB_DIRECTORY,
  OPT_KEY_BUFFER_SIZE,
  OPT_KEY_CACHE_AGE_THRESHOLD,
  OPT_KEY_CACHE_BLOCK_SIZE,
  OPT_KEY_CACHE_DIVISION_LIMIT,
  OPT_LC_MESSAGES_DIRECTORY,
  OPT_LOWER_CASE_TABLE_NAMES,
  OPT_MASTER_RETRY_COUNT,
  OPT_SOURCE_VERIFY_CHECKSUM,
  OPT_POOL_OF_THREADS,
  OPT_REPLICATE_DO_DB,
  OPT_REPLICATE_DO_TABLE,
  OPT_REPLICATE_IGNORE_DB,
  OPT_REPLICATE_IGNORE_TABLE,
  OPT_REPLICATE_REWRITE_DB,
  OPT_REPLICATE_WILD_DO_TABLE,
  OPT_REPLICATE_WILD_IGNORE_TABLE,
  OPT_SERVER_ID,
  OPT_SKIP_LOCK,
  OPT_SKIP_NEW,
  OPT_SKIP_RESOLVE,
  OPT_SKIP_STACK_TRACE,
  OPT_SKIP_SYMLINKS,
  OPT_REPLICA_SQL_VERIFY_CHECKSUM,
  OPT_SSL_CIPHER,
  OPT_TLS_CIPHERSUITES,
  OPT_TLS_VERSION,
  OPT_UPDATE_LOG,
  OPT_WANT_CORE,
  OPT_LOG_ERROR,
  OPT_MAX_LONG_DATA_SIZE,
  OPT_PLUGIN_LOAD,
  OPT_PLUGIN_LOAD_ADD,
  OPT_PFS_INSTRUMENT,
  OPT_DEFAULT_AUTH,
  OPT_THREAD_CACHE_SIZE,
  OPT_HOST_CACHE_SIZE,
  OPT_TABLE_DEFINITION_CACHE,
  OPT_ENFORCE_GTID_CONSISTENCY,
  OPT_INSTALL_SERVER,
  OPT_EARLY_PLUGIN_LOAD,
  OPT_KEYRING_MIGRATION_SOURCE,
  OPT_KEYRING_MIGRATION_DESTINATION,
  OPT_KEYRING_MIGRATION_USER,
  OPT_KEYRING_MIGRATION_HOST,
  OPT_KEYRING_MIGRATION_PASSWORD,
  OPT_KEYRING_MIGRATION_SOCKET,
  OPT_KEYRING_MIGRATION_PORT,
  OPT_LOG_REPLICA_UPDATES,
  OPT_REPLICA_PRESERVE_COMMIT_ORDER,
  OPT_LOG_SLOW_EXTRA,
  OPT_NAMED_PIPE_FULL_ACCESS_GROUP,
  OPT_ADMIN_SSL_CA,
  OPT_ADMIN_SSL_CAPATH,
  OPT_ADMIN_SSL_CERT,
  OPT_ADMIN_SSL_CIPHER,
  OPT_ADMIN_TLS_CIPHERSUITES,
  OPT_ADMIN_TLS_VERSION,
  OPT_ADMIN_SSL_KEY,
  OPT_ADMIN_SSL_CRL,
  OPT_ADMIN_SSL_CRLPATH,
  OPT_KEYRING_MIGRATION_TO_COMPONENT,
  OPT_KEYRING_MIGRATION_FROM_COMPONENT,
  OPT_SHOW_SLAVE_AUTH_INFO_DEPRECATED,
  OPT_REPLICA_PARALLEL_TYPE,
  OPT_REPLICA_PARALLEL_WORKERS,
  OPT_SYNC_RELAY_LOG_INFO,
  OPT_CHARACTER_SET_CLIENT_HANDSHAKE
};

#endif  // OPTIONS_MYSQLD_INCLUDED
