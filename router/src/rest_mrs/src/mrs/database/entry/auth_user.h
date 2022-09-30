/*
  Copyright (c) 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_USER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_USER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "mrs/database/entry/auth_privilege.h"

namespace mrs {
namespace database {
namespace entry {

struct AuthUser {
  using UserId = uint64_t;
  class UserIndex {
   public:
    UserIndex() {}
    UserIndex(const std::string &vendor_id) : vendor_user_id{vendor_id} {}
    UserIndex(const UserId id) : has_user_id{true}, user_id{id} {}
    UserIndex(const AuthUser &other) { *this = other; }
    UserIndex(AuthUser &&other)
        : has_user_id{other.has_user_id},
          user_id{other.user_id},
          vendor_user_id{std::move(other.vendor_user_id)} {}

    UserIndex &operator=(const AuthUser &other) {
      has_user_id = other.has_user_id;
      user_id = other.user_id;
      vendor_user_id = other.vendor_user_id;

      return *this;
    }

    bool operator==(const UserIndex &other) const {
      if (has_user_id && other.has_user_id) {
        return user_id == other.user_id;
      }

      return vendor_user_id.compare(other.vendor_user_id) == 0;
    }

    bool operator<(const UserIndex &other) const {
      if (has_user_id && other.has_user_id) {
        return user_id < other.user_id;
      }

      return vendor_user_id.compare(other.vendor_user_id) < 0;
    }

    bool has_user_id{false};
    UserId user_id{0};
    std::string vendor_user_id;
  };

  bool has_user_id{false};
  UserId user_id;
  uint64_t app_id;
  std::string name;
  std::string email;
  std::string vendor_user_id;
  bool login_permitted{true};
  std::vector<AuthPrivilege> privileges;
  std::set<uint64_t> groups;

  bool operator==(const AuthUser &other) const {
    if (has_user_id && other.has_user_id) {
      if (user_id != other.user_id) return false;
    }

    if (app_id != other.app_id) return false;

    if (name != other.name) return false;

    if (email != other.email) return false;

    if (vendor_user_id != other.vendor_user_id) return false;

    if (login_permitted != other.login_permitted) return false;

    return true;
  }

  bool match_other_fields(const AuthUser &other) const {
    if (!name.empty()) {
      if (name == other.name) return true;
    }

    if (!email.empty()) {
      if (email == other.email) return true;
    }

    return false;
  }
};

inline std::string to_string(const AuthUser &ud) {
  using std::to_string;
  std::string result{"{"};
  bool first = true;
  std::map<std::string, std::string> map;

  if (ud.has_user_id) map["user_id"] = std::to_string(ud.user_id);
  if (!ud.name.empty()) map["name"] = ud.name;
  if (!ud.email.empty()) map["email"] = ud.email;
  if (!ud.vendor_user_id.empty()) map["vendor_user_id"] = ud.vendor_user_id;
  map["login_permitted"] = ud.login_permitted ? "true" : "false";

  for (const auto &kv : map) {
    result += (first ? "'" : ", '");
    result += kv.first + "':'";
    result += kv.second + "'";

    first = false;
  }

  result += "}";
  return result;
}

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_USER_H_
