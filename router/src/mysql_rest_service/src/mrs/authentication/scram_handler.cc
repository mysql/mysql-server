/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "mrs/authentication/scram_handler.h"

#include <limits>
#include <memory>

#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "helper/string/random.h"
#include "mrs/authentication/helper/crypto.h"
#include "mrs/authentication/helper/http_result.h"
#include "mrs/authentication/helper/option_parser.h"
#include "mrs/authentication/helper/scram.h"
#include "mrs/interface/http_result.h"
#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using SaslResult = ScramHandler::SaslResult;
using AuthenticationState = ScramHandler::AuthenticationState;
using AuthApp = mrs::database::entry::AuthApp;
using HttpResult = mrs::interface::HttpResult;
using AuthUser = mrs::database::entry::AuthUser;

class ScramSessionData : public ScramHandler::SaslSessionData {
 public:
  std::string nonce;
  std::unique_ptr<ScramParser> scram;
  KeyStoredInformations ksi;
};

static std::string string_array(const std::string &s) {
  std::string result;

  for (std::size_t i = 0; i < s.length(); ++i) {
    if (i != 0) result += ",";
    result += std::to_string((int)(unsigned char)s[i]);
  }

  return result;
}

template <typename T = uint8_t>
std::vector<T> as_vector(const std::string &v) {
  return std::vector<T>(v.begin(), v.end());
}

/**
 * Generate `nonce` character.
 *
 * The `nonce` must consist only from ASCI characters that are printable.
 */
struct GeneratorNonceCharacters : public helper::GeneratorBase {
  /**
   * Return array containing only ASCI printable characters.
   *
   * The fact is that printable characters are all in following range: 32 - 126,
   * still to have simpler code that needs no double checking, the array is
   * provides back.
   *
   * `Nonce` excludes  comma from the range.
   */
  static std::vector<char> get_prinatable_characters() {
    std::vector<char> result;

    for (int ch = std::numeric_limits<char>::min();
         ch <= std::numeric_limits<char>::max(); ch++) {
      if (isprint(ch) && ch != ',') result.push_back(ch);
    }

    return result;
  }

  static char generate() {
    static std::vector<char> printable_characters = get_prinatable_characters();
    return printable_characters[get_random_int(printable_characters.size())];
  }
};

inline AuthApp limit_users(const AuthApp &a) {
  AuthApp result{a};
  result.limit_to_registered_users = true;
  return result;
}

ScramHandler::ScramHandler(const AuthApp &entry, const std::string &random_data,
                           QueryFactory *qf, SessionManager *session_manager)
    : SaslHandler{limit_users(entry), qf},
      random_data_{random_data},
      session_manager_{session_manager} {
  log_debug("ScramHandler for service %s", to_string(entry_).c_str());
}

const std::string &ScramHandler::get_handler_name() const {
  static const std::string k_app_name{"MRS authentication application"};

  return k_app_name;
}

std::unique_ptr<ScramHandler::SessionData>
ScramHandler::allocate_session_data() {
  return std::make_unique<ScramSessionData>();
}

SaslResult ScramHandler::client_request_authentication_exchange(
    RequestContext &, Session *, AuthUser *) {
  log_debug("ScramHandler::client_request_authentication_exchange");
  return SaslResult(
      get_problem_description(HttpStatusCode::Unauthorized,
                              "Initial response required in first step."));
}

std::string ScramHandler::get_salt_for_the_user(
    const std::string &user_name) const {
  return crypto_sha256(user_name + random_data_).substr(0, 20);
}

