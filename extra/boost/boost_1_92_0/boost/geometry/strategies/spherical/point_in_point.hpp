// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland

// This file was modified by Oracle on 2013-2025.
// Modifications copyright (c) 2013-2025, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGY_SPHERICAL_POINT_IN_POINT_HPP
#define BOOST_GEOMETRY_STRATEGY_SPHERICAL_POINT_IN_POINT_HPP


#include <cstddef>
#include <type_traits>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/radian_access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/coordinate_promotion.hpp>
#include <boost/geometry/core/coordinate_system.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/algorithms/detail/normalize.hpp>
#include <boost/geometry/algorithms/dispatch/disjoint.hpp>
#include <boost/geometry/algorithms/transform.hpp>

#include <boost/geometry/geometries/helper_geometry.hpp>

#include <boost/geometry/strategies/cartesian/point_in_point.hpp>
#include <boost/geometry/strategies/covered_by.hpp>
#include <boost/geometry/strategies/strategy_transform.hpp>
#include <boost/geometry/strategies/within.hpp>

#include <boost/geometry/util/math.hpp>
#include <boost/geometry/util/select_most_precise.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace within
{

class point_point_on_spheroid
{
public:
    using cs_tag = spherical_tag;

private:
    template <typename Point1, typename Point2, bool SameUnits>
    struct are_same_points
    {
        static inline bool apply(Point1 const& point1, Point2 const& point2)
        {
            using helper_point_type1 = typename helper_geometry<Point1>::type;
            using helper_point_type2 = typename helper_geometry<Point2>::type;

            helper_point_type1 point1_normalized;
            bool const exact_normalized = false;
            strategy::normalize::spherical_point::apply(point1, point1_normalized, exact_normalized);
            helper_point_type2 point2_normalized;
            strategy::normalize::spherical_point::apply(point2, point2_normalized, exact_normalized);

            return point_point_generic
                <
                    0, dimension<Point1>::value
                >::apply(point1_normalized, point2_normalized);
        }
    };

    template <typename Point1, typename Point2>
    struct are_same_points<Point1, Point2, false> // points have different units
    {
        static inline bool apply(Point1 const& point1, Point2 const& point2)
        {
            using calculation_type = typename geometry::select_most_precise
                <
                    typename fp_coordinate_type<Point1>::type,
                    typename fp_coordinate_type<Point2>::type
                >::type;

            typename helper_geometry
                <
                    Point1, calculation_type, radian
                >::type helper_point1, helper_point2;

            Point1 point1_normalized;
            strategy::normalize::spherical_point::apply(point1, point1_normalized);
            Point2 point2_normalized;
            strategy::normalize::spherical_point::apply(point2, point2_normalized);

            geometry::transform(point1_normalized, helper_point1);
            geometry::transform(point2_normalized, helper_point2);

            return point_point_generic
                <
                    0, dimension<Point1>::value
                >::apply(helper_point1, helper_point2);
        }
    };

public:
    template <typename Point1, typename Point2>
    static inline bool apply(Point1 const& point1, Point2 const& point2)
    {
        return are_same_points
            <
                Point1,
                Point2,
                std::is_same
                    <
                        typename detail::cs_angular_units<Point1>::type,
                        typename detail::cs_angular_units<Point2>::type
                    >::value
            >::apply(point1, point2);
    }
};

}} // namespace detail::within
#endif // DOXYGEN_NO_DETAIL


namespace strategy { namespace within
{

struct spherical_point_point
    : geometry::detail::within::point_point_on_spheroid
{};


#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS
namespace services
{

template <typename PointLike1, typename PointLike2, typename Tag1, typename Tag2>
struct default_strategy<PointLike1, PointLike2, Tag1, Tag2, pointlike_tag, pointlike_tag, spherical_tag, spherical_tag>
{
    using type = strategy::within::spherical_point_point;
};

} // namespace services
#endif


}} // namespace strategy::within


#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS
namespace strategy { namespace covered_by { namespace services
{

template <typename PointLike1, typename PointLike2, typename Tag1, typename Tag2>
struct default_strategy<PointLike1, PointLike2, Tag1, Tag2, pointlike_tag, pointlike_tag, spherical_tag, spherical_tag>
{
    using type = strategy::within::spherical_point_point;
};

}}} // namespace strategy::covered_by::services
#endif


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_STRATEGY_SPHERICAL_POINT_IN_POINT_HPP
