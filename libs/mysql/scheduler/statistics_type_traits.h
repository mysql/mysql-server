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

#ifndef MYSQL_SCHEDULER_STATISTICS_TYPE_TRAITS_H
#define MYSQL_SCHEDULER_STATISTICS_TYPE_TRAITS_H

#include "mysql/scheduler/sharded_counter.h"

namespace mysql::scheduler {

template <typename T>
concept Statistic_allowed_type = std::same_as<T, long long>;

/// @brief Helper structure that translates types between underlying supported
/// statistic type and concrete concurrent counter / type implementation
/// @tparam Type Value type to be translated
template <Statistic_allowed_type Type>
struct Statistic_type_traits;

/// Specialization of Statistic_type_traits with long long statistic type
template <>
struct Statistic_type_traits<long long> {
  /// Type of the counter held in the Statistics Instance Monitor
  using type = Sharded_counter;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_STATISTICS_TYPE_TRAITS_H
