//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_STD_FWD_HPP
#define BOOST_CONTAINER_DETAIL_STD_FWD_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

//////////////////////////////////////////////////////////////////////////////
//                        Standard predeclarations
//////////////////////////////////////////////////////////////////////////////

#include <cstddef>

//<version> is needed to reliably query the C++23 library feature-test macros
//for ::std::from_range_t / P1206R7. It is one of the lightest standard headers.
#if defined(__has_include)
#  if __has_include(<version>)
#     include <version>
#  endif
#elif BOOST_CXX_VERSION >= 202002L
#  include <version>
#endif

// Whether ::std::from_range_t tag is provided by the standard library.
//
// The official feature-test macro for std::from_range_t is
// __cpp_lib_containers_ranges. However, std::from_range_t lives in <ranges>
// next to std::ranges::to, whose specification uses the tag, so any library
// that provides std::ranges::to necessarily provides std::from_range_t too.
//
// Some standard libraries (e.g. libc++ 17/18) ship std::ranges::to and
// std::from_range_t -- defining __cpp_lib_ranges_to_container -- before they
// complete the container support and define __cpp_lib_containers_ranges.
// In that window only __cpp_lib_ranges_to_container is defined even though the
// tag is already present.
#if defined(__cpp_lib_ranges_to_container) || defined(__cpp_lib_containers_ranges)
#  define BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT
#endif

#include <boost/move/detail/std_ns_begin.hpp>
BOOST_MOVE_STD_NS_BEG

template<class T>
class allocator;

template<class T>
struct less;

template<class T>
struct equal_to;

template<class T1, class T2>
struct pair;

template<class T>
struct char_traits;

struct input_iterator_tag;
struct forward_iterator_tag;
struct bidirectional_iterator_tag;
struct random_access_iterator_tag;

template<class Container>
class insert_iterator;

struct allocator_arg_t;

struct piecewise_construct_t;

template <class Ptr>
struct pointer_traits;

//Only forward declare ::std::from_range_t when the standard library actually
//provides it; see BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT above.
#if defined(BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT)
struct from_range_t;
#endif

BOOST_MOVE_STD_NS_END
#include <boost/move/detail/std_ns_end.hpp>

#if defined(__cpp_aligned_new)

//align_val_t is not usually in an inline namespace
namespace std {

enum class align_val_t : std::size_t;

}  //namespace std

#endif   //defined(__cpp_aligned_new)

#endif //#ifndef BOOST_CONTAINER_DETAIL_STD_FWD_HPP
