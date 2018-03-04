/*
   Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_REPLICATION_CONFIGURATION_H
#define TABLE_REPLICATION_CONFIGURATION_H

/**
  @file storage/perfschema/table_replication_connection_configuration.h
  Table replication_connection_configuration (declarations).
*/

#include <sys/types.h>

#include "my_io.h"
#include "mysql_com.h"
#include "sql/rpl_info.h" /* CHANNEL_NAME_LENGTH*/
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Master_info;

/**
  @addtogroup performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
enum enum_rpl_yes_no
{
  PS_RPL_YES = 1,
  PS_RPL_NO
};
#endif

/** enum values for SSL_Allowed*/
enum enum_ssl_allowed
{
  PS_SSL_ALLOWED_YES = 1,
  PS_SSL_ALLOWED_NO,
  PS_SSL_ALLOWED_IGNORED
};

/**
  A row in the table. The fields with string values have an additional
  length field denoted by \<field_name\>_length.
*/
struct st_row_connect_config
{
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  char host[HOSTNAME_LENGTH];
  uint host_length;
  uint port;
  char user[USERNAME_LENGTH];
  uint user_length;
  char network_interface[HOSTNAME_LENGTH];
  uint network_interface_length;
  enum_rpl_yes_no auto_position;
  enum_ssl_allowed ssl_allowed;
  char ssl_ca_file[FN_REFLEN];
  uint ssl_ca_file_length;
  char ssl_ca_path[FN_REFLEN];
  uint ssl_ca_path_length;
  char ssl_certificate[FN_REFLEN];
  uint ssl_certificate_length;
  char ssl_cipher[FN_REFLEN];
  uint ssl_cipher_length;
  char ssl_key[FN_REFLEN];
  uint ssl_key_length;
  enum_rpl_yes_no ssl_verify_server_certificate;
  char ssl_crl_file[FN_REFLEN];
  uint ssl_crl_file_length;
  char ssl_crl_path[FN_REFLEN];
  uint ssl_crl_path_length;
  uint connection_retry_interval;
  ulong connection_retry_count;
  double heartbeat_interval;
  char tls_version[FN_REFLEN];
  uint tls_version_length;
  char public_key_path[FN_REFLEN];
  uint public_key_path_length;
  enum_rpl_yes_no get_public_key;
};

class PFS_index_rpl_connection_config : public PFS_engine_index
{
public:
  PFS_index_rpl_connection_config()
    : PFS_engine_index(&m_key), m_key("CHANNEL_NAME")
  {
  }

  ~PFS_index_rpl_connection_config()
  {
  }

  virtual bool match(Master_info *mi);

private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.TABLE_REPLICATION_CONNECTION_CONFIGURATION. */
class table_replication_connection_configuration : public PFS_engine_table
{
private:
  int make_row(Master_info *);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_connect_config m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_replication_connection_configuration();

public:
  ~table_replication_connection_configuration();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();
  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

private:
  PFS_index_rpl_connection_config *m_opened_index;
};

/** @} */
#endif
