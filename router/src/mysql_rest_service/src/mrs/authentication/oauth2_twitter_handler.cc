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

#include "mrs/authentication/oauth2_twitter_handler.h"

#include <chrono>
#include <string_view>

#include <my_rapidjson_size_t.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

#include <helper/json/text_to.h>
#include "helper/container/map.h"
#include "helper/http/url.h"
#include "helper/json/rapid_json_to_map.h"
#include "helper/json/to_string.h"
#include "helper/string/random.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/rest/request_context.h"

#include "http/base/request.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"
#include "mysqlrouter/http_client.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

using RequestHandlerPtr = Oauth2Handler::RequestHandlerPtr;

class RequestHandlerJsonSimpleObjectBasicAuthorization
    : public Oauth2TwitterHandler::RequestHandlerJsonSimpleObject {
 public:
  using AuthApp = database::entry::AuthApp;
  using Request = ::http::base::Request;

 public:
  RequestHandlerJsonSimpleObjectBasicAuthorization(
      const AuthApp &entry, OutJsonObjectKeyValues output)
      : RequestHandlerJsonSimpleObject(std::move(output)), entry_{entry} {}

  void before_send(Request *request) override {
    std::string basic = "Basic ";
    std::string authentication = entry_.app_id + ":" + entry_.app_token;
    basic += Base64::encode(authentication);
    request->get_output_headers().add("authorization", basic.c_str());
  }

  const AuthApp &entry_;
};

class RequestHandlerJsonSubSimpleObject
    : public Oauth2TwitterHandler::RequestHandlerJsonSimpleObject {
 public:
  using Request = ::http::base::Request;

 public:
  RequestHandlerJsonSubSimpleObject(const std::string &bearer,
                                    OutJsonObjectKeyValues output)
      : RequestHandlerJsonSimpleObject{std::move(output)}, bearer_{bearer} {}

  void before_send(Request *request) override {
    std::string bearer = "Bearer " + bearer_;
    request->get_output_headers().add("Authorization", bearer.c_str());
  }

  bool response(const std::vector<uint8_t> &value) override {
    using HandlerMapOfSimpleValues =
        helper::json::RapidReaderHandlerToMapOfSimpleValues;
    using HandlerSubObject =
        helper::json::ExtractSubObjectHandler<HandlerMapOfSimpleValues>;

    HandlerMapOfSimpleValues handler_map;
    HandlerSubObject handler("data", handler_map);
    if (!helper::json::text_to(&handler, value)) {
      log_debug("Parsing JSON response failed.");
      return false;
    }

    for (auto &e : output_) {
      auto &key = e.first;
      auto out_value = e.second;
      if (!helper::container::get_value_other(handler_map.get_result(), key,
                                              out_value)) {
        log_debug("Getting key:'%s' from container failed.", key);
        return false;
      }
    }

    return true;
  }

  const std::string &bearer_;
};

Oauth2TwitterHandler::Oauth2TwitterHandler(const AuthApp &entry)
    : Oauth2Handler{entry} {
  log_debug("Oauth2TwitterHandler for service %s", to_string(entry_).c_str());
}

std::string Oauth2TwitterHandler::get_url_location(GenericSessionData *data,
                                                   Url *) const {
  std::string result{!entry_.url.empty()
                         ? entry_.url
                         : "https://twitter.com/i/oauth2/authorize"};

  result +=
      "?response_type=code&state=first&client_id=" + entry_.app_id +
      "&scope=tweet.read%20users.read%20follows.read%20follows.write&state="
      "state&code_challenge=" +
      data->challange +
      "&code_challenge_method=plain&redirect_uri=" + data->redirection;
  return result;
}

std::string Oauth2TwitterHandler::get_url_direct_auth() const {
  if (!entry_.url_access_token.empty()) return entry_.url_access_token;

  return "https://api.twitter.com/2/oauth2/token";
}

std::string Oauth2TwitterHandler::get_url_validation(
    GenericSessionData *) const {
  std::string result{!entry_.url_validation.empty()
                         ? entry_.url_validation
                         : "https://api.twitter.com/2/users/me"};

  return result;
}

std::string Oauth2TwitterHandler::get_body_access_token_request(
    GenericSessionData *session_data) const {
  std::string body =
      "grant_type=authorization_code&code=" + session_data->auth_code +
      "&client_id=" + entry_.app_id +
      "&redirect_uri=" + session_data->redirection +
      "&code_verifier=" + session_data->challange;

  return body;
}

RequestHandlerPtr Oauth2TwitterHandler::get_request_handler_verify_account(
    Session *session, GenericSessionData *session_data) {
  return RequestHandlerPtr{new RequestHandlerJsonSubSimpleObject{
      session_data->access_token,
      {{"id", &session->user.vendor_user_id},
       {"username", &session->user.name}}}};
}

RequestHandlerPtr Oauth2TwitterHandler::get_request_handler_access_token(
    GenericSessionData *session_data) {
  return RequestHandlerPtr{new RequestHandlerJsonSimpleObjectBasicAuthorization{
      entry_,
      {{"access_token", &session_data->access_token},
       {"expires_in", &session_data->expires}}}};
}

}  // namespace authentication
}  // namespace mrs
