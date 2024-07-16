/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include <array>
#include <csignal>
#include <iostream>  // cerr
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "dim.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/process_state_component.h"
#include "mysql/harness/signal_handler.h"
#include "mysql/harness/stdx/filesystem.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

constexpr unsigned kHelpScreenWidth = 72;
constexpr unsigned kHelpScreenIndent = 8;

struct MysqlServerMockConfig {
  std::string queries_filename;
  std::string module_prefix;
  std::string bind_address{"0.0.0.0"};
  std::string port{"3306"};
  std::string http_port{};
  std::string xport{};
  bool verbose{false};
  std::string logging_folder;

  std::string ssl_cert;
  std::string ssl_key;
  std::string ssl_mode;
  std::string tls_version;
  std::string ssl_ca;
  std::string ssl_capath;
  std::string ssl_crl;
  std::string ssl_crlpath;
  std::string ssl_cipher;

  bool core_file{false};
};

static void init_DIM() {
  static mysql_harness::logging::Registry static_registry;

  // logging facility
  mysql_harness::DIM::instance().set_static_LoggingRegistry(&static_registry);
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
    init_DIM();
    auto loader_config = std::make_unique<mysql_harness::LoaderConfig>(
        mysql_harness::Config::allow_keys);

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();

    mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

    const auto log_level = config_.verbose
                               ? mysql_harness::logging::LogLevel::kDebug
                               : mysql_harness::logging::LogLevel::kWarning;

    mysql_harness::logging::clear_registry(registry);
    mysql_harness::logging::create_module_loggers(
        registry, log_level,
        {mysql_harness::logging::kMainLogger, "mock_server", "http_server", "",
         "rest_mock_server"},
        mysql_harness::logging::kMainLogger);
    mysql_harness::logging::create_main_log_handler(
        registry, "mock_server", config_.logging_folder, true);

    registry.set_ready();

    if (config_.module_prefix.empty()) {
      std::error_code ec;

      auto cwd = stdx::filesystem::current_path(ec);
      if (ec) {
        throw std::system_error(ec);
      }

      config_.module_prefix = cwd.native();
    }
    loader_config->set_default("logging_folder", config_.logging_folder);
    loader_config->add("logger");
    auto &logger_conf = loader_config->get("logger", "");
    logger_conf.add("level", config_.verbose ? "debug" : "warning");
    logger_conf.add("timestamp_precision", "ms");
    const std::string logfile_name = "mock_server_" + config_.port + ".log";
    logger_conf.add("filename", logfile_name);

    // initialize the signal handler
    signal_handler_.register_ignored_signals_handler();
    signal_handler_.block_all_nonfatal_signals();
    signal_handler_.register_fatal_signal_handler(config_.core_file);
    signal_handler_.spawn_signal_handler_thread();
#ifdef _WIN32
    signal_handler_.register_ctrl_c_handler();
#endif

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

    {
      auto &section = loader_config->add("io");
      section.add("library", "io");
      section.add("threads", "1");
    }

    if (!config_.http_port.empty()) {
      auto &rest_mock_server_config =
          loader_config->add("rest_mock_server", "");
      rest_mock_server_config.set("library", "rest_mock_server");

      auto &http_server_config = loader_config->add("http_server", "");
      http_server_config.set("library", "http_server");
      http_server_config.set("bind_address", config_.bind_address);
      http_server_config.set("port", config_.http_port);
      http_server_config.set("static_folder", "");
    }

    auto &mock_server_config = loader_config->add("mock_server", "classic");
    mock_server_config.set("library", "mock_server");
    mock_server_config.set("bind_address", config_.bind_address);
    mock_server_config.set("port", config_.port);
    mock_server_config.set("filename", config_.queries_filename);
    mock_server_config.set("module_prefix", config_.module_prefix);
    mock_server_config.set("protocol", "classic");
    mock_server_config.set("ssl_mode", config_.ssl_mode);
    mock_server_config.set("ssl_cert", config_.ssl_cert);
    mock_server_config.set("ssl_key", config_.ssl_key);
    mock_server_config.set("tls_version", config_.tls_version);
    mock_server_config.set("ssl_cipher", config_.ssl_cipher);
    mock_server_config.set("ssl_ca", config_.ssl_ca);
    mock_server_config.set("ssl_capath", config_.ssl_capath);
    mock_server_config.set("ssl_crl", config_.ssl_crl);
    mock_server_config.set("ssl_crlpath", config_.ssl_crlpath);

    if (!config_.xport.empty()) {
      auto &mock_x_server_config = loader_config->add("mock_server", "x");
      mock_x_server_config.set("library", "mock_server");
      mock_x_server_config.set("bind_address", config_.bind_address);
      mock_x_server_config.set("port", config_.xport);
      mock_x_server_config.set("filename", config_.queries_filename);
      mock_x_server_config.set("module_prefix", config_.module_prefix);
      mock_x_server_config.set("protocol", "x");
      mock_x_server_config.set("ssl_mode", config_.ssl_mode);
      mock_x_server_config.set("ssl_cert", config_.ssl_cert);
      mock_x_server_config.set("ssl_key", config_.ssl_key);
      mock_x_server_config.set("tls_version", config_.tls_version);
      mock_x_server_config.set("ssl_cipher", config_.ssl_cipher);
      mock_x_server_config.set("ssl_ca", config_.ssl_ca);
      mock_x_server_config.set("ssl_capath", config_.ssl_capath);
      mock_x_server_config.set("ssl_crl", config_.ssl_crl);
      mock_x_server_config.set("ssl_crlpath", config_.ssl_crlpath);
    }

    dim.set_Config(loader_config.release(),
                   std::default_delete<mysql_harness::LoaderConfig>());

    std::unique_ptr<mysql_harness::Loader> loader_;
    try {
      loader_ = std::make_unique<mysql_harness::Loader>("server-mock",
                                                        dim.get_Config());
    } catch (const std::runtime_error &err) {
      throw std::runtime_error(std::string("init-loader failed: ") +
                               err.what());
    }

    logger_.debug("Starting");

#if !defined(_WIN32)
    //
    // reopen the logfile on SIGHUP.
    //

    static const char kSignalHandlerServiceName[]{"signal_handler"};

    loader_->waitable_services().emplace_back(kSignalHandlerServiceName);

    // as the LogReopener depends on the loggers being started, it must be
    // initialized after Loader::start_all() has been called.
    loader_->after_all_started([&]() {
      signal_handler_.add_sig_handler(
          SIGTERM, [&](int /* sig */, const std::string &signal_info) {
            mysql_harness::ProcessStateComponent::get_instance()
                .request_application_shutdown(
                    mysql_harness::ShutdownPending::Reason::REQUESTED,
                    signal_info);
          });

      signal_handler_.add_sig_handler(
          SIGINT, [&](int /* sig */, const std::string &signal_info) {
            mysql_harness::ProcessStateComponent::get_instance()
                .request_application_shutdown(
                    mysql_harness::ShutdownPending::Reason::REQUESTED,
                    signal_info);
          });

      mysql_harness::on_service_ready(kSignalHandlerServiceName);
    });

    // after the first plugin finished, stop the log-reopener
    loader_->after_first_finished([&]() {
      signal_handler_.remove_sig_handler(SIGTERM);
      signal_handler_.remove_sig_handler(SIGINT);
    });
#endif

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
        CmdOption::OptionNames({"-B", "--bind-address"}),
        "TCP address to bind to listen on for classic protocol connections.",
        CmdOptionValueReq::required, "string",
        [this](const std::string &bind_address) {
          config_.bind_address = bind_address;
        });

    arg_handler_.add_option(
        CmdOption::OptionNames({"-P", "--port"}),
        "TCP port to listen on for classic protocol connections.",
        CmdOptionValueReq::required, "int",
        [this](const std::string &port) { config_.port = port; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"-X", "--xport"}),
        "TCP port to listen on for X protocol connections.",
        CmdOptionValueReq::required, "int",
        [this](const std::string &port) { config_.xport = port; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--http-port"}),
        "TCP port to listen on for HTTP/REST connections.",
        CmdOptionValueReq::required, "int",
        [this](const std::string &port) { config_.http_port = port; });
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
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-cert"}),
        "path to PEM file containing a SSL certificate",
        CmdOptionValueReq::required, "path",
        [this](const std::string &value) { config_.ssl_cert = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-key"}),
        "path to PEM file containing a SSL key", CmdOptionValueReq::required,
        "path", [this](const std::string &value) { config_.ssl_key = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-mode"}), "SSL mode",
        CmdOptionValueReq::required, "mode",
        [this](const std::string &value) { config_.ssl_mode = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--tls-version"}), "TLS version",
        CmdOptionValueReq::required, "version",
        [this](const std::string &value) { config_.tls_version = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-cipher"}), "SSL ciphers",
        CmdOptionValueReq::required, "cipher-list",
        [this](const std::string &value) { config_.ssl_cipher = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-ca"}), "PEM file containg CA",
        CmdOptionValueReq::required, "PEM_file",
        [this](const std::string &value) { config_.ssl_ca = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-capath"}),
        "directory containing PEM files of CA", CmdOptionValueReq::required,
        "directory",
        [this](const std::string &value) { config_.ssl_capath = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-crl"}), "PEM file containg CRL",
        CmdOptionValueReq::required, "PEM_file",
        [this](const std::string &value) { config_.ssl_crl = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--ssl-crlpath"}),
        "directory containing PEM files of CRL", CmdOptionValueReq::required,
        "directory",
        [this](const std::string &value) { config_.ssl_crlpath = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--logging-folder"}), "logging folder",
        CmdOptionValueReq::required, "directory",
        [this](const std::string &value) { config_.logging_folder = value; });
    arg_handler_.add_option(
        CmdOption::OptionNames({"--core-file"}),
        "Write a core file if mysqlrouter dies.", CmdOptionValueReq::optional,
        "", [this](const std::string &value) {
          if (value.empty() || value == "1") {
            config_.core_file = true;
          } else if (value == "0") {
            config_.core_file = false;
          } else {
            throw std::runtime_error(
                "Value for parameter '--core-file' needs to be "
                "one of: ['0', '1']");
          }
        });
  }

  CmdArgHandler arg_handler_;
  bool do_print_and_exit_{false};

  MysqlServerMockConfig config_;

  std::string program_name_;
  mysql_harness::Path origin_dir_;

  mysql_harness::SignalHandler signal_handler_;

  mysql_harness::logging::DomainLogger logger_;
};

int main(int argc, char *argv[]) {
  MysqlServerMockFrontend frontend;

#ifdef _WIN32
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed with error: " << result << std::endl;
    return EXIT_FAILURE;
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
    return EXIT_FAILURE;
  }

  return 0;
}
