/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_ADMIN_CMD_INDEX_H_
#define PLUGIN_X_SRC_ADMIN_CMD_INDEX_H_

#include <list>
#include <string>
#include <vector>

#include "plugin/x/src/admin_cmd_handler.h"

namespace xpl {

class Query_string_builder;

class Admin_command_index {
 public:
  using Command_arguments = Admin_command_handler::Command_arguments;

  explicit Admin_command_index(ngs::Session_interface *session)
      : m_session(session) {}
  ngs::Error_code create(const std::string &name_space,
                         Command_arguments *args);
  ngs::Error_code drop(const std::string &name_space, Command_arguments *args);

  class Index_field {
   public:
    virtual ~Index_field() = default;

    static const Index_field *create(const std::string &name_space,
                                     const bool is_virtual_allowed,
                                     const std::string &default_type,
                                     Command_arguments *constraint,
                                     ngs::Error_code *error);

    bool is_column_exists(ngs::Sql_session_interface *sql_session,
                          const std::string &schema,
                          const std::string &collection,
                          ngs::Error_code *error) const;
    void add_column(Query_string_builder *qb) const;
    void add_field(Query_string_builder *qb) const;
    bool is_required() const { return m_is_required; }

   protected:
    enum class Field_type_id {
      TINYINT,
      SMALLINT,
      MEDIUMINT,
      INT,
      INTEGER,
      BIGINT,
      REAL,
      FLOAT,
      DOUBLE,
      DECIMAL,
      NUMERIC,
      DATE,
      TIME,
      TIMESTAMP,
      DATETIME,
      YEAR,
      BIT,
      BLOB,
      TEXT,
      GEOJSON,
      FULLTEXT,
      UNSUPPORTED = 99
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

    static Field_type_id get_type_id(const std::string &type_name);

    const std::string m_path;
    const bool m_is_required{false};
    const std::string m_name;
    const bool m_is_virtual_allowed{false};
  };

 protected:
  enum class Index_type_id { INDEX, SPATIAL, FULLTEXT, UNSUPPORTED = 99 };

  Index_type_id get_type_id(const std::string &type_name) const;
  std::string get_default_field_type(const Index_type_id id) const;

  ngs::Error_code get_index_generated_column_names(
      const std::string &schema, const std::string &collection,
      const std::string &index_name,
      std::vector<std::string> *column_names) const;

  bool is_table_support_virtual_columns(const std::string &schema_name,
                                        const std::string &table_name,
                                        ngs::Error_code *error) const;

  ngs::Session_interface *m_session;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_INDEX_H_
