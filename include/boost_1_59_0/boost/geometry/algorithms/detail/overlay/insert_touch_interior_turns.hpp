// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_INSERT_TOUCH_INTERIOR_TURNS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_INSERT_TOUCH_INTERIOR_TURNS_HPP

#include <deque>
#include <iterator>
#include <set>

#include <boost/range.hpp>

#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/policies/robustness/segment_ratio.hpp>

#include <boost/geometry/algorithms/append.hpp>
#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/convert.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/self_turn_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>


namespace boost { namespace geometry
{

namespace detail { namespace overlay
{


struct self_turns_no_interrupt_policy
{
    static bool const enabled = false;
    static bool const has_intersections = false;

    template <typename Range>
    static inline bool apply(Range const&)
    {
        return false;
    }
};


template <typename Turn>
static inline
typename Turn::turn_operation_type get_correct_op(Turn const& t)
{
    return
        (t.operations[0].fraction.is_zero()
         || t.operations[0].fraction.is_one())
        ?
        t.operations[1]
        :
        t.operations[0]
        ;
}


template <typename MAA_Turn>
struct maa_turn_less
{
    bool operator()(MAA_Turn const& t1, MAA_Turn const& t2) const
    {
        BOOST_GEOMETRY_ASSERT(t1.method == method_touch_interior);
        BOOST_GEOMETRY_ASSERT(t2.method == method_touch_interior);

        typename MAA_Turn::turn_operation_type op1 = get_correct_op(t1);
        typename MAA_Turn::turn_operation_type op2 = get_correct_op(t2);

        BOOST_GEOMETRY_ASSERT(! op1.fraction.is_zero()
                              && ! op1.fraction.is_one());
        BOOST_GEOMETRY_ASSERT(! op2.fraction.is_zero()
                              && ! op2.fraction.is_one());


        if (op1.seg_id.multi_index != op2.seg_id.multi_index)
        {
            return op1.seg_id.multi_index < op2.seg_id.multi_index;
        }
        if (op1.seg_id.ring_index != op2.seg_id.ring_index)
        {
            return op1.seg_id.ring_index < op2.seg_id.ring_index;
        }
        if (op1.seg_id.segment_index != op2.seg_id.segment_index)
        {
            return op1.seg_id.segment_index < op2.seg_id.segment_index;
        }
        return op1.fraction < op2.fraction;
    }
};


template <typename Areal, typename TagIn = typename tag<Areal>::type>
struct insert_maa_turns
{};

template <typename Box>
struct insert_maa_turns<Box, box_tag>
{
    template <typename BoxOut, typename TurnIterator>
    static inline TurnIterator apply(Box const& box,
                                     TurnIterator first,
                                     TurnIterator,
                                     BoxOut& box_out,
                                     int = -1,
                                     int = -1)
    {
        geometry::convert(box, box_out);
        return first;
    }
};


template <typename Ring>
struct insert_maa_turns<Ring, ring_tag>
{
    template <typename RingOut, typename TurnIterator>
    static inline TurnIterator apply(Ring const& ring,
                                     TurnIterator first,
                                     TurnIterator last,
                                     RingOut& ring_out,
                                     int ring_index = -1,
                                     int multi_index = -1)
    {
        typedef typename boost::range_iterator
            <
                Ring const
            >::type iterator_type;

        typedef typename std::iterator_traits
            <
                TurnIterator
            >::value_type::turn_operation_type operation_type;

        typename boost::range_size<Ring>::type point_index = 0;
        for (iterator_type it = boost::begin(ring);
             it != boost::end(ring);
             ++it, ++point_index)
        {
            geometry::append(ring_out, ring[point_index]);
            while (first != last)
            {
                operation_type op = get_correct_op(*first);
                if (op.seg_id.multi_index == multi_index
                    &&
                    op.seg_id.ring_index == ring_index
                    &&
                    op.seg_id.segment_index
                    == static_cast<signed_size_type>(point_index))
                {
                    geometry::append(ring_out, first->point);
                    ++first;
                }
                else
                {
                    break;
                }
            }
        }
        return first;
    }
};


template <typename Polygon>
struct insert_maa_turns<Polygon, polygon_tag>
{
    template
    <
        typename Ring,
        typename RingIterator,
        typename PolygonOut,
        typename TurnIterator
    >
    static inline
    TurnIterator do_interior_rings(RingIterator rings_first,
                                   RingIterator rings_last,
                                   TurnIterator first,
                                   TurnIterator last,
                                   PolygonOut& polygon_out,
                                   int multi_index)
    {
        typename geometry::ring_type<PolygonOut>::type ring_out;
        int ring_index = 0;
        for (RingIterator rit = rings_first;
             rit != rings_last;
             ++rit, ++ring_index)
        {
            geometry::clear(ring_out);
            first = insert_maa_turns<Ring>::apply(*rit,
                                                  first,
                                                  last,
                                                  ring_out,
                                                  ring_index,
                                                  multi_index);
            geometry::traits::push_back
                <
                    typename boost::remove_reference
                        <
                            typename traits::interior_mutable_type
                                <
                                    PolygonOut
                                >::type
                        >::type
                >::apply(geometry::interior_rings(polygon_out), ring_out);
        }
        return first;
    }

