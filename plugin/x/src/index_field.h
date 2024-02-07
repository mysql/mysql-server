/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_ADMIN_CMD_INDEX_FIELD_H_
#define PLUGIN_X_SRC_ADMIN_CMD_INDEX_FIELD_H_

#include <string>

#include "plugin/x/src/admin_cmd_index.h"

namespace xpl {

class Query_string_builder;

class Index_field : public Admin_command_index::Index_field_interface {
 public:
  static const Index_field *create(
      const bool is_virtual_allowed,
      const Admin_command_index::Index_field_info &info,
      ngs::Error_code *error);

  ngs::Error_code add_column_if_necessary(
      iface::Sql_session *sql_session, const std::string &schema,
      const std::string &collection, Query_string_builder *qb) const override;
  void add_field(Query_string_builder *qb) const override;
  bool is_required() const override { return m_is_required; }

  bool is_column_exists(iface::Sql_session *sql_session,
                        const std::string &schema,
                        const std::string &collection,
                        ngs::Error_code *error) const;
  void add_column(Query_string_builder *qb) const;

 protected:
  enum class Type_id {
    k_tinyint,
    k_smallint,
    k_mediumint,
    k_int,
    k_integer,
    k_bigint,
    k_real,
    k_float,
    k_double,
    k_decimal,
    k_numeric,
    k_date,
    k_time,
    k_timestamp,
    k_datetime,
    k_year,
    k_bit,
    k_blob,
    k_text,
    k_geojson,
    k_fulltext,
    k_char,
    k_unsupported = 99
  };

  Index_field(const std::string &path, const bool is_required,
              const std::string &name, const bool is_virtual_allowed)
      : m_path(path),
        m_is_required(is_required),
        m_name(name),
        m_is_virtual_allowed(is_virtual_allowed) {}

  virtual void add_type(Query_string_builder *qb) const = 0;
  virtual void add_path(Query_string_builder *qb) const = 0;
  virtual void add_length(Query_string_builder * /*qb*/) const {}
  virtual void add_options(Query_string_builder *qb) const;

  static Type_id get_type_id(const std::string &type_name);

  const std::string m_path;
  const bool m_is_required{false};
  const std::string m_name;
  const bool m_is_virtual_allowed{false};
};
}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_INDEX_FIELD_H_
