/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   predicates/wrap_filter.hpp
 * \author Andrey Semashev
 * \date   09.07.2025
 *
 * The header contains a filter function wrapper that enables third-party functions to participate in filtering expressions.
 */

#ifndef BOOST_LOG_EXPRESSIONS_PREDICATES_WRAP_FILTER_HPP_INCLUDED_
#define BOOST_LOG_EXPRESSIONS_PREDICATES_WRAP_FILTER_HPP_INCLUDED_

#include <type_traits>
#include <boost/phoenix/core/actor.hpp>
#include <boost/log/detail/config.hpp>
#include <boost/log/detail/unary_function_terminal.hpp>
#include <boost/log/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace expressions {

/*!
 * The function wraps a function object in order it to be able to participate in filtering expressions. The wrapped function must be
 * compatible with the following signature:
 *
 * <pre>
 * bool (attribute_value_set const& values) const
 * </pre>
 *
 * The wrapped function must return \c true if the log record is to be passed by the filter and \c false otherwise.
 */
template< typename FunT >
BOOST_FORCEINLINE phoenix::actor<
    aux::unary_function_terminal< typename std::remove_cv< typename std::remove_reference< FunT >::type >::type >
> wrap_filter(FunT&& fun)
{
    typedef aux::unary_function_terminal< typename std::remove_cv< typename std::remove_reference< FunT >::type >::type > terminal_type;
    phoenix::actor< terminal_type > act = {{ terminal_type(static_cast< FunT&& >(fun)) }};
    return act;
}

} // namespace expressions

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#include <boost/log/detail/footer.hpp>

#endif // BOOST_LOG_EXPRESSIONS_PREDICATES_WRAP_FILTER_HPP_INCLUDED_
