/* Hub container.
 * 
 * Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_CONTAINER_PMR_HUB_HPP
#define BOOST_CONTAINER_PMR_HUB_HPP

#if defined (_MSC_VER)
#  pragma once
#endif

#include <boost/container/hub.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>

namespace boost {
namespace container {
namespace pmr {

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

template <class T>
using hub = boost::container::hub<T, polymorphic_allocator<T> >;

#endif

//! A portable metafunction to obtain a hub
//! that uses a polymorphic allocator.
template<class T>
struct hub_of
{
   typedef boost::container::hub
      < T, polymorphic_allocator<T> > type;
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#endif   //BOOST_CONTAINER_PMR_HUB_HPP
