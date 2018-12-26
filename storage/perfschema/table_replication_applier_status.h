/*
   Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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


#ifndef TABLE_REPLICATION_APPLIER_STATUS_H
#define TABLE_REPLICATION_APPLIER_STATUS_H

/**
  @file storage/perfschema/table_replication_applier_status.h
  Table replication_applier_status (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "rpl_mi.h"
#include "mysql_com.h"
#include "rpl_msr.h"
#include "rpl_info.h" /*CHANNEL_NAME_LENGTH*/

class Master_info;

/**
  @addtogroup Performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
/** enum values for Service_State field*/
enum enum_rpl_yes_no {
  PS_RPL_YES= 1,
  PS_RPL_NO
};
#endif

/** A row in the table. */
struct st_row_applier_status {
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  enum_rpl_yes_no service_state;
  uint remaining_delay;
  bool remaining_delay_is_set;
  ulong count_transactions_retries;
};

/** Table PERFORMANCE_SCHEMA.replication_applier_status */
class table_replication_applier_status: public PFS_engine_table
{
  typedef PFS_simple_index pos_t;

private:
  void make_row(Master_info *mi);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** Current row */
  st_row_applier_status m_row;
  /** True is the current row exists. */
  bool m_row_exists;
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

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_replication_applier_status();

public:
  ~table_replication_applier_status();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static ha_rows get_row_count();
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

};

/** @} */
#endif
