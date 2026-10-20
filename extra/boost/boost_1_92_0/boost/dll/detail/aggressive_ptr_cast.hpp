// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_DETAIL_AGGRESSIVE_PTR_CAST_HPP
#define BOOST_DLL_DETAIL_AGGRESSIVE_PTR_CAST_HPP

#include <boost/dll/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <cstring>              // std::memcpy
#include <memory>
#include <type_traits>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

#if defined(__GNUC__) && defined(__GNUC_MINOR__) && (__GNUC__ * 100 + __GNUC_MINOR__ > 301)
#   pragma GCC system_header
#endif

namespace boost { namespace dll { namespace detail {

// GCC warns when reinterpret_cast between function pointer and object pointer occur.
// This method suppress the warnings and ensures that such casts are safe.
template <class To, class From>
BOOST_FORCEINLINE typename std::enable_if<!std::is_member_pointer<To>::value && !std::is_reference<To>::value && !std::is_member_pointer<From>::value, To>::type
    aggressive_ptr_cast(From v) noexcept
{
    static_assert(
        std::is_pointer<To>::value && std::is_pointer<From>::value,
        "`agressive_ptr_cast` function must be used only for pointer casting."
    );

    static_assert(
        std::is_void< typename std::remove_pointer<To>::type >::value
        || std::is_void< typename std::remove_pointer<From>::type >::value,
        "`agressive_ptr_cast` function must be used only for casting to or from void pointers."
    );

    static_assert(
        sizeof(v) == sizeof(To),
        "Pointer to function and pointer to object differ in size on your platform."
    );

    return reinterpret_cast<To>(v);
}

#ifdef BOOST_MSVC
#   pragma warning(push)
#   pragma warning(disable: 4172) // "returning address of local variable or temporary" but **v is not local!
#endif

template <class To, class From>
BOOST_FORCEINLINE typename std::enable_if<std::is_reference<To>::value && !std::is_member_pointer<From>::value, To>::type
    aggressive_ptr_cast(From v) noexcept
{
    static_assert(
        std::is_pointer<From>::value,
        "`agressive_ptr_cast` function must be used only for pointer casting."
    );

    static_assert(
        std::is_void< typename std::remove_pointer<From>::type >::value,
        "`agressive_ptr_cast` function must be used only for casting to or from void pointers."
    );

    static_assert(
        sizeof(v) == sizeof(typename std::remove_reference<To>::type*),
        "Pointer to function and pointer to object differ in size on your platform."
    );
    return static_cast<To>(
        **reinterpret_cast<typename std::remove_reference<To>::type**>(
            v
        )
    );
}

#ifdef BOOST_MSVC
#   pragma warning(pop)
#endif

template <class To, class From>
BOOST_FORCEINLINE typename std::enable_if<std::is_member_pointer<To>::value && !std::is_member_pointer<From>::value, To>::type
    aggressive_ptr_cast(From v) noexcept
{
    static_assert(
        std::is_pointer<From>::value,
        "`agressive_ptr_cast` function must be used only for pointer casting."
    );

    static_assert(
        std::is_void< typename std::remove_pointer<From>::type >::value,
        "`agressive_ptr_cast` function must be used only for casting to or from void pointers."
    );

    To res = 0;
    std::memcpy(&res, &v, sizeof(From));
    return res;
}

template <class To, class From>
BOOST_FORCEINLINE typename std::enable_if<!std::is_member_pointer<To>::value && std::is_member_pointer<From>::value, To>::type
    aggressive_ptr_cast(From /* v */) noexcept
{
    static_assert(
        std::is_pointer<To>::value,
        "`agressive_ptr_cast` function must be used only for pointer casting."
    );

    static_assert(
        std::is_void< typename std::remove_pointer<To>::type >::value,
        "`agressive_ptr_cast` function must be used only for casting to or from void pointers."
    );

    static_assert(
        !sizeof(From),
        "Casting from member pointers to void pointer is not implemnted in `agressive_ptr_cast`."
    );

    return 0;
}

}}} // boost::dll::detail

#endif // BOOST_DLL_DETAIL_AGGRESSIVE_PTR_CAST_HPP

