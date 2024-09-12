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

#ifndef _PROCESS_MANAGER_H_
#define _PROCESS_MANAGER_H_

#include "mysql/harness/loader.h"
#include "process_launcher.h"
#include "process_wrapper.h"

#include <gmock/gmock.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/net_ts/win32_named_pipe.h"
#include "mysql/harness/stdx/expected.h"
#include "router_test_helpers.h"
#include "test/temp_directory.h"

using mysql_harness::Path;

/** @class ProcessManager
 *
 * Manages collection of the processes
 * Enables creating, shutting down etc.
 *
 **/
class ProcessManager {
 public:
#ifdef _WIN32
  using wait_socket_t = local::byte_protocol::acceptor;
  using notify_socket_t = local::byte_protocol::socket;
#else
  using wait_socket_t = local::datagram_protocol::socket;
  using notify_socket_t = local::datagram_protocol::socket;
#endif

  using OutputResponder = ProcessWrapper::OutputResponder;

  using exit_status_type = mysql_harness::ProcessLauncher::exit_status_type;

  /**
   * set origin path.
   */
  static void set_origin(const Path &dir);

  class Spawner {
   public:
    enum class SyncPoint {
      NONE,
      RUNNING,  // signal handler, reopen, plugins started.
      READY,    // all services are "READY"
    };

    Spawner &catch_stderr(bool v) {
      catch_stderr_ = v;
      return *this;
    }

    Spawner &with_sudo(bool v) {
      with_sudo_ = v;
      return *this;
    }

    Spawner &wait_for_notify_ready(std::chrono::milliseconds v) {
      sync_point_timeout_ = std::move(v);
      return *this;
    }

    Spawner &expected_exit_code(int v) {
      expected_exit_status_ = v;
      return *this;
    }

    Spawner &expected_exit_code(exit_status_type v) {
      expected_exit_status_ = v;
      return *this;
    }

    Spawner &wait_for_sync_point(SyncPoint sync_point) {
      sync_point_ = sync_point;
      return *this;
    }

    Spawner &output_responder(OutputResponder resp) {
      output_responder_ = std::move(resp);
      return *this;
    }

    Spawner &with_core_dump(bool dump_core) {
      with_core_ = dump_core;
      return *this;
    }

    ProcessWrapper &spawn(
        const std::vector<std::string> &params,
        const std::vector<std::pair<std::string, std::string>> &env_vars);

    ProcessWrapper &spawn(const std::vector<std::string> &params) {
      return spawn(params, {});
    }

    friend class ProcessManager;

   private:
    Spawner(
        std::string executable, std::string logging_dir,
        std::string logging_file, std::string notify_socket_path,
        std::list<std::tuple<std::unique_ptr<ProcessWrapper>, exit_status_type>>
            &processes)
        : executable_{std::move(executable)},
          logging_dir_{std::move(logging_dir)},
          logging_file_{std::move(logging_file)},
          notify_socket_path_{std::move(notify_socket_path)},
          processes_(processes) {}

    ProcessWrapper &launch_command(
        const std::string &command, const std::vector<std::string> &params,
        const std::vector<std::pair<std::string, std::string>> &env_vars);

    ProcessWrapper &launch_command_and_wait(
        const std::string &command, const std::vector<std::string> &params,
        std::vector<std::pair<std::string, std::string>> env_vars);

    static stdx::expected<void, std::error_code> wait_for_notified(
        wait_socket_t &sock, const std::string &expected_notification,
        std::chrono::milliseconds timeout);

    static stdx::expected<void, std::error_code> wait_for_notified_ready(
        wait_socket_t &sock, std::chrono::milliseconds timeout);
    static stdx::expected<void, std::error_code> wait_for_notified_stopping(
        wait_socket_t &sock, std::chrono::milliseconds timeout);

    std::string executable_;
    exit_status_type expected_exit_status_{EXIT_SUCCESS};

