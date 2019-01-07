/* Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_host_cache.cc
  Table HOST_CACHE (implementation).
*/

#include "storage/perfschema/table_host_cache.h"

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/hostname.h"
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/table.h"

THR_LOCK table_host_cache::m_table_lock;

Plugin_table table_host_cache::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "host_cache",
    /* Definition */
    "  IP VARCHAR(64) not null,\n"
    "  HOST VARCHAR(255) collate utf8mb4_bin,\n"
    "  HOST_VALIDATED ENUM ('YES', 'NO') not null,\n"
    "  SUM_CONNECT_ERRORS BIGINT not null,\n"
    "  COUNT_HOST_BLOCKED_ERRORS BIGINT not null,\n"
    "  COUNT_NAMEINFO_TRANSIENT_ERRORS BIGINT not null,\n"
    "  COUNT_NAMEINFO_PERMANENT_ERRORS BIGINT not null,\n"
    "  COUNT_FORMAT_ERRORS BIGINT not null,\n"
    "  COUNT_ADDRINFO_TRANSIENT_ERRORS BIGINT not null,\n"
    "  COUNT_ADDRINFO_PERMANENT_ERRORS BIGINT not null,\n"
    "  COUNT_FCRDNS_ERRORS BIGINT not null,\n"
    "  COUNT_HOST_ACL_ERRORS BIGINT not null,\n"
    "  COUNT_NO_AUTH_PLUGIN_ERRORS BIGINT not null,\n"
    "  COUNT_AUTH_PLUGIN_ERRORS BIGINT not null,\n"
    "  COUNT_HANDSHAKE_ERRORS BIGINT not null,\n"
    "  COUNT_PROXY_USER_ERRORS BIGINT not null,\n"
    "  COUNT_PROXY_USER_ACL_ERRORS BIGINT not null,\n"
    "  COUNT_AUTHENTICATION_ERRORS BIGINT not null,\n"
    "  COUNT_SSL_ERRORS BIGINT not null,\n"
    "  COUNT_MAX_USER_CONNECTIONS_ERRORS BIGINT not null,\n"
    "  COUNT_MAX_USER_CONNECTIONS_PER_HOUR_ERRORS BIGINT not null,\n"
    "  COUNT_DEFAULT_DATABASE_ERRORS BIGINT not null,\n"
    "  COUNT_INIT_CONNECT_ERRORS BIGINT not null,\n"
    "  COUNT_LOCAL_ERRORS BIGINT not null,\n"
    "  COUNT_UNKNOWN_ERRORS BIGINT not null,\n"
    "  FIRST_SEEN TIMESTAMP(0) NOT NULL default 0,\n"
    "  LAST_SEEN TIMESTAMP(0) NOT NULL default 0,\n"
    "  FIRST_ERROR_SEEN TIMESTAMP(0) null default 0,\n"
    "  LAST_ERROR_SEEN TIMESTAMP(0) null default 0,\n"
    "  PRIMARY KEY (IP) USING HASH,\n"
    "  KEY (HOST) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_host_cache::m_share = {
    &pfs_truncatable_acl,
    table_host_cache::create,
    NULL, /* write_row */
    table_host_cache::delete_all_rows,
    table_host_cache::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_host_cache_by_ip::match(const row_host_cache *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row->m_ip, row->m_ip_length)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_host_cache_by_host::match(const row_host_cache *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row->m_hostname, row->m_hostname_length)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_host_cache::create(PFS_engine_table_share *) {
  table_host_cache *t = new table_host_cache();
  if (t != NULL) {
    THD *thd = current_thd;
    DBUG_ASSERT(thd != NULL);
    t->materialize(thd);
  }
  return t;
}

int table_host_cache::delete_all_rows(void) {
  /*
    TRUNCATE TABLE performance_schema.host_cache
    is an alternate syntax for
    FLUSH HOSTS
  */
  hostname_cache_refresh();
  return 0;
}

ha_rows table_host_cache::get_row_count(void) {
  ha_rows count;
  hostname_cache_lock();
  count = hostname_cache_size();
  hostname_cache_unlock();
  return count;
}

table_host_cache::table_host_cache()
    : PFS_engine_table(&m_share, &m_pos),
      m_all_rows(NULL),
      m_row_count(0),
      m_row(NULL),
      m_pos(0),
      m_next_pos(0) {}

