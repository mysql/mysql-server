// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015.
// Modifications copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_CARTESIAN_SIDE_BY_TRIANGLE_HPP
#define BOOST_GEOMETRY_STRATEGIES_CARTESIAN_SIDE_BY_TRIANGLE_HPP

#include <boost/mpl/if.hpp>
#include <boost/type_traits.hpp>

#include <boost/geometry/arithmetic/determinant.hpp>
#include <boost/geometry/core/access.hpp>
#include <boost/geometry/util/select_coordinate_type.hpp>
#include <boost/geometry/strategies/side.hpp>

#include <boost/geometry/algorithms/detail/relate/less.hpp>
#include <boost/geometry/algorithms/detail/equals/point_point.hpp>


namespace boost { namespace geometry
{

namespace strategy { namespace side
{

/*!
\brief Check at which side of a segment a point lies:
    left of segment (> 0), right of segment (< 0), on segment (0)
\ingroup strategies
\tparam CalculationType \tparam_calculation
 */
template <typename CalculationType = void>
class side_by_triangle
{
public :

    // Template member function, because it is not always trivial
    // or convenient to explicitly mention the typenames in the
    // strategy-struct itself.

    // Types can be all three different. Therefore it is
    // not implemented (anymore) as "segment"

    template <typename coordinate_type, typename promoted_type, typename P1, typename P2, typename P>
    static inline promoted_type side_value(P1 const& p1, P2 const& p2, P const& p)
    {
        coordinate_type const x = get<0>(p);
        coordinate_type const y = get<1>(p);

        coordinate_type const sx1 = get<0>(p1);
        coordinate_type const sy1 = get<1>(p1);
        coordinate_type const sx2 = get<0>(p2);
        coordinate_type const sy2 = get<1>(p2);

        promoted_type const dx = sx2 - sx1;
        promoted_type const dy = sy2 - sy1;
        promoted_type const dpx = x - sx1;
        promoted_type const dpy = y - sy1;

        return geometry::detail::determinant<promoted_type>
                (
                    dx, dy,
                    dpx, dpy
                );

    }

    template <typename P1, typename P2, typename P>
    static inline int apply(P1 const& p1, P2 const& p2, P const& p)
    {
        typedef typename boost::mpl::if_c
            <
                boost::is_void<CalculationType>::type::value,
                typename select_most_precise
                    <
                        typename select_most_precise
                            <
                                typename coordinate_type<P1>::type,
                                typename coordinate_type<P2>::type
                            >::type,
                        typename coordinate_type<P>::type
                    >::type,
                CalculationType
            >::type coordinate_type;

        // Promote float->double, small int->int
        typedef typename select_most_precise
            <
                coordinate_type,
                double
            >::type promoted_type;

        if (detail::equals::equals_point_point(p1, p2)
            || detail::equals::equals_point_point(p1, p)
            || detail::equals::equals_point_point(p2, p))
        {
            return 0;
        }

        geometry::detail::relate::less less;

        promoted_type s;
        if (less(p2, p1) && less(p2, p))
        {
            s = side_value<coordinate_type, promoted_type>(p2, p, p1);
        }
        else if (less(p, p1) && less(p, p2))
        {
            s = side_value<coordinate_type, promoted_type>(p, p1, p2);
        }
        else
        {
            s = side_value<coordinate_type, promoted_type>(p1, p2, p);
        }

        promoted_type const zero = promoted_type();
        return math::equals(s, zero) ? 0
            : s > zero ? 1
            : -1;
    }

};


#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS
namespace services
{

template <typename CalculationType>
struct default_strategy<cartesian_tag, CalculationType>
{
    typedef side_by_triangle<CalculationType> type;
};

}
#endif

}} // namespace strategy::side

}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_STRATEGIES_CARTESIAN_SIDE_BY_TRIANGLE_HPP
