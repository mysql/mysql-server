/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_TLS_CHANNEL_H
#define TABLE_TLS_CHANNEL_H

/**
  @file storage/perfschema/table_tls_channel_status.h
  Table TLS_CHANNEL_STATUS (declarations).
*/

#include <string>
#include <vector>

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.TLS_CHANNEL_STATUS table .*/
struct row_tls_channel_status {
  /** Interface name. In UTF8MB4 */
  std::string m_interface;
  /** Property name. In UTF8MB4 */
  std::string m_property_name;
  /** Property value. In UTF8MB4 */
  std::string m_property_value;
};

/** Table PERFORMANCE_SCHEMA.TLS_CHANNEL_STATUS */
class table_tls_channel_status : public PFS_engine_table {
 public:
  using TLS_channel_status_container = std::vector<row_tls_channel_status>;
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position() override;
  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  ~table_tls_channel_status() override = default;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_tls_channel_status();

 private:
  void materialize();
  /* Safe copy of TLS properties */
  TLS_channel_status_container m_row_tls_channel_status;

  /** Table share lock */
  static THR_LOCK m_table_lock;
  /** Table definition */
  static Plugin_table m_table_def;

  /** Current row. */
  row_tls_channel_status *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */

#endif  // !TABLE_TLS_CHANNEL_H
