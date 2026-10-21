/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/atomic_ref.hpp
 *
 * This header contains definition of \c atomic_ref template.
 */

#ifndef BOOST_ATOMIC_ATOMIC_REF_HPP_INCLUDED_
#define BOOST_ATOMIC_ATOMIC_REF_HPP_INCLUDED_

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

//! Atomic reference to external object
template< typename T >
class atomic_ref :
    public atomics::detail::base_atomic_ref< T, typename atomics::detail::classify< T >::type, false >
{
private:
    using base_type = atomics::detail::base_atomic_ref< T, typename atomics::detail::classify< T >::type, false >;
    using value_arg_type = typename base_type::value_arg_type;

public:
    using value_type = typename base_type::value_type;

    static_assert(sizeof(value_type) > 0u, "boost::atomic_ref<T> requires T to be a complete type");
    static_assert(atomics::detail::is_trivially_copyable< value_type >::value, "boost::atomic_ref<T> requires T to be a trivially copyable type");

private:
    using storage_type = typename base_type::storage_type;

public:
    atomic_ref(atomic_ref const&) = default;

    BOOST_FORCEINLINE explicit atomic_ref(value_type& v) noexcept : base_type(v)
    {
        // Check that referenced object alignment satisfies required alignment
        BOOST_ASSERT((((atomics::detail::uintptr_t)this->m_value) & (base_type::required_alignment - 1u)) == 0u);
    }

    atomic_ref& operator= (atomic_ref const&) = delete;

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
atomic_ref(T&) -> atomic_ref< T >;
#endif // !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)

//! Atomic reference factory function
template< typename T >
BOOST_FORCEINLINE atomic_ref< T > make_atomic_ref(T& value) noexcept
{
    return atomic_ref< T >(value);
}

} // namespace atomics

using atomics::atomic_ref;
using atomics::make_atomic_ref;

} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_ATOMIC_REF_HPP_INCLUDED_
