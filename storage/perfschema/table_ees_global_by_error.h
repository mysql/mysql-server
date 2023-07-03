/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_EES_GLOBAL_BY_EVENT_NAME_H
#define TABLE_EES_GLOBAL_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_ees_global_by_error.h
  Table EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_error.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup Performance_schema_tables
  @{
*/

class PFS_index_ees_global_by_error : public PFS_engine_index {
 public:
  PFS_index_ees_global_by_error()
      : PFS_engine_index(&m_key), m_key("ERROR_NUMBER") {}

  ~PFS_index_ees_global_by_error() override = default;

  virtual bool match_error_index(uint error_index);

 private:
  PFS_key_error_number m_key;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR.
*/
struct row_ees_global_by_error {
  /** Columns ERROR_NUMBER, ERROR_NAME, COUNT_STAR. */
  PFS_error_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR.
  Index 1 on error (0 based)
*/
struct pos_ees_global_by_error : public PFS_simple_index {
  pos_ees_global_by_error() : PFS_simple_index(0) {}

  inline void reset(void) { m_index = 0; }

  inline void next(void) { m_index++; }

  inline bool has_more_error(void) {
    return (m_index < max_global_server_errors);
  }

  inline void next_error(void) { m_index++; }
};

/** Table PERFORMANCE_SCHEMA.EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR. */
class table_ees_global_by_error : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position(void) override;

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_ees_global_by_error();

 public:
  ~table_ees_global_by_error() override = default;

 protected:
  int make_row(uint error_index);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_ees_global_by_error m_row;
  /** Current position. */
  pos_ees_global_by_error m_pos;
  /** Next position. */
  pos_ees_global_by_error m_next_pos;

 protected:
  PFS_index_ees_global_by_error *m_opened_index;
};

/** @} */
#endif
