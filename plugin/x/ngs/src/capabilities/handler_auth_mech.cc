/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/capabilities/handler_auth_mech.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"
#include "plugin/x/ngs/include/ngs_common/connection_vio.h"


namespace ngs
{

bool Capability_auth_mech::is_supported() const
{
  return true;
}

void Capability_auth_mech::get(::Mysqlx::Datatypes::Any &any)
{
  std::vector<std::string> auth_mechs;

  m_client.server().get_authentication_mechanisms(auth_mechs, m_client);

  Setter_any::set_array(any, auth_mechs);
}

bool Capability_auth_mech::set(const ::Mysqlx::Datatypes::Any&)
{
  return false;
}

void Capability_auth_mech::commit()
{
}

} // namespace ngs
