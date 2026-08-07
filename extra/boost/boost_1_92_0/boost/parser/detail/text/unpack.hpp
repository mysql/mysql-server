// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_UNPACK_HPP
#define BOOST_PARSER_DETAIL_TEXT_UNPACK_HPP

#include <boost/parser/detail/text/transcode_iterator_fwd.hpp>

#include <type_traits>
#include <optional>


namespace boost::parser::detail { namespace text {

    struct no_op_repacker
    {
        template<class T>
        T operator()(T x) const
        {
            return x;
        }
    };

    namespace detail {
        // Using this custom template is quite a bit faster than using lambdas.
        // Unexpected.
        template<
            typename RepackedIterator,
            typename I,
            typename S,
            typename Then,
            bool Bidi>
        struct repacker
        {
            repacker() = default;
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            template<bool Enable = Bidi, typename = std::enable_if_t<Enable>>
#endif
            repacker(I first, S last, Then then)
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                requires Bidi
#endif
                : first{first},
                  last{last},
                  then{then}
            {}
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            template<bool Enable = !Bidi, typename = std::enable_if_t<Enable>>
#endif
            repacker(S last, Then then)
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                requires(!Bidi)
#endif
                :
                last{last}, then{then}
            {}

            auto operator()(I it) const
            {
                if constexpr (Bidi) {
                    return then(RepackedIterator(*first, it, last));
                } else {
                    return then(RepackedIterator(it, last));
                }
            }

            std::optional<I> first;
            [[no_unique_address]] S last;
            [[no_unique_address]] Then then;
        };

        template<typename I, typename S, typename Repack>
        constexpr auto
        unpack_iterator_and_sentinel_impl(I first, S last, Repack repack);

        template<
            format FromFormat,
            format ToFormat,
            typename I,
            typename S,
            typename ErrorHandler,
            typename Repack>
        constexpr auto unpack_iterator_and_sentinel_impl(
            utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler> first,
            utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler> last,
            Repack repack);

        template<
            format FromFormat,
            format ToFormat,
            typename I,
            typename S,
            typename ErrorHandler,
            typename Repack>
        constexpr auto unpack_iterator_and_sentinel_impl(
            utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler> first,
            S last,
            Repack repack);

        template<typename I, typename S, typename Repack>
        constexpr auto
        unpack_iterator_and_sentinel(I first, S last, Repack repack)
        {
            return detail::unpack_iterator_and_sentinel_impl(
                first, last, repack);
        }

        struct unpack_iterator_and_sentinel_cpo
        {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            template<
                utf_iter I,
                std::sentinel_for<I> S,
                typename Repack = no_op_repacker>
            requires std::forward_iterator<I>
#else
            template<typename I, typename S, typename Repack = no_op_repacker>
#endif
            constexpr auto
            operator()(I first, S last, Repack repack = Repack()) const
            {
                return unpack_iterator_and_sentinel(first, last, repack);
            }
        };
    }

    inline namespace cpo {
        inline constexpr detail::unpack_iterator_and_sentinel_cpo
            unpack_iterator_and_sentinel{};
    }

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<format FormatTag, utf_iter I, std::sentinel_for<I> S, class Repack>
#else
    template<format FormatTag, typename I, typename S, class Repack>
#endif
    struct unpack_result
    {
        static constexpr format format_tag = FormatTag;

        I first;
        [[no_unique_address]] S last;
        [[no_unique_address]] Repack repack;
    };

    namespace detail {
        struct no_such_type
        {};
        template<typename I, typename S, typename Repack>
        constexpr auto
        unpack_iterator_and_sentinel_impl(I first, S last, Repack repack)
        {
            using value_type = detail::iter_value_t<I>;
            if constexpr (
                std::is_same_v<value_type, char>
#if defined(__cpp_char8_t)
                || std::is_same_v<value_type, char8_t>
#endif
            ) {
                return unpack_result<format::utf8, I, S, Repack>{
                    first, last, repack};
            } else if constexpr (
#if defined(_MSC_VER)
                std::is_same_v<value_type, wchar_t> ||
#endif
                std::is_same_v<value_type, char16_t>) {
                return unpack_result<format::utf16, I, S, Repack>{
                    first, last, repack};
            } else if constexpr (
#if !defined(_MSC_VER)
                std::is_same_v<value_type, wchar_t> ||
#endif
                std::is_same_v<value_type, char32_t>) {
                return unpack_result<format::utf32, I, S, Repack>{
                    first, last, repack};
            } else {
                static_assert(
                    std::is_same_v<Repack, no_such_type>,
                    "Unpacked iterator is not a utf_iter!");
                return 0;
            }
        }

    }
}}

#include <boost/parser/detail/text/transcode_iterator.hpp>

namespace boost::parser::detail { namespace text { namespace detail {

    template<
        format FromFormat,
        format ToFormat,
        typename I,
        typename S,
        typename ErrorHandler,
        typename Repack>
    constexpr auto unpack_iterator_and_sentinel_impl(
        utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler> first,
        utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler> last,
        Repack repack)
    {
        using iterator = utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler>;
        if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            std::bidirectional_iterator<I>
#else
            std::is_base_of_v<
                std::bidirectional_iterator_tag,
                typename std::iterator_traits<I>::iterator_category>
#endif
        ) {
            return boost::parser::detail::text::unpack_iterator_and_sentinel(
                first.base(),
                last.base(),
                repacker<
                    iterator,
                    decltype(first.begin()),
                    decltype(first.end()),
                    Repack,
                    true>(first.begin(), first.end(), repack));
        } else {
            return boost::parser::detail::text::unpack_iterator_and_sentinel(
                first.base(),
                last.base(),
                repacker<iterator, int, decltype(first.end()), Repack, false>(
                    first.end(), repack));
        }
    }

    template<
        format FromFormat,
        format ToFormat,
        typename I,
        typename S,
        typename ErrorHandler,
        typename Repack>
    constexpr auto unpack_iterator_and_sentinel_impl(
        utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler> first,
        S last,
        Repack repack)
    {
        using iterator = utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler>;
        if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            std::bidirectional_iterator<I>
#else
            std::is_base_of_v<
                std::bidirectional_iterator_tag,
                typename std::iterator_traits<I>::iterator_category>
#endif
        ) {
            return boost::parser::detail::text::unpack_iterator_and_sentinel(
                first.base(),
                last,
                repacker<
                    iterator,
                    decltype(first.begin()),
                    decltype(first.end()),
                    Repack,
                    true>(first.begin(), first.end(), repack));
        } else {
            return boost::parser::detail::text::unpack_iterator_and_sentinel(
                first.base(),
                last,
                repacker<iterator, int, S, Repack, false>(last, repack));
        }
    }

}}}

#endif
