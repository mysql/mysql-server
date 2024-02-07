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

#include "mysql/binlog/event/compression/buffer/grow_calculator.h"
#include "mysql/binlog/event/math/math.h"          // add_bounded
#include "mysql/binlog/event/wrapper_functions.h"  // BAPI_TRACE

#include <cassert>

namespace mysql::binlog::event::compression::buffer {

Grow_calculator::Grow_calculator() {
  set_max_size(default_max_size);
  set_grow_factor(default_grow_factor);
  set_grow_increment(default_grow_increment);
  set_block_size(default_block_size);
}

Grow_calculator::Result_t Grow_calculator::compute_new_size(
    Size_t old_size, Size_t requested_size) const {
  BAPI_TRACE;
  // See if we exceed the maximum size.
  if (std::max(old_size, requested_size) > get_max_size())
    return Result_t(true, 0);
  // See if we already have enough size.
  if (requested_size <= old_size) return Result_t(false, old_size);
  // Compute new size
  Size_t new_size = requested_size;
  // Grow by at least the grow factor.
  new_size =
      std::max(new_size, mysql::binlog::event::math::multiply_bounded(
                             old_size, get_grow_factor(), machine_max_size));
  // Grow by at least the grow increment.
  new_size =
      std::max(new_size, mysql::binlog::event::math::add_bounded(
                             old_size, get_grow_increment(), machine_max_size));
  // Round up to nearest multiple of block size.
  auto remainder = new_size % get_block_size();
  if (remainder != 0)
    new_size = mysql::binlog::event::math::add_bounded(
        new_size, get_block_size() - remainder, machine_max_size);
  // Limit by max size.
  new_size = std::min(new_size, get_max_size());

  assert(new_size >= requested_size);
  BAPI_LOG("info", BAPI_VAR(old_size) << " " << BAPI_VAR(requested_size) << " "
                                      << BAPI_VAR(new_size));
  return Result_t(false, new_size);
}

}  // namespace mysql::binlog::event::compression::buffer
