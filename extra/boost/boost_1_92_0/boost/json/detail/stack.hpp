//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_DETAIL_STACK_HPP
#define BOOST_JSON_DETAIL_STACK_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/mp11/integral.hpp>
#include <cstring>
#include <type_traits>

namespace boost {
namespace json {
namespace detail {

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000

template<class T>
struct is_trivially_copy_assignable
    : mp11::mp_bool<
        std::is_copy_assignable<T>::value &&
          std::has_trivial_copy_assign<T>::value >
{};

#else

using std::is_trivially_copy_assignable;

#endif

class stack
{
    template< class T = void >
    struct non_trivial;

    storage_ptr sp_;
    std::size_t cap_ = 0;
    std::size_t size_ = 0;
    non_trivial<>* head_ = nullptr;
    unsigned char* base_ = nullptr;
    unsigned char* buf_ = nullptr;

public:
    BOOST_JSON_DECL
    ~stack();

    stack() = default;

    stack(
        storage_ptr sp,
        unsigned char* buf,
        std::size_t buf_size) noexcept;

    bool
    empty() const noexcept
    {
        return size_ == 0;
    }

    BOOST_JSON_DECL
    void
    clear() noexcept;

    void
    reserve(std::size_t n)
    {
        if(n > cap_)
            reserve_impl(n);
    }

    template<class T>
    void
    push(T&& t)
    {
        using U = remove_cvref<T>;
        push( static_cast<T&&>(t), is_trivially_copy_assignable<U>() );
    }

    template<class T>
    void
    push_unchecked(
        T const& t);

    template<class T>
    void
    peek(T& t);

    template<class T>
    void
    pop(T& t)
    {
        using U = remove_cvref<T>;
        pop( t, is_trivially_copy_assignable<U>() );
    }

private:
    template<class T> void push(
        T const& t, std::true_type);
    template<class T> void push(
        T&& t, std::false_type);
    template<class T> void pop(
        T& t, std::true_type);
    template<class T> void pop(
        T& t, std::false_type);

    BOOST_JSON_DECL
    void
    reserve_impl(
        std::size_t n);
};

} // detail
} // namespace json
} // namespace boost

#include <boost/json/detail/impl/stack.hpp>

#endif
