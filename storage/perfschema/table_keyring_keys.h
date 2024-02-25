/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

#ifndef TABLE_KEYRING_KEYS_H
#define TABLE_KEYRING_KEYS_H

/**
  @file storage/perfschema/table_keyring_keys.h
  TABLE KEYRING_KEYS.
*/

#include <sys/types.h>
#include "storage/perfschema/table_helper.h"

/**
  A row of PERFORMANCE_SCHEMA.KEYRING_KEYS table.
*/
struct row_keyring_keys {
  /** Column KEY_ID. In UTF8MB4 */
  std::string m_key_id;
  /** Column KEY_OWNER. In UTF8MB4 */
  std::string m_key_owner;
  /** Column BACK_END_KEY_ID. In UTF8MB4 */
  std::string m_backend_key_id;
};

/**
  @addtogroup performance_schema_tables
  @{
*/

/** Table PERFORMANCE_SCHEMA.KEYRING_KEYS. */
class table_keyring_keys : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share s_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position() override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

 private:
  table_keyring_keys();
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  bool copy_keys_from_keyring();

 private:
  /** Safe copy of the keyring keys. */
  std::vector<row_keyring_keys> m_copy_keyring_keys;

  /** Current row. */
  row_keyring_keys *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  /** Table share lock. */
  static THR_LOCK s_table_lock;
  /** Table definition. */
  static Plugin_table s_table_def;
};

/** @} */

#endif
