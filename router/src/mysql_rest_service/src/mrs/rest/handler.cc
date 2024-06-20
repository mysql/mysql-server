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

#include "mrs/rest/handler.h"

#include <memory>
#include <string>
#include <utility>

#include "mysqld_error.h"

#include "http/base/request_handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/component/http_server_component.h"

#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/http/error.h"
#include "mrs/interface/object.h"
#include "mrs/interface/rest_error.h"
#include "mrs/rest/request_context.h"
#include "mrs/router_observation_entities.h"

#include "collector/mysql_cache_manager.h"
#include "helper/container/generic.h"
#include "helper/json/rapid_json_to_map.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "helper/string/contains.h"
#include "helper/to_string.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using RestError = mrs::interface::RestError;
using ETagMismatch = mrs::interface::ETagMismatch;
using AuthHandler = mrs::interface::AuthorizeManager::AuthorizeHandlerPtr;
using AuthHandlers = mrs::interface::AuthorizeManager::AuthHandlers;
using WwwAuthenticationHandler = mrs::authentication::WwwAuthenticationHandler;
using Parameters = mrs::interface::RestHandler::Parameters;
using HttpHeaders = ::http::base::Headers;
using HttpBuffer = ::http::base::IOBuffer;
using HttpRequest = ::http::base::Request;

std::string to_string(const UniversalId &id) { return id.to_string(); }

template <typename T>
std::string to_string(const helper::Optional<T> &v) {
  using std::to_string;
  if (!v) return "null";

  return to_string(v.value());
}

