// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2017-2020.
// Modifications copyright (c) 2017-2020, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_WITHIN_HPP
#define BOOST_GEOMETRY_STRATEGIES_WITHIN_HPP


#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/static_assert.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/core/tag_cast.hpp>


namespace boost { namespace geometry
{

namespace strategy { namespace within
{


namespace services
{

/*!
\brief Traits class binding a within determination strategy to a coordinate system
\ingroup within
\tparam GeometryContained geometry-type of input (possibly) contained type
\tparam GeometryContaining geometry-type of input (possibly) containing type
\tparam TagContained casted tag of (possibly) contained type
\tparam TagContaining casted tag of (possibly) containing type
\tparam CsTagContained tag of coordinate system of (possibly) contained type
\tparam CsTagContaining tag of coordinate system of (possibly) containing type
*/
template
<
    typename GeometryContained,
    typename GeometryContaining,
    typename TagContained = tag_t<GeometryContained>,
    typename TagContaining = tag_t<GeometryContaining>,
    typename CastedTagContained = tag_cast_t
                                    <
                                        tag_t<GeometryContained>,
                                        pointlike_tag, linear_tag, polygonal_tag, areal_tag
                                    >,
    typename CastedTagContaining = tag_cast_t
                                    <
                                        tag_t<GeometryContaining>,
                                        pointlike_tag, linear_tag, polygonal_tag, areal_tag
                                    >,
    typename CsTagContained = tag_cast_t
                                <
                                    cs_tag_t<point_type_t<GeometryContained>>,
                                    spherical_tag
                                >,
    typename CsTagContaining = tag_cast_t
                                <
                                    cs_tag_t<point_type_t<GeometryContaining>>,
                                    spherical_tag
                                >
>
struct default_strategy
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for these types.",
        GeometryContained, GeometryContaining);
};


} // namespace services


}} // namespace strategy::within


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_STRATEGIES_WITHIN_HPP

