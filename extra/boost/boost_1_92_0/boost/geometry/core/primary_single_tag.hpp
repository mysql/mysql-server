// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2024-2024 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PRIMARY_SINGLE_TAG_HPP
#define BOOST_GEOMETRY_PRIMARY_SINGLE_TAG_HPP

#include <boost/geometry/core/tags.hpp>

namespace boost { namespace geometry
{

/*!
\brief Metafunction defining a type being the primary tag
    (point, linestring, polygon) of the tag related to its topological dimension.
\details pointlike_tag will be casted to point_tag, linear_tag to linestring_tag,
    areal_tag to polygon_tag.
\ingroup core
\tparam Tag The tag to get the canonical tag from
*/
template <typename Tag>
struct primary_single_tag
{

};

template <>
struct primary_single_tag<pointlike_tag>
{
    using type = point_tag;
};

template <>
struct primary_single_tag<linear_tag>
{
    using type = linestring_tag;
};

template <>
struct primary_single_tag<areal_tag>
{
    using type = polygon_tag;
};

template <typename Tag>
using primary_single_tag_t = typename primary_single_tag<Tag>::type;


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PRIMARY_SINGLE_TAG_HPP
