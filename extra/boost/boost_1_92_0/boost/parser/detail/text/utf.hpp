// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_UTF_HPP
#define BOOST_PARSER_DETAIL_TEXT_UTF_HPP

#include <boost/parser/detail/text/config.hpp>

#include <cstdint>
#include <type_traits>
#include <cstdint>


namespace boost::parser::detail { namespace text {

    /** The Unicode Transformation Formats. */
    enum class format { none = 0, utf8 = 1, utf16 = 2, utf32 = 4 };

    namespace detail {
        template<typename T>
        constexpr format format_of()
        {
            if constexpr (
                std::is_same_v<T, char>
#if defined(__cpp_char8_t)
                || std::is_same_v<T, char8_t>
#endif
            ) {
                return format::utf8;
            } else if (
                std::is_same_v<T, char16_t>
#ifdef _MSC_VER
                || std::is_same_v<T, wchar_t>
#endif
            ) {
                return format::utf16;
            } else {
                return format::utf32;
            }
        }
    }

}}

#endif
