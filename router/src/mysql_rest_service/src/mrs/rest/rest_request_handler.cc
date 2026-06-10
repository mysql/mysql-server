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

#include "mrs/rest/rest_request_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "mysqld_error.h"

#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/router_observation_entities.h"

#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysqlrouter/component/http_server_component.h"
#include "mysqlrouter/log_filter.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

namespace {

bool _match_glob(const std::string &pat, size_t ppos, const std::string &str,
                 size_t spos) {
  size_t pend = pat.length();
  size_t send = str.length();
  // we allow the string to be matched up to the \0
  while (ppos < pend && spos <= send) {
    int sc = str[spos];
    int pc = pat[ppos];
    switch (pc) {
      case '*':
        // skip multiple consecutive *
        while (ppos < pend && pat[ppos + 1] == '*') ++ppos;

        // match * by trying every substring of str with the rest of the pattern
        for (size_t sp = spos; sp <= send; ++sp) {
          // if something matched, we're fine
          if (_match_glob(pat, ppos + 1, str, sp)) return true;
        }
        // if there were no matches, then give up
        return false;
      case '\\':
        ++ppos;
        if (ppos >= pend)  // can't have an escape at the end of the pattern
          throw std::logic_error("Invalid pattern " + pat);
        pc = pat[ppos];
        if (sc != pc) return false;
        ++ppos;
        ++spos;
        break;
      case '?':
        ++ppos;
        ++spos;
        break;
      default:
        if (sc != pc) return false;
        ++ppos;
        ++spos;
        break;
    }
  }
  return ppos == pend && spos == send;
}

/**
 * Match a string against a glob-like pattern.
 *
 * Allowed wildcard characters: '*', '?'.
 * Supports escaping wildcards via '\\' character.
 *
 * Note: works with ASCII only, no UTF8 support
 */
bool match_glob(const std::string &pattern, const std::string &s) {
  return _match_glob(pattern, 0, s, 0);
}

}  // namespace

using RestError = mrs::interface::RestError;
using ETagMismatch = mrs::interface::ETagMismatch;
using AuthHandler = mrs::interface::AuthorizeManager::AuthorizeHandlerPtr;
using AuthHandlers = mrs::interface::AuthorizeManager::AuthHandlers;
using WwwAuthenticationHandler = mrs::authentication::WwwAuthenticationHandler;
using Parameters = mrs::interface::RestHandler::Parameters;
using HttpHeaders = ::http::base::Headers;
using HttpBuffer = ::http::base::IOBuffer;
using ApplyToV3 = mrs::database::entry::AuthPrivilege::ApplyToV3;
using ApplyToV4 = mrs::database::entry::AuthPrivilege::ApplyToV4;

template <typename T>
std::string to_string(const std::optional<T> &v) {
  using std::to_string;
  if (!v.has_value()) return "null";

  return to_string(v.value());
}

static bool check_privileges_v3(const ApplyToV3 &p,
                                const UniversalId &service_id,
                                const UniversalId &schema_id,
                                const UniversalId &db_object_id) {
  const bool log_level_is_debug = mysql_harness::logging::log_level_is_handled(
      mysql_harness::logging::LogLevel::kDebug);

  if (log_level_is_debug) {
    log_debug("RestRequestHandler: object_id:%s",
              to_string(p.object_id).c_str());
    log_debug("RestRequestHandler: schema_id:%s",
              to_string(p.schema_id).c_str());
    log_debug("RestRequestHandler: service_id:%s",
              to_string(p.service_id).c_str());
  }

  if (!p.object_id && !p.schema_id && !p.service_id) {
    return true;
  }

  if (p.object_id.has_value() && db_object_id == *p.object_id) {
    return true;
  }

  if (p.schema_id.has_value() && schema_id == *p.schema_id) {
    return true;
  }

  return (p.service_id.has_value() && service_id == *p.service_id);
}

static bool check_privileges_v4(const ApplyToV4 &p,
                                const std::string &service_path,
                                const std::string &schema_path,
                                const std::string &db_object_path) {
  if (p.service_name != "*" && !match_glob(p.service_name, service_path))
    return false;

  if (p.schema_name != "*" && !match_glob(p.schema_name, schema_path))
    return false;

  if (p.object_name != "*" && !match_glob(p.object_name, db_object_path))
    return false;

  return true;
}

