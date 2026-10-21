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

#ifndef BOOST_GEOMETRY_CORE_ACCESS_HPP
#define BOOST_GEOMETRY_CORE_ACCESS_HPP


#include <cstddef>

#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/static_assert.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/util/type_traits_std.hpp>


namespace boost { namespace geometry
{

/// Index of minimum corner of the box.
BOOST_INLINE_CONSTEXPR int min_corner = 0;

/// Index of maximum corner of the box.
BOOST_INLINE_CONSTEXPR int max_corner = 1;

namespace traits
{

/*!
\brief Traits class which gives access (get,set) to points.
\ingroup traits
\par Geometries:
///     @li point
\par Specializations should provide, per Dimension
///     @li static inline T get(G const&)
///     @li static inline void set(G&, T const&)
\tparam Geometry geometry-type
\tparam Dimension dimension to access
*/
template <typename Geometry, std::size_t Dimension, typename Enable = void>
struct access
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry type.",
        Geometry);
};


/*!
\brief Traits class defining "get" and "set" to get
    and set point coordinate values
\tparam Geometry geometry (box, segment)
\tparam Index index (min_corner/max_corner for box, 0/1 for segment)
\tparam Dimension dimension
\par Geometries:
    - box
    - segment
\par Specializations should provide:
    - static inline T get(G const&)
    - static inline void set(G&, T const&)
\ingroup traits
*/
template <typename Geometry, std::size_t Index, std::size_t Dimension>
struct indexed_access
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry type.",
        Geometry);
};


} // namespace traits

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

template
<
    typename Geometry,
    std::size_t Index,
    std::size_t Dimension
>
struct indexed_access_non_pointer
{
    static constexpr coordinate_type_t<Geometry> get(Geometry const& geometry)
    {
        return traits::indexed_access<Geometry, Index, Dimension>::get(geometry);
    }
    static void set(Geometry& b, coordinate_type_t<Geometry> const& value)
    {
        traits::indexed_access<Geometry, Index, Dimension>::set(b, value);
    }
};

template
<
    typename Geometry,
    std::size_t Index,
    std::size_t Dimension
>
struct indexed_access_pointer
{
    static constexpr coordinate_type_t<Geometry> get(Geometry const* geometry)
    {
        return traits::indexed_access<std::remove_pointer_t<Geometry>, Index, Dimension>::get(*geometry);
    }
    static void set(Geometry* geometry, coordinate_type_t<Geometry> const& value)
    {
        traits::indexed_access<std::remove_pointer_t<Geometry>, Index, Dimension>::set(*geometry, value);
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
    std::size_t Dimension,
    bool IsPointer
>
struct access
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry Tag type.",
        Geometry, Tag);
    //static inline T get(G const&) {}
    //static inline void set(G& g, T const& value) {}
};

template
<
    typename Tag,
    typename Geometry,
    std::size_t Index,
    std::size_t Dimension,
    bool IsPointer
>
struct indexed_access
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Geometry Tag type.",
        Geometry, Tag);
    //static inline T get(G const&) {}
    //static inline void set(G& g, T const& value) {}
};

template <typename Point, std::size_t Dimension>
struct access<point_tag, Point, Dimension, false>
{
    static constexpr coordinate_type_t<Point> get(Point const& point)
    {
        return traits::access<Point, Dimension>::get(point);
    }
    static void set(Point& p, coordinate_type_t<Point> const& value)
    {
        traits::access<Point, Dimension>::set(p, value);
    }
};

template <typename Point, std::size_t Dimension>
struct access<point_tag, Point, Dimension, true>
{
    static constexpr coordinate_type_t<Point> get(Point const* point)
    {
        return traits::access<std::remove_pointer_t<Point>, Dimension>::get(*point);
    }
    static void set(Point* p, coordinate_type_t<Point> const& value)
    {
        traits::access<std::remove_pointer_t<Point>, Dimension>::set(*p, value);
    }
};


template
<
    typename Box,
    std::size_t Index,
    std::size_t Dimension
>
struct indexed_access<box_tag, Box, Index, Dimension, false>
    : detail::indexed_access_non_pointer<Box, Index, Dimension>
{};

