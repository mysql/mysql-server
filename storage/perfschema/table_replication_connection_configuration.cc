/*
  Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @file storage/perfschema/table_replication_connection_configuration.cc
  Table replication_connection_configuration (implementation).
*/

#include "storage/perfschema/table_replication_connection_configuration.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/rpl_info.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h" /* Multisource replciation */
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_replication_connection_configuration::m_table_lock;

Plugin_table table_replication_connection_configuration::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "replication_connection_configuration",
    /* Definition */
    "  CHANNEL_NAME CHAR(64) not null,\n"
    "  HOST CHAR(255) CHARACTER SET ASCII not null,\n"
    "  PORT INTEGER not null,\n"
    "  USER CHAR(32) collate utf8mb4_bin not null,\n"
    "  NETWORK_INTERFACE CHAR(60) collate utf8mb4_bin not null,\n"
    "  AUTO_POSITION ENUM('1','0') not null,\n"
    "  SSL_ALLOWED ENUM('YES','NO','IGNORED') not null,\n"
    "  SSL_CA_FILE VARCHAR(512) not null,\n"
    "  SSL_CA_PATH VARCHAR(512) not null,\n"
    "  SSL_CERTIFICATE VARCHAR(512) not null,\n"
    "  SSL_CIPHER VARCHAR(512) not null,\n"
    "  SSL_KEY VARCHAR(512) not null,\n"
    "  SSL_VERIFY_SERVER_CERTIFICATE ENUM('YES','NO') not null,\n"
    "  SSL_CRL_FILE VARCHAR(255) not null,\n"
    "  SSL_CRL_PATH VARCHAR(255) not null,\n"
    "  CONNECTION_RETRY_INTERVAL INTEGER not null,\n"
    "  CONNECTION_RETRY_COUNT BIGINT unsigned not null,\n"
    "  HEARTBEAT_INTERVAL DOUBLE(10,3) not null\n"
    "  COMMENT 'Number of seconds after which a heartbeat will be sent .',\n"
    "  TLS_VERSION VARCHAR(255) not null,\n"
    "  PUBLIC_KEY_PATH VARCHAR(512) not null,\n"
    "  GET_PUBLIC_KEY ENUM('YES', 'NO') not null,\n"
    "  NETWORK_NAMESPACE VARCHAR(64) not null,\n"
    "  COMPRESSION_ALGORITHM CHAR(64) collate utf8mb4_bin not null\n"
    "  COMMENT 'Compression algorithm used for data transfer between master "
    "and slave.',\n"
    "  ZSTD_COMPRESSION_LEVEL INTEGER not null\n"
    "  COMMENT 'Compression level associated with zstd compression "
    "algorithm.',\n"
    "  TLS_CIPHERSUITES TEXT CHARACTER SET utf8mb3 COLLATE utf8mb3_bin NULL,\n"
    "  SOURCE_CONNECTION_AUTO_FAILOVER ENUM('1','0') not null,\n"
    "  GTID_ONLY ENUM('1','0') not null\n"
    "  COMMENT 'Indicates if this channel only uses GTIDs and does not persist "
    "positions.',\n"
    "  PRIMARY KEY (channel_name) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_replication_connection_configuration::m_share = {
    &pfs_readonly_acl,
    table_replication_connection_configuration::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_replication_connection_configuration::get_row_count, /* records */
    sizeof(pos_t),                                             /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_rpl_connection_config::match(Master_info *mi) {
  if (m_fields >= 1) {
    st_row_connect_config row;

    /* Mutex locks not necessary for channel name. */
    row.channel_name_length =
        mi->get_channel() ? (uint)strlen(mi->get_channel()) : 0;
    memcpy(row.channel_name, mi->get_channel(), row.channel_name_length);

    if (!m_key.match_not_null(row.channel_name, row.channel_name_length)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_replication_connection_configuration::create(
    PFS_engine_table_share *) {
  return new table_replication_connection_configuration();
}

table_replication_connection_configuration::
    table_replication_connection_configuration()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_replication_connection_configuration::
    ~table_replication_connection_configuration() = default;

void table_replication_connection_configuration::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_replication_connection_configuration::get_row_count() {
  /*
     We actually give the MAX_CHANNELS rather than the current
     number of channels
  */
  return channel_map.get_max_channels();
}

int table_replication_connection_configuration::rnd_next(void) {
  Master_info *mi;
  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels(); m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);
    if (mi && mi->host[0]) {
      make_row(mi);
      m_next_pos.set_after(&m_pos);
      channel_map.unlock();
      return 0;
    }
  }

  channel_map.unlock();
  return HA_ERR_END_OF_FILE;
}

int table_replication_connection_configuration::rnd_pos(const void *pos) {
  int res = HA_ERR_RECORD_DELETED;

  Master_info *mi;

  channel_map.rdlock();

  set_position(pos);

  if ((mi = channel_map.get_mi_at_pos(m_pos.m_index))) {
    res = make_row(mi);
  }

  channel_map.unlock();

  return res;
}

int table_replication_connection_configuration::index_init(uint idx
                                                           [[maybe_unused]],
                                                           bool) {
  PFS_index_rpl_connection_config *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_rpl_connection_config);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_replication_connection_configuration::index_next(void) {
  int res = HA_ERR_END_OF_FILE;

  Master_info *mi;

  channel_map.rdlock();

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < channel_map.get_max_channels() && res != 0;
       m_pos.next()) {
    mi = channel_map.get_mi_at_pos(m_pos.m_index);

    if (mi && mi->host[0]) {
      if (m_opened_index->match(mi)) {
        res = make_row(mi);
        m_next_pos.set_after(&m_pos);
      }
    }
  }

  channel_map.unlock();

  return res;
}

