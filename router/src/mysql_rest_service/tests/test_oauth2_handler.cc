/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "http/base/io_buffer.h"
#include "http/base/request.h"
#include "http/base/uri.h"
#include "mrs/authentication/oauth2_handler.h"
#include "mrs/http/error.h"
#include "mrs/http/session_manager.h"
#include "mrs/rest/request_context.h"

using testing::HasSubstr;
using testing::Not;

namespace {

class FakeRequest : public ::http::base::Request {
 public:
  explicit FakeRequest(const std::string &uri) : uri_{uri} {}

  const Headers &get_input_headers() const override { return input_headers_; }
  IOBuffer &get_input_buffer() const override { return input_buffer_; }
  const std::string &get_input_body() const override { return input_body_; }

  Headers &get_output_headers() override { return output_headers_; }
  IOBuffer &get_output_buffer() override { return output_buffer_; }

  const Uri &get_uri() const override { return uri_; }

  ConnectionInterface *get_connection() const override { return nullptr; }

  Headers &input_headers() { return input_headers_; }
  Headers &output_headers() { return output_headers_; }

 private:
  mutable Headers input_headers_;
  Headers output_headers_;
  mutable IOBuffer input_buffer_;
  IOBuffer output_buffer_;
  Uri uri_;
  std::string input_body_;
};

class NoopRequestHandler
    : public mrs::authentication::Oauth2Handler::RequestHandler {
 public:
  void before_send(::http::base::Request *) override {}
  bool response(const std::vector<uint8_t> &) override { return true; }
};

class FakeOauth2Handler : public mrs::authentication::Oauth2Handler {
 public:
  explicit FakeOauth2Handler(const AuthApp &entry)
      : Oauth2Handler{entry, nullptr} {}

  const std::string &get_handler_name() const override {
    static const std::string result{"fake-oauth2"};
    return result;
  }

  void pre_authorize_account(mrs::interface::AuthorizeHandler *,
                             const std::string &) override {}

  std::string get_url_direct_auth() const override {
    return "http://127.0.0.1:1/token";
  }

  std::string get_url_location(GenericSessionData *data, Url *) const override {
    return "https://auth.example/authorize?redirect_uri=" + data->redirection;
  }

  std::string get_url_validation(GenericSessionData *data) const override {
    return "http://127.0.0.1:1/userinfo?access_token=" + data->access_token;
  }

  RequestHandlerPtr get_request_handler_access_token(
      GenericSessionData *) override {
    return std::make_unique<NoopRequestHandler>();
  }

  RequestHandlerPtr get_request_handler_verify_account(
      Session *, GenericSessionData *) override {
    return std::make_unique<NoopRequestHandler>();
  }

  std::string get_body_access_token_request(
      GenericSessionData *) const override {
    return "grant_type=authorization_code";
  }
};

class Oauth2HandlerTests : public ::testing::Test {
 protected:
  using AuthUser = mrs::database::entry::AuthUser;
  using ErrorRedirect = mrs::http::ErrorRedirect;
  using GenericSessionData =
      mrs::authentication::Oauth2Handler::GenericSessionData;
  using Session = mrs::http::SessionManager::Session;
  using SessionPtr = mrs::http::SessionManager::SessionPtr;

  mrs::database::entry::AuthApp make_auth_app() {
    mrs::database::entry::AuthApp result;
    result.id = mrs::UniversalId{1, 1};
    result.url = "https://auth.example/authorize";
    result.url_validation = "http://127.0.0.1:1/userinfo";
    result.app_id = "client-id";
    result.app_token = "client-secret";
    result.limit_to_registered_users = false;
    return result;
  }

  mrs::http::SessionManager::SessionPtr make_session() {
    auto result = session_manager_.new_session(mrs::UniversalId{1, 1}, "oauth");
    result->proto = "https";
    result->host = "router.example";
    return result;
  }

