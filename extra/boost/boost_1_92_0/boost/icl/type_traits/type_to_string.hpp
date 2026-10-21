/*-----------------------------------------------------------------------------+
Copyright (c) 2007-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_TYPE_TO_STRING_HPP_JOFA_080416
#define BOOST_ICL_TYPE_TO_STRING_HPP_JOFA_080416

#include <stdio.h>
#include <string>
#include <sstream>

#include <tuple>
#include <type_traits>

#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_float.hpp>
#include <boost/mpl/if.hpp>

namespace boost{ namespace icl
{
    // Primary template: note the additional defaulted Enable parameter so we can SFINAE
    template<class Type, class Enable = void>
    struct type_to_string
    {
        /** Convert the type to its typestring */
        static std::string apply();
    };

    //--------------------------------------------------------------------------
    // Explicit specializations for builtins (Enable defaults to void)
    template<> inline std::string type_to_string<bool, void>::apply() { return "bool"; }
    template<> inline std::string type_to_string<char, void>::apply() { return "char"; }
    template<> inline std::string type_to_string<short, void>::apply(){ return "short"; }
    template<> inline std::string type_to_string<int, void>::apply()  { return "int"; }
    template<> inline std::string type_to_string<long, void>::apply() { return "long"; }
    template<> inline std::string type_to_string<long long, void>::apply(){ return "Long"; }

    template<> inline std::string type_to_string<unsigned char, void>::apply(){ return "char+"; }
    template<> inline std::string type_to_string<unsigned short, void>::apply(){ return "short+"; }
    template<> inline std::string type_to_string<unsigned int, void>::apply()  { return "int+"; }
    template<> inline std::string type_to_string<unsigned long, void>::apply() { return "long+"; }
    template<> inline std::string type_to_string<unsigned long long, void>::apply(){ return "Long+"; }

    template<> inline std::string type_to_string<float, void>::apply() { return "flt"; }
    template<> inline std::string type_to_string<double, void>::apply() { return "dbl"; }

    //-------------------------------------------------------------------------
    template<template<class...> class Templ>
    struct unary_template_to_string
    {
        static std::string apply();
    };

    // ---------------------------------------------------------------------------
    template<template<class...> class Templ>
    struct binary_template_to_string
    {
        static std::string apply();
    };

    // ---------------------------------------------------------------------------
    template<>
    struct type_to_string<std::string, void>
    {
        static std::string apply() { return "string"; }
    };

    // =======================================================================
    // Constrained partial specializations:
    // - unary: matches only when the template instantiation has exactly 1 type parameter
    // - binary: matches only when the template instantiation has exactly 2 type parameters
    //
    // These use template<template<class...> class ...> + sizeof...(Args) to SFINAE-out
    // matches for templates with different number of type-parameters (including those
    // that have additional defaulted parameters).
    // =======================================================================

    // unary: exactly one type parameter (C++11-compatible enable_if)
    template< template<class...> class Unary, class... Args >
    struct type_to_string< Unary<Args...>,
        typename std::enable_if< (sizeof...(Args) == 1), void >::type
    >
    {
        using First = typename std::tuple_element<0, std::tuple<Args...> >::type;

        static std::string apply()
        {
            return unary_template_to_string<Unary>::apply()
                + "<" + type_to_string<First>::apply() + ">";
        }
    };

    // binary: exactly two type parameters (C++11-compatible enable_if)
    template< template<class...> class Binary, class... Args >
    struct type_to_string< Binary<Args...>,
        typename std::enable_if< (sizeof...(Args) == 2), void >::type
    >
    {
        using First  = typename std::tuple_element<0, std::tuple<Args...> >::type;
        using Second = typename std::tuple_element<1, std::tuple<Args...> >::type;

        static std::string apply()
        {
            return binary_template_to_string<Binary>::apply()
                + "<" + type_to_string<First>::apply()
                + ","  + type_to_string<Second>::apply() + ">";
        }
    };

}} // namespace boost icl

#endif
