// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2025 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.
// Copyright (c) 2014-2024 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2017-2023.
// Modifications copyright (c) 2017-2023, Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_CONVERT_HPP
#define BOOST_GEOMETRY_ALGORITHMS_CONVERT_HPP


#include <cstddef>
#include <type_traits>

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/size.hpp>
#include <boost/range/value_type.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant_fwd.hpp>

#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/detail/assign_box_corners.hpp>
#include <boost/geometry/algorithms/detail/assign_indexed_point.hpp>
#include <boost/geometry/algorithms/detail/convert_point_to_point.hpp>
#include <boost/geometry/algorithms/detail/convert_indexed_to_indexed.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>

#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/tag_cast.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/concepts/check.hpp>

#include <boost/geometry/util/numeric_cast.hpp>
#include <boost/geometry/util/range.hpp>

#include <boost/geometry/views/detail/closed_clockwise_view.hpp>


namespace boost { namespace geometry
{

// Silence warning C4127: conditional expression is constant
// Silence warning C4512: assignment operator could not be generated
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127 4512)
#endif


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace conversion
{

template
<
    typename Point,
    typename Box,
    std::size_t Index,
    std::size_t Dimension,
    std::size_t DimensionCount
>
struct point_to_box
{
    static inline void apply(Point const& point, Box& box)
    {
        set<Index, Dimension>(box,
                util::numeric_cast<coordinate_type_t<Box>>(get<Dimension>(point)));
        point_to_box
            <
                Point, Box,
                Index, Dimension + 1, DimensionCount
            >::apply(point, box);
    }
};


template
<
    typename Point,
    typename Box,
    std::size_t Index,
    std::size_t DimensionCount
>
struct point_to_box<Point, Box, Index, DimensionCount, DimensionCount>
{
    static inline void apply(Point const& , Box& )
    {}
};

template <bool Close, bool Reverse>
struct box_to_range
{
    template <typename Box, typename Range>
    static inline void apply(Box const& box, Range& range)
    {
        traits::resize<Range>::apply(range, Close ? 5 : 4);
        assign_box_corners_oriented<Reverse>(box, range);
        if (Close)
        {
            range::at(range, 4) = range::at(range, 0);
        }
    }
};

template <typename Segment, typename Range>
struct segment_to_range
{
    static inline void apply(Segment const& segment, Range& range)
    {
        traits::resize<Range>::apply(range, 2);

        auto it = boost::begin(range);

        assign_point_from_index<0>(segment, *it);
        ++it;
        assign_point_from_index<1>(segment, *it);
    }
};

struct range_to_range
{
    struct default_policy
    {
        template <typename Point1, typename Point2>
        static inline void apply(Point1 const& point1, Point2 & point2)
        {
            geometry::detail::conversion::convert_point_to_point(point1, point2);
        }
    };

    template <typename Range1, typename Range2>
    static inline void apply(Range1 const& source, Range2& destination)
    {
        apply(source, destination, default_policy());
    }

    template <typename ConvertPointPolicy, typename Range1, typename Range2>
    static inline ConvertPointPolicy apply(Range1 const& source, Range2& destination,
                                           ConvertPointPolicy convert_point)
    {
        geometry::clear(destination);

        constexpr bool reverse = geometry::point_order<Range1>::value != geometry::point_order<Range2>::value;

        using view_type = detail::closed_clockwise_view
            <
                Range1 const,
                geometry::closure<Range1>::value,
                reverse ? counterclockwise : clockwise
            >;

        // We consider input always as closed, and skip last
        // point for open output.
        view_type const view(source);

        auto n = boost::size(view);
        if (geometry::closure<Range2>::value == geometry::open)
        {
            n--;
        }

        // If size == 0 && geometry::open <=> n = numeric_limits<size_type>::max()
        // but ok, sice below it == end()

        decltype(n) i = 0;
        for (auto it = boost::begin(view);
            it != boost::end(view) && i < n;
            ++it, ++i)
        {
            typename boost::range_value<Range2>::type point;
            convert_point.apply(*it, point);
            range::push_back(destination, point);
        }

        return convert_point;
    }
};

// Converts a polygon to an output iterator of ranges.
// The iterator is updated such that it can used iteratively.
struct polygon_to_multi_range
{
    template <typename Polygon, typename OutputIterator>
    static inline void apply(Polygon const& source, OutputIterator& it)
    {
        range_to_range::apply(geometry::exterior_ring(source), *it++);
        for (auto const& ring : geometry::interior_rings(source))
        {
            range_to_range::apply(ring, *it++);
        }
    }
};

template <typename Polygon1, typename Polygon2>
struct polygon_to_polygon
{
    static inline void apply(Polygon1 const& source, Polygon2& destination)
    {
        range_to_range::apply(geometry::exterior_ring(source),
            geometry::exterior_ring(destination));

        // Container should be resizeable
        traits::resize
            <
                std::remove_reference_t
                    <
                        typename traits::interior_mutable_type<Polygon2>::type
                    >
            >::apply(interior_rings(destination), num_interior_rings(source));

        auto const& rings_source = interior_rings(source);
        auto&& rings_dest = interior_rings(destination);

        auto it_source = boost::begin(rings_source);
        auto it_dest = boost::begin(rings_dest);

        for ( ; it_source != boost::end(rings_source); ++it_source, ++it_dest)
        {
            range_to_range::apply(*it_source, *it_dest);
        }
    }
};

struct range_to_multi_point
{
    template <typename Range, typename Iterator>
    static inline void append_from_source(Range const& source, Iterator& it)
    {
        for (auto const& point : source)
        {
            detail::conversion::convert_point_to_point(point, *it);
            ++it;
        }
    }

    template <typename Range, typename MultiPoint>
    static inline void apply(Range const& source, MultiPoint& destination)
    {
        traits::resize<MultiPoint>::apply(destination, boost::size(source));
        auto it = boost::begin(destination);
        append_from_source(source, it);
    }
};

struct polygon_to_range
{
    template <typename Polygon, typename Iterator>
    static inline void append_from_polygon(Polygon const& source, Iterator& it)
    {
        range_to_multi_point::append_from_source(geometry::exterior_ring(source), it);
        for (auto const& ring : geometry::interior_rings(source))
        {
            range_to_multi_point::append_from_source(ring, it);
        }
    }

    template <typename Polygon, typename MultiPoint>
    static inline void apply(Polygon const& source, MultiPoint& destination)
    {
        traits::resize<MultiPoint>::apply(destination, geometry::num_points(source));
        auto it = boost::begin(destination);
        append_from_polygon(source, it);
    }
};

struct multi_polygon_to_range
{
    template <typename MultiPolygon, typename MultiPoint>
    static inline void apply(MultiPolygon const& source, MultiPoint& destination)
    {
        traits::resize<MultiPoint>::apply(destination, geometry::num_points(source));
        auto it = boost::begin(destination);
        for (auto const& polygon : source)
        {
            polygon_to_range::append_from_polygon(polygon, it);
        }
    }
};

template <typename Single, typename Multi, typename ConversionAlgorithm>
struct single_to_multi
{
    static inline void apply(Single const& single, Multi& multi)
    {
        traits::resize<Multi>::apply(multi, 1);
        ConversionAlgorithm::apply(single, *boost::begin(multi));
    }
};


template <typename Multi1, typename Multi2, typename ConversionAlgorithm>
struct multi_to_multi
{
    static inline void apply(Multi1 const& multi1, Multi2& multi2)
    {
        traits::resize<Multi2>::apply(multi2, boost::size(multi1));

        auto it1 = boost::begin(multi1);
        auto it2 = boost::begin(multi2);

        for (; it1 != boost::end(multi1); ++it1, ++it2)
        {
            ConversionAlgorithm::apply(*it1, *it2);
        }
    }
};


}} // namespace detail::conversion
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

