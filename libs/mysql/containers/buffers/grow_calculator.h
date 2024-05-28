/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

/// @file

#ifndef MYSQL_CONTAINERS_BUFFERS_GROW_CALCULATOR_H
#define MYSQL_CONTAINERS_BUFFERS_GROW_CALCULATOR_H

#include "mysql/containers/buffers/grow_constraint.h"  // Grow_constraint

#include <algorithm>  // std::min
#include <limits>     // std::numeric_limits
#include <string>     // std::string
#ifdef NDEBUG
#include <sstream>  // std::stringstream
#endif

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers::buffers {

/// Description of a heuristic to determine how much memory to allocate.
///
/// This may be used in diverse contexts such as growing a memory
/// buffer, or growing a pool of objects.
///
/// This encapsulates several common heuristics for growth:
///
/// - The growth rate can be exponential.  This is useful for cases
///   such as contiguous memory buffers, where each size increment may
///   copy all the existing data, e.g., using 'realloc'.  Or, more
///   generally, any data structure where size growth has a cost that
///   is linear in the total size.  In such cases an exponential
///   growth rate ensures that execution time is not quadratic in the
///   number of grow operations.
///
/// - The growth rate can be linear.  This is useful for cases such as
///   linked lists, where each size increment is linear in the
///   increment size.
///
/// - There can be an upper bound on the size.  This is useful
///   e.g. when there are configurable memory limits.
///
/// - The size can be specified to be a multiple of a given number.
///   This can potentially be useful if there is a way to align
///   allocated objects to page sizes, or similar.
class Grow_calculator : public Grow_constraint {
 public:
  using Grow_constraint::Size_t;
  /// Return type for compute_new_size.
  using Grow_constraint::Result_t;
  /// By default, limit memory to 1 GiB.
  static constexpr Size_t default_max_size =
      Size_t(1024) * Size_t(1024) * Size_t(1024);
  /// By default, double the size in each allocation.
  static constexpr double default_grow_factor = 2.0;
  /// By default, allocate at least 1 KiB more in each call.
  static constexpr Size_t default_grow_increment = 1024;
  /// By default, allocate multiples of 1 KiB.
  static constexpr Size_t default_block_size = 1024;

  Grow_calculator();

  /// Compute the new size.
  ///
  /// This follows the following rules:
  ///
  /// - It returns exceeds_max_size if the requested size, or the
  ///   existing size, exceeds the configured max size.
  ///
  /// - It never shrinks.  If the request is smaller than the existing
  ///   size, it just returns the existing size.
  ///
  /// - It multiplies the old size by the grow_factor, and if needed
  ///   increments the size further until it has grown by the
  ///   grow_increment, and then rounds up to the nearest multiple of
  ///   the block_size.  If the result of these operations exceeds the
  ///   max size, the result is reduced to the max size.
  ///
  /// @param old_size The existing size.
  ///
  /// @param requested_size The total size needed.
  ///
  /// @retval A pair.  The first component is `bool` and contains the
  /// error status: `false` means success, i.e., the requested size
  /// does not exceed the maximum size.  It also counts as success if
  /// the request is less than the existing size, or if the request is
  /// zero.  `true` means error, i.e., the requested size exceeds the
  /// maximum size.  The second component is the new size.  If the
  /// first component is `true` for error, the second component is
  /// zero.
  ///
  /// @retval other value The new size.
  Result_t compute_new_size(Size_t old_size, Size_t requested_size) const;
};

}  // namespace mysql::containers::buffers

/// @}

#endif /* MYSQL_CONTAINERS_BUFFERS_GROW_CALCULATOR_H */