  void expect_redirect_without_direct_token(FakeRequest *request,
                                            FakeOauth2Handler *handler,
                                            const SessionPtr &session) {
    mrs::rest::RequestContext ctxt{request};
    AuthUser user;

    try {
      handler->authorize(ctxt, session, &user);
      FAIL() << "Direct token must not complete OAuth2 authorization";
    } catch (const ErrorRedirect &redirect) {
      redirect.change_response(request);
    }

    const auto *location = request->output_headers().find("Location");
    ASSERT_NE(nullptr, location);
    EXPECT_THAT(*location, HasSubstr("https://auth.example/authorize"));
    EXPECT_THAT(*location, Not(HasSubstr("direct-token")));

    auto *data = session->get_data<GenericSessionData>();
    ASSERT_NE(nullptr, data);
    EXPECT_TRUE(data->access_token.empty());
    EXPECT_THAT(data->redirection, Not(HasSubstr("direct-token")));
  }

  mrs::http::SessionManager session_manager_;
};

TEST_F(Oauth2HandlerTests, direct_token_without_code_starts_provider_login) {
  auto auth_app = make_auth_app();
  FakeOauth2Handler handler{auth_app};
  FakeRequest request{"/login?token=direct-token"};
  auto session = make_session();

  expect_redirect_without_direct_token(&request, &handler, session);
  EXPECT_NE(Session::kTokenVerified, session->state);
}

TEST_F(Oauth2HandlerTests,
       direct_token_does_not_verify_waiting_for_code_session) {
  auto auth_app = make_auth_app();
  FakeOauth2Handler handler{auth_app};
  FakeRequest request{"/login?token=direct-token"};
  auto session = make_session();
  session->set_data(new GenericSessionData());
  session->state = Session::kWaitingForCode;

  expect_redirect_without_direct_token(&request, &handler, session);
  EXPECT_EQ(Session::kWaitingForCode, session->state);
}

TEST_F(Oauth2HandlerTests,
       sensitive_query_parameters_are_not_reflected_to_provider_redirect_uri) {
  auto auth_app = make_auth_app();
  FakeOauth2Handler handler{auth_app};
  FakeRequest request{
      "/login?token=direct-token&access_token=access-token&id_token=id-token&"
      "refresh_token=refresh-token&code=auth-code&foo=bar"};
  auto session = make_session();
  mrs::rest::RequestContext ctxt{&request};
  AuthUser user;

  try {
    handler.authorize(ctxt, session, &user);
    FAIL() << "Invalid callback must start provider authorization";
  } catch (const ErrorRedirect &redirect) {
    redirect.change_response(&request);
  }

  const auto *location = request.output_headers().find("Location");
  ASSERT_NE(nullptr, location);
  EXPECT_THAT(*location, HasSubstr("https://auth.example/authorize"));
  EXPECT_THAT(*location, HasSubstr("foo=bar"));
  EXPECT_THAT(*location, Not(HasSubstr("direct-token")));
  EXPECT_THAT(*location, Not(HasSubstr("access-token")));
  EXPECT_THAT(*location, Not(HasSubstr("id-token")));
  EXPECT_THAT(*location, Not(HasSubstr("refresh-token")));
  EXPECT_THAT(*location, Not(HasSubstr("auth-code")));

  auto *data = session->get_data<GenericSessionData>();
  ASSERT_NE(nullptr, data);
  EXPECT_THAT(data->redirection, HasSubstr("foo=bar"));
  EXPECT_THAT(data->redirection, Not(HasSubstr("direct-token")));
  EXPECT_THAT(data->redirection, Not(HasSubstr("access-token")));
  EXPECT_THAT(data->redirection, Not(HasSubstr("id-token")));
  EXPECT_THAT(data->redirection, Not(HasSubstr("refresh-token")));
  EXPECT_THAT(data->redirection, Not(HasSubstr("auth-code")));
}

}  // namespace
