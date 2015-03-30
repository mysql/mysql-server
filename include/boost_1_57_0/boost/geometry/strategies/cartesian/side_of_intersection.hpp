// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_CARTESIAN_SIDE_OF_INTERSECTION_HPP
#define BOOST_GEOMETRY_STRATEGIES_CARTESIAN_SIDE_OF_INTERSECTION_HPP


#include <boost/geometry/arithmetic/determinant.hpp>
#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/util/math.hpp>


namespace boost { namespace geometry
{

namespace strategy { namespace side
{

// Calculates the side of the intersection-point (if any) of
// of segment a//b w.r.t. segment c
// This is calculated without (re)calculating the IP itself again and fully
// based on integer mathematics; there are no divisions
// It can be used for either integer (rescaled) points, and also for FP
class side_of_intersection
{
public :

    // Calculates the side of the intersection-point (if any) of
    // of segment a//b w.r.t. segment c
    // This is calculated without (re)calculating the IP itself again and fully
    // based on integer mathematics
    template <typename T, typename Segment>
    static inline T side_value(Segment const& a, Segment const& b,
                Segment const& c)
    {
        // The first point of the three segments is reused several times
        T const ax = get<0, 0>(a);
        T const ay = get<0, 1>(a);
        T const bx = get<0, 0>(b);
        T const by = get<0, 1>(b);
        T const cx = get<0, 0>(c);
        T const cy = get<0, 1>(c);

        T const dx_a = get<1, 0>(a) - ax;
        T const dy_a = get<1, 1>(a) - ay;

        T const dx_b = get<1, 0>(b) - bx;
        T const dy_b = get<1, 1>(b) - by;

        T const dx_c = get<1, 0>(c) - cx;
        T const dy_c = get<1, 1>(c) - cy;

        // Cramer's rule: d (see cart_intersect.hpp)
        T const d = geometry::detail::determinant<T>
                    (
                        dx_a, dy_a,
                        dx_b, dy_b
                    );

        T const zero = T();
        if (d == zero)
        {
            // There is no IP of a//b, they are collinear or parallel
            // We don't have to divide but we can already conclude the side-value
            // is meaningless and the resulting determinant will be 0
            return zero;
        }

        // Cramer's rule: da (see cart_intersect.hpp)
        T const da = geometry::detail::determinant<T>
                    (
                        dx_b,    dy_b,
                        ax - bx, ay - by
                    );

        // IP is at (ax + (da/d) * dx_a, ay + (da/d) * dy_a)
        // Side of IP is w.r.t. c is: determinant(dx_c, dy_c, ipx-cx, ipy-cy)
        // We replace ipx by expression above and multiply each term by d
        T const result = geometry::detail::determinant<T>
                    (
                        dx_c * d,                   dy_c * d,
                        d * (ax - cx) + dx_a * da,  d * (ay - cy) + dy_a * da
                    );

        // Note: result / (d * d)
        // is identical to the side_value of side_by_triangle
        // Therefore, the sign is always the same as that result, and the
        // resulting side (left,right,collinear) is the same

        return result;

    }

    template <typename Segment>
    static inline int apply(Segment const& a, Segment const& b, Segment const& c)
    {
        typedef typename geometry::coordinate_type<Segment>::type coordinate_type;
        coordinate_type const s = side_value<coordinate_type>(a, b, c);
        coordinate_type const zero = coordinate_type();
        return math::equals(s, zero) ? 0
            : s > zero ? 1
            : -1;
    }

};


}} // namespace strategy::side

}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_STRATEGIES_CARTESIAN_SIDE_OF_INTERSECTION_HPP
