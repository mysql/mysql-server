/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <direct.h>  // getcwd
#include <winsock2.h>
#else
#include <limits.h>  // PATH_MAX
#include <unistd.h>  // getcwd
#endif

#include "dim.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/registry.h"

#ifndef PATH_MAX
#ifdef _MAX_PATH
// windows has _MAX_PATH instead
#define PATH_MAX _MAX_PATH
#endif
#endif

constexpr unsigned kHelpScreenWidth = 72;
constexpr unsigned kHelpScreenIndent = 8;

struct MysqlServerMockConfig {
  std::string queries_filename;
  std::string module_prefix;
  unsigned port{3306};
  unsigned http_port{0};
  bool verbose{false};
};

static void init_DIM() {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  // logging facility
  dim.set_LoggingRegistry(
      []() {
        static mysql_harness::logging::Registry registry;
        return &registry;
      },
      [](mysql_harness::logging::Registry *) {}  // don't delete our static!
  );
}

class MysqlServerMockFrontend {
 public:
  std::string get_version_line() { return MYSQL_ROUTER_VERSION; }

  std::string get_help() {
    std::stringstream os;

    for (auto line : arg_handler_.usage_lines("Usage: mysql_server_mock", "",
                                              kHelpScreenWidth)) {
      os << line << std::endl;
    }

    os << "\nOptions:" << std::endl;
    for (auto line : arg_handler_.option_descriptions(kHelpScreenWidth,
                                                      kHelpScreenIndent)) {
      os << line << std::endl;
    }

    return os.str();
  }

  MysqlServerMockConfig init_from_arguments(
      const std::vector<std::string> &arguments) {
    program_name_ = arguments[0];
    origin_dir_ = mysql_harness::Path(program_name_).dirname();

    prepare_command_options();
    arg_handler_.process(
        std::vector<std::string>{arguments.begin() + 1, arguments.end()});

    return config_;
  }

  bool is_print_and_exit() { return do_print_and_exit_; }

  void run() {
    std::unique_ptr<mysql_harness::Loader> loader_;

    init_DIM();
    std::unique_ptr<mysql_harness::LoaderConfig> loader_config(
        new mysql_harness::LoaderConfig(mysql_harness::Config::allow_keys));

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

    mysql_harness::Config config;
    const mysql_harness::logging::LogLevel log_level =
        mysql_harness::logging::get_default_log_level(config);

    mysql_harness::logging::clear_registry(registry);
    mysql_harness::logging::create_module_loggers(
        registry, log_level,
        {mysql_harness::logging::kMainLogger, "mock_server", "http_server", "",
         "rest_mock_server"},
        mysql_harness::logging::kMainLogger);
    mysql_harness::logging::create_main_log_handler(registry, "", "", true);

    registry.set_ready();

    if (config_.module_prefix.empty()) {
      char cwd[PATH_MAX];

      if (nullptr == getcwd(cwd, sizeof(cwd))) {
        throw std::system_error(errno, std::generic_category());
      }

      config_.module_prefix = cwd;
    }

    // log to stderr
    loader_config->set_default("logging_folder", "");
    loader_config->add("logger");
    loader_config->get("logger", "")
        .add("level", config_.verbose ? "debug" : "warning");

    // assume all path relative to the installed binary
    auto plugin_dir = mysql_harness::get_plugin_dir(origin_dir_.str());
    loader_config->set_default("plugin_folder", plugin_dir);

    // those are unused, but must be set
    auto base_path = mysql_harness::Path(origin_dir_).join("..");
    loader_config->set_default(
        "runtime_folder",
        mysql_harness::Path(base_path).join("var").join("lib").str());
    loader_config->set_default(
        "config_folder", mysql_harness::Path(base_path).join("etc").str());
    loader_config->set_default(
        "data_folder",
        mysql_harness::Path(base_path).join("var").join("share").str());

    if (config_.http_port != 0) {
      auto &rest_mock_server_config =
          loader_config->add("rest_mock_server", "");
      rest_mock_server_config.set("library", "rest_mock_server");

      auto &http_server_config = loader_config->add("http_server", "");
      http_server_config.set("library", "http_server");
      http_server_config.set("port", std::to_string(config_.http_port));
      http_server_config.set("static_folder", "");
    }

    auto &mock_server_config = loader_config->add("mock_server", "");
    mock_server_config.set("library", "mock_server");
    mock_server_config.set("port", std::to_string(config_.port));
    mock_server_config.set("filename", config_.queries_filename);
    mock_server_config.set("module_prefix", config_.module_prefix);

    mysql_harness::DIM::instance().set_Config(
        [&]() { return loader_config.release(); },
        std::default_delete<mysql_harness::LoaderConfig>());

    try {
      loader_.reset(new mysql_harness::Loader("server-mock", *loader_config));
    } catch (const std::runtime_error &err) {
      throw std::runtime_error(std::string("init-loader failed: ") +
                               err.what());
    }

    loader_->start();
  }

 private:
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

    arg_handler_.add_option(CmdOption::OptionNames({"-f", "--filename"}),
                            "tracefile to load.", CmdOptionValueReq::required,
                            "filename", [this](const std::string &filename) {
                              config_.queries_filename = filename;
                            });
    arg_handler_.add_option(
        CmdOption::OptionNames({"-P", "--port"}),
        "TCP port to listen on for classic protocol connections.",
        CmdOptionValueReq::required, "int", [this](const std::string &port) {
          config_.port = static_cast<unsigned>(std::stoul(port));
        });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--http-port"}),
        "TCP port to listen on for HTTP/REST connections.",
        CmdOptionValueReq::required, "int", [this](const std::string &port) {
          config_.http_port = static_cast<unsigned>(std::stoul(port));
        });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--module-prefix"}),
        "path prefix for javascript modules (default current directory).",
        CmdOptionValueReq::required, "path",
        [this](const std::string &module_prefix) {
          config_.module_prefix = module_prefix;
        });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--verbose"}), "verbose",
        CmdOptionValueReq::none, "",
        [this](const std::string &) { config_.verbose = true; });
  }

  CmdArgHandler arg_handler_;
  bool do_print_and_exit_{false};

  MysqlServerMockConfig config_;

  std::string program_name_;
  mysql_harness::Path origin_dir_;
};

int main(int argc, char *argv[]) {
  MysqlServerMockFrontend frontend;

#ifdef _WIN32
  register_ctrl_c_handler();

  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed with error: " << result << std::endl;
    return -1;
  }
#endif

  std::vector<std::string> arguments{argv, argv + argc};
  try {
    auto frontend_config = frontend.init_from_arguments(arguments);

    if (frontend.is_print_and_exit()) {
      return 0;
    }

    frontend.run();
  } catch (const std::exception &e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}
