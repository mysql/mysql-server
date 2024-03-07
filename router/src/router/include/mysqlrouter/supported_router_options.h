/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SUPPORTED_ROUTER_OPTIONS_INCLUDED
#define ROUTER_SUPPORTED_ROUTER_OPTIONS_INCLUDED

#include <array>
#include <string_view>

namespace router {
namespace options {
static constexpr std::string_view kUser{"user"};
static constexpr std::string_view kName{"name"};
static constexpr std::string_view kKeyringPath{"keyring_path"};
static constexpr std::string_view kMasterKeyPath{"master_key_path"};
static constexpr std::string_view kMasterKeyReader{"master_key_reader"};
static constexpr std::string_view kMasterKeyWriter{"master_key_writer"};
static constexpr std::string_view kDynamicState{"dynamic_state"};
static constexpr std::string_view kMaxTotalConnections{"max_total_connections"};
static constexpr std::string_view kPidFile{"pid_file"};
#ifdef _WIN32
static constexpr std::string_view kEventSourceName{"event_source_name"};
#endif
}  // namespace options
}  // namespace router

static constexpr std::array router_supported_options [[maybe_unused]]{
    router::options::kUser,
    router::options::kName,
    router::options::kKeyringPath,
    router::options::kMasterKeyPath,
    router::options::kMasterKeyReader,
    router::options::kMasterKeyWriter,
    router::options::kDynamicState,
    router::options::kMaxTotalConnections,
    router::options::kPidFile,
#ifdef _WIN32
    router::options::kEventSourceName,
#endif
};

#endif /* ROUTER_SUPPORTED_ROUTER_OPTIONS_INCLUDED */
