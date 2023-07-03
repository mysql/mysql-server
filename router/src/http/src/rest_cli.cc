/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "dim.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/tls_client_context.h"
#include "mysqlrouter/http_request.h"
#include "mysqlrouter/rest_client.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

/**
 * exception thrown by the frontend.
 *
 * Should be presented to the user.
 */
class FrontendError : public std::runtime_error {
 public:
  FrontendError(const std::string &what) : std::runtime_error(what) {}
};

struct RestClientConfig {
  std::string content_type{"application/json"};
  bool verbose{false};
  bool request_data_stdin{false};
  HttpMethod::type method{HttpMethod::Get};
  std::string request_data;
  std::string ssl_ca_file;
  std::string ssl_ca_dir;
  std::string ssl_cipher;
};

class RestClientFrontend {
 public:
  RestClientFrontend(const std::vector<std::string> &arguments) {
    program_name_ = arguments[0];

    prepare_command_options();
    try {
      arg_handler_.process(
          std::vector<std::string>{arguments.begin() + 1, arguments.end()});
    } catch (const std::invalid_argument &e) {
      throw FrontendError(e.what());
    }
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

  /**
   * run app according to commandline optiions.
   *
   * @returns exit-code
   * @retval EXIT_SUCESS success
   * @retval EXIT_FAILURE on error
   * @throws FrontendError
   */
  int run();

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
        CmdOption::OptionNames({"--ssl-ca"}), "trusted CAs.",
        CmdOptionValueReq::required, "filename",
        [this](const std::string &val) { config_.ssl_ca_file = val; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-ca-dir"}), "trusted CAs.",
        CmdOptionValueReq::required, "filename",
        [this](const std::string &val) { config_.ssl_ca_dir = val; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-cipher"}), "trusted CAs.",
        CmdOptionValueReq::required, "ciphers",
        [this](const std::string &val) { config_.ssl_cipher = val; });
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

  CmdArgHandler arg_handler_{true};
  bool do_print_and_exit_{false};
};

int RestClientFrontend::run() {
  if (is_print_and_exit()) {
    return 0;
  }

  if (!config_.request_data.empty() && config_.request_data_stdin) {
    throw FrontendError("--data-stdin and --data are mutual exclusive options");
  }

  auto &rest_args = arg_handler_.get_rest_arguments();
  auto rest_args_count = rest_args.size();

  if (rest_args_count != 1) {
    throw FrontendError("URI is required");
  }

  HttpUri u{HttpUri::parse(rest_args.at(0))};

  if (u.get_scheme().empty()) {
    throw FrontendError("scheme required in URI");
  }
  if (u.get_host().empty()) {
    throw FrontendError("host required in URI");
  }

  if (u.get_port() == 65535u) {
    if (u.get_scheme() == "http") {
      u.set_port(80);
    } else if (u.get_scheme() == "https") {
      u.set_port(443);
    } else {
      throw FrontendError("unknown scheme");
    }
  }

  std::string request_data;
  if (config_.request_data_stdin) {
    while (true) {
      char buf[128];
      auto bytes_read = ::fread(buf, 1, sizeof(buf), stdin);

      if (bytes_read == 0) {
        if (feof(stdin)) {
          break;
        } else if (ferror(stdin)) {
          throw FrontendError("reading from stdin failed");
        } else {
          throw FrontendError("fread() returned 0, but no EOF nor error?");
        }
      }
      request_data.append(buf, buf + bytes_read);
    }
  } else {
    request_data = config_.request_data;
  }

  IOContext io_ctx;
  TlsClientContext tls_ctx;
  std::unique_ptr<HttpClient> http_client;
  if (u.get_scheme() == "https") {
#ifdef EVENT__HAVE_OPENSSL
    if (!config_.ssl_ca_file.empty() || !config_.ssl_ca_dir.empty()) {
      if (!tls_ctx.ssl_ca(config_.ssl_ca_file, config_.ssl_ca_dir)) {
        throw FrontendError("setting CA's failed");
      }
    }
    if (!config_.ssl_cipher.empty()) {
      const auto res = tls_ctx.cipher_list(config_.ssl_cipher);
      if (!res) {
        throw FrontendError(res.error().message());
      }
    }
    http_client = std::make_unique<HttpsClient>(io_ctx, std::move(tls_ctx),
                                                u.get_host(), u.get_port());
#else
    throw FrontendError("HTTPS support disabled at buildtime");
#endif
  } else {
    http_client =
        std::make_unique<HttpClient>(io_ctx, u.get_host(), u.get_port());
  }

  RestClient client(std::move(http_client));

  auto req = client.request_sync(config_.method, u.get_path(), request_data,
                                 config_.content_type);
  if (req) {
    if (req.get_response_code() > 0) {
      if (config_.verbose) {
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
      // "timeout" is returned for ECONNREFUSED
      //
      // the evhttp_request_closecb() may help capture
      std::cerr << u.get_scheme() << " request to " << u.get_host() << ":"
                << std::to_string(u.get_port())
                << " failed: " << req.error_msg() << std::endl;
      return -1;
    }
  } else {
    std::cerr << u.get_scheme() << " request to " << u.get_host() << ":"
              << std::to_string(u.get_port())
              << " failed (early): " << req.error_msg()
              << (req.socket_error_code()
                      ? (", system-error: " + req.socket_error_code().message())
                      : "")
              << ", client-error: " << client.error_msg() << std::endl;
    return -1;
  }

  return 0;
}

int main(int argc, char **argv) {
  TlsLibraryContext tls_lib_ctx;
  try {
    RestClientFrontend frontend(std::vector<std::string>{argv, argv + argc});

    return frontend.run();
  } catch (const FrontendError &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }
}
