/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_HOST_CACHE_H
#define TABLE_HOST_CACHE_H

/**
  @file storage/perfschema/table_host_cache.h
  Table HOST_CACHE (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"

class Host_entry;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.HOST_CACHE. */
struct row_host_cache
{
  /** Column IP. */
  char m_ip[64];
  uint m_ip_length;
  /** Column HOST. */
  char m_hostname[255];
  uint m_hostname_length;
  /** Column HOST_VALIDATED. */
  bool m_host_validated;
  /** Column SUM_CONNECT_ERRORS. */
  ulonglong m_sum_connect_errors;
  /** Column COUNT_HOST_BLOCKED_ERRORS. */
  ulonglong m_count_host_blocked_errors;
  /** Column COUNT_NAMEINFO_TRANSIENT_ERRORS. */
  ulonglong m_count_nameinfo_transient_errors;
  /** Column COUNT_NAMEINFO_PERMANENT_ERRORS. */
  ulonglong m_count_nameinfo_permanent_errors;
  /** Column COUNT_FORMAT_ERRORS. */
  ulonglong m_count_format_errors;
  /** Column COUNT_ADDRINFO_TRANSIENT_ERRORS. */
  ulonglong m_count_addrinfo_transient_errors;
  /** Column COUNT_ADDRINFO_PERMANENT_ERRORS. */
  ulonglong m_count_addrinfo_permanent_errors;
  /** Column COUNT_FCRDNS_ERRORS. */
  ulonglong m_count_fcrdns_errors;
  /** Column COUNT_HOST_ACL_ERRORS. */
  ulonglong m_count_host_acl_errors;
  /** Column COUNT_NO_AUTH_PLUGIN_ERRORS. */
  ulonglong m_count_no_auth_plugin_errors;
  /** Column COUNT_AUTH_PLUGIN_ERRORS. */
  ulonglong m_count_auth_plugin_errors;
  /** Column COUNT_HANDSHAKE_ERRORS. */
  ulonglong m_count_handshake_errors;
  /** Column COUNT_PROXY_USER_ERRORS. */
  ulonglong m_count_proxy_user_errors;
  /** Column COUNT_PROXY_USER_ACL_ERRORS. */
  ulonglong m_count_proxy_user_acl_errors;
  /** Column COUNT_AUTHENTICATION_ERRORS. */
  ulonglong m_count_authentication_errors;
  /** Column COUNT_SSL_ERRORS. */
  ulonglong m_count_ssl_errors;
  /** Column COUNT_MAX_USER_CONNECTION_ERRORS. */
  ulonglong m_count_max_user_connection_errors;
  /** Column COUNT_MAX_USER_CONNECTION_PER_HOUR_ERRORS. */
  ulonglong m_count_max_user_connection_per_hour_errors;
  /** Column COUNT_DEFAULT_DATABASE_ERRORS. */
  ulonglong m_count_default_database_errors;
  /** Column COUNT_INIT_CONNECT_ERRORS. */
  ulonglong m_count_init_connect_errors;
  /** Column COUNT_LOCAL_ERRORS. */
  ulonglong m_count_local_errors;
  /** Column COUNT_UNKNOWN_ERRORS. */
  ulonglong m_count_unknown_errors;
  /** Column FIRST_SEEN. */
  ulonglong m_first_seen;
  /** Column LAST_SEEN. */
  ulonglong m_last_seen;
  /** Column FIRST_ERROR_SEEN. */
  ulonglong m_first_error_seen;
  /** Column LAST_ERROR_SEEN. */
  ulonglong m_last_error_seen;
};

/** Table PERFORMANCE_SCHEMA.HOST_CACHE. */
class table_host_cache : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_host_cache();

public:
  ~table_host_cache()
  {}

private:
  void materialize(THD *thd);
  static void make_row(Host_entry *entry, row_host_cache *row);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  row_host_cache *m_all_rows;
  uint m_row_count;
  /** Current row. */
  row_host_cache *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
