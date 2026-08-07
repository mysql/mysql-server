// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_MEMCPY_HPP
#define BOOST_DECIMAL_DETAIL_MEMCPY_HPP

#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstring>
#include <cstdint>
#endif

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89689
// GCC 10 added checks for length of memcpy which yields the following warning (converted to error with -Werror)
// /usr/include/x86_64-linux-gnu/bits/string_fortified.h:34:33: error: 
// ‘void* __builtin___memcpy_chk(void*, const void*, long unsigned int, long unsigned int)’ specified size between 
// 18446744071562067968 and 18446744073709551615 exceeds maximum object size 9223372036854775807 [-Werror=stringop-overflow=]
//
// memcpy is defined as taking a size_t for the count and the largest count this will recieve is the number of digits
// in a 128-bit int (39) so we can safely ignore
#if defined(__GNUC__) && __GNUC__ >= 10
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-overflow"
#  pragma GCC diagnostic ignored "-Warray-bounds"
#  define BOOST_DECIMAL_STRINGOP_OVERFLOW_DISABLED
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace impl {

BOOST_DECIMAL_CUDA_CONSTEXPR char* memcpy_impl(char* dest, const char* src, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        dest[i] = src[i];
    }

    return dest;
}

BOOST_DECIMAL_CUDA_CONSTEXPR char* memset_impl(char* dest, int ch, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        dest[i] = static_cast<char>(ch);
    }

    return dest;
}

BOOST_DECIMAL_CUDA_CONSTEXPR char* memmove_impl(char* dest, const char* src, std::size_t count)
{
    if (dest < src || dest >= src + count)
    {
        // Non-overlapping or safe to copy forward
        for (std::size_t i = 0; i < count; ++i)
        {
            dest[i] = src[i];
        }
    }
    else
    {
        // Overlapping, copy backward to avoid overwriting source
        for (std::size_t i = count; i > 0; --i)
        {
            dest[i - 1] = src[i - 1];
        }
    }

    return dest;
}

}

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION)

BOOST_DECIMAL_CUDA_CONSTEXPR char* memcpy(char* dest, const char* src, std::size_t count)
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(count))
    {
        return impl::memcpy_impl(dest, src, count);
    }
    else
    {
        // Workaround for GCC-11 because it does not honor GCC diagnostic ignored
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
        // Hopefully the optimizer turns this into memcpy
        #if defined(__GNUC__) && __GNUC__ == 11
            for (std::size_t i = 0; i < count; ++i)
            {
                *(dest + i) = *(src + i);
            }

            return dest;
        #else
            return static_cast<char*>(std::memcpy(dest, src, count));
        #endif
    }
}

BOOST_DECIMAL_CUDA_CONSTEXPR char* memset(char* dest, int ch, std::size_t count)
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(count))
    {
        return impl::memset_impl(dest, ch, count);
    }
    else
    {
        return static_cast<char*>(std::memset(dest, ch, count));
    }
}

BOOST_DECIMAL_CUDA_CONSTEXPR char* memmove(char* dest, const char* src, std::size_t count)
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(count))
    {
        return impl::memmove_impl(dest, src, count);
    }
    else
    {
        return static_cast<char*>(std::memmove(dest, src, count));
    }
}

#else // No consteval detection

BOOST_DECIMAL_CUDA_CONSTEXPR char* memcpy(char* dest, const char* src, std::size_t count)
{
    return impl::memcpy_impl(dest, src, count);
}

BOOST_DECIMAL_CUDA_CONSTEXPR char* memset(char* dest, int ch, std::size_t count)
{
    return impl::memset_impl(dest, ch, count);
}

BOOST_DECIMAL_CUDA_CONSTEXPR char* memmove(char* dest, const char* src, std::size_t count)
{
    return impl::memmove_impl(dest, src, count);
}

#endif

} //namespace detail
} //namespace decimal
} //namespace boost

#ifdef BOOST_DECIMAL_STRINGOP_OVERFLOW_DISABLED
#  pragma GCC diagnostic pop
#endif

#endif // BOOST_DECIMAL_DETAIL_MEMCPY_HPP
