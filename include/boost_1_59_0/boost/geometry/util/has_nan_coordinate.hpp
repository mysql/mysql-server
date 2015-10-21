// Boost.Geometry

// Copyright (c) 2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_UTIL_HAS_NAN_COORDINATE_HPP
#define BOOST_GEOMETRY_UTIL_HAS_NAN_COORDINATE_HPP

#include <cstddef>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>

#include <boost/math/special_functions/fpclassify.hpp>


namespace boost { namespace geometry {
    
#ifndef DOXYGEN_NO_DETAIL
namespace detail {

template <typename Point,
          std::size_t I = 0,
          std::size_t N = geometry::dimension<Point>::value>
struct has_nan_coordinate
{
    static bool apply(Point const& point)
    {
        return boost::math::isnan(geometry::get<I>(point))
            || has_nan_coordinate<Point, I+1, N>::apply(point);
    }
};

template <typename Point, std::size_t N>
struct has_nan_coordinate<Point, N, N>
{
    static bool apply(Point const& )
    {
        return false;
    }
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL

template <typename Point>
bool has_nan_coordinate(Point const& point)
{
    return detail::has_nan_coordinate<Point>::apply(point);
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_UTIL_HAS_NAN_COORDINATE_HPP
