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

#ifndef MYSQL_GTID_TSID_PLAIN_H
#define MYSQL_GTID_TSID_PLAIN_H

#include "mysql/gtid/tag_plain.h"
#include "mysql/gtid/uuid.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

class Tsid;

/// @brief TSID representation so that:
/// - Tsid_plain is trivial
/// - Tsid_plain is a standard layout type
struct Tsid_plain {
  /// @brief Default ctor
  Tsid_plain() = default;
  /// @brief Construct from tsid object
  /// @param tsid tsid to copy
  explicit Tsid_plain(const Tsid &tsid);

  /// @brief Clear this TSID
  void clear();

  /// @brief Copies internal tsid into a given buffer
  /// @param[out] out Buffer, needs to be pre-allocated
  /// @return Number of bytes written into the buffer
  std::size_t to_string(char *out) const;

  friend class Tsid;

 private:
  Uuid m_uuid;      ///< GTID UUID
  Tag_plain m_tag;  ///< GTID Tag
};

static_assert(std::is_trivial_v<Uuid>);
static_assert(std::is_standard_layout_v<Uuid>);

}  // namespace mysql::gtid

/// @}

#endif  // MYSQL_GTID_TSID_PLAIN_H
