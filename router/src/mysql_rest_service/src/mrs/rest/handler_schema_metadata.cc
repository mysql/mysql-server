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

#include "mrs/rest/handler_schema_metadata.h"

#include "mysql/harness/logging/logging.h"

#include "helper/http/url.h"
#include "mrs/http/error.h"
#include "mrs/interface/object.h"
#include "mrs/json/response_json_template.h"
#include "mrs/rest/request_context.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using HttpResult = Handler::HttpResult;
using Route = mrs::interface::Object;
using Url = helper::http::Url;

HandlerSchemaMetadata::HandlerSchemaMetadata(
    RouteSchema *schema, mrs::interface::AuthorizeManager *auth_manager)
    : Handler(schema->get_url(), {schema->get_path()}, schema->get_options(),
              auth_manager),
      schema_{schema} {}

void HandlerSchemaMetadata::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerSchemaMetadata::handle_get(rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  log_debug("Schema::handle_get '%s'", requests_uri.get_path().c_str());
  const uint32_t k_default_limit = 25;
  uint32_t offset = 0;
  uint32_t limit = k_default_limit;
  json::ResponseJsonTemplate response_template{false};

  Url::parse_offset_limit(requests_uri.get_query_elements(), &offset, &limit);

  response_template.begin_resultset(offset, limit, limit == k_default_limit,
                                    schema_->get_url(), {});

  auto &routes = schema_->get_routes();
  uint32_t noOfRoute = 0;
  for (auto it = routes.begin() + offset;
       it < routes.end() && noOfRoute < limit; ++noOfRoute, ++it) {
    //    bool mayShow!(*it)->requires_authentication()
    //    ctxt->user.has_user_id
    response_template.push_json_document((*it)->get_json_description().c_str());
  }

  response_template.end_resultset();

  return response_template.get_result();
}

HttpResult HandlerSchemaMetadata::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerSchemaMetadata::handle_delete(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerSchemaMetadata::handle_put(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Handler::Authorization HandlerSchemaMetadata::requires_authentication() const {
  return schema_->requires_authentication() ? Authorization::kCheck
                                            : Authorization::kNotNeeded;
}

UniversalId HandlerSchemaMetadata::get_service_id() const {
  return schema_->get_service_id();
}

UniversalId HandlerSchemaMetadata::get_db_object_id() const {
  // TODO(lkotula): id of file is not the same as db_object.
  // It should be considered to return (Shouldn't be in
  // review)
  return {};
}

UniversalId HandlerSchemaMetadata::get_schema_id() const {
  return schema_->get_id();
}

uint32_t HandlerSchemaMetadata::get_access_rights() const {
  return Route::kRead;
}

}  // namespace rest
}  // namespace mrs
