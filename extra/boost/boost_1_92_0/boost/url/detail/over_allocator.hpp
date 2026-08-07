 //
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_OVER_ALLOCATOR_HPP
#define BOOST_URL_DETAIL_OVER_ALLOCATOR_HPP

#include <boost/config.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/assert.hpp>
#include <boost/type_traits/is_final.hpp>
#include <boost/type_traits/type_with_alignment.hpp>
#ifdef BOOST_NO_CXX11_ALLOCATOR
# include <boost/core/allocator_traits.hpp>
#endif
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace boost {
namespace urls {
namespace detail {

// This is a workaround for allocator_traits
// implementations which falsely claim C++11
// compatibility.
#ifdef BOOST_NO_CXX11_ALLOCATOR
template<class Alloc>
using allocator_traits =
    boost::allocator_traits<Alloc>;
#else
template<class Alloc>
using allocator_traits = std::allocator_traits<Alloc>;
#endif

template<class T, class Allocator>
class over_allocator
    : private empty_value<Allocator>
{
    template<class U, class OtherAlloc>
    friend class over_allocator;

    std::size_t extra_;

public:
    using is_always_equal = std::false_type;
    using value_type = typename
        allocator_traits<typename allocator_traits<
            Allocator>::template rebind_alloc<T>>::value_type;
    using pointer = typename
        allocator_traits<typename allocator_traits<
            Allocator>::template rebind_alloc<T>>::pointer;
    using const_pointer = typename
        allocator_traits<typename allocator_traits<
            Allocator>::template rebind_alloc<T>>::const_pointer;
    using size_type = typename
        allocator_traits<typename allocator_traits<
            Allocator>::template rebind_alloc<T>>::size_type;
    using difference_type = typename
        allocator_traits<typename allocator_traits<
            Allocator>::template rebind_alloc<T>>::difference_type;

    template<class U>
    struct rebind
    {
        using other = over_allocator<U, Allocator>;
    };

    over_allocator(
        std::size_t extra,
        Allocator const& alloc)
        : empty_value<Allocator>(
            empty_init, alloc)
        , extra_(extra)
    {
    }

    template<class U>
    over_allocator(over_allocator<U, Allocator> const& other) noexcept
        : empty_value<Allocator>(
            empty_init, other.get())
        , extra_(other.extra_)
    {
    }

    pointer
    allocate(size_type n)
    {
        BOOST_ASSERT(n == 1);
        using U = typename boost::type_with_alignment<
            alignof(value_type)>::type;
        auto constexpr S = sizeof(U);
        using A = typename allocator_traits<
            Allocator>::template rebind_alloc<U>;
        A a(this->get());
        return reinterpret_cast<pointer>(
            std::allocator_traits<A>::allocate(a,
                (n * sizeof(value_type) + extra_ + S - 1) / S));
    }

    void
    deallocate(pointer p, size_type n)
    {
        BOOST_ASSERT(n == 1);
        using U = typename boost::type_with_alignment<
            alignof(value_type)>::type;
        auto constexpr S = sizeof(U);
        using A = typename allocator_traits<
            Allocator>::template rebind_alloc<U>;
        A a{this->get()};
        std::allocator_traits<A>::deallocate(a,
            reinterpret_cast<U*>(p),
                (n * sizeof(value_type) + extra_ + S - 1) / S);
    }

#if defined(BOOST_LIBSTDCXX_VERSION) && BOOST_LIBSTDCXX_VERSION < 60000
    template<class U, class... Args>
    void
    construct(U* ptr, Args&&... args)
    {
        ::new((void*)ptr) U(std::forward<Args>(args)...);
    }

    template<class U>
    void
    destroy(U* ptr)
    {
        ptr->~U();
    }
#endif

    template<class U>
    friend
    bool
    operator==(
        over_allocator const& lhs,
        over_allocator<U, Allocator> const& rhs)
    {
        return
            lhs.get() == rhs.get() &&
            lhs.extra_ == rhs.extra_;
    }

    template<class U>
    friend
    bool
    operator!=(
        over_allocator const& lhs,
        over_allocator<U, Allocator> const& rhs)
    {
        return ! (lhs == rhs);
    }
};

} // detail
} // urls
} // boost

#endif
