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

#ifndef MYSQL_GTID_TAG_PLAIN_H
#define MYSQL_GTID_TAG_PLAIN_H

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "mysql/gtid/gtid_format.h"
#include "mysql/gtid/tag.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

class Tag;

/// @brief Tag representation so that:
/// - Tag_plain is trivial
/// - Tag_plain is standard layout
/// Therefore, a Tag_plain object is not empty by default! Use clear() or
/// brace-enclosed initializer list to create an empty tag.
/// Tag_plain must be a POD because it is used in Gtid_specification
struct Tag_plain {
  /// @brief Default ctor
  Tag_plain() = default;
  /// @brief Construct from tag object
  /// @param tag tag to copy
  explicit Tag_plain(const Tag &tag);

  /// @brief Clear this tag
  void clear();

  /// @brief Checks whether tag is defined
  /// @retval true Tag is defined
  /// @retval false Tag is empty
  bool is_defined() const;

  /// @brief Obtains tag length
  /// @return tag length
  std::size_t length() const;

  /// @brief Copies internal tag into a given buffer
  /// @param[in,out] buf Buffer, needs to be pre-allocated
  /// @return Number of bytes written into the buffer
  std::size_t to_string(char *buf) const;

  /// @brief Sets internal data to match a given pattern
  /// @param[in] tag Pattern to copy from
  void set(const Tag &tag);

  /// @brief Internal data accessor (for encoding)
  const unsigned char *data() const;

 private:
  /// null terminated tag representation
  unsigned char m_data[tag_max_length + 1];
};

static_assert(std::is_trivial_v<Tag_plain>);
static_assert(std::is_standard_layout_v<Tag_plain>);

}  // namespace mysql::gtid

/// @}

#endif  // MYSQL_GTID_TAG_PLAIN_H
