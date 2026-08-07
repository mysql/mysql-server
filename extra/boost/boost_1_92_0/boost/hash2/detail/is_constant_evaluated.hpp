#ifndef BOOST_HASH2_DETAIL_IS_CONSTANT_EVALUATED_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_IS_CONSTANT_EVALUATED_HPP_INCLUDED

// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/config.hpp>

namespace boost
{
namespace hash2
{
namespace detail
{

constexpr bool is_constant_evaluated() noexcept
{
#if defined(BOOST_HASH2_HAS_BUILTIN_IS_CONSTANT_EVALUATED)

    return __builtin_is_constant_evaluated();

#else

    return true;

#endif
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_IS_CONSTANT_EVALUATED_HPP_INCLUDED
