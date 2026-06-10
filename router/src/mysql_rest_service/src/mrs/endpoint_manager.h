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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_MANAGER_H_

#include <compare>
#include <map>
#include <memory>
#include <vector>

#include "collector/mysql_cache_manager.h"
#include "mrs/configuration.h"
#include "mrs/database/entry/content_file.h"
#include "mrs/database/entry/content_set.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/database/entry/db_schema.h"
#include "mrs/database/entry/db_service.h"
#include "mrs/database/mysql_task_monitor.h"
#include "mrs/database/slow_query_monitor.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/endpoint/endpoint_factory.h"
#include "mrs/endpoint/url_host_endpoint.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/endpoint_base.h"
#include "mrs/interface/endpoint_manager.h"
#include "mrs/rest/response_cache.h"
#include "my_compiler.h"

namespace mrs {

class EndpointManager : public mrs::interface::EndpointManager {
 public:
  using EntryKey = database::entry::EntryKey;
  using EntryType = database::entry::EntryType;
  using UniversalId = database::entry::UniversalId;
  using EndpointBase = mrs::interface::EndpointBase;
  using EndpointBasePtr = std::shared_ptr<EndpointBase>;
  using EndpointFactory = mrs::endpoint::EndpointFactory;
  using EndpointFactoryPtr = std::shared_ptr<EndpointFactory>;
  using ResponseCache = mrs::ResponseCache;
  using SlowQueryMonitor = mrs::database::SlowQueryMonitor;
  using MysqlTaskMonitor = mrs::database::MysqlTaskMonitor;

  class EndpointId {
   public:
    enum IdType {
      IdNone,
      IdUrlHost,
      IdService,
      IdSchema,
      IdContentSet,
      IdContentFile,
      IdObject
    };

   public:
    EndpointId() : type{IdNone} {}

    template <IdType p_type>
    EndpointId(const UniversalId &p_id) : type{p_type}, id{p_id} {}

    EndpointId(const IdType p_type, const UniversalId &p_id)
        : type{p_type}, id{p_id} {}

    MY_COMPILER_DIAGNOSTIC_PUSH()
    MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wzero-as-null-pointer-constant")
    MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wzero-as-null-pointer-constant")
    std::strong_ordering operator<=>(const EndpointId &rhs) const {
      const auto cmp_res = type <=> rhs.type;
      if (cmp_res != 0) return cmp_res;

      return id <=> rhs.id;
    }
    MY_COMPILER_DIAGNOSTIC_POP()

   public:
    IdType type;
    UniversalId id;
  };

 public:
  EndpointManager(const std::shared_ptr<mrs::interface::EndpointConfiguration>
                      &configuration,
                  collector::MysqlCacheManager *cache,
                  mrs::interface::AuthorizeManager *auth_manager,
                  mrs::GtidManager *gtid_manager,
                  EndpointFactoryPtr endpoint_factory = {},
                  ResponseCache *response_cache = {},
                  ResponseCache *file_cache = {},
                  SlowQueryMonitor *slow_query_monitor = {},
                  MysqlTaskMonitor *task_monitor = {});

  void configure(const std::optional<std::string> &options) override;

  void update(const std::vector<DbService> &services) override;
  void update(const std::vector<DbSchema> &schemas) override;
  void update(const std::vector<DbObject> &objs) override;
  void update(const std::vector<UrlHost> &hosts) override;
  void update(const std::vector<ContentSet> &set) override;
  void update(const std::vector<ContentFile> &files) override;

  void clear() override;

 private:
  // Keep shared ownership of Hosts.
  std::map<UniversalId, EndpointBasePtr> hold_host_endpoints_;

  // Lets keep all objects in weak ptrs:
  std::map<EndpointId, std::weak_ptr<EndpointBase>> endpoints_;

  // Essential objects
  collector::MysqlCacheManager *cache_;
  mrs::interface::AuthorizeManager *auth_manager_;
  mrs::GtidManager *gtid_manager_;
  std::vector<std::shared_ptr<interface::RestHandler>> custom_paths_;
  std::shared_ptr<EndpointFactory> endpoint_factory_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_MANAGER_H_
