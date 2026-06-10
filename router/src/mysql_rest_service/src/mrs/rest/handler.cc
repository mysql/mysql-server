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

#include "mrs/rest/handler.h"

#include <memory>
#include <string>
#include <utility>

#include "mrs/rest/rest_request_handler.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/component/http_server_component.h"

#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

uint32_t Handler::check_privileges(
    const std::vector<database::entry::AuthPrivilege> &privileges,
    const UniversalId &service_id, const std::string &service_path,
    const UniversalId &schema_id, const std::string &schema_path,
    const UniversalId &db_object_id, const std::string &db_object_path) {
  return do_privilege_check(privileges, service_id, service_path, schema_id,
                            schema_path, db_object_id, db_object_path);
}

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

template <typename ValueType>
double to_double(const ValueType &value) {
  const auto &v = cvt::to_string(value);
  return std::stod(v.c_str());
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
    } else if (key == "passthroughDbUser") {
      result_.query.passthrough_db_user = to_bool(vt);
    } else if (key == "sqlQuery.wait") {
      result_.query.wait = to_uint(vt);
    } else if (key == "sqlQuery.embedWait") {
      result_.query.embed_wait = to_bool(vt);
    } else if (key == "sqlQuery.timeout") {
      result_.query.timeout = to_uint(vt);
    } else if (key == "http.allowedOrigin") {
      if (mysql_harness::make_lower(cvt::to_string(vt)) == "auto")
        result_.allowed_origins.type = Result::AllowedOrigins::AllowAll;
      else
        result_.allowed_origins.allowed_origins.push_back(cvt::to_string(vt));
    } else if (key == "result.includeLinks") {
      result_.result.include_links = to_bool(vt);
    } else if (key == "result.cacheTimeToLive") {
      result_.result.cache_ttl_ms = to_double(vt) * 1000;
    } else if (key == "mysqlTask.name") {
      result_.mysql_task.name = cvt::to_string(vt);
    } else if (key == "mysqlTask.eventSchema") {
      result_.mysql_task.event_schema = cvt::to_string(vt);
    } else if (key == "mysqlTask.driver") {
      auto driver = mysql_harness::make_lower(cvt::to_string(vt));
      if (driver == "database") {
        result_.mysql_task.driver =
            mrs::interface::Options::MysqlTask::DriverType::kDatabase;
      } else if (driver == "router") {
        result_.mysql_task.driver =
            mrs::interface::Options::MysqlTask::DriverType::kRouter;
      } else {
        log_warning("Invalid driver type '%s' for option '%s'", driver.c_str(),
                    key.c_str());
        result_.mysql_task.driver =
            mrs::interface::Options::MysqlTask::DriverType::kNone;
      }
    } else if (key == "mysqlTask.monitoringSql") {
      result_.mysql_task.monitoring_sql.push_back(cvt::to_string(vt));
    }
  }

  template <typename ValueType>
  void handle_array_value(const std::string &key, const ValueType &vt) {
    using std::to_string;
    if (key == "http.allowedOrigin") {
      result_.allowed_origins.type = Result::AllowedOrigins::AllowSpecified;
      result_.allowed_origins.allowed_origins.push_back(cvt::to_string(vt));
    } else if (key == "mysqlTask.monitoringSql.monitoringSql") {
      result_.mysql_task.monitoring_sql.push_back(cvt::to_string(vt));
    }
  }

  template <typename ValueType>
  void handle_value(const ValueType &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      if (key == "mysqlTask.statusDataJsonSchema")
        result_.mysql_task.status_data_json_schema = cvt::to_string(vt);
      else
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

mrs::interface::Options parse_json_options(
    const std::optional<std::string> &options) {
  if (!options.has_value()) return {};

  // Ignore parse errors here, as there may be multiple handlers defined for the
  // same JSON configuration. Error reporting will be handled by the parent
  // object, such as Endpoint.
  auto result = helper::json::text_to_handler<ParseOptions>(options.value());

  return result.value_or(ParseOptions::Result{});
}

Handler::Handler(const Protocol protocol, const std::string &url_host,
                 const std::vector<UriPathMatcher> &rest_path_matcher,
                 const std::optional<std::string> &options,
                 mrs::interface::AuthorizeManager *auth_manager)
    : options_{parse_json_options(options)},
      url_host_{url_host},
      rest_path_matcher_{rest_path_matcher},
      authorization_manager_{auth_manager},
      protocol_{protocol == endpoint::handler::k_protocolHttp ? "http"
                                                              : "https"},
      log_level_is_debug_(mysql_harness::logging::log_level_is_handled(
          mysql_harness::logging::LogLevel::kDebug)),
      log_level_is_info_(mysql_harness::logging::log_level_is_handled(
          mysql_harness::logging::LogLevel::kInfo)) {
  if (log_level_is_debug_) {
    for (const auto &kv : options_.parameters_) {
      log_debug("headers: '%s':'%s'", kv.first.c_str(), kv.second.c_str());
    }
    log_debug("debug.log_exceptions: %s",
              to_cstr(options_.debug.log_exceptions));
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
  }
}

Handler::~Handler() {
  if (log_level_is_debug_ || log_level_is_info_) {
    for (const auto &path : rest_path_matcher_) {
      if (log_level_is_info_) {
        log_info(
            "Removing Url-Handler that processes requests on host: '%s' and "
            "path that matches path: '%s'",
            url_host_.c_str(), path.path.c_str());
      }
      if (log_level_is_debug_) {
        log_debug("route-remove: '%s' on host '%s'", path.path.c_str(),
                  url_host_.c_str());
      }
    }
  }

  assert(!handler_id_.empty() && "initialize() was not called.");

  for (auto id : handler_id_) {
    HttpServerComponent::get_instance().remove_route(id);
  }
}

void Handler::initialize(const Configuration &configuration) {
  const bool may_log_requests = configuration.may_log_request();

  for (auto &path : rest_path_matcher_) {
    auto handler = std::make_unique<RestRequestHandler>(
        weak_from_this(), authorization_manager_, may_log_requests);

    if (log_level_is_debug_) {
      log_debug("route-add: '%s' on host '%s'", path.path.c_str(),
                url_host_.c_str());
    }

    if (log_level_is_info_) {
      log_info(
          "Adding Url-Handler that processes requests on host '%s' and path "
          "that matches: '%s'",
          url_host_.c_str(), path.path.c_str());
    }

    handler_id_.emplace_back(
        HttpServerComponent::get_instance().add_direct_match_route(
            url_host_, path, std::move(handler)));
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

const std::string &Handler::get_url_host() const { return url_host_; }

const std::string &Handler::get_protocol() const { return protocol_; }

bool Handler::may_check_access() const { return true; }

const std::string &Handler::empty_path() const {
  static std::string k_empty;
  return k_empty;
}

}  // namespace rest
}  // namespace mrs
