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

#ifndef MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_GROW_CONSTRAINT_H
#define MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_GROW_CONSTRAINT_H

#include "mysql/binlog/event/nodiscard.h"  // NODISCARD

#include <algorithm>  // std::min
#include <limits>     // std::numeric_limits
#include <string>     // std::string
#ifndef NDEBUG
#include <sstream>  // std::stringstream
#endif

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event::compression::buffer {

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
class Grow_constraint {
 public:
  using Size_t = std::size_t;
  /// Return type for compute_new_size.
  using Result_t = std::pair<bool, Size_t>;
  /// Maximum allowed value for the application max size.
  static constexpr Size_t machine_max_size = std::numeric_limits<Size_t>::max();

  Grow_constraint() = default;
  Grow_constraint(const Grow_constraint &other) = default;
  Grow_constraint(Grow_constraint &&other) = default;
  Grow_constraint &operator=(const Grow_constraint &other) = default;
  Grow_constraint &operator=(Grow_constraint &&other) = default;
  virtual ~Grow_constraint() = default;

  /// Set the maximum size.
  ///
  /// Whenever more than this is requested, the response should be to
  /// fail.  This is an inclusive upper bound, so requests for exactly
  /// this size are allowed.
  void set_max_size(Size_t max_size);

  /// @return the maximum size.
  Size_t get_max_size() const;

  /// Set the grow factor.
  ///
  /// Whenever the size needs to increase, it should increase it by at
  /// least this factor.
  ///
  /// Using a value > 1 ensures that successive calls to reserve()
  /// with sizes increasing up to N take amortized linear time in N; a
  /// value equal to 1 may result in execution time that is quadratic
  /// in N.
  void set_grow_factor(double grow_factor);

  /// @return the grow factor.
  double get_grow_factor() const;

  /// Set the grow increment.
  ///
  /// Whenever the size needs to increase, it should increase by
  /// at least this amount.
  void set_grow_increment(Size_t grow_increment);

  /// @return the grow increment.
  Size_t get_grow_increment() const;

  /// Set the block size.
  ///
  /// The size should be kept to a multiple of this number.
  void set_block_size(Size_t block_size);

  /// @return the block size.
  Size_t get_block_size() const;

  /// In debug mode, return a string that describes the internal
  /// structure of this object, to use for debugging.
  std::string debug_string() const {
#ifdef NDEBUG
    return "";
#else
    std::ostringstream ss;
    // clang-format off
    ss << "Grow_constraint(ptr=" << (const void *)this
       << ", max_size=" << m_max_size
       << ", grow_factor=" << m_grow_factor
       << ", grow_increment=" << m_grow_increment
       << ", block_size=" << m_block_size
       << ")";
    // clang-format on
    return ss.str();
#endif
  }

  /// Combine the constraints of this object with another
  /// Grow_constraint or Grow_calculator object.
  ///
  /// This will return a new object of the same type as the argument.
  /// The returned object will have the smallest max_size among `this`
  /// and `other`, and the largest `grow_factor`, `grow_increment`, and
  /// `block_size`.
  template <class T>
  [[NODISCARD]] T combine_with(const T &other) const {
    T ret;
    ret.set_max_size(std::min(get_max_size(), other.get_max_size()));
    ret.set_grow_factor(std::max(get_grow_factor(), other.get_grow_factor()));
    ret.set_grow_increment(
        std::max(get_grow_increment(), other.get_grow_increment()));
    ret.set_block_size(std::max(get_block_size(), other.get_block_size()));
    return ret;
  }

 private:
  /// Size must not exceed this number.
  Size_t m_max_size{machine_max_size};

  /// By default, don't constrain the grow factor.
  static constexpr double default_grow_factor = 1.0;

  /// By default, don't constrain the grow increment.
  static constexpr Size_t default_grow_increment = 0;

  /// By default, don't constrain the block size.
  static constexpr Size_t default_block_size = 1;

  // Size should grow by at least this factor.
  double m_grow_factor{default_grow_factor};

  // Size should grow by at least this number of bytes.
  Size_t m_grow_increment{default_grow_increment};

  // Size should be rounded up to a multiple of at least this number of bytes.
  Size_t m_block_size{default_block_size};
};

}  // namespace mysql::binlog::event::compression::buffer

/// @}

#endif /* MYSQL_BINLOG_EVENT_COMPRESSION_BUFFER_GROW_CONSTRAINT_H */
