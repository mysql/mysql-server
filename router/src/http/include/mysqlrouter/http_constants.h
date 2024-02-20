/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_HTTP_CONSTANTS_INCLUDED
#define MYSQLROUTER_HTTP_CONSTANTS_INCLUDED

#include <string_view>

constexpr const std::string_view kHttpAuthPluginDefaultBackend{
    "metadata_cache"};
constexpr const std::string_view kHttpDefaultAuthBackendName{
    "default_auth_backend"};
constexpr const std::string_view kHttpDefaultAuthRealmName{
    "default_auth_realm"};
constexpr const std::string_view kHttpDefaultAuthMethod{"basic"};

constexpr const uint16_t kHttpPluginDefaultPortBootstrap{8443};
constexpr const unsigned kHttpPluginDefaultSslBootstrap{1};

#endif  // MYSQLROUTER_HTTP_CONSTANTS_INCLUDED
