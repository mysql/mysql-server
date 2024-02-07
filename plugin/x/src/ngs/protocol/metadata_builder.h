/*
 * Copyright (c) 2016, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_METADATA_BUILDER_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_METADATA_BUILDER_H_

#include <cstdint>
#include <vector>

#include "mysql/strings/m_ctype.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/ngs/protocol/column_info_builder.h"

namespace xpl {
namespace iface {

struct Encode_column_info;
}  // namespace iface
}  // namespace xpl

namespace ngs {

using Metadata_vector = std::vector<Column_info_builder>;

class Metadata_builder {
 public:
  void begin_metdata(const int num_of_columns) {
    m_columns.resize(num_of_columns);

    for (auto &c : m_columns) {
      c.reset();
    }
  }

  Metadata_vector &get_columns() { return m_columns; }

 private:
  Metadata_vector m_columns;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_METADATA_BUILDER_H_
