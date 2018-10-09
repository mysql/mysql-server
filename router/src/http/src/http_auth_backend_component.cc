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

#include "mysqlrouter/http_auth_backend_component.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#include "http_auth_backend.h"
#include "http_auth_error.h"

void HttpAuthBackendComponent::init(std::shared_ptr<value_type> auth_backends) {
  auth_backends_ = auth_backends;
}

std::error_code HttpAuthBackendComponent::authenticate(
    const std::string &inst, const std::string &username,
    const std::string &authdata) {
  if (auto backends = auth_backends_.lock()) {
    auto it = backends->find(inst);
    if (it == backends->end()) {
      return std::make_error_code(HttpAuthErrc::kBackendNotFound);
    }

    return it->second->authenticate(username, authdata);
  } else {
    // backends couldn't get locked
    return std::make_error_code(std::errc::invalid_argument);
  }
}

HttpAuthBackendComponent &HttpAuthBackendComponent::get_instance() {
  static HttpAuthBackendComponent instance;

  return instance;
}
