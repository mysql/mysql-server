//  boost polymorphic_pointer_cast.hpp header file  ----------------------------------------------//
//  (C) Copyright Boris Rasin, 2014-2021.
//  (C) Copyright Antony Polukhin, 2014-2026.
//  (C) Copyright Fedor Osetrov, 2025-2026.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/conversion for Documentation.

#ifndef BOOST_CONVERSION_POLYMORPHIC_POINTER_CAST_HPP
#define BOOST_CONVERSION_POLYMORPHIC_POINTER_CAST_HPP

#include <boost/conversion/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_CONVERSION_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if !defined(BOOST_CONVERSION_INTERFACE_UNIT)
# include <boost/assert.hpp>
# include <boost/pointer_cast.hpp>
# include <boost/throw_exception.hpp>
#endif

BOOST_CONVERSION_BEGIN_MODULE_EXPORT
namespace boost
{
//  See the documentation for descriptions of how to choose between
//  static_pointer_cast<>, dynamic_pointer_cast<>, polymorphic_pointer_cast<> and polymorphic_pointer_downcast<>

//  polymorphic_pointer_downcast  --------------------------------------------//

    //  BOOST_ASSERT() checked polymorphic downcast.  Crosscasts prohibited.
    //  Supports any type with static_pointer_cast/dynamic_pointer_cast functions:
    //  built-in pointers, std::shared_ptr, boost::shared_ptr, boost::intrusive_ptr, etc.

    //  WARNING: Because this cast uses BOOST_ASSERT(), it violates
    //  the One Definition Rule if used in multiple translation units
    //  where BOOST_DISABLE_ASSERTS, BOOST_ENABLE_ASSERT_HANDLER
    //  NDEBUG are defined inconsistently.

    //  Contributed by Boris Rasin

    template <typename Target, typename Source>
    inline auto polymorphic_pointer_downcast (const Source& x)
        -> decltype(static_pointer_cast<Target>(x))
    {
        BOOST_ASSERT(dynamic_pointer_cast<Target> (x) == x);
        return static_pointer_cast<Target> (x);
    }

    template <typename Target, typename Source>
    inline auto polymorphic_pointer_cast (const Source& x)
        -> decltype(dynamic_pointer_cast<Target>(x))
    {
        auto tmp = dynamic_pointer_cast<Target> (x);
        if ( !tmp ) boost::throw_exception( std::bad_cast() );
        return tmp;
    }

} // namespace boost
BOOST_CONVERSION_END_MODULE_EXPORT

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_CONVERSION_INTERFACE_UNIT)

#endif  // BOOST_CONVERSION_POLYMORPHIC_POINTER_CAST_HPP
