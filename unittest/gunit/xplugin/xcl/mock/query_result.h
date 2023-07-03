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

#ifndef UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_QUERY_RESULT_H_
#define UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_QUERY_RESULT_H_

#include <gmock/gmock.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/client/mysqlxclient/xquery_result.h"

namespace xcl {
namespace test {
namespace mock {

class XQuery_result : public xcl::XQuery_result {
 public:
  XQuery_result();
  virtual ~XQuery_result() override;

  MOCK_METHOD(const Metadata &, get_metadata, (XError * out_error), (override));
  MOCK_METHOD(void, set_metadata, (const Metadata &), (override));
  MOCK_METHOD(const Warnings &, get_warnings, (), (override));
  MOCK_METHOD(bool, get_next_row, (const XRow **out_row, XError *out_error),
              (override));
  MOCK_METHOD(const XRow *, get_next_row, (XError *), (override));
  MOCK_METHOD(XQuery_result::Row *, get_next_row_raw_raw, (XError *));
  MOCK_METHOD(bool, next_resultset, (XError *), (override));
  MOCK_METHOD(bool, try_get_last_insert_id, (uint64_t *), (const, override));
  MOCK_METHOD(bool, try_get_affected_rows, (uint64_t *), (const, override));
  MOCK_METHOD(bool, try_get_info_message, (std::string *), (const, override));
  MOCK_METHOD(bool, try_get_generated_document_ids,
              (std::vector<std::string> *), (const, override));
  MOCK_METHOD(bool, has_resultset, (XError *), (override));
  MOCK_METHOD(bool, is_out_parameter_resultset, (), (const, override));

 private:
  std::unique_ptr<XQuery_result::Row> get_next_row_raw(
      XError *out_error) override {
    return std::unique_ptr<XQuery_result::Row>(get_next_row_raw_raw(out_error));
  }
};

}  // namespace mock
}  // namespace test
}  // namespace xcl

#endif  // UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_QUERY_RESULT_H_
