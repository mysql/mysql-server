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

#ifndef ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_CONFIGURATION_H_
#define ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_CONFIGURATION_H_

#include <chrono>
#include <map>
#include <optional>
#include <string>

#include "client/session_type.h"
#include "configuration/request.h"

using Seconds = std::chrono::seconds;

namespace http_client {

using namespace mrs_client;
enum class AuthenticationType {
  kNone,
  kBasic,
  kBasicJson,
  kScramGet,
  kScramPost,
  kOauth2
};
enum class ResponseType { kJson, kBinary, kRaw };
enum class WriteFileFormat { kRaw, kMTR };
enum class WireProtocol { kHttp_1_1, kHttp_2 };

struct ApplicationDisplay {
  bool request{true};
  bool status{false};
  bool header{false};
  bool body{true};
  bool result{true};
  bool title{false};

  static ApplicationDisplay display_all() {
    ApplicationDisplay result = {true, true, true, true, true, true};
    return result;
  }
};

struct ApplicationConfiguration {
  ApplicationConfiguration() {}

  std::string url;
  std::string path;
  std::string path_before_escape;
  std::string user;
  std::string password;
  std::vector<std::string> json_pointer;
  std::vector<std::string> excluscive_json_pointer;
  AuthenticationType authentication{AuthenticationType::kNone};
  Request::Type request{HttpMethod::Get};
  std::string session_file;
  std::string json_schema;
  std::string payload;

  // request headers
  std::string accept;
  std::string host;
  std::string authorization;
  WireProtocol wire_protocol{WireProtocol::kHttp_1_1};

  std::string write_to_file;
  WriteFileFormat write_format{WriteFileFormat::kRaw};
  bool help{false};
  ApplicationDisplay display;
  SessionType session_type{SessionType::kCookie};
  ResponseType response_type{ResponseType::kJson};
  std::optional<std::string> auth_app;
  std::string content_type;
  HttpStatusCode::key_type expected_status{HttpStatusCode::Ok};
  std::map<std::string, std::string> expected_headers;
  std::optional<Seconds> wait_until_status;
  std::optional<std::string> expect_fatal_error;
  std::optional<std::string> header_upgrade;
  std::optional<std::string> header_http2_settings;
};

}  // namespace http_client

#endif  // ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_CONFIGURATION_H_
