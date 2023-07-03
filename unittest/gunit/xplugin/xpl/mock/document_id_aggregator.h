/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_DOCUMENT_ID_AGGREGATOR_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_DOCUMENT_ID_AGGREGATOR_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/document_id_aggregator.h"

namespace xpl {
namespace test {
namespace mock {

class Document_id_aggregator : public iface::Document_id_aggregator {
 public:
  Document_id_aggregator();
  virtual ~Document_id_aggregator() override;

  MOCK_METHOD(std::string, generate_id, (), (override));
  MOCK_METHOD(std::string, generate_id, (const Variables &), (override));
  MOCK_METHOD(void, clear_ids, (), (override));
  MOCK_METHOD(const Document_id_list &, get_ids, (), (const, override));
  MOCK_METHOD(ngs::Error_code, configue, (iface::Sql_session *), (override));
  MOCK_METHOD(void, set_id_retention, (const bool), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_DOCUMENT_ID_AGGREGATOR_H_