template
<
    typename Box,
    std::size_t Index,
    std::size_t Dimension
>
struct indexed_access<box_tag, Box, Index, Dimension, true>
    : detail::indexed_access_pointer<Box, Index, Dimension>
{};


template
<
    typename Segment,
    std::size_t Index,
    std::size_t Dimension
>
struct indexed_access<segment_tag, Segment, Index, Dimension, false>
    : detail::indexed_access_non_pointer<Segment, Index, Dimension>
{};


template
<
    typename Segment,
    std::size_t Index,
    std::size_t Dimension
>
struct indexed_access<segment_tag, Segment, Index, Dimension, true>
    : detail::indexed_access_pointer<Segment, Index, Dimension>
{};

} // namespace core_dispatch
#endif // DOXYGEN_NO_DISPATCH


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

// Two dummy tags to distinguish get/set variants below.
// They don't have to be specified by the user. The functions are distinguished
// by template signature also, but for e.g. GCC this is not enough. So give them
// a different signature.
struct signature_getset_dimension {};
struct signature_getset_index_dimension {};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL


/*!
\brief Get coordinate value of a geometry (usually a point)
\details \details_get_set
\ingroup get
\tparam Dimension \tparam_dimension_required
\tparam Geometry \tparam_geometry (usually a Point Concept)
\param geometry \param_geometry (usually a point)
\return The coordinate value of specified dimension of specified geometry

\qbk{[include reference/core/get_point.qbk]}
*/
template <std::size_t Dimension, typename Geometry>
constexpr inline coordinate_type_t<Geometry> get(Geometry const& geometry
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        , detail::signature_getset_dimension* = 0
#endif
        )
{
    return core_dispatch::access
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>,
            Dimension,
            std::is_pointer<Geometry>::value
        >::get(geometry);
}


/*!
\brief Set coordinate value of a geometry (usually a point)
\details \details_get_set
\tparam Dimension \tparam_dimension_required
\tparam Geometry \tparam_geometry (usually a Point Concept)
\param geometry \param_geometry
\param value The coordinate value to set
\ingroup set

\qbk{[include reference/core/set_point.qbk]}
*/
template <std::size_t Dimension, typename Geometry>
inline void set(Geometry& geometry
        , coordinate_type_t<Geometry> const& value
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        , detail::signature_getset_dimension* = 0
#endif
        )
{
    core_dispatch::access
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>,
            Dimension,
            std::is_pointer<Geometry>::value
        >::set(geometry, value);
}


/*!
\brief get coordinate value of a Box or Segment
\details \details_get_set
\tparam Index \tparam_index_required
\tparam Dimension \tparam_dimension_required
\tparam Geometry \tparam_box_or_segment
\param geometry \param_geometry
\return coordinate value
\ingroup get

\qbk{distinguish,with index}
\qbk{[include reference/core/get_box.qbk]}
*/
template <std::size_t Index, std::size_t Dimension, typename Geometry>
constexpr inline coordinate_type_t<Geometry> get(Geometry const& geometry
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        , detail::signature_getset_index_dimension* = 0
#endif
        )
{
    return core_dispatch::indexed_access
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>,
            Index,
            Dimension,
            std::is_pointer<Geometry>::value
        >::get(geometry);
}

/*!
\brief set coordinate value of a Box / Segment
\details \details_get_set
\tparam Index \tparam_index_required
\tparam Dimension \tparam_dimension_required
\tparam Geometry \tparam_box_or_segment
\param geometry \param_geometry
\param value The coordinate value to set
\ingroup set

\qbk{distinguish,with index}
\qbk{[include reference/core/set_box.qbk]}
*/
template <std::size_t Index, std::size_t Dimension, typename Geometry>
inline void set(Geometry& geometry
        , coordinate_type_t<Geometry> const& value
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        , detail::signature_getset_index_dimension* = 0
#endif
        )
{
    core_dispatch::indexed_access
        <
            tag_t<Geometry>,
            util::remove_cptrref_t<Geometry>,
            Index,
            Dimension,
            std::is_pointer<Geometry>::value
        >::set(geometry, value);
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_CORE_ACCESS_HPP
