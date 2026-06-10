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

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <streambuf>
#include <thread>
#include <vector>

#include "helper/container/to_string.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"  //remove
#include "helper/string/contains.h"
#include "helper/string/hex.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/http_client.h"

#include "client/authentication.h"
#include "client/http_client_request.h"
#include "client/session.h"
#include "configuration/application_configuration.h"
#include "json/json_copy_pointers.h"

#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>

#include "my_sys.h"
#include "mysql.h"

#define HTTP_STATUS_ENTRY(X) \
  { mysql_harness::make_lower(#X), HttpStatusCode::X, }

using mrs_client::HttpClientRequest;
using mrs_client::HttpClientSession;
using mrs_client::Result;
using Display = http_client::ApplicationDisplay;
using Request = http_client::Request;
template <typename T>
using PrettyWriter = rapidjson::PrettyWriter<T>;

const int kHelpScreenWidth = 1024;
const int kHelpScreenIdent = 4;
std::string g_executable;
CmdArgHandler g_cmd_handler{};
http_client::ApplicationConfiguration g_configuration;

bool mrs_client_app_debug{false};

static std::string decode_from_url_query_escaping(const std::string &value) {
  std::string decoded;
  int val;
  for (size_t i = 0; i < value.length(); i++) {
    if (value[i] == '%') {
      sscanf(value.substr(i + 1, 2).c_str(), "%x", &val);
      decoded += static_cast<char>(val);
      i += 2;
    } else {
      decoded += value[i];
    }
  }
  return decoded;
}

class CustomHex {
 public:
  void operator()(std::ostringstream &os) {
    os << std::setfill('0') << std::setw(2) << std::hex;
  }

  int operator()(const char &v) { return (int)static_cast<uint8_t>(v); }
};

template <typename OptionalValue>
bool optional_set(const std::optional<OptionalValue> &opt_value,
                  OptionalValue *holder) {
  if (holder && opt_value.has_value()) {
    *holder = opt_value.value();
    return true;
  }
  return opt_value.has_value();
}

static void check_payload() {
  if (g_configuration.request != HttpMethod::Post &&
      g_configuration.request != HttpMethod::Put) {
    throw std::invalid_argument(
        "'Payload' may only be used with POST and PUT request type.");
  }
}

static std::optional<http_client::ApplicationDisplay> display_type_convert(
    const std::string &value) {
  using Display = http_client::ApplicationDisplay;
  using MemberBool = bool Display::*;
  Display result;
  static std::map<std::string, MemberBool> allowed_values{
      {"request", &Display::request}, {"status", &Display::status},
      {"header", &Display::header},   {"body", &Display::body},
      {"result", &Display::result},   {"title", &Display::title}};

  if ("all" == value) {
    for (auto &[key, value] : allowed_values) {
      result.*(value) = true;
    }
    return {result};
  }

  for (auto &[key, value] : allowed_values) {
    result.*(value) = false;
  }

  if ("none" == value) {
    return {result};
  }

  auto types = mysql_harness::split_string(value, ',', false);

  if (types.empty()) return {};

  for (auto &type : types) {
    auto ltype = mysql_harness::make_lower(type);
    auto it = allowed_values.find(ltype);
    if (allowed_values.end() == it) return {};
    result.*(it->second) = true;
  }

  return {result};
}

static std::optional<http_client::WireProtocol> protocol_type_convert(
    std::string value) {
  using namespace http_client;
  const static std::map<std::string, WireProtocol> map{
      {"http2", WireProtocol::kHttp_2}, {"http1", WireProtocol::kHttp_1_1}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return {};

  return {it->second};
}

static std::optional<http_client::SessionType> session_type_convert(
    std::string value) {
  using namespace http_client;
  const static std::map<std::string, SessionType> map{
      {"cookie", SessionType::kCookie}, {"jwt", SessionType::kJWT}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return {};
  return {it->second};
}

static std::optional<http_client::WriteFileFormat> wire_format_convert(
    std::string value) {
  using namespace http_client;
  const static std::map<std::string, WriteFileFormat> map{
      {"raw", WriteFileFormat::kRaw}, {"mtr", WriteFileFormat::kMTR}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return {};

  return {it->second};
}

static std::optional<http_client::AuthenticationType>
authentication_type_convert(std::string value) {
  using namespace http_client;
  const static std::map<std::string, AuthenticationType> map{
      {"none", AuthenticationType::kNone},
      {"basic", AuthenticationType::kBasic},
      {"basic_json", AuthenticationType::kBasicJson},
      {"scram_get", AuthenticationType::kScramGet},
      {"scram_post", AuthenticationType::kScramPost},
      {"oauth2_f", AuthenticationType::kOauth2}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return {};

  return {it->second};
}

static std::optional<http_client::ResponseType> response_type_convert(
    std::string value) {
  using namespace http_client;
  const static std::map<std::string, ResponseType> map{
      {"json", ResponseType::kJson},
      {"raw", ResponseType::kRaw},
      {"binary", ResponseType::kBinary}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return {};
  return {it->second};
}

static const std::map<std::string, HttpStatusCode::key_type>
    &get_status_code_map() {
  const static std::map<std::string, HttpStatusCode::key_type> map{
      HTTP_STATUS_ENTRY(Continue),
      HTTP_STATUS_ENTRY(Ok),
      HTTP_STATUS_ENTRY(MovedPermanently),
      HTTP_STATUS_ENTRY(NotModified),
      HTTP_STATUS_ENTRY(TemporaryRedirect),
      HTTP_STATUS_ENTRY(PermanentRedirect),
      HTTP_STATUS_ENTRY(BadRequest),
      HTTP_STATUS_ENTRY(Unauthorized),
      HTTP_STATUS_ENTRY(Forbidden),
      HTTP_STATUS_ENTRY(RequestTimeout),
      HTTP_STATUS_ENTRY(NotFound),
      HTTP_STATUS_ENTRY(MethodNotAllowed),
      HTTP_STATUS_ENTRY(InternalError),
      HTTP_STATUS_ENTRY(TooManyRequests),
      HTTP_STATUS_ENTRY(NotImplemented),
  };
  return map;
}

static std::optional<HttpStatusCode::key_type> status_code_convert(
    std::string value) {
  using namespace http_client;

  auto &map = get_status_code_map();

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return {};
  return {it->second};
}

static void print_usage() {
  std::cout << "# Usage" << std::endl;
  auto out = g_cmd_handler.usage_lines(g_executable, {}, kHelpScreenWidth);
  for (auto &line : out) {
    std::cout << line << std::endl;
  }
}

static void print_description() {
  std::cout << std::endl << "# Command line argument description" << std::endl;
  auto out =
      g_cmd_handler.option_descriptions(kHelpScreenWidth, kHelpScreenIdent);
  for (auto &line : out) {
    std::cout << line << std::endl;
  }
}

static void verify_required_arguments() {
  if (g_configuration.url.empty()) {
    throw std::invalid_argument(
        "URL parameter is required, and it must be not empty.");
  }
}

static void print_results(const Result &result, const Display &display) {
  if (display.request) {
    if (display.title) std::cout << "Request: ";
    std::string rt{"unknown"};
    for (auto &pk : Request::get_map()) {
      if (pk.second == g_configuration.request) {
        rt = pk.first;
        break;
      }
    }
    mysql_harness::upper(rt);
    auto path = g_configuration.path_before_escape;
    if (path.empty()) path = "/";

    std::cout << rt << " " << path;
    if (g_configuration.payload.length() > 0) {
      std::cout << " (payload-size:"
                << std::to_string(g_configuration.payload.length()) << ")";
    }
    std::cout << std::endl;
  }
  if (display.status) {
    if (display.title) std::cout << "Status: ";
    std::cout << HttpStatusCode::get_default_status_text(result.status) << "("
              << result.status << ")" << std::endl;
  }

  if (display.header) {
    for (const auto &h : result.headers) {
      if (display.title) std::cout << "Header: ";
      std::cout << h.first << "  ->  " << h.second << std::endl;
    }
  }

  if (display.body) {
    std::string body = result.body;
    if (display.title) std::cout << "Body: ";

    if (g_configuration.response_type == http_client::ResponseType::kBinary) {
      std::cout << helper::string::hex<decltype(body), CustomHex>(body)
                << std::endl;
    } else
      std::cout << body << std::endl;
  }

  if (display.result) {
    std::cout << (result.ok ? "OK" : "FAILED") << std::endl;
  }
}

static CmdOption make_required(CmdOption cmd_option) {
  cmd_option.required = true;
  return cmd_option;
}

bool cnf_should_execute_authentication_flow() {
  return http_client::AuthenticationType::kNone !=
         g_configuration.authentication;
}

std::vector<CmdOption> g_options{
    {{"--help"},
     "Show help and exit.",
     CmdOptionValueReq::none,
     "help",
     [](const std::string &) { g_configuration.help = true; }},
    make_required(
        {{"--url"},
         "HTTP/HTTPS url pointing to remote resources.",
         CmdOptionValueReq::required,
         "meta_url",
         [](const std::string &value) { g_configuration.url = value; }}),
    {{"--user", "-u"},
     "User name, required for authentication.",
     CmdOptionValueReq::required,
     "meta_user",
     [](const std::string &value) { g_configuration.user = value; }},
    {{"--password", "-p"},
     "Users password, required for authentication.",
     CmdOptionValueReq::required,
     "meta_password",
     [](const std::string &value) { g_configuration.password = value; }},
    {{"--authentication", "-a"},
     "Execute authentication flow on given endpoint. Allowed values: "
     "NONE,BASIC,SCRAM_GET,OAUTH2_F",
     CmdOptionValueReq::required,
     "meta_auth",
     [](const std::string &value) {
       if (!optional_set(authentication_type_convert(value),
                         &g_configuration.authentication))
         throw std::invalid_argument(
             "Invalid parameter for authentication type.");

       if (g_configuration.authentication !=
           http_client::AuthenticationType::kNone) {
         g_configuration.response_type = http_client::ResponseType::kRaw;
       }
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         if (g_configuration.user.empty())
           throw std::invalid_argument(
               "User name is required (password optional), when executing "
               "authentication flow.");
       }
     }},

    {{"--accept"},
     "Set the mime-type, that client can process.",
     CmdOptionValueReq::required,
     "meta_accept",
     [](const std::string &value) { g_configuration.accept = value; },
     [](const std::string &) {}},

    {{"--request-header-host"},
     "Set the value in the 'Host' header of the request.",
     CmdOptionValueReq::required,
     "host",
     [](const std::string &value) { g_configuration.host = value; },
     [](const std::string &) {}},

    {{"--encoded-request-header-host"},
     "Set the value in the 'Host' header of the request. The value must be "
     "encoded using url-parameter encoding.",
     CmdOptionValueReq::required,
     "host",
     [](const std::string &value) {
       g_configuration.host = decode_from_url_query_escaping(value);
     },
     [](const std::string &) {}},

    {{"--encoded-request-header-authorization"},
     "Set the value in the 'Authorization' header of the request. The value "
     "must be "
     "encoded using url-parameter encoding.",
     CmdOptionValueReq::required,
     "authorization",
     [](const std::string &value) {
       g_configuration.authorization = decode_from_url_query_escaping(value);
     },
     [](const std::string &) {}},

    {{"--payload-file"},
     "Set the request body for POST, PUT requests.",
     CmdOptionValueReq::required,
     "meta_payload",
     [](const std::string &value) {
       std::ifstream file{value};
       if (!file.is_open()) {
         throw std::runtime_error("File with `payload`, cannot be opened.");
       }

       g_configuration.payload =
           std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
     },
     [](const std::string &) { check_payload(); }},

    {{"--payload"},
     "Set the request body for POST, PUT requests.",
     CmdOptionValueReq::required,
     "meta_payload",
     [](const std::string &value) { g_configuration.payload = value; },
     [](const std::string &) { check_payload(); }},

    {{"--encoded-payload"},
     "Set the request body for POST, PUT requests (provided as an "
     "URL-encoded "
     "string).",
     CmdOptionValueReq::required,
     "meta_payload",
     [](const std::string &value) {
       g_configuration.payload = decode_from_url_query_escaping(value);
     },
     [](const std::string &) { check_payload(); }},

    {{"--write-format", "-f"},
     "Write format.",
     CmdOptionValueReq::required,
     "write_format",
     [](const std::string &value) {
       if (!optional_set(wire_format_convert(value),
                         &g_configuration.write_format))
         throw std::invalid_argument("Invalid parameter for output format.");
     },
     [](const std::string &) {}},
    {{"--write-to-file", "-w"},
     "Write output to file.",
     CmdOptionValueReq::required,
     "write_to_file",
     [](const std::string &value) { g_configuration.write_to_file = value; },
     [](const std::string &) {}},

    {{"--wait-until-status"},
     "Wait until server returns status-code specified in `expected-status` "
     "parameter. "
     "The value specifies how long `mrs_client` should wait, for it.",
     CmdOptionValueReq::required,
     "meta_wait_status",
     [](const std::string &value) {
       auto i = atoi(value.c_str());
       if (i <= 0)
         throw std::invalid_argument(
             "Wait timeout should be greater than zero.");
       g_configuration.wait_until_status = Seconds(i);
     },
     [](const std::string &) {}},

    {{"--path"},
     "Overwrite the path specified in URL. Using this parameter, user may "
     "split the URL on host part specified in --url and path.",
     CmdOptionValueReq::required,
     "meta_path",
     [](const std::string &value) {
       std::string decoded;
       int val;
       // if the path has "&"" encoded we decode it first as MRS does not
       // support it
       for (size_t i = 0; i < value.length(); i++) {
         if (value[i] == '%') {
           sscanf(value.substr(i + 1, 2).c_str(), "%x", &val);
           if (val == static_cast<int>('&')) {
             decoded += "&";
             i += 2;
             continue;
           }
         }
         decoded += value[i];
       }

       std::string translate{" []{}\""};
       std::string path;
       for (auto c : decoded) {
         if (translate.find(c) != std::string::npos) {
           path += "%" + helper::string::hex(std::string{c});
         } else {
           path.append(1, c);
         }
       }
       g_configuration.path_before_escape = decoded;
       g_configuration.path = path;
     }},
    {{"--session-type", "-s"},
     "Define how the session should be identified by the client. Allowed "
     "values: "
     "COOKIE,JWT. Where default value is COOKIE.",
     CmdOptionValueReq::required,
     "meta_auth_session",
     [](const std::string &value) {
       if (!optional_set(session_type_convert(value),
                         &g_configuration.session_type))
         throw std::invalid_argument(
             "Invalid parameter for authentication type.");
     },
     [](const std::string &) {
       if (!cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Session type can only be defined while executing "
             "authentication "
             "flow.");
       }
     }},

    {{"--auth-app"},
     "Define which authentication application should be used while "
     "authenticating.",
     CmdOptionValueReq::required,
     "meta_auth_app",
     [](const std::string &value) { g_configuration.auth_app = value; },
     [](const std::string &) {
       if (!cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Authentication app can only be defined while executing "
             "authentication flow.");
       }
     }},

    {{"--json-pointer", "-j"},
     "Print only values selected by pointers (inclusive pointer). Multiple "
     "pointer should be separated by comma.",
     CmdOptionValueReq::required,
     "meta_json_pointer",
     [](const std::string &value) {
       g_configuration.json_pointer =
           mysql_harness::split_string(value, ',', false);
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Json pointer can't be used while executing authentication "
             "flow.");
       }

       if (g_configuration.response_type != http_client::ResponseType::kJson) {
         throw std::invalid_argument(
             "Json pointer can only be used with JSON responses.");
       }

       if (g_configuration.json_pointer.empty())
         throw std::invalid_argument("There is no valid json-pointer.");
     }},

    {{"--exclude-json-pointer", "-e"},
     "Print values that are not selected by the pointers(exclusive pointer). "
     "Multiple pointer should be "
     "separated by comma. Inclusive and exclusive pointers can't be used in "
     "the same execution.",
     CmdOptionValueReq::required,
     "meta_exclude_json_pointer",
     [](const std::string &value) {
       g_configuration.excluscive_json_pointer =
           mysql_harness::split_string(value, ',', false);
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Json pointer can't be used while executing authentication "
             "flow.");
       }

       if (g_configuration.response_type != http_client::ResponseType::kJson) {
         throw std::invalid_argument(
             "Json pointer can only be used with JSON responses.");
       }

       if (g_configuration.excluscive_json_pointer.empty())
         throw std::invalid_argument("There is no valid json-pointer.");
     }},
    {{"--expected-header"},
     "Expect header/value in server's response message.",
     CmdOptionValueReq::required,
     "meta_header",
     [](const std::string &value) {
       auto equal_pos = value.find_first_of("=");
       if (equal_pos == std::string::npos) {
         throw std::invalid_argument(
             "Expected header value must follow this format: "
             "'--expected-header=HEADER=VALUE'");
       }
       std::string key = value.substr(0, equal_pos);
       std::string keys_value = value.substr(equal_pos + 1);
       g_configuration.expected_headers[key] = keys_value;
     }},
    {{"--expected-status"},
     "Specify allowed status code. Default is OK(200).",
     CmdOptionValueReq::required,
     "meta_status",
     [](const std::string &value) {
       g_configuration.expected_status = atoi(value.c_str());
       if (0 == g_configuration.expected_status) {
         if (!optional_set(status_code_convert(value),
                           &g_configuration.expected_status)) {
           using namespace std::string_literals;
           auto &map = get_status_code_map();
           throw std::invalid_argument(
               "Invalid value specified for 'expected-status', allowed "
               "values "
               "are positive integers or predefined text/values: "s +
               helper::container::to_string(map));
         }
       }
     }},

    {{"--display"},
     "What should be presented as output: "
     "VALUES=(none|all|VALUE[,VALUE[....]])"
     "where VALUE can be: REQUEST, TITLE, BODY, HEADER, STATUS, RESULT. By "
     "default its "
     "set to REQUEST,BODY,RESULT.",
     CmdOptionValueReq::required,
     "meta_display",
     [](const std::string &value) {
       if (!optional_set(display_type_convert(value), &g_configuration.display))
         throw std::invalid_argument("Invalid parameter for display.");
     }},

    {{"--session-file"},
     "File that contains session state, at start of the application\n"
     "the state is restored. At end the application stores the state to it..",
     CmdOptionValueReq::required,
     "meta_password",
     [](const std::string &value) { g_configuration.session_file = value; }},
    {{"--request-type", "-t"},
     "Define type of the request which should be send to the server.\n"
     "By default its GET, where allowed values are: "
     "GET,POST,PUT,DELETE,OPTIONS.",
     CmdOptionValueReq::required,
     "type",
     [](const std::string &value) {
       if (!http_client::Request::convert(value, &g_configuration.request))
         throw std::invalid_argument("Invalid parameter for request type.");
     }},

    {{"--content-type"},
     "Expect the response to have this content type.\n",
     CmdOptionValueReq::required,
     "ct",
     [](const std::string &value) { g_configuration.content_type = value; }},

    {{"--response-type", "-r"},
     "Define expected response type by the server.\n"
     "By default its JSON, where allowed values are: JSON,RAW, BINARY(same "
     "as "
     "raw still shouldn't y).",
     CmdOptionValueReq::required,
     "type",
     [](const std::string &value) {
       if (!optional_set(response_type_convert(value),
                         &g_configuration.response_type))
         throw std::invalid_argument("Invalid parameter for response type.");
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Response type, shoudn't be used with authentication flow.");
       }
     }},

    {{"--json-schema-file"},
     "Specify a file that contains JSON schema, which should be used for "
     "response validation.\n",
     CmdOptionValueReq::required,
     "json_schema_file",
     [](const std::string &value) {
       std::ifstream file{value};
       if (!file.is_open()) {
         throw std::runtime_error("File with `json-scheme`, cannot be opened.");
       }

       g_configuration.json_schema =
           std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Response type, shoudn't be used with authentication flow.");
       }

       if (g_configuration.response_type != http_client::ResponseType::kJson) {
         throw std::invalid_argument(
             "Json schema can only be used with JSON responses.");
       }
     }},

    {{"--json-schema"},
     "Specify the JSON schema, which should be used for "
     "response validation.\n",
     CmdOptionValueReq::required,
     "json_schema",
     [](const std::string &value) { g_configuration.json_schema = value; },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Response type, shoudn't be used with authentication flow.");
       }

       if (g_configuration.response_type != http_client::ResponseType::kJson) {
         throw std::invalid_argument(
             "Json schema can only be used with JSON responses.");
       }
     }},
    {{"--encoded-json-schema"},
     "Specify the JSON schema, which should be used for "
     "response validation.\n",
     CmdOptionValueReq::required,
     "json_schema",
     [](const std::string &value) {
       g_configuration.json_schema = decode_from_url_query_escaping(value);
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Response type, shoudn't be used with authentication flow.");
       }

       if (g_configuration.response_type != http_client::ResponseType::kJson) {
         throw std::invalid_argument(
             "Json schema can only be used with JSON responses.");
       }
     }},
    {{"--encoded-expected-header"},
     "Expect header/value in server's response message. The value must be "
     "encoded using url-parameter encoding.",
     CmdOptionValueReq::required,
     "meta_header",
     [](const std::string &value) {
       auto equal_pos = value.find_first_of("=");
       if (equal_pos == std::string::npos) {
         throw std::invalid_argument(
             "Expected header value must follow this format: "
             "'--encoded-expected-header=HEADER=VALUE'");
       }
       std::string key = value.substr(0, equal_pos);
       std::string keys_value =
           decode_from_url_query_escaping(value.substr(equal_pos + 1));
       g_configuration.expected_headers[key] = keys_value;
     }},
    {{"--protocol"},
     "Force wire protocol. The value must be on of following values: HTTP1, "
     "HTTP2. Where the default is HTTP1.",
     CmdOptionValueReq::required,
     "meta_protcol",
     [](const std::string &value) {
       if (!optional_set(protocol_type_convert(value),
                         &g_configuration.wire_protocol)) {
         throw std::invalid_argument("Invalid value for wire-protocol: " +
                                     value);
       }
     }},
    {{"--expected-fatal-error"},
     "Expect that HTTP client will return connection or protocol error. Match "
     "the error description with specified value.",
     CmdOptionValueReq::required,
     "meta_fatal_error",
     [](const std::string &value) {
       g_configuration.expect_fatal_error = value;
     }},
    {{"--request-header-upgrade"},
     "Specifies the value of the Upgrade header to be included in the HTTP/1.1 "
     "request, typically used to initiate protocol switching (e.g., to "
     "WebSocket).",
     CmdOptionValueReq::required,
     "meta_header_upgrade",
     [](const std::string &value) { g_configuration.header_upgrade = value; }},
    {{"--request-header-http2-settings"},
     "Specifies the value of the HTTP2-Settings header to be included in the "
     "HTTP/1.1 request, used during HTTP/2 protocol upgrades to convey HTTP/2 "
     "settings in base64url-encoded format.",
     CmdOptionValueReq::required,
     "meta_header_upgrade",
     [](const std::string &value) {
       g_configuration.header_http2_settings = value;
     }},
};