    bool with_sudo_{false};
    bool catch_stderr_{true};
    std::chrono::milliseconds sync_point_timeout_{30000};
    SyncPoint sync_point_{SyncPoint::READY};
    OutputResponder output_responder_{kEmptyResponder};

    std::string logging_dir_;
    std::string logging_file_;
    std::string notify_socket_path_;

    std::list<std::tuple<std::unique_ptr<ProcessWrapper>, exit_status_type>>
        &processes_;

    bool with_core_{false};
  };

  Spawner spawner(std::string executable, std::string logging_file = "");

  Spawner router_spawner() {
    return spawner(mysqlrouter_exec_.str(), "mysqlrouter.log")
        .with_core_dump(true);
  }

  class MockServerCmdline {
   public:
    MockServerCmdline &filename(std::string filename) {
      return absolute_filename(get_data_dir().join(std::move(filename)).str());
    }

    MockServerCmdline &absolute_filename(const std::string &filename) {
      filename_ = filename;

      return *this;
    }

    /**
     * set the port for the classic protocol.
     *
     * @param port  number of the port where the mock server will accept
     * the client connections.
     */
    MockServerCmdline &port(uint16_t port) {
      port_ = port;

      return *this;
    }

    /**
     * set the port for the x-protocol.
     *
     * @param port  port number where the mock server will accept x client
     *              connections
     */
    MockServerCmdline &x_port(uint16_t port) {
      x_port_ = port;

      return *this;
    }

    /**
     * set the port for the HTTP protocol.
     *
     * @param port  port number where the http_server module of the mock
     * server will accept REST client requests
     */
    MockServerCmdline &http_port(uint16_t port) {
      http_port_ = port;

      return *this;
    }

    /**
     * set the bind-address of the mysql_server_mock.
     *
     * @param addr listen address for the mock server to bind to
     */
    MockServerCmdline &bind_address(const std::string &addr) {
      bind_address_ = addr;

      return *this;
    }

    /**
     * set the prefix for the modules.
     *
     * @param prefix base-path for javascript modules used by the tests
     */
    MockServerCmdline &module_prefix(const std::string &prefix) {
      module_prefix_ = prefix;

      return *this;
    }

    /**
     * set the logging folder.
     *
     * @param folder base-path for logging files.
     */
    MockServerCmdline &logging_folder(const std::string &folder) {
      logging_folder_ = folder;

      return *this;
    }

    /**
     * enable the SSL config parts of the arguments.
     *
     * @param enable enable SSL connections to the mock server.
     */
    MockServerCmdline &enable_ssl(bool enable) {
      enable_ssl_ = enable;

      return *this;
    }

    /**
     * build the arguments according to the configuration.
     *
     * @returns arguments as defined by the earlier config.
     */
    std::vector<std::string> args() {
      std::vector<std::string> server_params{
          "--filename",       filename_,              //
          "--port",           std::to_string(port_),  //
          "--bind-address",   bind_address_,
          "--logging-folder", logging_folder_,
      };

      server_params.emplace_back("--module-prefix");
      if (module_prefix_.empty()) {
        server_params.emplace_back(get_data_dir().str());
      } else {
        server_params.emplace_back(module_prefix_);
      }

      if (http_port_ > 0) {
        server_params.emplace_back("--http-port");
        server_params.emplace_back(std::to_string(http_port_));
      }

      if (x_port_ > 0) {
        server_params.emplace_back("--xport");
        server_params.emplace_back(std::to_string(x_port_));
      }

      if (enable_ssl_) {
        server_params.emplace_back("--ssl-mode");
        server_params.emplace_back("PREFERRED");
        server_params.emplace_back("--ssl-key");
        server_params.emplace_back(SSL_TEST_DATA_DIR "server-key.pem");
        server_params.emplace_back("--ssl-cert");
        server_params.emplace_back(SSL_TEST_DATA_DIR "server-cert.pem");
      }

      return server_params;
    }

   private:
    MockServerCmdline() = default;

    // use mock_server_cmdline()
    MockServerCmdline(std::string filename)
        : filename_(get_data_dir().join(std::move(filename)).str()) {}

