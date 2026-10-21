/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2021 Andrey Semashev
 */
/*!
 * \file   atomic/ipc_atomic_ref.hpp
 *
 * This header contains definition of \c ipc_atomic_ref template.
 */

#ifndef BOOST_ATOMIC_IPC_ATOMIC_REF_HPP_INCLUDED_
#define BOOST_ATOMIC_IPC_ATOMIC_REF_HPP_INCLUDED_

#include <type_traits>
#include <boost/assert.hpp>
#include <boost/memory_order.hpp>
#include <boost/atomic/capabilities.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/intptr.hpp>
#include <boost/atomic/detail/classify.hpp>
#include <boost/atomic/detail/atomic_ref_impl.hpp>
#include <boost/atomic/detail/type_traits/is_trivially_copyable.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {

//! Atomic reference to external object for inter-process communication
template< typename T >
class ipc_atomic_ref :
    public atomics::detail::base_atomic_ref< T, typename atomics::detail::classify< T >::type, true >
{
private:
    using base_type = atomics::detail::base_atomic_ref< T, typename atomics::detail::classify< T >::type, true >;
    using value_arg_type = typename base_type::value_arg_type;

public:
    using value_type = typename base_type::value_type;

    static_assert(sizeof(value_type) > 0u, "boost::ipc_atomic_ref<T> requires T to be a complete type");
    static_assert(atomics::detail::is_trivially_copyable< value_type >::value, "boost::ipc_atomic_ref<T> requires T to be a trivially copyable type");

private:
    using storage_type = typename base_type::storage_type;

public:
    ipc_atomic_ref(ipc_atomic_ref const&) = default;

    BOOST_FORCEINLINE explicit ipc_atomic_ref(value_type& v) noexcept : base_type(v)
    {
        // Check that referenced object alignment satisfies required alignment
        BOOST_ASSERT((((atomics::detail::uintptr_t)this->m_value) & (base_type::required_alignment - 1u)) == 0u);
    }

    ipc_atomic_ref& operator= (ipc_atomic_ref const&) = delete;

    BOOST_FORCEINLINE value_type operator= (value_arg_type v) const noexcept
    {
        this->store(v);
        return v;
    }

    BOOST_FORCEINLINE operator value_type() const noexcept
    {
        return this->load();
    }
};

#if !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)
template< typename T >
ipc_atomic_ref(T&) -> ipc_atomic_ref< T >;
#endif // !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)

//! IPC atomic reference factory function
template< typename T >
BOOST_FORCEINLINE ipc_atomic_ref< T > make_ipc_atomic_ref(T& value) noexcept
{
    return ipc_atomic_ref< T >(value);
}

} // namespace atomics

using atomics::ipc_atomic_ref;
using atomics::make_ipc_atomic_ref;

} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_IPC_ATOMIC_REF_HPP_INCLUDED_