// TODO: We could use std::is_assignable instead of std::is_same.
//   Then we should rather check ! std::is_array<Geometry2>::value
//   which is Destination.

template
<
    typename Geometry1, typename Geometry2,
    typename Tag1 = tag_t<Geometry1>,
    typename Tag2 = tag_t<Geometry2>,
    std::size_t DimensionCount = dimension<Geometry1>::value,
    bool UseAssignment = std::is_same<Geometry1, Geometry2>::value
                         && !std::is_array<Geometry1>::value
>
struct convert
    : not_implemented
        <
            Tag1, Tag2,
            std::integral_constant<std::size_t, DimensionCount>
        >
{};


template
<
    typename Geometry1, typename Geometry2,
    typename Tag,
    std::size_t DimensionCount
>
struct convert<Geometry1, Geometry2, Tag, Tag, DimensionCount, true>
{
    // Same geometry type -> copy geometry
    static inline void apply(Geometry1 const& source, Geometry2& destination)
    {
        destination = source;
    }
};


template <typename Geometry1, typename Geometry2, std::size_t DimensionCount>
struct convert<Geometry1, Geometry2, point_tag, point_tag, DimensionCount, false>
    : detail::conversion::point_to_point<Geometry1, Geometry2, 0, DimensionCount>
{};