    friend ProcessManager;

    std::string filename_;
    std::string module_prefix_;
    std::string bind_address_{"127.0.0.1"};
    std::string logging_folder_;

    uint16_t port_{};
    uint16_t x_port_{};
    uint16_t http_port_{};

    bool enable_ssl_{false};

    std::vector<std::string> args_;
  };

  Spawner mock_server_spawner() {
    return spawner(mysqlserver_mock_exec_.str(), "mockserver.log")
        .with_core_dump(true);
  }

  /**
   * build cmdline argument builder for the mysql_server_mock.
   *
   * @param filename  path to the json file containing expected queries
   * definitions
   */
  MockServerCmdline mock_server_cmdline(std::string filename) {
    return MockServerCmdline(std::move(filename))
        .logging_folder(get_test_temp_dir_name());
  }

  /** @brief Gets path to the directory used as log output directory
   */
  Path get_logging_dir() const { return logging_dir_.name(); }

  /** @brief Gets path set as origin
   */
  static const Path &get_origin() { return origin_dir_; }

  /** @brief Gets path to the directory containing testing data
   *         (conf files, json files).
   */
  static Path get_data_dir() { return data_dir_; }

  /** @brief Creates a state file with a given contents and returns full path to
   * the file
   */
  static std::string create_state_file(const std::string &dir_name,
                                       const std::string &content);

  /** @brief Gets path to the directory plugin dynamic librabries.
   */
  static const Path &get_plugin_dir() { return plugin_dir_; }

 protected:
  virtual ~ProcessManager() = default;

  /**
   * shutdown all managed processes.
   */
  void shutdown_all(mysql_harness::ProcessLauncher::ShutdownEvent event =
                        mysql_harness::ProcessLauncher::ShutdownEvent::TERM);

  /**
   * terminate processes with ABRT which are still alive.
   *
   * may trigger a core-file if enabled in the process.
   */
  void terminate_all_still_alive();

  /**
   * ensures all processes exited and checks for crashes.
   */
  void ensure_clean_exit();

  void ensure_clean_exit(ProcessWrapper &process);

  stdx::expected<void, std::error_code> wait_for_exit(
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout);

  /**
   * ensures given process exited with expected return value and checks for
   * crashes.
   */
  void check_exit_code(
      ProcessWrapper &process, exit_status_type exit_status = EXIT_SUCCESS,
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout);

  std::string dump(ProcessWrapper &proccess);

  void dump_all();

  /**
   * reset the monitored processes.
   *
   * - shuts down all running processes
   * - checks for expected exit-code
   * - removes the monitoring of the processes
   */
  void clear();

  /**
   * ensures given port is ready for accepting connections, prints some debug
   * data otherwise.
   *
   * @param process       process that should be listening on that port
   * @param port          TCP port number to check
   * @param timeout       maximum timeout to wait for the port
   * @param hostname      name/IP address of the network host to check
   */
  void check_port_ready(
      ProcessWrapper &process, uint16_t port,
      std::chrono::milliseconds timeout = kDefaultPortReadyTimeout,
      const std::string &hostname = "127.0.0.1");

  /**
   * ensures given port is NOT ready for accepting connections, prints some
   * debug data otherwise.
   *
   * @param process       process that should be listening on that port
   * @param port          TCP port number to check
   * @param timeout       maximum timeout to wait for the port
   * @param hostname      name/IP address of the network host to check
   */
  void check_port_not_ready(
      ProcessWrapper &process, uint16_t port,
      std::chrono::milliseconds timeout = kDefaultPortReadyTimeout,
      const std::string &hostname = "127.0.0.1");

