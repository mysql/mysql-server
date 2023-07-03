/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_KEYRING_COMPONENT_STATUS_H
#define TABLE_KEYRING_COMPONENT_STATUS_H

/**
  @file storage/perfschema/table_keyring_component_status.h
  TABLE KEYRING_COMPONENT_STATUS
*/
#include <string>
#include <vector>

#include <sys/types.h>
#include "storage/perfschema/table_helper.h"

/**
  A row in PERFORMANCE_SCHEMA.KEYRING_COMPONENT_STATUS table.
*/
struct row_keyring_component_status {
  /** STATUS_KEY */
  std::string m_status_key;
  /** STATUS_VALUE */
  std::string m_status_value;
};

/**
  @addtogroup performance_schema_tables
  @{
*/

/** Table PERFORMANCE_SCHEMA.KEYRING_COMPONENT_STATUS. */
class table_keyring_component_status : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position(void) override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

 private:
  table_keyring_component_status();
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

 private:
  void materialize();
  /** Safe copy of the keyring status */
  std::vector<row_keyring_component_status> m_row_keyring_component_status;

  /** Table share lock */
  static THR_LOCK m_table_lock;
  /** Table definition */
  static Plugin_table m_table_def;

  /** Current row. */
  row_keyring_component_status *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */

#endif  // TABLE_KEYRING_COMPONENT_STATUS_H