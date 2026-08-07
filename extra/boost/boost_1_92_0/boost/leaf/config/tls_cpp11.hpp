#ifndef BOOST_LEAF_CONFIG_TLS_CPP11_HPP_INCLUDED
#define BOOST_LEAF_CONFIG_TLS_CPP11_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This header implements the TLS API specified in tls.hpp using the C++11
// built-in thread_local storage class specifier. On Windows, this
// implementation does not allow error objects to cross DLL boundaries. If this
// is required, define BOOST_LEAF_CFG_WIN32=2 before including any LEAF headers
// to enable the alternative implementation defined in tls_win32.hpp.

#include <atomic>
#include <cstdint>

namespace boost { namespace leaf {

namespace detail
{
    using atomic_unsigned_int = std::atomic<unsigned int>;

    template <class=void>
    struct BOOST_LEAF_SYMBOL_VISIBLE id_factory
    {
        static atomic_unsigned_int counter;
    };

    template <class T>
    atomic_unsigned_int id_factory<T>::counter(1);

    template <class T>
    struct BOOST_LEAF_SYMBOL_VISIBLE ptr
    {
        static thread_local T * p;
    };

    template <class T>
    thread_local T * ptr<T>::p;

    template <class=void>
    struct BOOST_LEAF_SYMBOL_VISIBLE current_error_id_storage
    {
        static thread_local unsigned x;
    };

    template <class T>
    thread_local unsigned current_error_id_storage<T>::x;
} // namespace detail

} } // namespace boost::leaf

////////////////////////////////////////

namespace boost { namespace leaf {

namespace tls
{
    BOOST_LEAF_ALWAYS_INLINE unsigned generate_next_error_id() noexcept
    {
        unsigned id = (detail::id_factory<>::counter += 4);
        BOOST_LEAF_ASSERT((id&3) == 1);
        return id;
    }

    BOOST_LEAF_ALWAYS_INLINE void write_current_error_id( unsigned x ) noexcept
    {
        detail::current_error_id_storage<>::x = x;
    }

    BOOST_LEAF_ALWAYS_INLINE unsigned read_current_error_id() noexcept
    {
        return detail::current_error_id_storage<>::x;
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE void reserve_ptr()
    {
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE void write_ptr( T * p ) noexcept
    {
        detail::ptr<T>::p = p;
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE T * read_ptr() noexcept
    {
        return detail::ptr<T>::p;
    }
} // namespace tls

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_CONFIG_TLS_CPP11_HPP_INCLUDED
