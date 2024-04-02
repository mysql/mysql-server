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

#include "mrs/authentication/authorize_manager.h"

#include <time.h>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>

#include "my_macros.h"

#include "mrs/authentication/auth_handler_factory.h"
#include "mrs/authentication/track_authorize_handler.h"
#include "mrs/authentication/www_authentication_handler.h"
#include "mrs/http/error.h"
#include "mrs/rest/handler_authorize.h"
#include "mrs/rest/handler_authorize_apps.h"
#include "mrs/rest/handler_authorize_ok.h"
#include "mrs/rest/handler_is_authorized.h"
#include "mrs/rest/handler_unauthorize.h"
#include "mrs/rest/handler_user.h"
#include "mrs/rest/request_context.h"

#include "helper/container/generic.h"
#include "helper/container/map.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "helper/make_shared_ptr.h"
#include "helper/string/random.h"
#include "helper/string/replace.h"
#include "helper/token/jwt.h"

#include "http/base/headers.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
using Headers = ::http::base::Headers;
using JwtHolder = helper::JwtHolder;
using Jwt = helper::Jwt;
using Handlers = AuthorizeManager::AuthHandlers;
using AuthorizeHandlerPtr = AuthorizeManager::AuthorizeHandlerPtr;

const UniversalId k_vendor_mrs{{0x30, 0}};
const UniversalId k_vendor_mysql{{0x31, 0}};
const UniversalId k_vendor_facebook{{0x32, 0}};
const UniversalId k_vendor_twitter{{0x33, 0}};
const UniversalId k_vendor_google{{0x34, 0}};

namespace {

class AuthenticationOptions {
 public:
  std::optional<uint64_t> host_requests_per_minute_{};
  std::optional<milliseconds> host_minimum_time_between_requests{};
  std::optional<uint64_t> account_requests_per_minute_{};
  std::optional<milliseconds> account_minimum_time_between_requests;
  seconds block_for{60};
};

class ParseAuthenticationOptions
    : public helper::json::RapidReaderHandlerToStruct<AuthenticationOptions> {
 public:
  template <typename ValueType>
  uint64_t to_uint(const ValueType &value) {
    return std::stoull(value.c_str());
  }

  template <typename ValueType>
  void handle_object_value(const std::string &key, const ValueType &vt) {
    using std::to_string;
    if (key ==
        "authentication.throttling.perAccount.minimumTimeBetweenRequestsInMs") {
      result_.account_minimum_time_between_requests = milliseconds{to_uint(vt)};
    } else if (key ==
               "authentication.throttling.perAccount."
               "maximumAttemptsPerMinute") {
      result_.account_requests_per_minute_ = to_uint(vt);
    } else if (key ==
               "authentication.throttling.perHost."
               "minimumTimeBetweenRequestsInMs") {
      result_.host_minimum_time_between_requests = milliseconds{to_uint(vt)};
    } else if (key ==
               "authentication.throttling.perHost."
               "maximumAttemptsPerMinute") {
      result_.host_requests_per_minute_ = to_uint(vt);
    } else if (key ==
               "authentication.throttling.blockWhenAttemptsExceededInSeconds") {
      result_.block_for = seconds{to_uint(vt)};
    }
  }

  template <typename ValueType>
  void handle_value(const ValueType &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      handle_object_value(key, vt);
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
    const static std::string k_true{"true"}, k_false{"false"};
    handle_value(v ? k_true : k_false);
    return true;
  }
};

auto parse_json_options(const std::string &options) {
  return helper::json::text_to_handler<ParseAuthenticationOptions>(options);
}

class UserIdContainer {
 public:
  using UserId = database::entry::AuthUser::UserId;
  auto begin() const { return std::begin(user_id_.raw); }
  auto end() const { return std::end(user_id_.raw); }
  void push_back(uint8_t value) { user_id_.raw[push_index_++] = value; }
  auto get_user_id() const { return user_id_; }

