/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_MONITOR_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_MONITOR_H_

#include <thread>
#include <vector>

#include "mysql/harness/stdx/monitor.h"

#include "collector/mysql_cache_manager.h"
#include "helper/wait_variable.h"
#include "mrs/authentication/authorize_manager.h"
#include "mrs/configuration.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/database/monitor/schema_monitor_factory.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/schema_monitor_factory.h"
#include "mrs/object_manager.h"
#include "mrs/observability/entities_manager.h"

namespace mrs {
namespace database {

class SchemaMonitor {
 public:
  SchemaMonitor(
      const mrs::Configuration &configuration,
      collector::MysqlCacheManager *cache, mrs::ObjectManager *dbobject_manager,
      authentication::AuthorizeManager *auth_manager,
      mrs::observability::EntitiesManager *entities_manager,
      mrs::GtidManager *gtid_manager,
      SchemaMonitorFactoryMethod method = &create_schema_monitor_factory);
  ~SchemaMonitor();

  void start();
  void stop();

 private:
  void run();
  bool wait_until_next_refresh();

  class Waitable : public WaitableMonitor<void *> {
   public:
    using Parent = WaitableMonitor<void *>;
    using Parent::WaitableMonitor;
  };

  enum State { k_initializing, k_running, k_stopped };

  std::thread monitor_thread_;
  const mrs::Configuration configuration_;
  collector::MysqlCacheManager *cache_;
  mrs::ObjectManager *dbobject_manager_;
  mrs::authentication::AuthorizeManager *auth_manager_;
  mrs::observability::EntitiesManager *entities_manager_;
  mrs::GtidManager *gtid_manager_;
  WaitableVariable<State> state_{k_initializing};
  Waitable waitable_{this};
  SchemaMonitorFactoryMethod schema_monitor_factory_method_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_MONITOR_H_
