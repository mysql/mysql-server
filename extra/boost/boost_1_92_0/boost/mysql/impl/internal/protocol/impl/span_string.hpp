//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SPAN_STRING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SPAN_STRING_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/core/span.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

inline string_view to_string(span<const std::uint8_t> v)
{
    return string_view(reinterpret_cast<const char*>(v.data()), v.size());
}
inline span<const std::uint8_t> to_span(string_view v)
{
    return span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(v.data()), v.size());
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
