// Copyright (C) 2020 T. Zachary Laine
// Copyright Louis Dionne 2013-2017
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_TUPLE_HPP
#define BOOST_PARSER_TUPLE_HPP

#include <boost/parser/config.hpp>
#include <boost/parser/detail/detection.hpp>

#if BOOST_PARSER_USE_STD_TUPLE

#include <tuple>

#else

// Silence very verbose warnings about std::is_pod/std::is_literal being
// deprecated.
#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#    pragma GCC diagnostic ignored "-Wunused-value"
#endif
#include <boost/hana.hpp>
#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#endif

#endif


namespace boost { namespace parser {

    namespace detail {
        // to_int() and parse_llong() taken from boost/hana/bool.hpp.
        constexpr int to_int(char c)
        {
            int result = 0;

            if (c >= 'A' && c <= 'F') {
                result = static_cast<int>(c) - static_cast<int>('A') + 10;
            } else if (c >= 'a' && c <= 'f') {
                result = static_cast<int>(c) - static_cast<int>('a') + 10;
            } else {
                result = static_cast<int>(c) - static_cast<int>('0');
            }

            return result;
        }

        template<std::size_t N>
        constexpr long long parse_llong(const char (&arr)[N])
        {
            long long base = 10;
            std::size_t offset = 0;

            if constexpr (N > 2) {
                bool starts_with_zero = arr[0] == '0';
                bool is_hex = starts_with_zero && arr[1] == 'x';
                bool is_binary = starts_with_zero && arr[1] == 'b';

                if (is_hex) {
                    // 0xDEADBEEF (hexadecimal)
                    base = 16;
                    offset = 2;
                } else if (is_binary) {
                    // 0b101011101 (binary)
                    base = 2;
                    offset = 2;
                } else if (starts_with_zero) {
                    // 012345 (octal)
                    base = 8;
                    offset = 1;
                }
            }

            long long number = 0;
            long long multiplier = 1;

            for (std::size_t i = 0; i < N - offset; ++i) {
                char c = arr[N - 1 - i];
                if (c != '\'') { // skip digit separators
                    number += to_int(c) * multiplier;
                    multiplier *= base;
                }
            }

            return number;
        }
    }

    /** The tuple template alias used within Boost.Parser.  This will be
        `boost::hana::tuple` if `BOOST_PARSER_USE_HANA_TUPLE` is defined, and
        `std::tuple` otherwise. */
#if BOOST_PARSER_USE_STD_TUPLE
    template<typename... Args>
    using tuple = std::tuple<Args...>;
#else
    template<typename... Args>
    using tuple = hana::tuple<Args...>;
#endif

    /** A template alias that is `boost::hana::integral_constant<T, I>` if
        `BOOST_PARSER_USE_HANA_TUPLE` is defined, and
        `std::integral_constant<T, I>` otherwise. */
#if BOOST_PARSER_USE_STD_TUPLE
    template<typename T, T I>
    using integral_constant = std::integral_constant<T, I>;
#else
    template<typename T, T I>
    using integral_constant = hana::integral_constant<T, I>;
#endif

    /** An accessor that returns a reference to the `I`-th data member of an
        aggregate struct or `boost::parser::tuple`. */
    template<typename T, typename U, U I>
    constexpr decltype(auto) get(T && x, integral_constant<U, I> i);

    /** A template alias that is `boost::hana::llong<I>` if
        `BOOST_PARSER_USE_HANA_TUPLE` is defined, and
        `std::integral_constant<long long, I>` otherwise. */
    template<long long I>
    using llong = integral_constant<long long, I>;

    namespace literals {
        /** A literal that can be used to concisely name `parser::llong`
            integral constants. */
        template<char... chars>
        constexpr auto operator""_c()
        {
            constexpr long long n =
                detail::parse_llong<sizeof...(chars)>({chars...});
            return llong<n>{};
        }
    }

    namespace detail {
        /** A tuple accessor that returns a reference to the `I`-th element. */
        template<typename T, T I, typename... Args>
        constexpr decltype(auto)
        tuple_get(tuple<Args...> const & t, integral_constant<T, I>)
        {
#if BOOST_PARSER_USE_STD_TUPLE
            return std::get<I>(t);
#else
            return hana::at_c<I>(t);
#endif
        }

        /** A tuple accessor that returns a reference to the `I`-th element. */
        template<typename T, T I, typename... Args>
        constexpr decltype(auto)
        tuple_get(tuple<Args...> & t, integral_constant<T, I>)
        {
#if BOOST_PARSER_USE_STD_TUPLE
            return std::get<I>(t);
#else
            return hana::at_c<I>(t);
#endif
        }

        /** A tuple accessor that returns a reference to the `I`-th element. */
        template<typename T, T I, typename... Args>
        constexpr decltype(auto)
        tuple_get(tuple<Args...> && t, integral_constant<T, I>)
        {
#if BOOST_PARSER_USE_STD_TUPLE
            return std::move(std::get<I>(t));
#else
            return std::move(hana::at_c<I>(t));
#endif
        }

        template<int N>
        struct ce_int
        {
            constexpr static int value = N;
        };