    template
    <
        typename Ring,
        typename InteriorRings,
        typename PolygonOut,
        typename TurnIterator
    >
    static inline
    TurnIterator do_interior_rings(InteriorRings const& irings,
                                   TurnIterator first,
                                   TurnIterator last,
                                   PolygonOut& polygon_out,
                                   int multi_index)
    {
        return do_interior_rings<Ring>(boost::begin(irings),
                                       boost::end(irings),
                                       first,
                                       last,
                                       polygon_out,
                                       multi_index);
    }

    template <typename PolygonOut, typename TurnIterator>
    static inline TurnIterator apply(Polygon const& polygon,
                                     TurnIterator first,
                                     TurnIterator last,
                                     PolygonOut& polygon_out,
                                     int multi_index = -1)
    {
        typedef typename geometry::ring_type<Polygon>::type ring_type;

        typename geometry::ring_type<PolygonOut>::type ring_out;

        // do exterior ring
        first = insert_maa_turns
            <
                ring_type
            >::apply(exterior_ring(polygon),
                     first,
                     last,
                     ring_out,
                     -1,
                     multi_index);
        geometry::append(polygon_out, ring_out, -1);

        return do_interior_rings<ring_type>(interior_rings(polygon),
                                            first,
                                            last,
                                            polygon_out,
                                            multi_index);
    }
};

template <typename MultiPolygon>
struct insert_maa_turns<MultiPolygon, multi_polygon_tag>
{
    template <typename MultiPolygonOut, typename TurnIterator>
    static inline TurnIterator apply(MultiPolygon const& multi_polygon,
                                     TurnIterator first,
                                     TurnIterator last,
                                     MultiPolygonOut& multi_polygon_out)
    {
        typedef typename boost::range_iterator
            <
                MultiPolygon const
            >::type iterator_type;

        typename boost::range_value<MultiPolygonOut>::type polygon_out;

        int polygon_index = 0;
        for (iterator_type it = boost::begin(multi_polygon);
             it != boost::end(multi_polygon);
             ++it, ++polygon_index)
        {
            geometry::clear(polygon_out);

            first = insert_maa_turns
                <
                    typename boost::range_value<MultiPolygon>::type
                >::apply(*it, first, last, polygon_out, polygon_index);

            geometry::traits::push_back
                <
                    MultiPolygonOut
                >::apply(multi_polygon_out, polygon_out);
        }
        return first;
    }
};


// returns true if the input geometry has been modified (in which case
// the modified geometry is stored in geometry_out), false otherwise
template <typename GeometryIn, typename GeometryOut, typename RobustPolicy>
inline bool insert_touch_interior_turns(GeometryIn const& geometry_in,
                                        GeometryOut& geometry_out,
                                        RobustPolicy const& robust_policy)
{
    typedef turn_info
        <
            typename point_type<GeometryIn>::type,
            typename geometry::segment_ratio_type
                <
                    typename point_type<GeometryIn>::type, RobustPolicy
                >::type
        > turn_type;

    typedef std::deque<turn_type> turns_container_type;

    self_turns_no_interrupt_policy interrupt_policy;

    // compute self turns
    turns_container_type turns;
    geometry::self_turns
        <
            get_turn_info<assign_null_policy>
        >(geometry_in, robust_policy, turns, interrupt_policy);

    // select touch interior turns
    typedef std::set<turn_type, maa_turn_less<turn_type> > maa_turn_set;

    maa_turn_set maa_turns;
    for (typename turns_container_type::const_iterator it = turns.begin();
         it != turns.end();
         ++it)
    {
        if (it->method == method_touch_interior)
        {
            maa_turns.insert(*it);
        }
    }

    // if not such turns indicate that the input geometry need not change
    if (maa_turns.empty())
    {
        return false;
    }

    // insert the touch interior turns
    insert_maa_turns
        <
            GeometryIn
        >::apply(geometry_in, maa_turns.begin(), maa_turns.end(), geometry_out);

    // return that the input geometry needed change
    return true;
}


}} // detail::overlay

}} // boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_INSERT_TOUCH_INTERIOR_TURNS_HPP