 private:
  UserId user_id_;
  uint64_t push_index_{0};
};

void throw_max_rate_exceeded(milliseconds ms) {
  std::string v;
  auto s = std::chrono::duration_cast<seconds>(ms);
  v = (s.count() == 0) ? "1" : std::to_string(s.count());

  throw http::ErrorWithHttpHeaders(HttpStatusCode::TooManyRequests,
                                   {{"Retry-After", v}});
}

std::string get_peer_host(rest::RequestContext &ctxt) {
  return ctxt.request->get_connection()->get_peer_address();
}

Jwt get_bearer_token_jwt(const Headers &headers) {
  auto authorization =
      headers.find_cstr(WwwAuthenticationHandler::kAuthorization);

  if (!authorization) return {};

  auto args = mysql_harness::split_string(authorization, ' ', false);
  std::string value = args.size() > 1 ? args[1] : "";

  log_debug("authorization: \"%s\"", authorization);

  JwtHolder holder;

  try {
    helper::Jwt::parse(value, &holder);

    auto jwt = helper::Jwt::create(holder);
    return jwt;
  } catch (const std::exception &e) {
    log_debug("JWT failure: %s.", e.what());
  }
  return {};
}

std::string get_session_cookie_key_name(const AuthorizeManager::ServiceId id) {
  using namespace std::literals::string_literals;
  return "session_"s + id.to_string();
}

}  // namespace

AuthorizeManager::AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                                   const std::string &jwt_secret,
                                   AuthHandlerFactoryPtr factory)
    : cache_manager_{cache_manager},
      jwt_secret_{jwt_secret},
      factory_{factory},
      random_data_{
          helper::generate_string<64, helper::Generator8bitsValues>()} {
  log_info("JWT bearer authorization disabled, the signing secret is empty.");
}

AuthorizeManager::AuthorizeManager(collector::MysqlCacheManager *cache_manager,
                                   const std::string &jwt_secret)
    : cache_manager_{cache_manager},
      jwt_secret_{jwt_secret},
      factory_{std::make_shared<AuthHandlerFactory>()} {}

void AuthorizeManager::configure(const std::string &options) {
  auto cnf = parse_json_options(options);
  accounts_rate_ = RateControlFor<std::string>(
      cnf.account_requests_per_minute_, cnf.block_for,
      cnf.account_minimum_time_between_requests);
  hosts_rate_ =
      RateControlFor<std::string>(cnf.host_requests_per_minute_, cnf.block_for,
                                  cnf.host_minimum_time_between_requests);
}

void AuthorizeManager::update(const Entries &entries) {
  Container::iterator it;

  if (entries.size()) {
    log_debug("auth_app: Number of updated entries:%i", (int)entries.size());
  }

  for (const auto &e : entries) {
    log_debug("auth_app: Processing update of id=%s", e.id.to_string().c_str());
    auto auth = make_auth(e);

    if (get_handler_by_id(e.id, &it)) {
      *it = auth;

      if (!auth) container_.erase(it);
    } else {
      if (auth) container_.push_back(auth);
    }
  }
}

AuthorizeManager::Container AuthorizeManager::get_handlers_by_service_id(
    const UniversalId service_id) {
  Container out_result;

  helper::container::copy_if(
      container_,
      [service_id](auto &element) {
        return element->get_service_id() == service_id;
      },
      out_result);

  return out_result;
}

bool AuthorizeManager::get_handler_by_id(const UniversalId auth_id,
                                         Container::iterator *out_it) {
  auto it = std::find_if(container_.begin(), container_.end(),
                         [auth_id](auto &i) { return i->get_id() == auth_id; });

  if (it != container_.end()) {
    *out_it = it;
    return true;
  }

  return false;
}

bool AuthorizeManager::get_handler_by_id(const UniversalId auth_id,
                                         AuthorizeHandlerPtr &out_it) {
  Container::iterator it;
  if (get_handler_by_id(auth_id, &it)) {
    out_it = *it;
    return true;
  }
  return false;
}

