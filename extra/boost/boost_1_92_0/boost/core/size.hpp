/*
Copyright 2023 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_CORE_SIZE_HPP
#define BOOST_CORE_SIZE_HPP

#include <iterator>

// Note: MSVC doesn't define __cpp_lib_nonmember_container_access but supports the feature even in C++14 mode
#if (defined(__cpp_lib_nonmember_container_access) && (__cpp_lib_nonmember_container_access >= 201411l)) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1900))

namespace boost {
using std::size;
} /* boost */

#else // (defined(__cpp_lib_nonmember_container_access) ...

#include <cstddef>

namespace boost {

template<class C>
inline constexpr auto
size(const C& c) noexcept(noexcept(c.size())) -> decltype(c.size())
{
    return c.size();
}

template<class T, std::size_t N>
inline constexpr std::size_t
size(T(&)[N]) noexcept
{
    return N;
}

} /* boost */

#endif // (defined(__cpp_lib_nonmember_container_access) ...

#endif
