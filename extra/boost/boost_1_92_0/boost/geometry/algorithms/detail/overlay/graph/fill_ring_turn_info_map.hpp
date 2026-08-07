// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_FILL_RING_TURN_INFO_MAP_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_FILL_RING_TURN_INFO_MAP_HPP

#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_ring.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <typename TurnInfoMap, typename Turns>
void update_ring_turn_info_map(TurnInfoMap& ring_turn_info_map, Turns const& turns)
{
    for (auto const& turn : turns)
    {
        for (int i = 0; i < 2; i++)
        {
            auto const& op = turn.operations[i];
            if (op.enriched.is_traversed)
            {
                ring_identifier const ring_id = ring_id_by_seg_id(op.seg_id);
                ring_turn_info_map[ring_id].has_traversed_turn = true;

                if (op.operation == operation_continue)
                {
                    // Continue operations should mark the other operation
                    // as traversed too
                    auto const& other_op = turn.operations[1 - i];
                    ring_identifier const other_ring_id = ring_id_by_seg_id(other_op.seg_id);
                    ring_turn_info_map[other_ring_id].has_traversed_turn = true;
                }
            }
        }
    }
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_FILL_RING_TURN_INFO_MAP_HPP
