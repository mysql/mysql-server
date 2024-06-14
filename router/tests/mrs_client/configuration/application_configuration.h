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

#ifndef ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_CONFIGURATION_H_
#define ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_CONFIGURATION_H_

#include <chrono>
#include <optional>
#include <string>

#include "client/session_type.h"
#include "configuration/request.h"

using Seconds = std::chrono::seconds;

namespace http_client {

using namespace mrs_client;
enum class AuthenticationType { kNone, kBasic, kScramGet, kOauth2 };
enum class ResponseType { kJson, kBinary, kRaw };
enum class WriteFileFormat { kRaw, kMTR };

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
  std::string json_schema_file;
  std::string payload;
  std::string accept;
  std::string write_to_file;
  WriteFileFormat write_format{WriteFileFormat::kRaw};
  bool help{false};
  ApplicationDisplay display;
  SessionType session_type{SessionType::kCookie};
  ResponseType response_type{ResponseType::kJson};
  std::string content_type;
  HttpStatusCode::key_type expected_status{HttpStatusCode::Ok};
  std::optional<Seconds> wait_until_found;
  std::optional<Seconds> wait_until_status;
};

}  // namespace http_client

#endif  // ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_CONFIGURATION_H_
