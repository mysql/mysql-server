/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_XPROTOCOL_FACTORY_H_
#define PLUGIN_X_CLIENT_XPROTOCOL_FACTORY_H_

#include <memory>

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/mysqlxclient/xconnection.h"
#include "plugin/x/client/mysqlxclient/xprotocol.h"
#include "plugin/x/client/xquery_instances.h"

namespace xcl {

class Protocol_factory {
 public:
  virtual ~Protocol_factory() = default;

  virtual std::shared_ptr<XProtocol> create_protocol(
      std::shared_ptr<Context> context) = 0;

  virtual std::unique_ptr<XConnection> create_connection(
      std::shared_ptr<Context> context) = 0;

  virtual std::unique_ptr<XQuery_result> create_result(
      std::shared_ptr<XProtocol> protocol, Query_instances *query_instances,
      std::shared_ptr<Context> context) = 0;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XPROTOCOL_FACTORY_H_
