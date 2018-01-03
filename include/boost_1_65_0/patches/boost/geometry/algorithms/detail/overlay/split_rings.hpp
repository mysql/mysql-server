// Boost.Geometry

// Copyright (c) 2015, 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SPLIT_RINGS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SPLIT_RINGS_HPP

#include <deque>
#include <iterator>
#include <list>
#include <set>
#include <stack>

#include <boost/range.hpp>

#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/point_type.hpp>

#include <boost/geometry/policies/compare.hpp>
#include <boost/geometry/policies/robustness/segment_ratio_type.hpp>

#include <boost/geometry/util/condition.hpp>

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/inconsistent_turns_exception.hpp>
#include <boost/geometry/algorithms/detail/overlay/insert_touch_interior_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/self_turn_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>


namespace boost { namespace geometry
{

namespace detail { namespace overlay
{


// the ring_as_dcl class implements a ring using a doubly-connected list;
// it is a model of a bidirectional-traversal Boost.Range and
// supports:
// (1) constant-time push_back(), size() and empty() operations;
// (2) splitting at two vertices (in order to create two rings out of
//     the initial ring) also in constant time.
//
// The second operation cannot be done in constant time using typical
// random access ranges, which is precisely the reason why this class
// has been introduced and used.
template
<
    typename Point,
    closure_selector Closure,
    typename List = std::list<Point>
>
class ring_as_dcl
{
public:
    typedef Point point_type;
    typedef List list_type;
    typedef typename List::size_type size_type;
    typedef typename List::iterator iterator;
    typedef typename List::const_iterator const_iterator;

    ring_as_dcl()
        : m_list()
    {}

    inline void push_back(Point const& point)
    {
        m_list.push_back(point);
    }

    inline void split_at(iterator pos1, iterator pos2, ring_as_dcl& other)
    {
        // for this method to work, I think that the iterator pos1
        // must preceed iterator pos2 in the original list node sequence
        other.m_list.splice(other.m_list.end(), m_list, pos1, pos2);

        if (BOOST_GEOMETRY_CONDITION(Closure == closed))
        {
            other.m_list.push_back(*other.m_list.begin());
        }
    }

    inline iterator begin() { return m_list.begin(); }
    inline const_iterator begin() const { return m_list.begin(); }

    inline iterator end() { return m_list.end(); }
    inline const_iterator end() const { return m_list.end(); }

    inline void swap(ring_as_dcl& other)
    {
        m_list.swap(other.m_list);
    }
private:
    List m_list;
};


template <closure_selector Closure>
class find_duplicate_points
{
    static bool const is_closed = Closure == closed;

    struct point_iterator_less
    {
        template <typename PointIterator>
        inline bool operator()(PointIterator it1, PointIterator it2) const
        {
            return geometry::less
                <
                    typename std::iterator_traits<PointIterator>::value_type
                >()(*it1, *it2);
        }
    };

public:
    template <typename Ring, typename Iterator>
    static inline bool apply(Ring const& ring, Iterator& pos1, Iterator& pos2)
    {
        typedef typename Ring::iterator iterator_type;
        typedef std::set<iterator_type, point_iterator_less> point_set_type;
        point_set_type point_set;

        Ring& nc_ring = const_cast<Ring&>(ring);
        iterator_type last
            = is_closed ? --boost::end(nc_ring) : boost::end(nc_ring);
        for (iterator_type it = boost::begin(nc_ring); it != last; ++it)
        {
            std::pair<typename point_set_type::iterator, bool> res
                = point_set.insert(it);

            if (! res.second)
            {
                pos1 = *res.first;
                pos2 = it;
                return true;
            }
        }

        // initialize pos1 and pos2 with something
        pos1 = boost::begin(nc_ring);
        pos2 = pos1;
        return false;
    }
};


template <overlay_type OverlayType, typename Ring, typename RobustPolicy>
struct split_ring
{
    template <typename RingCollection, typename IntersectionStrategy>
    static inline void apply(Ring const& ring,
                             RingCollection& collection,
                             IntersectionStrategy const& intersection_strategy,
                             RobustPolicy const& robust_policy)
    {
        split_ring
            <
                overlay_union, Ring, RobustPolicy
            >::apply(ring, collection, intersection_strategy, robust_policy);
    }
};

// specialization for union
// TODO: add another specialization for intersection once implemented
template <typename Ring, typename RobustPolicy>
class split_ring<overlay_union, Ring, RobustPolicy>
{
    typedef turn_info
        <
            typename point_type<Ring>::type,
            typename geometry::segment_ratio_type
                <
                    typename point_type<Ring>::type, RobustPolicy
                >::type
        > turn_type;

