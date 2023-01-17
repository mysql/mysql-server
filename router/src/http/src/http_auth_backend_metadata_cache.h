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

#ifndef ROUTER_HTTP_AUTH_METADATA_CACHE_INCLUDED
#define ROUTER_HTTP_AUTH_METADATA_CACHE_INCLUDED

#include "http_auth_backend.h"

#include "mysqlrouter/http_auth_backend_export.h"

/**
 * Authentication backend based on the information stored in the metadata.
 *
 * Authentication is based on the credentials stored in the
 * 'router_rest_accounts' table. Password hashes are stored according to the
 * following pattern:
 * `$<id>$<rounds>$<salt>$<hash>`
 * Where:
 * id - set to "A", only caching_sha2_password is supported currently
 * rounds - three digits number containing hashing rounds divided by 1000
 * salt - password salt, length 20
 * hash - password hash
 */
class HTTP_AUTH_BACKEND_EXPORT HttpAuthBackendMetadataCache
    : public HttpAuthBackend {
 public:
  /**
   * validate user and authdata against backend.
   *
   * @returns error
   * @retval false no authentication error
   */
  std::error_code authenticate(const std::string &username,
                               const std::string &authdata) override;

  /**
   * Check user privileges and perform authorization. Empty privileges document
   * means 'all privileges'.
   *
   * @param[in] privileges - JSON document containing user privileges.
   * @returns error
   * @retval false no authorization error
   */
  std::error_code authorize(const rapidjson::Document &privileges);
};

#endif  // ROUTER_HTTP_AUTH_METADATA_CACHE_INCLUDED
