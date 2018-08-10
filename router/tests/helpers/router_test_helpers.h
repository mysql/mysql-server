/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_TESTS_TEST_HELPERS_INCLUDED
#define ROUTER_TESTS_TEST_HELPERS_INCLUDED

#include <typeinfo>

#define SKIP_GIT_TESTS(COND)                                       \
  if (COND) {                                                      \
    std::cout << "[  SKIPPED ] Tests using Git repository skipped" \
              << std::endl;                                        \
    return;                                                        \
  }

#define ASSERT_THROW_LIKE(expr, exc, msg)                                   \
  try {                                                                     \
    expr;                                                                   \
    FAIL() << "Expected exception of type " #exc << " but got none\n";      \
  } catch (const exc &e) {                                                  \
    if (std::string(e.what()).find(msg) == std::string::npos) {             \
      FAIL() << "Expected exception of type " #exc " with message: " << msg \
             << "\nbut got message: " << e.what() << "\n";                  \
    }                                                                       \
  } catch (const std::exception &e) {                                       \
    FAIL() << "Expected exception of type " #exc "\nbut got "               \
           << typeid(e).name() << ": " << e.what() << "\n";                 \
  }

/*
 * it would be great if the catch-all part could report the type of the
 * exception we got in a simpler way.
 */
#define EXPECT_THROW_LIKE(expr, exc, msg)                                     \
  try {                                                                       \
    expr;                                                                     \
    ADD_FAILURE() << "Expected exception of type " #exc << " but got none\n"; \
  } catch (const exc &e) {                                                    \
    if (std::string(e.what()).find(msg) == std::string::npos) {               \
      ADD_FAILURE() << "Expected exception of type " #exc " with message: "   \
                    << msg << "\nbut got message: " << e.what() << "\n";      \
    }                                                                         \
  } catch (...) {                                                             \
    auto user_e = std::current_exception();                                   \
    try {                                                                     \
      std::rethrow_exception(user_e);                                         \
    } catch (const std::exception &e) {                                       \
      ADD_FAILURE() << "Expected exception of type " #exc << "\nbut got "     \
                    << typeid(e).name() << ": " << e.what() << "\n";          \
    }                                                                         \
  }

#include "mysql/harness/filesystem.h"

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

/** @brief Returns the current working directory
 *
 * Uses `getcwd()` and returns the current working directory as as std::string.
 *
 * Throws std::runtime_error on errors.
 *
 * @return std::string
 */
const std::string get_cwd();

/** @brief Changes the current working directory
 *
 * Uses `chdir()` to change the current working directory. When succesfully
 * change to the folder, the old working directory is returned.
 *
 * Throws std::runtime_error on errors.
 *
 * @return std::string
 */
const std::string change_cwd(std::string &dir);

/** @brief Checks whether string ends with the specified suffix
 *
 * Returns true if the string ends with the given suffix.
 *
 * @return bool
 */
bool ends_with(const std::string &str, const std::string &suffix);

/** @brief Checks whether string starts with the specified prefix
 *
 * Returns true if the string begins with the given prefix.
 *
 * @return bool
 */
bool starts_with(const std::string &str, const std::string &prefix);

/** @brief Reads a specified number of bytes from a non-blocking socket
 *
 * reads a non-blocking socket until one of three things happen:
 *   1. specified number of bytes have been read - returns this number
 *   2. timeout expires - throws, describing the error
 *   3. read() fails    - throws, describing the error
 *
 * Returns number of bytes read (should be the number of bytes requested,
 * can be less on EOF).  Throws std::runtime_error on I/O error or timeout;
 * the reason can be extracted from the thrown object with what() method.
 *
 * @param sockfd file decriptor
 * @param buffer to store read bytes
 * @param n_bytes of bytes to read
 * @param timeout_in_ms expressed in milliseconds
 *
 * @return number of bytes read
 */
size_t read_bytes_with_timeout(int sockfd, void *buffer, size_t n_bytes,
                               uint64_t timeout_in_ms);

#ifdef _WIN32
std::string get_last_error(int err_code);
#endif

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

#endif  // ROUTER_TESTS_TEST_HELPERS_INCLUDED
