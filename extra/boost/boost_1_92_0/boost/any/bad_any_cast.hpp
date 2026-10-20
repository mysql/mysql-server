// Copyright Antony Polukhin, 2020-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org/libs/any for Documentation.

#ifndef BOOST_ANYS_BAD_ANY_CAST_HPP_INCLUDED
#define BOOST_ANYS_BAD_ANY_CAST_HPP_INCLUDED

#include <boost/any/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_ANY_INTERFACE_UNIT)

#ifndef BOOST_ANY_INTERFACE_UNIT
#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#ifndef BOOST_NO_RTTI
#include <typeinfo>
#endif

#include <stdexcept>
#endif  // #ifndef BOOST_ANY_INTERFACE_UNIT

namespace boost {

BOOST_ANY_BEGIN_MODULE_EXPORT

/// The exception thrown in the event of a failed boost::any_cast of
/// an boost::any, boost::anys::basic_any or boost::anys::unique_any value.
class BOOST_SYMBOL_VISIBLE bad_any_cast :
#ifndef BOOST_NO_RTTI
    public std::bad_cast
#else
    public std::exception
#endif
{
public:
    const char * what() const BOOST_NOEXCEPT_OR_NOTHROW override
    {
        return "boost::bad_any_cast: "
               "failed conversion using boost::any_cast";
    }
};

BOOST_ANY_END_MODULE_EXPORT

} // namespace boost

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_ANY_INTERFACE_UNIT)

#endif // #ifndef BOOST_ANYS_BAD_ANY_CAST_HPP_INCLUDED
