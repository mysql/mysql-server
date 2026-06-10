// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_GTIDS_STRCONV_GTID_BINARY_FORMAT_H
#define MYSQL_GTIDS_STRCONV_GTID_BINARY_FORMAT_H

/// @file
/// Experimental API header

#include <cassert>                          // assert
#include "mysql/gtids/gtid.h"               // Is_gtid
#include "mysql/gtids/gtid_set.h"           // Is_gtid_set
#include "mysql/gtids/tag.h"                // Is_tag
#include "mysql/gtids/tsid.h"               // Is_tsid
#include "mysql/strconv/strconv.h"          // Format_base
#include "mysql/utils/enumeration_utils.h"  // enum_max

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::strconv {

struct Gtid_binary_format : Format_base {
  // NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint

  /// The format version. See readme.md for format specifications.
  enum class Version {
    /// Version 0, which does not support tags.
    /// Encoding a nonempty tag with this is undefined behavior.
    /// Decoding a tag with this clears the tag without reading any input.
    v0_tagless = 0,

    /// Version 1, which supports tags.
    v1_tags = 1,

    /// Version 2, which supports tags and is more space-efficient.
    v2_tags_compact = 2
  };

  /// Policy for choosing a version. This either a specified version, or
  /// 'automatic'.
  enum class Version_policy {
    v0_tagless = 0,
    v1_tags = 1,
    v2_tags_compact = 2,

    /// Encode using an automatically selected format. Currently, this favors
    /// compatibility and uses the minimum version supported by the object type,
    /// i.e.:
    /// - v0_tagless if the set does not have tags
    /// - v1_tags if the set has tags.
    ///
    /// Decode Gtid sets using whatever format is encoded in the object.
    ///
    /// Decode Gtids/Tsids/Uuids/Tags using v1 (which coincides with v2 for
    /// these objects).
    automatic = 3
  };

  // NOLINTEND(performance-enum-size)

  /// Returns the Version_policy that specifies the given concrete version.
  [[nodiscard]] static Version_policy to_version_policy(Version version) {
    switch (version) {
      case Version::v0_tagless:
        return Version_policy::v0_tagless;
      case Version::v1_tags:
        return Version_policy::v1_tags;
      case Version::v2_tags_compact:
        return Version_policy::v2_tags_compact;
      default:
        assert(0);
        return Version_policy::v1_tags;
    }
  }

  Gtid_binary_format() = default;
  explicit constexpr Gtid_binary_format(const Version_policy &version_policy)
      : m_version_policy(version_policy) {}

  [[nodiscard]] auto parent() const { return Binary_format{}; }

  /// Policy for the version to use.
  ///
  /// Note: user code should rely on the default, which is `automatic`. Other
  /// modes are available only for unittests.
  Version_policy m_version_policy = Version_policy::automatic;
};

template <class Object_t>
  requires mysql::gtids::Is_tag<Object_t> || mysql::gtids::Is_gtid<Object_t> ||
           mysql::gtids::Is_tsid<Object_t> ||
           mysql::gtids::Is_gtid_set<Object_t>
auto get_default_format(const Binary_format &, const Object_t &) {
  return Gtid_binary_format{};
}

}  // namespace mysql::strconv

namespace mysql::utils {
template <>
constexpr mysql::strconv::Gtid_binary_format::Version
enum_max<mysql::strconv::Gtid_binary_format::Version>() {
  return mysql::strconv::Gtid_binary_format::Version::v2_tags_compact;
}
}  // namespace mysql::utils

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_STRCONV_GTID_BINARY_FORMAT_H