template <typename Duration>
bool should_retry(Duration start, const Result &r) {
  auto expected_status = r.status == g_configuration.expected_status;
  auto waiting_for = Duration::clock::now() - start;

  if (g_configuration.wait_until_status.has_value() && !expected_status) {
    if (waiting_for < g_configuration.wait_until_status.value()) {
      std::this_thread::sleep_for(Seconds(1));
      return true;
    }
  }

  return false;
}

static Result execute_http_flow(HttpClientRequest *request,
                                const std::string &path) {
  auto start = std::chrono::steady_clock::now();
  Result r;
  mrs_client::Authentication b;

  do {
    switch (g_configuration.authentication) {
      case http_client::AuthenticationType::kNone: {
        r = request->do_request(g_configuration.request, path,
                                g_configuration.payload);
      } break;

      case http_client::AuthenticationType::kBasicJson: {
        r = b.do_basic_json_flow(
            request, path, g_configuration.user, g_configuration.password,
            g_configuration.session_type, g_configuration.auth_app);
      } break;
      case http_client::AuthenticationType::kBasic: {
        r = b.do_basic_flow(
            request, path, g_configuration.user, g_configuration.password,
            g_configuration.session_type, g_configuration.auth_app);
      } break;
      case http_client::AuthenticationType::kScramGet: {
        r = b.do_scram_get_flow(
            request, path, g_configuration.user, g_configuration.password,
            g_configuration.session_type, g_configuration.auth_app);
      } break;
      case http_client::AuthenticationType::kScramPost: {
        r = b.do_scram_post_flow(
            request, path, g_configuration.user, g_configuration.password,
            g_configuration.session_type, g_configuration.auth_app);
      } break;

      default: {
        assert(false && "Not implemented");
      }
    }
  } while (should_retry(start, r));

  return r;
}

