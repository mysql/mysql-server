// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.
// Copyright (c) 2024 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014-2020.
// Modifications copyright (c) 2014-2020 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_CORE_RADIUS_HPP
#define BOOST_GEOMETRY_CORE_RADIUS_HPP


#include <cstddef>

#include <boost/geometry/core/static_assert.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/util/type_traits_std.hpp>


namespace boost { namespace geometry
{

namespace traits
{

/*!
    \brief Traits class to get/set radius of a circle/sphere/(ellipse)
    \details the radius access meta-functions give read/write access to the radius of a circle or a sphere,
    or to the major/minor axis or an ellipse, or to one of the 3 equatorial radii of an ellipsoid.

    It should be specialized per geometry, in namespace core_dispatch. Those specializations should
    forward the call via traits to the geometry class, which could be specified by the user.

    There is a corresponding generic radius_get and radius_set function
    \par Geometries:
        - n-sphere (circle,sphere)
        - upcoming ellipse
    \par Specializations should provide:
        - inline static T get(Geometry const& geometry)
        - inline static void set(Geometry& geometry, T const& radius)
    \ingroup traits
*/
template <typename Geometry, std::size_t Dimension>
struct radius_access {};


/*!
    \brief Traits class indicating the type (double,float,...) of the radius of a circle or a sphere
    \par Geometries:
        - n-sphere (circle,sphere)
        - upcoming ellipse
    \par Specializations should provide:
        - typedef T type (double,float,int,etc)
    \ingroup traits
*/
template <typename Geometry>
struct radius_type {};

} // namespace traits


#ifndef DOXYGEN_NO_DISPATCH
namespace core_dispatch
{

template <typename Tag, typename Geometry>
struct radius_type
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry Tag type.",
        Geometry, Tag);
    //typedef core_dispatch_specialization_required type;
};

/*!
    \brief radius access meta-functions, used by concept n-sphere and upcoming ellipse.
*/
template
<
    typename Tag,
    typename Geometry,
    std::size_t Dimension,
    bool IsPointer
>
struct radius_access
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry Tag type.",
        Geometry, Tag);
    //static inline CoordinateType get(Geometry const& ) {}
    //static inline void set(Geometry& g, CoordinateType const& value) {}
};

} // namespace core_dispatch
#endif // DOXYGEN_NO_DISPATCH


/*!
    \brief Metafunction to get the type of radius of a circle / sphere / ellipse / etc.
    \ingroup access
    \tparam Geometry the type of geometry
*/
template <typename Geometry>
struct radius_type
{
    using type = typename core_dispatch::radius_type
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>
        >::type;
};


template <typename Geometry>
using radius_type_t = typename radius_type<Geometry>::type;


/*!
    \brief Function to get radius of a circle / sphere / ellipse / etc.
    \return radius The radius for a given axis
    \ingroup access
    \param geometry the geometry to get the radius from
    \tparam I index of the axis
*/
template <std::size_t I, typename Geometry>
inline radius_type_t<Geometry> get_radius(Geometry const& geometry)
{
    return core_dispatch::radius_access
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>,
            I,
            std::is_pointer<Geometry>::value
        >::get(geometry);
}

/*!
    \brief Function to set the radius of a circle / sphere / ellipse / etc.
    \ingroup access
    \tparam I index of the axis
    \param geometry the geometry to change
    \param radius the radius to set
*/
template <std::size_t I, typename Geometry>
inline void set_radius(Geometry& geometry,
                       radius_type_t<Geometry> const& radius)
{
    core_dispatch::radius_access
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>,
            I,
            std::is_pointer<Geometry>::value
        >::set(geometry, radius);
}



#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

template <typename Tag, typename Geometry, std::size_t Dimension>
struct radius_access
{
    static inline radius_type_t<Geometry> get(Geometry const& geometry)
    {
        return traits::radius_access<Geometry, Dimension>::get(geometry);
    }
    static inline void set(Geometry& geometry,
                           radius_type_t<Geometry> const& value)
    {
        traits::radius_access<Geometry, Dimension>::set(geometry, value);
    }
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace core_dispatch
{

template
<
    typename Tag,
    typename Geometry,
    std::size_t Dimension
>
struct radius_access<Tag, Geometry, Dimension, true>
{
    static inline radius_type_t<Geometry> get(const Geometry * geometry)
    {
        return radius_access
            <
                Tag,
                Geometry,
                Dimension,
                std::is_pointer<Geometry>::value
            >::get(*geometry);
    }

    static inline void set(Geometry * geometry, radius_type_t<Geometry> const& value)
    {
        return radius_access
            <
                Tag,
                Geometry,
                Dimension,
                std::is_pointer<Geometry>::value
            >::set(*geometry, value);
    }
};


template <typename Geometry>
struct radius_type<srs_sphere_tag, Geometry>
{
    using type = typename traits::radius_type<Geometry>::type;
};

template <typename Geometry, std::size_t Dimension>
struct radius_access<srs_sphere_tag, Geometry, Dimension, false>
    : detail::radius_access<srs_sphere_tag, Geometry, Dimension>
{
    //BOOST_STATIC_ASSERT(Dimension == 0);
    static_assert(Dimension < 3, "Dimension must be lower than 3.");
};

template <typename Geometry>
struct radius_type<srs_spheroid_tag, Geometry>
{
    using type = typename traits::radius_type<Geometry>::type;
};

template <typename Geometry, std::size_t Dimension>
struct radius_access<srs_spheroid_tag, Geometry, Dimension, false>
    : detail::radius_access<srs_spheroid_tag, Geometry, Dimension>
{
    //BOOST_STATIC_ASSERT(Dimension == 0 || Dimension == 2);
    static_assert(Dimension < 3, "Dimension must be lower than 3.");
};

} // namespace core_dispatch
#endif // DOXYGEN_NO_DISPATCH


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_CORE_RADIUS_HPP