  /** @brief Launches the MySQLRouter process.
   *
   * @param   params vector<string> containing command line parameters to pass
   * to process
   * @param expected_exit_code expected exit-code for ensure_clean_exit()
   * @param   catch_stderr bool flag indicating if the process' error output
   * stream should be included in the output caught from the process
   * @param   with_sudo    bool flag indicating if the process' should be
   * execute with sudo privileges
   * @param wait_for_notify_ready
   *        if >=0 the method should use the notification socket and the value
   * is the time in milliseconds - how long the it should wait for the process
   * to notify it is ready. if < 0 is should not use (open) the notification
   * socket to wait for ready notification
   * @param output_responder method to be called when the process outputs a line
   * returning string that should be send back to the process input (if not
   * empty)
   *
   * @returns handle to the launched process
   */
  ProcessWrapper &launch_router(
      const std::vector<std::string> &params, int expected_exit_code = 0,
      bool catch_stderr = true, bool with_sudo = false,
      std::chrono::milliseconds wait_for_notify_ready =
          std::chrono::seconds(30),
      OutputResponder output_responder = kEmptyResponder);

  /**
   * build cmdline args for mysql_server_mock.
   */
  std::vector<std::string> mysql_server_mock_cmdline_args(
      const std::string &json_file, uint16_t port, uint16_t http_port = 0,
      uint16_t x_port = 0, const std::string &module_prefix = "",
      const std::string &bind_address = "127.0.0.1", bool enable_ssl = false);

  /** @brief Launches a process.
   *
   * @param command       path to executable
   * @param params        array of commandline parameters to pass to the
   * executable
   * @param expected_exit_status expected ExitStatus
   * @param catch_stderr  if true stderr will also be captured (combined with
   * stdout)
   * @param env_vars      environment variables that shoould be passed to the
   * process
   * @param output_responder method to be called when the process outputs a line
   * returning string that should be send back to the process input (if not
   * empty)
   *
   * @returns handle to the launched process
   */
  ProcessWrapper &launch_command(
      const std::string &command, const std::vector<std::string> &params,
      ExitStatus expected_exit_status, bool catch_stderr,
      std::vector<std::pair<std::string, std::string>> env_vars,
      OutputResponder output_responder = kEmptyResponder);

  /** @brief Launches a process.
   *
   * @param command       path to executable
   * @param params        array of commandline parameters to pass to the
   * executable
   * @param expected_exit_status expected ExitStatus
   * @param catch_stderr  if true stderr will also be captured (combined with
   * stdout)
   * @param wait_notify_ready if >=0 time in milliseconds - how long the
   * launching command should wait for the process to notify it is ready.
   * Otherwise the caller does not want to wait for the notification.
   * @param output_responder method to be called when the process outputs a line
   * returning string that should be send back to the process input (if not
   * empty)
   *
   * @returns handle to the launched process
   */
  ProcessWrapper &launch_command(
      const std::string &command, const std::vector<std::string> &params,
      ExitStatus expected_exit_status = 0, bool catch_stderr = true,
      std::chrono::milliseconds wait_notify_ready =
          std::chrono::milliseconds(-1),
      OutputResponder output_responder = kEmptyResponder);

  /** @brief Launches a process.
   *
   * @param command       path to executable
   * @param logging_file  path to logfile name
   * @param params        array of commandline parameters to pass to the
   * executable
   * @param expected_exit_status expected ExitStatus
   * @param catch_stderr  if true stderr will also be captured (combined with
   * stdout)
   * @param wait_notify_ready if >=0 time in milliseconds - how long the
   * launching command should wait for the process to notify it is ready.
   * Otherwise the caller does not want to wait for the notification.
   * @param output_responder method to be called when the process outputs a line
   * returning string that should be send back to the process input (if not
   * empty)
   *
   * @returns handle to the launched process
   */
  ProcessWrapper &launch_command(
      const std::string &command, const std::string &logging_file,
      const std::vector<std::string> &params,
      ExitStatus expected_exit_status = 0, bool catch_stderr = true,
      std::chrono::milliseconds wait_notify_ready =
          std::chrono::milliseconds(-1),
      OutputResponder output_responder = kEmptyResponder);

  /** @brief returns a map with default [DEFAULT] section parameters
   *
   * @return default parameters for [DEFAULT] section
   */
  std::map<std::string, std::string> get_DEFAULT_defaults() const;

