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

#include "mrs/rest/handler.h"

#include <memory>
#include <utility>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/http_request.h"
#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/rest_api_utils.h"

#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/http/error.h"
#include "mrs/interface/object.h"
#include "mrs/rest/request_context.h"

#include "collector/mysql_cache_manager.h"
#include "helper/json/rapid_json_to_map.h"
#include "helper/json/text_to.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using AuthHandler = mrs::interface::AuthorizeManager::AuthorizeHandlerPtr;
using AuthHandlers = mrs::interface::AuthorizeManager::AuthHandlers;
using WwwAuthenticationHandler = mrs::authentication::WwwAuthenticationHandler;
using Parameters = mrs::interface::RestHandler::Parameters;

template <typename T>
std::string to_string(const helper::Optional<T> &v) {
  if (!v) return "null";

  return std::to_string(v.value());
}

uint32_t check_privileges(
    const std::vector<database::entry::AuthPrivilege> &privileges,
    uint64_t service_id, uint64_t schema_id, uint64_t db_object_id) {
  uint32_t aggregated_privileges = 0;
  log_debug("RestRequestHandler: look for service:%i, schema:%i, obj:%i",
            (int)service_id, (int)schema_id, (int)db_object_id);

  for (const auto &p : privileges) {
    log_debug("RestRequestHandler: next iteration");
    log_debug("RestRequestHandler: permissions:%i", p.crud);
    log_debug("RestRequestHandler: object_id:%s",
              to_string(p.object_id).c_str());
    log_debug("RestRequestHandler: schema_id:%s",
              to_string(p.schema_id).c_str());
    log_debug("RestRequestHandler: service_id:%s",
              to_string(p.service_id).c_str());

    if (!p.object_id && !p.schema_id && !p.service_id) {
      aggregated_privileges |= p.crud;
      continue;
    }

    if (p.object_id && db_object_id == *p.object_id) {
      aggregated_privileges |= p.crud;
    }

    if (p.schema_id && schema_id == *p.schema_id) {
      aggregated_privileges |= p.crud;
    }

    if (p.service_id && service_id == *p.service_id) {
      aggregated_privileges |= p.crud;
    }
  }

  log_debug("RestRequestHandler: aggregated_privileges:%i",
            aggregated_privileges);

  return aggregated_privileges;
}

uint32_t get_access_right_from_http_method(const uint32_t method) {
  using Route = mrs::interface::Object;

  switch (method) {
    case HttpMethod::Get:
      return Route::kRead;
    case HttpMethod::Post:
      return Route::kCreate;
    case HttpMethod::Put:
      return Route::kUpdate;
    case HttpMethod::Delete:
      return Route::kDelete;
  }

  return 0;
}

static const char *get_content_type(
    const Handler::Result::Type type,
    const std::optional<std::string> &type_text) {
  if (type_text) return type_text.value().c_str();

  switch (type) {
    case Handler::Result::Type::typeJson:
      return "application/json";
    case Handler::Result::Type::typeUnknownBinary:
      return "application/octet-stream";
    case Handler::Result::Type::typeUnknownText:
      return "text/plain";
    case Handler::Result::Type::typePlain:
      return "text/plain";
    case Handler::Result::Type::typeHtml:
      return "text/html";
    case Handler::Result::Type::typeJs:
      return "text/javascript";
    case Handler::Result::Type::typeCss:
      return "text/css";
    case Handler::Result::Type::typePng:
      return "image/png";
    case Handler::Result::Type::typeJpg:
      return "image/jpeg";
    case Handler::Result::Type::typeGif:
      return "image/gif";
    case Handler::Result::Type::typeBmp:
      return "image/bmp";
    case Handler::Result::Type::typeAvi:
      return "image/avi";
    case Handler::Result::Type::typeWav:
      return "image/wav";
  }

  return "";
}

class RestRequestHandler : public BaseRequestHandler {
 public:
  using Cached = collector::MysqlCacheManager::CachedObject;
  using AuthUser = database::entry::AuthUser;

 public:
  RestRequestHandler(Handler *rest_handler,
                     mrs::interface::AuthorizeManager *auth_manager)
      : rest_handler_{rest_handler}, auth_manager_{auth_manager} {}

