/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2025 Andrey Semashev
 */
/*!
 * \file   atomic/wait_result.hpp
 *
 * This header contains definition of the \c wait_result class template.
 */

#ifndef BOOST_ATOMIC_WAIT_RESULT_HPP_INCLUDED_
#define BOOST_ATOMIC_WAIT_RESULT_HPP_INCLUDED_

#include <type_traits>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {

//! The structure contains the result of a timed waiting operation
template< typename T >
struct wait_result
{
    //! Last value read as part of the waiting operation
    T value;
    //! Indicates whether the waiting operation has ended due to timeout
    bool timeout;

    constexpr wait_result() noexcept(std::is_nothrow_default_constructible< T >::value) :
        value(),
        timeout(false)
    {
    }

    template< typename U, typename = typename std::enable_if< std::is_constructible< T, U&& >::value >::type >
    constexpr wait_result(U&& val, bool tout) noexcept(std::is_nothrow_constructible< T, U&& >::value) :
        value(static_cast< U&& >(val)),
        timeout(tout)
    {
    }
};

} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_WAIT_RESULT_HPP_INCLUDED_
