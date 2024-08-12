/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_STDX_EXPECTED_OSTREAM_H_
#define MYSQL_HARNESS_STDX_EXPECTED_OSTREAM_H_

#include "mysql/harness/stdx/expected.h"

#include <ostream>
#include <type_traits>

// operator<< for std::expected
//
// the functions are kept in a separate header as it
//
// - isn't part of the std-proposal
// - includes <ostream> which isn't need for stdx::expected<> itself

namespace stdx {
namespace impl {
template <typename S, typename T, typename = void>
struct is_to_stream_writable : std::false_type {};

template <typename S, typename T>
struct is_to_stream_writable<
    S, T, std::void_t<decltype(std::declval<S &>() << std::declval<T>())>>
    : std::true_type {};

}  // namespace impl

/**
 * write stdx::expected<T, E> to std::ostream.
 *
 * T and E must be non-void.
 *
 * only takes part in overload-resolution if T and E support 'os << v'
 */
template <class T, class E>
inline std::ostream &operator<<(std::ostream &os,
                                const stdx::expected<T, E> &res)
  requires((impl::is_to_stream_writable<std::ostream, T>::value &&
            impl::is_to_stream_writable<std::ostream, E>::value))
{
  if (res)
    os << res.value();
  else
    os << res.error();

  return os;
}

/**
 * write stdx::expected<void, E> to std::ostream.
 *
 * only takes part in overload-resolution if E supports 'os << v'
 */
template <class E>
inline std::ostream &operator<<(std::ostream &os,
                                const stdx::expected<void, E> &res)  //
  requires(impl::is_to_stream_writable<std::ostream, E>::value)
{
  if (!res) os << res.error();

  return os;
}

/**
 * write stdx::unexpected<E> to std::ostream.
 *
 * only takes part in overload-resolution if E supports 'os << v'
 */
template <class E>
inline std::ostream &operator<<(std::ostream &os,
                                const stdx::unexpected<E> &res)  //
  requires(impl::is_to_stream_writable<std::ostream, E>::value)
{
  os << res.error();

  return os;
}
}  // namespace stdx

#endif
