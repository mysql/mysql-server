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

#ifndef XPLUGIN_XCL_MOCK_CONNECTION_STATE_H_
#define XPLUGIN_XCL_MOCK_CONNECTION_STATE_H_

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>

#include "plugin/x/client/mysqlxclient/xconnection.h"


namespace xcl {
namespace test {

class Mock_connection_state : public XConnection::State {
 public:
  MOCK_CONST_METHOD0(is_ssl_configured,
      bool());
  MOCK_CONST_METHOD0(is_ssl_activated,
      bool());
  MOCK_CONST_METHOD0(is_connected,
      bool());
  MOCK_CONST_METHOD0(get_ssl_version,
      std::string());
  MOCK_CONST_METHOD0(get_ssl_cipher,
      std::string());
  MOCK_CONST_METHOD0(get_connection_type, Connection_type());
};

}  // namespace test
}  // namespace xcl

#endif  // XPLUGIN_XCL_MOCK_CONNECTION_STATE_H_
