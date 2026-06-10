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

#ifndef MYSQL_STRCONV_DECODE_PARSE_OPTIONS_H
#define MYSQL_STRCONV_DECODE_PARSE_OPTIONS_H

/// @file
/// Experimental API header

#include <tuple>                                // tuple
#include "mysql/meta/is_specialization.h"       // Is_specialization
#include "mysql/strconv/decode/checker.h"       // Is_checker
#include "mysql/strconv/decode/repeat.h"        // Repeat
#include "mysql/strconv/formats/format.h"       // Is_format
#include "mysql/strconv/formats/text_format.h"  // Text_format
#include "mysql/utils/tuple_find.h"             // tuple_find

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Helpers ====

namespace detail {

/// Type predicate with a member constant `value` that is true if Test is a
/// format.
template <class Test>
struct Is_format_pred : public std::bool_constant<Is_format<Test>> {};

/// Type predicate with a member constant `value` that is true if Test is
/// `Repeat`.
template <class Test>
struct Is_repeat_pred : public std::bool_constant<Is_repeat<Test>> {};

/// Class template parameterized by an object type, holding a member class
/// template, which is a type predicate whose member constant `value` is true if
/// Test is a checker for the object type.
template <class Test>
struct Is_checker_pred : public std::bool_constant<Is_checker<Test>> {};

/// The number of elements in a Compound_parse_options of Format type.
template <class Tuple_t>
constexpr std::size_t Format_count =
    mysql::utils::Tuple_matching_element_type_count<Tuple_t, Is_format_pred>;

/// The number of elements in a Compound_parse_options of Repeat type.
template <class Tuple_t>
constexpr std::size_t Repeat_count =
    mysql::utils::Tuple_matching_element_type_count<Tuple_t, Is_repeat_pred>;

/// The number of element in a Compound_parse_options of Checker type
template <class Tuple_t>
constexpr std::size_t Checker_count =
    mysql::utils::Tuple_matching_element_type_count<Tuple_t, Is_checker_pred>;

template <class Tuple>
concept Is_compound_parse_options_tuple =
    // At most one tuple element of each kind of type.
    (Format_count<Tuple> <= 1) && (Repeat_count<Tuple> <= 1) &&
    (Checker_count<Tuple> <= 1) &&
    // No other tuple elements
    (Format_count<Tuple> + Repeat_count<Tuple> + Checker_count<Tuple> ==
     std::tuple_size_v<Tuple>);

}  // namespace detail

// ==== Compound_parse_options ====

/// Represents parse options consisting of a tuple where each of the following
/// elements occurs optionally: Format, Repeat, Checker.
template <detail::Is_compound_parse_options_tuple Tuple_tp>
struct Compound_parse_options {
  using Tuple_t = Tuple_tp;
  Tuple_t m_tuple;
  static constexpr bool has_format = detail::Format_count<Tuple_t> > 0;
  static constexpr bool has_repeat = detail::Repeat_count<Tuple_t> > 0;
  static constexpr bool has_checker = detail::Checker_count<Tuple_t> > 0;
  // Deliberate use of `::type`
  // NOLINTNEXTLINE(modernize-type-traits)
  using Format_t = std::conditional_t<
      has_format,
      mysql::utils::Tuple_find_helper<Tuple_t, detail::Is_format_pred>,
      std::type_identity<Text_format>>::type;
};

/// Deduction guide
template <class... Args_t>
Compound_parse_options(std::tuple<Args_t...>)
    -> Compound_parse_options<std::tuple<Args_t...>>;

using Empty_parse_options = Compound_parse_options<std::tuple<>>;

// ==== Concepts to identify Compound_parse_options ====

/// True if Test is a Compound_parse_options object with at most one Format
/// element, at most one Repeat element, and at most one other element.
template <class Test>
concept Is_compound_parse_options =
    mysql::meta::Is_specialization<Test, Compound_parse_options>;

