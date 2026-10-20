/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#pragma once

#include <climits>  // ULLONG_MAX
#include <cstdint>
#include <limits>  // numeric_limits

namespace hm {

static constexpr int max_int{std::numeric_limits<int>::max()};
static constexpr uint64_t max_uint64_t{std::numeric_limits<uint64_t>::max()};

static constexpr uint64_t microseconds_per_second{1000000};
static constexpr uint64_t hundredth_ns{10000000};

static constexpr uint64_t days_per_year{365};
static constexpr uint64_t seconds_per_minute{60};
static constexpr uint64_t seconds_per_hour{3600};
static constexpr uint64_t seconds_per_day{86400};
static constexpr uint64_t seconds_per_week{seconds_per_day * 7};
static constexpr uint64_t seconds_per_month{seconds_per_day * 30};
static constexpr uint64_t seconds_per_year{seconds_per_day * days_per_year};

static constexpr uint64_t minutes_per_hour{60};
static constexpr uint64_t minutes_per_day{minutes_per_hour * 24};
static constexpr uint64_t minutes_per_month{minutes_per_day * 30};

static constexpr uint64_t mib{1};
static constexpr uint64_t gib{1024 * mib};
static constexpr uint64_t tib{1024 * gib};
static constexpr uint64_t pib{1024 * tib};
static constexpr uint64_t eib{1024 * pib};

static constexpr int megabyte{1048576};
static constexpr double MiB{1048576.0};

enum class Yes_No { YES = 1, NO = 2 };

// Test value for maximum resource level used to provoke warnings
// TODO: Remove when the resource simulator (WL#15935) is implemented
static constexpr uint64_t k_max_level{ULLONG_MAX - 1};

}  // namespace hm
