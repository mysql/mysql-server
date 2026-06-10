/*
 Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DB_OBJECT_SCRIPT_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DB_OBJECT_SCRIPT_H_

#include "mrs/endpoint/handler/handler_db_object_table.h"

namespace mrs {
namespace endpoint {
namespace handler {

class HandlerDbObjectScript : public HandlerDbObjectTable {
 public:
  HandlerDbObjectScript(std::weak_ptr<DbObjectEndpoint> endpoint,
                        mrs::interface::AuthorizeManager *auth_manager,
                        mrs::GtidManager *gtid_manager = nullptr,
                        collector::MysqlCacheManager *cache = nullptr,
                        mrs::ResponseCache *response_cache = nullptr);

  HttpResult handle_get(rest::RequestContext *ctxt) override;
  HttpResult handle_delete(rest::RequestContext *ctxt) override;
  HttpResult handle_put(rest::RequestContext *ctxt) override;
  HttpResult handle_post(rest::RequestContext *ctxt,
                         const std::vector<uint8_t> &document) override;

  ~HandlerDbObjectScript() override = default;

  uint32_t get_access_rights() const override;

 private:
  HttpResult handle_script(rest::RequestContext *ctxt);
  HttpResult handle_script(rest::RequestContext *ctxt,
                           const std::vector<uint8_t> &document);

  class Impl;
  friend class Impl;
  std::shared_ptr<Impl> m_impl;
  bool always_nest_result_sets_{false};
};

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DB_OBJECT_SCRIPT_H_
