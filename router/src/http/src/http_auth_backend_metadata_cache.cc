/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "http_auth_backend_metadata_cache.h"

#include <algorithm>
#include <fstream>
#include <istream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

#include "http_auth_error.h"
#include "kdf_pbkdf2.h"
#include "kdf_sha_crypt.h"
#include "mysqlrouter/metadata_cache.h"

std::error_code HttpAuthBackendMetadataCache::authorize(
    const rapidjson::Document &privileges) {
  if (!privileges.IsNull())
    return make_error_code(HttpAuthErrc::kAuthorizationNotSupported);

  return {};
}

std::error_code HttpAuthBackendMetadataCache::authenticate(
    const std::string &username, const std::string &password) {
  if (!metadata_cache::MetadataCacheAPI::instance()->is_initialized())
    return make_error_code(McfErrc::kMetadataNotInitialized);

  const auto auth_data_maybe =
      metadata_cache::MetadataCacheAPI::instance()->get_rest_user_auth_data(
          username);
  if (!auth_data_maybe.first) return make_error_code(McfErrc::kUserNotFound);

  const auto &auth_data = auth_data_maybe.second;
  const auto &encoded_hash = auth_data.first;
  const auto &privileges = auth_data.second;

  if (encoded_hash.empty() && password.empty()) return {};

  const auto &ec = authorize(privileges);
  if (ec) return ec;

  return ShaCryptMcfAdaptor::validate(encoded_hash, password);
}
