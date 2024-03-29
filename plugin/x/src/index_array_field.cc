/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/index_array_field.h"

#include <limits>
#include <string>

#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_regex.h"

namespace xpl {

ngs::Error_code Index_array_field::add_column_if_necessary(
    iface::Sql_session *, const std::string &, const std::string &,
    Query_string_builder *) const {
  return ngs::Success();
}

void Index_array_field::add_field(Query_string_builder *qb) const {
  qb->put("(CAST(JSON_EXTRACT(`doc`,")
      .quote_string(m_path)
      .put(") AS ")
      .put(m_type)
      .put(" ARRAY))");
}

namespace {
inline bool is_valid(const uint64_t arg) {
  return arg != std::numeric_limits<uint64_t>::max();
}

}  // namespace

const Index_array_field *Index_array_field::create(
    const Admin_command_index::Index_field_info &info, ngs::Error_code *error) {
  if (info.m_path.empty()) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Argument value for document member is invalid");
    return nullptr;
  }

  static const Regex re(
      "^("
      "BINARY(\\([[:digit:]]+\\))?|"
      "DATE|DATETIME|TIME|"
      "CHAR(\\([[:digit:]]+\\))?|"
      //"(?: (?:CHARACTER SET|CHARSET) \\w+)?(?: COLLATE \\w+)?|"
      "DECIMAL(\\([[:digit:]]+(,[[:digit:]]+)?\\))?|"
      "SIGNED( INTEGER)?|UNSIGNED( INTEGER)?"
      "){1}$");

  if (!re.match(info.m_type.c_str())) {
    *error = ngs::Error(
        ER_X_CMD_ARGUMENT_VALUE,
        "Invalid or unsupported type specification for array index '%s'",
        info.m_type.c_str());
    return nullptr;
  }

  if (info.m_is_required || is_valid(info.m_options) || is_valid(info.m_srid)) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Unsupported argument specification for '%s'",
                        info.m_path.c_str());
    return nullptr;
  }

  return new Index_array_field(info.m_path, info.m_type);
}

}  // namespace xpl
