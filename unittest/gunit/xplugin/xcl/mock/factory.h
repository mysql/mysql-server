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


#ifndef XPLUGIN_XCL_MOCK_FACTORY_H_
#define XPLUGIN_XCL_MOCK_FACTORY_H_

#include <gmock/gmock.h>
#include <memory>

#include "plugin/x/client/xprotocol_factory.h"


namespace xcl {
namespace test {

class Mock_factory : public Protocol_factory {
 public:
  MOCK_METHOD1(create_protocol_raw,
      XProtocol *(std::shared_ptr<Context> context));
  MOCK_METHOD1(create_connection_raw,
      XConnection *(std::shared_ptr<Context> context));
  MOCK_METHOD3(create_result_raw,
      XQuery_result *(std::shared_ptr<XProtocol>,
                      Query_instances *,
                      std::shared_ptr<Context>));

 private:
    std::shared_ptr<XProtocol> create_protocol(
        std::shared_ptr<Context> context) override {
      std::shared_ptr<XProtocol> result {
        create_protocol_raw(context)
      };

      return result;
    }

    std::unique_ptr<XConnection> create_connection(
        std::shared_ptr<Context> context) override {
      std::unique_ptr<XConnection> result {
        create_connection_raw(context)
      };

      return result;
    }

    std::unique_ptr<XQuery_result> create_result(
        std::shared_ptr<XProtocol> protocol,
        Query_instances *query_instances,
        std::shared_ptr<Context> context) override {
      std::unique_ptr<XQuery_result> result {
        create_result_raw(protocol, query_instances, context)
      };

      return result;
    }
};

}  // namespace test
}  // namespace xcl

#endif  // XPLUGIN_XCL_MOCK_FACTORY_H_
