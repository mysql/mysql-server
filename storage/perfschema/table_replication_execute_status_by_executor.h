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


#ifndef TABLE_REPLICATION_EXECUTE_STATUS_BY_EXECUTOR_H
#define TABLE_REPLICATION_EXECUTE_STATUS_BY_EXECUTOR_H

/**
  @file storage/perfschema/table_replication_execute_status_by_executor.h
  Table replication_execute_status_by_executor (declarations).
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

#ifndef ST_RPL_STATUS_FIELD
#define ST_RPL_STATUS_FIELD
typedef struct st_rpl_status_field {
  union {
    LEX_STRING  s;
    ulonglong   n;
  } u;
  bool is_null;
} ST_STATUS_FIELD_DATA;

typedef struct st_rpl_status_field_info
{
  /** 
      the column name
  */
  const char* name;
  /**
     size in bytes of a buffer capable to keep internal representation
     of an attribute
  */
  uint max_size;
  /**
     mysql data type
  */
  enum enum_field_types type;
  bool can_be_null;
} ST_STATUS_FIELD_INFO;
#endif

enum enum_rpl_execute_field_names {
  RPL_EXECUTE_THREAD_ID= 0,
  RPL_EXECUTE_SERVICE_STATE,
  RPL_LAST_EXECUTED_TRANSACTION,
  RPL_EXECUTE_LAST_ERROR_NUMBER,
  RPL_EXECUTE_LAST_ERROR_MESSAGE,
  RPL_EXECUTE_LAST_ERROR_TIMESTAMP, 
  _RPL_EXECUTE_LAST_FIELD_= RPL_EXECUTE_LAST_ERROR_TIMESTAMP
};

/** Table PERFORMANCE_SCHEMA.replication_execute_status_per_executor */
class table_replication_execute_status_by_executor: public PFS_engine_table
{
private:
  void fill_rows(Master_info *);
  void drop_null(enum enum_rpl_execute_field_names f_name);
  void set_null(enum enum_rpl_execute_field_names f_name);
  void str_store(enum enum_rpl_execute_field_names f_name, const char * val);
  void int_store(enum enum_rpl_execute_field_names f_name, longlong val);
  void enum_store(enum enum_rpl_execute_field_names f_name, longlong val)
  {
    int_store(f_name, val);
  }

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** Current only row is represented by array of fields */
  ST_STATUS_FIELD_DATA m_fields[_RPL_EXECUTE_LAST_FIELD_ + 1];
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

  table_replication_execute_status_by_executor();

public:
  ~table_replication_execute_status_by_executor();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

};

/** @} */
#endif