 public:
  class ConfigWriter {
   public:
    using section_type = std::map<std::string, std::string>;

    using sections_type = std::map<std::string, section_type>;

    ConfigWriter(std::string directory, sections_type sections)
        : directory_{std::move(directory)}, sections_{std::move(sections)} {}

    /**
     * set a section by name and key-value pairs.
     *
     * @param name section name
     * @param section section's key-value pairs
     */
    ConfigWriter &section(const std::string &name, section_type section) {
      sections_[name] = std::move(section);

      return *this;
    }

    /**
     * set a section by pair.first name and pair.second value.
     *
     * @param section pair of section-name and section-key-value pairs
     */
    ConfigWriter &section(std::pair<std::string, section_type> section) {
      sections_[section.first] = std::move(section.second);

      return *this;
    }

    // directory that's set
    std::string directory() const { return directory_; }

    // allow to modify the sections
    sections_type &sections() { return sections_; }

    // write config to file.
    std::string write(const std::string &name = "mysqlrouter.conf");

   private:
    std::string directory_;
    sections_type sections_;
  };

  /**
   * create writer for structured config.
   *
   * Allows to build the config fluently:
   *
   * @code{.cc}
   * // write config to ${dir}/mysqlrouter.conf
   * config_writer(dir)
   *   .section("logger", {{"level", "DEBUG"}})
   *   .section("routing", {{"bind_port", "6446"}})
   *   .write();
   * @endcode
   */
  ConfigWriter config_writer(const std::string &directory);

 protected:
  /** @brief create config file
   *
   * @param directory directory in which the config file will be created
   * @param sections text to follow [DEFAULT] section (typically all other
   * sections)
   * @param default_section [DEFAULT] section parameters
   * @param name config file name
   * @param extra_defaults additional parameters to add to [DEFAULT]
   * @param enable_debug_logging add a logger section with debug level
   *
   * @return path to the created file
   */
  std::string create_config_file(
      const std::string &directory, const std::string &sections = "",
      const std::map<std::string, std::string> *default_section = nullptr,
      const std::string &name = "mysqlrouter.conf",
      const std::string &extra_defaults = "",
      bool enable_debug_logging = true) const;

  const Path &get_mysqlrouter_exec() const { return mysqlrouter_exec_; }

  /**
   * get Path to mysql_server_mock inside the build-dir.
   *
   * valid after SetUp() got called.
   */
  const Path &get_mysqlserver_mock_exec() const {
    return mysqlserver_mock_exec_;
  }

  void set_mysqlrouter_exec(const Path &path) { mysqlrouter_exec_ = path; }

  std::string get_test_temp_dir_name() const { return test_dir_.name(); }

 protected:
  /** @brief returns a [DEFAULT] section as string
   *
   * @param params map of [DEFAULT] section parameters
   * @returns [DEFAULT] section text
   */
  std::string make_DEFAULT_section(
      const std::map<std::string, std::string> *params) const;

  stdx::expected<void, std::error_code> wait_for_notified(
      wait_socket_t &sock, const std::string &expected_notification,
      std::chrono::milliseconds timeout);

  stdx::expected<void, std::error_code> wait_for_notified_ready(
      wait_socket_t &sock, std::chrono::milliseconds timeout);
  stdx::expected<void, std::error_code> wait_for_notified_stopping(
      wait_socket_t &sock, std::chrono::milliseconds timeout);

 private:
  void check_port(bool should_be_ready, ProcessWrapper &process, uint16_t port,
                  std::chrono::milliseconds timeout,
                  const std::string &hostname);

  static Path origin_dir_;
  static Path data_dir_;
  static Path plugin_dir_;
  static Path mysqlrouter_exec_;
  static Path mysqlserver_mock_exec_;

  TempDirectory logging_dir_;
  TempDirectory test_dir_;

  std::list<std::tuple<std::unique_ptr<ProcessWrapper>, exit_status_type>>
      processes_;
  static const OutputResponder kEmptyResponder;
};

#endif  // _PROCESS_MANAGER_H_