AuthorizeHandlerPtr AuthorizeManager::make_auth(const AuthApp &entry) {
  // TODO(lkotula): Rework this (Shouldn't be in review)
  AuthorizeHandlerPtr result;

  if (entry.deleted) return {};

  if (!entry.active) return {};

  if (entry.vendor_id == k_vendor_mysql)
    result = factory_->create_basic_auth_handler(this, entry, cache_manager_);
  else if (entry.vendor_id == k_vendor_facebook)
    result = factory_->create_facebook_auth_handler(this, entry);
  else if (entry.vendor_id == k_vendor_twitter)
    result = factory_->create_twitter_auth_handler(this, entry);
  else if (entry.vendor_id == k_vendor_google)
    result = factory_->create_google_auth_handler(this, entry);
  else if (entry.vendor_id == k_vendor_mrs)
    result = factory_->create_scram_auth_handler(this, entry, random_data_);

  return result;
}

void AuthorizeManager::fill_service(const AuthApp &e, ServiceAuthorize &sa) {
  std::string auth_path =
      !e.auth_path.empty() ? e.auth_path : "/authentication";
  std::string path1 = "^" + e.service_name + auth_path + "/login$";
  std::string path2 = "^" + e.service_name + auth_path + "/status$";
  std::string path3 = "^" + e.service_name + auth_path + "/logout$";
  std::string path4 = "^" + e.service_name + auth_path + "/completed";
  std::string path5 = "^" + e.service_name + auth_path + "/user";
  std::string path6 = "^" + e.service_name + auth_path + "/authApps$";
  std::string redirect = e.redirect;

  if (redirect.empty()) {
    redirect = e.host + e.service_name + auth_path + "/completed";
  }

  auto login_handler = std::make_shared<mrs::rest::HandlerAuthorize>(
      e.service_id, e.service_name, path1, e.options, redirect, this);
  auto status_handler = std::make_shared<mrs::rest::HandlerIsAuthorized>(
      e.service_id, e.service_name, path2, e.options, this);
  auto unauth_handler = std::make_shared<mrs::rest::HandlerUnauthorize>(
      e.service_id, e.service_name, path3, e.options, this);
  auto auth_ok_handler = std::make_shared<mrs::rest::HandlerAuthorizeOk>(
      e.service_id, e.service_name, path4, e.options,
      e.redirection_default_page, this);
  auto user_handler = std::make_shared<mrs::rest::HandlerUser>(
      e.service_id, e.service_name, path5, e.options, this);
  auto list_handler = std::make_shared<mrs::rest::HandlerAuthorizeApps>(
      e.service_id, e.service_name, path6, e.options, redirect, this);

  sa.authorize_handler_ = login_handler;
  sa.status_handler_ = status_handler;
  sa.unauthorize_handler_ = unauth_handler;
  sa.authorization_result_handler_ = auth_ok_handler;
  sa.user_handler_ = user_handler;
  sa.list_handler_ = list_handler;
}

void AuthorizeManager::acquire(interface::AuthorizeHandler *handler) {
  ServiceAuthorizePtr out_service_authorization;
  auto lock = std::unique_lock(service_authorize_mutext_);

  if (!helper::container::get_value(service_authorize_,
                                    handler->get_service_id(),
                                    &out_service_authorization)) {
    helper::MakeSharedPtr<ServiceAuthorize> service_authorization;

    fill_service(handler->get_entry(), *service_authorization);

    service_authorize_[handler->get_service_id()] =
        service_authorization.copy_base();
    return;
  } else {
    fill_service(handler->get_entry(), *out_service_authorization);
  }
  ++out_service_authorization->references_;
}

void AuthorizeManager::destroy(interface::AuthorizeHandler *handler) {
  auto lock = std::unique_lock(service_authorize_mutext_);
  auto &el = service_authorize_[handler->get_service_id()];

  if (0 == --el->references_) {
    service_authorize_.erase(handler->get_service_id());
  }
}

