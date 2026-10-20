// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_CONCEPTS_HPP
#define BOOST_PARSER_CONCEPTS_HPP

#include <boost/parser/config.hpp>
#include <boost/parser/parser_fwd.hpp>
#include <boost/parser/detail/text/transcode_view.hpp>

#if defined(BOOST_PARSER_DOXYGEN) || BOOST_PARSER_USE_CONCEPTS

#include <ranges>


namespace boost { namespace parser {

    //[ all_concepts
    template<typename T>
    concept code_unit =
        std::same_as<std::remove_cv_t<T>, char> ||
        std::same_as<std::remove_cv_t<T>, wchar_t> ||
        std::same_as<std::remove_cv_t<T>, char8_t> ||
        std::same_as<std::remove_cv_t<T>, char16_t>||
        std::same_as<std::remove_cv_t<T>, char32_t>;

    template<typename T>
    concept parsable_iter =
        std::forward_iterator<T> && code_unit<std::iter_value_t<T>>;

    //[ parsable_range_like_concept
    //[ parsable_range_concept
    template<typename T>
    concept parsable_range = std::ranges::forward_range<T> &&
        code_unit<std::ranges::range_value_t<T>>;
    //]

    template<typename T>
    concept parsable_pointer = std::is_pointer_v<std::remove_cvref_t<T>> &&
        code_unit<std::remove_pointer_t<std::remove_cvref_t<T>>>;

    template<typename T>
    concept parsable_range_like = parsable_range<T> || parsable_pointer<T>;
    //]

    template<typename T>
    concept range_like = std::ranges::range<T> || parsable_pointer<T>;

    template<
        typename I,
        typename S,
        typename ErrorHandler,
        typename GlobalState>
    using minimal_parse_context = decltype(detail::make_context<false, false>(
        std::declval<I>(),
        std::declval<S>(),
        std::declval<bool &>(),
        std::declval<int &>(),
        std::declval<ErrorHandler const &>(),
        std::declval<detail::nope const &>(),
        std::declval<detail::symbol_table_tries_t &>(),
        std::declval<detail::pending_symbol_table_operations_t &>()));

    template<typename T, typename I, typename S, typename GlobalState>
    concept error_handler =
        requires (
            T const & t,
            I first,
            S last,
            parse_error<I> const & e,
            diagnostic_kind kind,
            std::string_view message,
            minimal_parse_context<
                I, S, T, GlobalState> const & context) {
            { t(first, last, e) } -> std::same_as<error_handler_result>;
            t.diagnose(kind, message, context, first);
            t.diagnose(kind, message, context);
        };

    //[ container_concept
    template<typename T>
    concept container = std::ranges::common_range<T> && requires(T t) {
        { t.insert(t.begin(), *t.begin()) }
            -> std::same_as<std::ranges::iterator_t<T>>;
    };
    //]

    //]

    namespace detail {

        template<typename T, typename U>
        concept container_and_value_type = container<T> &&
            (std::is_same_v<std::ranges::range_value_t<T>, U> ||
             (std::is_same_v<T, std::string> && std::is_same_v<U, char32_t>));

    }

}}

#endif

#endif
