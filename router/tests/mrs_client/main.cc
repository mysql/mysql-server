/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include <iostream>
#include <map>
#include <vector>

#include "helper/json/rapid_json_to_text.h"
#include "helper/json/text_to.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/tls_context.h"
#include "mysqlrouter/http_client.h"

#include "client/basic_authentication.h"
#include "client/http_client_request.h"
#include "client/session.h"
#include "configuration/application_configuration.h"

#include <rapidjson/pointer.h>

using mrs_client::HttpClientRequest;
using mrs_client::HttpClientSession;
using mrs_client::Result;
using mrs_client::Url;
using Display = http_client::ApplicationDisplay;

const int kHelpScreenWidth = 1024;
const int kHelpScreenIdent = 4;
std::string g_executable;
CmdArgHandler g_cmd_handler{};
http_client::ApplicationConfiguration g_configuration;

static bool display_type_convert(const std::string &value,
                                 http_client::ApplicationDisplay &d) {
  using Display = http_client::ApplicationDisplay;
  using MemberBool = bool Display::*;
  static std::map<std::string, MemberBool> allowed_values{
      {"status", &Display::status},
      {"header", &Display::header},
      {"body", &Display::body},
      {"result", &Display::result},
      {"title", &Display::title}};

  for (auto &[key, value] : allowed_values) {
    d.*(value) = false;
  }

  auto types = mysql_harness::split_string(value, ',', false);

  if (types.empty()) return false;

  for (auto &type : types) {
    auto ltype = mysql_harness::make_lower(type);
    auto it = allowed_values.find(ltype);
    if (allowed_values.end() == it) return false;
    d.*(it->second) = true;
  }

  return true;
}

static bool session_type_convert(std::string value,
                                 http_client::SessionType *out_at = nullptr) {
  using namespace http_client;
  const static std::map<std::string, SessionType> map{
      {"cookie", SessionType::kCookie}, {"jwt", SessionType::kJWT}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return false;
  if (out_at) *out_at = it->second;

  return true;
}

static bool authentication_type_convert(
    std::string value, http_client::AuthenticationType *out_at = nullptr) {
  using namespace http_client;
  const static std::map<std::string, AuthenticationType> map{
      {"none", AuthenticationType::kNone},
      {"basic", AuthenticationType::kBasic},
      {"scram", AuthenticationType::kBasic},
      {"oauth2_f", AuthenticationType::kOauth2}};

  mysql_harness::lower(value);
  auto it = map.find(value);

  if (map.end() == it) return false;

  if (out_at) *out_at = it->second;

  return true;
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

static void present_results(const std::string &json_ptr, const Result &result,
                            const Display &display) {
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

    if (!json_ptr.empty()) {
      auto doc = helper::json::text_to_document(result.body);
      auto ptr = rapidjson::GetValueByPointer(
          doc, rapidjson::Pointer(json_ptr.c_str()));
      if (nullptr == ptr) {
        // TODO(lkotula): Move the extraction of the pointer outside this
        // function (Shouldn't be in review)
        g_configuration.expected_status = -1;
      } else {
        helper::json::rapid_json_to_text(ptr, body);
      }
    }
    std::cout << body << std::endl;
  }

  if (display.result) {
    std::cout << ((g_configuration.expected_status == result.status) ? "OK"
                                                                     : "FAILED")
              << std::endl;
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
     "NONE,BASIC,SCRAM,OAUTH2_F",
     CmdOptionValueReq::required,
     "meta_auth",
     [](const std::string &value) {
       if (!authentication_type_convert(value, &g_configuration.authentication))
         throw std::invalid_argument(
             "Invalid parameter for authentication type.");
     },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         if (g_configuration.user.empty())
           throw std::invalid_argument(
               "User name is required (password optional), when executing "
               "authentication flow.");
       }
     }},

    {{"--path"},
     "Overwrite the path specified in URL. Using this parameter, user may "
     "split the URL on host part specified in --url and path.",
     CmdOptionValueReq::required,
     "meta_path",
     [](const std::string &value) { g_configuration.path = value; }},
    {{"--session-type", "-s"},
     "Define how the session should be identified by the client. Allowed "
     "values: "
     "COOKIE,JWT. Where default value is COOKIE.",
     CmdOptionValueReq::required,
     "meta_auth_session",
     [](const std::string &value) {
       if (!session_type_convert(value, &g_configuration.session_type))
         throw std::invalid_argument(
             "Invalid parameter for authentication type.");
     },
     [](const std::string &) {
       if (!cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Session type, can be defined while executing authentication "
             "flow.");
       }
     }},

    {{"--json-pointer", "-j"},
     "Print only value selected by pointer.",
     CmdOptionValueReq::required,
     "meta_json_pointer",
     [](const std::string &value) { g_configuration.json_pointer = value; },
     [](const std::string &) {
       if (cnf_should_execute_authentication_flow()) {
         throw std::invalid_argument(
             "Json pointer can't be used while executing authentication "
             "flow.");
       }
     }},
    {{"--expected-status"},
     "Specify allowed status code. Default is OK(200).",
     CmdOptionValueReq::required,
     "meta_status",
     [](const std::string &value) {
       g_configuration.expected_status = atoi(value.c_str());
       if (0 == g_configuration.expected_status)
         throw std::invalid_argument(
             "Invalid value specified for 'expected-status', allowed values "
             "are positive integers.");
     }},

    {{"--display"},
     "What should be presented as output: VALUES=VALUE[,VALUE[....]]"
     "where VALUE can be: BODY, HEADER, STATUS, RESULT. By default its set to "
     "RESULT.",
     CmdOptionValueReq::required,
     "meta_display",
     [](const std::string &value) {
       if (!display_type_convert(value, g_configuration.display))
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
     "By default its GET, where allowed values are: GET,POST,PUT,DELETE.",
     CmdOptionValueReq::required,
     "type",
     [](const std::string &value) {
       if (!http_client::Request::convert(value, &g_configuration.request))
         throw std::invalid_argument("Invalid parameter for request type.");
     }},
};

static Result execute_http_flow(HttpClientRequest &request, const Url &url) {
  switch (g_configuration.authentication) {
    case http_client::AuthenticationType::kNone:
      return request.do_request(g_configuration.request, url.get_request(), {});

    case http_client::AuthenticationType::kBasic: {
      mrs_client::BasicAuthentication b;
      return b.do_basic_flow_with_session(request, g_configuration.url,
                                          g_configuration.user,
                                          g_configuration.password);
    }

    default: {
      assert(false && "Not implemented");
    }
  }

  return {};
}

int main(int argc, char *argv[]) {
  g_executable = argv[0];
  std::vector<std::string> arguments;

  for (int arg_idx = 1; arg_idx < argc; ++arg_idx) {
    arguments.emplace_back(argv[arg_idx]);
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

    Url url{g_configuration.url, g_configuration.path};
    IOContext ctx;
    HttpClientSession session{g_configuration.session_file};
    HttpClientRequest request{&ctx, &session, &url};

    auto result = execute_http_flow(request, url);
    auto result_ok = g_configuration.expected_status == result.status;

    present_results(g_configuration.json_pointer, result,
                    result_ok ? display : Display::display_all());
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    print_usage();
    return -1;
  }

  return 0;
}
