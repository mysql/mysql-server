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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DB_OBJECT_SP_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DB_OBJECT_SP_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "helper/http/url.h"
#include "mrs/database/mysql_task_monitor.h"
#include "mrs/endpoint/handler/handler_db_object_table.h"

namespace mrs {
namespace endpoint {
namespace handler {

class HandlerDbObjectSP : public HandlerDbObjectTable {
 public:
  HandlerDbObjectSP(std::weak_ptr<DbObjectEndpoint> endpoint,
                    mrs::interface::AuthorizeManager *auth_manager,
                    mrs::GtidManager *gtid_manager = nullptr,
                    collector::MysqlCacheManager *cache = nullptr,
                    mrs::ResponseCache *response_cache = nullptr,
                    mrs::database::SlowQueryMonitor *slow_monitor = nullptr,
                    mrs::database::MysqlTaskMonitor *task_monitor = nullptr);

  HttpResult handle_get(rest::RequestContext *ctxt) override;
  HttpResult handle_delete(rest::RequestContext *ctxt) override;
  HttpResult handle_put(rest::RequestContext *ctxt) override;
  HttpResult handle_post(rest::RequestContext *ctxt,
                         const std::vector<uint8_t> &document) override;

  uint32_t get_access_rights() const override;

 private:
  bool always_nest_result_sets_{false};
  mrs::database::MysqlTaskMonitor *task_monitor_{nullptr};

  HttpResult call(rest::RequestContext *ctxt, rapidjson::Document doc);
  HttpResult call(rest::RequestContext *ctxt,
                  const helper::http::Url::Parameters &query_kv);

  HttpResult call_async(rest::RequestContext *ctxt, rapidjson::Document doc);
};

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DB_OBJECT_SP_H_