template <typename Box1, typename Box2, std::size_t DimensionCount>
struct convert<Box1, Box2, box_tag, box_tag, DimensionCount, false>
    : detail::conversion::indexed_to_indexed<Box1, Box2, 0, DimensionCount>
{};


template <typename Segment1, typename Segment2, std::size_t DimensionCount>
struct convert<Segment1, Segment2, segment_tag, segment_tag, DimensionCount, false>
    : detail::conversion::indexed_to_indexed<Segment1, Segment2, 0, DimensionCount>
{};


template <typename Segment, typename LineString, std::size_t DimensionCount>
struct convert<Segment, LineString, segment_tag, linestring_tag, DimensionCount, false>
    : detail::conversion::segment_to_range<Segment, LineString>
{};


template <typename Ring1, typename Ring2, std::size_t DimensionCount>
struct convert<Ring1, Ring2, ring_tag, ring_tag, DimensionCount, false>
    : detail::conversion::range_to_range
{};

template <typename LineString1, typename LineString2, std::size_t DimensionCount>
struct convert<LineString1, LineString2, linestring_tag, linestring_tag, DimensionCount, false>
    : detail::conversion::range_to_range
{};

template <typename Polygon1, typename Polygon2, std::size_t DimensionCount>
struct convert<Polygon1, Polygon2, polygon_tag, polygon_tag, DimensionCount, false>
    : detail::conversion::polygon_to_polygon<Polygon1, Polygon2>
{};

template <typename Box, typename LineString>
struct convert<Box, LineString, box_tag, linestring_tag, 2, false>
    : detail::conversion::box_to_range<true, false>
{};

template <typename Box, typename Ring>
struct convert<Box, Ring, box_tag, ring_tag, 2, false>
    : detail::conversion::box_to_range
        <
            geometry::closure<Ring>::value == closed,
            geometry::point_order<Ring>::value == counterclockwise
        >
{};

template <typename Box, typename MultiPoint>
struct convert<Box, MultiPoint, box_tag, multi_point_tag, 2, false>
    : detail::conversion::box_to_range<false, false>
{};


template <typename Box, typename Polygon>
struct convert<Box, Polygon, box_tag, polygon_tag, 2, false>
{
    static inline void apply(Box const& box, Polygon& polygon)
    {
        convert
            <
                Box, ring_type_t<Polygon>,
                box_tag, ring_tag,
                2, false
            >::apply(box, exterior_ring(polygon));
    }
};


template <typename Point, typename Box, std::size_t DimensionCount>
struct convert<Point, Box, point_tag, box_tag, DimensionCount, false>
{
    static inline void apply(Point const& point, Box& box)
    {
        detail::conversion::point_to_box
            <
                Point, Box, min_corner, 0, DimensionCount
            >::apply(point, box);
        detail::conversion::point_to_box
            <
                Point, Box, max_corner, 0, DimensionCount
            >::apply(point, box);
    }
};


