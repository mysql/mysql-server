/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_EES_BY_ACCOUNT_BY_ERROR_H
#define TABLE_EES_BY_ACCOUNT_BY_ERROR_H

/**
  @file storage/perfschema/table_ees_by_account_by_error.h
  Table EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_account.h"
#include "table_helper.h"
#include "pfs_error.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR.
*/
struct row_ees_by_account_by_error
{
  /** Columns USER, HOST. */
  PFS_account_row m_account;
  /** Columns ERROR_NUMBER, ERROR_NAME, COUNT_STAR. */
  PFS_error_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR.
  Index 1 on account (0 based)
  Index 2 on error class (1 based)
  Index 3 on error (0 based)
*/
struct pos_ees_by_account_by_error
: public PFS_triple_index
{
  pos_ees_by_account_by_error()
    : PFS_triple_index(0, 1, 0)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 1;
    m_index_3= 0;
  }

  inline void next_account(void)
  {
    m_index_1++;
    m_index_2= 1;
    m_index_3= 0;
  }

  inline bool has_more_error(void)
  { return (m_index_3 < max_server_errors); }

  inline void next_error(void)
  {
    m_index_3++;
  }

};

/** Table PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR. */
class table_ees_by_account_by_error : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_ees_by_account_by_error();

public:
  ~table_ees_by_account_by_error()
  {}

protected:
  void make_row(PFS_account *account, PFS_error_class *klass, int error_index);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_ees_by_account_by_error m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_ees_by_account_by_error m_pos;
  /** Next position. */
  pos_ees_by_account_by_error m_next_pos;
};

/** @} */
#endif