    typedef std::deque<turn_type> turns_container_type;

    struct no_interrupt_policy
    {
        static bool const enabled = false;
        static bool const has_intersections = false;

        template <typename Range>
        static inline bool apply(Range const&)
        {
            return false;
        }
    };
    /*
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
    */
    template <typename MAA_Turn>
    struct maa_turn_less
    {
        bool operator()(MAA_Turn const& t1, MAA_Turn const& t2) const
        {
#if ! defined(BOOST_GEOMETRY_OVERLAY_NO_THROW)
            if (t1.method != method_touch_interior
                ||
                (! t1.both(operation_union)
                 && ! t1.both(operation_intersection))
                ||
                t2.method != method_touch_interior
                ||
                (! t2.both(operation_union)
                 && ! t2.both(operation_intersection))
                )
            {
                throw inconsistent_turns_exception();
            }
#else
            BOOST_GEOMETRY_ASSERT(t1.method == method_touch_interior);
            BOOST_GEOMETRY_ASSERT(t1.both(operation_union)
                                  ||
                                  t1.both(operation_intersection));
            BOOST_GEOMETRY_ASSERT(t2.method == method_touch_interior);
            BOOST_GEOMETRY_ASSERT(t2.both(operation_union)
                                  ||
                                  t2.both(operation_intersection));
#endif

            typename MAA_Turn::turn_operation_type op1 = get_correct_op(t1);
            typename MAA_Turn::turn_operation_type op2 = get_correct_op(t2);

            //BOOST_GEOMETRY_ASSERT(! op1.fraction.is_zero()
            //                      && ! op1.fraction.is_one());
            //BOOST_GEOMETRY_ASSERT(! op2.fraction.is_zero()
            //                      && ! op2.fraction.is_one());

            if (op1.seg_id.segment_index != op2.seg_id.segment_index)
            {
                return op1.seg_id.segment_index < op2.seg_id.segment_index;
            }
            return op1.fraction < op2.fraction;
        }
    };

    template <typename IntersectionStrategy, typename InterruptPolicy>
    static inline void get_self_turns(Ring const& ring,
                                      turns_container_type& turns,
                                      IntersectionStrategy const& intersection_strategy,
                                      RobustPolicy const& robust_policy,
                                      InterruptPolicy const& policy)
    {  
        geometry::self_turns
            <
                get_turn_info<assign_null_policy>
            >(ring, intersection_strategy, robust_policy, turns, policy);
    }

    template <typename IntersectionStrategy>
    static inline void get_self_turns(Ring const& ring,
                                      turns_container_type& turns,
                                      IntersectionStrategy const& intersection_strategy,
                                      RobustPolicy const& robust_policy)
    {
        get_self_turns(ring, turns, intersection_strategy, robust_policy, no_interrupt_policy());
    }

    template <typename MAA_Turns, typename RingOut>
    static inline void insert_maa_turns(Ring const& ring,
                                        MAA_Turns const& maa_turns,
                                        RingOut& ring_out)
    {
        typedef typename boost::range_size<Ring>::type size_type;
        typedef typename boost::range_iterator
            <
                MAA_Turns const
            >::type iterator_type;

        size_type point_index = 0;
        for (iterator_type it = maa_turns.begin(); it != maa_turns.end(); ++it)
        {
            signed_size_type segment_index
                = get_correct_op(*it).seg_id.segment_index;

            while (point_index <= static_cast<size_type>(segment_index))
            {
                ring_out.push_back(ring[point_index]);
                ++point_index;
            }

            ring_out.push_back(it->point);
        }

        while (point_index < ring.size())
        {
            ring_out.push_back(ring[point_index]);
            ++point_index; 
        }
    }

    template <typename RingIn, typename Collection>
    static inline void copy_to_collection(RingIn const& ring,
                                          Collection& collection)
    {
        typedef typename boost::range_value<Collection>::type ring_out_type;

        ring_out_type tmp;
        for (typename boost::range_iterator<RingIn const>::type
                 it = ring.begin();
             it != ring.end();
             ++it)
        {
            geometry::traits::push_back<ring_out_type>::apply(tmp, *it);
        }
        collection.push_back(tmp);
    }

    template <typename Stack>
    static inline void move_to_top(Stack& stack,
                                   typename Stack::value_type& value)
    {
        typedef typename Stack::value_type value_type;
        stack.push(value_type());
        stack.top().swap(value);
    }

