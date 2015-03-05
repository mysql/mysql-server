// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2011-2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_PARTITION_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_PARTITION_HPP

#include <vector>
#include <boost/range/algorithm/copy.hpp>
#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/core/coordinate_type.hpp>

namespace boost { namespace geometry
{

namespace detail { namespace partition
{

typedef std::vector<std::size_t> index_vector_type;

template <int Dimension, typename Box>
inline void divide_box(Box const& box, Box& lower_box, Box& upper_box)
{
    typedef typename coordinate_type<Box>::type ctype;

    // Divide input box into two parts, e.g. left/right
    ctype two = 2;
    ctype mid = (geometry::get<min_corner, Dimension>(box)
            + geometry::get<max_corner, Dimension>(box)) / two;

    lower_box = box;
    upper_box = box;
    geometry::set<max_corner, Dimension>(lower_box, mid);
    geometry::set<min_corner, Dimension>(upper_box, mid);
}

// Divide collection into three subsets: lower, upper and oversized
// (not-fitting)
// (lower == left or bottom, upper == right or top)
template <typename OverlapsPolicy, typename InputCollection, typename Box>
inline void divide_into_subsets(Box const& lower_box,
        Box const& upper_box,
        InputCollection const& collection,
        index_vector_type const& input,
        index_vector_type& lower,
        index_vector_type& upper,
        index_vector_type& exceeding)
{
    typedef boost::range_iterator
        <
            index_vector_type const
        >::type index_iterator_type;

    for(index_iterator_type it = boost::begin(input);
        it != boost::end(input);
        ++it)
    {
        bool const lower_overlapping = OverlapsPolicy::apply(lower_box,
                    collection[*it]);
        bool const upper_overlapping = OverlapsPolicy::apply(upper_box,
                    collection[*it]);

        if (lower_overlapping && upper_overlapping)
        {
            exceeding.push_back(*it);
        }
        else if (lower_overlapping)
        {
            lower.push_back(*it);
        }
        else if (upper_overlapping)
        {
            upper.push_back(*it);
        }
        else
        {
            // Is nowhere. That is (since 1.58) possible, it might be
            // skipped by the OverlapsPolicy to enhance performance
        }
    }
}

template <typename ExpandPolicy, typename Box, typename InputCollection>
inline void expand_with_elements(Box& total,
                InputCollection const& collection,
                index_vector_type const& input)
{
    typedef boost::range_iterator<index_vector_type const>::type it_type;
    for(it_type it = boost::begin(input); it != boost::end(input); ++it)
    {
        ExpandPolicy::apply(total, collection[*it]);
    }
}


// Match collection with itself
template <typename InputCollection, typename Policy>
inline void handle_one(InputCollection const& collection,
        index_vector_type const& input,
        Policy& policy)
{
    if (boost::size(input) == 0)
    {
        return;
    }

    typedef boost::range_iterator<index_vector_type const>::type
                index_iterator_type;

    // Quadratic behaviour at lowest level (lowest quad, or all exceeding)
    for(index_iterator_type it1 = boost::begin(input);
        it1 != boost::end(input);
        ++it1)
    {
        index_iterator_type it2 = it1;
        for(++it2; it2 != boost::end(input); ++it2)
        {
            policy.apply(collection[*it1], collection[*it2]);
        }
    }
}

// Match collection 1 with collection 2
template
<
    typename InputCollection1,
    typename InputCollection2,
    typename Policy
>
inline void handle_two(
        InputCollection1 const& collection1, index_vector_type const& input1,
        InputCollection2 const& collection2, index_vector_type const& input2,
        Policy& policy)
{
    if (boost::size(input1) == 0 || boost::size(input2) == 0)
    {
        return;
    }

    typedef boost::range_iterator
        <
            index_vector_type const
        >::type index_iterator_type;

    for(index_iterator_type it1 = boost::begin(input1);
        it1 != boost::end(input1);
        ++it1)
    {
        for(index_iterator_type it2 = boost::begin(input2);
            it2 != boost::end(input2);
            ++it2)
        {
            policy.apply(collection1[*it1], collection2[*it2]);
        }
    }
}

inline bool recurse_ok(index_vector_type const& input,
                std::size_t min_elements, std::size_t level)
{
    return boost::size(input) >= min_elements
        && level < 100;
}

inline bool recurse_ok(index_vector_type const& input1,
                index_vector_type const& input2,
                std::size_t min_elements, std::size_t level)
{
    return boost::size(input1) >= min_elements
        && recurse_ok(input2, min_elements, level);
}

inline bool recurse_ok(index_vector_type const& input1,
                index_vector_type const& input2,
                index_vector_type const& input3,
                std::size_t min_elements, std::size_t level)
{
    return boost::size(input1) >= min_elements
        && recurse_ok(input2, input3, min_elements, level);
}

template
<
    int Dimension,
    typename Box,
    typename OverlapsPolicy1,
    typename OverlapsPolicy2,
    typename ExpandPolicy1,
    typename ExpandPolicy2,
    typename VisitBoxPolicy
>
class partition_two_collections;


template
<
    int Dimension,
    typename Box,
    typename OverlapsPolicy,
    typename ExpandPolicy,
    typename VisitBoxPolicy
>
class partition_one_collection
{
    typedef std::vector<std::size_t> index_vector_type;

