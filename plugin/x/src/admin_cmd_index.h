/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#include <limits>
#include <string>
#include <vector>

#include "plugin/x/src/admin_cmd_handler.h"
#include "plugin/x/src/interface/admin_command_arguments.h"

namespace xpl {

class Query_string_builder;

class Admin_command_index {
 public:
  using Command_arguments = iface::Admin_command_arguments;
  using Argument_appearance = Command_arguments::Appearance_type;

  explicit Admin_command_index(iface::Session *session) : m_session(session) {}
  ngs::Error_code create(Command_arguments *args);
  ngs::Error_code drop(Command_arguments *args);

  struct Index_field_info {
    std::string m_path;
    std::string m_type;
    bool m_is_required{false};
    uint64_t m_options{std::numeric_limits<uint64_t>::max()};
    uint64_t m_srid{std::numeric_limits<uint64_t>::max()};
  };

  class Index_field_interface {
   public:
    virtual ~Index_field_interface() = default;
    virtual ngs::Error_code add_column_if_necessary(
        iface::Sql_session *sql_session, const std::string &schema,
        const std::string &collection, Query_string_builder *qb) const = 0;
    virtual void add_field(Query_string_builder *qb) const = 0;
    virtual bool is_required() const = 0;
  };

 protected:
  enum class Index_type_id {
    k_index,
    k_spatial,
    k_fulltext,
    k_unsupported = 99
  };

  Index_type_id get_type_id(const std::string &type_name) const;
  std::string get_default_field_type(const Index_type_id id,
                                     const bool is_array) const;

  ngs::Error_code get_index_generated_column_names(
      const std::string &schema, const std::string &collection,
      const std::string &index_name,
      std::vector<std::string> *column_names) const;

  bool is_table_support_virtual_columns(const std::string &schema_name,
                                        const std::string &table_name,
                                        ngs::Error_code *error) const;

  const Index_field_interface *create_field(const bool is_virtual_allowed,
                                            const Index_type_id &index_type,
                                            Command_arguments *constraint,
                                            ngs::Error_code *error) const;

  iface::Session *m_session;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_INDEX_H_
