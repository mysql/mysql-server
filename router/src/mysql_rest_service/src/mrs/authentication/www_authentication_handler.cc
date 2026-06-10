/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "mrs/authentication/www_authentication_handler.h"

#include <utility>

#include "helper/container/map.h"
#include "helper/json/error.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

#include "mysqlrouter/base64.h"

#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

namespace {

struct UserJsonData {
  std::string username;
  mysql_harness::SecureString password;
};

class CredentialOptions
    : public helper::json::RapidReaderHandlerStringValuesToStruct<
          UserJsonData> {
 public:
  void handle_object_value(const std::string &key,
                           std::string &&value) override {
    using std::to_string;
    if (key == "username") {
      result_.username = std::move(value);
    } else if (key == "password") {
      result_.password = std::move(value);
    }
  }
};

}  // namespace

static bool extract_user_credentials_from_token(
    [[maybe_unused]] const std::string &token,
    [[maybe_unused]] std::string *user,
    [[maybe_unused]] mysql_harness::SecureString *password) {
  auto result = Base64::decode(token.c_str());

  auto it = std::find(result.begin(), result.end(), ':');
  if (it == result.end()) return false;

  *user = std::string(result.begin(), it);
  *password = mysql_harness::SecureString(it + 1, result.end());

  return true;
}

const static char *kBasicSchema = "basic";

using AuthApp = mrs::database::entry::AuthApp;
using AuthUser = mrs::database::entry::AuthUser;
using Session = mrs::http::SessionManager::Session;

bool WwwAuthenticationHandler::redirects(RequestContext &ctxt) const {
  log_debug("WwwAuthenticationHandler::redirects %s",
            (!ctxt.post_authentication ? "yes" : "no"));
  return !ctxt.post_authentication;
}

bool WwwAuthenticationHandler::authorize(RequestContext &ctxt,
                                         const SessionPtr &session,
                                         AuthUser *out_user) {
  log_debug("WwwAuth: Authorize user");
  if (session->state == Session::kUserVerified) {
    log_debug("WwwAuth: user already verified");
    *out_user = session->user;
    return true;
  }

  auto method = ctxt.request->get_method();
  Credentials credentials_opt;

  if (HttpMethod::Get == method)
    credentials_opt = authorize_method_get(ctxt, session.get());
  else if (HttpMethod::Post == method)
    credentials_opt = authorize_method_post(ctxt, session.get());

  if (verify_credential(credentials_opt, &ctxt.sql_session_cache, out_user)) {
    session->user = *out_user;
    session->state = Session::kUserVerified;

    init_session(session, credentials_opt);
    return true;
  }

  return false;
}

std::optional<std::string>
WwwAuthenticationHandler::get_session_id_from_request_data(RequestContext &) {
  return {};
}

static std::string find_header_or(const ::http::base::Headers &headers,
                                  const std::string &name,
                                  std::string &&default_value) {
  auto result = headers.find(name);
  if (!result) return std::move(default_value);

  return *result;
}

bool WwwAuthenticationHandler::validate_redirection_url(
    const std::optional<std::string> &redirect) {
  if (!redirect.has_value()) return true;

  const auto &v = redirect.value();
  try {
    // 1. Relative protocol check
    if (v.length() > 2 && v[0] == '/' && v[1] == '/') return false;

    // 2. Forbids %0A %0B %00 %20 etc various others
    auto u = mysqlrouter::URIParser::parse(
        redirect.value(), /*allow_path_rootless*/ true,
        /*allow_schemeless*/ true, /*path_keep_last_slash*/ true,
        /*query_single_parameter_when_cant_parse*/ true,
        /*keep_empty_root*/ true);

    // 3. Lets be strict forbid URL with credentials
    if (!u.username.empty() || !u.password.empty()) return false;

    // 4. Only know protocols, this check is complementary to 1.
    if (!u.scheme.empty()) {
      if (u.scheme != "http" && u.scheme != "https") return false;
    }

    // 5. double slashes in path & path traversing
    const auto k_count = u.path.size();
    for (size_t i = 0; i < k_count; ++i) {
      const auto &p = u.path[i];
      // Double slash is not allowed
      if (p.empty() && i != k_count - 1) return false;
      if (p == ".." || p == ".") return false;
    }

  } catch (const std::exception &) {
    return false;
  }

  return true;
}

