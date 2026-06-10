/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_APP_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_APP_H_

#include <set>
#include <vector>

#include "mrs/database/entry/universal_id.h"

#include "helper/json/serializer_to_text.h"
#include "helper/optional.h"

namespace mrs {
namespace database {
namespace entry {

class AuthApp {
 public:
  UniversalId id;
  std::set<UniversalId> service_ids;
  UniversalId vendor_id;
  std::string vendor_name;
  std::string app_name;
  bool active;
  bool deleted;
  std::string url;
  std::string url_validation;
  std::string app_id;
  std::string app_token;
  std::string url_access_token;
  bool limit_to_registered_users;
  helper::Optional<UniversalId> default_role_id;
};

inline helper::json::SerializerToText &operator<<(
    helper::json::SerializerToText &stt, const UniversalId &id) {
  stt.add_value(reinterpret_cast<const char *>(&id.raw[0]), id.k_size,
                helper::JsonType::kString);
  return stt;
}

inline std::string to_string(const AuthApp &entry) {
  const char *k_default_role_id = "default_role_id";
  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    stt.member_add_value("id", "0x" + entry.id.to_string());
    {
      auto arr = stt.member_add_array("service_id");
      for (const auto &id : entry.service_ids) {
        *arr << ("0x" + id.to_string());
      }
    }
    stt.member_add_value("name", entry.vendor_name);
    stt.member_add_value("limit_to_registered_users",
                         entry.limit_to_registered_users);
    if (!entry.url.empty()) {
      stt.member_add_value("url", entry.url);
    }

    if (!entry.url_access_token.empty()) {
      stt.member_add_value("url_access_token", entry.url_access_token);
    }

    if (!entry.url_validation.empty()) {
      stt.member_add_value("url_validation", entry.url_validation);
    }

    if (entry.default_role_id.has_value())
      stt.member_add_value(k_default_role_id,
                           "0x" + entry.default_role_id.value().to_string());
    else
      stt.member_add_null_value(k_default_role_id);
  }

  return stt.get_result();
}

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_APP_H_
