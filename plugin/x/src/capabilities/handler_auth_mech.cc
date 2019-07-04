/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/capabilities/handler_auth_mech.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"

namespace xpl {

bool Capability_auth_mech::is_supported_impl() const { return true; }

void Capability_auth_mech::get_impl(::Mysqlx::Datatypes::Any &any) {
  std::vector<std::string> auth_mechs;

  m_client.server().get_authentication_mechanisms(auth_mechs, m_client);

  ngs::Setter_any::set_array(any, auth_mechs);
}

ngs::Error_code Capability_auth_mech::set_impl(
    const ::Mysqlx::Datatypes::Any &) {
  return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                    "CapabilitiesSet not supported for the %s capability",
                    name().c_str());
}

void Capability_auth_mech::commit() {}

}  // namespace xpl
