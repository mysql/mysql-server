/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_REPLICATION_CONFIGURATION_H
#define TABLE_REPLICATION_CONFIGURATION_H

/**
  @file storage/perfschema/table_replication_connection_configuration.h
  Table replication_connection_configuration (declarations).
*/

#include <sys/types.h>

#include "compression.h"  // COMPRESSION_ALGORITHM_NAME_BUFFER_SIZE
#include "my_base.h"
#include "my_io.h"
#include "mysql_com.h"
#include "sql/rpl_info.h" /* CHANNEL_NAME_LENGTH*/
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Master_info;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
enum enum_rpl_yes_no { PS_RPL_YES = 1, PS_RPL_NO };
#endif

/** enum values for SSL_Allowed*/
enum enum_ssl_allowed {
  PS_SSL_ALLOWED_YES = 1,
  PS_SSL_ALLOWED_NO,
  PS_SSL_ALLOWED_IGNORED
};

/**
  A row in the table. The fields with string values have an additional
  length field denoted by \<field_name\>_length.
*/
struct st_row_connect_config {
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length{0};
  char host[HOSTNAME_LENGTH];
  uint host_length{0};
  uint port{0};
  char user[USERNAME_LENGTH];
  uint user_length{0};
  char network_interface[HOSTNAME_LENGTH];
  uint network_interface_length{0};
  enum_rpl_yes_no auto_position;
  enum_ssl_allowed ssl_allowed;
  char ssl_ca_file[FN_REFLEN];
  uint ssl_ca_file_length{0};
  char ssl_ca_path[FN_REFLEN];
  uint ssl_ca_path_length{0};
  char ssl_certificate[FN_REFLEN];
  uint ssl_certificate_length{0};
  char ssl_cipher[FN_REFLEN];
  uint ssl_cipher_length{0};
  char ssl_key[FN_REFLEN];
  uint ssl_key_length{0};
  enum_rpl_yes_no ssl_verify_server_certificate;
  char ssl_crl_file[FN_REFLEN];
  uint ssl_crl_file_length{0};
  char ssl_crl_path[FN_REFLEN];
  uint ssl_crl_path_length{0};
  uint connection_retry_interval{0};
  ulong connection_retry_count{0};
  double heartbeat_interval{0.0};
  char tls_version[FN_REFLEN];
  uint tls_version_length{0};
  char public_key_path[FN_REFLEN];
  uint public_key_path_length{0};
  enum_rpl_yes_no get_public_key;
  char network_namespace[NAME_LEN];
  uint network_namespace_length{0};
  char compression_algorithm[COMPRESSION_ALGORITHM_NAME_BUFFER_SIZE];
  uint compression_algorithm_length{0};
  uint zstd_compression_level{0};
  /*
    tls_ciphersuites = NULL means that TLS 1.3 default ciphersuites
    are enabled. To allow a value that can either be NULL or a string,
    it is represented by the pair:
      first:  true if tls_ciphersuites is set to NULL
      second: the string value when first is false
  */
  std::pair<bool, std::string> tls_ciphersuites = {true, ""};
  enum_rpl_yes_no source_connection_auto_failover{PS_RPL_NO};
  /*PS_RPL_NO if gtid_only is disabled, PS_RPL_YES if enabled */
  enum_rpl_yes_no gtid_only{PS_RPL_NO};
};

class PFS_index_rpl_connection_config : public PFS_engine_index {
 public:
  PFS_index_rpl_connection_config()
      : PFS_engine_index(&m_key), m_key("CHANNEL_NAME") {}

  ~PFS_index_rpl_connection_config() override = default;

  virtual bool match(Master_info *mi);

 private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.TABLE_REPLICATION_CONNECTION_CONFIGURATION. */
class table_replication_connection_configuration : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 private:
  int make_row(Master_info *);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_connect_config m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

 protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */

  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_replication_connection_configuration();

 public:
  ~table_replication_connection_configuration() override;

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();
  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 private:
  PFS_index_rpl_connection_config *m_opened_index;
};

/** @} */
#endif