template <typename Ring, typename Polygon, std::size_t DimensionCount>
struct convert<Ring, Polygon, ring_tag, polygon_tag, DimensionCount, false>
{
    static inline void apply(Ring const& ring, Polygon& polygon)
    {
        convert
            <
                Ring, ring_type_t<Polygon>,
                ring_tag, ring_tag,
                DimensionCount, false
            >::apply(ring, exterior_ring(polygon));
    }
};

// Polygon to ring, this ignores interior rings
template <typename Polygon, typename Ring, std::size_t DimensionCount>
struct convert<Polygon, Ring, polygon_tag, ring_tag, DimensionCount, false>
{
    static inline void apply(Polygon const& polygon, Ring& ring)
    {
        convert
            <
                ring_type_t<Polygon>, Ring,
                ring_tag, ring_tag,
                DimensionCount, false
            >::apply(exterior_ring(polygon), ring);
    }
};

template <typename LineString, typename MultiPoint, std::size_t DimensionCount>
struct convert<LineString, MultiPoint, linestring_tag, multi_point_tag, DimensionCount, false>
    : detail::conversion::range_to_multi_point {};

template <typename Ring, typename MultiPoint, std::size_t DimensionCount>
struct convert<Ring, MultiPoint, ring_tag, multi_point_tag, DimensionCount, false>
    : detail::conversion::range_to_multi_point {};

template <typename Polygon, typename MultiPoint, std::size_t DimensionCount>
struct convert<Polygon, MultiPoint, polygon_tag, multi_point_tag, DimensionCount, false>
    : detail::conversion::polygon_to_range {};

    template <typename MultiPolygon, typename MultiPoint, std::size_t DimensionCount>
struct convert<MultiPolygon, MultiPoint, multi_polygon_tag, multi_point_tag, DimensionCount, false>
    : detail::conversion::multi_polygon_to_range {};

template <typename MultiLineString, typename MultiPoint, std::size_t DimensionCount>
struct convert<MultiLineString, MultiPoint, multi_linestring_tag, multi_point_tag, DimensionCount, false>
{
    static inline void apply(MultiLineString const& source, MultiPoint& destination)
    {
        traits::resize<MultiPoint>::apply(destination, geometry::num_points(source));
        auto it = boost::begin(destination);
        for (auto const& linestring : source)
        {
            detail::conversion::range_to_multi_point::append_from_source(linestring, it);
        }
    }
};

template <typename Ring, typename MultiLinestring, std::size_t DimensionCount>
struct convert<Ring, MultiLinestring, ring_tag, multi_linestring_tag, DimensionCount, false>
{
    static inline void apply(Ring const& source, MultiLinestring& destination)
    {
        traits::resize<MultiLinestring>::apply(destination, 1);
        detail::conversion::range_to_range::apply(source, *boost::begin(destination));
    }
};

template <typename Polygon, typename MultiLinestring, std::size_t DimensionCount>
struct convert<Polygon, MultiLinestring, polygon_tag, multi_linestring_tag, DimensionCount, false>
{
    static inline void apply(Polygon const& source, MultiLinestring& destination)
    {
        traits::resize<MultiLinestring>::apply(destination, 1 + geometry::num_interior_rings(source));
        auto it = boost::begin(destination);
        detail::conversion::polygon_to_multi_range::apply(source, it);
    }
};

template <typename MultiPolygon, typename MultiLinestring, std::size_t DimensionCount>
struct convert<MultiPolygon, MultiLinestring, multi_polygon_tag, multi_linestring_tag, DimensionCount, false>
{
    static inline void apply(MultiPolygon const& source, MultiLinestring& destination)
    {
        std::size_t ring_count = boost::size(source);
        for (auto const& polygon : source)
        {
            ring_count += geometry::num_interior_rings(polygon);
        }
        traits::resize<MultiLinestring>::apply(destination, ring_count);

        auto it = boost::begin(destination);
        for (auto const& polygon : source)
        {
            detail::conversion::polygon_to_multi_range::apply(polygon, it);
        }
    }
};

