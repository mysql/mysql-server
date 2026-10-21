// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2008-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.
// Copyright (c) 2024 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_CORE_COORDINATE_SYSTEM_HPP
#define BOOST_GEOMETRY_CORE_COORDINATE_SYSTEM_HPP


#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/static_assert.hpp>
#include <boost/geometry/util/type_traits_std.hpp>


namespace boost { namespace geometry
{


namespace traits
{

/*!
\brief Traits class defining the coordinate system of a point, important for strategy selection
\ingroup traits
\par Geometries:
    - point
\par Specializations should provide:
    - typedef CS type; (cs::cartesian, cs::spherical, etc)
*/
template <typename Point, typename Enable = void>
struct coordinate_system
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Point type.",
        Point);
};

} // namespace traits



#ifndef DOXYGEN_NO_DISPATCH
namespace core_dispatch
{
    template <typename GeometryTag, typename G>
    struct coordinate_system
    {
        using P = typename point_type<GeometryTag, G>::type;

        // Call its own specialization on point-tag
        using type = typename coordinate_system<point_tag, P>::type;
    };


    template <typename Point>
    struct coordinate_system<point_tag, Point>
    {
        using type = typename traits::coordinate_system
            <
                util::remove_cptrref_t<Point>
            >::type;
    };


} // namespace core_dispatch
#endif


/*!
\brief \brief_meta{type, coordinate system (cartesian\, spherical\, etc), \meta_point_type}
\tparam Geometry \tparam_geometry
\ingroup core

\qbk{[include reference/core/coordinate_system.qbk]}
*/
template <typename Geometry>
struct coordinate_system
{
    using type = typename core_dispatch::coordinate_system
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>
        >::type;
};

template <typename Geometry>
using coordinate_system_t = typename coordinate_system<Geometry>::type;

#ifndef DOXYGEN_NO_DETAIL
namespace detail {

// Short cut for coordinate system units
template <typename Geometry>
struct coordinate_system_units
{
    using type = typename coordinate_system<Geometry>::type::units;
};


template <typename Geometry>
using coordinate_system_units_t = typename coordinate_system_units<Geometry>::type;

} // namespace detail
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_CORE_COORDINATE_SYSTEM_HPP