void table_host_cache::materialize(THD *thd) {
  uint size;
  uint index;
  row_host_cache *rows;
  row_host_cache *row;

  DBUG_ASSERT(m_all_rows == NULL);
  DBUG_ASSERT(m_row_count == 0);

  hostname_cache_lock();

  size = hostname_cache_size();
  if (size == 0) {
    /* Normal case, the cache is empty. */
    goto end;
  }

  rows = (row_host_cache *)thd->alloc(size * sizeof(row_host_cache));
  if (rows == NULL) {
    /* Out of memory, this thread will error out. */
    goto end;
  }

  index = 0;
  row = rows;

  {
    auto end = hostname_cache_end();
    for (auto it = hostname_cache_begin(); it != end; ++it) {
      make_row(it->get(), row);
      index++;
      row++;
    }
  }

  m_all_rows = rows;
  m_row_count = index;

end:
  hostname_cache_unlock();
}

int table_host_cache::make_row(Host_entry *entry, row_host_cache *row) {
  row->m_ip_length = (uint)strlen(entry->ip_key);
  strcpy(row->m_ip, entry->ip_key);
  row->m_hostname_length = entry->m_hostname_length;
  if (row->m_hostname_length > 0) {
    strncpy(row->m_hostname, entry->m_hostname, row->m_hostname_length);
  }
  row->m_host_validated = entry->m_host_validated;
  row->m_sum_connect_errors = entry->m_errors.m_connect;
  row->m_count_host_blocked_errors = entry->m_errors.m_host_blocked;
  row->m_count_nameinfo_transient_errors = entry->m_errors.m_nameinfo_transient;
  row->m_count_nameinfo_permanent_errors = entry->m_errors.m_nameinfo_permanent;
  row->m_count_format_errors = entry->m_errors.m_format;
  row->m_count_addrinfo_transient_errors = entry->m_errors.m_addrinfo_transient;
  row->m_count_addrinfo_permanent_errors = entry->m_errors.m_addrinfo_permanent;
  row->m_count_fcrdns_errors = entry->m_errors.m_FCrDNS;
  row->m_count_host_acl_errors = entry->m_errors.m_host_acl;
  row->m_count_no_auth_plugin_errors = entry->m_errors.m_no_auth_plugin;
  row->m_count_auth_plugin_errors = entry->m_errors.m_auth_plugin;
  row->m_count_handshake_errors = entry->m_errors.m_handshake;
  row->m_count_proxy_user_errors = entry->m_errors.m_proxy_user;
  row->m_count_proxy_user_acl_errors = entry->m_errors.m_proxy_user_acl;
  row->m_count_authentication_errors = entry->m_errors.m_authentication;
  row->m_count_ssl_errors = entry->m_errors.m_ssl;
  row->m_count_max_user_connection_errors =
      entry->m_errors.m_max_user_connection;
  row->m_count_max_user_connection_per_hour_errors =
      entry->m_errors.m_max_user_connection_per_hour;
  row->m_count_default_database_errors = entry->m_errors.m_default_database;
  row->m_count_init_connect_errors = entry->m_errors.m_init_connect;
  row->m_count_local_errors = entry->m_errors.m_local;

  /*
    Reserved for future use, to help with backward compatibility.
    When new errors are added in entry->m_errors.m_xxx,
    report them in this column (GA releases),
    until the table HOST_CACHE structure can be extended (next development
    version).
  */
  row->m_count_unknown_errors = 0;

  row->m_first_seen = entry->m_first_seen;
  row->m_last_seen = entry->m_last_seen;
  row->m_first_error_seen = entry->m_first_error_seen;
  row->m_last_error_seen = entry->m_last_error_seen;

  return 0;
}

void table_host_cache::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_host_cache::rnd_next(void) {
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < m_row_count) {
    m_row = &m_all_rows[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    result = 0;
  } else {
    m_row = NULL;
    result = HA_ERR_END_OF_FILE;
  }

  return result;
}

int table_host_cache::rnd_pos(const void *pos) {
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_row_count);
  m_row = &m_all_rows[m_pos.m_index];
  return 0;
}

int table_host_cache::index_init(uint idx, bool) {
  PFS_index_host_cache *result = NULL;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_host_cache_by_ip);
      break;
    case 1:
      result = PFS_NEW(PFS_index_host_cache_by_host);
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_host_cache::index_next(void) {
  int result;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_row_count; m_pos.next()) {
    m_row = &m_all_rows[m_pos.m_index];

    if (m_opened_index->match(m_row)) {
      m_next_pos.set_after(&m_pos);
      result = 0;
      return result;
    }
  }

  m_row = NULL;
  result = HA_ERR_END_OF_FILE;

  return result;
}

