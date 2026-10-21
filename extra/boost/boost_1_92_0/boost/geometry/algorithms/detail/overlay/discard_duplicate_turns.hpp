// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2023.
// Modifications copyright (c) 2023 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DISCARD_DUPLICATE_TURNS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DISCARD_DUPLICATE_TURNS_HPP

#include <map>
#include <set>

#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_ring.hpp>
#include <boost/geometry/views/enumerate_view.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{


inline bool same_multi_and_ring_id(segment_identifier const& first,
                                   segment_identifier const& second)
{
    return first.ring_index == second.ring_index
        && first.multi_index == second.multi_index;
}

template <typename Geometry0, typename Geometry1>
inline bool is_consecutive(segment_identifier const& first,
                           segment_identifier const& second,
                           Geometry0 const& geometry0,
                           Geometry1 const& geometry1)
{
    if (first.source_index == second.source_index
        && first.ring_index == second.ring_index
        && first.multi_index == second.multi_index)
    {
        // If the segment distance is 1, there are no segments in between
        // and the segments are consecutive.
        signed_size_type const sd = first.source_index == 0
                        ? segment_distance(geometry0, first, second)
                        : segment_distance(geometry1, first, second);
        return sd <= 1;
    }
    return false;
}

// Returns true if two turns correspond to each other in the sense that one has
// "method_start" and the other has "method_touch" or "method_interior" at that
// same point (but with next segment)
template <typename Turn, typename Geometry0, typename Geometry1>
inline bool corresponding_turn(Turn const& turn, Turn const& start_turn,
                               Geometry0 const& geometry0,
                               Geometry1 const& geometry1)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < 2; i++)
    {
        for (std::size_t j = 0; j < 2; j++)
        {
            if (same_multi_and_ring_id(turn.operations[i].seg_id,
                                       start_turn.operations[j].seg_id))
            {
                // Verify if all operations are consecutive
                if (is_consecutive(turn.operations[i].seg_id,
                                   start_turn.operations[j].seg_id,
                                   geometry0, geometry1)
                    && is_consecutive(turn.operations[1 - i].seg_id,
                                      start_turn.operations[1 - j].seg_id,
                                      geometry0, geometry1))

                {
                    count++;
                }
            }
        }
    }
    // An intersection is located on exactly two rings
    // The corresponding turn, if any, should be located on the same two rings.
    return count == 2;
}

//  Verify turns (other than start, and cross) if they are present in the map, and if so,
//  if they have the other turns as corresponding, discard the start turn.
template <typename Turns, typename TurnBySegmentMap, typename Geometry0, typename Geometry1>
void discard_duplicate_start_turns(Turns& turns,
                                   TurnBySegmentMap const& start_turns_by_segment,
                                   Geometry0 const& geometry0,
                                   Geometry1 const& geometry1)
{
    using multi_and_ring_id_type = std::pair<signed_size_type, signed_size_type>;
    auto adapt_id = [](segment_identifier const& seg_id)
    {
        return multi_and_ring_id_type{seg_id.multi_index, seg_id.ring_index};
    };

#if defined(BOOST_GEOMETRY_CONCEPT_FIX_START_TURNS)
    // Handle it by removing them from clusters.
    // Not complete yet.
    return;
#endif

    for (auto& turn : turns)
    {
        // Any turn which "crosses" does not have a corresponding turn.
        // Also avoid comparing "start" with itself
        if (turn.method == method_crosses || turn.method == method_start)
        {
            continue;
        }
        bool const is_touch = turn.method == method_touch;
        for (auto const& op : turn.operations)
        {
            auto it = start_turns_by_segment.find(adapt_id(op.seg_id));
            if (it == start_turns_by_segment.end())
            {
                continue;
            }
            for (std::size_t const& i : it->second)
            {
                auto& start_turn = turns[i];
                if (start_turn.cluster_id == turn.cluster_id
                    && corresponding_turn(turn, start_turn, geometry0, geometry1))
                {
                    // Discard the start turn, unless there is a touch before.
                    // In that case the start is used and the touch is discarded.
                    (is_touch ? turn : start_turn).discarded = true;
                }
            }
        }
    }
}

