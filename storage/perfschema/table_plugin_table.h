/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_PLUGIN_TABLE_H
#define TABLE_PLUGIN_TABLE_H

/**
  @file storage/perfschema/table_plugin_table.h
  plugins/components tables (declarations).
*/

#include <mysql/components/services/pfs_plugin_table_service.h>

#include "storage/perfschema/pfs_engine_table.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

class PFS_plugin_table_index;

class table_plugin_table : public PFS_engine_table {
 public:
  /** Table share */
  PFS_engine_table_share *m_share;
  PFS_engine_table_proxy *m_st_table;
  PSI_table_handle *plugin_table_handle;

  static PFS_engine_table *create(PFS_engine_table_share *share);
  int delete_all_rows();
  /*
  PFS_engine_table* open(void);
  ha_rows get_row_count();
  */

  void reset_position(void) override;

  int rnd_init(bool scan) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

  int write_row(PSI_field *field, uint index, bool finished);

  explicit table_plugin_table(PFS_engine_table_share *share);

  void deinitialize_table_share();

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  int update_row_values(TABLE *table, const unsigned char *, unsigned char *,
                        Field **fields) override;

  int delete_row_values(TABLE *table, const unsigned char *buf,
                        Field **fields) override;

 public:
  ~table_plugin_table() override {
    delete m_index;
    m_st_table->close_table(this->plugin_table_handle);
  }

 private:
  /** Table share lock. */
  THR_LOCK *m_table_lock;

  /** True is the current row exists. */
  bool m_row_exists;

  /** Current position. */
  PSI_pos *m_pos;
  /** Next position. */
  PSI_pos *m_next_pos;

  PFS_plugin_table_index *m_opened_index;
};

class PFS_plugin_table_index : public PFS_engine_index_abstract {
 public:
  explicit PFS_plugin_table_index(PFS_engine_table_proxy *st_table)
      : m_st_table(st_table), m_idx(0), m_plugin_index(nullptr) {}

  ~PFS_plugin_table_index() override = default;

  int init(PSI_table_handle *table, uint idx, bool sorted);

  int index_next(PSI_table_handle *table);

  void read_key(const uchar *key, uint key_len,
                enum ha_rkey_function find_flag) override;

 private:
  PFS_engine_table_proxy *m_st_table;
  uint m_idx;
  PSI_index_handle *m_plugin_index;
};

/** @} */
#endif
