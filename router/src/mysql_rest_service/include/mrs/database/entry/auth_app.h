/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_APP_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_APP_H_

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
  UniversalId service_id;
  UniversalId vendor_id;
  std::string service_name;
  std::string vendor_name;
  std::string app_name;
  bool active;
  bool deleted;
  std::string url;
  std::string url_validation;
  std::string app_id;
  std::string app_token;
  // TODO(lkotula): Remove ? (Shouldn't be in review)
  std::string host;
  std::string host_alias;
  std::string url_access_token;
  bool limit_to_registered_users;
  helper::Optional<UniversalId> default_role_id;
  std::string auth_path;
  std::string options;
  std::string redirect;
  std::string redirection_default_page;
};

inline helper::json::SerializerToText &operator<<(
    helper::json::SerializerToText &stt, const UniversalId &id) {
  stt.add_value(reinterpret_cast<const char *>(&id.raw[0]), id.k_size,
                helper::JsonType::kString);
  return stt;
}

inline std::string to_string(const AuthApp &entry) {
  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    stt.member_add_value("id", entry.id);
    stt.member_add_value("service_id", entry.service_id);
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

    stt.member_add_value("default_role_id", entry.default_role_id);
  }

  return stt.get_result();
}

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_APP_H_
