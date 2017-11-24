/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_INSTANCE_LOG_STATUS_H
#define TABLE_INSTANCE_LOG_STATUS_H

/**
  @file storage/perfschema/table_instance_log_status.h
  Table instance_log_status (declarations).
*/

#include "sql/json_dom.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

/*
  A row in the table. The fields with string values have an additional
  length field denoted by <field_name>_length.
*/
struct st_row_instance_log_status
{
  char server_uuid[UUID_LENGTH];
  Json_wrapper w_master;
  Json_wrapper w_channels;
  Json_wrapper w_storage_engines;

  st_row_instance_log_status()
  {
  }

  void
  cleanup()
  {
  }
};


/** Table PERFORMANCE_SCHEMA.INSTANCE_LOG_STATUS. */
class table_instance_log_status : public PFS_engine_table
{
private:
  int make_row();

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_instance_log_status m_row;
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

  table_instance_log_status();

public:
  ~table_instance_log_status();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();
  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
};

/** @} */
#endif
