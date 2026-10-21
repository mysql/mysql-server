// Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_DETAIL_CONFIG_HPP
#define BOOST_DLL_DETAIL_CONFIG_HPP

#if !defined(BOOST_DLL_INTERFACE_UNIT)
# include <boost/config.hpp>
# ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
# endif
#endif

#ifdef BOOST_DLL_INTERFACE_UNIT
# define BOOST_DLL_BEGIN_MODULE_EXPORT export {
# define BOOST_DLL_END_MODULE_EXPORT }
#else
# define BOOST_DLL_BEGIN_MODULE_EXPORT
# define BOOST_DLL_END_MODULE_EXPORT
#endif

#if defined(BOOST_USE_MODULES) && !defined(BOOST_DLL_INTERFACE_UNIT)
import boost.dll;
#endif

#endif // BOOST_DLL_DETAIL_CONFIG_HPP
