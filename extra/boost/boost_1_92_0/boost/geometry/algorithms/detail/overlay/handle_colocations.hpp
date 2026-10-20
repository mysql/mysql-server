// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2017-2023 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2017-2024.
// Modifications copyright (c) 2017-2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_COLOCATIONS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_COLOCATIONS_HPP

#include <cstddef>
#include <algorithm>
#include <map>
#include <vector>

#include <boost/core/ignore_unused.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/value_type.hpp>

#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/do_reverse.hpp>
#include <boost/geometry/algorithms/detail/overlay/colocate_clusters.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_clusters.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_ring.hpp>
#include <boost/geometry/algorithms/detail/overlay/is_self_turn.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>
#include <boost/geometry/util/constexpr.hpp>

#if defined(BOOST_GEOMETRY_DEBUG_HANDLE_COLOCATIONS)
#  include <iostream>
#  include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#  include <boost/geometry/io/wkt/wkt.hpp>
#  define BOOST_GEOMETRY_DEBUG_IDENTIFIER
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// Removes clusters which have only one point left, or are empty.
template <typename Turns, typename Clusters>
inline void remove_clusters(Turns& turns, Clusters& clusters)
{
    auto it = clusters.begin();
    while (it != clusters.end())
    {
        // Hold iterator and increase. We can erase cit, this keeps the
        // iterator valid (cf The standard associative-container erase idiom)
        auto current_it = it;
        ++it;

        auto const& turn_indices = current_it->second.turn_indices;
        if (turn_indices.size() == 1)
        {
            auto const turn_index = *turn_indices.begin();
            turns[turn_index].cluster_id = -1;
            clusters.erase(current_it);
        }
    }
}

template <typename Turns, typename Clusters>
inline void cleanup_clusters(Turns& turns, Clusters& clusters)
{
    // Removes discarded turns from clusters
    for (auto& pair : clusters)
    {
        auto& cinfo = pair.second;
        auto& indices = cinfo.turn_indices;
        for (auto sit = indices.begin(); sit != indices.end(); /* no increment */)
        {
            auto current_it = sit;
            ++sit;

            auto const turn_index = *current_it;
            if (turns[turn_index].discarded)
            {
                indices.erase(current_it);
            }
        }
    }

    remove_clusters(turns, clusters);
}


template
<
    typename Turns,
    typename Clusters
>
inline void assign_cluster_ids(Turns& turns, Clusters const& clusters)
{
    for (auto& turn : turns)
    {
        turn.cluster_id = -1;
    }
    for (auto const& kv : clusters)
    {
        for (auto const& index : kv.second.turn_indices)
        {
            turns[index].cluster_id = kv.first;
        }
    }
}

// Get clusters and assign their ids
template<typename Turns, typename Clusters>
inline void handle_colocations(Turns& turns, Clusters& clusters)
{
    get_clusters(turns, clusters);
    assign_cluster_ids(turns, clusters);
}


}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_COLOCATIONS_HPP
