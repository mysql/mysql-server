//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_PARTITIONED_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_PARTITIONED_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <boost/container/experimental/segmented_find_if_not.hpp>
#include <boost/container/experimental/segmented_none_of.hpp>

namespace boost {
namespace container {

//! Returns \c true if all elements satisfying \c pred appear before
//! all elements that do not, i.e. the range [first, last) is
//! partitioned with respect to \c pred.
//! Exploits segmentation when available.
template <class InpIter, class Sent, class Pred>
inline bool segmented_is_partitioned(InpIter first, Sent last, Pred pred)
{
   first = (segmented_find_if_not)(first, last, pred);
   return (segmented_none_of)(first, last, pred);
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_PARTITIONED_HPP
