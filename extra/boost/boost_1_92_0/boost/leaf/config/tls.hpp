#ifndef BOOST_LEAF_CONFIG_TLS_HPP_INCLUDED
#define BOOST_LEAF_CONFIG_TLS_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

namespace boost { namespace leaf {

// The following declarations specify the thread local storage API used
// internally by LEAF. To port LEAF to a new TLS API, provide definitions for
// each of these functions.
namespace tls
{
    // Generate the next unique error_id. Values start at 1 and increment by 4.
    // Error ids must be unique for the lifetime of the process, and this
    // function must be thread-safe. Postcondition: (id & 3) == 1 && id != 0.
    //
    // This function may not fail.
    unsigned generate_next_error_id() noexcept;

    // Write x to the TLS for the current error_id. The initial value for each
    // thread must be 0. Precondition: x == 0 or (x & 3) == 1.
    //
    // This function may not fail.
    void write_current_error_id( unsigned x ) noexcept;

    // Read the current error_id for this thread. The initial value for each
    // thread must be 0.
    //
    // This function may not fail.
    unsigned read_current_error_id() noexcept;

    // Reserve TLS storage for T. The TLS may be allocated dynamically on the
    // first call to reserve_ptr<T>, but subsequent calls must reuse the same
    // TLS. On platforms where allocation is not needed, this function is
    // still defined but does nothing.
    //
    // This function may throw on allocation failure.
    template <class T>
    void reserve_ptr();

    // Write p to the TLS previously reserved for T by a call to reserve_ptr<T>.
    // It is illegal to call write_ptr<T> without a prior successful call to
    // reserve_ptr<T>.
    //
    // This function may not fail.
    template <class T>
    void write_ptr( T * p ) noexcept;

    // Read the T * value previously written in the TLS for T. Returns nullptr
    // if TLS for T has not yet been reserved.
    //
    // This function may not fail.
    template <class T>
    T * read_ptr() noexcept;
} // namespace tls

} } // namespace boost::leaf

#if defined(BOOST_LEAF_TLS_FREERTOS)
#   include <boost/leaf/config/tls_freertos.hpp>
#   ifndef BOOST_LEAF_USE_TLS_ARRAY
#       define BOOST_LEAF_USE_TLS_ARRAY
#   endif
#endif

#ifndef BOOST_LEAF_USE_TLS_ARRAY
#	ifdef BOOST_LEAF_CFG_TLS_INDEX_TYPE
#		warning "BOOST_LEAF_CFG_TLS_INDEX_TYPE" is ignored if BOOST_LEAF_USE_TLS_ARRAY is not defined.
#	endif
#	ifdef BOOST_LEAF_CFG_TLS_ARRAY_SIZE
#		warning "BOOST_LEAF_CFG_TLS_ARRAY_SIZE" is ignored if BOOST_LEAF_USE_TLS_ARRAY is not defined.
#	endif
#	ifdef BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX
#		warning "BOOST_LEAF_CFG_TLS_ARRAY_START_INDEX" is ignored if BOOST_LEAF_USE_TLS_ARRAY is not defined.
#	endif
#endif

#if defined BOOST_LEAF_USE_TLS_ARRAY
#   include <boost/leaf/config/tls_array.hpp>
#elif BOOST_LEAF_CFG_WIN32 == 2
#   include <boost/leaf/config/tls_win32.hpp>
#elif defined(BOOST_LEAF_NO_THREADS)
#   include <boost/leaf/config/tls_globals.hpp>
#else
#   include <boost/leaf/config/tls_cpp11.hpp>
#endif

#endif // #ifndef BOOST_LEAF_CONFIG_TLS_HPP_INCLUDED
