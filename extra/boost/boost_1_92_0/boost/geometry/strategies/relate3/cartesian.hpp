// Boost.Geometry

// Copyright (c) 2025, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_RELATE3_CARTESIAN_HPP
#define BOOST_GEOMETRY_STRATEGIES_RELATE3_CARTESIAN_HPP

#include <boost/geometry/strategies/relate/cartesian.hpp>
#include <boost/geometry/strategies/relate3/services.hpp>
#include <boost/geometry/strategies/cartesian/side_rounded_input.hpp>
#include <boost/geometry/strategies/cartesian/side_3d_rounded_input.hpp>

namespace boost { namespace geometry
{

namespace strategies { namespace relate3
{

template <typename CalculationType = void>
class cartesian
    : public strategies::relate::cartesian<CalculationType>
{
public:

    static auto side3()
    {
        return strategy::side::side_3d_rounded_input<>();
    }

    static auto side()
    {
        return strategy::side::side_rounded_input<>();
    }
};

namespace services
{

template <typename Geometry1, typename Geometry2>
struct default_strategy<Geometry1, Geometry2, cartesian_tag, cartesian_tag>
{
    using type = strategies::relate3::cartesian<>;
};

} // namespace services

}} // namespace strategies::relate3

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_STRATEGIES_RELATE_CARTESIAN_HPP
