/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef _NGS_COLUMN_INFO_BUILDER_H_
#define _NGS_COLUMN_INFO_BUILDER_H_

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"

namespace ngs {

class Column_info_builder {
 public:
  Column_info_builder() = default;

  Column_info_builder(const ::Mysqlx::Resultset::ColumnMetaData_FieldType type,
                      const char *col_name) {
    m_column_info.m_type = type;
    set_non_compact_data("", col_name, "", "", "", "");
  }

  void set_type(const ::Mysqlx::Resultset::ColumnMetaData_FieldType type) {
    m_column_info.m_type = type;
  }

  void set_collation(const uint64_t collation) {
    m_column_info.m_has_collation = true;
    m_column_info.m_collation = collation;
  }

  void set_decimals(const uint32_t decimals) {
    m_column_info.m_has_decimals = true;
    m_column_info.m_decimals = decimals;
  }

  void set_flags(const int32_t flags) {
    m_column_info.m_has_flags = true;
    m_column_info.m_flags = flags;
  }

  void set_length(const uint64_t length) {
    m_column_info.m_has_length = true;
    m_column_info.m_length = length;
  }

  void set_content_type(const uint32_t content_type) {
    m_column_info.m_content_type = content_type;
  }

  void set_non_compact_data(const char *catalog, const char *col_name,
                            const char *table_name, const char *db_name,
                            const char *org_col_name,
                            const char *org_table_name) {
    m_column_info.m_compact = false;

    m_column_info.m_catalog = catalog;
    m_column_info.m_col_name = col_name;
    m_column_info.m_table_name = table_name;
    m_column_info.m_db_name = db_name;
    m_column_info.m_org_col_name = org_col_name;
    m_column_info.m_org_table_name = org_table_name;
  }

  const Encode_column_info &get() const { return m_column_info; }

 private:
  Encode_column_info m_column_info;
};

}  // namespace ngs

#endif  //  _NGS_COLUMN_INFO_BUILDER_H_