int table_host_cache::read_row_values(TABLE *table, unsigned char *buf,
                                      Field **fields, bool read_all) {
  Field *f;

  DBUG_ASSERT(m_row);

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* IP */
          set_field_varchar_utf8(f, m_row->m_ip, m_row->m_ip_length);
          break;
        case 1: /* HOST */
          if (m_row->m_hostname_length > 0)
            set_field_varchar_utf8(f, m_row->m_hostname,
                                   m_row->m_hostname_length);
          else {
            f->set_null();
          }
          break;
        case 2: /* HOST_VALIDATED */
          set_field_enum(f, m_row->m_host_validated ? ENUM_YES : ENUM_NO);
          break;
        case 3: /* SUM_CONNECT_ERRORS */
          set_field_ulonglong(f, m_row->m_sum_connect_errors);
          break;
        case 4: /* COUNT_HOST_BLOCKED_ERRORS. */
          set_field_ulonglong(f, m_row->m_count_host_blocked_errors);
          break;
        case 5: /* COUNT_NAMEINFO_TRANSIENT_ERRORS */
          set_field_ulonglong(f, m_row->m_count_nameinfo_transient_errors);
          break;
        case 6: /* COUNT_NAMEINFO_PERSISTENT_ERRORS */
          set_field_ulonglong(f, m_row->m_count_nameinfo_permanent_errors);
          break;
        case 7: /* COUNT_FORMAT_ERRORS */
          set_field_ulonglong(f, m_row->m_count_format_errors);
          break;
        case 8: /* COUNT_ADDRINFO_TRANSIENT_ERRORS */
          set_field_ulonglong(f, m_row->m_count_addrinfo_transient_errors);
          break;
        case 9: /* COUNT_ADDRINFO_PERSISTENT_ERRORS */
          set_field_ulonglong(f, m_row->m_count_addrinfo_permanent_errors);
          break;
        case 10: /* COUNT_FCRDNS_ERRORS */
          set_field_ulonglong(f, m_row->m_count_fcrdns_errors);
          break;
        case 11: /* COUNT_HOST_ACL_ERRORS */
          set_field_ulonglong(f, m_row->m_count_host_acl_errors);
          break;
        case 12: /* COUNT_NO_AUTH_PLUGIN_ERRORS */
          set_field_ulonglong(f, m_row->m_count_no_auth_plugin_errors);
          break;
        case 13: /* COUNT_AUTH_PLUGIN_ERRORS */
          set_field_ulonglong(f, m_row->m_count_auth_plugin_errors);
          break;
        case 14: /* COUNT_HANDSHAKE_ERRORS */
          set_field_ulonglong(f, m_row->m_count_handshake_errors);
          break;
        case 15: /* COUNT_PROXY_USER_ERRORS */
          set_field_ulonglong(f, m_row->m_count_proxy_user_errors);
          break;
        case 16: /* COUNT_PROXY_USER_ACL_ERRORS */
          set_field_ulonglong(f, m_row->m_count_proxy_user_acl_errors);
          break;
        case 17: /* COUNT_AUTHENTICATION_ERRORS */
          set_field_ulonglong(f, m_row->m_count_authentication_errors);
          break;
        case 18: /* COUNT_SSL_ERRORS */
          set_field_ulonglong(f, m_row->m_count_ssl_errors);
          break;
        case 19: /* COUNT_MAX_USER_CONNECTION_ERRORS */
          set_field_ulonglong(f, m_row->m_count_max_user_connection_errors);
          break;
        case 20: /* COUNT_MAX_USER_CONNECTION_PER_HOUR_ERRORS */
          set_field_ulonglong(
              f, m_row->m_count_max_user_connection_per_hour_errors);
          break;
        case 21: /* COUNT_DEFAULT_DATABASE_ERRORS */
          set_field_ulonglong(f, m_row->m_count_default_database_errors);
          break;
        case 22: /* COUNT_INIT_CONNECT_ERRORS */
          set_field_ulonglong(f, m_row->m_count_init_connect_errors);
          break;
        case 23: /* COUNT_LOCAL_ERRORS */
          set_field_ulonglong(f, m_row->m_count_local_errors);
          break;
        case 24: /* COUNT_UNKNOWN_ERRORS */
          set_field_ulonglong(f, m_row->m_count_unknown_errors);
          break;
        case 25: /* FIRST_SEEN */
          set_field_timestamp(f, m_row->m_first_seen);
          break;
        case 26: /* LAST_SEEN */
          set_field_timestamp(f, m_row->m_last_seen);
          break;
        case 27: /* FIRST_ERROR_SEEN */
          if (m_row->m_first_error_seen != 0) {
            set_field_timestamp(f, m_row->m_first_error_seen);
          } else {
            f->set_null();
          }
          break;
        case 28: /* LAST_ERROR_SEEN */
          if (m_row->m_last_error_seen != 0) {
            set_field_timestamp(f, m_row->m_last_error_seen);
          } else {
            f->set_null();
          }
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