  void handle_request(HttpRequest &req) override {
    RequestContext request_ctxt{&req};

    try {
      Handler::Result result;
      request_ctxt.sql_session_cache = auth_manager_->get_cache()->get_empty(
          collector::kMySQLConnectionMetadata);

      auto &out_hdrs = req.get_output_headers();
      const auto service_id = rest_handler_->get_service_id();
      const auto method = req.get_method();

      log_debug("RestRequestHandler(service_id:%i): start(url='%s')",
                static_cast<int>(service_id),
                request_ctxt.request->get_uri().join().c_str());

      for (auto &kv : rest_handler_->get_headers_parameters()) {
        auto &oh = request_ctxt.request->get_output_headers();
        oh.add(kv.first.c_str(), kv.second.c_str());
      }

      if (!rest_handler_->request_begin(&request_ctxt)) {
        log_debug("'request_begin' returned false");
        throw http::Error{HttpStatusCode::Forbidden};
      }

      auto required_access = get_access_right_from_http_method(method);
      if (!(required_access & rest_handler_->get_access_rights())) {
        log_debug(
            "'required_access' denied, required_access:%i, "
            "access:%i",
            required_access, rest_handler_->get_access_rights());
        throw http::Error{HttpStatusCode::Forbidden};
      }

      auto required_auth = rest_handler_->requires_authentication();
      if (Handler::Authorization::kNotNeeded != required_auth) {
        log_debug("RestRequestHandler(service_id:%i): authenticate",
                  static_cast<int>(service_id));

        // request_ctxt.user is valid after success of this call
        if (Handler::Authorization::kRequires == required_auth) {
          http::Url url{request_ctxt.request->get_uri()};
          if (!auth_manager_->authorize(service_id, &request_ctxt.cookies, &url,
                                        &request_ctxt.sql_session_cache,
                                        req.get_input_headers(),
                                        &request_ctxt.user)) {
            log_debug("Authentication handler returned false");
            throw http::Error(HttpStatusCode::Unauthorized);
          }
        } else {
          // Just check the user
          auth_manager_->is_authorized(service_id, &request_ctxt.cookies,
                                       req.get_input_headers(),
                                       &request_ctxt.user);
        }

        rest_handler_->authorization(&request_ctxt);

        if (rest_handler_->may_check_access()) {
          log_debug("RestRequestHandler(service_id:%i): required_access:%i",
                    static_cast<int>(service_id), required_access);
          if (!(required_access &
                check_privileges(request_ctxt.user.privileges, service_id,
                                 rest_handler_->get_schema_id(),
                                 rest_handler_->get_db_object_id()))) {
            throw http::Error{HttpStatusCode::Forbidden};
          }
        }
      }

      log_debug("RestRequestHandler(service_id:%i): dispatch",
                static_cast<int>(service_id));
      switch (method) {
        case HttpMethod::Get:
          result = rest_handler_->handle_get(&request_ctxt);
          break;

        case HttpMethod::Post: {
          auto &input_buffer = req.get_input_buffer();
          auto size = input_buffer.length();
          result = rest_handler_->handle_post(&request_ctxt,
                                              input_buffer.pop_front(size));
        } break;

        case HttpMethod::Delete:
          result = rest_handler_->handle_delete(&request_ctxt);
          break;

        case HttpMethod::Put:
          result = rest_handler_->handle_put(&request_ctxt);
          break;

        default:
          throw http::Error{HttpStatusCode::MethodNotAllowed};
      }

      auto &b = req.get_output_buffer();
      b.add(result.response.c_str(), result.response.length());

      if (!result.etag.empty()) {
        out_hdrs.add("Cache-Control", "no-cache");
        out_hdrs.add("ETag", result.etag.c_str());
      }
      out_hdrs.add("Content-Type",
                   get_content_type(result.type, result.type_text));

      req.send_reply(result.status,
                     HttpStatusCode::get_default_status_text(result.status), b);
      rest_handler_->request_end(&request_ctxt);
    } catch (const http::ErrorChangeResponse &e) {
      if (e.retry()) {
        log_debug("handle_request override");
        auto r = e.change_response(&req);
        req.send_reply(r.status, r.message);
      } else
        handle_error(&request_ctxt, e.change_response(&req));
    } catch (const http::Error &e) {
      handle_error(&request_ctxt, e);
    } catch (const std::exception &e) {
      handle_error(&request_ctxt, e);
    }
  }

 private:
  static const http::Error &err_to_http_error(const http::Error &err) {
    return err;
  }

  static http::Error err_to_http_error(const std::exception &) {
    return {HttpStatusCode::InternalError};
  }

  template <typename Err>
  void handle_error(RequestContext *ctxt, const Err &err) {
    const http::Error &e = err_to_http_error(err);
    if (!rest_handler_->request_error(ctxt, e)) {
      switch (e.status) {
        case HttpStatusCode::TemporaryRedirect:
          ctxt->request->send_reply(e.status, e.message);
          break;
        case HttpStatusCode::Unauthorized:
          if (ctxt->selected_handler) {
            auth_manager_->unauthorize(rest_handler_->get_service_id(),
                                       &ctxt->cookies);
          }
          [[fallthrough]];
        default:
          send_rfc7807_error(*ctxt->request, e.status,
                             {{"message", e.message}});
      }
    }
  }

  Handler *rest_handler_;
  mrs::interface::AuthorizeManager *auth_manager_;
};

static Parameters get_json(const std::string &txt, const std::string key_name) {
  using namespace helper::json;
  RapidReaderHandlerToMapOfSimpleValues sub_object;
  ExtractSubObjectHandler<RapidReaderHandlerToMapOfSimpleValues> extractor{
      key_name, sub_object};
  text_to(&extractor, txt);
  return sub_object.get_result();
}

Handler::Handler(const std::string &url, const std::string &rest_path_matcher,
                 const std::string &options,
                 mrs::interface::AuthorizeManager *auth_manager)
    : parameters_{get_json(options, "header")},
      url_{url},
      rest_path_matcher_{rest_path_matcher},
      authorization_manager_{auth_manager} {
  auto handler = std::make_unique<RestRequestHandler>(this, auth_manager);

  log_debug("Handling new URL: '%s'", url_.c_str());
  handler_id_ = HttpServerComponent::get_instance().add_route(
      rest_path_matcher_, std::move(handler));
}

Handler::~Handler() {
  log_debug("Removing URL handler: '%s'", url_.c_str());
  HttpServerComponent::get_instance().remove_route(handler_id_);
}

bool Handler::request_begin(RequestContext *) { return true; }

void Handler::request_end(RequestContext *) {}

bool Handler::request_error(RequestContext *, const http::Error &) {
  return false;
}

const Parameters &Handler::get_headers_parameters() const {
  return parameters_;
}

void Handler::throw_unauthorize_when_check_auth_fails(RequestContext *ctxt) {
  if (this->requires_authentication() != Authorization::kNotNeeded) {
    if (!ctxt->user.has_user_id)
      throw http::Error(HttpStatusCode::Unauthorized);
  }
}

void Handler::authorization(RequestContext *) {}

bool Handler::may_check_access() const { return true; }

}  // namespace rest
}  // namespace mrs
