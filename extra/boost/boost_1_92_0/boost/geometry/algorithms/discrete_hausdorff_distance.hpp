// Boost.Geometry

// Copyright (c) 2018 Yaghyavardhan Singh Khangarot, Hyderabad, India.
// Contributed and/or modified by Yaghyavardhan Singh Khangarot,
//   as part of Google Summer of Code 2018 program.

// This file was modified by Oracle on 2018-2021.
// Modifications copyright (c) 2018-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DISCRETE_HAUSDORFF_DISTANCE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DISCRETE_HAUSDORFF_DISTANCE_HPP

#include <algorithm>

#ifdef BOOST_GEOMETRY_DEBUG_HAUSDORFF_DISTANCE
#include <iostream>
#endif

#include <iterator>
#include <utility>
#include <vector>
#include <limits>

#include <boost/range/size.hpp>

#include <boost/geometry/algorithms/detail/dummy_geometries.hpp>
#include <boost/geometry/algorithms/detail/throw_on_empty_input.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/strategies/detail.hpp>
#include <boost/geometry/strategies/discrete_distance/cartesian.hpp>
#include <boost/geometry/strategies/discrete_distance/geographic.hpp>
#include <boost/geometry/strategies/discrete_distance/spherical.hpp>
#include <boost/geometry/strategies/distance_result.hpp>
#include <boost/geometry/util/range.hpp>

// Note that in order for this to work umbrella strategy has to contain
// index strategies.
#ifdef BOOST_GEOMETRY_ENABLE_SIMILARITY_RTREE
#include <boost/geometry/index/rtree.hpp>
#endif // BOOST_GEOMETRY_ENABLE_SIMILARITY_RTREE

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace discrete_hausdorff_distance
{

// TODO: The implementation should calculate comparable distances

struct point_range
{
    template <typename Point, typename Range, typename Strategies>
    static inline auto apply(Point const& pnt, Range const& range,
                             Strategies const& strategies)
    {
        typedef typename distance_result
            <
                Point,
                point_type_t<Range>,
                Strategies
            >::type result_type;

        typedef typename boost::range_size<Range>::type size_type;

        boost::geometry::detail::throw_on_empty_input(range);

        auto const strategy = strategies.distance(dummy_point(), dummy_point());

        size_type const n = boost::size(range);
        result_type dis_min = 0;
        bool is_dis_min_set = false;

        for (size_type i = 0 ; i < n ; i++)
        {
            result_type dis_temp = strategy.apply(pnt, range::at(range, i));
            if (! is_dis_min_set || dis_temp < dis_min)
            {
                dis_min = dis_temp;
                is_dis_min_set = true;
            }
        }
        return dis_min;
    }
};

struct range_range
{
    template <typename Range1, typename Range2, typename Strategies>
    static inline auto apply(Range1 const& r1, Range2 const& r2,
                             Strategies const& strategies)
    {
        typedef typename distance_result
            <
                point_type_t<Range1>,
                point_type_t<Range2>,
                Strategies
            >::type result_type;

        typedef typename boost::range_size<Range1>::type size_type;

        boost::geometry::detail::throw_on_empty_input(r1);
        boost::geometry::detail::throw_on_empty_input(r2);

        size_type const n = boost::size(r1);
        result_type dis_max = 0;

#ifdef BOOST_GEOMETRY_ENABLE_SIMILARITY_RTREE
        namespace bgi = boost::geometry::index;
        using point_t = point_type_t<Range1>;
        using rtree_type = bgi::rtree<point_t, bgi::linear<4> >;
        rtree_type rtree(boost::begin(r2), boost::end(r2));
        point_t res;
#endif

        for (size_type i = 0 ; i < n ; i++)
        {
#ifdef BOOST_GEOMETRY_ENABLE_SIMILARITY_RTREE
            size_type found = rtree.query(bgi::nearest(range::at(r1, i), 1), &res);
            result_type dis_min = strategy.apply(range::at(r1,i), res);
#else
            result_type dis_min = point_range::apply(range::at(r1, i), r2, strategies);
#endif
            if (dis_min > dis_max )
            {
                dis_max = dis_min;
            }
        }
        return dis_max;
    }
};


struct range_multi_range
{
    template <typename Range, typename MultiRange, typename Strategies>
    static inline auto apply(Range const& range, MultiRange const& multi_range,
                             Strategies const& strategies)
    {
        typedef typename distance_result
            <
                point_type_t<Range>,
                point_type_t<MultiRange>,
                Strategies
            >::type result_type;
        typedef typename boost::range_size<MultiRange>::type size_type;

        boost::geometry::detail::throw_on_empty_input(range);
        boost::geometry::detail::throw_on_empty_input(multi_range);

        size_type b = boost::size(multi_range);
        result_type haus_dis = 0;

        for (size_type j = 0 ; j < b ; j++)
        {
            result_type dis_max = range_range::apply(range, range::at(multi_range, j), strategies);
            if (dis_max > haus_dis)
            {
                haus_dis = dis_max;
            }
        }

        return haus_dis;
    }
};


struct multi_range_multi_range
{
    template <typename MultiRange1, typename MultiRange2, typename Strategies>
    static inline auto apply(MultiRange1 const& multi_range1, MultiRange2 const& multi_range2,
                             Strategies const& strategies)
    {
        typedef typename distance_result
            <
                point_type_t<MultiRange1>,
                point_type_t<MultiRange2>,
                Strategies
            >::type result_type;
        typedef typename boost::range_size<MultiRange1>::type size_type;

        boost::geometry::detail::throw_on_empty_input(multi_range1);
        boost::geometry::detail::throw_on_empty_input(multi_range2);

        size_type n = boost::size(multi_range1);
        result_type haus_dis = 0;

        for (size_type i = 0 ; i < n ; i++)
        {
            result_type dis_max = range_multi_range::apply(range::at(multi_range1, i), multi_range2, strategies);
            if (dis_max > haus_dis)
            {
                haus_dis = dis_max;
            }
        }
        return haus_dis;
    }
};

}} // namespace detail::hausdorff_distance
#endif // DOXYGEN_NO_DETAIL

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{
template
<
    typename Geometry1,
    typename Geometry2,
    typename Tag1 = tag_t<Geometry1>,
    typename Tag2 = tag_t<Geometry2>
>
struct discrete_hausdorff_distance : not_implemented<Tag1, Tag2>
{};

// Specialization for point and multi_point
template <typename Point, typename MultiPoint>
struct discrete_hausdorff_distance<Point, MultiPoint, point_tag, multi_point_tag>
    : detail::discrete_hausdorff_distance::point_range
{};

// Specialization for linestrings
template <typename Linestring1, typename Linestring2>
struct discrete_hausdorff_distance<Linestring1, Linestring2, linestring_tag, linestring_tag>
    : detail::discrete_hausdorff_distance::range_range
{};

// Specialization for multi_point-multi_point
template <typename MultiPoint1, typename MultiPoint2>
struct discrete_hausdorff_distance<MultiPoint1, MultiPoint2, multi_point_tag, multi_point_tag>
    : detail::discrete_hausdorff_distance::range_range
{};

// Specialization for Linestring and MultiLinestring
template <typename Linestring, typename MultiLinestring>
struct discrete_hausdorff_distance<Linestring, MultiLinestring, linestring_tag, multi_linestring_tag>
    : detail::discrete_hausdorff_distance::range_multi_range
{};

// Specialization for MultiLinestring and MultiLinestring
template <typename MultiLinestring1, typename MultiLinestring2>
struct discrete_hausdorff_distance<MultiLinestring1, MultiLinestring2, multi_linestring_tag, multi_linestring_tag>
    : detail::discrete_hausdorff_distance::multi_range_multi_range
{};

} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_strategy {

template
<
    typename Strategies,
    bool IsUmbrella = strategies::detail::is_umbrella_strategy<Strategies>::value
>
struct discrete_hausdorff_distance
{
    template <typename Geometry1, typename Geometry2>
    static inline auto apply(Geometry1 const& geometry1, Geometry2 const& geometry2,
                             Strategies const& strategies)
    {
        return dispatch::discrete_hausdorff_distance
            <
                Geometry1, Geometry2
            >::apply(geometry1, geometry2, strategies);
    }
};

template <typename Strategy>
struct discrete_hausdorff_distance<Strategy, false>
{
    template <typename Geometry1, typename Geometry2>
    static inline auto apply(Geometry1 const& geometry1, Geometry2 const& geometry2,
                             Strategy const& strategy)
    {
        using strategies::discrete_distance::services::strategy_converter;
        return dispatch::discrete_hausdorff_distance
            <
                Geometry1, Geometry2
            >::apply(geometry1, geometry2,
                     strategy_converter<Strategy>::get(strategy));
    }
};

template <>
struct discrete_hausdorff_distance<default_strategy, false>
{
    template <typename Geometry1, typename Geometry2>
    static inline auto apply(Geometry1 const& geometry1, Geometry2 const& geometry2,
                             default_strategy const&)
    {
        typedef typename strategies::discrete_distance::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategies_type;

        return dispatch::discrete_hausdorff_distance
            <
                Geometry1, Geometry2
            >::apply(geometry1, geometry2, strategies_type());
    }
};

} // namespace resolve_strategy


