/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2011 Helge Bahmann
 * Copyright (c) 2013 Tim Blechmann
 * Copyright (c) 2014, 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/atomic.hpp
 *
 * This header contains definition of \c atomic template.
 */

#ifndef BOOST_ATOMIC_ATOMIC_HPP_INCLUDED_
#define BOOST_ATOMIC_ATOMIC_HPP_INCLUDED_

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <boost/memory_order.hpp>
#include <boost/atomic/capabilities.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/classify.hpp>
#include <boost/atomic/detail/atomic_impl.hpp>
#include <boost/atomic/detail/type_traits/is_trivially_copyable.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {

//! Atomic object
template< typename T >
class atomic :
    public atomics::detail::base_atomic< T, typename atomics::detail::classify< T >::type, false >
{
private:
    using base_type = atomics::detail::base_atomic< T, typename atomics::detail::classify< T >::type, false >;
    using value_arg_type = typename base_type::value_arg_type;

public:
    using value_type = typename base_type::value_type;

    static_assert(sizeof(value_type) > 0u, "boost::atomic<T> requires T to be a complete type");
    static_assert(atomics::detail::is_trivially_copyable< value_type >::value, "boost::atomic<T> requires T to be a trivially copyable type");

public:
    atomic() = default;

    BOOST_FORCEINLINE constexpr atomic(value_arg_type v) noexcept : base_type(v)
    {
    }

    atomic(atomic const&) = delete;
    atomic& operator= (atomic const&) = delete;
    atomic& operator= (atomic const&) volatile = delete;

    BOOST_FORCEINLINE value_type operator= (value_arg_type v) noexcept
    {
        this->store(v);
        return v;
    }

    BOOST_FORCEINLINE value_type operator= (value_arg_type v) volatile noexcept
    {
        this->store(v);
        return v;
    }

    BOOST_FORCEINLINE operator value_type() const volatile noexcept
    {
        return this->load();
    }
};

using atomic_char = atomic< char >;
using atomic_uchar = atomic< unsigned char >;
using atomic_schar = atomic< signed char >;
using atomic_ushort = atomic< unsigned short >;
using atomic_short = atomic< short >;
using atomic_uint = atomic< unsigned int >;
using atomic_int = atomic< int >;
using atomic_ulong = atomic< unsigned long >;
using atomic_long = atomic< long >;
using atomic_ullong = atomic< unsigned long long >;
using atomic_llong = atomic< long long >;
using atomic_address = atomic< void* >;
using atomic_bool = atomic< bool >;
using atomic_wchar_t = atomic< wchar_t >;
#if defined(__cpp_char8_t) && (__cpp_char8_t >= 201811)
using atomic_char8_t = atomic< char8_t >;
#endif
using atomic_char16_t = atomic< char16_t >;
using atomic_char32_t = atomic< char32_t >;

using atomic_uint8_t = atomic< std::uint8_t >;
using atomic_int8_t = atomic< std::int8_t >;
using atomic_uint16_t = atomic< std::uint16_t >;
using atomic_int16_t = atomic< std::int16_t >;
using atomic_uint32_t = atomic< std::uint32_t >;
using atomic_int32_t = atomic< std::int32_t >;
using atomic_uint64_t = atomic< std::uint64_t >;
using atomic_int64_t = atomic< std::int64_t >;
using atomic_int_least8_t = atomic< std::int_least8_t >;
using atomic_uint_least8_t = atomic< std::uint_least8_t >;
using atomic_int_least16_t = atomic< std::int_least16_t >;
using atomic_uint_least16_t = atomic< std::uint_least16_t >;
using atomic_int_least32_t = atomic< std::int_least32_t >;
using atomic_uint_least32_t = atomic< std::uint_least32_t >;
using atomic_int_least64_t = atomic< std::int_least64_t >;
using atomic_uint_least64_t = atomic< std::uint_least64_t >;
using atomic_int_fast8_t = atomic< std::int_fast8_t >;
using atomic_uint_fast8_t = atomic< std::uint_fast8_t >;
using atomic_int_fast16_t = atomic< std::int_fast16_t >;
using atomic_uint_fast16_t = atomic< std::uint_fast16_t >;
using atomic_int_fast32_t = atomic< std::int_fast32_t >;
using atomic_uint_fast32_t = atomic< std::uint_fast32_t >;
using atomic_int_fast64_t = atomic< std::int_fast64_t >;
using atomic_uint_fast64_t = atomic< std::uint_fast64_t >;
using atomic_intmax_t = atomic< std::intmax_t >;
using atomic_uintmax_t = atomic< std::uintmax_t >;

#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
using atomic_float_t = atomic< float >;
using atomic_double_t = atomic< double >;
using atomic_long_double_t = atomic< long double >;
#endif

using atomic_size_t = atomic< std::size_t >;
using atomic_ptrdiff_t = atomic< std::ptrdiff_t >;

#if defined(UINTPTR_MAX)
using atomic_intptr_t = atomic< std::intptr_t >;
using atomic_uintptr_t = atomic< std::uintptr_t >;
#endif

