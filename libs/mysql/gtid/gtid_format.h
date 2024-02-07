// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_GTID_GTID_FORMAT_H
#define MYSQL_GTID_GTID_FORMAT_H

#include <array>
#include <memory>
#include <string>
#include "mysql/utils/enumeration_utils.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

/// @brief Gtid binary format indicator
enum class Gtid_format : uint8_t {
  untagged = 0,  // untagged GTID
  tagged = 1,    // GTID with non-empty tag
  last,          // no valid constant may appear after this constant
};

}  // namespace mysql::gtid

namespace mysql::utils {
// we need to provide enum_max specialization for Gtid_format in the
// mysql::utils namespace
template <>
/// @brief Specialization of enum_max method for Gtid_format
/// @return Maximum Gtid_format constant that can appear
constexpr inline gtid::Gtid_format enum_max<gtid::Gtid_format>() {
  return gtid::Gtid_format::tagged;
}
}  // namespace mysql::utils

/// @}

#endif  // MYSQL_GTID_GTID_FORMAT_H
