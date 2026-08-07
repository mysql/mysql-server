//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_AUTH_PLUGIN_COMMON_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_AUTH_PLUGIN_COMMON_HPP

#include <boost/config.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

// All scrambles in all the plugins we know have this size
BOOST_INLINE_CONSTEXPR std::size_t scramble_size = 20u;

// Hashed passwords vary in size, but they all fit in a buffer like this
BOOST_INLINE_CONSTEXPR std::size_t max_hash_size = 32u;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
