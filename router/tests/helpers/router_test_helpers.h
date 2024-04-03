/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_TESTS_TEST_HELPERS_INCLUDED
#define ROUTER_TESTS_TEST_HELPERS_INCLUDED

#include <chrono>
#include <functional>
#include <map>
#include <stdexcept>
#include <typeinfo>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/attribute.h"

#define HARNESS_TEST_THROW_LIKE_(statement, expected_exception,                \
                                 expected_message, fail)                       \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                                \
  if (::testing::internal::AlwaysTrue()) {                                     \
    try {                                                                      \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);               \
      fail() << "Expected exception of type " #expected_exception              \
             << " but got none\n";                                             \
    } catch (const expected_exception &e) {                                    \
      if (std::string(e.what()).find(expected_message) == std::string::npos) { \
        fail() << "Expected exception of type " #expected_exception            \
                  " with message: "                                            \
               << expected_message << "\nbut got message: " << e.what()        \
               << "\n";                                                        \
      }                                                                        \
    } catch (...) {                                                            \
      std::exception_ptr eptr = std::current_exception();                      \
      try {                                                                    \
        std::rethrow_exception(eptr);                                          \
      } catch (const std::exception &e) {                                      \
        fail() << "Expected exception of type " #expected_message "\nbut got " \
               << typeid(e).name() << ": " << e.what() << "\n";                \
      } catch (...) {                                                          \
        fail() << "Expected exception of type " #expected_message "\nbut got " \
               << "non-std-exception\n";                                       \
      }                                                                        \
    }                                                                          \
  } else                                                                       \
    do {                                                                       \
    } while (0)

#define ASSERT_THROW_LIKE(statement, expected_exception, expected_message)  \
  HARNESS_TEST_THROW_LIKE_(statement, expected_exception, expected_message, \
                           FAIL)

#define EXPECT_THROW_LIKE(statement, expected_exception, expected_message)  \
  HARNESS_TEST_THROW_LIKE_(statement, expected_exception, expected_message, \
                           ADD_FAILURE)

#include "mysql/harness/filesystem.h"

constexpr auto kDefaultPortReadyTimeout = std::chrono::milliseconds(5000);

/** @brief Returns the CMake source root folder
 *
 * @return mysql_harness::Path
 */
mysql_harness::Path get_cmake_source_dir();

/** @brief Gets environment variable as path
 *
 * Gets environment envvar and returns it as Path. When the environment
 * variable is not set, the alternative is tried.
 *
 * Throws runtime_error when the folder is not available.
 *
 * @param envvar Name of the environment variable
 * @param alternative Alternative Path when environment variable is not
 * available
 * @return mysql_harness::Path
 */
mysql_harness::Path get_envvar_path(const std::string &envvar,
                                    mysql_harness::Path alternative);

/** @brief Checks if the given regex pattern can be found in the input string
 *
 *
 * @param s       input string to check
 * @param pattern regex pattern to look for
 *
 * @return true if the given pattern could be found, false otherwise
 */
bool pattern_found(const std::string &s, const std::string &pattern);

/** @brief Initializes Windows sockets (no-op on other OSes)
 *
 * Exits program with error upon failure.
 */
void init_windows_sockets();

/** @brief Probes if the selected TCP port is accepting the connections.
 *
 * @param port      TCP port number to check
 * @param timeout   maximum timeout to wait for the port
 * @param hostname  name/IP address of the network host to check
 *
 * @returns true if the selected port accepts connections, false otherwise
 */
[[nodiscard]] bool wait_for_port_ready(
    uint16_t port, std::chrono::milliseconds timeout = kDefaultPortReadyTimeout,
    const std::string &hostname = "127.0.0.1");

/** @brief Probes if the selected unix socket is accepting the connections.
 *
 * @param socket    name of the socket to check
 * @param timeout   maximum timeout to wait for the socket
 *
 * @returns true if the selected socket accepts connections, false otherwise
 */
[[nodiscard]] bool wait_for_socket_ready(
    const std::string &socket,
    std::chrono::milliseconds timeout = kDefaultPortReadyTimeout);

/** @brief Probes if the selected file exists or not.
 *
 * @param file    name of the file to check
 * @param timeout maximum timeout to wait for the file to exist or not
 * @param exists  determines if we expect the file to exist or not
 *
 * @returns true if the file exists, false otherwise
 */
[[nodiscard]] bool wait_file_exists(
    const std::string &file, const bool exists = true,
    std::chrono::milliseconds timeout = std::chrono::seconds(5));

