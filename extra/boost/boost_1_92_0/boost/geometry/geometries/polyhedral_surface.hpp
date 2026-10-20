// Boost.Geometry

// Copyright (c) 2025 Siddharth Kumar, Roorkee, India.
// Copyright (c) 2025 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_GEOMETRIES_POLYHEDRAL_SURFACE_HPP
#define BOOST_GEOMETRY_GEOMETRIES_POLYHEDRAL_SURFACE_HPP

#include <memory>
#include <vector>
#include <boost/concept/assert.hpp>
#include <boost/geometry/geometries/concepts/point_concept.hpp>
#include <boost/geometry/geometries/concepts/polygon_concept.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/config.hpp>

namespace boost { namespace geometry
{
namespace model
{

/*!
\brief A Polyhedral Surface is a contiguous collection of polygons in 3-dimensional space,
    which share common boundary segments. The concepts and the constructors don't check if the
    boundary segments are common but is_valid() does.
\ingroup geometries
\tparam Polygon polygon type
\tparam Container container type for polygons,
    default std::vector
\tparam Allocator container-allocator-type, for the polygons

\qbk{[include reference/geometries/polyhedral_surface.qbk]}
\qbk{before.synopsis,
[heading Model of]
[link geometry.reference.concepts.concept_polyhedral_surface PolyhedralSurface Concept]
}

*/

template
<
    typename Polygon,
    template<typename, typename> class Container = std::vector,
    template<typename> class Allocator = std::allocator

>
class polyhedral_surface : public Container<Polygon, Allocator<Polygon> >
{
    BOOST_CONCEPT_ASSERT( (concepts::Polygon<Polygon>) );

public :
    using polygon_type = Polygon;
    using polygon_container = Container<Polygon, Allocator<Polygon> >;

    /// \constructor_default{polyhedron}
    inline polyhedral_surface()
        : polygon_container()
    {}

    /// \constructor_initialized_list{polyhedron}
    inline polyhedral_surface(std::initializer_list<Polygon> l)
        : polygon_container(l.begin(), l.end())
    {}

};
} // namespace model

#ifndef DOXYGEN_NO_TRAITS_SPECIALIZATIONS
namespace traits
{

template
<
    typename Polygon,
    template<typename, typename> class Container,
    template<typename> class Allocator
>
struct tag
<
    model::polyhedral_surface
        <
            Polygon,
            Container, Allocator
        >
>
{
    using type = polyhedral_surface_tag;
};

} // namespace traits
#endif // DOXYGEN_NO_TRAITS_SPECIALIZATIONS

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_GEOMETRIES_POLYHEDRAL_SURFACE_HPP
