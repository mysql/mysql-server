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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_PLUGIN_STATE_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_PLUGIN_STATE_H_

#include <gmock/gmock.h>

#include "mysql/harness/plugin_state.h"

class MockPluginState : public mysql_harness::PluginState {
 public:
  using PluginStateObserver = mysql_harness::PluginStateObserver;
  MockPluginState() = default;

  MOCK_METHOD(Plugins, get_loaded_plugins, (), (const, override));
  MOCK_METHOD(Plugins, get_running_plugins, (), (const, override));
  MOCK_METHOD(std::vector<ObserverId>, push_back_observers,
              (const std::vector<std::weak_ptr<PluginStateObserver>> &),
              (override));
  MOCK_METHOD(void, dispatch_shutdown, (const PluginName &), (override));
  MOCK_METHOD(ObserverId, push_back_observer, (ObserverPtr), (override));
  MOCK_METHOD(void, remove_observers, (const std::vector<ObserverId> &),
              (override));
  MOCK_METHOD(void, dispatch_register_waitable, (const PluginName &),
              (override));
  MOCK_METHOD(void, remove_observer, (ObserverId), (override));
  MOCK_METHOD(void, dispatch_startup, (const PluginName &), (override));
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_PLUGIN_STATE_H_
