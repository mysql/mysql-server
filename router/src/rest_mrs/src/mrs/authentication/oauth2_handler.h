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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_OAUTH2_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_OAUTH2_HANDLER_H_

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "helper/variant_pointer.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/interface/authorize_handler.h"
#include "mrs/users/user_manager.h"

#include "mysqlrouter/http_request.h"

namespace mrs {
namespace authentication {

class Oauth2Handler : public interface::AuthorizeHandler {
 protected:
  using AuthApp = database::entry::AuthApp;
  using duration = std::chrono::steady_clock::duration;
  using seconds = std::chrono::seconds;
  using steady_clock = std::chrono::steady_clock;
  using time_point = std::chrono::steady_clock::time_point;
  using HttpMethodType = HttpMethod::type;
  using UserManager = mrs::users::UserManager;
  using SessionManager = mrs::http::SessionManager;
  using VariantPointer = helper::VariantPointer;

 public:
  class RequestHandler {
   public:
    virtual ~RequestHandler() = default;

    virtual void before_send(HttpRequest *request) = 0;
    virtual bool response(const std::vector<uint8_t> &value) = 0;
  };
  using RequestHandlerPtr = std::unique_ptr<RequestHandler>;

  class GenericSessionData : public http::SessionManager::Session::SessionData {
   public:
    enum State {
      kWaitingForCode,
      kGettingTokken,
      kTokenVerified,
      kUserVerified
    };
    State state{kWaitingForCode};
    std::string access_token;
    std::string refresh_token;
    std::string auth_code;
    std::string redirection;
    AuthUser user;
    seconds expires;
    bool session_id_set{false};
    time_point acquired_at;
    std::string challange;
  };

 public:
  Oauth2Handler(const AuthApp &entry) : entry_{entry} {}

  const AuthApp &get_entry() const override;
  uint64_t get_service_id() const override;
  uint64_t get_id() const override;
  bool is_authorized(Session *session, AuthUser *user) override;
  bool authorize(Session *session, http::Url *url,
                 SqlSessionCached *sql_session, HttpHeaders &input_headers,
                 AuthUser *out_user) override;

  class RequestHandlerJsonSimpleObject : public RequestHandler {
   public:
    using OutPair = std::pair<const char *, VariantPointer>;
    using OutJsonObjectKeyValues = std::vector<OutPair>;

    RequestHandlerJsonSimpleObject(OutJsonObjectKeyValues output)
        : output_{std::move(output)} {}

    void before_send(HttpRequest *request) override;
    bool response(const std::vector<uint8_t> &value) override;

    OutJsonObjectKeyValues output_;
  };

 protected:
  virtual std::string get_url_direct_auth() const = 0;
  virtual std::string get_url_location(GenericSessionData *data,
                                       http::Url *url) const = 0;
  virtual std::string get_url_validation(GenericSessionData *data) const = 0;
  virtual RequestHandlerPtr get_request_handler_access_token(
      GenericSessionData *session_data) = 0;
  virtual RequestHandlerPtr get_request_handler_verify_account(
      GenericSessionData *session_data) = 0;
  virtual std::string get_body_access_token_request(
      GenericSessionData *session_data) const = 0;

 protected:
  const std::string &get_host_alias() const;
  std::string get_cookie_session_id(HttpRequest *request) const;
  void set_cookie_session_id(HttpRequest *request,
                             SessionManager::Session *session);

  void new_session_start_login(Session *session, http::Url *url);
  bool http_acquire_access_token(GenericSessionData *data);
  bool http_verify_account(GenericSessionData *data,
                           SqlSessionCached *sql_session);

 protected:
  static std::string get_request_path(HttpUri &u);

  static bool send_http_request(HttpMethodType method, const std::string &url,
                                const std::string &body,
                                RequestHandler *request_handler = nullptr);

  AuthApp entry_;
  UserManager um_{entry_.id, entry_.limit_to_registered_users,
                  entry_.default_role_id};
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_OAUTH2_HANDLER_H_