rapidjson::SchemaDocument get_json_schema() {
  rapidjson::Document doc;
  if (doc.Parse(g_configuration.json_schema.c_str()).HasParseError()) {
    throw std::runtime_error("JSON schema is not valid.");
  }

  return rapidjson::SchemaDocument{doc};
}

static std::string get_pointer_string(const rapidjson::Pointer &pointer) {
  rapidjson::StringBuffer buff;
  pointer.StringifyUriFragment(buff);
  return {buff.GetString(), buff.GetSize()};
}

json::JsonCopyPointers create_json_copier() {
  if (!g_configuration.json_pointer.empty())
    return json::JsonCopyPointers{g_configuration.json_pointer};

  return json::JsonCopyPointers{g_configuration.excluscive_json_pointer, true};
}

static void validate_result(Result &result) {
  result.ok = g_configuration.expected_status == result.status;

  if (g_configuration.expect_fatal_error.has_value()) {
    result.ok = false;
    std::cerr << "ERROR: Expected a fatal error, but the operation completed "
                 "without triggering one..\n";
    return;
  }

  if (!result.ok) return;

  auto content_type =
      mrs_client::find_in_headers(result.headers, "Content-Type");

  if (!g_configuration.content_type.empty()) {
    if (g_configuration.content_type != content_type) {
      result.ok = false;
      std::cerr << "ERROR: expected that content-type points to '"
                << g_configuration.content_type << ";.\n";
    }
  }

  for (auto &[k, v] : g_configuration.expected_headers) {
    auto header_value = mrs_client::try_find_in_headers(result.headers, k);
    if (!header_value) {
      result.ok = false;
      std::cerr << "ERROR: missing expected header: '" << k << "'.\n";
      continue;
    }
    if (header_value.value() != v) {
      std::cerr << "ERROR: Wrong value for header: '" << k << ", expecting: '";

      std::cerr << v << "', but received: '" << header_value.value() << "'.\n";
      result.ok = false;
    }
  }

  if (!result.ok) return;

  if (g_configuration.response_type != http_client::ResponseType::kJson) {
    return;
  }

  // application/problem+json
  // application/json
  if (!helper::contains(content_type, "json")) {
    result.ok = false;
    std::cerr << "ERROR: expected that content-type points to JSON.\n";
    return;
  }

  rapidjson::Document doc = helper::json::text_to_document(result.body);

  if (doc.HasParseError()) {
    std::cerr << "JSON parser error: " << static_cast<int>(doc.GetParseError())
              << ", at " << doc.GetErrorOffset() << std::endl;
    result.ok = false;
    return;
  }

  if (!g_configuration.json_schema.empty()) {
    auto schema = get_json_schema();
    rapidjson::SchemaValidator v(schema);
    if (!doc.Accept(v)) {
      std::cerr << "JSON validation location "
                << get_pointer_string(v.GetInvalidDocumentPointer())
                << " failed requirement: '" << v.GetInvalidSchemaKeyword()
                << "' at meta schema location '"
                << get_pointer_string(v.GetInvalidSchemaPointer()) << "'";
      result.ok = false;
      return;
    }
  }

  bool json_filter{false};
  if (!g_configuration.json_pointer.empty() ||
      !g_configuration.excluscive_json_pointer.empty()) {
    json::JsonCopyPointers jcp = create_json_copier();
    doc.Accept(jcp);
    json_filter = true;
    auto not_matched = jcp.get_not_matched_pointers();
    if (!not_matched.empty()) {
      result.ok = false;
      std::cerr << "ERROR: JSON pointer points to not existing node.\n";
      for (auto &s : not_matched) {
        std::cerr << "ERROR: Problem with pointer: " << s << std::endl;
      }
      return;
    }
    doc.CopyFrom(jcp.get_document(), doc.GetAllocator());
  }

  helper::json::rapid_json_to_text<PrettyWriter>(&doc, result.body);

  // the filter didn't select any object
  if (json_filter && doc.IsNull()) result.body = "";
}

