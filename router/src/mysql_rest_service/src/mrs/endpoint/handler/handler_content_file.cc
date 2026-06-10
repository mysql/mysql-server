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

#include "mrs/endpoint/handler/handler_content_file.h"

#include <map>
#include <vector>

#include "helper/digester/md5_digest.h"
#include "helper/media_type.h"
#include "helper/string/hex.h"

#include "mrs/database/query_factory.h"
#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

using Type = mrs::interface::RestHandler::HttpResult::Type;
using HttpResult = mrs::rest::Handler::HttpResult;
using Authorization = mrs::rest::Handler::Authorization;
using collector::MysqlCacheManager;
using collector::MySQLConnection;
using mysqlrouter::MySQLSession;

namespace mrs {
namespace endpoint {
namespace handler {

namespace {

auto get_path_for_db_object(
    std::weak_ptr<mrs::endpoint::ContentFileEndpoint> endpoint,
    const bool is_index) {
  auto ep = mrs::endpoint::handler::lock(endpoint);
  auto parent_ep = ep->get_parent_ptr();

  const std::string &object_path = ep->get()->request_path;
  const std::string &service_schema_path = parent_ep->get_url_path();

  return mrs::endpoint::handler::path_file(service_schema_path, object_path,
                                           is_index);
}

}  // namespace

HandlerContentFile::HandlerContentFile(
    std::weak_ptr<ContentFileEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager,
    std::shared_ptr<PersistentDataContentFile> persistent_data_content_file,
    const bool is_index)
    : Handler(handler::get_protocol(endpoint), get_endpoint_host(endpoint),
              get_path_for_db_object(endpoint, is_index),
              get_endpoint_options(lock(endpoint)), auth_manager),
      endpoint_{endpoint},
      persistent_data_content_file_{persistent_data_content_file} {
  auto endpoint_file = lock(endpoint);
  auto endpoint_set = lock_parent(endpoint_file);
  assert(endpoint_set && "Parent must be available.");
  auto service = lock_parent(endpoint_set);
  assert(service && "Parent must be available.");

  entry_file_ = endpoint_file->get();
  entry_set_ = endpoint_set->get();
  entry_service_ = service->get();

  helper::digester::Md5Digest version_calculation;
  version_calculation.update(std::to_string(time(nullptr)));
  version_calculation.update(std::to_string(entry_file_->size));
  version_ = helper::string::hex<std::string, helper::string::CStringHex>(
      version_calculation.finalize());
}

UniversalId HandlerContentFile::get_service_id() const {
  return entry_set_->service_id;
}

UniversalId HandlerContentFile::get_schema_id() const { return {}; }

UniversalId HandlerContentFile::get_db_object_id() const { return {}; }

const std::string &HandlerContentFile::get_service_path() const {
  return entry_service_->url_context_root;
}

const std::string &HandlerContentFile::get_schema_path() const {
  return empty_path();
}

const std::string &HandlerContentFile::get_db_object_path() const {
  return empty_path();
}

Authorization HandlerContentFile::requires_authentication() const {
  bool auth = entry_file_->requires_authentication ||
              entry_set_->requires_authentication;
  return auth ? Authorization::kRequires : Authorization::kNotNeeded;
}

uint32_t HandlerContentFile::get_access_rights() const {
  return mrs::database::entry::Operation::valueRead;
}

void HandlerContentFile::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerContentFile::handle_get(rest::RequestContext *ctxt) {
  auto if_not_matched =
      ctxt->request->get_input_headers().find_cstr("If-None-Match");

  if (if_not_matched && version_ == if_not_matched)
    throw http::Error(HttpStatusCode::NotModified);

  auto [content, content_type] =
      persistent_data_content_file_->fetch_file(ctxt->sql_session_cache.get());

  return {std::move(content), content_type, version_};
}

HttpResult HandlerContentFile::handle_delete(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HttpResult HandlerContentFile::handle_post(rest::RequestContext *,
                                           const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HttpResult HandlerContentFile::handle_put(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
