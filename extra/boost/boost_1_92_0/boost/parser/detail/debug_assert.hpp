// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_DEBUG_ASSERT_HPP
#define BOOST_PARSER_DETAIL_DEBUG_ASSERT_HPP

#if __has_include(<boost/assert.hpp>)
#include <boost/assert.hpp>
#define BOOST_PARSER_DEBUG_ASSERT(condition) BOOST_ASSERT(condition)
#define BOOST_PARSER_HAVE_BOOST_ASSERT
#elif defined(BOOST_DISABLE_ASSERTS)
#define BOOST_PARSER_DEBUG_ASSERT(condition) ((void)0)
#else
#include <cassert>
#define BOOST_PARSER_DEBUG_ASSERT(condition) assert(condition)
#endif

#endif
