/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_ERROR_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_ERROR_H_

#include "utils/polyglot_api_clean.h"

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mysqlrouter/jit_executor_value.h"
#include "utils/polyglot_store.h"

namespace shcore {
namespace polyglot {

inline const char *k_key_message{"message"};

/**
 * This class is used to represent errors occurred in the polyglot library while
 * executing one of the functions in the API. These errors don't have more
 * details, in general can be caused for 2 reasons:
 *
 * - The polyglot context got closed before executing the function.
 * - En error occurred on the guest language while executing the operation.
 */
class Polyglot_generic_error : public std::exception {
 public:
  explicit Polyglot_generic_error(const std::string &msg) : m_message{msg} {}

  const char *what() const noexcept override { return m_message.c_str(); }

  const std::string &message() const { return m_message; }

 protected:
  Polyglot_generic_error() = default;
  virtual void set_message(const std::string &msg) { m_message = msg; }

 private:
  std::string m_message;
};

/**
 * Represents polyglot errors that will be created from information available in
 * the polyglot library state, it handles cases like:
 *
 * - poly_get_last_error_info
 * - poly_exception_is_interrupted
 * - poly_exception_has_object: like error objects from the shcore::Exception
 */
class Polyglot_error : public Polyglot_generic_error {
 public:
  Polyglot_error(poly_thread thread, int64_t rc);
  Polyglot_error(poly_thread thread, poly_exception exc);

  std::string format(bool include_location = false) const;

  bool is_interrupted() const { return m_interrupted; }
  bool is_resource_exhausted() const { return m_resource_exhausted; }
  bool is_syntax_error() const;

  shcore::Dictionary_t data() const;
  std::optional<std::string> type() const { return m_type; }
  std::optional<uint64_t> line() const { return m_line; }
  std::optional<uint64_t> column() const { return m_column; }
  std::optional<std::string> source_line() const { return m_source_line; }
  std::optional<int64_t> code() const { return m_code; }
  std::optional<std::string> source() const { return m_source; }
  std::vector<std::string> backtrace() const { return m_backtrace; }

 private:
  void parse_and_translate(const std::string &source);

  void initialize(poly_thread thread, poly_exception exc);
  void initialize(poly_thread thread);
  void set_message(const std::string &msg) override;

  std::optional<std::string> m_type;
  std::optional<size_t> m_line;
  std::optional<size_t> m_column;
  std::optional<std::string> m_source_line;
  std::optional<int64_t> m_code;
  std::optional<std::string> m_source;
  std::vector<std::string> m_backtrace;
  bool m_interrupted = false;
  bool m_resource_exhausted = false;
};

template <typename F, typename... Args>
inline void throw_if_error(F f, poly_thread thread, Args &&...args) {
  if (const auto rc = f(thread, std::forward<Args>(args)...); poly_ok != rc)
      [[unlikely]] {
    throw Polyglot_error(thread, rc);
  }
}

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_ERROR_H_