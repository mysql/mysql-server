/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_RPL_ASYNC_CONNECTION_FAILOVER_MANAGED_H
#define TABLE_RPL_ASYNC_CONNECTION_FAILOVER_MANAGED_H

/**
  @file storage/perfschema/table_rpl_async_connection_failover_managed.h
  Table replication_asynchronous_connection_failover_managed (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_io.h"
#include "mysql_com.h"
#include "sql-common/json_dom.h"
#include "sql/rpl_async_conn_failover_table_operations.h"
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

/**
  A row in the table. The fields with string values have an additional
  length field denoted by \<field_name\>_length.
*/
struct st_row_rpl_async_conn_failover_managed {
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  char managed_name[HOSTNAME_LENGTH];
  uint managed_name_length;
  char managed_type[HOSTNAME_LENGTH];
  uint managed_type_length;
  Json_wrapper configuration;
};

/**
  Table
  PERFORMANCE_SCHEMA.TABLE_RPL_ASYNC_CONNECTION_FAILOVER_MANAGED.
*/
class table_rpl_async_connection_failover_managed : public PFS_engine_table {
  /** Position of a cursor, for simple iterations. */
  typedef PFS_simple_index pos_t;

 private:
  /**
    Stores current row (i.e.index) values for the table into m_row struct
    members. This stored data is read later through read_row_values().

    @param[in] index  current row position.

    @return Operation status
      @retval 0     Success
      @retval != 0  Error (error code returned)
  */
  int make_row(uint index);

  /** Table share lock. */
  static THR_LOCK m_table_lock;

  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_rpl_async_conn_failover_managed m_row;

  /** Current position. */
  pos_t m_pos;

  /** Next position. */
  pos_t m_next_pos;

 protected:
  /**
    Read the current row values.

    @param[in] table            Table handle
    @param[in] buf              row buffer
    @param[in] fields           Table fields
    @param[in] read_all         true if all columns are read.

    @return Operation status
      @retval 0     Success
      @retval != 0  Error (error code returned)
  */
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_rpl_async_connection_failover_managed();

 public:
  ~table_rpl_async_connection_failover_managed() override;

  /** Table share. */
  static PFS_engine_table_share m_share;

  /**
    Open table function.

    @param[in] tbs  Table share object
  */
  static PFS_engine_table *create(PFS_engine_table_share *tbs);

  /**
    Get the current number of rows read.

    @return number of rows read.
  */
  static ha_rows get_row_count();

  /** Reset the cursor position to the beginning of the table. */
  void reset_position() override;

  /**
    Initialize table for random read or scan.

    @param[in] scan  if true: Initialize for random scans through rnd_next()
                     if false: Initialize for random reads through rnd_pos()

    @return Operation status
      @retval 0     Success
      @retval != 0  Error (error code returned)
  */
  int rnd_init(bool scan) override;

  /**
    Read next row via random scan.

    @return Operation status
      @retval 0     Success
      @retval != 0  Error (error code returned)
  */
  int rnd_next() override;

  /**
    Read row via random scan from position.

    @param[in]      pos  Position from position() call

    @return Operation status
      @retval 0     Success
      @retval != 0  Error (error code returned)
  */
  int rnd_pos(const void *pos) override;

 private:
  /* Stores the data being read i.e. source connection details. */
  std::vector<RPL_FAILOVER_MANAGED_JSON_TUPLE> m_source_managed_list{};

  /* Stores the current number of rows read. */
  static ha_rows num_rows;
};

/** @} */
#endif
