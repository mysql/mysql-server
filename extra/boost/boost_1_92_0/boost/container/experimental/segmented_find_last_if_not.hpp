//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FIND_LAST_IF_NOT_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FIND_LAST_IF_NOT_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/compare_functors.hpp>
#include <boost/container/experimental/segmented_find_last_if.hpp>

namespace boost {
namespace container {

//! Returns an iterator to the last element for which \c pred
//! returns false in [first, last), or \c last if not found.
template <class FwdIt, class Sent, class Pred>
BOOST_CONTAINER_FORCEINLINE
FwdIt segmented_find_last_if_not(FwdIt first, Sent last, Pred pred)
{
   return (segmented_find_last_if)(first, last, not_pred<Pred>(pred));
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FIND_LAST_IF_NOT_HPP
