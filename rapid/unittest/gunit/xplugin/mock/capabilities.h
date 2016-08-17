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


#include "ngs/capabilities/configurator.h"
#include "ngs/capabilities/handler.h"

namespace ngs
{

namespace test
{


class Mock_capabilities_configurator : public Capabilities_configurator
{
public:
  Mock_capabilities_configurator() : Capabilities_configurator(std::vector<Capability_handler_ptr>())
  {}

  MOCK_METHOD0(get, ::Mysqlx::Connection::Capabilities *());

  MOCK_METHOD1(prepare_set, ngs::Error_code (const ::Mysqlx::Connection::Capabilities &capabilities));
  MOCK_METHOD0(commit, void ());
};

class Mock_capability_handler: public Capability_handler
{
public:
  MOCK_CONST_METHOD0(name, const std::string ());
  MOCK_CONST_METHOD0(is_supported, bool ());
  MOCK_METHOD1(set, bool (const ::Mysqlx::Datatypes::Any &));

  // Workaround for GMOCK undefined behaviour with ResultHolder
  MOCK_METHOD1(get_void, bool (::Mysqlx::Datatypes::Any &));
  MOCK_METHOD0(commit_void, bool ());

  void get(::Mysqlx::Datatypes::Any &any)
  {
    get_void(any);
  }

  void commit()
  {
    commit_void();
  }

};

} // namespace test

}  // namespace ngs
