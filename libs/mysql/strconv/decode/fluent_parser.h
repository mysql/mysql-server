// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_STRCONV_DECODE_FLUENT_PARSER_H
#define MYSQL_STRCONV_DECODE_FLUENT_PARSER_H

/// @file
/// Experimental API header

#include <concepts>                       // invocable
#include <cstddef>                        // ptrdiff_t
#include <utility>                        // forward
#include "mysql/strconv/decode/parser.h"  // Parser
#include "mysql/strconv/decode/repeat.h"  // Repeat
#include "mysql/utils/return_status.h"    // Return_status

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint

/// Used in read_repeated_with_separators: if `yes,` the separator may be
/// repeated more than once whenever it occurs.
enum class Allow_repeated_separators { no, yes };
/// Used in read_repeated_with_separators: if `yes`, the string must start with
/// a separator; if `no`, the string must not start with a separator; if
/// `optional`, the string may or may not start with a separator.
enum class Leading_separators { no, yes, optional };
/// Used in read_repeated_with_separators: if `yes`, the string must end with a
/// separator; if `no`, the string must not end with a separator; if `optional`,
/// the string may or may not end with a separator.
enum class Trailing_separators { no, yes, optional };

// NOLINTEND(performance-enum-size)

/// Fluent API for parsing strings.
///
/// This provides a higher-level, fluent API that wraps a lower-level
/// Parser. An instance represents the parser together with the state of
/// the fluent API.
///
/// By using the fluent API, parsers that read multiple sub-objects in sequence
/// can often be implemented without having to explicitly handle the failure of
/// each sub-object parser. This can make the implementation safer and the code
/// clearer.
///
/// In general, the state of the Fluent_parser is either "open" or "closed".
/// The state determines whether subsequent invocations of member functions are
/// inhibited or not: when in "open" state, subsequent invocations are
/// executed, and when in "closed" state, subsequent invocations do nothing. If
/// the state is "open" and the user invokes a member function, the state
/// remains "open" if the function succeeds and changes to "closed" if the
/// function fails. This enables patterns such as the following for parsing a
/// sequence of tokens:
///
/// @code
/// fluent_parser
///   .read(object1)
///   .read(object2)
///   .read(object3);
/// @endcode
///
/// The state of the Fluent_parser ensures that it will not attempt to parse
/// object2 or object3 if the parsing of object1 failed. Thus, the user does not
/// have to check for errors in this case. Fewer errors to check can make the
/// user code simpler and safer.
///
/// There are also special member functions that provide support for common
/// patterns in string formats:
///
/// - read_repeated_with_separators: Reads repeatedly into an object, skipping
///   literal separators between reads, and optionally skipping literal
///   separators before the first and/or after the last read.
///
/// - end_optional: Stores the information that it is valid for the string
///   to end at the current position. If an error occurs while parsing
///   subsequent tokens, the error is suppressed, the position rewound to
///   the saved position, and the state altered to "closed".
///
/// - next_token_only_if: If the given boolean predicate is true, temporarily
///   closes the Fluent_parser for only the following token.
///
/// - check_prev_token: Invokes a given invocable. If that produces an error in
///   the Parser, rewinds to the position just before the last parsed token
///   (so the error message points to just before the problematic token).
///
/// - call_unconditionally: Invokes a given invocable, regardless of the
///   open/closed state.
///
/// The Fluent_parser also holds a Format object, used when reading objects.
///
/// @tparam Format_t Type used for the internal Format object.
template <Is_format Format_t>
class Fluent_parser {
  using Self_t = Fluent_parser<Format_t>;

  /// Represents the execution state of the fluent parser.
  ///
  /// Using the fluent API amounts to a sequence of invocations of member
  /// functions. Within this sequence, each invocation either represents one
  /// token parsed from the string, or behaves as a modifier for the following
  /// token, or a modifier for the preceding token. Thus, invocations can be
  /// grouped according to which token they belong to. Accordingly, we define a
  /// *token sequence* as a maximal, non-empty sequence of invocations of the
  /// following member functions:
  ///
  /// next_token_only_if* (read|call|literal|end_optional)? check_prev_token*
  ///
  /// The Fluent_state tracks the open/closed state, and tracks whether
  /// `next_token_only_if` has temporarily altered the state to closed, needing
  /// to restore to open when the next token sequence begins.
  // NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
  enum class Fluent_state {
    /// Ready to parse tokens.
    open,
    /// `next_token_only_if` has temporarily closed the token, and none of
    /// `read`/`call`/`literal`/`end_optional`/`check_prev_token` has yet been
    /// invoked for the current token.
    next_suppressed,
    /// `next_token_only_if` has temporarily closed the token, and at least one
    /// of `read`/`call`/`literal`/ `end_optional`/`check_prev_token` has been
    /// invoked for the current token, but none of
    /// `next_token_only_if`/`read`/`call`/`literal`/`end_optional` has been
    /// invoked for the next token and thus subsequent `check_prev_token`
    /// invocations are still to be suppressed.
    last_suppressed,
    /// All subsequent tokens are suppressed.
    closed
  };

