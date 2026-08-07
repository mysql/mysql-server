//
// Copyright 2013-2026 Antony Polukhin.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_DETAIL_CONFIG_HPP
#define BOOST_TYPE_INDEX_DETAIL_CONFIG_HPP

#if !defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)
#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif
#endif

#ifdef BOOST_TYPE_INDEX_INTERFACE_UNIT
#   define BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT export {
#   define BOOST_TYPE_INDEX_END_MODULE_EXPORT }
#else
#   define BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT
#   define BOOST_TYPE_INDEX_END_MODULE_EXPORT
#endif

#if defined(BOOST_USE_MODULES) && !defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)
import boost.type_index;
#endif

#endif // BOOST_TYPE_INDEX_DETAIL_CONFIG_HPP

