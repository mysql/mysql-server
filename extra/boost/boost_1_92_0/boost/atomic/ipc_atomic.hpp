/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/ipc_atomic.hpp
 *
 * This header contains definition of \c ipc_atomic template.
 */

#ifndef BOOST_ATOMIC_IPC_ATOMIC_HPP_INCLUDED_
#define BOOST_ATOMIC_IPC_ATOMIC_HPP_INCLUDED_

#include <cstddef>
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

//! Atomic object for inter-process communication
template< typename T >
class ipc_atomic :
    public atomics::detail::base_atomic< T, typename atomics::detail::classify< T >::type, true >
{
private:
    using base_type = atomics::detail::base_atomic< T, typename atomics::detail::classify< T >::type, true >;
    using value_arg_type = typename base_type::value_arg_type;

public:
    using value_type = typename base_type::value_type;

    static_assert(sizeof(value_type) > 0u, "boost::ipc_atomic<T> requires T to be a complete type");
    static_assert(atomics::detail::is_trivially_copyable< value_type >::value, "boost::ipc_atomic<T> requires T to be a trivially copyable type");

public:
    ipc_atomic() = default;

    BOOST_FORCEINLINE constexpr ipc_atomic(value_arg_type v) noexcept : base_type(v)
    {
    }

    ipc_atomic(ipc_atomic const&) = delete;
    ipc_atomic& operator= (ipc_atomic const&) = delete;
    ipc_atomic& operator= (ipc_atomic const&) volatile = delete;

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

} // namespace atomics

using atomics::ipc_atomic;

} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_IPC_ATOMIC_HPP_INCLUDED_
