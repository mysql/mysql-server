/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_HANDLER_EXPIRED_PASSWORDS_H_
#define PLUGIN_X_SRC_CAPABILITIES_HANDLER_EXPIRED_PASSWORDS_H_

#include <string>

#include "plugin/x/src/capabilities/handler.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/ngs/mysqlx/setter_any.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

class Cap_handles_expired_passwords : public Capability_handler {
 public:
  explicit Cap_handles_expired_passwords(iface::Client *client)
      : m_client(client) {
    m_value = m_client->supports_expired_passwords();
  }

 private:
  std::string name() const override { return "client.pwd_expire_ok"; }

  bool is_supported_impl() const override { return true; }
  bool is_settable() const override { return true; }
  bool is_gettable() const override { return true; }

  void get_impl(::Mysqlx::Datatypes::Any *any) override {
    ngs::Setter_any::set_scalar(any, m_value);
  }

  ngs::Error_code set_impl(const ::Mysqlx::Datatypes::Any &any) override {
    try {
      m_value = ngs::Getter_any::get_numeric_value<bool>(any);
    } catch (const ngs::Error_code &DEBUG_VAR(error)) {
      log_debug("Capability expired password failed with error: %s",
                error.message.c_str());
      return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                        "Capability prepare failed for '%s'", name().c_str());
    }
    return {};
  }

  void commit() override { m_client->set_supports_expired_passwords(m_value); }

 private:
  iface::Client *m_client;
  bool m_value;
};

}  // namespace xpl
#endif  // PLUGIN_X_SRC_CAPABILITIES_HANDLER_EXPIRED_PASSWORDS_H_
