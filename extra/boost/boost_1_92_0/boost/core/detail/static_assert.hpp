#ifndef BOOST_CORE_DETAIL_STATIC_ASSERT_HPP_INCLUDED
#define BOOST_CORE_DETAIL_STATIC_ASSERT_HPP_INCLUDED

// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(__cpp_static_assert) && __cpp_static_assert >= 200410L

#define BOOST_CORE_STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#else

#include <boost/config.hpp>
#include <cstddef>

namespace boost
{
namespace core
{

template<bool> struct STATIC_ASSERTION_FAILURE;

template<> struct STATIC_ASSERTION_FAILURE<true>
{
};

template<std::size_t> struct static_assert_test
{
};

} // namespace core
} // namespace boost

#define BOOST_CORE_STATIC_ASSERT(expr) \
    typedef ::boost::core::static_assert_test< \
        sizeof( ::boost::core::STATIC_ASSERTION_FAILURE<(expr)? true: false> ) \
    > BOOST_JOIN(boost_static_assert_typedef_,__LINE__) BOOST_ATTRIBUTE_UNUSED

#endif

#endif  // #ifndef BOOST_CORE_DETAIL_STATIC_ASSERT_HPP_INCLUDED
