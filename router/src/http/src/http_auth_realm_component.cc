/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <memory>
#include <mutex>
#include <string>

#include "http_auth_error.h"
#include "http_auth_realm.h"
#include "mysqlrouter/http_auth_realm_component.h"

void HttpAuthRealmComponent::init(std::shared_ptr<value_type> auth_realms) {
  auth_realms_ = auth_realms;
}

std::shared_ptr<HttpAuthRealm> HttpAuthRealmComponent::get(
    const std::string &inst) {
  if (auto realms = auth_realms_.lock()) {
    auto it = realms->find(inst);
    if (it == realms->end()) {
      return nullptr;
    }

    return it->second;
  } else {
    return nullptr;
  }
}

std::error_code HttpAuthRealmComponent::authenticate(
    const std::string &inst, const std::string &username,
    const std::string &authdata) {
  if (auto realm = get(inst)) {
    return realm->authenticate(username, authdata);
  } else {
    return std::make_error_code(HttpAuthErrc::kRealmNotFound);
  }
}

HttpAuthRealmComponent &HttpAuthRealmComponent::get_instance() {
  static HttpAuthRealmComponent instance;

  return instance;
}
