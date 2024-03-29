/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_HANDLER_H_
#define PLUGIN_X_SRC_CAPABILITIES_HANDLER_H_

#include <memory>

#include "plugin/x/src/interface/capability_handler.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

class Capability_handler : public iface::Capability_handler {
 public:
  bool is_supported() const override {
    return (is_gettable() || is_settable()) && is_supported_impl();
  }

  void get(::Mysqlx::Datatypes::Any *any) override {
    if (is_gettable()) get_impl(any);
  }

  ngs::Error_code set(const ::Mysqlx::Datatypes::Any &any) override {
    if (is_settable()) return set_impl(any);
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "CapabilitiesSet not supported for the %s capability",
                      name().c_str());
  }

 protected:
  virtual void get_impl(::Mysqlx::Datatypes::Any *any) = 0;
  virtual ngs::Error_code set_impl(const ::Mysqlx::Datatypes::Any &any) = 0;
  virtual bool is_supported_impl() const = 0;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_HANDLER_H_