/*!
\brief \brief_calc2{discrete Hausdorff distance, between} \brief_strategy
\details \details_free_function{discrete_hausdorff_distance, discrete Hausdorff distance, between}.
\ingroup discrete_hausdorff_distance
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\tparam Strategy \tparam_strategy{Distance}
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param strategy \param_strategy{point to point distance}

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/discrete_hausdorff_distance.qbk]}

\qbk{
[heading Available Strategies]
\* [link geometry.reference.strategies.strategy_distance_pythagoras Pythagoras (cartesian)]
\* [link geometry.reference.strategies.strategy_distance_haversine Haversine (spherical)]
\* One of the geographic point to point strategies

[heading Example]
[discrete_hausdorff_distance_strategy]
[discrete_hausdorff_distance_strategy_output]
}
*/
template <typename Geometry1, typename Geometry2, typename Strategy>
inline auto discrete_hausdorff_distance(Geometry1 const& geometry1,
                                        Geometry2 const& geometry2,
                                        Strategy const& strategy)
{
    return resolve_strategy::discrete_hausdorff_distance
        <
            Strategy
        >::apply(geometry1, geometry2, strategy);
}

/*!
\brief \brief_calc2{discrete Hausdorff distance, between}
\details \details_free_function{discrete_hausdorff_distance, discrete Hausdorff distance, between}.
\ingroup discrete_hausdorff_distance
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry
\param geometry2 \param_geometry

\qbk{[include reference/algorithms/discrete_hausdorff_distance.qbk]}

\qbk{
[heading Example]
[discrete_hausdorff_distance]
[discrete_hausdorff_distance_output]
}
*/
template <typename Geometry1, typename Geometry2>
inline auto discrete_hausdorff_distance(Geometry1 const& geometry1,
                                        Geometry2 const& geometry2)
{
    return resolve_strategy::discrete_hausdorff_distance
        <
            default_strategy
        >::apply(geometry1, geometry2, default_strategy());
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DISCRETE_HAUSDORFF_DISTANCE_HPP
