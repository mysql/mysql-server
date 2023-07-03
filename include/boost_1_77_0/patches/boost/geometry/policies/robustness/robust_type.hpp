// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014 Bruno Lalande, Paris, France.
// Copyright (c) 2014 Mateusz Loskot, London, UK.
// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2020-2021.
// Modifications Copyright (c) 2020, 2023, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_POLICIES_ROBUSTNESS_ROBUST_TYPE_HPP
#define BOOST_GEOMETRY_POLICIES_ROBUSTNESS_ROBUST_TYPE_HPP


#include <type_traits>

#include <boost/config.hpp>


#define BOOST_GEOMETRY_ROBUST_TYPE_USE_MULTIPRECISION


//#ifdef BOOST_GEOMETRY_ROBUST_TYPE_USE_MULTIPRECISION

#include <boost/multiprecision/cpp_int.hpp>

//#endif


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL

namespace detail
{

#ifndef BOOST_GEOMETRY_ROBUST_TYPE_USE_MULTIPRECISION

typedef boost::long_long_type robust_signed_integral_type;

#else

typedef boost::multiprecision::number
    <
        boost::multiprecision::cpp_int_backend
            <
                64, 256,
                boost::multiprecision::signed_magnitude,
                boost::multiprecision::unchecked,
                void
            >
    > robust_signed_integral_type;

#endif

}

namespace detail_dispatch
{

template <typename CoordinateType, typename IsFloatingPoint>
struct robust_type
{
};

template <typename CoordinateType>
struct robust_type<CoordinateType, std::false_type>
{
    typedef CoordinateType type;
};

template <typename CoordinateType>
struct robust_type<CoordinateType, std::true_type>
{
    typedef geometry::detail::robust_signed_integral_type type;
};

} // namespace detail_dispatch

namespace detail
{

template <typename CoordinateType>
struct robust_type
{
    typedef typename detail_dispatch::robust_type
        <
            CoordinateType,
            typename std::is_floating_point<CoordinateType>::type
        >::type type;
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_POLICIES_ROBUSTNESS_ROBUST_TYPE_HPP
