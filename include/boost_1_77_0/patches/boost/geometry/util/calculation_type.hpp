// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2012 Bruno Lalande, Paris, France.
// Copyright (c) 2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2018-2021.
// Modifications Copyright (c) 2018, 2023, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_UTIL_CALCULATION_TYPE_HPP
#define BOOST_GEOMETRY_UTIL_CALCULATION_TYPE_HPP


#include <type_traits>

#include <boost/static_assert.hpp>

#include <boost/geometry/util/select_coordinate_type.hpp>
#include <boost/geometry/util/select_most_precise.hpp>

#include <boost/multiprecision/cpp_int.hpp>


namespace boost { namespace geometry
{

namespace util
{

namespace detail
{

struct default_integral
{
    typedef long long type;
};

template <typename Type>
struct is_multiprecision_integral
    : boost::false_type
{};

template
<
    unsigned MinBits, unsigned MaxBits,
    boost::multiprecision::cpp_integer_type SignType,
    boost::multiprecision::cpp_int_check_type Checked,
    class Allocator
>
struct is_multiprecision_integral
    <
        boost::multiprecision::number
            <
                boost::multiprecision::cpp_int_backend
                    <
                        MinBits, MaxBits,
                        SignType,
                        Checked,
                        Allocator
                    >
            >
    >
    : boost::true_type
{};

/*!
\details Selects the most appropriate:
    - if calculation type is specified (not void), that one is used
    - else if type is non-fundamental (user defined e.g. Boost.Multiprecision), that one
    - else if type is floating point, the specified default FP is used
    - else it is integral and the specified default integral is used
 */
template
<
    typename Type,
    typename CalculationType,
    typename DefaultFloatingPointCalculationType,
    typename DefaultIntegralCalculationType
>
struct calculation_type
{
    BOOST_STATIC_ASSERT((
        std::is_fundamental
            <
                DefaultFloatingPointCalculationType
            >::value
        ));
    BOOST_STATIC_ASSERT((
        std::is_fundamental
            <
                DefaultIntegralCalculationType
            >::value
        ));


    typedef std::conditional_t
        <
            std::is_void<CalculationType>::value,
            std::conditional_t
                <
                    std::is_floating_point<Type>::value,
                    typename select_most_precise
                        <
                            DefaultFloatingPointCalculationType,
                            Type
                        >::type,
                    std::conditional_t
                        <
                            is_multiprecision_integral<Type>::value,
                            // TODO: This is not fully correct since Multiprecision type
                            // will most likely be more precise than the DefaultIntegralCalculationType
                            // but the problem is that DefaultIntegralCalculationType is not always
                            // integral. This is a workaround for comparable_distance() calculation_type
                            // implemetation passing double here. E.g. checking a static_assert above
                            // boost::is_integral<DefaultIntegralCalculationType>::value would be a start.
                            DefaultIntegralCalculationType,
                            typename select_most_precise
                                <
                                    DefaultIntegralCalculationType,
                                    Type
                                >::type
                        >
                >,
            CalculationType
        > type;
};

} // namespace detail


namespace calculation_type
{

namespace geometric
{

template
<
    typename Geometry,
    typename CalculationType,
    typename DefaultFloatingPointCalculationType = double,
    typename DefaultIntegralCalculationType = detail::default_integral::type
>
struct unary
{
    typedef typename detail::calculation_type
        <
            typename geometry::coordinate_type<Geometry>::type,
            CalculationType,
            DefaultFloatingPointCalculationType,
            DefaultIntegralCalculationType
        >::type type;
};

template
<
    typename Geometry1,
    typename Geometry2,
    typename CalculationType,
    typename DefaultFloatingPointCalculationType = double,
    typename DefaultIntegralCalculationType = detail::default_integral::type
>
struct binary
{
    typedef typename detail::calculation_type
        <
            typename select_coordinate_type<Geometry1, Geometry2>::type,
            CalculationType,
            DefaultFloatingPointCalculationType,
            DefaultIntegralCalculationType
        >::type type;
};


/*!
\brief calculation type (ternary, for three geometry types)
 */
template
<
    typename Geometry1,
    typename Geometry2,
    typename Geometry3,
    typename CalculationType,
    typename DefaultFloatingPointCalculationType = double,
    typename DefaultIntegralCalculationType = detail::default_integral::type
>
struct ternary
{
    typedef typename detail::calculation_type
        <
            typename select_most_precise
                <
                    typename coordinate_type<Geometry1>::type,
                    typename select_coordinate_type
                        <
                            Geometry2,
                            Geometry3
                        >::type
                >::type,
            CalculationType,
            DefaultFloatingPointCalculationType,
            DefaultIntegralCalculationType
        >::type type;
};

}} // namespace calculation_type::geometric

} // namespace util

}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_UTIL_CALCULATION_TYPE_HPP
