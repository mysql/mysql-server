#ifndef BOOST_LEAF_CONFIG_TLS_ARRAY_HPP_INCLUDED
#define BOOST_LEAF_CONFIG_TLS_ARRAY_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Copyright (c) 2022 Khalil Estell

// This header implements the TLS API specified in tls.hpp for platforms that
// provide TLS by indexing an array (this is typical for embedded platforms).
// The array is accessed via user-defined functions.

namespace boost { namespace leaf {

namespace tls
{
    // The TLS support defined in this header requires the following two
    // functions to be defined elsewhere:
    void * read_void_ptr( int tls_index ) noexcept;
    void write_void_ptr( int tls_index, void * ) noexcept;
}

} }

////////////////////////////////////////

#include <atomic>
#include <limits>
#include <cstdint>
#include <type_traits>

#ifndef BOOST_LEAF_CFG_TLS_INDEX_TYPE
#   define BOOST_LEAF_CFG_TLS_INDEX_TYPE unsigned char
#endif

#ifndef BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX
#   define BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX 0
#endif

static_assert((BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX) >= 0,
    "Bad BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX");

#ifdef BOOST_LEAF_CFG_TLS_ARRAY_SIZE
    static_assert((BOOST_LEAF_CFG_TLS_ARRAY_SIZE) > (BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX),
        "Bad BOOST_LEAF_CFG_TLS_ARRAY_SIZE");
    static_assert((BOOST_LEAF_CFG_TLS_ARRAY_SIZE) - 1 <= std::numeric_limits<BOOST_LEAF_CFG_TLS_INDEX_TYPE>::max(),
        "Bad BOOST_LEAF_CFG_TLS_ARRAY_SIZE");
#endif

////////////////////////////////////////

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

    template <class=void>
    class BOOST_LEAF_SYMBOL_VISIBLE index_counter
    {
        static int c_;

        BOOST_LEAF_ALWAYS_INLINE static BOOST_LEAF_CFG_TLS_INDEX_TYPE next_() noexcept
        {
            int idx = ++c_;
            BOOST_LEAF_ASSERT(idx > (BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX + 1));
            BOOST_LEAF_ASSERT(idx < (BOOST_LEAF_CFG_TLS_ARRAY_SIZE));
            return idx;
        }

    public:

        template <class T>
        BOOST_LEAF_ALWAYS_INLINE static BOOST_LEAF_CFG_TLS_INDEX_TYPE next() noexcept
        {
            return next_();
        }
    };

    template <class T>
    int index_counter<T>::c_ = BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX + 1;

    template <class T>
    struct BOOST_LEAF_SYMBOL_VISIBLE tls_index
    {
        static BOOST_LEAF_CFG_TLS_INDEX_TYPE idx;
    };

    template <class T>
    BOOST_LEAF_CFG_TLS_INDEX_TYPE tls_index<T>::idx = BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX + 1;

    template <class T>
    struct BOOST_LEAF_SYMBOL_VISIBLE reserve_tls_index
    {
        static BOOST_LEAF_CFG_TLS_INDEX_TYPE const idx;
    };

    template <class T>
    BOOST_LEAF_CFG_TLS_INDEX_TYPE const reserve_tls_index<T>::idx = tls_index<T>::idx = index_counter<>::next<T>();
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
        static_assert(sizeof(std::intptr_t) >= sizeof(unsigned), "Incompatible tls_array implementation");
        write_void_ptr(BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX, (void *) (std::intptr_t) x);
    }

    BOOST_LEAF_ALWAYS_INLINE unsigned read_current_error_id() noexcept
    {
        static_assert(sizeof(std::intptr_t) >= sizeof(unsigned), "Incompatible tls_array implementation");
        return (unsigned) (std::intptr_t) read_void_ptr(BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX);
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE void reserve_ptr()
    {
        (void) detail::reserve_tls_index<T>::idx;
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE void write_ptr( T * p ) noexcept
    {
        int tls_idx = detail::tls_index<T>::idx;
        BOOST_LEAF_ASSERT(tls_idx != (BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX + 1));
        --tls_idx;
        write_void_ptr(tls_idx, p);
        BOOST_LEAF_ASSERT(read_void_ptr(tls_idx) == p);
    }

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE T * read_ptr() noexcept
    {
        int tls_idx = detail::tls_index<T>::idx;
        if( tls_idx == (BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX + 1) )
            return nullptr;
        --tls_idx;
        return reinterpret_cast<T *>(read_void_ptr(tls_idx));
    }
} // namespace tls

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_CONFIG_TLS_ARRAY_HPP_INCLUDED
