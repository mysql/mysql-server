/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_HANDLER_CLIENT_INTERACTIVE_H_
#define PLUGIN_X_SRC_CAPABILITIES_HANDLER_CLIENT_INTERACTIVE_H_

#include <string>

#include "plugin/x/src/capabilities/handler.h"
#include "plugin/x/src/client.h"

namespace xpl {

class Capability_client_interactive : public Capability_handler {
 public:
  explicit Capability_client_interactive(iface::Client *client);

  std::string name() const override { return "client.interactive"; }
  bool is_settable() const override { return true; }
  bool is_gettable() const override { return true; }

  void commit() override;

 private:
  void get_impl(::Mysqlx::Datatypes::Any *any) override;
  ngs::Error_code set_impl(const ::Mysqlx::Datatypes::Any &any) override;
  bool is_supported_impl() const override { return true; }

  iface::Client *m_client;
  bool m_value;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_HANDLER_CLIENT_INTERACTIVE_H_
