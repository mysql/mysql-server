/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <stdio.h>
#include <iostream>
#include <sstream>

#include "dim.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/registry.h"
#include "mysqlrouter/http_common.h"
#include "mysqlrouter/rest_client.h"

struct RestClientConfig {
  std::string uri{"/"};
  std::string hostname;
  std::string content_type{"application/json"};
  uint16_t port{80};
  bool verbose{false};
  bool request_data_stdin{false};
  HttpMethod::type method{HttpMethod::Get};
  std::string request_data;
};

class RestClientFrontend {
 public:
  RestClientConfig init_from_arguments(
      const std::vector<std::string> &arguments) {
    program_name_ = arguments[0];

    prepare_command_options();
    arg_handler_.process(
        std::vector<std::string>{arguments.begin() + 1, arguments.end()});

    return config_;
  }

  bool is_print_and_exit() const noexcept { return do_print_and_exit_; }

  std::string get_version_line() const noexcept { return MYSQL_ROUTER_VERSION; }

  std::string get_help() const {
    std::stringstream os;

    for (auto line :
         arg_handler_.usage_lines("Usage: " + program_name_, "", 80)) {
      os << line << std::endl;
    }

    return os.str();
  }

 private:
  RestClientConfig config_;
  std::string program_name_;

  void prepare_command_options() {
    arg_handler_.add_option(
        CmdOption::OptionNames({"-V", "--version"}),
        "Display version information and exit.", CmdOptionValueReq::none, "",
        [this](const std::string &) {
          std::cout << this->get_version_line() << std::endl;
          this->do_print_and_exit_ = true;
        });

    arg_handler_.add_option(
        CmdOption::OptionNames({"-?", "--help"}), "Display this help and exit.",
        CmdOptionValueReq::none, "", [this](const std::string &) {
          std::cout << this->get_help() << std::endl;
          this->do_print_and_exit_ = true;
        });

    arg_handler_.add_option(
        CmdOption::OptionNames({"--uri"}), "URI", CmdOptionValueReq::required,
        "uri", [this](const std::string &uri) { config_.uri = uri; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"-P", "--port"}),
        "TCP port to listen on for classic protocol connections.",
        CmdOptionValueReq::required, "int", [this](const std::string &port) {
          config_.port = static_cast<unsigned>(std::stoul(port));
        });
    arg_handler_.add_option(
        CmdOption::OptionNames({"-h", "--host"}), "hostname of HTTP server.",
        CmdOptionValueReq::required, "hostname",
        [this](const std::string &hostname) { config_.hostname = hostname; });
    arg_handler_.add_option(CmdOption::OptionNames({"--content-type"}),
                            "Content-Type of the request-body.",
                            CmdOptionValueReq::required, "string",
                            [this](const std::string &content_type) {
                              config_.content_type = content_type;
                            });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--data"}), "request-body.",
        CmdOptionValueReq::required, "string",
        [this](const std::string &data) { config_.request_data = data; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--data-stdin"}),
        "read request-body from stdin.", CmdOptionValueReq::none, "",
        [this](const std::string &) { config_.request_data_stdin = true; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--verbose"}), "verbose",
        CmdOptionValueReq::none, "",
        [this](const std::string &) { config_.verbose = true; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--method"}), "HTTP method",
        CmdOptionValueReq::required, "{GET|POST|PUT|DELETE|...}",
        [this](const std::string &method) {
          std::map<std::string, HttpMethod::type> methods{
              {"GET", HttpMethod::Get},         {"PUT", HttpMethod::Put},
              {"POST", HttpMethod::Post},       {"DELETE", HttpMethod::Delete},
              {"CONNECT", HttpMethod::Connect}, {"TRACE", HttpMethod::Trace},
              {"PATCH", HttpMethod::Patch},     {"HEAD", HttpMethod::Head},
              {"OPTIONS", HttpMethod::Options}};
          auto it = methods.find(method);

          if (it == methods.end()) {
            throw std::logic_error("unknown method: " + method);
          } else {
            config_.method = (*it).second;
          }
        });
  }

  CmdArgHandler arg_handler_;
  bool do_print_and_exit_{false};
};

int main(int argc, char **argv) {
  IOContext io_ctx;

  try {
    RestClientFrontend frontend;
    auto frontend_config = frontend.init_from_arguments(
        std::vector<std::string>{argv, argv + argc});

    if (frontend.is_print_and_exit()) {
      return 0;
    }

    if (!frontend_config.request_data.empty() &&
        frontend_config.request_data_stdin) {
      throw std::logic_error(
          "--data-stdin and --data are mutual exclusive options");
    }

    if (frontend_config.hostname.empty()) {
      throw std::logic_error("--host must be set");
    }

    std::string request_data;
    if (frontend_config.request_data_stdin) {
      while (true) {
        char buf[128];
        auto bytes_read = ::fread(buf, 1, sizeof(buf), stdin);

        if (bytes_read == 0) {
          if (feof(stdin)) {
            break;
          } else if (ferror(stdin)) {
            throw std::runtime_error("reading from stdin failed");
          } else {
            throw std::runtime_error(
                "fread() returned 0, but no EOF nor error?");
          }
        }
        request_data.append(buf, buf + bytes_read);
      }
    } else {
      request_data = frontend_config.request_data;
    }

    auto req =
        RestClient(io_ctx, frontend_config.hostname, frontend_config.port)
            .request_sync(frontend_config.method, frontend_config.uri,
                          request_data, frontend_config.content_type);
    if (req) {
      if (req.get_response_code() > 0) {
        if (frontend_config.verbose) {
          std::cerr << "> " << req.get_response_code() << " "
                    << req.get_response_code_line() << std::endl;
          for (auto const &hdr : req.get_input_headers()) {
            std::cerr << "> " << hdr.first << ": " << hdr.second << std::endl;
          }
        }
        auto resp_body = req.get_input_buffer();
        auto resp_body_content = resp_body.pop_front(resp_body.length());

        std::cout << std::string(resp_body_content.begin(),
                                 resp_body_content.end())
                  << std::endl;
        return 0;
      } else {
        std::cerr << "HTTP Request to " << frontend_config.hostname << ":"
                  << std::to_string(frontend_config.port)
                  << " failed: " << req.error_msg() << std::endl;
        return -1;
      }
    } else {
      // we don't really know why it failed.
      std::cerr << "HTTP Request to " << frontend_config.hostname << ":"
                << std::to_string(frontend_config.port)
                << " failed (early): " << req.error_msg() << std::endl;
      return -1;
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }
}
