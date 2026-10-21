#ifndef BOOST_SYSTEM_DETAIL_IS_AGGREGATE_HPP_INCLUDED
#define BOOST_SYSTEM_DETAIL_IS_AGGREGATE_HPP_INCLUDED

// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <type_traits>

#if defined(__has_builtin)
# if __has_builtin(__is_aggregate)
#  define BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE
# endif
#endif

#if !defined(BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE) && defined(BOOST_CLANG_VERSION) && BOOST_CLANG_VERSION >= 50000
# define BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE
#endif

#if !defined(BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE) && defined(BOOST_GCC) && BOOST_GCC >= 70000
# define BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE
#endif

#if !defined(BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE) && defined(BOOST_MSVC) && BOOST_MSVC >= 1910
# define BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE
#endif

namespace boost
{
namespace system
{
namespace detail
{

#if defined(BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE)

template<class T> struct is_aggregate: public std::integral_constant<bool, __is_aggregate(T)>
{
};

#elif defined(__cpp_lib_is_aggregate) && __cpp_lib_is_aggregate >= 201703L

template<class T> struct is_aggregate: public std::is_aggregate<T>
{
};

#else

template<class T> struct is_aggregate: public std::false_type
{
};

#endif

} // namespace detail
} // namespace system
} // namespace boost

#endif // #ifndef BOOST_SYSTEM_DETAIL_IS_AGGREGATE_HPP_INCLUDED
