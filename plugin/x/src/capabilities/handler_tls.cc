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

#include "plugin/x/src/capabilities/handler_tls.h"

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/ngs/mysqlx/setter_any.h"

namespace xpl {

using ::Mysqlx::Datatypes::Any;
using ::Mysqlx::Datatypes::Scalar;

bool Capability_tls::is_supported_impl() const {
  const Connection_type type = m_client->connection().get_type();
  const bool is_supported_connection_type = Connection_tcpip == type ||
                                            Connection_tls == type ||
                                            Connection_unixsocket == type;

  return m_client->server().ssl_context()->has_ssl() &&
         is_supported_connection_type;
}

void Capability_tls::get_impl(Any *any) {
  bool is_tls_active = m_client->connection().get_type() == Connection_tls;

  ngs::Setter_any::set_scalar(any, is_tls_active);
}

ngs::Error_code Capability_tls::set_impl(const Any &any) {
  bool is_tls_active = m_client->connection().get_type() == Connection_tls;

  tls_should_be_enabled =
      ngs::Getter_any::get_numeric_value_or_default<int>(any, false) &&
      !is_tls_active && is_supported();

  // Should fail if we try to turn it off
  // or if already activated
  if (tls_should_be_enabled) return {};
  return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                    "Capability prepare failed for '%s'", name().c_str());
}

void Capability_tls::commit() {
  if (tls_should_be_enabled) {
    m_client->activate_tls();
  }
}

}  // namespace xpl
