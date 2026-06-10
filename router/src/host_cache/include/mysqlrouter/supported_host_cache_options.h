/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_SUPPORTED_HOST_CACHE_OPTIONS_INCLUDED
#define MYSQLROUTER_SUPPORTED_HOST_CACHE_OPTIONS_INCLUDED

#include <cstdint>

namespace host_cache {
namespace options {

constexpr const char kOptionEnabled[]{"enabled"};
constexpr const char kOptionTtlSuccess[]{"ttl_success_seconds"};
constexpr const char kOptionTtlNegative[]{"ttl_negative_seconds"};
constexpr const char kOptionTtlJitter[]{"ttl_jitter_ratio"};
constexpr const char kOptionMaxEntries[]{"max_entries"};

static constexpr bool kDefaultEnabled{true};
static constexpr uint32_t kDefaultTtlSuccess{60};
static constexpr uint32_t kDefaultTtlNegative{10};
static constexpr double kDefaultTtlJitter{0.2};
static constexpr uint32_t kDefaultMaxEntries{250};

}  // namespace options
}  // namespace host_cache

#endif  // MYSQLROUTER_SUPPORTED_HOST_CACHE_OPTIONS_INCLUDED