    template <typename InputCollection>
    static inline Box get_new_box(InputCollection const& collection,
                    index_vector_type const& input)
    {
        Box box;
        geometry::assign_inverse(box);
        expand_with_elements<ExpandPolicy>(box, collection, input);
        return box;
    }

    template <typename InputCollection, typename Policy>
    static inline void next_level(Box const& box,
            InputCollection const& collection,
            index_vector_type const& input,
            std::size_t level, std::size_t min_elements,
            Policy& policy, VisitBoxPolicy& box_policy)
    {
        if (recurse_ok(input, min_elements, level))
        {
            partition_one_collection
            <
                1 - Dimension,
                Box,
                OverlapsPolicy,
                ExpandPolicy,
                VisitBoxPolicy
            >::apply(box, collection, input,
                level + 1, min_elements, policy, box_policy);
        }
        else
        {
            handle_one(collection, input, policy);
        }
    }

    // Function to switch to two collections if there are geometries exceeding
    // the separation line
    template <typename InputCollection, typename Policy>
    static inline void next_level2(Box const& box,
            InputCollection const& collection,
            index_vector_type const& input1,
            index_vector_type const& input2,
            std::size_t level, std::size_t min_elements,
            Policy& policy, VisitBoxPolicy& box_policy)
    {

        if (recurse_ok(input1, input2, min_elements, level))
        {
            partition_two_collections
            <
                1 - Dimension,
                Box,
                OverlapsPolicy, OverlapsPolicy,
                ExpandPolicy, ExpandPolicy,
                VisitBoxPolicy
            >::apply(box, collection, input1, collection, input2,
                level + 1, min_elements, policy, box_policy);
        }
        else
        {
            handle_two(collection, input1, collection, input2, policy);
        }
    }

public :
    template <typename InputCollection, typename Policy>
    static inline void apply(Box const& box,
            InputCollection const& collection,
            index_vector_type const& input,
            std::size_t level,
            std::size_t min_elements,
            Policy& policy, VisitBoxPolicy& box_policy)
    {
        box_policy.apply(box, level);

        Box lower_box, upper_box;
        divide_box<Dimension>(box, lower_box, upper_box);

        index_vector_type lower, upper, exceeding;
        divide_into_subsets<OverlapsPolicy>(lower_box, upper_box, collection,
                    input, lower, upper, exceeding);

        if (boost::size(exceeding) > 0)
        {
            // Get the box of exceeding-only
            Box exceeding_box = get_new_box(collection, exceeding);

            // Recursively do exceeding elements only, in next dimension they
            // will probably be less exceeding within the new box
            next_level(exceeding_box, collection, exceeding, level,
                min_elements, policy, box_policy);

            // Switch to two collections, combine exceeding with lower resp upper
            // but not lower/lower, upper/upper
            next_level2(exceeding_box, collection, exceeding, lower, level,
                min_elements, policy, box_policy);
            next_level2(exceeding_box, collection, exceeding, upper, level,
                min_elements, policy, box_policy);
        }

        // Recursively call operation both parts
        next_level(lower_box, collection, lower, level, min_elements,
                        policy, box_policy);
        next_level(upper_box, collection, upper, level, min_elements,
                        policy, box_policy);
    }
};

template
<
    int Dimension,
    typename Box,
    typename OverlapsPolicy1,
    typename OverlapsPolicy2,
    typename ExpandPolicy1,
    typename ExpandPolicy2,
    typename VisitBoxPolicy
>
class partition_two_collections
{
    typedef std::vector<std::size_t> index_vector_type;

