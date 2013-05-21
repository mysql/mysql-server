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


#ifndef TABLE_REPLICATION_EXECUTE_STATUS_H
#define TABLE_REPLICATION_EXECUTE_STATUS_H

/**
  @file storage/perfschema/table_replication_execute_status.h
  Table replication_execute_status (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "rpl_mi.h"
#include "mysql_com.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
enum enum_rpl_yes_no {
  PS_RPL_YES= 1,
  PS_RPL_NO
};
#endif

struct st_row_execute_status {
  enum_rpl_yes_no Service_State;
  char Remaining_Delay[11];
  uint Remaining_Delay_length;
};

/** Table PERFORMANCE_SCHEMA.replication_execute_status */
class table_replication_execute_status: public PFS_engine_table
{
private:
  void fill_rows(Master_info *);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** Current row */
  st_row_execute_status m_row;
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

  table_replication_execute_status();

public:
  ~table_replication_execute_status();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

};

/** @} */
#endif
