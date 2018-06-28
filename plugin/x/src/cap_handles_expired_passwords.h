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

#ifndef PLUGIN_X_SRC_CAP_HANDLES_EXPIRED_PASSWORDS_H_
#define PLUGIN_X_SRC_CAP_HANDLES_EXPIRED_PASSWORDS_H_

#include "plugin/x/ngs/include/ngs/capabilities/handler.h"
#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

class Cap_handles_expired_passwords : public ngs::Capability_handler {
 public:
  Cap_handles_expired_passwords(xpl::Client &client) : m_client(client) {
    m_value = m_client.supports_expired_passwords();
  }

 private:
  virtual const std::string name() const { return "client.pwd_expire_ok"; }

  virtual bool is_supported() const { return true; }

  virtual void get(::Mysqlx::Datatypes::Any &any) {
    ngs::Setter_any::set_scalar(any, m_value);
  }

  virtual bool set(const ::Mysqlx::Datatypes::Any &any) {
    try {
      m_value = ngs::Getter_any::get_numeric_value<bool>(any);
    } catch (const ngs::Error_code &error) {
      log_error(ER_XPLUGIN_CAPABILITY_EXPIRED_PASSWORD, error.message.c_str());
      return false;
    }
    return true;
  }

  virtual void commit() { m_client.set_supports_expired_passwords(m_value); }

 private:
  xpl::Client &m_client;
  bool m_value;
};

}  // namespace xpl
#endif  // PLUGIN_X_SRC_CAP_HANDLES_EXPIRED_PASSWORDS_H_
