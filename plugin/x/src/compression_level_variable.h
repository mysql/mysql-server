/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_COMPRESSION_LEVEL_VARIABLE_H_
#define PLUGIN_X_SRC_COMPRESSION_LEVEL_VARIABLE_H_

#include <cstdint>

#include "plugin/x/protocol/stream/compression/compression_algorithm_lz4.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zlib.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zstd.h"

namespace xpl {

template <typename Commpresion_algorithm>
class Compression_level_variable {
 public:
  Compression_level_variable() = default;
  static int32_t min() { return Commpresion_algorithm::get_level_min(); }
  static int32_t max() { return Commpresion_algorithm::get_level_max(); }
  int32_t *value() { return &m_value; }
  static bool check_range(const int32_t level) {
    return level >= min() && level <= max();
  }

 private:
  int32_t m_value{0};
};

template <>
inline bool
Compression_level_variable<::protocol::Compression_algorithm_zstd>::check_range(
    const int32_t level) {
  return level != 0 && level >= min() && level <= max();
}

using Compression_deflate_level_variable =
    Compression_level_variable<::protocol::Compression_algorithm_zlib>;
using Compression_lz4_level_variable =
    Compression_level_variable<::protocol::Compression_algorithm_lz4>;
using Compression_zstd_level_variable =
    Compression_level_variable<::protocol::Compression_algorithm_zstd>;

}  // namespace xpl

#endif  // PLUGIN_X_SRC_COMPRESSION_LEVEL_VARIABLE_H_
