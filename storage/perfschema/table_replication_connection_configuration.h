/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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


#ifndef TABLE_REPLICATION_CONFIGURATION_H
#define TABLE_REPLICATION_CONFIGURATION_H 

/**
  @file storage/perfschema/table_replication_connection_configuration.h
  Table replication_connection_configuration (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "rpl_mi.h"
#include "mysql_com.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/
#define HOST_MAX_LEN  HOSTNAME_LENGTH * SYSTEM_CHARSET_MBMAXLEN
#define USER_MAX_LEN  USERNAME_CHAR_LENGTH * SYSTEM_CHARSET_MBMAXLEN

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
enum enum_rpl_yes_no {
  PS_RPL_YES= 1,
  PS_RPL_NO
};
#endif

enum enum_ssl_allowed {
    PS_SSL_ALLOWED_YES= 1,
    PS_SSL_ALLOWED_NO,
    PS_SSL_ALLOWED_IGNORED
};

struct st_row_connect_config {
  char Host[HOST_MAX_LEN];
  uint Host_length;  
  uint Port;
  char User[USER_MAX_LEN];
  uint User_length;
  char Network_Interface[60];
  uint Network_Interface_length;
  bool Auto_Position;
  enum_ssl_allowed SSL_Allowed;
  char SSL_CA_File[FN_REFLEN];
  uint SSL_CA_File_length; 
  char SSL_CA_Path[FN_REFLEN];
  uint SSL_CA_Path_length;
  char SSL_Certificate[FN_REFLEN];
  uint SSL_Certificate_length;
  char SSL_Cipher[FN_REFLEN];
  uint SSL_Cipher_length;
  char SSL_Key[FN_REFLEN];
  uint SSL_Key_length;
  enum_rpl_yes_no SSL_Verify_Server_Certificate;
  char SSL_Crl_File[FN_REFLEN];
  uint SSL_Crl_File_length;
  char SSL_Crl_Path[FN_REFLEN];
  uint SSL_Crl_Path_length;
  uint Connection_Retry_Interval;
  ulong Connection_Retry_Count;
};

/** Table PERFORMANCE_SCHEMA.TABLE_REPLICATION_CONNECTION_CONFIGURATION. */
class table_replication_connection_configuration: public PFS_engine_table
{
private:
  void fill_rows(Master_info *);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** Current row */
  st_row_connect_config m_row;
  /** True is the table is filled up */
  bool m_filled;
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
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

};

/** @} */
#endif
