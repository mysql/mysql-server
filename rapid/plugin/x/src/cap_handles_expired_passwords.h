/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _CAP_HANDLES_EXPIRED_PASSWORDS_H_
#define _CAP_HANDLES_EXPIRED_PASSWORDS_H_

#include "ngs/capabilities/handler.h"
#include "ngs/mysqlx/getter_any.h"
#include "ngs/mysqlx/setter_any.h"

#include "xpl_client.h"
#include "xpl_log.h"

namespace xpl
{

class Cap_handles_expired_passwords : public ngs::Capability_handler
{
public:
  Cap_handles_expired_passwords(xpl::Client &client)
  : m_client(client)
  {
    m_value = m_client.supports_expired_passwords();
  }

private:
  virtual const std::string name() const { return "client.pwd_expire_ok"; }

  virtual bool is_supported() const { return true; }

  virtual void get(::Mysqlx::Datatypes::Any &any)
  {
    ngs::Setter_any::set_scalar(any, m_value);
  }

  virtual bool set(const ::Mysqlx::Datatypes::Any &any)
  {
    try
    {
      m_value = ngs::Getter_any::get_numeric_value<bool>(any);
    }
    catch (const ngs::Error_code &error)
    {
      log_error("Capability expired password failed with error: %s", error.message.c_str());
      return false;
    }
    return true;
  }

  virtual void commit()
  {
    m_client.set_supports_expired_passwords(m_value);
  }

private:
  xpl::Client &m_client;
  bool m_value;
};

}
#endif
