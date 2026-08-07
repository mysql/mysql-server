// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_TOI_BY_INCOMING_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_TOI_BY_INCOMING_HPP

#include <boost/geometry/algorithms/detail/overlay/turn_operation_id.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// For two operations from a cluster, having the same target, and having the same rank,
// the outgoing side makes it unclear. This function inspects the target and uses the incoming
// side, which should be more clear.
template <typename Turns, typename Operation>
bool select_toi_for_union(turn_operation_id& result, Operation const& op0, Operation const& op1, 
        turn_operation_id const& toi0, turn_operation_id const& toi1,
        Turns const& turns)
{
    if (op0.enriched.travels_to_ip_index != op1.enriched.travels_to_ip_index
        || op0.enriched.travels_to_ip_index < 0)
    {
        // Not the same target
        return false;
    }
    auto const& target_turn = turns[op0.enriched.travels_to_ip_index];
    auto const& target_op0 = target_turn.operations[0];
    auto const& target_op1 = target_turn.operations[1];

    bool const is_target_for_union0 = target_op0.enriched.count_left_incoming == 0;
    bool const is_target_for_union1 = target_op1.enriched.count_left_incoming == 0;
    if (is_target_for_union0 == is_target_for_union1)
    {
        // There is no incoming operation usable for union, or both are the same.
        return false;
    }

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
    std::cout << "SELECT_BY_INCOMING " << toi0 << " vs " << toi1
        << " " << operation_char(op0.operation) << operation_char(op1.operation)
        << " traveling to " << op0.enriched.travels_to_ip_index
        << std::endl;
#endif

    if (target_op0.seg_id.multi_index == target_op1.seg_id.multi_index)
    {
        // They have the same ring (should not occur normally, in buffer)
        // so they cannot be used for selection.
        return false;
    }

    if (is_target_for_union0)
    {
        if (target_op0.seg_id.multi_index == op0.seg_id.multi_index)
        {
            result = toi0;
            return true;
        }
        if (target_op0.seg_id.multi_index == op1.seg_id.multi_index)
        {
            result = toi1;
            return true;
        }
    }
    else
    {
        if (target_op1.seg_id.multi_index == op0.seg_id.multi_index)
        {
            result = toi0;
            return true;
        }
        if (target_op1.seg_id.multi_index == op1.seg_id.multi_index)
        {
            result = toi1;
            return true;
        }
    }
    return false;
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_TOI_BY_INCOMING_HPP