void AuthorizeManager::pre_authorize_account(
    interface::AuthorizeHandler *handler, const std::string &account) {
  auto unique_account_name = handler->get_id().to_string() + account;
  AcceptInfo ac;
  if (!accounts_rate_.allow(unique_account_name, &ac)) {
    if (ac.reason == BlockReason::kRateExceeded) {
      log_debug("Too many requests from user: '%s' for handler:%s.",
                account.c_str(), handler->get_id().to_string().c_str());
    }
    throw_max_rate_exceeded(ac.next_request_allowed_after);
  }
}

bool AuthorizeManager::unauthorize(ServiceId service_id,
                                   http::Cookie *cookies) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = cookies->get(session_cookie_key);

  if (session_identifier.empty()) return false;

  return session_manager_.remove_session(session_identifier);
}

static std::string current_timestamp(std::chrono::system_clock::duration d) {
  auto now = std::chrono::system_clock::now() + d;
  std::time_t tt = std::chrono::system_clock::to_time_t(now);

  std::ostringstream os;
  os << std::put_time(gmtime(&tt), "%F %T");

  return os.str();
}

static bool is_timestamp_in_past(const std::string ts) {
  std::tm t = {};
  std::istringstream ss(ts);
  ss >> std::get_time(&t, "%Y-%m-%d %T");
  if (ss.fail()) return true;

  auto past = IF_WIN(_mkgmtime, timegm)(&t);
  auto current =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  if (past == -1 || current == -1) return true;

  if (difftime(current, past) <= 0.0) return false;

  return true;
}

template <typename Document>
void doc_set_member(Document &doc, std::string_view name,
                    std::string_view value) {
  rapidjson::Value jname{name.data(), name.size(), doc.GetAllocator()};
  rapidjson::Value jvalue{value.data(), value.size(), doc.GetAllocator()};
  doc.AddMember(jname, jvalue, doc.GetAllocator());
}

std::string AuthorizeManager::get_jwt_token(UniversalId service_id,
                                            Session *s) {
  using namespace rapidjson;
  Document payload;
  auto exp = current_timestamp(session_manager_.get_timeout());
  auto user_id_hex = helper::string::hex(s->user.user_id.raw);

  payload.SetObject();
  doc_set_member(payload, "user_id", user_id_hex);

  if (!s->user.email.empty()) doc_set_member(payload, "email", s->user.email);

  doc_set_member(payload, "exp", exp);
  doc_set_member(
      payload, "service_id",
      std::string_view{reinterpret_cast<const char *>(service_id.raw),
                       service_id.k_size});

  auto jwt = helper::Jwt::create("HS256", payload);

  auto token = jwt.sign(jwt_secret_);

  std::string session_id =
      service_id.to_string() + "." + s->user.user_id.to_string() + "." + exp;
  if (session_manager_.get_session(session_id)) return token;

  auto session = session_manager_.new_session(session_id);
  session->user = s->user;
  session->state = http::SessionManager::Session::kUserVerified;

  return token;
}

AuthorizeManager::Session *AuthorizeManager::get_current_session(
    ServiceId id, const HttpHeaders &input_headers, http::Cookie *cookies) {
  auto session_cookie_key = get_session_cookie_key_name(id);
  auto session_identifier = cookies->get(session_cookie_key);

  if (session_identifier.empty()) {
    if (!jwt_secret_.empty()) {
      auto jwt = get_bearer_token_jwt(input_headers);

      session_identifier = authorize(id, jwt);
      if (session_identifier.empty()) return nullptr;
    }
  }

  auto session = session_manager_.get_session(session_identifier);
  log_debug("Current session state:%i", session ? session->state : -1);
  return session;
}

AuthorizeManager::Container
AuthorizeManager::get_supported_authentication_applications(ServiceId id) {
  return get_handlers_by_service_id(id);
}

