// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2024.
// Modifications copyright (c) 2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_CLUSTERS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_CLUSTERS_HPP

#include <algorithm>
#include <map>
#include <vector>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/algorithms/detail/overlay/approximately_equals.hpp>
#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_ring.hpp>
#include <boost/range/value_type.hpp>
#include <boost/geometry/util/math.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <bool Integral = false>
struct sweep_equal_policy
{

public:
    // Returns true if point are considered equal
    template <typename P>
    static inline bool equals(P const& p1, P const& p2)
    {
        using coor_t = coordinate_type_t<P>;
        static auto const tolerance
            = common_approximately_equals_epsilon_multiplier<coor_t>::value();
        return approximately_equals(p1, p2, tolerance);
    }

    template <typename T>
    static inline bool exceeds(T value)
    {
        static auto const tolerance
            = common_approximately_equals_epsilon_multiplier<T>::value();
        T const limit = T(1) / tolerance;
        return value > limit;
    }
};

template <>
struct sweep_equal_policy<true>
{
    template <typename P>
    static inline bool equals(P const& p1, P const& p2)
    {
        return geometry::get<0>(p1) == geometry::get<0>(p2)
            && geometry::get<1>(p1) == geometry::get<1>(p2);
    }

    template <typename T>
    static inline bool exceeds(T value)
    {
        return value > 0;
    }
};

template <typename Point>
struct turn_with_point
{
    std::size_t turn_index;
    Point pnt;
};

template <typename Cluster, typename Point>
struct cluster_with_point
{
    Cluster cluster;
    Point pnt;
};

// Use a sweep algorithm to detect clusters
template
<
    typename Turns,
    typename Clusters
>
inline void get_clusters(Turns& turns, Clusters& clusters)
{
    using turn_type = typename boost::range_value<Turns>::type;
    using cluster_type = typename Clusters::mapped_type;
    using point_type = typename turn_type::point_type;

    sweep_equal_policy
        <
            std::is_integral<coordinate_type_t<point_type>>::value
        > equal_policy;

    std::vector<turn_with_point<point_type>> points;
    std::size_t turn_index = 0;
    for (auto const& turn : turns)
    {
        if (! turn.discarded)
        {
            points.push_back({turn_index, turn.point});
        }
        turn_index++;
    }

    // Sort the points from top to bottom
    std::sort(points.begin(), points.end(), [](auto const& e1, auto const& e2)
    {
       return geometry::get<1>(e1.pnt) > geometry::get<1>(e2.pnt);
    });

    // The output vector will be sorted from bottom too
    std::vector<cluster_with_point<cluster_type, point_type>> clustered_points;

    // Compare points with each other. Performance is O(n log(n)) because of the sorting.
    for (auto it1 = points.begin(); it1 != points.end(); ++it1)
    {
        // Inner loop, iterates until it exceeds coordinates in y-direction
        for (auto it2 = it1 + 1; it2 != points.end(); ++it2)
        {
            auto const d = geometry::get<1>(it1->pnt) - geometry::get<1>(it2->pnt);
            if (equal_policy.exceeds(d))
            {
                // Points at this y-coordinate or below cannot be equal
                break;
            }
            if (equal_policy.equals(it1->pnt, it2->pnt))
            {
                std::size_t cindex = 0;

                // Most recent clusters (with this y-value) are at the bottom
                // therefore we can stop as soon as the y-value is out of reach (TODO)
                bool found = false;
                for (auto cit = clustered_points.begin();
                     cit != clustered_points.end(); ++cit)
                {
                    found = equal_policy.equals(cit->pnt, it1->pnt);
                    if (found)
                    {
                        break;
                    }
                    cindex++;
                }

                // Add new cluster
                if (! found)
                {
                   cindex = clustered_points.size();
                   cluster_type newcluster;
                   clustered_points.push_back({newcluster, it1->pnt});
                }
                clustered_points[cindex].cluster.turn_indices.insert(it1->turn_index);
                clustered_points[cindex].cluster.turn_indices.insert(it2->turn_index);
            }
        }
    }

    // Convert to map
    signed_size_type cluster_id = 1;
    for (auto& trace : clustered_points)
    {
        clusters[cluster_id++] = trace.cluster;
    }
}

}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_CLUSTERS_HPP
