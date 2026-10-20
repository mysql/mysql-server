//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_STACK_HPP
#define BOOST_JSON_DETAIL_IMPL_STACK_HPP

#include <boost/core/detail/static_assert.hpp>
#include <memory>

namespace boost {
namespace json {
namespace detail {

template<>
struct stack::non_trivial<void>
{
    using relocate_t = non_trivial* (*) (non_trivial*, void*);

    relocate_t rel;
    non_trivial* next;
    std::size_t offset;

    BOOST_JSON_DECL
    non_trivial<>*
    destroy() noexcept;

    BOOST_JSON_DECL
    non_trivial*
    relocate(void* dst) noexcept;

protected:
    ~non_trivial() = default;
};

template< class T >
struct stack::non_trivial
    : stack::non_trivial<void>
{
    T obj;

    explicit
    non_trivial(T t, non_trivial<>* next, std::size_t offset)
        : non_trivial<void>{relocate, next, offset}, obj( std::move(t) )
    {}

    static
    non_trivial<>*
    relocate(non_trivial<>* src, void* dest) noexcept
    {
        non_trivial* self = static_cast<non_trivial*>(src);
        non_trivial<>* result = nullptr;
        if( dest )
            result = ::new(dest) non_trivial( std::move(*self) );
        self->~non_trivial();
        return result;
    }
};

template<class T>
void
stack::
push_unchecked(T const& t)
{
    constexpr std::size_t n = sizeof(T);
    BOOST_CORE_STATIC_ASSERT( is_trivially_copy_assignable<T>::value );
    BOOST_ASSERT( n <= cap_ - size_ );
    std::memcpy( base_ + size_, &t, n );
    size_ += n;
}

template<class T>
void
stack::
peek(T& t)
{
    constexpr std::size_t n = sizeof(T);
    BOOST_CORE_STATIC_ASSERT( is_trivially_copy_assignable<T>::value );
    BOOST_ASSERT( size_ >= n );
    std::memcpy( &t, base_ + size_ - n, n );
}

//--------------------------------------

// trivial
template<class T>
void
stack::
push(T const& t, std::true_type)
{
    if( sizeof(T) > cap_ - size_ )
        reserve_impl( sizeof(T) + size_ );
    push_unchecked(t);
}

// non-trivial
template<class T>
void
stack::
push(T&& t, std::false_type)
{
    BOOST_CORE_STATIC_ASSERT( ! is_trivially_copy_assignable<T>::value );

    using Holder = non_trivial< remove_cvref<T> >;
    constexpr std::size_t size = sizeof(Holder);
    constexpr std::size_t alignment = alignof(Holder);

    void* ptr;
    std::size_t offset;
    do
    {
        std::size_t space = cap_ - size_;
        unsigned char* buf = base_ + size_;
        ptr = buf;
        if( std::align(alignment, size, ptr, space) )
        {
            offset = (reinterpret_cast<unsigned char*>(ptr) - buf) + size;
            break;
        }

        reserve_impl(size_ + size + alignment - 1);
    }
    while(true);
    BOOST_ASSERT(
        (reinterpret_cast<unsigned char*>(ptr) + size - offset) ==
        (base_ + size_) );

    head_ = ::new(ptr) Holder( static_cast<T&&>(t), head_, offset );
    size_ += offset;
}

// trivial
template<class T>
void
stack::
pop(T& t, std::true_type)
{
    BOOST_ASSERT( size_ >= sizeof(T) );
    peek(t);
    size_ -= sizeof(T);
}

// non-trivial
template<class T>
void
stack::
pop(T& t, std::false_type)
{
    auto next = head_->next;
    auto offset = head_->offset;

    using U = remove_cvref<T>;
    using Holder = non_trivial<U>;
    auto const head = static_cast<Holder*>(head_);

    t = std::move( head->obj );
    head->~Holder();

    head_ = next;
    size_ -= offset;
}

} // detail
} // json
} // boost

#endif
