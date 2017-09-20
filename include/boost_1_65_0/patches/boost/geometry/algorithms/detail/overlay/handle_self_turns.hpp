// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_SELF_TURNS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_SELF_TURNS_HPP

#include <boost/range.hpp>

#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/is_self_turn.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/within.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

struct discard_turns
{
    template <typename Turns, typename Clusters, typename Geometry0, typename Geometry1>
    static inline
    void apply(Turns& , Clusters const& , Geometry0 const& , Geometry1 const& )
    {}
};

template <overlay_type OverlayType, operation_type OperationType>
struct discard_closed_turns : discard_turns {};

// It is only implemented for operation_union, not in buffer
template <>
struct discard_closed_turns<overlay_union, operation_union>
{

    template <typename Turns, typename Clusters, typename Geometry0, typename Geometry1>
    static inline
    void apply(Turns& turns, Clusters const& clusters,
            Geometry0 const& geometry0, Geometry1 const& geometry1)
    {
        typedef typename boost::range_value<Turns>::type turn_type;

        for (typename boost::range_iterator<Turns>::type
                it = boost::begin(turns);
             it != boost::end(turns);
             ++it)
        {
            turn_type& turn = *it;

            if (turn.discarded || ! is_self_turn<overlay_union>(turn))
            {
                continue;
            }

            bool const within =
                    turn.operations[0].seg_id.source_index == 0
                    ? geometry::within(turn.point, geometry1)
                    : geometry::within(turn.point, geometry0);

            if (within)
            {
                // It is in the interior of the other geometry
                turn.discarded = true;
            }
        }
    }
};

struct discard_self_intersection_turns
{
private :

    template <typename Turns, typename Clusters>
    static inline
    bool any_blocked(signed_size_type cluster_id,
            const Turns& turns, Clusters const& clusters)
    {
        typename Clusters::const_iterator cit = clusters.find(cluster_id);
        if (cit == clusters.end())
        {
            return false;
        }
        cluster_info const& cinfo = cit->second;
        for (std::set<signed_size_type>::const_iterator it
             = cinfo.turn_indices.begin();
             it != cinfo.turn_indices.end(); ++it)
        {
            typename boost::range_value<Turns>::type const& turn = turns[*it];
            if (turn.any_blocked())
            {
                return true;
            }
        }
        return false;
    }

public :

    template <typename Turns, typename Clusters,
              typename Geometry0, typename Geometry1>
    static inline
    void apply(Turns& turns, Clusters const& clusters,
            Geometry0 const& geometry0, Geometry1 const& geometry1)
    {
        typedef typename boost::range_value<Turns>::type turn_type;

        for (typename boost::range_iterator<Turns>::type
                it = boost::begin(turns);
             it != boost::end(turns);
             ++it)
        {
            turn_type& turn = *it;

            if (turn.discarded || ! is_self_turn<overlay_intersection>(turn))
            {
                continue;
            }

            segment_identifier const& id0 = turn.operations[0].seg_id;
            segment_identifier const& id1 = turn.operations[1].seg_id;
            if (id0.multi_index != id1.multi_index
                    || (id0.ring_index == -1 && id1.ring_index == -1)
                    || (id0.ring_index >= 0 && id1.ring_index >= 0))
            {
                // Not an ii ring (int/ext) on same ring
                continue;
            }

            if (turn.cluster_id >= 0 && turn.has_colocated_both)
            {
                // Don't delete a self-ii-turn colocated with another ii-turn
                // (for example #case_recursive_boxes_70)
                // But for some cases (#case_58_iet) they should be deleted,
                // there are many self-turns there and also blocked turns there
                if (! any_blocked(turn.cluster_id, turns, clusters))
                {
                    continue;
                }
            }

            // It is a ii self-turn
            // Check if it is within the other geometry
            // If not, it can be ignored

            bool const within =
                    turn.operations[0].seg_id.source_index == 0
                    ? geometry::within(turn.point, geometry1)
                    : geometry::within(turn.point, geometry0);

            if (! within)
            {
                // It is not within another geometry, discard the turn
                turn.discarded = true;
            }
        }
    }
};

template <overlay_type OverlayType, operation_type OperationType>
struct discard_open_turns : discard_turns {};

// Handler it for intersection
template <>
struct discard_open_turns<overlay_intersection, operation_intersection>
        : discard_self_intersection_turns {};

// For difference, it should be done in a different way (TODO)

}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_SELF_TURNS_HPP
