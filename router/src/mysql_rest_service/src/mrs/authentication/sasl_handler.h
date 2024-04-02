/*
 Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_SASL_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_SASL_HANDLER_H_

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "helper/http/url.h"
#include "helper/variant_pointer.h"
#include "http/base/method.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/interface/authorize_handler.h"
#include "mrs/interface/http_result.h"
#include "mrs/users/user_manager.h"

namespace mrs {
namespace authentication {

class SaslHandler : public interface::AuthorizeHandler {
 protected:
  using HttpResult = mrs::interface::HttpResult;
  using AuthApp = database::entry::AuthApp;
  using duration = std::chrono::steady_clock::duration;
  using seconds = std::chrono::seconds;
  using steady_clock = std::chrono::steady_clock;
  using time_point = std::chrono::steady_clock::time_point;
  using HttpMethodType = ::http::base::method::key_type;
  using UserManager = mrs::users::UserManager;
  using SessionManager = mrs::http::SessionManager;
  using VariantPointer = helper::VariantPointer;
  using UrlParameters = helper::http::Url::Parameters;
  using SessionData = http::SessionManager::Session::SessionData;

 public:
  enum AuthenticationState {
    AuthenticationStateExchange,
    AuthenticationStateInitialResponse,
    AuthenticationStateResponse,
    AuthenticationStateInvalid,
  };

  class SaslSessionData : public SessionData {
   public:
    seconds expires;
    bool session_id_set{false};
    time_point acquired_at;

    AuthenticationState sasl_state{AuthenticationStateExchange};
  };

 public:
  SaslHandler(const AuthApp &entry);

  const AuthApp &get_entry() const override;
  UniversalId get_service_id() const override;
  UniversalId get_id() const override;

  virtual SessionData *allocate_session_data() = 0;
  bool redirects() const override;
  bool is_authorized(Session *session, AuthUser *user) override;
  bool authorize(RequestContext &ctxt, Session *session,
                 AuthUser *out_user) override;

  struct SaslResult {
    enum Type { SaslChallanage, SaslOk, SaslHttpStatusCode };

    SaslResult() : response_type{SaslOk} {}

    SaslResult(HttpResult result)
        : response_type{SaslHttpStatusCode}, http_result{result} {}

    Type response_type;
    HttpResult http_result;
  };

  struct SaslData {
    AuthenticationState state;
    std::string auth_data;
    bool is_json;
  };

  virtual AuthenticationState get_authentication_state(
      const UrlParameters &parameters, const bool has_auth_data);

  virtual SaslResult client_request_authentication_exchange(
      RequestContext &ctxt, Session *session, AuthUser *out_user) = 0;
  virtual SaslResult client_initial_response(RequestContext &ctxt,
                                             Session *session,
                                             AuthUser *out_user,
                                             const std::string &auth_data,
                                             const bool is_json) = 0;
  virtual SaslResult client_response(RequestContext &ctxt, Session *session,
                                     AuthUser *out_user,
                                     const std::string &auth_data,
                                     const bool is_json) = 0;

 protected:
  SaslData get_authorize_data(RequestContext &ctxt);
  AuthApp entry_;
  UserManager um_{entry_.limit_to_registered_users, entry_.default_role_id};
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_SASL_HANDLER_H_
