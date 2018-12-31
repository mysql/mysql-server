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

#include "plugin/x/src/capabilities/handler_client_interactive.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"

#include "plugin/x/src/xpl_log.h"

namespace xpl {

Capability_client_interactive::Capability_client_interactive(
    ngs::Client_interface &client)
    : m_client(client) {
  m_value = m_client.is_interactive();
}

void Capability_client_interactive::get_impl(::Mysqlx::Datatypes::Any &any) {
  ngs::Setter_any::set_scalar(any, m_value);
}

ngs::Error_code Capability_client_interactive::set_impl(
    const ::Mysqlx::Datatypes::Any &any) {
  try {
    m_value = ngs::Getter_any::get_numeric_value<bool>(any);
  } catch (const ngs::Error_code &error) {
    log_debug("Capability client interactive failed with error: %s",
              error.message.c_str());
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "Capability prepare failed for '%s'", name().c_str());
  }
  return {};
}

void Capability_client_interactive::commit() {
  m_client.set_is_interactive(m_value);
}

}  // namespace xpl