std::string AuthorizeManager::authorize(const UniversalId service_id,
                                        const helper::Jwt &jwt) {
  log_debug("Validating JWT token: %s", jwt.get_token().c_str());
  if (!jwt.is_valid()) {
    log_debug("JWT token is invalid");
    return {};
  }
  // We allow HS256, still it doesn't change much because
  // Jws supports only HS256 and none (just blocking use of none).
  if (jwt.get_header_claim_algorithm() != "HS256") {
    log_debug("JWT token not supported algorithm");
    return {};
  }
  if (!jwt.verify(jwt_secret_)) {
    log_debug("JWT token verification failed");
    return {};
  }

  auto claims = jwt.get_payload_claim_names();
  if (!helper::container::has(claims, "user_id")) return {};
  if (!helper::container::has(claims, "exp")) return {};
  if (!helper::container::has(claims, "service_id")) return {};

  auto json_uid = jwt.get_payload_claim_custom("user_id");
  auto json_exp = jwt.get_payload_claim_custom("exp");
  auto json_sid = jwt.get_payload_claim_custom("service_id");

  if (!json_uid->IsString()) return {};
  log_debug("JWT token  supported algorithm");
  if (!json_exp->IsString()) return {};
  if (!json_sid->IsString()) return {};

  // TODO(lkotula): Change from_raw ? (Shouldn't be in review)
  auto user_id = helper::string::unhex<UserIdContainer>(json_uid->GetString())
                     .get_user_id();
  auto exp = json_exp->GetString();
  auto sid = UniversalId::from_cstr(json_sid->GetString(),
                                    json_sid->GetStringLength());

  if (sid != service_id) {
    log_debug("Wrong service id.");
    return {};
  }

  if (is_timestamp_in_past(exp)) {
    log_debug("Token expired.");
    return {};
  }

  std::string session_id = user_id.to_string() + "." + exp;
  if (session_manager_.get_session(session_id)) {
    log_debug("Session for token already exsits: %s", session_id.c_str());
    return session_id;
  }

  auto session = session_manager_.new_session(session_id);

  auto instance = cache_manager_->get_instance(
      collector::kMySQLConnectionMetadataRW, false);
  if (user_manager_.user_get_by_id(user_id, &session->user, &instance)) {
    log_debug("Found user %s", user_id.to_string().c_str());
    session->state = http::SessionManager::Session::kUserVerified;
    return session_id;
  }
  log_debug("User not found");
  return {};
}

AuthorizeHandlerPtr AuthorizeManager::choose_authentication_handler(
    ServiceId service_id, const std::string &app_name) {
  auto handlers = get_handlers_by_service_id(service_id);
  if (handlers.empty())
    throw http::Error{
        HttpStatusCode::BadRequest,
        "Bad request - there is no authorization application available"};

  if (app_name.empty() && handlers.size() == 1) {
    return handlers[0];
  }

  AuthorizeHandlerPtr result;
  if (!helper::container::get_if(
          handlers,
          [&app_name](const auto &handler) {
            return (app_name == handler->get_entry().app_name);
          },
          &result))
    throw http::Error{
        HttpStatusCode::BadRequest,
        "Bad request - chosen authorization application no available"};
  return result;
}

