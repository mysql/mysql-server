/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_NOTICE_CONFIGURATION_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_NOTICE_CONFIGURATION_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/notice_descriptor.h"

namespace ngs {

class Notice_configuration_interface {
 public:
  virtual ~Notice_configuration_interface() = default;

  virtual bool get_notice_type_by_name(const std::string &name,
                                       Notice_type *out_notice_type) const = 0;
  virtual bool get_name_by_notice_type(const Notice_type notice_type,
                                       std::string *out_name) const = 0;
  virtual bool is_notice_enabled(const Notice_type notice_type) const = 0;
  virtual void set_notice(const Notice_type notice_type,
                          const bool should_be_enabled) = 0;
  virtual bool is_any_dispatchable_notice_enabled() const = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_NOTICE_CONFIGURATION_INTERFACE_H_