template <typename Single, typename Multi, std::size_t DimensionCount>
struct single_to_multi_convert
    : detail::conversion::single_to_multi
        <
            Single,
            Multi,
            convert
                <
                    Single,
                    typename boost::range_value<Multi>::type,
                    tag_t<Single>,
                    single_tag_of_t<tag_t<Multi>>,
                    DimensionCount,
                    false
                >
        >
{};

template <typename Multi1, typename Multi2, std::size_t DimensionCount>
struct multi_to_multi_convert
    : detail::conversion::multi_to_multi
        <
            Multi1,
            Multi2,
            convert
                <
                    typename boost::range_value<Multi1>::type,
                    typename boost::range_value<Multi2>::type,
                    single_tag_of_t<tag_t<Multi1>>,
                    single_tag_of_t<tag_t<Multi2>>,
                    DimensionCount
                >
        >
{};

// Multi to multi of the same tag type
template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, multi_point_tag, multi_point_tag, DimensionCount, false>
    : multi_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, multi_linestring_tag, multi_linestring_tag, DimensionCount, false>
    : multi_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, multi_polygon_tag, multi_polygon_tag, DimensionCount, false>
    : multi_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

// Single to multi of the same topological tag type
template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, point_tag, multi_point_tag, DimensionCount, false>
    : single_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, segment_tag, multi_linestring_tag, DimensionCount, false>
    : single_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, linestring_tag, multi_linestring_tag, DimensionCount, false>
    : single_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

// To multi_polygon (box, ring, polygon)
template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, box_tag, multi_polygon_tag, DimensionCount, false>
    : single_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, ring_tag, multi_polygon_tag, DimensionCount, false>
    : single_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};

template <typename GeometryIn, typename GeometryOut, std::size_t DimensionCount>
struct convert<GeometryIn, GeometryOut, polygon_tag, multi_polygon_tag, DimensionCount, false>
    : single_to_multi_convert<GeometryIn, GeometryOut, DimensionCount> {};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_variant {

template <typename Geometry1, typename Geometry2>
struct convert
{
    static inline void apply(Geometry1 const& geometry1, Geometry2& geometry2)
    {
        concepts::check_concepts_and_equal_dimensions<Geometry1 const, Geometry2>();
        dispatch::convert<Geometry1, Geometry2>::apply(geometry1, geometry2);
    }
};

template <BOOST_VARIANT_ENUM_PARAMS(typename T), typename Geometry2>
struct convert<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>, Geometry2>
{
    struct visitor: static_visitor<void>
    {
        Geometry2& m_geometry2;

        visitor(Geometry2& geometry2)
            : m_geometry2(geometry2)
        {}

        template <typename Geometry1>
        inline void operator()(Geometry1 const& geometry1) const
        {
            convert<Geometry1, Geometry2>::apply(geometry1, m_geometry2);
        }
    };

    static inline void apply(
        boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> const& geometry1,
        Geometry2& geometry2
    )
    {
        boost::apply_visitor(visitor(geometry2), geometry1);
    }
};

}


/*!
\brief Converts one geometry to another geometry
\details The convert algorithm converts one geometry, e.g. a BOX, to another
geometry, e.g. a RING. This only works if it is possible and applicable.
If the point-order is different, or the closure is different between two
geometry types, it will be converted correctly by explicitly reversing the
points or closing or opening the polygon rings.
\ingroup convert
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry (source)
\param geometry2 \param_geometry (target)

\qbk{[include reference/algorithms/convert.qbk]}
 */
template <typename Geometry1, typename Geometry2>
inline void convert(Geometry1 const& geometry1, Geometry2& geometry2)
{
    resolve_variant::convert<Geometry1, Geometry2>::apply(geometry1, geometry2);
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_CONVERT_HPP
