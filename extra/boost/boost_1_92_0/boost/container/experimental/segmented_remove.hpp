//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_REMOVE_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_REMOVE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_find.hpp>
#include <boost/container/experimental/segmented_remove_copy.hpp>

namespace boost {
namespace container {

//! Removes all elements equal to \c value from [first, last),
//! moving retained elements forward. Returns iterator to new end.
template <class FwdIt, class Sent, class T>
FwdIt segmented_remove(FwdIt first, Sent last, const T& value)
{
   typedef segmented_iterator_traits<FwdIt> traits;
   first = detail_algo::segmented_find_dispatch
      (first, last, value
      , typename traits::is_segmented_iterator()
      , typename iterator_traits<FwdIt>::iterator_category());

   if(first == last)
      return last;

   FwdIt next = first;
   ++next;

   return detail_algo::segmented_remove_copy_dispatch<true>
      (next, last, first, value,
       typename traits::is_segmented_iterator(),
       typename iterator_traits<FwdIt>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_REMOVE_HPP
