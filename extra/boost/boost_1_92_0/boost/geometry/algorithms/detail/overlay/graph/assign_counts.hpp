// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_COUNTS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_COUNTS_HPP

#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_operation_id.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <typename Turns, typename Clusters>
void assign_clustered_self_counts(Turns& turns, Clusters const& clusters)
{
    auto is_self_cluster = [&turns](auto const& cinfo)
    {
        return std::all_of(cinfo.turn_indices.cbegin(), cinfo.turn_indices.cend(),
            [&](auto index) { return turns[index].is_self(); });
    };

    for (auto const& cluster : clusters)
    {
        if (! is_self_cluster(cluster.second))
        {
            continue;
        }

        // If a cluster only contains self-intersections, their previously assigned right counts
        // should be adapted, they are within the other geometry,
        // and otherwise they were discarded already in handle_self_turns
        for (auto index : cluster.second.turn_indices)
        {
            for (auto& op : turns[index].operations)
            {
                op.enriched.count_right += 1;
            }
        }
    }
}

template <typename Turn>
void assign_counts(Turn& turn)
{
    using counts_per_op_t = std::pair<operation_type, std::size_t>;

    auto assign_left = [&turn](std::size_t count)
    {
        for (auto& op : turn.operations)
        {
            op.enriched.count_left = count;
        }
    };

    auto assign_right = [&turn](std::size_t count)
    {
        for (auto& op : turn.operations)
        {
            op.enriched.count_right = count;
        }
    };

    auto assign_for = [&turn](counts_per_op_t const& op1, counts_per_op_t op2, auto&& assign)
    {
        for (auto& op : turn.operations)
        {
            if (op.operation == op1.first) { assign(op.enriched, op1.second); }
            else if (op.operation == op2.first) { assign(op.enriched, op2.second); }
        }
    };

    auto assign_left_for = [&assign_for](counts_per_op_t const& op1, counts_per_op_t op2)
    {
        assign_for(op1, op2, [](auto& enriched, auto count) { enriched.count_left = count; });
    };

    auto assign_right_for = [&assign_for](counts_per_op_t const& op1, counts_per_op_t op2)
    {
        assign_for(op1, op2, [](auto& enriched, auto count) { enriched.count_right = count; });
    };

    auto assign_left_incoming_for = [&assign_for](counts_per_op_t const& op1, counts_per_op_t op2)
    {
        assign_for(op1, op2, [](auto& enriched, auto count) { enriched.count_left_incoming = count; });
    };

    auto assign_right_incoming_for = [&assign_for](counts_per_op_t const& op1, counts_per_op_t op2)
    {
        assign_for(op1, op2, [](auto& enriched, auto count) { enriched.count_right_incoming = count; });
    };

    if (turn.combination(operation_intersection, operation_union))
    {
        assign_left_for({operation_union, 0}, {operation_intersection, 1});
        assign_right_for({operation_union, 1}, {operation_intersection, 2});

        // For i/u (either originating from a "cross" or from a touch, but the segments cross
        // one another), the incoming counts can be assigned.

        // For other operations, this is not trivial (without retrieving the geometry).
        // It is only necessary for some collinear cases to see how they arrive at the target.
        // If it is not available, distance ahead is used.
        assign_left_incoming_for({operation_union, 1}, {operation_intersection, 0});
        assign_right_incoming_for({operation_union, 2}, {operation_intersection, 1});
    }
    else if (turn.combination(operation_blocked, operation_union))
    {
        assign_left_for({operation_union, 0}, {operation_blocked, 1});
        assign_right(1);
    }
    else if (turn.combination(operation_blocked, operation_intersection))
    {
        assign_left(1);
        assign_right_for({operation_blocked, 1}, {operation_intersection, 2});
    }
    else if (turn.both(operation_continue))
    {
        assign_left(0);
        assign_right(2);
    }
    else if (turn.both(operation_union))
    {
        assign_left(0);
        assign_right(1);
    }
    else if (turn.both(operation_intersection))
    {
        assign_left(1);
        assign_right(2);
    }
}

template <typename Turns>
void assign_unclustered_counts(Turns& turns)
{
    for (auto& turn : turns)
    {
        if (turn.is_clustered() || turn.discarded)
        {
            continue;
        }
        assign_counts(turn);
    }
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_COUNTS_HPP
