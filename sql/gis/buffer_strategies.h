#ifndef SQL_GIS_BUFFER_STRATEGIES_H_INCLUDED
#define SQL_GIS_BUFFER_STRATEGIES_H_INCLUDED

// Copyright (c) 2021, 2022, Oracle and/or its affiliates.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License, version 2.0,
//  as published by the Free Software Foundation.
//
//  This program is also distributed with certain software (including
//  but not limited to OpenSSL) that is licensed under separate terms,
//  as designated in a particular file or component or in included license
//  documentation.  The authors of MySQL hereby grant you an additional
//  permission to link the program and your derivative works with the
//  separately licensed software that they have included with MySQL.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License, version 2.0, for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

/// @file
///
/// This file implements a struct that is used to hold information that
/// decide how to compute the buffer of a geometry.

#include <cstddef>

namespace gis {

// Used as cases in the switch in Item_func_st_buffer::parse_strategy.
// Cannot use enum because switching on type uint.
constexpr uint kEndRound = 1;
constexpr uint kEndFlat = 2;
constexpr uint kJoinRound = 3;
constexpr uint kJoinMiter = 4;
constexpr uint kPointCircle = 5;
constexpr uint kPointSquare = 6;

struct BufferStrategies {
  // See Boost's documentation for information on bg::buffer's strategies.

  std::size_t join_circle_value = 32;
  std::size_t end_circle_value = 32;
  std::size_t point_circle_value = 32;
  double join_miter_value = 5.0;
  double distance = 0;

  bool join_is_set = false;
  bool end_is_set = false;
  bool point_is_set = false;

  /// 8 possible combinations since 'End = round || flat', 'Join = round ||
  /// miter', and 'Point = circle || square'. Default combo is 0, with
  /// default values. Combo made by bitwise OR-ing.
  /// 0 : join_round, end_round, point_circle
  /// 1 : join_round, end_flat,  point_circle
  /// 2 : join_miter, end_round, point_circle
  /// 3 : join_miter, end_flat,  point_circle
  /// 4 : join_round, end_round, point_square
  /// 5 : join_round, end_flat,  point_square
  /// 6 : join_miter, end_round, point_square
  /// 7 : join_miter, end_flat,  point_square
  int combination = 0;

  bool set_end_round(double value) {
    if (end_is_set ||
        value >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      return true;
    } else {
      end_is_set = true;
      end_circle_value = static_cast<std::size_t>(value);
      return false;
    }
  }

  bool set_end_flat() {
    if (end_is_set) {
      return true;
    } else {
      end_is_set = true;
      combination |= 1;
      return false;
    }
  }

  bool set_join_round(double value) {
    if (join_is_set ||
        value >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      return true;
    } else {
      join_is_set = true;
      join_circle_value = static_cast<std::size_t>(value);
      return false;
    }
  }

  bool set_join_miter(double value) {
    if (join_is_set) {
      return true;
    } else {
      join_is_set = true;
      combination |= 2;
      join_miter_value = value;
      return false;
    }
  }

  bool set_point_circle(double value) {
    if (point_is_set ||
        value >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      return true;
    } else {
      point_is_set = true;
      point_circle_value = static_cast<std::size_t>(value);
      return false;
    }
  }

  bool set_point_square() {
    if (point_is_set) {
      return true;
    } else {
      point_is_set = true;
      combination |= 4;
      return false;
    }
  }
};

}  // namespace gis

#endif  // SQL_GIS_BUFFER_H_INCLUDED