// Select the lock-free atomic types that has natively supported waiting/notifying operations.
// Prefer 32-bit types the most as those have the best performance on current 32 and 64-bit architectures.
#if BOOST_ATOMIC_INT32_LOCK_FREE == 2 && BOOST_ATOMIC_HAS_NATIVE_INT32_WAIT_NOTIFY == 2
using atomic_unsigned_lock_free = atomic< std::uint32_t >;
using atomic_signed_lock_free = atomic< std::int32_t >;
#elif BOOST_ATOMIC_INT64_LOCK_FREE == 2 && BOOST_ATOMIC_HAS_NATIVE_INT64_WAIT_NOTIFY == 2
using atomic_unsigned_lock_free = atomic< std::uint64_t >;
using atomic_signed_lock_free = atomic< std::int64_t >;
#elif BOOST_ATOMIC_INT16_LOCK_FREE == 2 && BOOST_ATOMIC_HAS_NATIVE_INT16_WAIT_NOTIFY == 2
using atomic_unsigned_lock_free = atomic< std::uint16_t >;
using atomic_signed_lock_free = atomic< std::int16_t >;
#elif BOOST_ATOMIC_INT8_LOCK_FREE == 2 && BOOST_ATOMIC_HAS_NATIVE_INT8_WAIT_NOTIFY == 2
using atomic_unsigned_lock_free = atomic< std::uint8_t >;
using atomic_signed_lock_free = atomic< std::int8_t >;
#elif BOOST_ATOMIC_INT32_LOCK_FREE == 2
using atomic_unsigned_lock_free = atomic< std::uint32_t >;
using atomic_signed_lock_free = atomic< std::int32_t >;
#elif BOOST_ATOMIC_INT64_LOCK_FREE == 2
using atomic_unsigned_lock_free = atomic< std::uint64_t >;
using atomic_signed_lock_free = atomic< std::int64_t >;
#elif BOOST_ATOMIC_INT16_LOCK_FREE == 2
using atomic_unsigned_lock_free = atomic< std::uint16_t >;
using atomic_signed_lock_free = atomic< std::int16_t >;
#elif BOOST_ATOMIC_INT8_LOCK_FREE == 2
using atomic_unsigned_lock_free = atomic< std::uint8_t >;
using atomic_signed_lock_free = atomic< std::int8_t >;
#else
#define BOOST_ATOMIC_DETAIL_NO_LOCK_FREE_TYPEDEFS
#endif

} // namespace atomics

using atomics::atomic;

using atomics::atomic_char;
using atomics::atomic_uchar;
using atomics::atomic_schar;
using atomics::atomic_ushort;
using atomics::atomic_short;
using atomics::atomic_uint;
using atomics::atomic_int;
using atomics::atomic_ulong;
using atomics::atomic_long;
using atomics::atomic_ullong;
using atomics::atomic_llong;
using atomics::atomic_address;
using atomics::atomic_bool;
using atomics::atomic_wchar_t;
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811
using atomics::atomic_char8_t;
#endif
using atomics::atomic_char16_t;
using atomics::atomic_char32_t;

using atomics::atomic_uint8_t;
using atomics::atomic_int8_t;
using atomics::atomic_uint16_t;
using atomics::atomic_int16_t;
using atomics::atomic_uint32_t;
using atomics::atomic_int32_t;
using atomics::atomic_uint64_t;
using atomics::atomic_int64_t;
using atomics::atomic_int_least8_t;
using atomics::atomic_uint_least8_t;
using atomics::atomic_int_least16_t;
using atomics::atomic_uint_least16_t;
using atomics::atomic_int_least32_t;
using atomics::atomic_uint_least32_t;
using atomics::atomic_int_least64_t;
using atomics::atomic_uint_least64_t;
using atomics::atomic_int_fast8_t;
using atomics::atomic_uint_fast8_t;
using atomics::atomic_int_fast16_t;
using atomics::atomic_uint_fast16_t;
using atomics::atomic_int_fast32_t;
using atomics::atomic_uint_fast32_t;
using atomics::atomic_int_fast64_t;
using atomics::atomic_uint_fast64_t;
using atomics::atomic_intmax_t;
using atomics::atomic_uintmax_t;

#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
using atomics::atomic_float_t;
using atomics::atomic_double_t;
using atomics::atomic_long_double_t;
#endif

using atomics::atomic_size_t;
using atomics::atomic_ptrdiff_t;

#if defined(UINTPTR_MAX)
using atomics::atomic_intptr_t;
using atomics::atomic_uintptr_t;
#endif

#if !defined(BOOST_ATOMIC_DETAIL_NO_LOCK_FREE_TYPEDEFS)
using atomics::atomic_unsigned_lock_free;
using atomics::atomic_signed_lock_free;
#endif
#undef BOOST_ATOMIC_DETAIL_NO_LOCK_FREE_TYPEDEFS

} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_ATOMIC_HPP_INCLUDED_
