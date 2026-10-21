#ifndef BOOST_SMART_PTR_DETAIL_SP_COUNTED_IMPL_HPP_INCLUDED
#define BOOST_SMART_PTR_DETAIL_SP_COUNTED_IMPL_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//
//  detail/sp_counted_impl.hpp
//
//  Copyright (c) 2001, 2002, 2003 Peter Dimov and Multi Media Ltd.
//  Copyright 2004-2005 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/smart_ptr/detail/sp_counted_base.hpp>
#include <boost/smart_ptr/detail/deprecated_macros.hpp>
#include <boost/core/checked_delete.hpp>
#include <boost/core/addressof.hpp>
#include <boost/config.hpp>

#include <memory>           // std::allocator, std::allocator_traits
#include <cstddef>          // std::size_t

namespace boost
{

namespace detail
{

// get_local_deleter

template<class D> class local_sp_deleter;

template<class D> D * get_local_deleter( D * /*p*/ ) noexcept
{
    return 0;
}

template<class D> D * get_local_deleter( local_sp_deleter<D> * p ) noexcept;

//

template<class X> class BOOST_SYMBOL_VISIBLE sp_counted_impl_p: public sp_counted_base
{
private:

    X * px_;

    sp_counted_impl_p( sp_counted_impl_p const & );
    sp_counted_impl_p & operator= ( sp_counted_impl_p const & );

    typedef sp_counted_impl_p<X> this_type;

public:

    explicit sp_counted_impl_p( X * px ): px_( px )
    {
    }

    void dispose() noexcept override
    {
        boost::checked_delete( px_ );
    }

    void * get_deleter( sp_typeinfo_ const & ) noexcept override
    {
        return 0;
    }

    void * get_local_deleter( sp_typeinfo_ const & ) noexcept override
    {
        return 0;
    }

    void * get_untyped_deleter() noexcept override
    {
        return 0;
    }
};

template<class P, class D> class BOOST_SYMBOL_VISIBLE sp_counted_impl_pd: public sp_counted_base
{
private:

    P ptr; // copy constructor must not throw
    D del; // copy/move constructor must not throw

    sp_counted_impl_pd( sp_counted_impl_pd const & );
    sp_counted_impl_pd & operator= ( sp_counted_impl_pd const & );

    typedef sp_counted_impl_pd<P, D> this_type;

public:

    // pre: d(p) must not throw

    sp_counted_impl_pd( P p, D & d ): ptr( p ), del( static_cast< D&& >( d ) )
    {
    }

    sp_counted_impl_pd( P p ): ptr( p ), del()
    {
    }

    void dispose() noexcept override
    {
        del( ptr );
    }

    void * get_deleter( sp_typeinfo_ const & ti ) noexcept override
    {
        return ti == BOOST_SP_TYPEID_(D)? &reinterpret_cast<char&>( del ): 0;
    }

    void * get_local_deleter( sp_typeinfo_ const & ti ) noexcept override
    {
        return ti == BOOST_SP_TYPEID_(D)? boost::detail::get_local_deleter( boost::addressof( del ) ): 0;
    }

    void * get_untyped_deleter() noexcept override
    {
        return &reinterpret_cast<char&>( del );
    }
};

template<class P, class D, class A> class BOOST_SYMBOL_VISIBLE sp_counted_impl_pda: public sp_counted_base
{
private:

    P p_; // copy constructor must not throw
    D d_; // copy/move constructor must not throw
    A a_; // copy constructor must not throw

    sp_counted_impl_pda( sp_counted_impl_pda const & );
    sp_counted_impl_pda & operator= ( sp_counted_impl_pda const & );

    typedef sp_counted_impl_pda<P, D, A> this_type;

public:

    // pre: d( p ) must not throw

    sp_counted_impl_pda( P p, D & d, A a ): p_( p ), d_( static_cast< D&& >( d ) ), a_( a )
    {
    }

    sp_counted_impl_pda( P p, A a ): p_( p ), d_( a ), a_( a )
    {
    }

    void dispose() noexcept override
    {
        d_( p_ );
    }

    void destroy() noexcept override
    {
        typedef typename std::allocator_traits<A>::template rebind_alloc< this_type > A2;

        A2 a2( a_ );

        this->~this_type();

        a2.deallocate( this, 1 );
    }

    void * get_deleter( sp_typeinfo_ const & ti ) noexcept override
    {
        return ti == BOOST_SP_TYPEID_( D )? &reinterpret_cast<char&>( d_ ): 0;
    }

    void * get_local_deleter( sp_typeinfo_ const & ti ) noexcept override
    {
        return ti == BOOST_SP_TYPEID_( D )? boost::detail::get_local_deleter( boost::addressof( d_ ) ): 0;
    }

    void * get_untyped_deleter() noexcept override
    {
        return &reinterpret_cast<char&>( d_ );
    }
};

} // namespace detail

} // namespace boost

#endif  // #ifndef BOOST_SMART_PTR_DETAIL_SP_COUNTED_IMPL_HPP_INCLUDED
