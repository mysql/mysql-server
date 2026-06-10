/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_REQUEST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_REQUEST_HANDLER_H_

#include <map>
#include <string>

#include "http/base/request_handler.h"
#include "mrs/database/json_mapper/errors.h"
#include "mrs/http/error.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/rest_error.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/rest/handler.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace rest {

uint32_t do_privilege_check(
    const std::vector<database::entry::AuthPrivilege> &privileges,
    const UniversalId &service_id, const std::string &service_path,
    const UniversalId &schema_id, const std::string &schema_path,
    const UniversalId &db_object_id, const std::string &db_object_path);

class RestRequestHandler : public ::http::base::RequestHandler {
 public:
  using Cached = collector::MysqlCacheManager::CachedObject;
  using AuthUser = database::entry::AuthUser;
  using RestHandler = mrs::interface::RestHandler;
  using HandlerPtr = std::weak_ptr<RestHandler>;
  using Options = mrs::interface::Options;
  using RestError = mrs::interface::RestError;
  using HttpBuffer = ::http::base::IOBuffer;
  using HttpHeaders = ::http::base::Headers;
  using ETagMismatch = mrs::interface::ETagMismatch;
  using ObjectKeyValue = std::map<std::string, std::string>;
  using HttpRequest = ::http::base::Request;

 public:
  RestRequestHandler(HandlerPtr rest_handler,
                     mrs::interface::AuthorizeManager *auth_manager,
                     const bool may_log_requests)
      : rest_handler_{rest_handler},
        auth_manager_{auth_manager},
        may_log_requests_{may_log_requests} {}

  void trace_error(const http::ErrorChangeResponse &e);
  void trace_error(const http::Error &e);
  void trace_error(const mysqlrouter::MySQLSession::Error &e);
  void trace_error(const RestError &e);
  void trace_error(const std::exception &e);
  void trace_error(const Handler::HttpResult &e);

  void trace_http(const char *type, interface::ReqRes &options,
                  HttpMethod::key_type method, const std::string &path,
                  const HttpHeaders &headers, HttpBuffer &buffer) const;

  Handler::HttpResult handle_request_impl(RestHandler *handler,
                                          RequestContext &ctxt);

  void handle_request(::http::base::Request &req) override;

  static std::string mask_password(const std::string &input);

 private:
  mysql_harness::logging::DomainLogger logger_;

  static http::Error err_to_http_error(
      const mysqlrouter::MySQLSession::Error &err);

  static const http::Error &err_to_http_error(const http::Error &err) {
    return err;
  }

  static http::Error err_to_http_error(
      const mrs::database::JSONInputError &err) {
    return {HttpStatusCode::BadRequest, err.what()};
  }

  static http::Error err_to_http_error(const RestError &err) {
    return {HttpStatusCode::BadRequest, err.what()};
  }

  static http::Error err_to_http_error(const ETagMismatch &err) {
    return {HttpStatusCode::PreconditionFailed, err.what()};
  }

  static http::Error err_to_http_error(const std::invalid_argument &err) {
    return {HttpStatusCode::BadRequest, err.what()};
  }

  static http::Error err_to_http_error(const std::exception &) {
    return {HttpStatusCode::InternalError};
  }

  ObjectKeyValue responose_encode_error(
      const http::Error &, const mysqlrouter::MySQLSession::Error &e) {
    ObjectKeyValue result{{"message", e.message()},
                          {"what", e.what()},
                          {"sqlcode", std::to_string(e.code())}};
    return result;
  }

  ObjectKeyValue responose_encode_error(const http::Error &converted,
                                        const std::exception &e) {
    ObjectKeyValue result{{"message", converted.message}, {"what", e.what()}};
    return result;
  }

  ObjectKeyValue responose_encode_error(const http::Error &converted,
                                        const http::Error &) {
    ObjectKeyValue result{{"message", converted.message}};
    return result;
  }

  template <typename Err>
  void handle_error(RestHandler *handler, RequestContext *ctxt, const Err &err);

  const Options &get_options();

  void send_reply(HttpRequest &req, int status_code);

  void send_reply(HttpRequest &req, int status_code,
                  const std::string &status_text);

  void send_reply(HttpRequest &req, int status_code,
                  const std::string &status_text, HttpBuffer &buffer);

  void send_rfc7807_error(HttpRequest &req,
                          HttpStatusCode::key_type status_code,
                          const std::map<std::string, std::string> &fields);

  void send_json_document(HttpRequest &req,
                          HttpStatusCode::key_type status_code,
                          const rapidjson::Document &json_doc);

  HandlerPtr rest_handler_;
  mrs::interface::AuthorizeManager *auth_manager_;
  const bool may_log_requests_;
};

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_
