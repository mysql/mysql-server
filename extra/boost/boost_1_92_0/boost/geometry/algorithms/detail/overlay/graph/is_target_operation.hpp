// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_IS_TARGET_OPERATION_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_IS_TARGET_OPERATION_HPP

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_operation_id.hpp>

#include <set>
#include <utility>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// For continue/continue cases where one of the targets
// is the same as a target of the other target.
// If is_target_ahead_op == true:
// CC turn -------> target_op -----> target_other
//         ------------------------> target_other
// In this case, take the target_op
template <typename Turns>
std::pair<bool, bool> is_cc_target_ahead(Turns const& turns, turn_operation_id const& toi)
{
    auto const& turn = turns[toi.turn_index];
    auto const& op = turn.operations[toi.operation_index];
    auto const& other_op = turn.operations[1 - toi.operation_index];

    auto const target_op = op.enriched.travels_to_ip_index;
    auto const target_other = other_op.enriched.travels_to_ip_index;

    auto const nop_result = std::make_pair(false, false);

    if (target_op < 0 || target_other < 0 || target_op == target_other)
    {
        return nop_result;
    }

    if (turn.is_clustered()
        && (turns[target_op].cluster_id == turn.cluster_id
            || turns[target_other].cluster_id == turn.cluster_id))
    {
        return nop_result;
    }

    auto has_target = [](auto const& turn, signed_size_type target)
    {
        return turn.operations[0].enriched.travels_to_ip_index == target
            || turn.operations[1].enriched.travels_to_ip_index == target;
    };

    bool const is_target_ahead_op = has_target(turns[target_op], target_other);
    bool const is_target_ahead_other = has_target(turns[target_other], target_op);
    if (is_target_ahead_op == is_target_ahead_other)
    {
        // It is not so that one is the target of the operation of the other,
        // or it is the case for both of them (this cannot be handled or
        // it does not occur).
        return nop_result;
    }

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
    std::cout << "Decide for turn " << toi.turn_index << " " << toi.operation_index
        << " targets: " << target_op
        << " / " << target_other
        << " clusters: " << turns[target_op].cluster_id
        << " / " << turns[target_other].cluster_id
        << " via " << std::boolalpha << is_target_ahead_op << " / " << is_target_ahead_other
        << std::endl;
#endif

    return std::make_pair(true, is_target_ahead_op);
}

template <typename Operation>
bool is_better_collinear_for_union(Operation const& op, Operation const& other_op,
        turn_operation_id const& toi, turn_operation_id const& other_toi)
{
    // Continue, prefer the one having no polygon on the left
    if (op.enriched.count_left > 0 && other_op.enriched.count_left == 0)
    {
        return false;
    }
    if (op.enriched.count_left == 0 && other_op.enriched.count_left > 0)
    {
        return true;
    }

    // For union the cc target ahead should not be called.

    // In some cases, one goes to a target further, while the other goes to a target closer,
    // and that target than goes to that same next target.

    if (op.enriched.ahead_side != other_op.enriched.ahead_side)
    {
        // If one of them goes left (1), this one is preferred above collinear or right (-1),
        // whatever the distance.
        //                                ^
        //    (empty)                    /  going left
        //                              /
        // >----------------------------
        //                 \             .
        //    (polygon)     \  going right
        //                   v
        //
        // The left is also preferred above the other one going collinearly.
        // Finally, if one of them is collinear, it is preferred above the one going right.

        return op.enriched.ahead_side > other_op.enriched.ahead_side;
    }

    // If both have the same side, the preference depends on which side.
    // For a left turn (1), the one with the smallest distance is preferred.
    // For a right turn (-1), the one with the largest distance is preferred.
    // For collinear (0), it should not matter.

    return
        op.enriched.ahead_side == 1
        ? op.enriched.ahead_distance_of_side_change
            <= other_op.enriched.ahead_distance_of_side_change
        : op.enriched.ahead_distance_of_side_change
            >= other_op.enriched.ahead_distance_of_side_change;
}

// The same for intersection - but it needs turns for the same target ahead check.
template <typename Operation, typename Turns>
bool is_better_collinear_for_intersection(Operation const& op, Operation const& other_op,
        turn_operation_id const& toi, turn_operation_id const& other_toi, Turns const& turns)
{
    // Continue, prefer the one having no polygon on the left
    if (op.enriched.count_right < 2 && other_op.enriched.count_right >= 2)
    {
        return false;
    }
    if (op.enriched.count_right >= 0 && other_op.enriched.count_right < 2)
    {
        return true;
    }

    auto const target_ahead = is_cc_target_ahead(turns, toi);
    if (target_ahead.first)
    {
        return target_ahead.second;
    }

    return op.enriched.ahead_distance_of_side_change
        <= other_op.enriched.ahead_distance_of_side_change;
}

template <operation_type Operation>
struct is_better_collinear_target {};

template <>
struct is_better_collinear_target<operation_union>
{
    template <typename Operation, typename Turns>
    static bool apply(Operation const& op, Operation const& other_op,
        turn_operation_id const& toi, turn_operation_id const& other_toi, Turns const&)
    {
        return is_better_collinear_for_union(op, other_op, toi, other_toi);
    }
};

template <>
struct is_better_collinear_target<operation_intersection>
{
    template <typename Operation, typename Turns>
    static bool apply(Operation const& op, Operation const& other_op,
        turn_operation_id const& toi, turn_operation_id const& other_toi, Turns const& turns)
    {
        return is_better_collinear_for_intersection(op, other_op, toi, other_toi, turns);
    }
};

template <operation_type TargetOperation, typename Turns>
bool is_target_operation(Turns const& turns, turn_operation_id const& toi)
{
    auto const& turn = turns[toi.turn_index];
    auto const& op = turn.operations[toi.operation_index];
    if (op.enriched.travels_to_ip_index < 0
        || op.enriched.travels_to_ip_index >= static_cast<int>(turns.size()))
    {
        return false;
    }
    if (op.operation == TargetOperation)
    {
        return true;
    }
    if (op.operation != operation_continue)
    {
        return false;
    }

    turn_operation_id const other_toi{toi.turn_index, 1 - toi.operation_index};
    auto const& other_op = turn.operations[other_toi.operation_index];
    return is_better_collinear_target<TargetOperation>
        ::apply(op, other_op, toi, other_toi, turns);
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_IS_TARGET_OPERATION_HPP