uint32_t do_privilege_check(
    const std::vector<database::entry::AuthPrivilege> &privileges,
    const UniversalId &service_id, const std::string &service_path,
    const UniversalId &schema_id, const std::string &schema_path,
    const UniversalId &db_object_id, const std::string &db_object_path) {
  uint32_t aggregated_privileges = 0;

  const bool log_level_is_debug = mysql_harness::logging::log_level_is_handled(
      mysql_harness::logging::LogLevel::kDebug);

  if (log_level_is_debug) {
    log_debug("RestRequestHandler: look for service:%s, schema:%s, obj:%s",
              service_path.c_str(), schema_path.c_str(),
              db_object_path.c_str());
  }

  for (const auto &p : privileges) {
    bool matches = false;

    if (std::holds_alternative<ApplyToV3>(p.select_by)) {
      matches = check_privileges_v3(std::get<ApplyToV3>(p.select_by),
                                    service_id, schema_id, db_object_id);
    } else {
      matches = check_privileges_v4(std::get<ApplyToV4>(p.select_by),
                                    service_path, schema_path, db_object_path);
    }

    if (matches) {
      if (log_level_is_debug) {
        log_debug("RestRequestHandler: appending:%i", p.crud);
      }
      aggregated_privileges |= p.crud;
    }
  }

  if (log_level_is_debug) {
    log_debug("RestRequestHandler: aggregated_privileges:%i",
              aggregated_privileges);
  }

  return aggregated_privileges;
}

