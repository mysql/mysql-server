// Boost.Geometry

// Copyright (c) 2025 Tinko Bartels, Berlin, Germany.
// Copyright (c) 2025 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// This file was modified by Oracle on 2025.
// Modifications copyright (c) 2025 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGY_CARTESIAN_SIDE_3D_ROUNDED_INPUT_HPP
#define BOOST_GEOMETRY_STRATEGY_CARTESIAN_SIDE_3D_ROUNDED_INPUT_HPP

#include <limits>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/config.hpp>

#include <boost/geometry/util/math.hpp>

#include <boost/geometry/strategies/side.hpp>

#include <boost/geometry/util/select_calculation_type.hpp>

namespace boost { namespace geometry
{

namespace strategy { namespace side
{

// This strategy is used for 3D side calculations.
// It is used to determine the side of a point p with respect to a triangle/plane defined by three
// points in 3D space namely p1, p2, p3.
// The following determinant is used to calculate the side:
//       | (p_x - p1_x)  (p_y - p1_y)  (p_z - p1_z)  |
// det = | (p2_x - p1_x) (p2_y - p1_y) (p2_z - p1_z) |
//       | (p3_x - p1_x) (p3_y - p1_y) (p3_z - p1_z) |
// To calculate we use cofactor expansion along the first row.
// Performance note:
// If this function is called repeatedly with the same p1, p2, and p3,
// the 2x2 cofactors can be precomputed and reused for efficiency.

template <typename CalculationType = void, int Coeff1 = 12>
struct side_3d_rounded_input
{
    using cs_tag = cartesian_tag;

    template <typename P1, typename P2, typename P3, typename P>
    static inline int apply(P1 const& p1, P2 const& p2, P3 const& p3, P const& p)
    {
        using coor_t = typename select_calculation_type_alt<CalculationType, P1, P2, P3, P>::type;

        coor_t const p1_x = geometry::get<0>(p1);
        coor_t const p1_y = geometry::get<1>(p1);
        coor_t const p1_z = geometry::get<2>(p1);
        coor_t const p2_x = geometry::get<0>(p2);
        coor_t const p2_y = geometry::get<1>(p2);
        coor_t const p2_z = geometry::get<2>(p2);
        coor_t const p3_x = geometry::get<0>(p3);
        coor_t const p3_y = geometry::get<1>(p3);
        coor_t const p3_z = geometry::get<2>(p3);
        coor_t const p_x = geometry::get<0>(p);
        coor_t const p_y = geometry::get<1>(p);
        coor_t const p_z = geometry::get<2>(p);

        static coor_t const eps = std::numeric_limits<coor_t>::epsilon() / 2;
        coor_t const det = (p_x - p1_x) * ((p2_y - p1_y) * (p3_z - p1_z) - (p3_y - p1_y) * (p2_z - p1_z))
                         - (p_y - p1_y) * ((p2_x - p1_x) * (p3_z - p1_z) - (p3_x - p1_x) * (p2_z - p1_z))
                         + (p_z - p1_z) * ((p2_x - p1_x) * (p3_y - p1_y) - (p3_x - p1_x) * (p2_y - p1_y));
        coor_t const err_bound = (Coeff1 * eps) *
            (  (geometry::math::abs(p_x) + geometry::math::abs(p1_x))
            * ((geometry::math::abs(p2_y) + geometry::math::abs(p1_y))
             * (geometry::math::abs(p3_z) + geometry::math::abs(p1_z))
             + (geometry::math::abs(p3_y) + geometry::math::abs(p1_y))
             * (geometry::math::abs(p2_z) + geometry::math::abs(p1_z)))
           +   (geometry::math::abs(p_y) + geometry::math::abs(p1_y))
            * ((geometry::math::abs(p2_x) + geometry::math::abs(p1_x))
             * (geometry::math::abs(p3_z) + geometry::math::abs(p1_z))
             + (geometry::math::abs(p3_x) + geometry::math::abs(p1_x))
             * (geometry::math::abs(p2_z) + geometry::math::abs(p1_z)))
           +   (geometry::math::abs(p_z) + geometry::math::abs(p1_z))
            * ((geometry::math::abs(p2_x) + geometry::math::abs(p1_x))
             * (geometry::math::abs(p3_y) + geometry::math::abs(p1_y))
             + (geometry::math::abs(p3_x) + geometry::math::abs(p1_x))
             * (geometry::math::abs(p2_y) + geometry::math::abs(p1_y))));
        return (det > err_bound) - (det < -err_bound);
    }
};

}} // namespace strategy::side

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_STRATEGY_CARTESIAN_SIDE_3D_ROUNDED_INPUT_HPP
