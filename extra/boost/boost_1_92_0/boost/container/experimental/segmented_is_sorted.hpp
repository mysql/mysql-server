//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_SORTED_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_SORTED_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_is_sorted_until.hpp>

namespace boost {
namespace container {

//! Returns true if [first, last) is sorted according to \c comp.
template <class FwdIt, class Sent, class Comp>
BOOST_CONTAINER_FORCEINLINE
bool segmented_is_sorted(FwdIt first, Sent last, Comp comp)
{
   return (segmented_is_sorted_until)(first, last, comp) == last;
}

//! Returns true if [first, last) is sorted using operator<.
template <class FwdIt, class Sent>
BOOST_CONTAINER_FORCEINLINE
bool segmented_is_sorted(FwdIt first, Sent last)
{
   return (segmented_is_sorted_until)(first, last) == last;
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_SORTED_HPP
