// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014-2024.
// Modifications copyright (c) 2014-2024, Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_INTERSECTION_INTERFACE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_INTERSECTION_INTERFACE_HPP


#include <boost/geometry/algorithms/detail/overlay/intersection_insert.hpp>
#include <boost/geometry/algorithms/detail/tupled_output.hpp>
#include <boost/geometry/geometries/adapted/boost_variant.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>
#include <boost/geometry/strategies/detail.hpp>
#include <boost/geometry/strategies/relate/services.hpp>
#include <boost/geometry/util/range.hpp>
#include <boost/geometry/util/type_traits_std.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

// By default, all is forwarded to the intersection_insert-dispatcher
template
<
    typename Geometry1, typename Geometry2,
    typename Tag1 = geometry::tag_t<Geometry1>,
    typename Tag2 = geometry::tag_t<Geometry2>,
    bool Reverse = reverse_dispatch<Geometry1, Geometry2>::type::value
>
struct intersection
{
    template <typename GeometryOut, typename Strategy>
    static inline bool apply(Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            GeometryOut& geometry_out,
            Strategy const& strategy)
    {
        using single_out = typename geometry::detail::output_geometry_value
            <
                GeometryOut
            >::type;

        intersection_insert
            <
                Geometry1, Geometry2, single_out,
                overlay_intersection
            >::apply(geometry1, geometry2,
                     geometry::detail::output_geometry_back_inserter(geometry_out),
                     strategy);

        return true;
    }

};


// If reversal is needed, perform it
template
<
    typename Geometry1, typename Geometry2,
    typename Tag1, typename Tag2
>
struct intersection
<
    Geometry1, Geometry2,
    Tag1, Tag2,
    true
>
    : intersection<Geometry2, Geometry1, Tag2, Tag1, false>
{
    template <typename GeometryOut, typename Strategy>
    static inline bool apply(
        Geometry1 const& g1,
        Geometry2 const& g2,
        GeometryOut& out,
        Strategy const& strategy)
    {
        return intersection
            <
                Geometry2, Geometry1,
                Tag2, Tag1,
                false
            >::apply(g2, g1, out, strategy);
    }
};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_collection
{

template
<
    typename Geometry1, typename Geometry2, typename GeometryOut,
    typename Tag1 = geometry::tag_t<Geometry1>,
    typename Tag2 = geometry::tag_t<Geometry2>,
    typename TagOut = geometry::tag_t<GeometryOut>
>
struct intersection
{
    template <typename Strategy>
    static bool apply(Geometry1 const& geometry1, Geometry2 const& geometry2,
                      GeometryOut & geometry_out, Strategy const& strategy)
    {
        return dispatch::intersection
            <
                Geometry1,
                Geometry2
            >::apply(geometry1, geometry2, geometry_out,
                     strategy);
    }
};

} // namespace resolve_collection


namespace resolve_strategy {

template
<
    typename Strategy,
    bool IsUmbrella = strategies::detail::is_umbrella_strategy<Strategy>::value
>
struct intersection
{
    template
    <
        typename Geometry1,
        typename Geometry2,
        typename GeometryOut
    >
    static inline bool apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             GeometryOut & geometry_out,
                             Strategy const& strategy)
    {
        return resolve_collection::intersection
            <
                Geometry1, Geometry2, GeometryOut
            >::apply(geometry1, geometry2, geometry_out, strategy);
    }
};

template <typename Strategy>
struct intersection<Strategy, false>
{
    template
    <
        typename Geometry1,
        typename Geometry2,
        typename GeometryOut
    >
    static inline bool apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             GeometryOut & geometry_out,
                             Strategy const& strategy)
    {
        using strategies::relate::services::strategy_converter;
        return intersection
            <
                decltype(strategy_converter<Strategy>::get(strategy))
            >::apply(geometry1, geometry2, geometry_out,
                     strategy_converter<Strategy>::get(strategy));
    }
};

template <>
struct intersection<default_strategy, false>
{
    template
    <
        typename Geometry1,
        typename Geometry2,
        typename GeometryOut
    >
    static inline bool apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             GeometryOut & geometry_out,
                             default_strategy)
    {
        using strategy_type = typename strategies::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type;

        return intersection
            <
                strategy_type
            >::apply(geometry1, geometry2, geometry_out, strategy_type());
    }
};

} // resolve_strategy