static bool is_debug_log_enabled() {
  auto d = getenv("DEBUG");
  if (!d) return false;

  return 0 == strcmp(d, "1") || 0 == strcmp(d, "true") ||
         0 == strcmp(d, "TRUE");
}

static void print_error_info(std::ostream &out,
                             const std::vector<std::string> &messages) {
  for (const auto &m : messages) {
    out << "ERROR: " << m << "\n";
  }
  out << std::endl;
}

int main_app(int argc, char *argv[]) {
  mrs_client_app_debug = is_debug_log_enabled();
  g_executable = argv[0];
  std::vector<std::string> arguments;

  for (int arg_idx = 1; arg_idx < argc; ++arg_idx) {
    auto &arg_ref = arguments.emplace_back(argv[arg_idx]);
    if (arg_ref.length() > 1) {
      // Remove quotes, implemented for windows MTR tests.
      if (*arg_ref.begin() == '\'' && *arg_ref.rbegin() == '\'')
        arg_ref = arg_ref.substr(1, arg_ref.size() - 2);
    }
  }

  for (auto &o : g_options) {
    g_cmd_handler.add_option(o);
  }

  try {
    TlsLibraryContext tls_ctxt;
    auto &display = g_configuration.display;
    g_cmd_handler.process(arguments);

    if (g_configuration.help) {
      print_usage();
      print_description();
      return 0;
    }

    verify_required_arguments();

    http::base::Uri uri{g_configuration.url};
    net::io_context ctx;
    HttpClientSession session{g_configuration.session_file};
    HttpClientRequest request{
        &ctx, &session, uri,
        g_configuration.wire_protocol == http_client::WireProtocol::kHttp_2};
    std::vector<std::string> header_connection;

    if (!g_configuration.accept.empty()) {
      request.add_header("Accept", g_configuration.accept.c_str());
    }

    if (!g_configuration.authorization.empty()) {
      request.add_header("Authorization",
                         g_configuration.authorization.c_str());
    }

    if (!g_configuration.host.empty()) {
      request.add_header("Host", g_configuration.host.c_str());
    }

    if (g_configuration.header_upgrade.has_value()) {
      request.add_header("Upgrade",
                         g_configuration.header_upgrade.value().c_str());
      header_connection.push_back("Upgrade");
    }

    if (g_configuration.header_http2_settings.has_value()) {
      request.add_header("HTTP2-Settings",
                         g_configuration.header_http2_settings.value().c_str());
      header_connection.push_back("HTTP2-Settings");
    }

    if (!header_connection.empty()) {
      request.add_header("Connection",
                         mysql_harness::join(header_connection, ",").c_str());
    }

    auto result = execute_http_flow(&request, g_configuration.path);
    validate_result(result);

    print_results(result, result.ok ? display : Display::display_all());

    if (!g_configuration.write_to_file.empty() && result.ok) {
      std::ofstream out{g_configuration.write_to_file};
      switch (g_configuration.write_format) {
        case http_client::WriteFileFormat::kRaw:
          out << result.body;
          break;
        case http_client::WriteFileFormat::kMTR: {
          auto r = result.body;
          if (g_configuration.response_type ==
              http_client::ResponseType::kJson) {
            if (g_configuration.json_pointer.size() == 1) {
              rapidjson::Document doc = helper::json::text_to_document(r);
              rapidjson::Pointer p{g_configuration.json_pointer[0].c_str()};
              auto *v = rapidjson::GetValueByPointer(doc, p);
              r = helper::json::to_string(v);
            }
          } else if (g_configuration.response_type ==
                     http_client::ResponseType::kBinary) {
            r = helper::string::hex<decltype(r), CustomHex>(r);
          }
          out << "let $mrs_result=" << r << ";" << std::endl;
          break;
        }
      }
    }

    return result.ok ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::exception &e) {
    std::vector<std::string> messages{{e.what()}};
    if (g_configuration.expect_fatal_error.has_value()) {
      if (g_configuration.expect_fatal_error.value() == e.what()) {
        print_error_info(std::cout, messages);
        return EXIT_SUCCESS;
      }
      messages.push_back(
          "Mismatch detected — expected a different error than "
          "the one printed above.");
    }
    print_error_info(std::cerr, messages);

    print_usage();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  mysql_library_init(argc, argv, nullptr);
  auto result = main_app(argc, argv);
  mysql_library_end();

  return result;
}
