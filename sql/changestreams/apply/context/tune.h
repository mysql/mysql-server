// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_TUNE_H
#define MYSQL_CSA_TUNE_H

#include "sql/sql_class.h"

namespace mysql::csa::tune {

// resource tracking
inline constexpr bool enabled_resource_tracking{true};
// session
inline constexpr std::size_t csa_session_default_cache_size{2048};
// scheduler
inline constexpr std::size_t scheduler_clock_capacity{16384};
inline constexpr std::size_t scheduler_tp_queue_size{8192};
// max task number = factor * number of threads
static constexpr std::size_t scheduler_max_task_number_factor = 4;
// prefetcher
inline constexpr bool prefetcher_enable{false};
inline constexpr bool csa_prefetcher_simple_queue{false};
inline constexpr std::size_t prefetcher_queue_max_size{128};
inline constexpr std::size_t prefetcher_batch_size{100 * 1024};
// provider
inline constexpr std::size_t provider_max_read_event_bytes{512};
inline constexpr std::size_t provider_max_read_payload_bytes{512};
inline constexpr bool csa_provider_enable_tune{true};
inline constexpr bool extended_statistics_enabled{false};

void csa_show_config_info(const char *channel_name);

}  // namespace mysql::csa::tune

#endif  // MYSQL_CSA_TUNE_H