namespace resolve_dynamic
{

template
<
    typename Geometry1, typename Geometry2,
    typename Tag1 = geometry::tag_t<Geometry1>,
    typename Tag2 = geometry::tag_t<Geometry2>
>
struct intersection
{
    template <typename GeometryOut, typename Strategy>
    static inline bool apply(Geometry1 const& geometry1, Geometry2 const& geometry2,
                             GeometryOut& geometry_out, Strategy const& strategy)
    {
        concepts::check<Geometry1 const>();
        concepts::check<Geometry2 const>();

        return resolve_strategy::intersection
            <
                Strategy
            >::apply(geometry1, geometry2, geometry_out, strategy);
    }
};


template <typename DynamicGeometry1, typename Geometry2, typename Tag2>
struct intersection<DynamicGeometry1, Geometry2, dynamic_geometry_tag, Tag2>
{
    template <typename GeometryOut, typename Strategy>
    static inline bool apply(DynamicGeometry1 const& geometry1, Geometry2 const& geometry2,
                             GeometryOut& geometry_out, Strategy const& strategy)
    {
        bool result = false;
        traits::visit<DynamicGeometry1>::apply([&](auto const& g1)
        {
            result = intersection
                <
                    util::remove_cref_t<decltype(g1)>,
                    Geometry2
                >::apply(g1, geometry2, geometry_out, strategy);
        }, geometry1);
        return result;
    }
};


template <typename Geometry1, typename DynamicGeometry2, typename Tag1>
struct intersection<Geometry1, DynamicGeometry2, Tag1, dynamic_geometry_tag>
{
    template <typename GeometryOut, typename Strategy>
    static inline bool apply(Geometry1 const& geometry1, DynamicGeometry2 const& geometry2,
                             GeometryOut& geometry_out, Strategy const& strategy)
    {
        bool result = false;
        traits::visit<DynamicGeometry2>::apply([&](auto const& g2)
        {
            result = intersection
                <
                    Geometry1,
                    util::remove_cref_t<decltype(g2)>
                >::apply(geometry1, g2, geometry_out, strategy);
        }, geometry2);
        return result;
    }
};


template <typename DynamicGeometry1, typename DynamicGeometry2>
struct intersection<DynamicGeometry1, DynamicGeometry2, dynamic_geometry_tag, dynamic_geometry_tag>
{
    template <typename GeometryOut, typename Strategy>
    static inline bool apply(DynamicGeometry1 const& geometry1, DynamicGeometry2 const& geometry2,
                             GeometryOut& geometry_out, Strategy const& strategy)
    {
        bool result = false;
        traits::visit<DynamicGeometry1, DynamicGeometry2>::apply([&](auto const& g1, auto const& g2)
        {
            result = intersection
                <
                    util::remove_cref_t<decltype(g1)>,
                    util::remove_cref_t<decltype(g2)>
                >::apply(g1, g2, geometry_out, strategy);
        }, geometry1, geometry2);
        return result;
    }
};

} // namespace resolve_dynamic


/*!
\brief \brief_calc2{intersection}
\ingroup intersection
\details \details_calc2{intersection, spatial set theoretic intersection}.
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\tparam GeometryOut Collection of geometries (e.g. std::vector, std::deque, boost::geometry::multi*) of which
    the value_type fulfills a \p_l_or_c concept, or it is the output geometry (e.g. for a box)
\tparam Strategy \tparam_strategy{Intersection}
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param geometry_out The output geometry, either a multi_point, multi_polygon,
    multi_linestring, or a box (for intersection of two boxes)
\param strategy \param_strategy{intersection}

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/intersection.qbk]}
*/
template
<
    typename Geometry1,
    typename Geometry2,
    typename GeometryOut,
    typename Strategy
>
inline bool intersection(Geometry1 const& geometry1,
                         Geometry2 const& geometry2,
                         GeometryOut& geometry_out,
                         Strategy const& strategy)
{
    return resolve_dynamic::intersection
        <
            Geometry1,
            Geometry2
        >::apply(geometry1, geometry2, geometry_out, strategy);
}


/*!
\brief \brief_calc2{intersection}
\ingroup intersection
\details \details_calc2{intersection, spatial set theoretic intersection}.
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\tparam GeometryOut Collection of geometries (e.g. std::vector, std::deque, boost::geometry::multi*) of which
    the value_type fulfills a \p_l_or_c concept, or it is the output geometry (e.g. for a box)
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param geometry_out The output geometry, either a multi_point, multi_polygon,
    multi_linestring, or a box (for intersection of two boxes)

\qbk{[include reference/algorithms/intersection.qbk]}
*/
template
<
    typename Geometry1,
    typename Geometry2,
    typename GeometryOut
>
inline bool intersection(Geometry1 const& geometry1,
                         Geometry2 const& geometry2,
                         GeometryOut& geometry_out)
{
    return resolve_dynamic::intersection
        <
            Geometry1,
            Geometry2
        >::apply(geometry1, geometry2, geometry_out, default_strategy());
}


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_INTERSECTION_INTERFACE_HPP