bool AuthorizeManager::authorize(ServiceId service_id,
                                 rest::RequestContext &ctxt,
                                 AuthUser *out_user) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = ctxt.cookies.get(session_cookie_key);
  auto url = ctxt.get_http_url();
  log_debug(
      "AuthorizeManager::authorize(service_id:%s, session_id:%s, "
      "can_use_jwt:%s)",
      service_id.to_string().c_str(), session_identifier.c_str(),
      (jwt_secret_.empty() ? "no" : "yes"));

  AuthorizeHandlerPtr selected_handler;

  bool generate_jwt_token = url.get_query_parameter("sessionType") == "bearer";
  if (generate_jwt_token) url.remove_query_parameter("sessionType");

  // TODO(lkotula): Change this hack (Shouldn't be in review)
  if (ctxt.request->get_method() == HttpMethod::Post &&
      session_identifier.empty()) {
    auto url_session_id = url.get_query_parameter("session");
    if (!url_session_id.empty()) {
      session_identifier = url_session_id;
      ctxt.cookies.direct()[session_cookie_key] = session_identifier;
    }
  }

  if (generate_jwt_token && jwt_secret_.empty()) {
    throw http::Error{HttpStatusCode::BadRequest,
                      "Bad request - bearer not allowed."};
  }

  AcceptInfo ac;
  auto peer_host = get_peer_host(ctxt);
  if (!hosts_rate_.allow(peer_host, &ac)) {
    if (ac.reason == BlockReason::kRateExceeded)
      log_warning("Too many requests from host: '%s'.", peer_host.c_str());
    throw_max_rate_exceeded(ac.next_request_allowed_after);
  }
  selected_handler =
      choose_authentication_handler(service_id, url.get_query_parameter("app"));

  // Ensure that all code paths, had selected the handlers.
  assert(nullptr != selected_handler.get());

  ctxt.selected_handler = selected_handler;
  if (!session_identifier.empty()) {
    auto session = session_manager_.get_session(session_identifier);
    if (session) {
      if (session->get_authorization_handler_id() !=
          selected_handler->get_id()) {
        session_manager_.remove_session(session_identifier);
        session_identifier.clear();
      }
    } else {
      session_identifier.clear();
    }
  }

  using namespace std::literals::string_literals;
  Session *session;

  if (session_identifier.empty()) {
    session = session_manager_.new_session(selected_handler->get_id());
    session->generate_token = generate_jwt_token;
    http::Cookie::SameSite same_site = http::Cookie::None;
    ctxt.cookies.set(session_cookie_key, session->get_session_id(),
                     http::Cookie::duration{0}, "/", &same_site, true, true,
                     {});
    log_debug("new session id=%s", session->get_session_id().c_str());
  } else {
    session = session_manager_.get_session(session_identifier);
    if (generate_jwt_token) session->generate_token = true;
    log_debug("existing session id=%s", session_identifier.c_str());
  }

  assert(nullptr != session);
  session->handler_name = selected_handler->get_entry().app_name;

  log_debug("selected_handler::redirects(%s)",
            (selected_handler->redirects() ? "yes" : "no"));

  if (selected_handler->authorize(ctxt, session, out_user)) {
    return true;
  }

  return false;
}

users::UserManager *AuthorizeManager::get_user_manager() {
  return &user_manager_;
}

bool AuthorizeManager::is_authorized(ServiceId service_id,
                                     rest::RequestContext &ctxt,
                                     AuthUser *user) {
  auto session_cookie_key = get_session_cookie_key_name(service_id);
  auto session_identifier = ctxt.cookies.get(session_cookie_key);

  log_debug(
      "AuthorizeManager::is_authorized(service_id:%s, session_id:%s, "
      "can_use_jwt:%s)",
      service_id.to_string().c_str(), session_identifier.c_str(),
      (jwt_secret_.empty() ? "no" : "yes"));

  if (session_identifier.empty()) {
    if (!jwt_secret_.empty()) {
      auto jwt = get_bearer_token_jwt(ctxt.get_in_headers());

      session_identifier = authorize(service_id, jwt);
    }

    if (session_identifier.empty()) return false;
  }

  auto session = session_manager_.get_session(session_identifier);

  if (!session) return false;

  if (session->state == Session::kUserVerified) {
    *user = session->user;
    return true;
  }

  return false;

  //  AuthorizeHandlerPtr out_handler;
  //  if (!get_handler_by_id(session->get_authorization_handler_id(),
  //  out_handler))
  //    return false;
  //
  //  return out_handler->is_authorized(session, user);
}

void AuthorizeManager::discard_current_session(ServiceId id,
                                               http::Cookie *cookies) {
  auto session_cookie_key = get_session_cookie_key_name(id);
  auto session_identifier = cookies->get(session_cookie_key);
  session_manager_.remove_session(session_identifier);
}

}  // namespace authentication
}  // namespace mrs
