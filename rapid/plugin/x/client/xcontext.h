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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef X_CLIENT_XCONTEXT_H_
#define X_CLIENT_XCONTEXT_H_

#include <cstring>
#include <string>

#include "mysqlxclient/xerror.h"
#include "mysqlxclient/xprotocol.h"
#include "xconnection_config.h"
#include "xssl_config.h"


namespace xcl {

class Context {
 public:
  Ssl_config            m_ssl_config;
  Connection_config     m_connection_config;
  bool                  m_consume_all_notices { true };
  XProtocol::Client_id  m_client_id         { XCL_CLIENT_ID_NOT_VALID };
  XError                m_global_error;
};

}  // namespace xcl

#endif  // X_CLIENT_XCONTEXT_H_
