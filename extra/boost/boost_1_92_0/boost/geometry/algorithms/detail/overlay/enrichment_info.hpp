// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICHMENT_INFO_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICHMENT_INFO_HPP

#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/algorithms/detail/signed_size_type.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{


/*!
\brief Keeps info to enrich intersection info (per source)
\details Class to keep information necessary for traversal phase (a phase
    of the overlay process). The information is gathered during the
    enrichment phase
 */
template<typename Point>
struct enrichment_info
{
    inline signed_size_type get_next_turn_index() const
    {
        return travels_to_ip_index;
    }

    // vertex to which is free travel after this IP,
    // so from "segment_index+1" to "travels_to_vertex_index", without IP-s,
    // can be -1
    signed_size_type travels_to_vertex_index{-1};

    // same but now IP index, so "next IP index" but not on THIS segment
    signed_size_type travels_to_ip_index{-1};

    bool startable{true}; // Can be used to start a traversal

    // Counts if polygons left/right of this operation.
    // Outgoing from this operation:
    signed_size_type count_left{-1};
    signed_size_type count_right{-1};

    // Incoming:
    signed_size_type count_left_incoming{-1};
    signed_size_type count_right_incoming{-1};

    // Set to true if the turn is traversed.
    // This is used for one condition.
    bool is_traversed{false};

    // The component_id of the operation. At uu or ii turns, it will
    // usually change components (dependent on the constellation).
    // This is detected by detect_biconnected_components
    signed_size_type component_id{-1};

    // Rank of this operation in a cluster. It can be used to compare
    // (again) two operations originating in the same cluster.
    std::size_t rank{0};

    // For CC turns, the distance ahead to the first side change
    using comparable_distance_type = coordinate_type_t<Point>;
    comparable_distance_type ahead_distance_of_side_change{-1};

    // For CC turns, the side of the ahead segment.
    // Indicated conform side strategies:
    //   1 for left
    //  -1 for right
    //   0 for collinear
    // -99 for unassigned
    int ahead_side{-99};
};


}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL



}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICHMENT_INFO_HPP
