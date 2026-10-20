/* Copyright 2016-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_IS_FINAL_HPP
#define BOOST_POLY_COLLECTION_DETAIL_IS_FINAL_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <type_traits>

#if __cpp_lib_is_final>=201402L
namespace boost{

namespace poly_collection{

namespace detail{

template<typename T> using is_final=std::is_final<T>;

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */
#else
#include <boost/type_traits/is_final.hpp>

namespace boost{

namespace poly_collection{

namespace detail{

template<typename T> using is_final=boost::is_final<T>;

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */
#endif

#endif
