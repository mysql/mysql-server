// Copyright Antony Polukhin, 2021-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LEXICAL_CAST_DETAIL_CONFIG_HPP
#define BOOST_LEXICAL_CAST_DETAIL_CONFIG_HPP

#ifdef BOOST_LEXICAL_CAST_INTERFACE_UNIT
#   define BOOST_LEXICAL_CAST_BEGIN_MODULE_EXPORT export {
#   define BOOST_LEXICAL_CAST_END_MODULE_EXPORT }
#else
#   define BOOST_LEXICAL_CAST_BEGIN_MODULE_EXPORT
#   define BOOST_LEXICAL_CAST_END_MODULE_EXPORT
#endif

#if defined(BOOST_USE_MODULES) && !defined(BOOST_LEXICAL_CAST_INTERFACE_UNIT)
import boost.lexical_cast;
#endif

#endif  // #ifndef BOOST_LEXICAL_CAST_DETAIL_CONFIG_HPP