/** @brief Check if a given port is open / not used by any application.
 *
 * @param port TCP port that will be checked
 *
 * @returns true if the selected port is not used, false otherwise
 */
[[nodiscard]] bool is_port_unused(const uint16_t port);

/** @brief Check if a given port can be bind to.
 *
 * @param port TCP port that will be checked
 *
 * @returns true if the selected port can be bind to, false otherwise
 */
[[nodiscard]] stdx::expected<void, std::error_code> is_port_bindable(
    const uint16_t port);

[[nodiscard]] stdx::expected<void, std::error_code> is_port_bindable(
    const net::ip::tcp::endpoint &ep);

[[nodiscard]] stdx::expected<void, std::error_code> is_port_bindable(
    net::io_context &io_ctx, const net::ip::tcp::endpoint &ep);

/** @brief Check if a given unix socket can be bind to.
 *
 * @param socket unix socket that will be checked
 *
 * @returns true if the selected socket can be bind to, false otherwise
 */
[[nodiscard]] bool is_socket_bindable(const std::string &socket);

/**
 * Wait until the port is not available (is used by any application).
 *
 * @param port      TCP port number to check
 * @param timeout   maximum timeout to wait for the port
 *
 * @return false if the port is still available after the timeout expiry,
 *         true otherwise.
 */
[[nodiscard]] bool wait_for_port_used(
    const uint16_t port,
    std::chrono::milliseconds timeout = std::chrono::seconds(10));

/**
 * Wait until the port is available (is not used by any application).
 *
 * @param port      TCP port number to check
 * @param timeout   maximum timeout to wait for the port
 *
 * @return false if the port is still not available after the timeout expiry,
 *         true otherwise.
 */
[[nodiscard]] bool wait_for_port_unused(
    const uint16_t port,
    std::chrono::milliseconds timeout = std::chrono::seconds(10));

/** @brief Initializes keyring and adds keyring-related config items to
 * [DEFAULT] section
 *
 * @param default_section [DEFAULT] section
 * @param keyring_dir directory inside of which keyring files will be created
 * @param user Router user
 * @param password Router user password
 */
void init_keyring(std::map<std::string, std::string> &default_section,
                  const std::string &keyring_dir,
                  const std::string &user = "mysql_router1_user",
                  const std::string &password = "root");

/** @brief returns true if the selected file contains a string
 *          that is true for a given predicate
 *
 * @param file_path path to the file we want to search
 * @param predicate predicate to test the file
 * @param sleep_time max time to wait for the entry in the file
 * @deprecated use wait_log_contains() or get_file_output() with
 * "EXPECT_THAT(..., Contains())"
 */
[[deprecated]] bool find_in_file(
    const std::string &file_path,
    const std::function<bool(const std::string &)> &predicate,
    std::chrono::milliseconds sleep_time = std::chrono::milliseconds(5000));

/** @brief returns the content of selected file as a string
 *
 * @param file_name name of the file
 * @param file_path path to the file
 * @param throw_on_error if true, throws std::runtime_error on failure
 *                       if false, returns error message instead of actual file
 * contents
 */
std::string get_file_output(const std::string &file_name,
                            const std::string &file_path,
                            bool throw_on_error = false);

/** @brief returns the content of selected file as a string
 *
 * @param file_name full path and name of the file
 * @param throw_on_error if true, throws std::runtime_error on failure
 *                       if false, returns error message instead of actual file
 * contents
 */
std::string get_file_output(const std::string &file_name,
                            bool throw_on_error = false);

// need to return void to be able to use ASSERT_ macros
void connect_client_and_query_port(unsigned router_port, std::string &out_port,
                                   bool should_fail = false);

/**
 * Wait for the nth occurrence of the log_regex in the log_file with timeout
 * If it's found returns the timepoint from the matched line prefix
 * If timed out or failed to convert the timestamp returns unexpected
 *
 * @param log_file path to file containing router log
 * @param log_regex value that is going to be searched for in the log
 * @param occurence number denoting which occurrence of a log_regex is expected
 * @param timeout number of milliseconds we are going to wait for the log_regex
 * to occur at expected position
 *
 * @returns if log_regex is found at expected position return the timestamp of
 * this log
 */
std::optional<std::chrono::time_point<std::chrono::system_clock>>
get_log_timestamp(
    const std::string &log_file, const std::string &log_regex,
    const unsigned occurence = 1,
    const std::chrono::milliseconds timeout = std::chrono::seconds(1));

#endif  // ROUTER_TESTS_TEST_HELPERS_INCLUDED