    template
    <
        typename InputCollection1,
        typename InputCollection2,
        typename Policy
    >
    static inline void next_level(Box const& box,
            InputCollection1 const& collection1,
            index_vector_type const& input1,
            InputCollection2 const& collection2,
            index_vector_type const& input2,
            std::size_t level, std::size_t min_elements,
            Policy& policy, VisitBoxPolicy& box_policy)
    {
        partition_two_collections
        <
            1 - Dimension,
            Box,
            OverlapsPolicy1,
            OverlapsPolicy2,
            ExpandPolicy1,
            ExpandPolicy2,
            VisitBoxPolicy
        >::apply(box, collection1, input1, collection2, input2,
                level + 1, min_elements,
                policy, box_policy);
    }

    template
    <
        typename ExpandPolicy,
        typename InputCollection
    >
    static inline Box get_new_box(InputCollection const& collection,
                    index_vector_type const& input)
    {
        Box box;
        geometry::assign_inverse(box);
        expand_with_elements<ExpandPolicy>(box, collection, input);
        return box;
    }

    template
    <
        typename InputCollection1,
        typename InputCollection2
    >
    static inline Box get_new_box(InputCollection1 const& collection1,
                    index_vector_type const& input1,
                    InputCollection2 const& collection2,
                    index_vector_type const& input2)
    {
        Box box = get_new_box<ExpandPolicy1>(collection1, input1);
        expand_with_elements<ExpandPolicy2>(box, collection2, input2);
        return box;
    }

public :
    template
    <
        typename InputCollection1,
        typename InputCollection2,
        typename Policy
    >
    static inline void apply(Box const& box,
            InputCollection1 const& collection1, index_vector_type const& input1,
            InputCollection2 const& collection2, index_vector_type const& input2,
            std::size_t level,
            std::size_t min_elements,
            Policy& policy, VisitBoxPolicy& box_policy)
    {
        box_policy.apply(box, level);

        Box lower_box, upper_box;
        divide_box<Dimension>(box, lower_box, upper_box);

        index_vector_type lower1, upper1, exceeding1;
        index_vector_type lower2, upper2, exceeding2;
        divide_into_subsets<OverlapsPolicy1>(lower_box, upper_box, collection1,
                    input1, lower1, upper1, exceeding1);
        divide_into_subsets<OverlapsPolicy2>(lower_box, upper_box, collection2,
                    input2, lower2, upper2, exceeding2);

        if (boost::size(exceeding1) > 0)
        {
            // All exceeding from 1 with 2:

            if (recurse_ok(exceeding1, exceeding2, min_elements, level))
            {
                Box exceeding_box = get_new_box(collection1, exceeding1,
                            collection2, exceeding2);
                next_level(exceeding_box, collection1, exceeding1,
                                collection2, exceeding2, level,
                                min_elements, policy, box_policy);
            }
            else
            {
                handle_two(collection1, exceeding1, collection2, exceeding2,
                            policy);
            }

            // All exceeding from 1 with lower and upper of 2:

            // (Check sizes of all three collections to avoid recurse into
            // the same combinations again and again)
            if (recurse_ok(lower2, upper2, exceeding1, min_elements, level))
            {
                Box exceeding_box
                    = get_new_box<ExpandPolicy1>(collection1, exceeding1);
                next_level(exceeding_box, collection1, exceeding1,
                    collection2, lower2, level, min_elements, policy, box_policy);
                next_level(exceeding_box, collection1, exceeding1,
                    collection2, upper2, level, min_elements, policy, box_policy);
            }
            else
            {
                handle_two(collection1, exceeding1, collection2, lower2, policy);
                handle_two(collection1, exceeding1, collection2, upper2, policy);
            }
        }

        if (boost::size(exceeding2) > 0)
        {
            // All exceeding from 2 with lower and upper of 1:
            if (recurse_ok(lower1, upper1, exceeding2, min_elements, level))
            {
                Box exceeding_box
                    = get_new_box<ExpandPolicy2>(collection2, exceeding2);
                next_level(exceeding_box, collection1, lower1,
                    collection2, exceeding2, level, min_elements, policy, box_policy);
                next_level(exceeding_box, collection1, upper1,
                    collection2, exceeding2, level, min_elements, policy, box_policy);
            }
            else
            {
                handle_two(collection1, lower1, collection2, exceeding2, policy);
                handle_two(collection1, upper1, collection2, exceeding2, policy);
            }
        }

        if (recurse_ok(lower1, lower2, min_elements, level))
        {
            next_level(lower_box, collection1, lower1, collection2, lower2, level,
                            min_elements, policy, box_policy);
        }
        else
        {
            handle_two(collection1, lower1, collection2, lower2, policy);
        }
        if (recurse_ok(upper1, upper2, min_elements, level))
        {
            next_level(upper_box, collection1, upper1, collection2, upper2, level,
                            min_elements, policy, box_policy);
        }
        else
        {
            handle_two(collection1, upper1, collection2, upper2, policy);
        }
    }
};

struct visit_no_policy
{
    template <typename Box>
    static inline void apply(Box const&, std::size_t )
    {}
};

struct include_all_policy
{
    template <typename Item>
    static inline bool apply(Item const&)
    {
        return true;
    }
};


}} // namespace detail::partition

template
<
    typename Box,
    typename ExpandPolicy1,
    typename OverlapsPolicy1,
    typename ExpandPolicy2 = ExpandPolicy1,
    typename OverlapsPolicy2 = OverlapsPolicy1,
    typename IncludePolicy1 = detail::partition::include_all_policy,
    typename IncludePolicy2 = detail::partition::include_all_policy,
    typename VisitBoxPolicy = detail::partition::visit_no_policy
>
class partition
{
    typedef std::vector<std::size_t> index_vector_type;

