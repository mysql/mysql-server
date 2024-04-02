/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_PLUGIN_STATE_OBSERVER_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_PLUGIN_STATE_OBSERVER_H_

#include "mysql/harness/plugin_state_observer.h"

class MockPluginStateObserver : public mysql_harness::PluginStateObserver {
  using PluginState = mysql_harness::PluginState;
  MOCK_METHOD(void, on_begin_observation, (const std::vector<std::string> &),
              (override));
  MOCK_METHOD(void, on_end_observation, (), (override));
  MOCK_METHOD(void, on_plugin_register_waitable,
              (const PluginState *, const std::string &), (override));
  MOCK_METHOD(void, on_plugin_startup,
              (const PluginState *, const std::string &), (override));
  MOCK_METHOD(void, on_plugin_shutdown,
              (const PluginState *, const std::string &), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_PLUGIN_STATE_OBSERVER_H_
