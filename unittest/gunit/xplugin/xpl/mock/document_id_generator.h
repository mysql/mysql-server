/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_DOCUMENT_ID_GENERATOR_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_DOCUMENT_ID_GENERATOR_H_

#include <gmock/gmock.h>
#include <string>

#include "plugin/x/src/interface/document_id_generator.h"

namespace xpl {
namespace test {
namespace mock {

class Document_id_generator : public iface::Document_id_generator {
 public:
  Document_id_generator();
  virtual ~Document_id_generator() override;

  MOCK_METHOD(std::string, generate,
              (const iface::Document_id_generator::Variables &), (override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_DOCUMENT_ID_GENERATOR_H_
