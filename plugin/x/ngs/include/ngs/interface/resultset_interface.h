/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_RESULTSET_INTERFACE_H_
#define NGS_RESULTSET_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/command_delegate.h"

namespace ngs {

class Resultset_interface {
 public:
  typedef Command_delegate::Info Info;
  virtual ~Resultset_interface() {}
  virtual Command_delegate &get_callbacks() = 0;
  virtual const Info &get_info() const = 0;
};

}  // namespace ngs

#endif  // NGS_RESULTSET_INTERFACE_H_
