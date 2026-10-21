// Copyright (c) 2022 Denis Mikhailov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PFR_DETAIL_TRAITS_FWD_HPP
#define BOOST_PFR_DETAIL_TRAITS_FWD_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_PFR_INTERFACE_UNIT)

namespace boost { namespace pfr {

BOOST_PFR_BEGIN_MODULE_EXPORT

template<class T, class WhatFor>
struct is_reflectable;

BOOST_PFR_END_MODULE_EXPORT

}} // namespace boost::pfr

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_PFR_INTERFACE_UNIT)

#endif // BOOST_PFR_DETAIL_TRAITS_FWD_HPP

