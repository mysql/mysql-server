/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs_common/connection_vio.h"
#include "ngs/capabilities/handler_tls.h"
#include "ngs/interface/client_interface.h"

#include "ngs/mysqlx/getter_any.h"
#include "ngs/mysqlx/setter_any.h"

namespace ngs
{
using ::Mysqlx::Datatypes::Any;
using ::Mysqlx::Datatypes::Scalar;
using ::Mysqlx::Datatypes::Any;

bool Capability_tls::is_supported() const
{
  const Connection_type type = m_client.connection().connection_type();
  const bool is_supported_connection_type = Connection_tcpip == type ||
      Connection_tls == type;

  return m_client.connection().options()->supports_tls() && is_supported_connection_type;
}


void Capability_tls::get(Any &any)
{
  bool is_tls_active = m_client.connection().options()->active_tls();

  Setter_any::set_scalar(any, is_tls_active);
}


bool Capability_tls::set(const Any &any)
{
  bool is_tls_active = m_client.connection().options()->active_tls();

  tls_should_be_enabled = Getter_any::get_numeric_value_or_default<int>(any, false) &&
                          ! is_tls_active &&
                          is_supported();

  // Should fail if we try to turn it off
  // or if already activated
  return tls_should_be_enabled;
}


void Capability_tls::commit()
{
  if (tls_should_be_enabled)
  {
    m_client.activate_tls();
  }
}


} // namespace ngs
