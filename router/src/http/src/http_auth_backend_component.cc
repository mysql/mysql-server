/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

std::error_code HttpAuthBackendComponent::authenticate(
    const std::string &inst, const std::string &username,
    const std::string &authdata) {
  std::lock_guard<std::mutex> lk(backends_m_);

  auto it = auth_backends_.find(inst);
  if (it == auth_backends_.end()) {
    return make_error_code(HttpAuthErrc::kBackendNotFound);
  }

  return it->second->authenticate(username, authdata);
}

void HttpAuthBackendComponent::add_backend(
    const std::string &name, std::shared_ptr<HttpAuthBackend> backend) {
  std::lock_guard<std::mutex> lk(backends_m_);

  auth_backends_[name] = std::move(backend);
}

void HttpAuthBackendComponent::remove_backend(const std::string &name) {
  std::lock_guard<std::mutex> lk(backends_m_);

  const auto it = auth_backends_.find(name);
  if (it != auth_backends_.end()) {
    auth_backends_.erase(it);
  }
}

HttpAuthBackendComponent &HttpAuthBackendComponent::get_instance() {
  static HttpAuthBackendComponent instance;

  return instance;
}
