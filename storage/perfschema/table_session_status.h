/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_SESSION_STATUS_H
#define TABLE_SESSION_STATUS_H

/**
  @file storage/perfschema/table_session_status.h
  Table SESSION_STATUS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_variable.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct TABLE;
struct THR_LOCK;
/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.SESSION_STATUS.
*/
struct row_session_status {
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column VARIABLE_NAME. */
  PFS_variable_name_row m_variable_name;
  /** Column VARIABLE_VALUE. */
  PFS_variable_value_row m_variable_value;
};

class PFS_index_session_status : public PFS_engine_index {
 public:
  PFS_index_session_status()
      : PFS_engine_index(&m_key), m_key("VARIABLE_NAME") {}

  ~PFS_index_session_status() override = default;

  virtual bool match(const Status_variable *pfs);

 private:
  PFS_key_variable_name m_key;
};

/** Table PERFORMANCE_SCHEMA.SESSION_STATUS. */
class table_session_status : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
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
  table_session_status();

 public:
  ~table_session_status() override = default;

 protected:
  int make_row(const Status_variable *status_var);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current THD variables. */
  PFS_status_variable_cache m_status_cache;
  /** Current row. */
  row_session_status m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_session_status *m_opened_index;
};

/** @} */
#endif
