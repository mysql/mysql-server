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

#ifndef MYSQL_STRCONV_DECODE_PARSER_H
#define MYSQL_STRCONV_DECODE_PARSER_H

/// @file
/// Experimental API header

#include <string_view>                             // string_view
#include <type_traits>                             // remove_cvref_t
#include "mysql/strconv/decode/parse_options.h"    // Is_parse_options
#include "mysql/strconv/decode/parse_position.h"   // Parse_position
#include "mysql/strconv/decode/parse_result.h"     // Parse_result
#include "mysql/strconv/encode/out_str_write.h"    // out_str_write
#include "mysql/strconv/encode/string_target.h"    // Is_string_target
#include "mysql/strconv/formats/resolve_format.h"  // resolve_format
#include "mysql/utils/return_status.h"             // Return_status

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// Forward declarations
class Parser;

template <Is_format Format_t>
class Fluent_parser;

namespace detail {
/// True if `decode_impl` can be invoked with the given format type and
/// object type.
template <class Format_t, class Object_t>
concept Can_invoke_decode_impl =
    requires(Format_t format, Parser parser, Object_t obj) {
      decode_impl(format, parser, obj);
    };
}  // namespace detail

/// Object used to parse strings. This holds state, including the parsed string,
/// position, success/failure status, and error messages. It is used and
/// manipulated by `decode_impl` functions, and returned by `decode`.
///
/// It aggregates a `Parse_position` and a `Parse_result`. We extend it with
/// higher-level functions that read from the string, advance the position, and
/// store the success/failure status.
class Parser : public detail::Parse_position<Parser>,
               public detail::Parse_result {
  using Return_status_t = mysql::utils::Return_status;

 public:
  /// Be default-constructible. The resulting Parser is a singular object,
  /// which may only be used as the target of an assignment operation.
  Parser() = default;

  /// Construct a new Parser object.
  ///
  /// @param source Source string.
  ///
  /// @param pos_arg Current position. Defaults to 0, i.e., the beginning.
  explicit Parser(const std::string_view &source, std::size_t pos_arg = 0)
      : Parse_position(source, pos_arg) {}

  // ==== Report parse_error at current position ====

  /// Store a result representing that the requested object could not be parsed
  /// because the string is wrong at the current position.
  ///
  /// @param message Error message, in the form of a full sentence with leading
  /// capital but no period. The full message will be constructed by
  /// concatenating a string like " after N characters [...]" to the given
  /// message.
  void set_parse_error(const std::string_view &message) {
    do_set_parse_error(message, Message_form::sentence, int_pos());
  }

  /// Store a result representing that the requested object could not be parsed
  /// because the string is wrong at the current position.
  ///
  /// @param expected_string A string that was expected but not found at the
  /// current position. For example, if a comma was expected, this should be
  /// ",". The full message will be constructed by escaping any special
  /// characters in `expected_string` and inserting it into a message like
  /// "Expected "expected_string" after N characters [...]""
  void set_parse_error_expected_string(
      const std::string_view &expected_string) {
    do_set_parse_error(expected_string, Message_form::expected_string,
                       int_pos());
  }

  // ==== Skip a string ====

  /// Skip occurrences of the literal string `sv`, if found.
  ///
  /// This overload is for parse options where the Repeat component allows for 0
  /// matches. Thus the function cannot fail and can return void.
  ///
  /// @param opt Parse options that describe how to parse. The Repeat option is
  /// used to determine the number of repetitions. Any `before_token` and
  /// `after_token` members of the Format are invoked before and after skipping
  /// `sv`.
  ///
  /// @param sv String to skip.
  void skip(const Is_parse_options_optional auto &opt,
            const std::string_view &sv) {
    [[maybe_unused]] auto ret = do_skip(opt, sv);
    assert(ret == mysql::utils::Return_status::ok);
  }

  /// Skip occurrences of the literal string `sv`, if found.
  ///
  /// This overload is for parse options where the Repeat component may require
  /// 1 or more matches. Therefore, the function can fail and is declared
  /// `[[nodiscard]]`.
  ///
  /// @param opt Parse options that describe how to parse. The Repeat option is
  /// used to determine the number of repetitions. Any `before_token` and
  /// `after_token` members of the Format are invoked before and after skipping
  /// `sv`.
  ///
  /// @param sv String to skip.
  ///
  /// @return `Return_status::ok` or `Return_status::error`.
  [[nodiscard]] Return_status_t skip(const Is_parse_options auto &opt,
                                     const std::string_view &sv) {
    return do_skip(opt, sv);
  }

  /// Determine if a prefix of @c remaining_str() matches @c str and return the
  /// length.
  ///
  /// If the format has @c before_token and/or @c after_token members, they will
  /// be invoked before/after attempting the match.
  ///
  /// This function does not alter the position or status of this object.
  ///
  /// @param format The format: in case this has a `before_token` and/or an
  /// `after_token` member, those functions will be invoked before and after
  /// skipping `sv`. Otherwise, the format is unused (in which case you may use
  /// the overload that does not take a format parameter).
  ///
  /// @param str String to skip
  ///
  /// @return If there is a match, return its length. This is at least
  /// `str.size()`, but may be longer in case `before_match` and/or
  /// `after_match` advanced the position. If there is no match, returns 0.
  [[nodiscard]] std::size_t match_length(const Is_format auto &format,
                                         std::string_view str) {
    assert(!str.empty());
    // If there is a match, advance the position to the end of the match and
    // return true. If there is no match, return false; in this case the
    // position may or may not have advanced and the caller needs to restore it.
    auto worker = [&]() -> Return_status_t {
      this->before_token(format);
      if (this->remaining_size() < str.size()) return Return_status_t::error;
      if (std::memcmp(this->pos(), str.data(), str.size()) != 0)
        return Return_status_t::error;
      this->advance(str.size());
      this->after_token(format);
      return Return_status_t::ok;
    };
    std::size_t ret{0};
    auto before_pos = int_pos();
    if (worker() == Return_status_t::ok) ret = int_pos() - before_pos;
    set_int_pos(before_pos);
    return ret;
  }

 private:
  /// Helper to implement the two overloads of @c skip.
  ///
  /// @param opt Parse options
  ///
  /// @param sv String to skip
  ///
  /// @return Return_status::ok or `Return_status::error`.
  [[nodiscard]] Return_status_t do_skip(const Is_parse_options auto &opt,
                                        const std::string_view &sv) {
    return call(opt, [&] {
      std::size_t length = this->match_length(get_format(opt), sv);
      if (length == 0) {
        this->set_parse_error_expected_string(sv);
        return;
      }
      advance(length);
    });
  }

  // ==== Parse into an object ====

 public:
  /// Parse into the given object.
  ///
  /// @param opt Parse options that describe how to parse. The Repeat component
  /// is used to determine the number of repetitions. The Format component is
  /// used to invoke the correct decode_impl function, and also the
  /// `before_token` and `after_token` members are invoked, if present. The
  /// Checker component is used to validate the object after parsing.
  ///
  /// @param obj Target object to parse into.
  template <class Object_t>
    requires(!Is_out_str<Object_t>)
  [[nodiscard]] Return_status_t
      read(const Is_parse_options auto &opt, Object_t &obj) {
    return do_read(opt, obj);
  }

 private:
  /// If @c format has a member function @c before_token, call it with *this
  /// as argument.
  void before_token(const Is_format auto &format) {
    if constexpr (requires(Parser & pos) { format.before_token(pos); }) {
      format.before_token(*this);
    }
  }

  /// If @c format has a member function @c after_token, call it with *this
  /// as argument.
  void after_token(const Is_format auto &format) {
    if constexpr (requires(Parser & pos) { format.after_token(pos); }) {
      format.after_token(*this);
    }
  }

  /// Helper type predicate used by detail::resolve_format. It has the static
  /// constexpr bool member variable `value` which is true if `encode_impl`
  /// has been defined for the `Format_t` and `Object_t` types given by the
  /// template arguments.
  ///
  /// @tparam Format_t Format to test.
  ///
  /// @tparam Object_t Object type to test.
  template <class Format_t, class Object_t>
  struct Can_invoke_decode_impl_pred
      : public std::bool_constant<
            detail::Can_invoke_decode_impl<Format_t, Object_t>> {};

  /// Resolve the format, using the rules to deduce format based on default
  /// format and parent format, and parse the given object using the resolved
  /// format.
  ///
  /// This invokes `before_token` and `after_token` for the format given by
  /// `opt`, not for the resolved format.
  ///
  /// @param opt Parse options
  ///
  /// @param[in,out] out Object to parse.
  ///
  /// @return Return_status::ok on success; Return_status::error if an error
  /// occurred.
  [[nodiscard]] Return_status_t do_read(const Is_parse_options auto &opt,
                                        auto &out) {
    // clang-tidy complains that resolve_format may return void. That's true
    // only in cases where we expect a compilation error so this is valid.
    // NOLINTBEGIN
    auto format = get_format(opt);
    auto resolved_format =
        detail::resolve_format<detail::Conversion_direction::decode,
                               Can_invoke_decode_impl_pred>(format, out);
    return call(opt, [&] {
      before_token(format);
      decode_impl(resolved_format, *this, out);
      if (!is_ok()) return;
      after_token(format);
    });
    // NOLINTEND
  }

  // ==== Parse into an out_str ====

 public:
  /// Read from this object to the given Output String Wrapper, using a
  /// decode_impl function that takes an Is_string_target output object.
  ///
  /// This function will create String Target objects wrapping the Output String
  /// Wrapper and pass them to `decode_impl`.
  ///
  /// @param opt Parse Options that describe how to parse. The Repeat component
  /// is used to determine the number of repetitions. The Format component is
  /// used to invoke the correct decode_impl function, and also the
  /// `before_token` and `after_token` members are invoked, if present.
  ///
  /// For this function, the Parse Options must not have a Checker component. If
  /// validation is needed for the object type, perform the validation in
  /// decode_impl when passed a String_counter. (This pattern prevents
  /// allocation in case the string cannot be parsed. That would result in
  /// unnecessary allocations e.g. when this is invoked from a higher level
  /// object which is invoked using Repeat::any.)
  ///
  /// @param out_str Output String Wrapper to parse into.
  ///
  /// @return Return_status_t::error if `is_ok` returns false after the call,
  /// i.e., either the string could not be parsed, or an out-of-memory condition
  /// occurred when growing the output buffer. Otherwise, returns
  /// `Return_status::ok`.
  [[nodiscard]] Return_status_t read_to_out_str(
      const Is_parse_options_nocheck auto &opt,
      const Is_out_str auto &out_str) {
    auto ret = read_to_out_str(
        [&](Is_string_target auto &target) {
          return this->do_read(opt, target);
        },
        out_str);
    assert((ret == Return_status_t::ok) == is_ok());
    return ret;
  }

  /// Invoke the given string producer function, which is assumed to read from
  /// the current Parser object and report errors to this object.
  ///
  /// The output willl be written to the given `out_str` object.
  ///
  /// @param producer String producer to invoke. This accept take a single
  /// argument whose type is either String_writer or String_counter.
  ///
  /// @param out_str Output string wrapper used as back-end by the
  /// String_writer. This may be resized according to its resize policy.
  ///
  /// @return Return_status_t::error if `is_ok` returns false after the call,
  /// i.e., either the string could not be parsed, or an out-of-memory condition
  /// occurred when growing the output buffer. Otherwise, returns
  /// `Return_status::ok`.
  [[nodiscard]] Return_status_t read_to_out_str(
      const Is_string_producer auto &producer, const Is_out_str auto &out_str) {
    return out_str_write(
        out_str,
        // Wrapper around the producer that rewinds the parse position after
        // computing the size.
        [&](Is_string_target auto &target) {
          using Target_t = std::remove_cvref_t<decltype(target)>;
          [[maybe_unused]] auto before = this->int_pos();
          auto ret = mysql::utils::void_to_ok([&] { producer(target); });
          // The string producer may only return error if it first sets set an
          // error status in this object.
          if (ret == Return_status_t::error) assert(!this->is_ok());
          if constexpr (Target_t::target_type == Target_type::counter) {
            // For counters: restore the position, since counters must not have
            // side effects (semantic requirements for Is_string_producer), and
            // because we must start the parser at the same position when
            // invoking the producer on the writer.
            this->set_int_pos(before);
          }
          return this->is_ok() ? Return_status_t::ok : Return_status_t::error;
        },
        // Handle OOM by setting the status in parser.
        [&] { this->set_oom(); });
  }

  // ==== Parse using an arbitrary function ====

  /// Parse using the given function, using empty parse options.
  ///
  /// @param func Function that parses a single token. This must be invocable
  /// without arguments, and update the status of this object in case an error
  /// occurs.
  [[nodiscard]] Return_status_t call(const std::invocable auto &func) {
    return call(Empty_parse_options{}, func);
  }

  /// Parse using the given function, using the given parse options.
  ///
  /// @param opt Parse options that describe how to parse. The Repeat component
  /// is used to determine the number of repetitions. The Format component is
  /// unused. The Checker component is used to validate the string after
  /// parsing (once per repeated element).
  ///
  /// @param func Function that parses a single token. This must be invocable
  /// without arguments, and update the status of this object in case an error
  /// occurs.
  [[nodiscard]] Return_status_t call(const Is_parse_options auto &opt,
                                     const std::invocable auto &func) {
    assert(is_ok());

    // The position to rewind to in case parsing/checking fails
    auto before_pos = int_pos();

    // Invoke `func`. On success, invoke the checker. If either the parser or
    // the checker fails, restore the position, and update the error position,
    // to `before_pos`.
    auto invoke_and_check = [&] {
      func();
      if (is_ok()) {
        invoke_checker(opt);
        if (!is_ok()) {
          // Validation error refers to what we parsed at `before_pos`
          if (is_parse_error()) update_parse_error_pos(before_pos);
          // After error, rewind to `before_pos`
          set_int_pos(before_pos);
        }
      } else {
        // After error, rewind to `before_pos`
        set_int_pos(before_pos);
      }
    };

    // Mandatorily parse `min` times
    std::size_t count = 0;
    if (get_repeat(opt).min() != 0) {
      for (; count != get_repeat(opt).min(); ++count) {
        invoke_and_check();
        if (!is_ok()) return Return_status_t::error;
      }
    }

    // Optionally parse `max-min` times
    for (; count != get_repeat(opt).max(); ++count) {
      // If last iteration didn't advance the position or produce a parse error,
      // next iteration won't either. So we know we would loop the maximum
      // repetitions if we tried. No need to actually loop, just return instead.
      // This optimization is essential in order for Repeat::any() to terminate
      // in case each iteration may match a zero-length string.
      if (count > 0 && int_pos() == before_pos) return Return_status_t::ok;

      before_pos = int_pos();
      invoke_and_check();
      if (!is_ok()) {
        if (is_parse_error()) {
          // Replace parse error by ok status.
          revert_parse_error_to_ok();
          set_match_count(count);
          return Return_status_t::ok;
        }
        // Propagate non-parse-errors to caller.
        assert(is_store_error());
        return Return_status_t::error;
      }
    }
    set_match_count(count);
    return Return_status_t::ok;
  }

  template <class Format_t>
  Fluent_parser<Format_t> fluent(const Format_t &format) {
    return {*this, format};
  }
};  // class Parser

static_assert(std::contiguous_iterator<Parser>);

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_PARSER_H
