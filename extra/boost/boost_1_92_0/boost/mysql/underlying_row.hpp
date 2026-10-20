//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_UNDERLYING_ROW_HPP
#define BOOST_MYSQL_UNDERLYING_ROW_HPP

#include <boost/mysql/detail/typing/row_traits.hpp>

#ifdef BOOST_MYSQL_CXX14

namespace boost {
namespace mysql {

/**
 * \brief Type trait to retrieve the underlying row type.
 * \details
 * Given an input type `T` satisfying the `StaticRow` concept,
 * this trait is an alias for its underlying row type. It is defined as follows:
 * \n
 * \li If `T` is a marker type, like \ref pfr_by_name "pfr_by_name< U >", `underlying_row_t`
 *     is an alias for the marker's inner type `U`.
 * \li If `T` is not a marker type (e.g. it's a Boost.Describe struct or a `std::tuple`),
 *     `underlying_row_t` is an alias for `T`.
 *
 * For instance, \ref static_results::rows uses this trait to determine its return type.
 */
template <BOOST_MYSQL_STATIC_ROW StaticRow>
using underlying_row_t =
#ifdef BOOST_MYSQL_DOXYGEN
    __see_below__
#else
    detail::underlying_row_t<StaticRow>
#endif
    ;

}  // namespace mysql
}  // namespace boost

#endif

#endif
