/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SUPPORTED_ROUTER_OPTIONS_INCLUDED
#define ROUTER_SUPPORTED_ROUTER_OPTIONS_INCLUDED

#include <array>
#include <string_view>

#ifdef _WIN32
static constexpr size_t router_supported_options_size = 10;
#else
static constexpr size_t router_supported_options_size = 9;
#endif

static constexpr std::array<std::string_view, router_supported_options_size>
    router_supported_options{"user",
                             "name",
                             "keyring_path",
                             "master_key_path",
                             "master_key_reader",
                             "master_key_writer",
                             "dynamic_state",
                             "max_total_connections",
                             "pid_file",
#ifdef _WIN32
                             "event_source_name"
#endif
    };

#endif /* ROUTER_SUPPORTED_ROUTER_OPTIONS_INCLUDED */
