/*
Copyright 2023 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_CORE_DATA_HPP
#define BOOST_CORE_DATA_HPP

#include <iterator>

// Note: MSVC doesn't define __cpp_lib_nonmember_container_access but supports the feature even in C++14 mode
#if (defined(__cpp_lib_nonmember_container_access) && (__cpp_lib_nonmember_container_access >= 201411l)) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1900))

namespace boost {
using std::data;
} /* boost */

#else // (defined(__cpp_lib_nonmember_container_access) ...

#include <cstddef>
#include <initializer_list>

namespace boost {

template<class C>
inline constexpr auto
data(C& c) noexcept(noexcept(c.data())) -> decltype(c.data())
{
    return c.data();
}

template<class C>
inline constexpr auto
data(const C& c) noexcept(noexcept(c.data())) -> decltype(c.data())
{
    return c.data();
}

template<class T, std::size_t N>
inline constexpr T*
data(T(&a)[N]) noexcept
{
    return a;
}

template<class T>
inline constexpr const T*
data(std::initializer_list<T> l) noexcept
{
    return l.begin();
}

} /* boost */

#endif // (defined(__cpp_lib_nonmember_container_access) ...

#endif
