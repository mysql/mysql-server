/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INDEX_ARRAY_FIELD_H_
#define PLUGIN_X_SRC_INDEX_ARRAY_FIELD_H_

#include "plugin/x/src/admin_cmd_index.h"

namespace xpl {

class Index_array_field : public Admin_command_index::Index_field_interface {
 public:
  static const Index_array_field *create(
      const Admin_command_index::Index_field_info &info,
      ngs::Error_code *error);

  ngs::Error_code add_column_if_necessary(
      iface::Sql_session *sql_session, const std::string &schema,
      const std::string &collection, Query_string_builder *qb) const override;
  void add_field(Query_string_builder *qb) const override;
  bool is_required() const override { return false; }

 protected:
  Index_array_field(const std::string &path, const std::string &type)
      : m_path(path), m_type(type) {}

  const std::string m_path;
  const std::string m_type;
};

}  // namespace xpl

#endif  // PLUGIN _X_SRC_INDEX_ARRAY_FIELD_H_