    template <closure_selector Closure, typename RingType, typename Collection>
    static inline void split_one_ring(RingType& ring, Collection& collection)
    {
        // create and initialize a stack with the input ring
        std::stack<RingType> stack;
        move_to_top(stack, ring);

        // while the stack is not empty:
        // look for duplicates, split and push to stack;
        // otherwise, copy to output collection
        while (! stack.empty())
        {
            RingType& top_ring = stack.top();

            typename boost::range_iterator<RingType>::type pos1, pos2;
            bool duplicate_found = find_duplicate_points
                <
                    Closure
                >::apply(top_ring, pos1, pos2);

            if (duplicate_found)
            {
                RingType other_ring;
                top_ring.split_at(pos1, pos2, other_ring);
                move_to_top(stack, other_ring);
            }
            else
            {
                copy_to_collection(top_ring, collection);
                stack.pop();
            }
        }
    }

public:
    template <typename RingCollection, typename IntersectionStrategy>
    static inline void apply(Ring const& ring,
                             RingCollection& collection,
                             IntersectionStrategy const& intersection_strategy,
                             RobustPolicy const& robust_policy)
    {
        typedef std::set<turn_type, maa_turn_less<turn_type> > maa_turn_set;
        typedef ring_as_dcl
            <
                typename point_type<Ring>::type, closure<Ring>::value
            > ring_dcl_type;


        // compute the ring's self turns
        turns_container_type turns;
        get_self_turns(ring, turns, intersection_strategy, robust_policy);

        // collect the ring's m:u/u and m:i/i turns (the latter can
        // appear when we perform an intersection and the intersection
        // result consists of a multipolygon whose polygons touch each
        // other);
        // notice the use of std::set; we want to record coinciding
        // m:u/u and m:i/i turns only once
        maa_turn_set maa_turns;
        for (typename turns_container_type::const_iterator it = turns.begin();
             it != turns.end();
             ++it)
        {
            if (it->method == method_touch_interior)
            {
#if ! defined(BOOST_GEOMETRY_OVERLAY_NO_THROW)
                if (! it->both(operation_union)
                    &&
                    ! it->both(operation_intersection))
                {
                    throw inconsistent_turns_exception();
                }
#else
                BOOST_GEOMETRY_ASSERT(it->both(operation_union)
                                      ||
                                      it->both(operation_intersection));
#endif
                maa_turns.insert(*it);
            }
        }

        // insert the m:u/u turns as points in the original ring
        ring_dcl_type output;
        insert_maa_turns(ring, maa_turns, output);

        // split the ring into simple rings
        split_one_ring<closure<Ring>::value>(output, collection);
    }
};


template <overlay_type OverlayType>
struct split_rings
{
    template <typename RingCollection, typename IntersectionStrategy, typename RobustPolicy>
    static inline void apply(RingCollection& collection,
                             IntersectionStrategy const& intersection_strategy,
                             RobustPolicy const& robust_policy)
    {
        typedef typename boost::range_iterator
            <
                RingCollection
            >::type ring_iterator_type;

        RingCollection new_collection;
        for (ring_iterator_type rit = boost::begin(collection);
             rit != boost::end(collection);
             ++rit)
        {
            split_ring
                <
                    OverlayType,
                    typename boost::range_value<RingCollection>::type,
                    RobustPolicy
                >::apply(*rit, new_collection, intersection_strategy, robust_policy);
        }
        collection.swap(new_collection);
    }
};

// specialization for union
// TODO: add another specialization for intersection once implemented
template <>
struct split_rings<overlay_union>
{
    template <typename RingCollection, typename IntersectionStrategy, typename RobustPolicy>
    static inline void apply(RingCollection& collection,
                             IntersectionStrategy const& intersection_strategy,
                             RobustPolicy const& robust_policy)
    {
        typedef typename boost::range_iterator
            <
                RingCollection
            >::type ring_iterator_type;

        RingCollection new_collection;
        for (ring_iterator_type rit = boost::begin(collection);
             rit != boost::end(collection);
             ++rit)
        {
            split_ring
                <
                    overlay_union,
                    typename boost::range_value<RingCollection>::type,
                    RobustPolicy
                >::apply(*rit, new_collection, intersection_strategy, robust_policy);
        }
        collection.swap(new_collection);
    }
};


}} // namespace detail::overlay

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SPLIT_RINGS_HPP
