/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

#include "unittest/gunit/xplugin/xcl/mock/connection.h"
#include "unittest/gunit/xplugin/xcl/mock/connection_state.h"
#include "unittest/gunit/xplugin/xcl/mock/factory.h"
#include "unittest/gunit/xplugin/xcl/mock/protocol.h"
#include "unittest/gunit/xplugin/xcl/mock/query_instance.h"
#include "unittest/gunit/xplugin/xcl/mock/query_result.h"

namespace xcl {
namespace test {
namespace mock {

XConnection_state::XConnection_state() {}
XConnection_state::~XConnection_state() {}

XConnection::XConnection() {}
XConnection::~XConnection() {}

Protocol_factory::Protocol_factory() {}
Protocol_factory::~Protocol_factory() {}

XProtocol::XProtocol() {}
XProtocol::~XProtocol() {}

Query_instances::Query_instances() {}
Query_instances::~Query_instances() {}

XQuery_result::XQuery_result() {}
XQuery_result::~XQuery_result() {}

}  // namespace mock
}  // namespace test
}  // namespace xcl
