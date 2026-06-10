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

#ifndef MYSQL_STRCONV_ENCODE_OUT_STR_WRITE_H
#define MYSQL_STRCONV_ENCODE_OUT_STR_WRITE_H

/// @file
/// Experimental API header

#include <cassert>                         // assert
#include "mysql/strconv/encode/out_str.h"  // Is_out_str
#include "mysql/strconv/encode/string_counter.h"  // detail::Constructible_string_counter
#include "mysql/strconv/encode/string_writer.h"  // detail::Constructible_string_writer
#include "mysql/utils/return_status.h"           // Return_status

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv::detail {
/// Lambda that takes no argument, does nothing, and returns void.
inline auto nop = [] {};

/// Type of `nop`.
using Nop_t = decltype(nop);
}  // namespace mysql::strconv::detail

namespace mysql::strconv {

/// True if Test can be invoked with a `String_counter &` argument and returns
/// void or Return_status.
template <class Test>
concept Is_string_producer_counter =
    std::is_invocable_r_v<void, Test, String_counter &> ||
    std::is_invocable_r_v<mysql::utils::Return_status, Test, String_counter &>;

/// True if Test can be invoked with a `String_writer &` argument and returns
/// void or Return_status.
template <class Test>
concept Is_string_producer_writer =
    std::is_invocable_r_v<void, Test, String_writer &> ||
    std::is_invocable_r_v<mysql::utils::Return_status, Test, String_writer &>;

/// True for pairs of string producer counters/writers that can be used with
/// `out_str_write`, i.e., the counter accepts a `String_counter &`, the writer
/// accepts a `String_writer &`, and the return types are either void or
/// `Return_status`.
///
/// Moreover, the same semantic requirements as for `Is_string_producer` apply.
template <class Counter, class Writer>
concept Is_string_producer_pair =
    Is_string_producer_counter<Counter> && Is_string_producer_writer<Writer>;

/// True for invocables that can be used with `out_str_write`, i.e., which
/// accept either a `String_writer &` or a `String_counter &` argument, and the
/// return type is either void or `Return_status`.
///
/// Moreover, semantic requirements related to determinism apply. For an object
/// `obj` of type `Test`, say that `obj(...)` *succeeds* if either the return
/// type is void, or the call returns Return_status::ok. Let `sc` be a
/// `String_counter` object and `sw` a `String_writer` object.
///
/// - If `obj(sc)` succeeds, and `sw.remaining_size() < sc.size()`, then
/// `obj(sw)` must succeed.
///
/// - `obj(sc)` must not have side effects. (If it modifies the state of other
/// objects, it must restore those side effects.)
template <class Test>
concept Is_string_producer = Is_string_producer_pair<Test, Test>;

namespace detail {
/// Common implementation for the two overloads of `out_str_write(out_str,
/// producer_counter, producer_writer, oom_action)`.
template <Is_out_str Out_str_t, class Producer_counter_t,
          class Producer_writer_t, std::invocable Oom_action_t>
  requires Is_string_producer_pair<Producer_counter_t, Producer_writer_t>
[[nodiscard]] mysql::utils::Return_status do_out_str_write(
    const Out_str_t &out_str, const Producer_counter_t &producer_counter,
    const Producer_writer_t &producer_writer, const Oom_action_t &oom_action) {
  using mysql::utils::Return_status;

  // 1. Obtain an upper bound on the output size.
  std::size_t size{};
  [[maybe_unused]] std::size_t old_size{};
  if constexpr (Out_str_t::resize_policy == Resize_policy::fixed) {
    // User has guaranteed that the initial capacity suffices.
    size = out_str.initial_capacity();
    old_size = out_str.size();
  } else {
    // Compute exact size.
    detail::Constructible_string_counter counter;
    Return_status ret =
        mysql::utils::void_to_ok([&] { return producer_counter(counter); });
    if (ret != Return_status::ok) return Return_status::error;
    size = counter.size();
  }

  // 2. Resize.
  if (mysql::utils::void_to_ok([&] { return out_str.resize(size); }) !=
      Return_status::ok) {
    oom_action();
    return Return_status::error;
  }

  // 3. Write output
  detail::Constructible_string_writer writer(out_str);
  Return_status ret =
      mysql::utils::void_to_ok([&] { return producer_writer(writer); });
  if constexpr (Out_str_t::resize_policy == Resize_policy::fixed) {
    if (ret != Return_status::ok) {
      // Restore size.
      out_str.resize(old_size);
      return Return_status::error;
    }
    // Trim size: it was an upper bound, now we know the actual size.
    out_str.resize(writer.pos() - out_str.data());
  } else {
    // Since producer_counter succeeded, producer_writer should succeed too.
    // (semantic requirment of Is_string_producer_pair).
    assert(ret == Return_status::ok);
  }
  return Return_status::ok;
}
}  // namespace detail

/// Given an Is_out_str object, a String_producer_counter, and a
/// String_producer_writer, resizes the object as needed and then writes to it.
///
/// This overload is for when there are error cases, i.e., when either the
/// producer can return errors, or the Output String Wrapper is growable.
///
/// @tparam Out_str_t Type of Output String Wrapper.
///
/// @tparam Producer_counter_t Type of the producer counter function.
///
/// @tparam Producer_writer_t Type of the producer writer function.
/// `Producer_counter_t` and `Producer_writer_t` must satisfy
/// `Is_string_producer_pair` (including the semantic requirements).
///
/// @param out_str Wrapper around the output, typically obtained from one of the
/// `out_str_*` functions.
///
/// @param producer_counter Function that writes to its `String_counter &`
/// parameter, and returns either `Return_status` indicating success or failure,
/// or `void` (if it can't fail).
///
/// @param producer_writer Function that writes to its `String_writer &`
/// parameter, and returns either `Return_status` indicating success or failure,
/// or `void` (if it can't fail).
///
/// @param oom_action Optional callback function. If given, will be invoked in
/// case an out-of-memory condition occurs.
///
/// @return `Return_status::error` if either `write` or `out_str.resize` returns
/// `Return_status::error`; otherwise `Return_status::ok`.
template <Is_out_str Out_str_t, class Producer_counter_t,
          class Producer_writer_t, std::invocable Oom_action_t = detail::Nop_t>
  requires Is_string_producer_pair<Producer_counter_t, Producer_writer_t>
[[nodiscard]] mysql::utils::Return_status out_str_write(
    const Out_str_t &out_str, const Producer_counter_t &producer_counter,
    const Producer_writer_t &producer_writer,
    const Oom_action_t &oom_action = detail::nop) {
  return do_out_str_write(out_str, producer_counter, producer_writer,
                          oom_action);
}

/// Given an Is_out_str object, a String_producer_counter, and a
/// String_producer_writer, resizes the object as needed and then writes to it.
///
/// This overload is for when there are no error cases, i.e., when the producer
/// returns void and the Output String Wrapper is fixed-size.
///
/// @tparam Out_str_t Type of output wrapper.
///
/// @tparam Producer_counter_t Type of the producer counter function.
///
/// @tparam Producer_writer_t Type of the producer writer function.
/// `Producer_counter_t` and `Producer_writer_t` must satisfy
/// `Is_string_producer_pair` (including the semantic requirements).
///
/// @param out_str Wrapper around the output, typically obtained from one of the
/// `out_str_*` functions.
///
/// @param producer_counter Function that writes to its `String_counter &`
/// parameter, and returns either `Return_status` indicating success or failure,
/// or `void` (if it can't fail).
///
/// @param producer_writer Function that writes to its `String_writer &`
/// parameter, and returns either `Return_status` indicating success or failure,
/// or `void` (if it can't fail).
///
/// @param oom_action Optional callback function. If given, will be invoked in
/// case an out-of-memory condition occurs.
template <Is_out_str_fixed Out_str_t, class Producer_counter_t,
          class Producer_writer_t, std::invocable Oom_action_t = detail::Nop_t>
  requires Is_string_producer_pair<Producer_counter_t, Producer_writer_t> &&
           (std::same_as<
               std::invoke_result_t<Producer_counter_t, String_counter &>,
               void>) &&
           (std::same_as<
               std::invoke_result_t<Producer_writer_t, String_writer &>, void>)
void out_str_write(const Out_str_t &out_str,
                   const Producer_counter_t &producer_counter,
                   const Producer_writer_t &producer_writer,
                   const Oom_action_t &oom_action = detail::nop) {
  [[maybe_unused]] auto ret =
      do_out_str_write(out_str, producer_counter, producer_writer, oom_action);
  assert(ret == mysql::utils::Return_status::ok);
}

/// Given an `Is_out_str object` and an `Is_string_producer`, resize the object
/// as needed and then write to it.
///
/// @param out_str Wrapper around the output, typically obtained from one of the
/// `out_str_*` functions.
///
/// @param producer Function that writes to its `Is_string_target` parameter,
/// and returns either `Return_status` indicating success or failure, or (if it
/// can't fail) `void`.
///
/// @param oom_action Optional callback function. If given, will be invoked in
/// case an out-of-memory condition occurs.
///
/// @return If `out_str` is growable or `producer` returns non-void, returns
/// `Return_status` to indicate success or failure; otherwise returns void.
template <std::invocable Oom_action_t = detail::Nop_t>
[[nodiscard]] auto out_str_write(const Is_out_str auto &out_str,
                                 const Is_string_producer auto &producer,
                                 const Oom_action_t &oom_action = detail::nop) {
  return out_str_write(out_str, producer, producer, oom_action);
}

/// Copy the given `string_view` to the Output String Wrapper, resizing as
/// needed.
///
/// @param out_str Output String Wrapper to write to.
///
/// @param sv `string_view` to write.
///
/// @param oom_action Optional callback function. If given, will be invoked in
/// case an out-of-memory condition occurs.
///
/// @return If `out_str` is growable, return `Return_status::ok` on success and
/// `Return_status::error` on out-of-memory error; if `out_str` is fixed-size,
/// return void.
template <std::invocable Oom_action_t = detail::Nop_t>
[[nodiscard]] auto out_str_copy(const Is_out_str auto &out_str,
                                const std::string_view &sv,
                                const Oom_action_t &oom_action = detail::nop) {
  return out_str_write(
      out_str, [&](Is_string_target auto &target) { target.write_raw(sv); },
      oom_action);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_OUT_STR_WRITE_H
