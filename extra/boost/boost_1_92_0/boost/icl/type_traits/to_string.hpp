/*-----------------------------------------------------------------------------+
Copyright (c) 2007-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
Copyright (c) 1999-2006: Cortex Software GmbH, Kantstrasse 57, Berlin
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/

#ifndef BOOST_ICL_TO_STRING_HPP_JOFA_000712
#define BOOST_ICL_TO_STRING_HPP_JOFA_000712

#include <stdio.h>
#include <string>
#include <sstream>
#include <type_traits>
#include <utility>
#include <iterator>

namespace boost{ namespace icl
{    

// Helper: detect streamability: whether "std::declval<std::ostream&>() << std::declval<T>()"
// is a valid expression.
template <class T, class = void>
struct is_streamable : std::false_type {};

template <class T>
struct is_streamable<T, decltype(void(std::declval<std::ostream&>() << std::declval<T>()))>
    : std::true_type {};

// Helper: detect iterable (has begin/end via ADL or std::begin/std::end)
template <class T, class = void>
struct is_iterable : std::false_type {};

template <class T>
struct is_iterable<T, decltype(void(std::begin(std::declval<T>()), std::end(std::declval<T>())))>
    : std::true_type {};

// convenience alias
template <bool B>
using enable_if_t = typename std::enable_if<B, void>::type;

/// Primary template: fallback for unprintable types
template <class Type, class Enable = void>
struct to_string
{
    static std::string apply(const Type&)
    {
        return std::string("<unprintable>");
    }
};

// Specialization for types that are streamable
template <class Type>
struct to_string<Type, enable_if_t<is_streamable<Type>::value> >
{
    static std::string apply(const Type& value)
    {
        std::stringstream repr;
        repr << value;
        return repr.str();
    }
};

// Specialization for iterable types that are not streamable
// (we print their elements using recursive to_string)
template <class Type>
struct to_string<Type, enable_if_t<!is_streamable<Type>::value && is_iterable<Type>::value> >
{
    static std::string apply(const Type& value)
    {
        std::stringstream repr;
        repr << "{";

        auto it = std::begin(value);
        auto end = std::end(value);
        bool first = true;
        for (; it != end; ++it)
        {
            if (!first) repr << ", ";
            first = false;
            // element may be a reference type; pass by const-ref
            using ElemT = typename std::decay<decltype(*it)>::type;
            repr << boost::icl::to_string<ElemT>::apply(*it);
        }

        repr << "}";
        return repr.str();
    }
};

}} // namespace boost::icl

#endif // BOOST_ICL_TO_STRING_HPP_JOFA_000712