uint32_t get_access_right_from_http_method(const uint32_t method) {
  using Op = mrs::database::entry::Operation::Values;

  switch (method) {
    case HttpMethod::Get:
      return Op::valueRead;
    case HttpMethod::Post:
      return Op::valueCreate;
    case HttpMethod::Put:
      return Op::valueUpdate;
    case HttpMethod::Delete:
      return Op::valueDelete;
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

void RestRequestHandler::trace_error(const http::ErrorChangeResponse &e) {
  logger_.debug([&]() {
    return std::string("Catch: ErrorChangeResponse name: ").append(e.name());
  });
  logger_.debug([&]() {
    return std::string("Catch: ErrorChangeResponse retry: ")
        .append((e.retry() ? "true" : "false"));
  });
}

void RestRequestHandler::trace_error(const http::Error &e) {
  logger_.debug([&]() {
    return std::string("Catch: http::Error status: ")
        .append(std::to_string(e.status));
  });
  logger_.debug([&]() {
    return std::string("Catch: http::Error message: ").append(e.message);
  });
}

void RestRequestHandler::trace_error(
    const mysqlrouter::MySQLSession::Error &e) {
  logger_.debug([&]() {
    return std::string("Catch: MySQLSession::Error code: ")
        .append(std::to_string(static_cast<int>(e.code())));
  });
  logger_.debug([&]() {
    return std::string("Catch: MySQLSession::Error message: ")
        .append(e.message());
  });
  logger_.debug([&]() {
    return std::string("Catch: MySQLSession::Error message: ").append(e.what());
  });
}

void RestRequestHandler::trace_error(const RestError &e) {
  logger_.debug([&]() {
    return std::string("Catch: RestError message: ").append(e.what());
  });
}

void RestRequestHandler::trace_error(const std::exception &e) {
  logger_.debug([&]() {
    return std::string("Catch: std::exception message: ").append(e.what());
  });
}

void RestRequestHandler::trace_error(const Handler::HttpResult &e) {
  logger_.debug([&]() {
    return std::string("Catch: HttpResult with code: ")
        .append(std::to_string(static_cast<int>(e.status)));
  });
  logger_.debug([&]() {
    return std::string("Catch: HttpResult with message: ").append(e.response);
  });
}

void RestRequestHandler::trace_http(const char *type,
                                    interface::ReqRes &options,
                                    HttpMethod::key_type method,
                                    const std::string &path,
                                    const HttpHeaders &headers,
                                    HttpBuffer &buffer) const {
  if (!may_log_requests_) return;
  if (!options.header_) return;

  logger_.info([&]() {
    return std::string("HTTP ")
        .append(type)
        .append(" method: ")
        .append(get_http_method_name(method));
  });

  logger_.info([&]() {
    return std::string("HTTP ")  //
        .append(type)
        .append(" path: ")
        .append(path);
  });

  for (const auto &[k, v] : headers) {
    logger_.info([&]() {
      const bool hide = (k == "Authorization") || (k == "Location") ||
                        (k == "Set-Cookie") || (k == "Cookie");
      return std::string("HTTP ")
          .append(type)
          .append(" parameters: ")
          .append(k)
          .append("=")
          .append(hide ? "*****" : v);
    });
  }

  // The length header is not available at this point because it is added
  // just before sending the payload to the client.
  if (auto in_len = buffer.length()) {
    logger_.info([&]() {
      return std::string("HTTP ")
          .append(type)
          .append(" body-length: ")
          .append(std::to_string(in_len));
    });
    const bool has_token =
        buffer.get().find("accessToken") != std::string::npos ||
        buffer.get().find("password") != std::string::npos;
    auto data = buffer.copy(in_len);
    logger_.info([&]() {
      return std::string("HTTP ")  //
          .append(type)
          .append(" body: ")
          .append(has_token ? mask_password(buffer.get()) : buffer.get());
    });
  }
}

Handler::HttpResult RestRequestHandler::handle_request_impl(
    RestHandler *handler, RequestContext &ctxt) {
  // Debug handlers do not have auth_manager.
  if (auth_manager_) {
    ctxt.sql_session_cache = auth_manager_->get_cache()->get_empty(
        collector::kMySQLConnectionMetadataRO, false);
  }

  const auto service_id = handler->get_service_id();
  const auto method = ctxt.request->get_method();

  std::string full_service_path =
      handler->get_url_host() + handler->get_service_path();

  logger_.debug([&]() {
    return std::string("handle_request(service_id:")
        .append(service_id.to_string())
        .append("): start(method:")
        .append(get_http_method_name(method))
        .append(" url:'")
        .append(ctxt.request->get_uri().join())
        .append("')");
  });

  auto options = handler->get_options();
  const auto &ih = ctxt.request->get_input_headers();
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

  trace_http("Request", options.debug.http.request, method,
             ctxt.request->get_uri().join(), ih,
             ctxt.request->get_input_buffer());

  for (const auto &kv : handler->get_options().parameters_) {
    if (mysql_harness::make_lower(kv.first) == "access-control-allow-origin") {
      if (handler->get_options().allowed_origins.type !=
          mrs::interface::Options::AllowedOrigins::AllowNone) {
        continue;
      }
    }
    oh.add(kv.first.c_str(), kv.second.c_str());
  }

  auto origin = ih.find_cstr("Origin");

  if (origin) {
    using AO = mrs::interface::Options::AllowedOrigins;

    auto &ao = handler->get_options().allowed_origins;

    switch (ao.type) {
      case AO::AllowAll:
        oh.add("Access-Control-Allow-Origin", origin);
        break;
      case AO::AllowSpecified:
        if (mysql_harness::utility::container::has(ao.allowed_origins, origin))
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
      if ((get_access_right_from_http_method(method) &
           handler->get_access_rights()) ||
          HttpMethod::Options == method) {
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

  if (!handler->request_begin(&ctxt)) {
    logger_.debug("'request_begin' returned false");
    throw http::Error{HttpStatusCode::Forbidden};
  }

  auto required_access = get_access_right_from_http_method(method);
  if (!(required_access & handler->get_access_rights())) {
    logger_.debug([&]() {
      return std::string("'required_access' denied, required_access:")
          .append(std::to_string(required_access))
          .append(", access:")
          .append(std::to_string(handler->get_access_rights()));
    });
    throw http::Error{HttpStatusCode::Forbidden};
  }

  auto required_auth = handler->requires_authentication();
  if (Handler::Authorization::kNotNeeded != required_auth) {
    logger_.debug([&]() {
      return std::string("RestRequestHandler(service_id:")
          .append(service_id.to_string())
          .append("): authenticate");
    });

    // request_ctxt.user is valid after success of this call
    if (Handler::Authorization::kRequires == required_auth) {
      try {
        if (!auth_manager_->authorize(
                handler->get_protocol(), handler->get_url_host(), service_id,
                handler->get_options().query.passthrough_db_user, ctxt,
                &ctxt.user)) {
          logger_.debug("Authentication handler fails");
          throw http::Error(HttpStatusCode::Unauthorized);
        }
      } catch (const Handler::HttpResult &force_result) {
        if (handler->get_options().debug.log_exceptions)
          trace_error(force_result);
        return force_result;
      }

      logger_.debug("Authentication handler ok.");
    } else {
      // Just check the user
      auth_manager_->is_authorized(service_id, ctxt, &ctxt.user);
    }

    handler->authorization(&ctxt);

    if (handler->may_check_access()) {
      logger_.debug([&]() {
        return std::string("RestRequestHandler(service_id:")
            .append(service_id.to_string())
            .append("): required_access:")
            .append(std::to_string(required_access));
      });
      if (!(required_access &
            do_privilege_check(ctxt.user.privileges, handler->get_service_id(),
                               full_service_path, handler->get_schema_id(),
                               handler->get_schema_path(),
                               handler->get_db_object_id(),
                               handler->get_db_object_path()))) {
        throw http::Error{HttpStatusCode::Forbidden};
      }
    }
  }

  logger_.debug([&]() {
    return std::string("RestRequestHandler(service_id:")
        .append(service_id.to_string())
        .append(")::")
        .append("dispatch(method:")
        .append(get_http_method_name(ctxt.request->get_method()))
        .append(", path:")
        .append(ctxt.request->get_uri().get_path())
        .append(")");
  });

  switch (method) {
    case HttpMethod::Get:
      return handler->handle_get(&ctxt);
      break;

    case HttpMethod::Post: {
      auto &input_buffer = ctxt.request->get_input_buffer();
      auto size = input_buffer.length();
      return handler->handle_post(&ctxt, input_buffer.pop_front(size));
    } break;

    case HttpMethod::Delete:
      return handler->handle_delete(&ctxt);
      break;

    case HttpMethod::Put: {
      auto response = handler->handle_put(&ctxt);
      return response;
      break;
    }

    default:
      throw http::Error{HttpStatusCode::MethodNotAllowed};
  }
}

void RestRequestHandler::handle_request(::http::base::Request &req) {
  RequestContext request_ctxt{&req, auth_manager_};
  auto handler = rest_handler_.lock();

  if (!handler) {
    send_rfc7807_error(req, HttpStatusCode::GatewayTimeout, {});
    return;
  }

  try {
    auto result = handle_request_impl(handler.get(), request_ctxt);

    auto &b = req.get_output_buffer();
    auto &out_hdrs = req.get_output_headers();

    b.add(result.response.c_str(), result.response.length());

    if (!result.etag.empty()) {
      out_hdrs.add("Cache-Control", "no-cache");
      out_hdrs.add("ETag", result.etag.c_str());
    }

    out_hdrs.add("Content-Type",
                 get_content_type(result.type, result.type_text));

    handler->request_end(&request_ctxt);
    send_reply(req, result.status,
               HttpStatusCode::get_default_status_text(result.status), b);
  } catch (const http::ErrorChangeResponse &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    if (e.retry()) {
      logger_.debug("handle_request override");
      auto r = e.change_response(&req);
      send_reply(req, r.status, r.message);
    } else
      handle_error(handler.get(), &request_ctxt, e.change_response(&req));
  } catch (const http::Error &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  } catch (const mrs::database::JSONInputError &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  } catch (const RestError &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  } catch (const ETagMismatch &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  } catch (const std::invalid_argument &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  } catch (const std::exception &e) {
    if (handler->get_options().debug.log_exceptions) trace_error(e);
    handle_error(handler.get(), &request_ctxt, e);
  }
}

http::Error RestRequestHandler::err_to_http_error(
    const mysqlrouter::MySQLSession::Error &err) {
  if (ER_GTID_MODE_OFF == err.code()) {
    return {HttpStatusCode::BadRequest,
            "'Asof' requirement was not fulfilled, GTID_MODE is not "
            "configured properly on the MySQL Server."};
  } else if (ER_WRONG_VALUE == err.code()) {
    // Its safe to forward this message:
    //     ER_WRONG_VALUE_MSG
    //     "Incorrect %-.32s value: \'%-.128s\'"
    return {HttpStatusCode::BadRequest, err.message()};
  }

  return {HttpStatusCode::InternalError};
}

template <typename Err>
void RestRequestHandler::handle_error(RestHandler *handler,
                                      RequestContext *ctxt, const Err &err) {
  const http::Error &e = err_to_http_error(err);
  logger_.debug([&]() {
    std::string msg;
    for (const auto &it : responose_encode_error(e, err)) {
      msg += " " + it.first + "=" + it.second;
    }
    return "handle_error" + msg;
  });
  if (!handler->request_error(ctxt, e)) {
    switch (e.status) {
      case HttpStatusCode::Ok:
        [[fallthrough]];
      case HttpStatusCode::NotModified:
        [[fallthrough]];
      case HttpStatusCode::TemporaryRedirect:
        [[fallthrough]];
      case HttpStatusCode::PermanentRedirect:
        send_reply(*ctxt->request, e.status, e.message);
        break;
      case HttpStatusCode::Unauthorized:
        if (ctxt->selected_handler && ctxt->session) {
          auth_manager_->unauthorize(ctxt->session, &ctxt->cookies);
        }
        [[fallthrough]];
      default:
        if (handler->get_options().debug.http.response.detailed_errors_ &&
            may_log_requests_)
          send_rfc7807_error(*ctxt->request, e.status,
                             responose_encode_error(e, err));
        else
          send_rfc7807_error(*ctxt->request, e.status,
                             responose_encode_error(e, e));
    }
  }
}

const mrs::interface::Options &RestRequestHandler::get_options() {
  const static mrs::interface::Options default_options;
  auto h = rest_handler_.lock();

  if (h) return h->get_options();

  return default_options;
}

void RestRequestHandler::send_reply(HttpRequest &req, int status_code) {
  auto options = get_options();
  if (options.debug.http.response.body_) {
    logger_.debug([status_code]() {
      return std::string("HTTP Response status: ")
          .append(std::to_string(status_code));
    });
  }

  trace_http("Response", options.debug.http.response, req.get_method(), "",
             req.get_output_headers(), req.get_output_buffer());
  req.send_reply(status_code);
}

void RestRequestHandler::send_reply(HttpRequest &req, int status_code,
                                    const std::string &status_text) {
  auto options = get_options();
  if (options.debug.http.response.body_) {
    logger_.debug([&]() {
      return std::string("HTTP Response status: ")
          .append(std::to_string(status_code));
    });

    logger_.debug([&]() {
      return std::string("HTTP Response status text: ")  //
          .append(status_text);
    });
  }
  trace_http("Response", options.debug.http.response, req.get_method(), "",
             req.get_output_headers(), req.get_output_buffer());
  req.send_reply(status_code, status_text);
}

void RestRequestHandler::send_reply(HttpRequest &req, int status_code,
                                    const std::string &status_text,
                                    HttpBuffer &buffer) {
  auto options = get_options();
  if (options.debug.http.response.body_) {
    logger_.debug([&]() {
      return std::string("HTTP Response status: ")
          .append(std::to_string(status_code));
    });
    logger_.debug([&]() {
      return std::string("HTTP Response status text: ").append(status_text);
    });
  }
  trace_http("Response", options.debug.http.response, req.get_method(), "",
             req.get_output_headers(), buffer);
  req.send_reply(status_code, status_text, buffer);
}

void RestRequestHandler::send_rfc7807_error(
    HttpRequest &req, HttpStatusCode::key_type status_code,
    const std::map<std::string, std::string> &fields) {
  auto &out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/problem+json");

  rapidjson::Document json_doc;

  auto &allocator = json_doc.GetAllocator();

  json_doc.SetObject();
  for (const auto &field : fields) {
    json_doc.AddMember(
        rapidjson::Value(field.first.c_str(), field.first.size(), allocator),
        rapidjson::Value(field.second.c_str(), field.second.size(), allocator),
        allocator);
  }

  json_doc.AddMember("status", status_code, allocator);

  send_json_document(req, status_code, json_doc);
}

void RestRequestHandler::send_json_document(
    HttpRequest &req, HttpStatusCode::key_type status_code,
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

std::string RestRequestHandler::mask_password(const std::string &input) {
  mysqlrouter::LogFilter log_filter;
  // It allows for \" and \\ inside of the password, so that we do not stop
  // matching password if it contains such characters
  for (const std::string keyword : {R"("password")", R"("accessToken")"}) {
    log_filter.add_pattern(keyword + R"(\s*:\s*"([^"\\]|\\.)*")",
                           keyword + R"(: "*****")");
  }

  return log_filter.filter(input);
}

}  // namespace rest
}  // namespace mrs