uint32_t check_privileges(
    const std::vector<database::entry::AuthPrivilege> &privileges,
    UniversalId service_id, UniversalId schema_id, UniversalId db_object_id) {
  uint32_t aggregated_privileges = 0;
  log_debug("RestRequestHandler: look for service:%s, schema:%s, obj:%s",
            service_id.to_string().c_str(), schema_id.to_string().c_str(),
            db_object_id.to_string().c_str());

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

    if (p.object_id.has_value() && db_object_id == *p.object_id) {
      aggregated_privileges |= p.crud;
    }

    if (p.schema_id.has_value() && schema_id == *p.schema_id) {
      aggregated_privileges |= p.crud;
    }

    if (p.service_id.has_value() && service_id == *p.service_id) {
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
    const Handler::HttpResult::Type type,
    const std::optional<std::string> &type_text) {
  if (type_text) return type_text.value().c_str();

  return helper::get_mime_name(type);
}

std::string get_http_method_name(HttpMethod::key_type type) {
  static const std::map<HttpMethod::key_type, std::string> allowed_types{
      {HttpMethod::Connect, "CONNECT"}, {HttpMethod::Delete, "DELETE"},
      {HttpMethod::Get, "GET"},         {HttpMethod::Head, "HEAD"},
      {HttpMethod::Options, "OPTIONS"}, {HttpMethod::Patch, "PATCH"},
      {HttpMethod::Post, "POST"},       {HttpMethod::Put, "PUT"},
      {HttpMethod::Trace, "TRACE"}};

  auto it = allowed_types.find(type);
  if (it != allowed_types.end()) {
    return it->second;
  }

  return std::to_string(type);
}

void trace_error(const http::ErrorChangeResponse &e) {
  log_debug("Catch: ErrorChangeResponse name: %s", e.name());
  log_debug("Catch: ErrorChangeResponse retry: %s",
            (e.retry() ? "true" : "false"));
}

void trace_error(const http::Error &e) {
  log_debug("Catch: http::Error status: %i", e.status);
  log_debug("Catch: http::Error message: %s", e.message.c_str());
}

void trace_error(const mysqlrouter::MySQLSession::Error &e) {
  log_debug("Catch: MySQLSession::Error code: %i", static_cast<int>(e.code()));
  log_debug("Catch: MySQLSession::Error message: %s", e.message().c_str());
  log_debug("Catch: MySQLSession::Error message: %s", e.what());
}

void trace_error(const RestError &e) {
  log_debug("Catch: RestError message: %s", e.what());
}

void trace_error(const std::exception &e) {
  log_debug("Catch: std::exception message: %s", e.what());
}

void trace_error(const Handler::HttpResult &e) {
  log_debug("Catch: HttpResult with code: %i", (int)e.status);
  log_debug("Catch: HttpResult with message: %s", e.response.c_str());
}

class RestRequestHandler : public ::http::base::RequestHandler {
 public:
  using Cached = collector::MysqlCacheManager::CachedObject;
  using AuthUser = database::entry::AuthUser;

 public:
  RestRequestHandler(Handler *rest_handler,
                     mrs::interface::AuthorizeManager *auth_manager)
      : rest_handler_{rest_handler}, auth_manager_{auth_manager} {}

  void trace_http(const char *type, interface::ReqRes &options,
                  HttpMethod::key_type method, const HttpHeaders &headers,
                  HttpBuffer &buffer) {
    if (options.header_) {
      log_debug("HTTP %s method: %s", type,
                get_http_method_name(method).c_str());

      for (const auto &[k, v] : headers) {
        log_debug("HTTP %s parameters: %s=%s", type, k.c_str(), v.c_str());
      }
    }

    auto in_len = buffer.length();
    if (in_len && options.header_) {
      auto data = buffer.copy(in_len);
      log_debug("HTTP %s body: %.*s", type, static_cast<int>(data.size()),
                reinterpret_cast<const char *>(&data[0]));
    }
  }

  Handler::HttpResult handle_request_impl(RequestContext &ctxt) {
    ctxt.sql_session_cache = auth_manager_->get_cache()->get_empty(
        collector::kMySQLConnectionMetadataRO, false);

    const auto service_id = rest_handler_->get_service_id();
    const auto method = ctxt.request->get_method();

    log_debug("handle_request(service_id:%s): start(url='%s')",
              service_id.to_string().c_str(),
              ctxt.request->get_uri().join().c_str());

    auto options = rest_handler_->get_options();
    auto &ih = ctxt.request->get_input_headers();
    auto &oh = ctxt.request->get_output_headers();

    switch (ctxt.request->get_method()) {
      case HttpMethod::Options:
        Counter<kEntityCounterHttpRequestOptions>::increment();
        break;
      case HttpMethod::Get:
        Counter<kEntityCounterHttpRequestGet>::increment();
        break;
      case HttpMethod::Post:
        Counter<kEntityCounterHttpRequestPost>::increment();
        break;
      case HttpMethod::Put:
        Counter<kEntityCounterHttpRequestPut>::increment();
        break;
      case HttpMethod::Delete:
        Counter<kEntityCounterHttpRequestDelete>::increment();
        break;
      default:
        break;
    }

    trace_http("Request", options.debug.http.request, method, ih,
               ctxt.request->get_input_buffer());

    for (auto &kv : rest_handler_->get_options().parameters_) {
      if (mysql_harness::make_lower(kv.first) ==
          "access-control-allow-origin") {
        if (rest_handler_->get_options().allowed_origins.type !=
            mrs::interface::Options::AllowedOrigins::AllowNone) {
          continue;
        }
      }
      oh.add(kv.first.c_str(), kv.second.c_str());
    }

    auto origin = ih.find_cstr("Origin");

    if (origin) {
      using AO = mrs::interface::Options::AllowedOrigins;

      auto &ao = rest_handler_->get_options().allowed_origins;

      switch (ao.type) {
        case AO::AllowAll:
          oh.add("Access-Control-Allow-Origin", origin);
          break;
        case AO::AllowSpecified:
          if (helper::container::has(ao.allowed_origins, origin))
            oh.add("Access-Control-Allow-Origin", origin);
          break;
        case AO::AllowNone:
          break;
      }
    }

    // set the Access-Control-Allow-Methods if not already set on the service
    // level
    if (!oh.find("Access-Control-Allow-Methods")) {
      std::string access_control_allow_methods;
      for (const auto method :
           {HttpMethod::Get, HttpMethod::Post, HttpMethod::Put,
            HttpMethod::Delete, HttpMethod::Options}) {
        if ((method == HttpMethod::Options) ||
            (get_access_right_from_http_method(method) &
             rest_handler_->get_access_rights())) {
          if (!access_control_allow_methods.empty()) {
            access_control_allow_methods += ", ";
          }
          access_control_allow_methods += get_http_method_name(method);
        }
      }
      oh.add("Access-Control-Allow-Methods",
             access_control_allow_methods.c_str());
    }

    if (method == HttpMethod::Options) {
      throw http::Error{HttpStatusCode::Ok};
    }

    if (!rest_handler_->request_begin(&ctxt)) {
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
      log_debug("RestRequestHandler(service_id:%s): authenticate",
                service_id.to_string().c_str());

      // request_ctxt.user is valid after success of this call
      if (Handler::Authorization::kRequires == required_auth) {
        try {
          if (!auth_manager_->authorize(service_id, ctxt, &ctxt.user)) {
            log_debug("Authentication handler fails");
            throw http::Error(HttpStatusCode::Unauthorized);
          }

        } catch (const Handler::HttpResult &force_result) {
          if (rest_handler_->get_options().debug.log_exceptions)
            trace_error(force_result);
          return force_result;
        }

        log_debug("Authentication handler ok.");
      } else {
        // Just check the user
        auth_manager_->is_authorized(service_id, ctxt, &ctxt.user);
      }

      rest_handler_->authorization(&ctxt);

      if (rest_handler_->may_check_access()) {
        log_debug("RestRequestHandler(service_id:%s): required_access:%i",
                  service_id.to_string().c_str(), required_access);
        if (!(required_access &
              check_privileges(ctxt.user.privileges, service_id,
                               rest_handler_->get_schema_id(),
                               rest_handler_->get_db_object_id()))) {
          throw http::Error{HttpStatusCode::Forbidden};
        }
      }
    }

    log_debug("RestRequestHandler(service_id:%s): dispatch(method:%s, path:%s)",
              service_id.to_string().c_str(),
              get_http_method_name(ctxt.request->get_method()).c_str(),
              ctxt.request->get_uri().get_path().c_str());
    switch (method) {
      case HttpMethod::Get:
        return rest_handler_->handle_get(&ctxt);
        break;

      case HttpMethod::Post: {
        auto &input_buffer = ctxt.request->get_input_buffer();
        auto size = input_buffer.length();
        return rest_handler_->handle_post(&ctxt, input_buffer.pop_front(size));
      } break;

      case HttpMethod::Delete:
        return rest_handler_->handle_delete(&ctxt);
        break;

      case HttpMethod::Put:
        return rest_handler_->handle_put(&ctxt);
        break;

      default:
        throw http::Error{HttpStatusCode::MethodNotAllowed};
    }
  }

  void handle_request(::http::base::Request &req) override {
    RequestContext request_ctxt{&req, auth_manager_};

    try {
      auto result = handle_request_impl(request_ctxt);

      auto &b = req.get_output_buffer();
      auto &out_hdrs = req.get_output_headers();

      b.add(result.response.c_str(), result.response.length());

      if (!result.etag.empty()) {
        out_hdrs.add("Cache-Control", "no-cache");
        out_hdrs.add("ETag", result.etag.c_str());
      }
      out_hdrs.add("Content-Type",
                   get_content_type(result.type, result.type_text));

      send_reply(req, result.status,
                 HttpStatusCode::get_default_status_text(result.status), b);
      rest_handler_->request_end(&request_ctxt);
    } catch (const http::ErrorChangeResponse &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      if (e.retry()) {
        log_debug("handle_request override");
        auto r = e.change_response(&req);
        send_reply(req, r.status, r.message);
      } else
        handle_error(&request_ctxt, e.change_response(&req));
    } catch (const http::Error &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    } catch (const mrs::database::JSONInputError &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    } catch (const RestError &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    } catch (const ETagMismatch &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    } catch (const std::invalid_argument &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    } catch (const std::exception &e) {
      if (rest_handler_->get_options().debug.log_exceptions) trace_error(e);
      handle_error(&request_ctxt, e);
    }
  }

 private:
  static const http::Error err_to_http_error(
      const mysqlrouter::MySQLSession::Error &err) {
    if (ER_GTID_MODE_OFF == err.code()) {
      return {HttpStatusCode::BadRequest,
              "'Asof' requirement was not fulfilled, GTID_MODE is not "
              "configured properly on the MySQL Server."};
    }
    return {HttpStatusCode::InternalError};
  }

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

  using ObjectKeyValue = std::map<std::string, std::string>;

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
  void handle_error(RequestContext *ctxt, const Err &err) {
    log_debug("void handle_error(RequestContext *ctxt, const Err &err)");
    const http::Error &e = err_to_http_error(err);
    if (!rest_handler_->request_error(ctxt, e)) {
      switch (e.status) {
        case HttpStatusCode::Ok:
          [[fallthrough]];
        case HttpStatusCode::NotModified:
          send_reply(*ctxt->request, e.status, e.message);
          break;
        case HttpStatusCode::TemporaryRedirect:
          send_reply(*ctxt->request, e.status, e.message);
          break;
        case HttpStatusCode::Unauthorized:
          if (ctxt->selected_handler) {
            auth_manager_->unauthorize(rest_handler_->get_service_id(),
                                       &ctxt->cookies);
          }
          [[fallthrough]];
        default:
          if (rest_handler_->get_options().debug.http.response.detailed_errors_)
            send_rfc7807_error(*ctxt->request, e.status,
                               responose_encode_error(e, err));
          else
            send_rfc7807_error(*ctxt->request, e.status,
                               responose_encode_error(e, e));
      }
    }
  }

  void send_reply(HttpRequest &req, int status_code) {
    auto options = rest_handler_->get_options();
    if (options.debug.http.response.body_)
      log_debug("HTTP Response status: %i", status_code);

    trace_http("Response", options.debug.http.response, req.get_method(),
               req.get_output_headers(), req.get_output_buffer());
    req.send_reply(status_code);
  }

  void send_reply(HttpRequest &req, int status_code, std::string status_text) {
    auto options = rest_handler_->get_options();
    if (options.debug.http.response.body_) {
      log_debug("HTTP Response status: %i", status_code);
      log_debug("HTTP Response status text: %s", status_text.c_str());
    }
    trace_http("Response", options.debug.http.response, req.get_method(),
               req.get_output_headers(), req.get_output_buffer());
    req.send_reply(status_code, status_text);
  }

  void send_reply(HttpRequest &req, int status_code, std::string status_text,
                  HttpBuffer &buffer) {
    auto options = rest_handler_->get_options();
    if (options.debug.http.response.body_) {
      log_debug("HTTP Response status: %i", status_code);
      log_debug("HTTP Response status text: %s", status_text.c_str());
    }
    trace_http("Response", options.debug.http.response, req.get_method(),
               req.get_output_headers(), buffer);
    req.send_reply(status_code, status_text, buffer);
  }

  void send_rfc7807_error(HttpRequest &req,
                          HttpStatusCode::key_type status_code,
                          const std::map<std::string, std::string> &fields) {
    auto &out_hdrs = req.get_output_headers();
    out_hdrs.add("Content-Type", "application/problem+json");

    rapidjson::Document json_doc;

    auto &allocator = json_doc.GetAllocator();

    json_doc.SetObject();
    for (const auto &field : fields) {
      json_doc.AddMember(
          rapidjson::Value(field.first.c_str(), field.first.size(), allocator),
          rapidjson::Value(field.second.c_str(), field.second.size(),
                           allocator),
          allocator);
    }

    json_doc.AddMember("status", status_code, allocator);

    send_json_document(req, status_code, json_doc);
  }

  void send_json_document(HttpRequest &req,
                          HttpStatusCode::key_type status_code,
                          const rapidjson::Document &json_doc) {
    // serialize json-document into a string
    auto &chunk = req.get_output_buffer();

    {
      rapidjson::StringBuffer json_buf;
      {
        rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);

        json_doc.Accept(json_writer);

      }  // free json_doc and json_writer early

      // perhaps we could use evbuffer_add_reference() and a unique-ptr on
      // json_buf here. needs to be benchmarked
      chunk.add(json_buf.GetString(), json_buf.GetSize());
    }  // free json_buf early
    send_reply(req, status_code,
               HttpStatusCode::get_default_status_text(status_code), chunk);
  }

  Handler *rest_handler_;
  mrs::interface::AuthorizeManager *auth_manager_;
};

