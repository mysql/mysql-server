/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_REST_HANDLER_SCHEMA_METADATA_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_REST_HANDLER_SCHEMA_METADATA_H_

#include <string>

#include "mrs/interface/route_schema.h"
#include "mrs/rest/handler.h"

namespace mrs {
namespace rest {

class HandlerSchemaMetadata : public Handler {
 public:
  using RouteSchema = mrs::interface::RouteSchema;

 public:
  HandlerSchemaMetadata(RouteSchema *schema,
                        mrs::interface::AuthManager *auth_manager);

  Authorization requires_authentication() const override;
  std::pair<IdType, uint64_t> get_id() const override;
  uint64_t get_db_object_id() const override;
  uint64_t get_schema_id() const override;
  uint32_t get_access_rights() const override;
  void authorization(rest::RequestContext *ctxt) override;

  Result handle_get(rest::RequestContext *ctxt) override;
  Result handle_post(rest::RequestContext *ctxt,
                     const std::vector<uint8_t> &document) override;
  Result handle_delete(rest::RequestContext *ctxt) override;
  Result handle_put(rest::RequestContext *ctxt) override;

 private:
  RouteSchema *schema_;
};

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_REST_HANDLER_SCHEMA_METADATA_H_
