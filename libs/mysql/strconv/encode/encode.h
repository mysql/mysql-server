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

#ifndef MYSQL_STRCONV_ENCODE_ENCODE_H
#define MYSQL_STRCONV_ENCODE_ENCODE_H

/// @file
/// Experimental API header

#include <ios>                                   // ios_base
#include <ostream>                               // ostream
#include <string>                                // string
#include "mysql/meta/is_specialization.h"        // Is_specialization
#include "mysql/strconv/encode/out_str.h"        // Is_out_str_fixed
#include "mysql/strconv/encode/out_str_write.h"  // out_str_write
#include "mysql/strconv/encode/string_target.h"  // Constructible_string_writer
#include "mysql/strconv/formats/format.h"        // class
#include "mysql/utils/call_and_catch.h"          // call_and_catch

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Return the string length of the object.
///
/// @param format Description of output format.
///
/// @param object Object for which the size of the string representation shall
/// be computed.
///
/// @note If you plan to allocate memory for a null-terminated string, remember
/// to add 1 byte for the trailing '\0'.
///
/// @return Size of the string representation of `object`, not counting any
/// trailing '\0' characters.
[[nodiscard]] std::size_t compute_encoded_length(const Is_format auto &format,
                                                 const auto &object) {
  detail::Constructible_string_counter counter;
  counter.write(format, object);
  return counter.size();
}

/// Write the string representation of the object to the given string output
/// wrapper.
///
/// This overload is for the case that the output wrapper's resize policy is
/// `fixed`, i.e., the caller guarantees that the output buffer has enough space
/// to store the output. Therefore, this function cannot fail and returns void.
///
/// param format Description of output format.
///
/// param[in,out] out_str Output string wrapper to write to.
///
/// param object Object to write.
///
/// @note The caller must ensure that @c out is big enough. The length can be
/// determined using compute_encoded_length(object).
//
// Doxygen complains that the parameters are duplicated (maybe mixing up
// overloads with different constraints). We work around that by using param
// instead of @param.
void encode(const Is_format auto &format, const Is_out_str_fixed auto &out_str,
            const auto &object) {
  out_str_write(out_str, [&](Is_string_target auto &target) {
    target.write(format, object);
  });
}

/// Write the string representation of the object to the given string output
/// wrapper.
///
/// This overload is for the case that the output wrapper's resize policy is
/// `growable`. Therefore, this function can fail to allocate memory and has the
/// `[[nodiscard]]` attribute.
///
/// @param format Description of output format.
///
/// @param[in,out] out_str Output string wrapper to write to.
///
/// @param object Object to write.
///
/// @return Return_status::ok on success, Return_status::error on out-of-memory
/// error.
///
/// @note The caller must ensure that @c out is big enough. The length can be
/// determined using compute_encoded_length(object).
[[nodiscard]] auto encode(const Is_format auto &format,
                          const Is_out_str_growable auto &out_str,
                          const auto &object) {
  return out_str_write(out_str, [&](Is_string_target auto &target) {
    target.write(format, object);
  });
}

namespace throwing {

/// Return an std::string object holding the string representation of the given
/// object.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @param format Description of output format.
///
/// @param object Object to write.
///
/// @return String representation of object.
///
/// @throws bad_alloc if an out-of-memory condition occurs.
template <class String_t = std::string>
[[nodiscard]] String_t encode(const Is_format auto &format,
                              const auto &object) {
  String_t ret;
  if (mysql::strconv::encode(format, out_str_growable(ret), object) !=
      mysql::utils::Return_status::ok)
    throw std::bad_alloc();
  return ret;
}

}  // namespace throwing

/// Return an std::optional<std::string> object holding the string
/// representation of the object.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @param format Description of the output format.
///
/// @param object Object to write.
///
/// @returns std::optional<std::string> object holding the string representation
/// of @c object, or holding no value if an out-of-memory condition occurred.
template <class String_t = std::string>
[[nodiscard]] std::optional<String_t> encode(const Is_format auto &format,
                                             const auto &object) {
  String_t ret;
  if (mysql::strconv::encode(format, out_str_growable(ret), object) !=
      mysql::utils::Return_status::ok)
    return {};
  return ret;
}

/// Wrapper around an std::optional<std::string> object returned from encode,
/// intended to be passed to operator<<, allowing it to implement
/// ostream-idiomatic error handling.
///
/// @see encode_to_streamable
template <class String_t = std::string>
class Streamable {
 public:
  using Opt_string = std::optional<String_t>;

  /// Construct a new Streamable from an expiring optional string.
  explicit Streamable(Opt_string &&other) : m_opt_string(std::move(other)) {}

  /// Return reference to the optional string stored in this object.
  const Opt_string &opt_string_ref() const { return m_opt_string; }

 private:
  /// Stored optional string object.
  Opt_string m_opt_string;
};

/// Return a Streamable object holding the result from `encode(format,
/// object)`. If the Streamable object is passed to operator<<, it will
/// write the string if any, or handle the allocation error according to the
/// stream's policy, if the allocation in `encode` failed.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @param format Description of the output format.
///
/// @param object Object to write.
///
/// @return Streamable object that wraps the std::optional<std::string> returned
/// from `encode`.
///
/// Example:
///
/// @code
/// // If allocation fails while allocating a temporary string to hold the
/// // text form of `object`, set the stream status accordingly.
/// s << encode_to_streamable(Text_format{}, object);
/// @endcode
template <class String_t = std::string>
Streamable<String_t> encode_to_streamable(const Is_format auto &format,
                                          const auto &object) {
  return Streamable<String_t>(encode<String_t>(format, object));
}

/// Use output operator to write the string representation of a format-tagged
/// object to the given ostream.
///
/// @param[out] out ostream to write to.
///
/// @param streamable Streamable object to write.
///
/// @returns out.
///
/// @throws std::ios_base::failure if streamable does not hold a value and
/// out.exceptions() has the badbit set.
///
/// @note If @c streamable does not hold a value, this sets badbit in out
/// (regardless if it throws or not), and it does not write to the output.
std::ostream &operator<<(
    std::ostream &out,
    const mysql::meta::Is_specialization<Streamable> auto &streamable) {
  auto &opt_string_ref = streamable.opt_string_ref();
  if (opt_string_ref.has_value()) {
    out << opt_string_ref.value();
  } else {
    out.setstate(std::ios_base::badbit);
    if ((out.exceptions() & std::ios_base::badbit) != 0)
      throw std::ios_base::failure("Out of memory");
  }
  return out;
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_ENCODE_H