int table_replication_connection_configuration::make_row(Master_info *mi) {
  const char *temp_store;

  assert(mi != nullptr);

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  m_row.channel_name_length = strlen(mi->get_channel());
  memcpy(m_row.channel_name, (char *)mi->get_channel(),
         m_row.channel_name_length);

  m_row.host_length = strlen(mi->host);
  memcpy(m_row.host, mi->host, m_row.host_length);

  m_row.port = (unsigned int)mi->port;

  /* can't the user be NULL? */
  temp_store = mi->get_user();
  m_row.user_length = strlen(temp_store);
  memcpy(m_row.user, temp_store, m_row.user_length);

  temp_store = (char *)mi->bind_addr;
  m_row.network_interface_length = strlen(temp_store);
  memcpy(m_row.network_interface, temp_store, m_row.network_interface_length);

  if (mi->is_auto_position()) {
    m_row.auto_position = PS_RPL_YES;
  } else {
    m_row.auto_position = PS_RPL_NO;
  }

  m_row.ssl_allowed = mi->ssl ? PS_SSL_ALLOWED_YES : PS_SSL_ALLOWED_NO;

  temp_store = mi->ssl_ca;
  m_row.ssl_ca_file_length = strlen(temp_store);
  memcpy(m_row.ssl_ca_file, temp_store, m_row.ssl_ca_file_length);

  temp_store = mi->ssl_capath;
  m_row.ssl_ca_path_length = strlen(temp_store);
  memcpy(m_row.ssl_ca_path, temp_store, m_row.ssl_ca_path_length);

  temp_store = mi->ssl_cert;
  m_row.ssl_certificate_length = strlen(temp_store);
  memcpy(m_row.ssl_certificate, temp_store, m_row.ssl_certificate_length);

  temp_store = mi->ssl_cipher;
  m_row.ssl_cipher_length = strlen(temp_store);
  memcpy(m_row.ssl_cipher, temp_store, m_row.ssl_cipher_length);

  temp_store = mi->ssl_key;
  m_row.ssl_key_length = strlen(temp_store);
  memcpy(m_row.ssl_key, temp_store, m_row.ssl_key_length);

  if (mi->ssl_verify_server_cert) {
    m_row.ssl_verify_server_certificate = PS_RPL_YES;
  } else {
    m_row.ssl_verify_server_certificate = PS_RPL_NO;
  }

  temp_store = mi->ssl_crl;
  m_row.ssl_crl_file_length = strlen(temp_store);
  memcpy(m_row.ssl_crl_file, temp_store, m_row.ssl_crl_file_length);

  temp_store = mi->ssl_crlpath;
  m_row.ssl_crl_path_length = strlen(temp_store);
  memcpy(m_row.ssl_crl_path, temp_store, m_row.ssl_crl_path_length);

  m_row.connection_retry_interval = (unsigned int)mi->connect_retry;

  m_row.connection_retry_count = (ulong)mi->retry_count;

  m_row.heartbeat_interval = (double)mi->heartbeat_period;

  temp_store = mi->tls_version;
  m_row.tls_version_length = strlen(temp_store);
  memcpy(m_row.tls_version, temp_store, m_row.tls_version_length);

  temp_store = mi->public_key_path;
  m_row.public_key_path_length = strlen(temp_store);
  memcpy(m_row.public_key_path, temp_store, m_row.public_key_path_length);

  m_row.get_public_key = mi->get_public_key ? PS_RPL_YES : PS_RPL_NO;

  temp_store = mi->network_namespace_str();
  m_row.network_namespace_length = strlen(temp_store);
  memcpy(m_row.network_namespace, temp_store, m_row.network_namespace_length);

  temp_store = mi->compression_algorithm;
  m_row.compression_algorithm_length = strlen(temp_store);
  memcpy(m_row.compression_algorithm, temp_store,
         m_row.compression_algorithm_length);

  m_row.zstd_compression_level = mi->zstd_compression_level;

  m_row.tls_ciphersuites = mi->tls_ciphersuites;

  if (mi->is_source_connection_auto_failover()) {
    m_row.source_connection_auto_failover = PS_RPL_YES;
  } else {
    m_row.source_connection_auto_failover = PS_RPL_NO;
  }

  m_row.gtid_only = mi->is_gtid_only_mode() ? PS_RPL_YES : PS_RPL_NO;

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  return 0;
}

