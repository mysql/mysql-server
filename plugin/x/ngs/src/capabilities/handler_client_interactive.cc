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

#include "plugin/x/ngs/include/ngs/capabilities/handler_client_interactive.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"

#include "plugin/x/src/xpl_log.h"

namespace ngs {

Capability_client_interactive::Capability_client_interactive(
    Client_interface &client)
  : m_client(client) {
  m_value = m_client.is_interactive();
}

void Capability_client_interactive::get(::Mysqlx::Datatypes::Any &any) {
  ngs::Setter_any::set_scalar(any, m_value);
}

bool Capability_client_interactive::set(const ::Mysqlx::Datatypes::Any &any) {
  try
  {
    m_value = ngs::Getter_any::get_numeric_value<bool>(any);
  }
  catch (const ngs::Error_code &error)
  {
    log_error("Capability client interactive failed with error: %s",
        error.message.c_str());
    return false;
  }
  return true;
}

void Capability_client_interactive::commit() {
  m_client.set_is_interactive(m_value);
}

} // namespace ngs
