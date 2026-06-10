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

#include "mrs/endpoint/handler/handler_db_schema_metadata_catalog.h"

#include <ranges>

#include "helper/http/url.h"
#include "helper/json/serializer_to_text.h"
#include "mrs/endpoint/db_object_endpoint.h"
#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/http/error.h"
#include "mrs/json/response_json_template.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = mrs::rest::Handler::HttpResult;
using Url = helper::http::Url;

namespace {

template <typename Vector>
auto subrange(Vector *v, uint64_t offset, uint64_t length,
              bool *has_more = nullptr) {
  typename Vector::iterator b = v->begin();
  typename Vector::iterator e = v->end();

  if (v->size() <= offset) b = v->end();

  if (v->size() > (offset + length)) e = v->begin() + offset + length;

  if (has_more) *has_more = e != v->end();

  return std::ranges::subrange(b, e);
}

std::string generate_json_descriptor(mrs::interface::EndpointBase *endpoint,
                                     const std::string &metadata_catalog) {
  auto db_object = dynamic_cast<mrs::endpoint::DbObjectEndpoint *>(endpoint);
  if (!db_object) return {};

  auto entry = db_object->get();

  helper::json::SerializerToText serializer{false};

  {
    auto root = serializer.add_object();
    root->member_add_value("name", entry->request_path);

    auto links = root->member_add_array("links");

    {
      auto array_first = links->add_object();
      array_first->member_add_value("rel", "describes");
      array_first->member_add_value("href", db_object->get_url_path());
    }

    {
      auto array_first = links->add_object();
      array_first->member_add_value("rel", "canonical");
      array_first->member_add_value("href",
                                    metadata_catalog + entry->request_path);
    }
  }

  return serializer.get_result();
}

}  // namespace

HandlerDbSchemaMetadataCatalog::HandlerDbSchemaMetadataCatalog(
    std::weak_ptr<DbSchemaEndpoint> schema_endpoint,
    mrs::interface::AuthorizeManager *auth_manager)
    : Handler(handler::get_protocol(schema_endpoint),
              get_endpoint_host(schema_endpoint),
              /* path: /service/schema/metadata-catalog/ */
              {path_schema_catalog(lock(schema_endpoint)->get_url_path())},
              get_endpoint_options(lock(schema_endpoint)), auth_manager),
      endpoint_{schema_endpoint} {
  // Please note that constructor should either, make copy
  //  of shared pointers, or copy the data, because
  //  later they may be not consistent.
  //
  //  Locked weak-pointers, must not be copied. Doing so may
  //  lead to circular dependencies.
  //
  // Important: The handler should minimize the access to dynamic
  // content. Mostly only "children" should be accessed by locking.

  auto ep = lock(endpoint_);
  entry_ = ep->get();
  required_authnetication_ = ep->required_authentication();
  url_path_ = ep->get_url_path();

  auto sp = lock_parent(ep);
  assert(sp);
  service_entry_ = sp->get();
}

void HandlerDbSchemaMetadataCatalog::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerDbSchemaMetadataCatalog::handle_get(
    rest::RequestContext *ctxt) {
  auto &requests_uri = ctxt->request->get_uri();
  log_debug("Schema::handle_get '%s'", requests_uri.get_path().c_str());
  auto locked_endpoint = lock_or_throw_unavail(endpoint_);
  auto url = url_sch_metadata_catalog(locked_endpoint->get_url());
  const uint64_t k_default_limit = 25;
  uint64_t offset = 0;
  uint64_t limit = k_default_limit;
  bool has_more = false;
  json::ResponseJsonTemplate response_template{false};

  Url::parse_offset_limit(requests_uri.get_query_elements(), &offset, &limit);

  response_template.begin_resultset_with_limits(
      offset, limit, limit == k_default_limit, url, {});

  auto children = locked_endpoint->get_children();

  for (auto &child : subrange(&children, offset, limit, &has_more)) {
    response_template.push_json_document(
        generate_json_descriptor(child.get(),
                                 url_path_ + "/" + k_path_metadata_catalog)
            .c_str());
  }

  response_template.end_resultset(has_more);

  return response_template.get_result();
}

HttpResult HandlerDbSchemaMetadataCatalog::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbSchemaMetadataCatalog::handle_delete(
    rest::RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbSchemaMetadataCatalog::handle_put(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HandlerDbSchemaMetadataCatalog::Authorization
HandlerDbSchemaMetadataCatalog::requires_authentication() const {
  return required_authnetication_ ? Authorization::kCheck
                                  : Authorization::kNotNeeded;
}

UniversalId HandlerDbSchemaMetadataCatalog::get_service_id() const {
  return entry_->service_id;
}

UniversalId HandlerDbSchemaMetadataCatalog::get_db_object_id() const {
  return {};
}

UniversalId HandlerDbSchemaMetadataCatalog::get_schema_id() const {
  return entry_->id;
}

const std::string &HandlerDbSchemaMetadataCatalog::get_service_path() const {
  return service_entry_->url_context_root;
}

const std::string &HandlerDbSchemaMetadataCatalog::get_schema_path() const {
  return entry_->request_path;
}

const std::string &HandlerDbSchemaMetadataCatalog::get_db_object_path() const {
  return empty_path();
}

uint32_t HandlerDbSchemaMetadataCatalog::get_access_rights() const {
  return mrs::database::entry::Operation::valueRead;
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
