/*
  Copyright (c) 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with the
  program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysqlrouter/rest_sync_point_export.h"

#include <array>

#include "dim.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/rest_api_component.h"
#include "mysqlrouter/routing_component.h"

#include "rest_sync_point_handler.h"

static void init(mysql_harness::PluginFuncEnv *) {
  mysql_harness::DIM::instance().set_static_SyncPointRegistry(
      sync_points().get());
}

static void run(mysql_harness::PluginFuncEnv *env) {
  auto &rest_api = RestApiComponent::get_instance();
  std::array<RestApiComponentPath, 2> paths{{
      {rest_api, RestSyncPointStatus::path_regex,
       std::make_unique<RestSyncPointStatus>()},
      {rest_api, RestSyncPointRelease::path_regex,
       std::make_unique<RestSyncPointRelease>()},
  }};

  mysql_harness::on_service_ready(env);
  mysql_harness::wait_for_stop(env, 0);
}

static const std::array<const char *, 2> plugin_requires = {"logger",
                                                            "rest_api"};

extern "C" {
mysql_harness::Plugin REST_SYNC_POINT_EXPORT harness_plugin_rest_sync_point = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "REST_SYNC_POINT",
    VERSION_NUMBER(0, 0, 1),
    plugin_requires.size(),
    plugin_requires.data(),
    0,
    nullptr,
    init,
    nullptr,
    run,
    nullptr,
    true,
    0,
    nullptr,
    nullptr,
};
}