//  Discard turns for the following (rare) case:
//  - they are consecutive
//  - the first has a touch, the second a touch_interior
// And then one of the segments touches the others next in the middle.
// This is geometrically not possible, and caused by floating point precision.
// Discard the second (touch interior)
template <typename Turns, typename Geometry0, typename Geometry1>
void discard_touch_touch_interior_turns(Turns& turns,
                                        Geometry0 const& geometry0,
                                        Geometry1 const& geometry1)
{
    for (auto& current_turn : turns)
    {
        if (current_turn.method != method_touch_interior)
        {
            // Because touch_interior is a rarer case, it is more efficient to start with that
            continue;
        }
        for (auto const& previous_turn : turns)
        {
            if (previous_turn.method != method_touch)
            {
                continue;
            }

            // Compare 0 with 0 and 1 with 1
            // Note that 0 has always source 0 and 1 has always source 1
            // (not in buffer). Therefore this comparison is OK.
            // MAYBE we need to check for buffer.
            bool const common0 = current_turn.operations[0].seg_id == previous_turn.operations[0].seg_id;
            bool const common1 = current_turn.operations[1].seg_id == previous_turn.operations[1].seg_id;

            // If one of the operations is common, and the other is not, then there is one comment segment.
            bool const has_common_segment = common0 != common1;

            if (! has_common_segment)
            {
                continue;
            }

            // If the second index (1) is common, we need to check consecutivity of the first index (0)
            // and vice versa.
            bool const consecutive =
                common1 ? is_consecutive(previous_turn.operations[0].seg_id, current_turn.operations[0].seg_id, geometry0, geometry1)
                : is_consecutive(previous_turn.operations[1].seg_id, current_turn.operations[1].seg_id, geometry0, geometry1);

            if (consecutive)
            {
                current_turn.discarded = true;
            }
        }
    }
}

template <typename Turns, typename Geometry0, typename Geometry1>
void discard_duplicate_turns(Turns& turns,
                             Geometry0 const& geometry0,
                             Geometry1 const& geometry1)
{
    // Start turns are generated, in case the previous turn is missed.
    // But often it is not missed, and then it should be deleted.
    // This is how it can be
    // (in float, collinear, points far apart due to floating point precision)
    //   [m, i s:0, v:6 1/1 (1) // u s:1, v:5  pnt (2.54044, 3.12623)]
    //   [s, i s:0, v:7 0/1 (0) // u s:1, v:5  pnt (2.70711, 3.29289)]
    //
    // Also, if two turns are consecutive, and one is touch and the other touch_interior,
    // the touch_interior is discarded.

    using multi_and_ring_id_type = std::pair<signed_size_type, signed_size_type>;

    auto add_to_map = [](auto const& turn, auto& map, std::size_t index)
    {
        auto adapt_id = [](segment_identifier const& seg_id)
        {
            return multi_and_ring_id_type{seg_id.multi_index, seg_id.ring_index};
        };
        for (auto const& op : turn.operations)
        {
            map[adapt_id(op.seg_id)].insert(index);
        }
    };

    // Build map of start turns (multi/ring-id -> turn indices)
    // and count touch and touch_interior turns (to verify if later checks are needed)
    std::map<multi_and_ring_id_type, std::set<std::size_t>> start_turns_by_segment;
    std::size_t touch_count = 0;
    std::size_t touch_interior_count = 0;
    for (auto const& item : util::enumerate(turns))
    {
        auto const& turn = item.value;
        switch(turn.method)
        {
            case method_start: add_to_map(turn, start_turns_by_segment, item.index); break;
            case method_touch: touch_count++; break;
            case method_touch_interior: touch_interior_count++; break;
            default: break;
        }
    }

    if (!start_turns_by_segment.empty())
    {
        discard_duplicate_start_turns(turns, start_turns_by_segment, geometry0, geometry1);
    }

    if (touch_count > 0 && touch_interior_count > 0)
    {
       discard_touch_touch_interior_turns(turns, geometry0, geometry1);
    }
}


}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DISCARD_DUPLICATE_TURNS_HPP
