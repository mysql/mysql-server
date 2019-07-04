/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_RESULTSET_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_RESULTSET_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/command_delegate.h"

namespace ngs {

class Resultset_interface {
 public:
  Resultset_interface() = default;
  Resultset_interface(const Resultset_interface &) = default;
  Resultset_interface(Resultset_interface &&) = default;
  Resultset_interface &operator=(const Resultset_interface &) = default;
  Resultset_interface &operator=(Resultset_interface &&) = default;

  typedef Command_delegate::Info Info;
  virtual ~Resultset_interface() {}
  virtual Command_delegate &get_callbacks() = 0;
  virtual const Info &get_info() const = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_RESULTSET_INTERFACE_H_
