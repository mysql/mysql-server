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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_ENCODE_COLUMN_INFO_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_ENCODE_COLUMN_INFO_H_

#include <cstdint>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

namespace ngs {

struct Encode_column_info {
  const char *m_catalog = "";
  const char *m_db_name = "";
  const char *m_table_name = "";
  const char *m_org_table_name = "";
  const char *m_col_name = "";
  const char *m_org_col_name = "";

  uint64_t *m_collation_ptr{nullptr};
  int32_t m_type = 0;
  uint32_t *m_decimals_ptr{nullptr};
  uint32_t *m_flags_ptr{nullptr};
  uint32_t *m_length_ptr{nullptr};
  uint32_t *m_content_type_ptr{nullptr};

  bool m_compact{true};
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_ENCODE_COLUMN_INFO_H_
