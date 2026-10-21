// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_CONCEPTS_HPP
#define BOOST_PARSER_DETAIL_TEXT_CONCEPTS_HPP

#include <boost/parser/detail/text/config.hpp>
#include <boost/parser/detail/text/utf.hpp>
#include <boost/parser/detail/text/detail/begin_end.hpp>

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS

#include <ranges>
#include <string_view>


namespace boost::parser::detail { namespace text { BOOST_PARSER_DETAIL_TEXT_NAMESPACE_V2 {

    //[ concepts_concepts

#ifdef _MSC_VER
    inline constexpr format wchar_t_format = format::utf16;
#else
    inline constexpr format wchar_t_format = format::utf32;
#endif

    template<typename T, format F>
    concept code_unit = (std::same_as<T, char8_t> && F == format::utf8) ||
                        (std::same_as<T, char16_t> && F == format::utf16) ||
                        (std::same_as<T, char32_t> && F == format::utf32) ||
                        (std::same_as<T, char> && F == format::utf8) ||
                        (std::same_as<T, wchar_t> && F == wchar_t_format);

    template<typename T>
    concept utf8_code_unit = code_unit<T, format::utf8>;

    template<typename T>
    concept utf16_code_unit = code_unit<T, format::utf16>;

    template<typename T>
    concept utf32_code_unit = code_unit<T, format::utf32>;

    template<typename T, format F>
    concept code_unit_iter =
        std::input_iterator<T> && code_unit<std::iter_value_t<T>, F>;

    template<typename T>
    concept utf_code_unit =
        utf8_code_unit<T> || utf16_code_unit<T> || utf32_code_unit<T>;


    template<typename T, format F>
    concept code_unit_range = std::ranges::input_range<T> &&
        code_unit<std::ranges::range_value_t<T>, F>;

    template<typename T, format F>
    concept contiguous_code_unit_range = std::ranges::contiguous_range<T> &&
        code_unit<std::ranges::range_value_t<T>, F>;

    template<typename T>
    concept utf8_iter = code_unit_iter<T, format::utf8>;
    template<typename T>
    concept utf8_range = code_unit_range<T, format::utf8>;
    template<typename T>
    concept contiguous_utf8_range = contiguous_code_unit_range<T, format::utf8>;

    template<typename T>
    concept utf16_iter = code_unit_iter<T, format::utf16>;
    template<typename T>
    concept utf16_range = code_unit_range<T, format::utf16>;
    template<typename T>
    concept contiguous_utf16_range =
        contiguous_code_unit_range<T, format::utf16>;

    template<typename T>
    concept utf32_iter = code_unit_iter<T, format::utf32>;
    template<typename T>
    concept utf32_range = code_unit_range<T, format::utf32>;
    template<typename T>
    concept contiguous_utf32_range =
        contiguous_code_unit_range<T, format::utf32>;

    template<typename T>
    concept code_point = utf32_code_unit<T>;
    template<typename T>
    concept code_point_iter = utf32_iter<T>;
    template<typename T>
    concept code_point_range = utf32_range<T>;

    template<typename T>
    concept utf_iter = utf8_iter<T> || utf16_iter<T> || utf32_iter<T>;
    template<typename T>
    concept utf_range = utf8_range<T> || utf16_range<T> || utf32_range<T>;


    template<typename T>
    concept grapheme_iter =
        std::input_iterator<T> && code_point_range<std::iter_reference_t<T>> &&
        requires(T t) {
            { t.base() } -> code_point_iter;
        };

    template<typename T>
    concept grapheme_range = std::ranges::input_range<T> &&
                             grapheme_iter<std::ranges::iterator_t<T>>;

    template<typename R>
    using code_point_iterator_t = decltype(std::declval<R>().begin().base());

    template<typename R>
    using code_point_sentinel_t = decltype(std::declval<R>().end().base());

    template<typename T, format F>
    concept grapheme_iter_code_unit = grapheme_iter<T> && requires(T t) {
        { t.base().base() } -> code_unit_iter<F>;
    };

    template<typename T, format F>
    concept grapheme_range_code_unit =
        grapheme_range<T> &&
        grapheme_iter_code_unit<std::ranges::iterator_t<T>, F>;


    namespace dtl {
        template<typename T, class CodeUnit>
        concept eraseable_insertable_sized_bidi_range =
            std::ranges::sized_range<T> && std::ranges::input_range<T> &&
            requires(T t, CodeUnit const * it) {
                {
                    t.erase(t.begin(), t.end())
                } -> std::same_as<std::ranges::iterator_t<T>>;
                {
                    t.insert(t.end(), it, it)
                } -> std::same_as<std::ranges::iterator_t<T>>;
            };
    }

    template<typename T>
    concept utf8_string = utf8_code_unit<std::ranges::range_value_t<T>> &&
                          dtl::eraseable_insertable_sized_bidi_range<
                              T,
                              std::ranges::range_value_t<T>>;

    template<typename T>
    concept utf16_string = utf16_code_unit<std::ranges::range_value_t<T>> &&
                           dtl::eraseable_insertable_sized_bidi_range<
                               T,
                               std::ranges::range_value_t<T>>;

    template<typename T>
    concept utf_string = utf8_string<T> || utf16_string<T>;

    template<typename T>
    concept transcoding_error_handler = requires(T t, std::string_view msg) {
        { t(msg) } -> std::same_as<char32_t>;
    };
    //]

    // Clang 13 defines __cpp_lib_concepts but not std::indirectly copyable.
#if defined(__clang_major__) && __clang_major__ <= 13
    template<typename In, typename Out>
    concept indirectly_copyable =
        std::indirectly_readable<In> &&
        std::indirectly_writable<Out, std::iter_reference_t<In>>;
#else
    template<typename In, typename Out>
    concept indirectly_copyable = std::indirectly_copyable<In, Out>;
#endif
    }
}}

#endif

namespace boost::parser::detail { namespace text { namespace detail {

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS

    template<typename T>
    using iterator_t = std::ranges::iterator_t<T>;
    template<typename T>
    using sentinel_t = std::ranges::sentinel_t<T>;
    template<typename T>
    using iter_value_t = std::iter_value_t<T>;
    template<typename T>
    using iter_reference_t = std::iter_reference_t<T>;
    template<typename T>
    using range_value_t = std::ranges::range_value_t<T>;
    template<typename T>
    using range_reference_t = std::ranges::range_reference_t<T>;
    template<typename T>
    using range_difference_t = std::ranges::range_difference_t<T>;

#else

    template<typename T>
    using iterator_t = decltype(detail::begin(std::declval<T &>()));
    template<typename T>
    using sentinel_t = decltype(detail::end(std::declval<T &>()));
    template<typename T>
    using iter_value_t = typename std::iterator_traits<T>::value_type;
    template<typename T>
    using iter_reference_t = decltype(*std::declval<T &>());
    template<typename T>
    using range_value_t = iter_value_t<iterator_t<T>>;
    template<typename T>
    using range_reference_t = iter_reference_t<iterator_t<T>>;
    template<typename T>
    using range_difference_t = std::ptrdiff_t;

    template<typename T>
    constexpr bool code_unit_v =
#if defined(__cpp_char8_t)
        std::is_same_v<T, char8_t> ||
#endif
        std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> ||
        std::is_same_v<T, char> || std::is_same_v<T, wchar_t>;

#endif

}}}

#endif
