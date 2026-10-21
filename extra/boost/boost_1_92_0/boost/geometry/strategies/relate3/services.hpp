// Boost.Geometry

// Copyright (c) 2025, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_RELATE3_SERVICES_HPP
#define BOOST_GEOMETRY_STRATEGIES_RELATE3_SERVICES_HPP


#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/static_assert.hpp>


namespace boost { namespace geometry
{


namespace strategies { namespace relate3 {

namespace services
{

template
<
    typename Geometry1,
    typename Geometry2,
    typename CSTag1 = geometry::cs_tag_t<Geometry1>,
    typename CSTag2 = geometry::cs_tag_t<Geometry2>
>
struct default_strategy
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry's coordinate system.",
        Geometry1, Geometry2, CSTag1, CSTag2);
};

template <typename Strategy>
struct strategy_converter
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Strategy.",
        Strategy);
};


} // namespace services

}} // namespace strategies::relate3


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_STRATEGIES_RELATE3_SERVICES_HPP
