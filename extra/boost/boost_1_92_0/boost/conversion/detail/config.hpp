// Copyright (c) 2025-2026 Antony Polukhin
// Copyright (c) 2025-2026 Fedor Osetrov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONVERSION_DETAIL_CONFIG_HPP
#define BOOST_CONVERSION_DETAIL_CONFIG_HPP

#if !defined(BOOST_CONVERSION_INTERFACE_UNIT)
# include <boost/config.hpp>
# ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
# endif
#endif

#ifdef BOOST_CONVERSION_INTERFACE_UNIT
# define BOOST_CONVERSION_BEGIN_MODULE_EXPORT export {
# define BOOST_CONVERSION_END_MODULE_EXPORT }
#else
# define BOOST_CONVERSION_BEGIN_MODULE_EXPORT
# define BOOST_CONVERSION_END_MODULE_EXPORT
#endif

#if defined(BOOST_USE_MODULES) && !defined(BOOST_CONVERSION_INTERFACE_UNIT)
import boost.conversion;
#endif

#endif
