// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_GTIDS_SEQUENCE_NUMBER_H
#define MYSQL_GTIDS_SEQUENCE_NUMBER_H

/// @file
/// Experimental API header

#include <cstdint>  // int64_t
#include <limits>   // numeric_limits

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::gtids {

/// The type of the sequence number component of a GTID.
using Sequence_number = uint64_t;

/// One plus the largest allowed value for a GTID sequence number.
constexpr Sequence_number sequence_number_max_exclusive =
    std::numeric_limits<int64_t>::max();

/// The largest allowed value for a GTID sequence number.
constexpr Sequence_number sequence_number_max_inclusive =
    sequence_number_max_exclusive - 1;

/// The smallest allowed value for a GTID sequence number.
constexpr Sequence_number sequence_number_min = 1;

/// Return true if the given Sequence_number is in the allowed range.
constexpr bool is_valid_sequence_number(Sequence_number sequence_number) {
  return sequence_number >= sequence_number_min &&
         sequence_number <= sequence_number_max_inclusive;
}

}  // namespace mysql::gtids

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_SEQUENCE_NUMBER_H
