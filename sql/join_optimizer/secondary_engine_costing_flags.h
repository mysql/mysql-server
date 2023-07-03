/* Copyright (c) 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_JOIN_OPTIMIZER_SECONDARY_ENGINE_COSTING_FLAGS_H_
#define SQL_JOIN_OPTIMIZER_SECONDARY_ENGINE_COSTING_FLAGS_H_

#include <cstdint>

/**
  @file

  For updating an AccessPath's costs by a secondary engine, i.e. costing
  a partial plan, the secondary engine may need to know ahead of time
  certain information about the current planning, for which we provide
  some flags here.

 */

using SecondaryEngineCostingFlags = uint64_t;

enum class SecondaryEngineCostingFlag : SecondaryEngineCostingFlags {
  HAS_MULTIPLE_BASE_TABLES,
  CONTAINS_AGGREGATION_ACCESSPATH,
  CONTAINS_WINDOW_ACCESSPATH,
  HANDLING_DISTINCT_ORDERBY_LIMITOFFSET
};

/// Creates an empty bitmap of costing flags. This is the base
/// case for the function template with the same name below.
constexpr inline SecondaryEngineCostingFlags MakeSecondaryEngineCostingFlags() {
  return 0;
}

/// Creates a bitmap representing a set of costing flags.
template <typename... Args>
constexpr inline SecondaryEngineCostingFlags MakeSecondaryEngineCostingFlags(
    const SecondaryEngineCostingFlag &flag1, const Args &... rest) {
  return (uint64_t{1} << static_cast<SecondaryEngineCostingFlags>(flag1)) |
         MakeSecondaryEngineCostingFlags(rest...);
}

constexpr inline SecondaryEngineCostingFlags operator|(
    const SecondaryEngineCostingFlags &a, const SecondaryEngineCostingFlag &b) {
  return a | MakeSecondaryEngineCostingFlags(b);
}

constexpr inline SecondaryEngineCostingFlags operator|(
    const SecondaryEngineCostingFlag &a, const SecondaryEngineCostingFlags &b) {
  return MakeSecondaryEngineCostingFlags(a) | b;
}

constexpr inline SecondaryEngineCostingFlags &operator|=(
    SecondaryEngineCostingFlags &a, const SecondaryEngineCostingFlag &b) {
  return a |= MakeSecondaryEngineCostingFlags(b);
}

constexpr inline SecondaryEngineCostingFlags operator&(
    const SecondaryEngineCostingFlags &a, const SecondaryEngineCostingFlag &b) {
  return a & MakeSecondaryEngineCostingFlags(b);
}

constexpr inline SecondaryEngineCostingFlags operator&(
    const SecondaryEngineCostingFlag &a, const SecondaryEngineCostingFlags &b) {
  return MakeSecondaryEngineCostingFlags(a) & b;
}

constexpr inline SecondaryEngineCostingFlags &operator&=(
    SecondaryEngineCostingFlags &a, const SecondaryEngineCostingFlag &b) {
  return a &= MakeSecondaryEngineCostingFlags(b);
}

constexpr inline SecondaryEngineCostingFlags operator~(
    const SecondaryEngineCostingFlag &flag) {
  return ~MakeSecondaryEngineCostingFlags(flag);
}

#endif /* SQL_JOIN_OPTIMIZER_SECONDARY_ENGINE_COSTING_FLAGS_H_ */