  /// Value used by m_backtrack_position, indicating that end_optional has not
  /// been invoked.
  static constexpr std::ptrdiff_t no_backtrack_position = -1;

 public:
  /// Construct a new object, wrapping the given Parser, and using the
  /// given format when reading sub-objects.
  Fluent_parser(Parser &parser, const Format_t &format)
      : m_format(format), m_parser(parser) {}

  // Delete default constructor and copy semantics; enable only move semantics.
  Fluent_parser() = delete;
  Fluent_parser(const Fluent_parser &) = delete;
  Fluent_parser(Fluent_parser &&) noexcept = default;
  Fluent_parser &operator=(const Fluent_parser &) = delete;
  Fluent_parser &operator=(Fluent_parser &&) noexcept = default;
  ~Fluent_parser() = default;

  // ==== Special operations ====

  /// Suppress the next token if `condition` is false.
  ///
  /// @param condition If false, the state will be "closed" while processing the
  /// following token.
  Self_t &next_token_only_if(bool condition) {
    switch (m_fluent_state) {
      case Fluent_state::last_suppressed:  // Forget previous token state,
                                           // execute this check.
        m_fluent_state = Fluent_state::open;
        [[fallthrough]];
      case Fluent_state::open:  // Execute this check.
        if (condition) m_fluent_state = Fluent_state::next_suppressed;
        break;
      case Fluent_state::next_suppressed:  // Suppress this check.
      case Fluent_state::closed:           // Suppress this check.
        break;
    }
    return *this;
  }

  /// If the states is not "closed", suppress the next token if `condition`
  /// evaluates to false.
  ///
  /// @param condition If this evaluates to false, the state will be "closed"
  /// while processing the following token.
  Self_t &next_token_only_if(const std::invocable auto &condition) {
    return next_token_only_if(condition());
  }

  /// If the state is not "closed", invoke the given invocable. If that changes
  /// the state of the underlying Parse_stat to not-ok, rewind the position to
  /// before the last token.
  ///
  /// @param checker Invocable to invoke.
  Self_t &check_prev_token(const std::invocable auto &checker) {
    switch (m_fluent_state) {
      case Fluent_state::open:  // Execute this check.
        checker();
        if (!m_parser.is_ok()) {
          m_fluent_state = Fluent_state::closed;
          if (m_parser.is_parse_error()) {
            if (m_backtrack_position != no_backtrack_position) {
              m_parser.set_int_pos(m_backtrack_position);
              m_parser.revert_parse_error_to_ok();
            } else {
              m_parser.update_parse_error_pos(m_before_last_token);
            }
          }
        }
        break;
      case Fluent_state::last_suppressed:  // Suppress this call.
      case Fluent_state::closed:           // Suppress this call.
        break;
      case Fluent_state::next_suppressed:  // Consume suppression request,
                                           // suppress this call.
        m_fluent_state = Fluent_state::last_suppressed;
        break;
    }
    return *this;
  }

  /// If the state is not "closed" save the current position, and if a
  /// subsequent invocation of a member function fails, rewind to the saved
  /// position and suppress the error.
  Self_t &end_optional() {
    switch (m_fluent_state) {
      case Fluent_state::last_suppressed:  // Store current position.
        m_fluent_state = Fluent_state::open;
        [[fallthrough]];
      case Fluent_state::open:  // Store current position.
        m_backtrack_position = m_parser.int_pos();
        break;
      case Fluent_state::next_suppressed:  // Consume suppression request,
                                           // suppress this call.
        m_fluent_state = Fluent_state::last_suppressed;
        break;
      case Fluent_state::closed:  // Suppress this call.
        break;
    }
    return *this;
  }

  /// Invoke the given invocable regardless o the open/closed state.
  Self_t &call_unconditionally(const std::invocable auto &function) {
    function();
    return *this;
  }

