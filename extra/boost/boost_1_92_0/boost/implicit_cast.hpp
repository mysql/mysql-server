//  boost implicit_cast.hpp header file  ----------------------------------------------//
//  (C) Copyright David Abrahams, 2003.
//  (C) Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_IMPLICIT_CAST_DWA200356_HPP
#define BOOST_IMPLICIT_CAST_DWA200356_HPP

#include <boost/conversion/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_CONVERSION_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

namespace boost {

namespace detail {

template<class T> struct icast_identity
{
    using type = T;
};

} // namespace detail

BOOST_CONVERSION_BEGIN_MODULE_EXPORT

// implementation originally suggested by C. Green in
// http://lists.boost.org/MailArchives/boost/msg00886.php

// The use of identity creates a non-deduced form, so that the
// explicit template argument must be supplied
template <typename T>
constexpr T implicit_cast (typename boost::detail::icast_identity<T>::type x) {
    return x;
}

BOOST_CONVERSION_END_MODULE_EXPORT

} // namespace boost

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_CONVERSION_INTERFACE_UNIT)

#endif // BOOST_IMPLICIT_CAST_DWA200356_HPP
