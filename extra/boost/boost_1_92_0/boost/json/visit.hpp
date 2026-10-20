//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_VISIT_HPP
#define BOOST_JSON_VISIT_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/value.hpp>
#include <utility>

namespace boost {
namespace json {

/** Invoke a function object with the contents of a @ref value.

    Invokes `v` as if by `std::forward<Visitor>(v)( X )`, where `X` is
    a `const` **(2)** or mutable **(1)** lvalue reference, or an rvalue
    reference **(3)** to

    @li the underlying stored value, if `jv.kind()` is not equal to
        @ref json::kind::null; or
    @li an object of type @ref std::nullptr_t.

    @returns The result of calling `v`.

    @param v The visitation function to invoke.
    @param jv The value to visit.

    @{
*/
template<class Visitor>
auto
visit(
    Visitor&& v,
    value& jv)
#ifndef BOOST_JSON_DOCS
        -> decltype(
                static_cast<Visitor&&>(v)( std::declval<std::nullptr_t&>() ) )
#else
        -> __see_below__
#endif
        ;

template<class Visitor>
auto
visit(
    Visitor &&v,
    value const &jv)
#ifndef BOOST_JSON_DOCS
        -> decltype(
                static_cast<Visitor&&>(v)( std::declval<std::nullptr_t const&>() ) )
#else
        -> __see_below__
#endif
        ;

template<class Visitor>
auto
visit(
    Visitor &&v,
    value&& jv)
#ifndef BOOST_JSON_DOCS
        -> decltype(
            static_cast<Visitor&&>(v)( std::declval<std::nullptr_t&&>() ) )
#else
        -> __see_below__
#endif
        ;
/// @}

} // namespace json
} // namespace boost

#include <boost/json/impl/visit.hpp>

#endif