namespace cvt {
using std::to_string;
const std::string &to_string(const std::string &str) { return str; }
}  // namespace cvt

static const char *to_cstr(const bool b) { return b ? "true" : "false"; }

template <typename ValueType, bool default_value = false>
bool to_bool(const ValueType &value) {
  using std::to_string;
  const static std::map<std::string, bool> allowed_values{
      {"true", true}, {"false", false}, {"1", true}, {"0", false}};
  auto it = allowed_values.find(cvt::to_string(value));
  if (it != allowed_values.end()) {
    return it->second;
  }

  return default_value;
}

template <typename ValueType>
uint64_t to_uint(const ValueType &value) {
  const auto &v = cvt::to_string(value);
  return std::stoull(v.c_str());
}

class ParseOptions
    : public helper::json::RapidReaderHandlerToStruct<mrs::interface::Options> {
 public:
  template <typename ValueType>
  void handle_object_value(const std::string &key, const ValueType &vt) {
    //    log_debug("handle_object_value key:%s, v:%s", key.c_str(),
    //              cvt::to_string(vt).c_str());
    static const std::string kHeaders = "headers.";
    using std::to_string;

    if (helper::starts_with(key, kHeaders)) {
      result_.parameters_[key.substr(kHeaders.length())] = cvt::to_string(vt);
    } else if (key == "logging.exceptions") {
      result_.debug.log_exceptions = to_bool(vt);
    } else if (key == "logging.request.headers") {
      result_.debug.http.request.header_ = to_bool(vt);
    } else if (key == "logging.request.body") {
      result_.debug.http.request.body_ = to_bool(vt);
    } else if (key == "logging.response.headers") {
      result_.debug.http.response.header_ = to_bool(vt);
    } else if (key == "logging.response.body") {
      //      log_debug("handle_object_value hit %s", to_cstr(to_bool(vt)));
      result_.debug.http.response.body_ = to_bool(vt);
    } else if (key == "returnInternalErrorDetails") {
      result_.debug.http.response.detailed_errors_ = to_bool(vt);
    } else if (key == "metadata.gtid") {
      result_.metadata.gtid = to_bool(vt);
    } else if (key == "query.wait") {
      result_.query.wait = to_uint(vt);
    } else if (key == "query.embed_wait") {
      result_.query.embed_wait = to_bool(vt);
    } else if (key == "http.allowedOrigin") {
      if (mysql_harness::make_lower(cvt::to_string(vt)) == "auto")
        result_.allowed_origins.type = Result::AllowedOrigins::AllowAll;
      else
        result_.allowed_origins.allowed_origins.push_back(cvt::to_string(vt));
    } else if (key == "result.include_links") {
      result_.result.include_links = to_bool(vt);
    } else if (key == "result.stored_procedure_nest_resultsets") {
      result_.result.stored_procedure_nest_resultsets = to_bool(vt);
    }
  }

  template <typename ValueType>
  void handle_array_value(const std::string &key, const ValueType &vt) {
    using std::to_string;
    if (key == "http.allowedOrigin") {
      result_.allowed_origins.type = Result::AllowedOrigins::AllowSpecified;
      result_.allowed_origins.allowed_origins.push_back(cvt::to_string(vt));
    }
  }

  template <typename ValueType>
  void handle_value(const ValueType &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      handle_object_value(key, vt);
    } else if (is_array_value()) {
      handle_array_value(key, vt);
    }
  }

  bool String(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool RawNumber(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool Bool(bool v) override {
    handle_value(v);
    return true;
  }
};

mrs::interface::Options parse_json_options(const std::string &options) {
  return helper::json::text_to_handler<ParseOptions>(options);
}

Handler::Handler(const std::string &url,
                 const std::vector<std::string> &rest_path_matcher,
                 const std::string &options,
                 mrs::interface::AuthorizeManager *auth_manager)
    : options_{parse_json_options(options)},
      url_{url},
      rest_path_matcher_{rest_path_matcher},
      authorization_manager_{auth_manager} {
  log_debug("Handling new URL: '%s'", url_.c_str());

  for (const auto &kv : options_.parameters_) {
    log_debug("headers: '%s':'%s'", kv.first.c_str(), kv.second.c_str());
  }
  log_debug("debug.log_exceptions: %s", to_cstr(options_.debug.log_exceptions));
  log_debug("debug.http.request.header: %s",
            to_cstr(options_.debug.http.request.header_));
  log_debug("debug.http.request.body: %s",
            to_cstr(options_.debug.http.request.body_));
  log_debug("debug.http.response.header: %s",
            to_cstr(options_.debug.http.response.header_));
  log_debug("debug.http.response.body: %s",
            to_cstr(options_.debug.http.response.body_));
  log_debug("debug.http.response.detailed_errors_: %s",
            to_cstr(options_.debug.http.response.detailed_errors_));

  for (auto &path : rest_path_matcher_) {
    auto handler = std::make_unique<RestRequestHandler>(this, auth_manager);
    log_debug("adding_route: '%s'", path.c_str());
    handler_id_.emplace_back(HttpServerComponent::get_instance().add_route(
        path, std::move(handler)));
  }
}

Handler::~Handler() {
  log_debug("Removing URL handler: '%s'", url_.c_str());
  for (auto id : handler_id_) {
    HttpServerComponent::get_instance().remove_route(id);
  }
}

bool Handler::request_begin(RequestContext *) { return true; }

void Handler::request_end(RequestContext *) {}

bool Handler::request_error(RequestContext *, const http::Error &) {
  return false;
}

const interface::Options &Handler::get_options() const { return options_; }

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