int table_replication_connection_configuration::read_row_values(
    TABLE *table, unsigned char *buf, Field **fields, bool read_all) {
  DBUG_TRACE;
  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (Field *f = nullptr; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /** channel_name */
          set_field_char_utf8mb4(f, m_row.channel_name,
                                 m_row.channel_name_length);
          break;
        case 1: /** host */
          set_field_char_utf8mb4(f, m_row.host, m_row.host_length);
          break;
        case 2: /** port */
          set_field_ulong(f, m_row.port);
          break;
        case 3: /** user */
          set_field_char_utf8mb4(f, m_row.user, m_row.user_length);
          break;
        case 4: /** network_interface */
          set_field_char_utf8mb4(f, m_row.network_interface,
                                 m_row.network_interface_length);
          break;
        case 5: /** auto_position */
          set_field_enum(f, m_row.auto_position);
          break;
        case 6: /** ssl_allowed */
          set_field_enum(f, m_row.ssl_allowed);
          break;
        case 7: /**ssl_ca_file */
          set_field_varchar_utf8mb4(f, m_row.ssl_ca_file,
                                    m_row.ssl_ca_file_length);
          break;
        case 8: /** ssl_ca_path */
          set_field_varchar_utf8mb4(f, m_row.ssl_ca_path,
                                    m_row.ssl_ca_path_length);
          break;
        case 9: /** ssl_certificate */
          set_field_varchar_utf8mb4(f, m_row.ssl_certificate,
                                    m_row.ssl_certificate_length);
          break;
        case 10: /** ssl_cipher */
          set_field_varchar_utf8mb4(f, m_row.ssl_cipher,
                                    m_row.ssl_cipher_length);
          break;
        case 11: /** ssl_key */
          set_field_varchar_utf8mb4(f, m_row.ssl_key, m_row.ssl_key_length);
          break;
        case 12: /** ssl_verify_server_certificate */
          set_field_enum(f, m_row.ssl_verify_server_certificate);
          break;
        case 13: /** ssl_crl_file */
          set_field_varchar_utf8mb4(f, m_row.ssl_crl_file,
                                    m_row.ssl_crl_file_length);
          break;
        case 14: /** ssl_crl_path */
          set_field_varchar_utf8mb4(f, m_row.ssl_crl_path,
                                    m_row.ssl_crl_path_length);
          break;
        case 15: /** connection_retry_interval */
          set_field_ulong(f, m_row.connection_retry_interval);
          break;
        case 16: /** connect_retry_count */
          set_field_ulonglong(f, m_row.connection_retry_count);
          break;
        case 17: /** number of seconds after which heartbeat will be sent */
          set_field_double(f, m_row.heartbeat_interval);
          break;
        case 18: /** tls_version */
          set_field_varchar_utf8mb4(f, m_row.tls_version,
                                    m_row.tls_version_length);
          break;
        case 19: /** master_public_key_path */
          set_field_varchar_utf8mb4(f, m_row.public_key_path,
                                    m_row.public_key_path_length);
          break;
        case 20: /** get_master_public_key */
          set_field_enum(f, m_row.get_public_key);
          break;
        case 21: /** network_namespace */
          set_field_varchar_utf8mb4(f, m_row.network_namespace,
                                    m_row.network_namespace_length);
          break;
        case 22: /** compression_algorithm */
          set_field_char_utf8mb4(f, m_row.compression_algorithm,
                                 m_row.compression_algorithm_length);
          break;
        case 23: /** zstd_compression_level */
          set_field_ulong(f, m_row.zstd_compression_level);
          break;
        case 24: /** tls_ciphersuites */
          if (m_row.tls_ciphersuites.first)
            f->set_null();
          else
            set_field_text(f, m_row.tls_ciphersuites.second.data(),
                           m_row.tls_ciphersuites.second.length(),
                           &my_charset_utf8mb4_bin);
          break;
        case 25: /** source_connection_auto_failover */
          set_field_enum(f, m_row.source_connection_auto_failover);
          break;
        case 26: /** gtid_only */
          set_field_enum(f, m_row.gtid_only);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
