// Copyright (c) 2016-2026 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_PFR_TUPLE_SIZE_HPP
#define BOOST_PFR_TUPLE_SIZE_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_PFR_INTERFACE_UNIT)

#include <boost/pfr/detail/sequence_tuple.hpp>
#include <boost/pfr/detail/fields_count.hpp>

#if !defined(BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>
#include <utility>      // metaprogramming stuff
#endif

/// \file boost/pfr/tuple_size.hpp
/// Contains tuple-like interfaces to get fields count \forcedlink{tuple_size}, \forcedlink{tuple_size_v}.
///
/// \b Synopsis:
namespace boost { namespace pfr {

BOOST_PFR_BEGIN_MODULE_EXPORT

/// Has a static const member variable `value` that contains fields count in a T.
/// Works for any T that satisfies \aggregate.
///
/// \b Example:
/// \code
///     std::array<int, boost::pfr::tuple_size<my_structure>::value > a;
/// \endcode
template <class T>
using tuple_size = detail::size_t_< boost::pfr::detail::fields_count<T>() >;


/// `tuple_size_v` is a template variable that contains fields count in a T and
/// works for any T that satisfies \aggregate.
///
/// \b Example:
/// \code
///     std::array<int, boost::pfr::tuple_size_v<my_structure> > a;
/// \endcode
template <class T>
constexpr std::size_t tuple_size_v = tuple_size<T>::value;

BOOST_PFR_END_MODULE_EXPORT

}} // namespace boost::pfr

#endif  // #if defined(BOOST_USE_MODULES) && !defined(BOOST_PFR_INTERFACE_UNIT)

#endif // BOOST_PFR_TUPLE_SIZE_HPP
