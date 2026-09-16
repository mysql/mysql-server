/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
#include <optional>
#include "srv0mon.h" /*mon_type_t, mon_id_t*/
namespace ib {

/** Interface for repoting values of some INFORMATION_SCHEMA.INNODB_METRICS */
class Monitoring_interface {
 public:
  /** Default destructor */
  virtual ~Monitoring_interface() = default;

  /** Returns the value of given Innodb-specific monitor by id or std::nullopt
  if not supported.
  @param[in]     id   The id of the monitor
  @return The value for that monitor, or std::nullopt if not supported by
  implementation.
  */
  [[nodiscard]] virtual std::optional<mon_type_t> get_value(
      monitor_id_t id) = 0;
};

}  // namespace ib
