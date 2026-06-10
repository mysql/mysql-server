// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_STRCONV_DECODE_PARSE_RESULT_H
#define MYSQL_STRCONV_DECODE_PARSE_RESULT_H

/// @file
/// Experimental API header

#include <cassert>                              // assert
#include <string>                               // string
#include <string_view>                          // string_view
#include <variant>                              // variant
#include "mysql/strconv/decode/parse_status.h"  // Parse_status

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv::detail {

class Parser_internals;

/// Class holding the result from parsing a string, in the form of a status and
/// a message.
class Parse_result {
 public:
  void revert_parse_error_to_ok() noexcept {
    assert(m_status == Parse_status::ok ||
           m_status == Parse_status::parse_error);
    m_status = Parse_status::ok_backtracked_from_parse_error;
  }

  /// Update the position of a parse error to the given position.
  ///
  /// This requires that the current sattus is `parse_error`.
  ///
  /// @param position New position
  void update_parse_error_pos(std::size_t position) {
    assert(status() == Parse_status::parse_error);
    m_parse_error_position = position;
  }

  /// Store a result representing that the requested object could
  /// not be parsed because something went wrong which is not the string's
  /// fault.
  ///
  /// @param message Error message describing what went wrong.
  void set_store_error(const std::string_view &message) noexcept {
    m_status = Parse_status::store_error;
    m_message = message;
  }

  /// Store a result representing that an object was successfully parsed from a
  /// prefix of the string, but the user invoked a function such as
  /// `decode` which requires that the object description extends to the
  /// end of the string.
  ///
  /// This should only be used by the framework, not by user-defined
  /// decode_impl functions.
  void set_fullmatch_error() noexcept {
    assert(is_ok());
    m_status = Parse_status::fullmatch_error;
  }

  /// Store a result representing that the requested object could not be parsed
  /// because and out-of-memory condition occurred. This is shorthand for
  /// set_store_error("Out of memory").
  void set_oom() noexcept { set_store_error(oom_message); }

  // ==== Query the success/failure status ====

  /// Return true if the last operation succeeded, i.e., either a full match was
  /// requested and an object was found which extended to the end of the string;
  /// or a prefix match was requested and an object was found, possibly followed
  /// by unparsed character; or an optional match was requested and the object
  /// was found or was completely absent (but no error occurred).
  [[nodiscard]] bool is_ok() const {
    return status() == Parse_status::ok ||
           status() == Parse_status::ok_backtracked_from_parse_error;
  }

  /// Return true if either the last operation succeeded, or failed because a
  /// full match was requested and only a prefix match was found.
  [[nodiscard]] bool is_prefix_ok() const {
    return is_ok() || status() == Parse_status::fullmatch_error;
  }

  /// Return the number of repetitions found in the last call.
  ///
  /// This may be nonzero if the status is `ok` or `fullmatch_error`; in other
  /// cases it is 0.
  [[nodiscard]] std::size_t found_count() const {
    if (!is_ok() && !is_fullmatch_error()) return 0;
    return m_found_count;
  }

  /// Return true if found_count() != 0.
  [[nodiscard]] bool is_found() const { return found_count() != 0; }

  /// Return true if an environment error occurred.
  [[nodiscard]] bool is_store_error() const {
    return status() == Parse_status::store_error;
  }

  /// Return true if a parse error occurred.
  [[nodiscard]] bool is_parse_error() const {
    return status() == Parse_status::parse_error ||
           status() == Parse_status::fullmatch_error;
  }

  /// Return true if the object was parsed successfully, but there were more
  /// characters after the end.
  [[nodiscard]] bool is_fullmatch_error() const {
    return status() == Parse_status::fullmatch_error;
  }

 protected:
  /// The form of the message stored in this object.
  // NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
  enum class Message_form {
    /// The message is expressed as a full sentence, for example "Value out of
    /// range". It should begin with a capital letter and *not* end with a
    /// period. It will be exended with a string like
    /// " after 5 characters, near [HERE] in "foo: [HERE]123".".
    sentence,

    /// The message is a string that was expected but not found at the current
    /// position. It must be a literal string, not a descriptive name of a
    /// token. For example, if a comma was missing, the string should be ",".
    /// It will be escaped and quoted, and the result inserted in a string like
    /// "Expected "," after 4 characters, near [HERE] in "foo [HERE]bar"."
    expected_string
  };

  /// Common implementation of `set_parse_error` and
  /// `set_parse_error_expected_string`.
  void do_set_parse_error(const std::string_view &string,
                          Message_form message_form, std::size_t position) {
    assert(is_ok());
    auto sposition = static_cast<std::ptrdiff_t>(position);
    if (m_status != Parse_status::ok_backtracked_from_parse_error ||
        sposition >= m_parse_error_position) {
      m_message_form = message_form;
      m_message = string;
      m_parse_error_position = sposition;
    }
    m_status = Parse_status::parse_error;
  }

  /// Store a result representing that the requested object was successfully
  /// parsed, overriding a previous error state.
  ///
  /// @param count The number of successfully parsed repetitions of the parsed
  /// token.
  void set_match_count(std::size_t count) noexcept {
    assert(m_status == Parse_status::ok ||
           m_status == Parse_status::ok_backtracked_from_parse_error);
    m_found_count = count;
  }

 private:
  /// Return the current status.
  [[nodiscard]] Parse_status status() const { return m_status; }

  /// Return the message.
  [[nodiscard]] std::string_view message() const { return m_message; }

  /// Message to use on out-of-memory.
  static constexpr char oom_message[] = "Out of memory";

  /// The message.
  std::string_view m_message{};

  /// If the status is "ok", the number of tokens found.
  std::size_t m_found_count{0};

  /// The position of the most recent "parse_error".
  std::ptrdiff_t m_parse_error_position{-1};

  /// The status.
  Parse_status m_status{Parse_status::ok};

  /// The form of the message.
  Message_form m_message_form{Message_form::sentence};

  friend class Parse_result_internals;
};  // class Parse_result

/// Helper class that exports the internals from Parse_result.
///
/// This is only intended to be used by encode_impl.
class Parse_result_internals {
 public:
  using Message_form = Parse_result::Message_form;

  explicit Parse_result_internals(const Parse_result &parse_result)
      : m_parse_result(parse_result) {}

  [[nodiscard]] std::ptrdiff_t parse_error_position() const {
    return m_parse_result.m_parse_error_position;
  }

  [[nodiscard]] Parse_status status() const { return m_parse_result.status(); }

  [[nodiscard]] auto message_form() const {
    return m_parse_result.m_message_form;
  }

  [[nodiscard]] std::string_view message() const {
    return m_parse_result.message();
  }

  const Parse_result &m_parse_result;
};

}  // namespace mysql::strconv::detail

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_PARSE_RESULT_H
