#ifndef BOOST_SMART_PTR_DETAIL_LIGHTWEIGHT_THREAD_HPP_INCLUDED
#define BOOST_SMART_PTR_DETAIL_LIGHTWEIGHT_THREAD_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//  boost/detail/lightweight_thread.hpp
//
//  Copyright (c) 2002 Peter Dimov and Multi Media Ltd.
//  Copyright (c) 2008, 2018 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//
//
//  typedef /*...*/ lw_thread_t; // as pthread_t
//  template<class F> int lw_thread_create( lw_thread_t & th, F f );
//  void lw_thread_join( lw_thread_t th );

#include <thread>
#include <exception>

namespace boost
{
namespace detail
{

using lw_thread_t = std::thread*;

template<class F> int lw_thread_create( lw_thread_t& th, F f )
{
    try
    {
        th = new std::thread( f );
        return 0;
    }
    catch( std::exception const& )
    {
        return -1;
    }
}

void lw_thread_join( lw_thread_t th )
{
    th->join();
    delete th;
}

} // namespace detail
} // namespace boost

#endif // #ifndef BOOST_SMART_PTR_DETAIL_LIGHTWEIGHT_THREAD_HPP_INCLUDED