  // ==== read ====

  /// If the state is not "closed", read into the given object once.
  Self_t &read(auto &obj) { return read_repeated(Repeat::one(), obj); }

  /// If the state is not "closed" read into the given object once; if that
  /// fails with parse error, restore to the previous position and suppress the
  /// error.
  Self_t &read_optional(auto &obj) {
    return read_repeated(Repeat::optional(), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// it fails. Then, if the error is parse_error, restore to the previous
  /// position after the last successful read and suppress the error.
  Self_t &read_any(auto &obj) { return read_repeated(Repeat::any(), obj); }

  /// If the state is not "closed" read repeatedly into the given object until
  /// it fails. Then, if the error is parse_error and at least `count` instances
  /// were read, restore to the previous position after the last successful read
  /// and suppress the error.
  Self_t &read_at_least(std::size_t count, auto &obj) {
    return read_repeated(Repeat::at_least(count), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// `count` instances are found or it fails. If that failed with parse_error,
  /// restore to the previous position after the last successful read and
  /// suppress the error.
  Self_t &read_at_most(std::size_t count, auto &obj) {
    return read_repeated(Repeat::at_most(count), obj);
  }

  /// If the state is not "closed", read into the given object `count` times.
  Self_t &read_exact(std::size_t count, auto &obj) {
    return read_repeated(Repeat::exact(count), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// `max` instances are found or it fails. If that failed with parse_error and
  /// at least `count` instances were read, restore to the previous position
  /// after the last successful read and suppress the error.
  Self_t &read_range(std::size_t min, std::size_t max, auto &obj) {
    return read_repeated(Repeat::range(min, max), obj);
  }

  /// If the state is not "closed" read with a number of repetitions specified
  /// by the given `Is_repeat` object. If that failed with parse_error and at
  /// least the minimum number of repetitions were read, restore to the previous
  /// position after the last successful read and suppress the error.
  Self_t &read_repeated(const Is_repeat auto &repeat, auto &object) {
    return call_repeated(
        repeat, [&] { std::ignore = m_parser.read(m_format, object); });
  }

  // ==== read_with_format ====

  /// If the state is not "closed", read into the given object once.
  Self_t &read_with_format(const auto &format, auto &obj) {
    return read_with_format_repeated(format, Repeat::one(), obj);
  }

  /// If the state is not "closed" read into the given object once; if that
  /// fails with parse error, restore to the previous position and suppress the
  /// error.
  Self_t &read_with_format_optional(const auto &format, auto &obj) {
    return read_with_format_repeated(format, Repeat::optional(), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// it fails. Then, if the error is parse_error, restore to the previous
  /// position after the last successful read and suppress the error.
  Self_t &read_with_format_any(const auto &format, auto &obj) {
    return read_with_format_repeated(format, Repeat::any(), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// it fails. Then, if the error is parse_error and at least `count` instances
  /// were read, restore to the previous position after the last successful read
  /// and suppress the error.
  Self_t &read_with_format_at_least(const auto &format, std::size_t count,
                                    auto &obj) {
    return read_with_format_repeated(format, Repeat::at_least(count), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// `count` instances are found or it fails. If that failed with parse_error,
  /// restore to the previous position after the last successful read and
  /// suppress the error.
  Self_t &read_with_format_at_most(const auto &format, std::size_t count,
                                   auto &obj) {
    return read_with_format_repeated(format, Repeat::at_most(count), obj);
  }

  /// If the state is not "closed" read into the given object `count` times.
  Self_t &read_with_format_exact(const auto &format, std::size_t count,
                                 auto &obj) {
    return read_with_format_repeated(format, Repeat::exact(count), obj);
  }

  /// If the state is not "closed" read repeatedly into the given object until
  /// `max` instances are found or it fails. If that failed with parse_error and
  /// at least `count` instances were read, restore to the previous position
  /// after the last successful read and suppress the error.
  Self_t &read_with_format_range(const auto &format, std::size_t min,
                                 std::size_t max, auto &obj) {
    return read_with_format_repeated(format, Repeat::range(min, max), obj);
  }

  /// If the state is not "closed" read with a number of repetitions specified
  /// by the given `Is_repeat` object. If that failed with parse_error and at
  /// least the minimum number of repetitions were read, restore to the previous
  /// position after the last successful read and suppress the error.
  Self_t &read_with_format_repeated(const auto &format,
                                    const Is_repeat auto &repeat,
                                    auto &object) {
    return call_repeated(repeat,
                         [&] { std::ignore = m_parser.read(format, object); });
  }

  // ==== literal ====

  /// Like `read`, but skips the given string literal.
  Self_t &literal(const std::string_view &sv) {
    return literal_repeated(Repeat::one(), sv);
  }

  /// Like `read_optional`, but skips the given string literal.
  Self_t &literal_optional(const std::string_view &sv) {
    return literal_repeated(Repeat::optional(), sv);
  }

  /// Like `read_any`, but skips the given string literal.
  Self_t &literal_any(const std::string_view &sv) {
    return literal_repeated(Repeat::any(), sv);
  }

  /// Like `read_at_least`, but skips the given string literal.
  Self_t &literal_at_least(std::size_t count, const std::string_view &sv) {
    return literal_repeated(Repeat::at_least(count), sv);
  }

  /// Like `read_at_most`, but skips the given string literal.
  Self_t &literal_at_most(std::size_t count, const std::string_view &sv) {
    return literal_repeated(Repeat::at_most(count), sv);
  }

  /// Like `read_exact`, but skips the given string literal.
  Self_t &literal_exact(std::size_t count, const std::string_view &sv) {
    return literal_repeated(Repeat::exact(count), sv);
  }

  /// Like `read_range`, but skips the given string literal.
  Self_t &literal_range(std::size_t min, std::size_t max,
                        const std::string_view &sv) {
    return literal_repeated(Repeat::range(min, max), sv);
  }

  /// Like `read_repeated`, but skips the given string literal.
  Self_t &literal_repeated(const Is_repeat auto &repeat,
                           const std::string_view &sv) {
    return do_call(repeat, [&] { std::ignore = m_parser.skip(m_format, sv); });
  }

  // ==== call ====

  /// Like `read`, but invokes the given function instead of reading an object.
  Self_t &call(const std::invocable auto &function) {
    return call_repeated(Repeat::one(), function);
  }

  /// Like `read_optional`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_optional(const std::invocable auto &function) {
    return call_repeated(Repeat::optional(), function);
  }

  /// Like `read_any`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_any(const std::invocable auto &function) {
    return call_repeated(Repeat::any(), function);
  }

  /// Like `read_at_least`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_at_least(std::size_t count,
                        const std::invocable auto &function) {
    return call_repeated(Repeat::at_least(count), function);
  }

  /// Like `read_at_most`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_at_most(std::size_t count, const std::invocable auto &function) {
    return call_repeated(Repeat::at_most(count), function);
  }

  /// Like `read_exact`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_exact(std::size_t count, const std::invocable auto &function) {
    return call_repeated(Repeat::exact(count), function);
  }

  /// Like `read_range`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_range(std::size_t min, std::size_t max,
                     const std::invocable auto &function) {
    return call_repeated(Repeat::range(min, max), function);
  }

  /// Like `read_repeated`, but invokes the given function instead of reading an
  /// object.
  Self_t &call_repeated(const Is_repeat auto &repeat,
                        const std::invocable auto &function) {
    return do_call(repeat, function);
  }

  // ==== {read|call}_repeated_with_separators ====

  /// Reads repeatedly into the given object, using the given number of
  /// repetitions, and skipping string literals equal to the given string_view
  /// between reads.
  ///
  /// @param object Object to read into.
  ///
  /// @param separator String separator to read between objects.
  ///
  /// @param repeat The range of allowed repetitions. If fewer than the minimum
  /// number of reads succeeded, the Parser remains in error state and this
  /// Fluent_parser is closed. If at least the minimum number of reads succeed
  /// and a parse error occurs, the error of the Parser is suppressed and
  /// the Fluent_parser remains open.
  ///
  /// @param allow_repeated_separators When a separator is expected in the input
  /// string, this controls the number of repetitions allowed: if `no`, exactly
  /// one instance of the separator is parsed; if `yes`, one or more separators
  /// are parsed.
  ///
  /// @param leading_separators If `yes`, the separator is required before the
  /// first read. If `no`, the first read is attempted without reading the
  /// separator. If `optional`, a separator preceding the first read is
  /// optional.
  ///
  /// @param trailing_separators If `yes`, the separator is required after the
  /// last read. If `no`, the separator is not read after the last read. If
  /// `optional`, a separator after the last read is optional.
  Self_t &read_repeated_with_separators(
      auto &object, const std::string_view &separator,
      const Is_repeat auto &repeat,
      Allow_repeated_separators allow_repeated_separators,
      Leading_separators leading_separators,
      Trailing_separators trailing_separators) {
    return call_repeated_with_separators(
        [&] { std::ignore = m_parser.read(m_format, object); }, separator,
        repeat, allow_repeated_separators, leading_separators,
        trailing_separators);
  }

  /// Like `read_repeated_with_separators`, but invokes a function instead of
  /// reads into an object. @see read_repeated_with_separators.
  Self_t &call_repeated_with_separators(
      const std::invocable auto &function, const std::string_view &separator,
      const Is_repeat auto &repeat,
      Allow_repeated_separators allow_repeated_separators,
      Leading_separators leading_separators,
      Trailing_separators trailing_separators) {
    std::size_t max_separators{1};
    if (allow_repeated_separators == Allow_repeated_separators::yes) {
      max_separators = std::numeric_limits<std::size_t>::max();
    }
    auto parse_separator = [&] { literal_range(1, max_separators, separator); };
    auto parse_leading_separator = [&] {
      switch (leading_separators) {
        case Leading_separators::yes:
          parse_separator();
          break;
        case Leading_separators::no:
          break;
        case Leading_separators::optional:
          literal_range(0, max_separators, separator);
          break;
      }
    };

    if (trailing_separators == Trailing_separators::no) {
      // No trailing separators: repeat (separator, call)
      bool first = true;
      call_repeated(repeat, [&] {
        if (first) {
          parse_leading_separator();
          first = false;
        } else {
          parse_separator();
        }
        call(function);
      });
    } else {
      // (Possibly optional) trailing separators: repeat (call, separator)
      parse_leading_separator();
      call_repeated(repeat, [&] {
        call(function);
        if (trailing_separators == Trailing_separators::optional)
          end_optional();
        parse_separator();
      });
    }

    return *this;
  }

 private:
  /// Invoke the given function with the given number of repetitions.
  ///
  /// This function performs the bookkeeping of progressing the Fluent_state, of
  /// tracking the position before the token (used by subsequent
  /// check_prev_token calls), and of rewinding the position to
  /// m_backtrack_position after parse errors.
  Self_t &do_call(const Is_repeat auto &repeat,
                  const std::invocable auto &function) {
    switch (m_fluent_state) {
      case Fluent_state::last_suppressed:  // Forget previous token state,
                                           // execute this call.
        m_fluent_state = Fluent_state::open;
        [[fallthrough]];
      case Fluent_state::open:  // Execute this call.
      {
        auto wrapped_function = [&] {
          // Temporarily clear m_backtrack_position while invoking the function.
          // This allows the recursively invoked function to re-use the
          // Fluent_parser.
          auto old_backtrack_position = m_backtrack_position;
          m_backtrack_position = no_backtrack_position;
          function();
          m_backtrack_position = old_backtrack_position;
        };
        auto before_token = m_parser.int_pos();
        if (m_parser.call(repeat, wrapped_function) ==
            mysql::utils::Return_status::ok) {
          // Restore state, just in case sub-parser reused this Fluent_parser.
          m_fluent_state = Fluent_state::open;
          // Update m_before_last_token.
          m_before_last_token = before_token;
        } else {
          // End parsing.
          m_fluent_state = Fluent_state::closed;
          if (m_parser.is_parse_error() &&
              m_backtrack_position != no_backtrack_position) {
            m_parser.set_int_pos(m_backtrack_position);
            m_backtrack_position = no_backtrack_position;
            m_parser.revert_parse_error_to_ok();
          }
        }
        break;
      }
      case Fluent_state::next_suppressed:  // Consume suppression request,
                                           // suppress this call.
        m_fluent_state = Fluent_state::last_suppressed;
        break;
      case Fluent_state::closed:  // Suppress this call.
        break;
    }
    return *this;
  }

  /// Format used to parse objects.
  Format_t m_format;

  /// Underlying Parser that this Fluent_parser operates on.
  Parser &m_parser;

  /// Previous position we parsed to at which it is OK to stop parsing in
  /// case a parse error occurs. Set by `end_optional`.
  std::ptrdiff_t m_backtrack_position{no_backtrack_position};

  /// Position before the last parsed token. Used if `check_prev_token` sets an
  /// error state.
  std::ptrdiff_t m_before_last_token{0};

  /// The open/closed state.
  Fluent_state m_fluent_state{Fluent_state::open};
};  // Fluent_parser

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_FLUENT_PARSER_H