SaslResult ScramHandler::client_initial_response(RequestContext &ctxt,
                                                 Session *session, AuthUser *,
                                                 const std::string &auth_data,
                                                 const bool is_json) {
  log_debug("ScramHandler::client_initial_response auth_data=%s",
            auth_data.c_str());

  constexpr int kServerNonceLength = 12;
  auto session_data = session->get_data<ScramSessionData>();
  session_data->scram = create_scram_parser(is_json);
  auto ireq = session_data->scram->set_initial_request(auth_data);
  session_data->nonce = ireq.nonce;

  pre_authorize_account(this, ireq.user);
  session->user = AuthUser();
  session->user.name = ireq.user;
  session->user.app_id = entry_.id;
  session->state = Session::kWaitingForCode;
  // Do not update user, because we are passing partial user entry to
  // the search procedure.
  const bool k_allow_user_update = false;

  bool found = um_.user_get(&session->user, &ctxt.sql_session_cache,
                            k_allow_user_update);

  if (!found) {
    session->user.name.clear();
    session->user.email = ireq.user;
    found = um_.user_get(&session->user, &ctxt.sql_session_cache,
                         k_allow_user_update);
  }

  if (!found) {
    log_debug("User doesn't exists, generate fake salt.");
    // In this case we could randomize something to
    // make other side believe that the user exists (even if not).
    session->user.has_user_id = false;
    session_data->ksi.iterations = 5000;
    session_data->ksi.salt = get_salt_for_the_user(ireq.user);
    session_data->ksi.is_valid = true;
  } else {
    session_data->ksi = UserOptionsParser(session->user.auth_string).decode();
  }

  session_manager_->set_unique_session_secondary_id(
      session, [&session_data]() -> auto{
        return session_data->nonce +
               helper::generate_string<kServerNonceLength,
                                       GeneratorNonceCharacters>();
      });

  session_data->nonce = session->handler_secondary_id;

  if (!session_data->ksi.is_valid)
    return SaslResult(get_problem_description(HttpStatusCode::Unauthorized,
                                              "Account invalid configuration"));
  log_debug("s:ksi.salt:          %s",
            string_array(session_data->ksi.salt).c_str());

  ScramServerAuthChallange challange{
      as_vector(session_data->ksi.salt),
      static_cast<uint32_t>(session_data->ksi.iterations), session_data->nonce};
  auto auth_continue =
      session_data->scram->set_challange(challange, session->get_session_id());
  if (!is_json) {
    return SaslResult(get_problem_description(HttpStatusCode::Unauthorized,
                                              "Solve challenge",
                                              {{"data", auth_continue}}));
  }

  return {HttpResult{auth_continue, HttpResult::Type::typeJson}};
}

const char *to_string(const bool b) { return b ? "yes" : "no"; }

SaslResult ScramHandler::client_response(RequestContext &ctxt, Session *session,
                                         AuthUser *out_user,
                                         const std::string &auth_data,
                                         const bool is_json) {
  log_debug("ScramHandler::client_response is_json=%s auth_data=%s ",
            to_string(is_json), auth_data.c_str());
  const auto session_data = session->get_data<ScramSessionData>();
  if (!session_data->scram || session_data->scram->is_json() != is_json) {
    return SaslResult(get_problem_description(HttpStatusCode::Unauthorized));
  }

  auto auth_continue = session_data->scram->set_continue(auth_data);
  if (!auth_continue.session.empty() &&
      (session->get_session_id() != auth_continue.session)) {
    return SaslResult(get_problem_description(HttpStatusCode::Unauthorized));
  }

  if (!session_data->scram || session_data->scram->is_json() != is_json)
    return SaslResult(get_problem_description(HttpStatusCode::Unauthorized));

  const auto authe_message = session_data->scram->get_auth_message();

  auto client_sig = crypto_hmac(session_data->ksi.stored_key, authe_message);
  auto client_key = crypto_xor(client_sig, auth_continue.client_proof);
  auto stored_key_from_client = crypto_sha256(client_key);

  if (stored_key_from_client != session_data->ksi.stored_key)
    return SaslResult(get_problem_description(HttpStatusCode::Unauthorized));

  *out_user = session->user;
  session->state = Session::kUserVerified;
  ctxt.post_authentication = is_json ? true : ctxt.post_authentication;

  return {};
}

bool ScramHandler::redirects(RequestContext &) const {
  log_debug("ScramHandler::redirects - false");
  return false;
}

std::optional<std::string> ScramHandler::get_session_id_from_request_data(
    RequestContext &ctxt) {
  auto [state, auth_data, is_json] = get_authorize_data(ctxt);
  if (AuthenticationStateResponse == state) {
    auto scram_parser = create_scram_parser(is_json);
    auto auth_continue = scram_parser->set_continue(auth_data);

    if (!auth_continue.nonce.empty()) return auth_continue.nonce;
  }
  return {};
}

}  // namespace authentication
}  // namespace mrs