WwwAuthenticationHandler::Credentials
WwwAuthenticationHandler::authorize_method_get(RequestContext &ctxt,
                                               Session *session) {
  auto url = ctxt.get_http_url();

  url.get_if_query_parameter("onCompletionRedirect",
                             &session->users_on_complete_url_redirection);
  url.get_if_query_parameter("onCompletionClose",
                             &session->users_on_complete_timeout);

  if (!validate_redirection_url(session->users_on_complete_url_redirection)) {
    log_debug("URL redirection is invalid.");
    // Ignore the error, but the redirection URL is not usable, lets fail the
    // authentication.
    session->users_on_complete_url_redirection.reset();

    throw http::Error{HttpStatusCode::Unauthorized};
  }

  if (ctxt.redirection_validator &&
      session->users_on_complete_url_redirection.has_value()) {
    if (!ctxt.redirection_validator->is_valid()) {
      log_debug("URL validation pattern is invalid.");
      session->users_on_complete_url_redirection.reset();
      throw http::Error{HttpStatusCode::Unauthorized};
    }

    if (!ctxt.redirection_validator->matches(
            session->users_on_complete_url_redirection.value())) {
      log_debug("URL validation pattern doesn't match '%s'.",
                session->users_on_complete_url_redirection.value().c_str());
      session->users_on_complete_url_redirection.reset();
      throw http::Error{HttpStatusCode::Unauthorized};
    }
  }

  if (session->users_on_complete_url_redirection.has_value()) {
  }

  auto authorization =
      find_header_or(ctxt.get_in_headers(), kAuthorization, "");
  if (authorization.empty()) {
    log_debug("WwwAuth: no authorization selected, retry.");
    throw_add_www_authenticate(kBasicSchema);
  }

  const auto args = mysql_harness::split_string(authorization, ' ', false);
  const std::string &auth_schema =
      mysql_harness::make_lower(args.size() > 0 ? args[0] : "");
  const std::string &auth_token = args.size() > 1 ? args[1] : "";

  if (auth_schema.empty() || auth_schema != kBasicSchema) {
    log_debug("WwwAuth: no authorization scheme, retry.");
    throw_add_www_authenticate(kBasicSchema);
  }

  if (auth_token.empty()) {
    log_debug("WwwAuth: no authorization token, retry.");
    throw_add_www_authenticate(kBasicSchema);
  }

  Credentials result;
  if (!extract_user_credentials_from_token(auth_token, &result.user,
                                           &result.password)) {
    log_debug("WwwAuth: invalid token data, retry.");
    throw_add_www_authenticate(kBasicSchema);
  }

  return result;
}

WwwAuthenticationHandler::Credentials
WwwAuthenticationHandler::authorize_method_post(RequestContext &ctxt,
                                                Session *) {
  const auto user_post_data = helper::json::text_to_handler<CredentialOptions>(
      ctxt.request->get_input_body());

  if (!user_post_data) throw helper::json::ErrorJsonParse();

  ctxt.post_authentication = true;

  return {std::move(user_post_data->username),
          std::move(user_post_data->password)};
}

const AuthApp &WwwAuthenticationHandler::get_entry() const { return entry_; }

void WwwAuthenticationHandler::throw_add_www_authenticate(const char *schema) {
  class ErrorAddWwwBasicAuth : public http::ErrorChangeResponse {
   public:
    ErrorAddWwwBasicAuth(const std::string &schema) : schema_{schema} {}

    const char *name() const override { return "ErrorAddWwwBasicAuth"; }
    bool retry() const override { return true; }
    http::Error change_response(::http::base::Request *request) const override {
      request->get_output_headers().add(kWwwAuthenticate, schema_.c_str());

      return http::Error{HttpStatusCode::Unauthorized};
    }

   private:
    const std::string schema_;
  };

  throw ErrorAddWwwBasicAuth(schema);
}

}  // namespace authentication
}  // namespace mrs
