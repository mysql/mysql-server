/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include <vector>

#include "collector/mysql_cache_manager.h"
#include "mrs/authentication/authorize_manager.h"
#include "mrs/configuration.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/database/metadata_logger.h"
#include "mrs/database/monitor/schema_monitor_factory.h"
#include "mrs/database/query_factory_proxy.h"
#include "mrs/database/slow_query_monitor.h"
#include "mrs/endpoint_manager.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/query_monitor_factory.h"
#include "mrs/observability/entities_manager.h"
#include "mrs/rest/response_cache.h"

#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/utility/wait_variable.h"

namespace mrs {
namespace database {

class SchemaMonitor {
 public:
  SchemaMonitor(const mrs::Configuration &configuration,
                collector::MysqlCacheManager *cache,
                mrs::EndpointManager *dbobject_manager,
                authentication::AuthorizeManager *auth_manager,
                mrs::observability::EntitiesManager *entities_manager,
                mrs::GtidManager *gtid_manager,
                mrs::database::QueryFactoryProxy *query_factory,
                mrs::ResponseCache *response_cache,
                mrs::ResponseCache *file_cache,
                SlowQueryMonitor *slow_query_monitor,
                MetadataLogger *metadata_logger);
  ~SchemaMonitor();

  void start();
  void stop();
  void reset();

 private:
  template <typename T>
  using WaitableVariable = mysql_harness::utility::WaitableVariable<T>;

  class MetadataSourceDestination {
   public:
    MetadataSourceDestination(collector::MysqlCacheManager *cache,
                              const bool is_dynamic)
        : cache_{cache}, is_dynamic_{is_dynamic} {}

    std::optional<collector::MysqlCacheManager::CachedObject> get_rw_session();
    bool handle_error();

   private:
    enum DestinationState { k_ok, k_read_only, k_offline };

    DestinationState current_destination_state_{
        DestinationState::k_read_only};  // initialize with read-only to force
                                         // check on the first run
    DestinationState previous_destination_state_{DestinationState::k_ok};

    collector::MysqlCacheManager *cache_;
    const bool is_dynamic_;
  };

  void run();
  bool wait_until_next_refresh();
  std::pair<std::string, std::string> get_router_name_and_address();

  class Waitable : public WaitableMonitor<void *> {
   public:
    using Parent = WaitableMonitor<void *>;
    using Parent::WaitableMonitor;
  };

  enum State { k_initializing, k_running, k_stopped };

  const mrs::Configuration configuration_;
  std::optional<std::string> router_name_;
  collector::MysqlCacheManager *cache_;
  mrs::EndpointManager *dbobject_manager_;
  mrs::authentication::AuthorizeManager *auth_manager_;
  mrs::observability::EntitiesManager *entities_manager_;
  mrs::GtidManager *gtid_manager_;
  WaitableVariable<State> state_{k_initializing};
  Waitable waitable_{this};
  mrs::database::QueryFactoryProxy *proxy_query_factory_;
  mrs::ResponseCache *response_cache_;
  mrs::ResponseCache *file_cache_;
  SlowQueryMonitor *slow_query_monitor_;
  MetadataLogger *metadata_logger_;
  MetadataSourceDestination md_source_destination_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_MONITOR_H_