    template <typename ExpandPolicy, typename IncludePolicy, typename InputCollection>
    static inline void expand_to_collection(InputCollection const& collection,
                Box& total, index_vector_type& index_vector)
    {
        std::size_t index = 0;
        for(typename boost::range_iterator<InputCollection const>::type it
            = boost::begin(collection);
            it != boost::end(collection);
            ++it, ++index)
        {
            if (IncludePolicy::apply(*it))
            {
                ExpandPolicy::apply(total, *it);
                index_vector.push_back(index);
            }
        }
    }

public :
    template <typename InputCollection, typename VisitPolicy>
    static inline void apply(InputCollection const& collection,
            VisitPolicy& visitor,
            std::size_t min_elements = 16,
            VisitBoxPolicy box_visitor = detail::partition::visit_no_policy()
            )
    {
        if (std::size_t(boost::size(collection)) > min_elements)
        {
            index_vector_type index_vector;
            Box total;
            assign_inverse(total);
            expand_to_collection<ExpandPolicy1, IncludePolicy1>(collection,
                    total, index_vector);

            detail::partition::partition_one_collection
                <
                    0, Box,
                    OverlapsPolicy1,
                    ExpandPolicy1,
                    VisitBoxPolicy
                >::apply(total, collection, index_vector, 0, min_elements,
                                visitor, box_visitor);
        }
        else
        {
            typedef typename boost::range_iterator
                <
                    InputCollection const
                >::type iterator_type;
            for(iterator_type it1 = boost::begin(collection);
                it1 != boost::end(collection);
                ++it1)
            {
                iterator_type it2 = it1;
                for(++it2; it2 != boost::end(collection); ++it2)
                {
                    visitor.apply(*it1, *it2);
                }
            }
        }
    }

    template
    <
        typename InputCollection1,
        typename InputCollection2,
        typename VisitPolicy
    >
    static inline void apply(InputCollection1 const& collection1,
                InputCollection2 const& collection2,
                VisitPolicy& visitor,
                std::size_t min_elements = 16,
                VisitBoxPolicy box_visitor = detail::partition::visit_no_policy()
                )
    {
        if (std::size_t(boost::size(collection1)) > min_elements
            && std::size_t(boost::size(collection2)) > min_elements)
        {
            index_vector_type index_vector1, index_vector2;
            Box total;
            assign_inverse(total);
            expand_to_collection<ExpandPolicy1, IncludePolicy1>(collection1,
                    total, index_vector1);
            expand_to_collection<ExpandPolicy2, IncludePolicy2>(collection2,
                    total, index_vector2);

            detail::partition::partition_two_collections
                <
                    0, Box, OverlapsPolicy1, OverlapsPolicy2,
                    ExpandPolicy1, ExpandPolicy2, VisitBoxPolicy
                >::apply(total,
                    collection1, index_vector1,
                    collection2, index_vector2,
                    0, min_elements, visitor, box_visitor);
        }
        else
        {
            typedef typename boost::range_iterator
                <
                    InputCollection1 const
                >::type iterator_type1;
            typedef typename boost::range_iterator
                <
                    InputCollection2 const
                >::type iterator_type2;
            for(iterator_type1 it1 = boost::begin(collection1);
                it1 != boost::end(collection1);
                ++it1)
            {
                for(iterator_type2 it2 = boost::begin(collection2);
                    it2 != boost::end(collection2);
                    ++it2)
                {
                    visitor.apply(*it1, *it2);
                }
            }
        }
    }
};


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_PARTITION_HPP
