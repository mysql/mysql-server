// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ADAPT_OPERATIONS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ADAPT_OPERATIONS_HPP

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_operation_id.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// Changes the operation of a UU turn, following a UX turn, to X (blocked)
// under certain conditions, such that it is not followed
// ADAPT: still necessary for just 2 cases. It should be possible to fix it in get_turn_info instead.
// It happens in issue_1100_rev (union) and in ticket_10108 (sym diff)
//
// Situation sketch (issue_1100 reversed - the non reversed version does not need the workaround).
//
// +-----\   +--------+
// |      \  |        |
// |       \ |        |
// |         + UX     |
// |         |        |
// |   P     |     Q  |
// |         |        |
// |         |        |
// +---------+--------+
//          UU            <- This UU turn is wrong, it should be UX
//                           If it is UU, it will travel right (as designed) and polygon P will
//                           not be part of the union.
//
template <typename Turns>
void block_ux_uu_workaround(Turns& turns)
{
    auto get_op_index = [](auto const& turn, auto&& lambda)
    {
        for (int i = 0; i < 2; i++)
        {
            if (lambda(turn.operations[i]))
            {
                return i;
            }
        }
        return -1;
    };

    for (std::size_t turn_index = 0; turn_index < turns.size(); turn_index++)
    {
        auto const& turn = turns[turn_index];
        if (turn.is_clustered()
            || turn.discarded
            || turn.is_self()
            || ! turn.combination(operation_blocked, operation_union))
        {
            continue;
        }

        auto const blocked_index = get_op_index(turn, [](auto const& op)
            {
                return op.operation == operation_blocked;
            });

        auto const& blocked_op = turn.operations[blocked_index];
        auto const next_index = blocked_op.enriched.travels_to_ip_index;
        if (next_index < 0 || next_index >= static_cast<int>(turns.size()))
        {
            continue;
        }

        auto& next_turn = turns[next_index];
        if (next_turn.is_self() || ! next_turn.both(operation_union))
        {
            // If it is a self-turn, they will both have the same source, and both are union.
            // The "other source" is then ambiguous.
            // It might be handled later, but only with extra conditions.
            continue;
        }

        int const same_source_index = get_op_index(next_turn, [&](auto const& op)
            {
                return op.seg_id.source_index == blocked_op.seg_id.source_index;
            });

        if (same_source_index < 0)
        {
            continue;
        }
        int const other_index = 1 - same_source_index;
        auto& opposite_op = next_turn.operations[other_index];
        if (opposite_op.enriched.travels_to_ip_index != static_cast<signed_size_type>(turn_index))
        {
            // It is not opposite
            continue;
        }

        opposite_op.operation = operation_blocked;
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        std::cout << "BLOCK XU/UU at turns " << turn_index << "/" << next_index << std::endl;
#endif
    }
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ADAPT_OPERATIONS_HPP
