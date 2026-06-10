/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_VERSION_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_VERSION_H_

#include <array>

#include "mrs/database/entry/auth_user.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

struct MrsSchemaVersion {
  int major{0};
  int minor{0};
  int patch{0};

  bool is_compatible(
      std::initializer_list<MrsSchemaVersion> accepted_versions) const {
    for (const auto &other : accepted_versions) {
      if (major == other.major) {
        return true;
      }
    }

    return false;
  }

  bool operator==(const MrsSchemaVersion &o) const {
    return major == o.major && minor == o.minor && patch == o.patch;
  }

  bool operator!=(const MrsSchemaVersion &o) const { return !operator==(o); }

  bool operator<(const MrsSchemaVersion &o) const {
    if (major == o.major) {
      if (minor == o.minor) {
        return patch < o.patch;
      } else {
        return minor < o.minor;
      }
    } else {
      return major < o.major;
    }
  }

  std::string str() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
  }
};

class QueryVersion : private Query {
 public:
  MrsSchemaVersion query_version(MySQLSession *session);

 private:
  void on_metadata(unsigned number, MYSQL_FIELD *fields) override;
  void on_row(const ResultRow &r) override;
  MrsSchemaVersion v_;
};

constexpr MrsSchemaVersion kCurrentMrsMetadataVersion{4, 0, 0};

constexpr MrsSchemaVersion kSchemaUpgradeMrsMetadataVersion{0, 0, 0};

constexpr std::initializer_list<MrsSchemaVersion>
    kCompatibleMrsMetadataVersions{mrs::database::kCurrentMrsMetadataVersion,
                                   {2, 2}};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_VERSION_H_
