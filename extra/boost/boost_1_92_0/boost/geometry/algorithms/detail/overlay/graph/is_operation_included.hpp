// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_IS_OPERATION_INCLUDED_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_IS_OPERATION_INCLUDED_HPP

#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <operation_type target_operation>
struct is_operation_included {};

template <>
struct is_operation_included<operation_intersection>
{
    template <typename Operation>
    static bool apply(Operation const& op)
    {
        return op.enriched.count_right >= 2;
    }
};

template <>
struct is_operation_included<operation_union>
{
    template <typename Operation>
    static bool apply(Operation const& op)
    {
        return op.enriched.count_left == 0;
    }
};

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_IS_OPERATION_INCLUDED_HPP