        struct whatever
        {
            int _;
            template<typename T>
            operator T() const && noexcept
            {
                // Yuck.
                std::remove_reference_t<T> * ptr = nullptr;
                ptr += 1; // warning mitigation
                return *ptr;
            }
        };

        template<typename T, int... Is>
        constexpr auto
            constructible_expr_impl(std::integer_sequence<int, Is...>)
                -> decltype(T{whatever{Is}...}, ce_int<1>{});

        template<typename T, typename N>
        using constructible_expr = decltype(detail::constructible_expr_impl<T>(
            std::make_integer_sequence<int, N::value>()));

        template<typename T, int... Is>
        constexpr int struct_arity_impl(std::integer_sequence<int, Is...>)
        {
            return (
                detected_or_t<ce_int<0>, constructible_expr, T, ce_int<Is>>::
                    value +
                ... + 0);
        }

        // This often mistakenly returns 1 when you give it a struct with
        // private/protected members, because of copy/move construction.
        // Fortunately, we don't care -- we never assign from tuples of size
        // 1.
        template<typename T>
        constexpr int struct_arity_v =
            detail::struct_arity_impl<T>(std::make_integer_sequence<
                                         int,
                                         BOOST_PARSER_MAX_AGGREGATE_SIZE>()) -
            1;

        template<typename T>
        constexpr int tuple_size_ = -1;

        template<typename... Elems>
        constexpr int tuple_size_<tuple<Elems...>> = sizeof...(Elems);

        template<typename T, typename Tuple, int... Is>
        auto assign_tuple_to_aggregate(
            T & x, Tuple tup, std::integer_sequence<int, Is...>)
            -> decltype(x = T{parser::get(std::move(tup), llong<Is>{})...});

        template<typename T, typename Tuple, int... Is>
        auto tuple_to_aggregate(Tuple && tup, std::integer_sequence<int, Is...>)
            -> decltype(T{std::move(parser::get(tup, llong<Is>{}))...})
        {
            return T{std::move(parser::get(tup, llong<Is>{}))...};
        }

        template<typename T, typename Tuple>
        using tuple_to_aggregate_expr =
            decltype(detail::assign_tuple_to_aggregate(
                std::declval<T &>(),
                std::declval<Tuple>(),
                std::make_integer_sequence<int, tuple_size_<Tuple>>()));

        template<typename Struct, typename Tuple>
        constexpr bool is_struct_assignable_v =
            struct_arity_v<Struct> == tuple_size_<Tuple>
                ? is_detected_v<tuple_to_aggregate_expr, Struct, Tuple>
                : false;

        template<int N>
        struct tie_aggregate_impl
        {
            template<typename T>
            static constexpr auto call(T & x)
            {
                static_assert(
                    sizeof(T) && false,
                    "It looks like you're trying to use a struct larger than "
                    "the limit.");
            }
        };

        template<typename T>
        constexpr auto tie_aggregate(T & x)
        {
            static_assert(!std::is_union_v<T>);
            return tie_aggregate_impl<struct_arity_v<T>>::call(x);
        }

        template<typename Tuple, typename Tie, int... Is>
        auto aggregate_to_tuple(
            Tuple & tup, Tie tie, std::integer_sequence<int, Is...>)
            -> decltype((
                (parser::get(tup, llong<Is>{}) =
                     std::move(parser::get(tie, llong<Is>{}))),
                ...,
                (void)0))
        {
            return (
                (parser::get(tup, llong<Is>{}) =
                     std::move(parser::get(tie, llong<Is>{}))),
                ...,
                (void)0);
        }

        template<typename Tuple, typename T>
        using aggregate_to_tuple_expr = decltype(detail::aggregate_to_tuple(
            std::declval<Tuple &>(),
            detail::tie_aggregate(std::declval<T &>()),
            std::make_integer_sequence<int, tuple_size_<Tuple>>()));

        template<typename Tuple, typename Struct>
        constexpr bool is_tuple_assignable_impl()
        {
            if constexpr (
                std::is_aggregate_v<Struct> &&
                struct_arity_v<Struct> == tuple_size_<Tuple>) {
                return is_detected_v<aggregate_to_tuple_expr, Tuple, Struct>;
            } else {
                return false;
            }
        }

        template<typename Tuple, typename Struct>
        constexpr bool
            is_tuple_assignable_v = is_tuple_assignable_impl<Tuple, Struct>();

        template<typename T>
        struct is_tuple : std::false_type
        {};
        template<typename... T>
        struct is_tuple<tuple<T...>> : std::true_type
        {};
    }

    template<typename T, typename U, U I>
    constexpr decltype(auto) get(T && x, integral_constant<U, I> i)
    {
        using just_t = std::decay_t<T>;
        if constexpr (detail::is_tuple<just_t>::value) {
            return detail::tuple_get((T &&) x, i);
        } else if constexpr (std::is_aggregate_v<just_t>) {
            auto tup = detail::tie_aggregate(x);
            return detail::tuple_get(tup, i);
        } else {
            static_assert(
                sizeof(T) != sizeof(T),
                "boost::parser::get() is only defined for boost::parser::tuple "
                "and aggregate structs.");
        }
    }

}}

#include <boost/parser/detail/aggr_to_tuple_generated.hpp>

#endif
