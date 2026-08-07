#ifndef BOOST_COMPAT_TO_UNDERLYING_HPP_INCLUDED
#define BOOST_COMPAT_TO_UNDERLYING_HPP_INCLUDED

// Copyright 2025 Braden Ganetsky
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <type_traits>

namespace boost {
namespace compat {

template <class E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

}  // namespace compat
}  // namespace boost

#endif  // BOOST_COMPAT_TO_UNDERLYING_HPP_INCLUDED
