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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_SP_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_SP_H_

#include "mrs/interface/object.h"
#include "mrs/rest/handler.h"

namespace mrs {
namespace rest {

class HandlerSP : public rest::Handler {
 public:
  using Route = mrs::interface::Object;

 public:
  HandlerSP(Route *r, mrs::interface::AuthorizeManager *auth_manager);

  HttpResult handle_get(rest::RequestContext *ctxt) override;
  HttpResult handle_delete(rest::RequestContext *ctxt) override;
  HttpResult handle_put(rest::RequestContext *ctxt) override;
  HttpResult handle_post(rest::RequestContext *ctxt,
                         const std::vector<uint8_t> &document) override;

  void authorization(rest::RequestContext *ctxt) override;
  Authorization requires_authentication() const override;
  UniversalId get_service_id() const override;
  UniversalId get_db_object_id() const override;
  UniversalId get_schema_id() const override;

  uint32_t get_access_rights() const override;

 private:
  bool always_nest_result_sets_{false};
  Route *route_;
  mrs::interface::AuthorizeManager *auth_manager_;
};

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_SP_H_
