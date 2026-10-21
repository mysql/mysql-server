// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_STL_INTERFACES_CONFIG_HPP
#define BOOST_PARSER_DETAIL_STL_INTERFACES_CONFIG_HPP

#include <boost/parser/config.hpp>


#if !BOOST_PARSER_USE_CONCEPTS
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_USE_CONCEPTS 0
#else
// This is now hard-coded to use the pre-C++20 code path.  There are a bunch
// of really odd compile errorswith Clang+libstdc++ I can't be bothered to
// address right now.
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_USE_CONCEPTS 0
#endif

#if defined(__cpp_explicit_this_parameter) && BOOST_PARSER_DETAIL_STL_INTERFACES_USE_CONCEPTS
#define BOOST_PARSER_USE_DEDUCED_THIS 1
#else
#define BOOST_PARSER_USE_DEDUCED_THIS 0
#endif

// The inline namespaces v1, v2, and v3 represent C++14, C++20, and C++23 and
// later, respectively.  v1 is inline for standards before C++20, and v2 is
// inline for C++20 and later.  Note that this only applies to code for which
// multiple vI namespace alternatives exist.  For example, some instances of
// the v1 namespace may still be inline, if there is no v2 version of its
// contents.
#if !BOOST_PARSER_DETAIL_STL_INTERFACES_USE_CONCEPTS && !BOOST_PARSER_USE_DEDUCED_THIS
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V1 inline namespace v1
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V2 namespace v2
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V3 namespace v3
#elif BOOST_PARSER_DETAIL_STL_INTERFACES_USE_CONCEPTS && !BOOST_PARSER_USE_DEDUCED_THIS
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V1 namespace v1
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V2 inline namespace v2
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V3 namespace v3
#else
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V1 namespace v1
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V2 namespace v2
#    define BOOST_PARSER_DETAIL_STL_INTERFACES_NAMESPACE_V3 inline namespace v3
#endif

#endif
