/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

*****************************************************************************/

#pragma once

#include <string_view>

#include "ha0sys_var_handler_interface.h"

namespace ib::redo {
class Handler;
/** This class implements the handling of redo log related system variables */
class Sys_var_handler final : public Sys_var_handler_interface {
 public:
  Sys_var_handler(Handler &handler) : m_handler{handler} {}
  ~Sys_var_handler() override;

  [[nodiscard]] bool update_var(THD *thd, std::string_view name,
                                uint64_t new_value) override;

  [[nodiscard]] bool update_var(THD *thd, std::string_view name,
                                bool new_value) override;

 private:
  /** Update the innodb_log_buffer_size system variable */
  [[nodiscard]] bool buffer_size_update(uint64_t value);

  /** Update the innodb_log_writer_threads system variable */
  void writer_threads_update(bool value);

  /** Update the innodb_redo_log_capacity system variable */
  [[nodiscard]] bool capacity_update(THD *thd, std::string_view var_name,
                                     uint64_t value);

  [[nodiscard]] bool log_write_ahead_size_update(THD *thd,
                                                 std::string_view var_name,
                                                 uint64_t value);
  Handler &m_handler;
};
}  // namespace ib::redo