/// True for any kind of parse options: Format, Repeat, Checker, or
/// Compound_parse_options.
template <class Test>
concept Is_parse_options =  // this comment helps clang-fornat
    Is_format<Test> || Is_repeat<Test> || Is_checker<Test> ||
    Is_compound_parse_options<Test>;

/// True for any kind of parse options: Format, Repeat, Checker, or
/// Compound_parse_options.
template <class Test>
concept Is_parse_options_nocheck =  // this comment helps clang-fornat
    Is_format<Test> || Is_repeat<Test> ||
    (Is_compound_parse_options<Test> && !Test::has_checker);

/// True for any kind of parse options for which `get_repeat()` returns
/// `Repeat_optional`, i.e., it is known at compile-time that a string of length
/// 0 matches, and thus parse error is impossible. Functions taking such an
/// argument can may declared without `[[nodiscard]]` (if no other errors are
/// possible either).
template <class Test>
concept Is_parse_options_optional =  // this comment helps clang-format
    Is_parse_options<Test> &&        // this comment helps clang-format
    requires(Test opt) {
      { get_repeat(opt) } -> std::same_as<Repeat_optional>;
    };

// ==== Projection functions ====

/// Return the Format component of any parse options that has one.
auto get_format(const Is_parse_options auto &) { return Text_format{}; }
auto get_format(const Is_format auto &format) { return format; }
template <Is_compound_parse_options Compound_parse_options_t>
  requires Compound_parse_options_t::has_format
auto get_format(const Compound_parse_options_t &opt) {
  return mysql::utils::tuple_find<detail::Is_format_pred>(opt.m_tuple);
}

/// Return the Repeat component of any parse options, if it exists; otherwise a
/// default-constructed Repeat object.
auto get_repeat(const Is_parse_options auto &) { return Repeat{}; }
auto get_repeat(const Is_repeat auto &repeat) { return repeat; }
template <Is_compound_parse_options Compound_parse_options_t>
  requires Compound_parse_options_t::has_repeat
auto get_repeat(const Compound_parse_options_t &opt) {
  return mysql::utils::tuple_find<detail::Is_repeat_pred>(opt.m_tuple);
}

/// Invoke the Checker member of any parse options, if it exists; otherwise do
/// nothing.
void invoke_checker(const Is_parse_options auto &) {}
void invoke_checker(const Is_checker auto &checker) { checker.check(); }
template <Is_compound_parse_options Compound_parse_options_t>
  requires Compound_parse_options_t::has_checker
void invoke_checker(const Compound_parse_options_t &opt) {
  invoke_checker(
      mysql::utils::tuple_find<detail::Is_checker_pred>(opt.m_tuple));
}

// ==== Combine operator ====

namespace detail {

/// Given any form of parse options; Format, Repeat, Checker, or
/// Compound_parse_options; return a Compound_parse_options object.
inline auto make_compound_parse_options() {
  return Compound_parse_options<std::tuple<>>();
}
template <Is_format Format_t>
auto make_compound_parse_options(const Format_t &format) {
  return Compound_parse_options<std::tuple<Format_t>>(format);
}
template <Is_repeat Repeat_t>
auto make_compound_parse_options(const Repeat_t &repeat) {
  return Compound_parse_options<std::tuple<Repeat_t>>(repeat);
}
template <Is_checker Checker_t>
auto make_compound_parse_options(const Checker_t &checker) {
  return Compound_parse_options<std::tuple<Checker_t>>(checker);
}
auto make_compound_parse_options(const Is_compound_parse_options auto &opt) {
  return opt;
}

}  // namespace detail

/// Combine two Parse Options objects into one.
template <Is_parse_options Opt1, Is_parse_options Opt2>
auto operator|(const Opt1 &opt1, const Opt2 &opt2) {
  return Compound_parse_options(
      std::tuple_cat(detail::make_compound_parse_options(opt1).m_tuple,
                     detail::make_compound_parse_options(opt2).m_tuple));
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_PARSE_OPTIONS_H
