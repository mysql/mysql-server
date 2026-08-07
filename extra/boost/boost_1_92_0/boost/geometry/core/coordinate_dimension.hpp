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

#ifndef BOOST_GEOMETRY_CORE_COORDINATE_DIMENSION_HPP
#define BOOST_GEOMETRY_CORE_COORDINATE_DIMENSION_HPP


#include <cstddef>

#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/static_assert.hpp>
#include <boost/geometry/util/type_traits_std.hpp>

namespace boost { namespace geometry
{

namespace traits
{

/*!
\brief Traits class indicating the number of dimensions of a point
\par Geometries:
    - point
\par Specializations should provide:
    - value (e.g. derived from std::integral_constant<std::size_t, D>)
\ingroup traits
*/
template <typename Point, typename Enable = void>
struct dimension
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Point type.",
        Point);
};

} // namespace traits

#ifndef DOXYGEN_NO_DISPATCH
namespace core_dispatch
{

// Base class derive from its own specialization of point-tag
template <typename T, typename G>
struct dimension
    : dimension<point_tag, typename point_type<T, G>::type>::type
{};

template <typename P>
struct dimension<point_tag, P>
    : std::integral_constant
        <
            std::size_t,
            traits::dimension<util::remove_cptrref_t<P>>::value
        >
{
    BOOST_GEOMETRY_STATIC_ASSERT(
        (traits::dimension<util::remove_cptrref_t<P>>::value > 0),
        "Dimension has to be greater than 0.",
        traits::dimension<util::remove_cptrref_t<P>>
    );
};

} // namespace core_dispatch
#endif

/*!
\brief \brief_meta{value, number of coordinates (the number of axes of any geometry), \meta_point_type}
\tparam Geometry \tparam_geometry
\ingroup core

\qbk{[include reference/core/coordinate_dimension.qbk]}
*/
template <typename Geometry>
struct dimension
    : core_dispatch::dimension
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>
        >
{};


#ifndef BOOST_NO_CXX17_INLINE_VARIABLES
template <typename Geometry>
inline constexpr std::size_t dimension_v = dimension<Geometry>::value;
#endif


/*!
\brief assert_dimension, enables compile-time checking if coordinate dimensions are as expected
\ingroup utility
*/
template <typename Geometry, std::size_t Dimensions>
constexpr inline void assert_dimension()
{
    static_assert(dimension<Geometry>::value == Dimensions, "Dimension check failed.");
}

/*!
\brief assert_dimension, enables compile-time checking if coordinate dimensions are as expected
\ingroup utility
*/
template <typename Geometry, std::size_t Dimensions>
constexpr inline void assert_dimension_less_equal()
{
    static_assert(dimension<Geometry>::value <= Dimensions, "Dimension check failed.");
}

template <typename Geometry, std::size_t Dimensions>
constexpr inline void assert_dimension_greater_equal()
{
    static_assert(dimension<Geometry>::value >= Dimensions, "Dimension check failed.");
}

/*!
\brief assert_dimension_equal, enables compile-time checking if coordinate dimensions of two geometries are equal
\ingroup utility
*/
template <typename G1, typename G2>
constexpr inline void assert_dimension_equal()
{
    static_assert(dimension<G1>::value == dimension<G2>::value, "Dimension check failed.");
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_CORE_COORDINATE_DIMENSION_HPP
