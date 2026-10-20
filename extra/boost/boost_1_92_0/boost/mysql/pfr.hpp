//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PFR_HPP
#define BOOST_MYSQL_PFR_HPP

#include <boost/mysql/detail/config.hpp>

#include <boost/pfr/config.hpp>

#if BOOST_PFR_ENABLED && defined(BOOST_MYSQL_CXX14)

namespace boost {
namespace mysql {

/**
 * \brief Static interface marker type to use Boost.PFR reflection with positional matching.
 * \details
 * A marker type that satisfies the `StaticRow` concept.
 * When used within the static interface, modifies its behavior
 * for the marked type so that Boost.PFR is used for reflection, instead of Boost.Describe.
 * Field matching is performed by position, with the same algorithm used for `std::tuple`.
 * \n
 * The underlying row type for this marker is `T`, i.e.
 * \ref underlying_row_t "underlying_row_t<pfr_by_position<T>>" is an alias for `T`.
 * \n
 * The type `T` must be a PFR-reflectable non-const object type.
 * \n
 * This type is only defined if the macro `BOOST_PFR_ENABLED` is defined and set to `1`.
 */
template <class T>
struct pfr_by_position;

#if BOOST_PFR_CORE_NAME_ENABLED

/**
 * \brief Static interface marker type to use Boost.PFR reflection with name-based field matching.
 * \details
 * A marker type that satisfies the `StaticRow` concept.
 * When used within the static interface, modifies its behavior
 * for the marked type so that Boost.PFR is used for reflection, instead of Boost.Describe.
 * Field matching is performed by name, with the same algorithm used for Boost.Describe structs.
 * \n
 * The underlying row type for this marker is `T`, i.e.
 * \ref underlying_row_t "underlying_row_t<pfr_by_name<T>>" is an alias for `T`.
 * \n
 * The type `T` must be a PFR-reflectable non-const object type.
 * \n
 * This type is only defined if the macro `BOOST_PFR_CORE_NAME_ENABLED` is defined and set to `1`
 * (requires C++20).
 */
template <class T>
struct pfr_by_name;

#endif

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/pfr.hpp>

#endif

#endif
