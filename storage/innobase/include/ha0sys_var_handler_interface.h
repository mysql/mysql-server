/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once

#include <cstdint>
#include <string_view>

class THD;  // Forward declaration
namespace ib {
/* This interface provides methods to handle the system variables related
configurations. The goal is to enable multiple implementation by various
sub systems within Innodb. A Subsystem may have different implementations
to handle the system variables related to it.*/
class Sys_var_handler_interface {
 public:
  /** Default destructor */
  virtual ~Sys_var_handler_interface() = default;

  /** Update the system variable to the new value.

  @param[in] thd  THD object, may be used to print warning
  @param[in] name The system variable name
  @param[in] new_value  The value to be updated
  @return true  System variable was updated successfully
  @return false Otherwise*/
  [[nodiscard]] virtual bool update_var(THD *thd [[maybe_unused]],
                                        std::string_view name,
                                        uint64_t new_value) = 0;

  /** Update the system variable to the new value.

  @param[in] thd  THD object, may be used to print warning
  @param[in] name The system variable name
  @param[in] new_value  The value to be updated
  @return true  System variable was updated successfully
  @return false Otherwise*/
  [[nodiscard]] virtual bool update_var(THD *thd [[maybe_unused]],
                                        std::string_view name,
                                        bool new_value) = 0;
};
}  // namespace ib
