// Boost.Geometry

// Copyright (c) 2025 Siddharth Kumar, Roorkee, India.
// Copyright (c) 2025 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_GEOMETRIES_CONCEPTS_POLYHEDRAL_SURFACE_CONCEPT_HPP
#define BOOST_GEOMETRY_GEOMETRIES_CONCEPTS_POLYHEDRAL_SURFACE_CONCEPT_HPP

#include <type_traits>

#include <boost/concept_check.hpp>
#include <boost/range/concepts.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/geometries/concepts/polygon_concept.hpp>

namespace boost { namespace geometry { namespace concepts
{

template <typename Geometry>
class PolyhedralSurface
{
#ifndef DOXYGEN_NO_CONCEPT_MEMBERS
    using polygon_type = typename boost::range_value<Geometry>::type;

    BOOST_CONCEPT_ASSERT( (concepts::Polygon<polygon_type>) );
    BOOST_CONCEPT_ASSERT( (boost::RandomAccessRangeConcept<Geometry>) );
    static_assert(geometry::dimension<Geometry>::value == 3,
        "PolyhedralSurface must be 3-dimensional");
    static_assert(
        std::is_same<typename geometry::cs_tag_t<Geometry>, geometry::cartesian_tag>::value,
        "PolyhedralSurface must be cartesian.");

public:

    BOOST_CONCEPT_USAGE(PolyhedralSurface)
    {
        Geometry* ps = 0;
        traits::clear<Geometry>::apply(*ps);
        traits::resize<Geometry>::apply(*ps, 0);
        // The concept should support the second version of push_back, using &&
        polygon_type* poly = 0;
        traits::push_back<Geometry>::apply(*ps, std::move(*poly));
    }
#endif
};

// polyhedral surface(constant version)
template <typename Geometry>
class ConstPolyhedralSurface
{
#ifndef DOXYGEN_NO_CONCEPT_MEMBERS
    using polygon_type = typename boost::range_value<Geometry>::type;

    BOOST_CONCEPT_ASSERT( (concepts::ConstPolygon<polygon_type>) );
    BOOST_CONCEPT_ASSERT( (boost::RandomAccessRangeConcept<Geometry>) );
    static_assert(geometry::dimension<Geometry>::value == 3,
        "PolyhedralSurface must be 3-dimensional");
    static_assert(
        std::is_same<typename geometry::cs_tag_t<Geometry>, geometry::cartesian_tag>::value,
        "PolyhedralSurface must be cartesian.");

public:

    BOOST_CONCEPT_USAGE(ConstPolyhedralSurface)
    {}

#endif
};

template <typename Geometry>
struct concept_type<Geometry, polyhedral_surface_tag>
{
    using type = PolyhedralSurface<Geometry>;
};

template <typename Geometry>
struct concept_type<Geometry const, polyhedral_surface_tag>
{
    using type = ConstPolyhedralSurface<Geometry>;
};

}}} // namespace boost::geometry::concepts
#endif // BOOST_GEOMETRY_GEOMETRIES_CONCEPTS_POLYHEDRAL_SURFACE_CONCEPT_HPP
