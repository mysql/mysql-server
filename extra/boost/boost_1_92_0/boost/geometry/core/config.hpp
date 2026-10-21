// Boost.Geometry

// Copyright (c) 2019-2021 Barend Gehrels, Amsterdam, the Netherlands.

// Copyright (c) 2018-2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_CORE_CONFIG_HPP
#define BOOST_GEOMETRY_CORE_CONFIG_HPP

#include <boost/config.hpp>

#if    defined(BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_BY_TRIANGLE) \
    && defined(BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_ROBUST)
#error "Both BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_BY_TRIANGLE" \
    " and BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_ROBUST are defined." \
    " Only one of them should be defined."
#endif

// Define default side strategy, if not defined by the user.
// Until Boost 1.88.0, the default strategy is side_by_triangle.
#if    ! defined(BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_BY_TRIANGLE) \
    && ! defined(BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_ROBUST)
#define BOOST_GEOMETRY_DEFAULT_STRATEGY_SIDE_USE_SIDE_BY_TRIANGLE
#endif

#endif // BOOST_GEOMETRY_CORE_CONFIG_HPP
