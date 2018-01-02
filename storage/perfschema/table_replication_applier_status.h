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

#include <sys/types.h>

#include "mysql_com.h"
#include "sql/rpl_info.h" /*CHANNEL_NAME_LENGTH*/
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
/** enum values for Service_State field*/
enum enum_rpl_yes_no
{
  PS_RPL_YES = 1,
  PS_RPL_NO
};
#endif

/** A row in the table. */
struct st_row_applier_status
{
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  enum_rpl_yes_no service_state;
  uint remaining_delay;
  bool remaining_delay_is_set;
  ulong count_transactions_retries;
};

class PFS_index_rpl_applier_status : public PFS_engine_index
{
public:
  PFS_index_rpl_applier_status()
    : PFS_engine_index(&m_key), m_key("CHANNEL_NAME")
  {
  }

  ~PFS_index_rpl_applier_status()
  {
  }

  virtual bool match(Master_info *mi);

private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.replication_applier_status */
class table_replication_applier_status : public PFS_engine_table
{
private:
  int make_row(Master_info *mi);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_applier_status m_row;
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

  table_replication_applier_status();

public:
  ~table_replication_applier_status();

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
  PFS_index_rpl_applier_status *m_opened_index;
};

/** @} */
#endif
