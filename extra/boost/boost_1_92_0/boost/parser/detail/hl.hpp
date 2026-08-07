// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_HL_HPP
#define BOOST_PARSER_DETAIL_HL_HPP

#include <boost/parser/config.hpp>
#include <boost/parser/tuple.hpp>
#include <boost/parser/detail/detection.hpp>

#include <type_traits>
#include <utility>


namespace boost { namespace parser { namespace detail::hl {

    // Boost.Hana lite.  These functions work with boost::hana::tuple and
    // std::tuple.

    struct forward
    {
        template<typename T>
        decltype(auto) operator()(T && t)
        {
            return (T &&) t;
        }
    };

    template<typename... Args>
    constexpr auto make_tuple(Args &&... args)
    {
#if BOOST_PARSER_USE_STD_TUPLE
        return std::make_tuple((Args &&) args...);
#else
        return hana::make_tuple((Args &&) args...);
#endif
    }

    template<typename T, typename U>
    constexpr auto make_pair(T && t, U && u)
    {
        return hl::make_tuple((T &&) t, (U &&) u);
    }

    template<typename Tuple1, typename Tuple2>
    constexpr auto concat(Tuple1 const & t1, Tuple2 const & t2)
    {
#if BOOST_PARSER_USE_STD_TUPLE
        return std::tuple_cat(t1, t2);
#else
        // Hana's concat does not seem to do what it says on the tin.
        // Concatenating (int, string) with (double, int) yields (int, string,
        // double).  I maybe don't understand it well enough.
        return hana::insert_range(t1, hana::size(t1), t2);
#endif
    }


    // apply

    template<typename F, typename Tuple, std::size_t... I>
    constexpr auto
    apply_impl(F && f, Tuple && t, std::integer_sequence<std::size_t, I...>)
        -> decltype(((F &&) f)(parser::get((Tuple &&) t, llong<I>{})...))
    {
        return ((F &&) f)(parser::get((Tuple &&) t, llong<I>{})...);
    }
    template<
        typename F,
        typename Tuple,
        typename Enable = std::enable_if_t<detail::is_tuple<
            std::remove_cv_t<std::remove_reference_t<Tuple>>>{}>>
    constexpr auto apply(F && f, Tuple && t) -> decltype(hl::apply_impl(
        (F &&) f,
        (Tuple &&) t,
        std::make_integer_sequence<std::size_t, tuple_size_<Tuple>>{}))
    {
        return hl::apply_impl(
            (F &&) f,
            (Tuple &&) t,
            std::make_integer_sequence<std::size_t, tuple_size_<Tuple>>{});
    }


    // for_each

    template<typename F, typename Tuple, std::size_t... I>
    constexpr void for_each_impl(
        Tuple const & t, F && f, std::integer_sequence<std::size_t, I...>)
    {
        int _[] = {0, (f(parser::get(t, llong<I>{})), 0)...};
        (void)_;
    }
    template<
        typename F,
        typename Tuple,
        std::size_t... I,
        typename Enable = std::enable_if_t<!std::is_reference_v<Tuple>>>
    constexpr void
    for_each_impl(Tuple && t, F && f, std::integer_sequence<std::size_t, I...>)
    {
        int _[] = {0, (f(std::move(parser::get(t, llong<I>{}))), 0)...};
        (void)_;
    }

    template<typename F, typename... Args>
    constexpr void for_each(tuple<Args...> && t, F && f)
    {
        hl::for_each_impl(
            std::move(t),
            (F &&) f,
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }
    template<typename F, typename... Args>
    constexpr void for_each(tuple<Args...> const & t, F && f)
    {
        hl::for_each_impl(
            t,
            (F &&) f,
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }


    // transform

    template<int offset, typename F, typename Tuple, std::size_t... I>
    constexpr auto transform_impl(
        Tuple const & t, F && f, std::integer_sequence<std::size_t, I...>)
    {
        return tuple<
            std::decay_t<decltype(f(parser::get(t, llong<I + offset>{})))>...>{
            f(parser::get(t, llong<I + offset>{}))...};
    }
    template<
        int offset,
        typename F,
        typename Tuple,
        std::size_t... I,
        typename Enable = std::enable_if_t<!std::is_reference_v<Tuple>>>
    auto constexpr transform_impl(
        Tuple && t, F && f, std::integer_sequence<std::size_t, I...>)
    {
        return tuple<std::decay_t<decltype(
            f(std::move(parser::get(t, llong<I + offset>{}))))>...>{
            f(std::move(parser::get(t, llong<I + offset>{})))...};
    }

    template<typename F, typename... Args>
    constexpr auto transform(tuple<Args...> && t, F && f)
    {
        return hl::transform_impl<0>(
            std::move(t),
            (F &&) f,
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }
    template<typename F, typename... Args>
    constexpr auto transform(tuple<Args...> const & t, F && f)
    {
        return hl::transform_impl<0>(
            t,
            (F &&) f,
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }


    // fold_left

    template<std::size_t I, std::size_t N>
    struct fold_left_dispatch
    {
        template<typename F, typename State, typename... Args>
        constexpr static auto
        call(tuple<Args...> const & t, State && s, F const & f)
        {
            return fold_left_dispatch<I + 1, N>::call(
                t, f((State &&) s, parser::get(t, llong<I>{})), f);
        }
    };
    template<std::size_t I>
    struct fold_left_dispatch<I, I>
    {
        template<typename F, typename State, typename... Args>
        constexpr static auto
        call(tuple<Args...> const & t, State && s, F const & f)
        {
            return (State &&) s;
        }
    };

    template<typename F, typename State, typename... Args>
    constexpr auto fold_left(tuple<Args...> const & t, State && s, F const & f)
    {
        return hl::fold_left_dispatch<0, sizeof...(Args)>::call(
            t, (State &&) s, (F &&) f);
    }


    // size

    template<typename... Args>
    constexpr auto size(tuple<Args...> const & t)
    {
        return llong<sizeof...(Args)>{};
    }

    template<typename... Args>
    constexpr auto size_minus_one(tuple<Args...> const & t)
    {
        return llong<sizeof...(Args) - 1>{};
    }


    // contains

    template<typename T, typename U>
    using comparable = decltype(std::declval<T>() == std::declval<U>());

    struct typesafe_equals
    {
        template<typename T, typename U>
        constexpr bool operator()(T const & t, U const & u)
        {
            if constexpr (detail::is_detected_v<comparable, T, U>) {
                return t == u;
            } else {
                return false;
            }
        }
    };

    template<typename T, typename Tuple, std::size_t... I>
    constexpr bool contains_impl(
        Tuple const & t, T const & x, std::integer_sequence<std::size_t, I...>)
    {
        typesafe_equals eq;
        (void)eq;
        return (eq(parser::get(t, llong<I>{}), x) || ...);
    }

    template<typename T, typename... Args>
    constexpr bool contains(tuple<Args...> const & t, T const & x)
    {
        return contains_impl(
            t, x, std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }


    // front, back

    template<typename Arg, typename... Args>
    constexpr decltype(auto) front(tuple<Arg, Args...> const & t)
    {
        return parser::get(t, llong<0>{});
    }
    template<typename Arg, typename... Args>
    constexpr decltype(auto) back(tuple<Arg, Args...> const & t)
    {
        return parser::get(t, llong<sizeof...(Args)>{});
    }


    // drop_front

    template<typename Arg, typename... Args>
    constexpr auto drop_front(tuple<Arg, Args...> && t)
    {
        return hl::transform_impl<1>(
            std::move(t),
            forward{},
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }
    template<typename Arg, typename... Args>
    constexpr auto drop_front(tuple<Arg, Args...> const & t)
    {
        return hl::transform_impl<1>(
            t,
            forward{},
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }


    // drop_back

    template<typename Arg, typename... Args>
    constexpr auto drop_back(tuple<Arg, Args...> && t)
    {
        return hl::transform_impl<0>(
            std::move(t),
            forward{},
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }
    template<typename Arg, typename... Args>
    constexpr auto drop_back(tuple<Arg, Args...> const & t)
    {
        return hl::transform_impl<0>(
            t,
            forward{},
            std::make_integer_sequence<std::size_t, sizeof...(Args)>());
    }


    // first, second

    template<typename T, typename U>
    constexpr decltype(auto) first(tuple<T, U> const & t)
    {
        return parser::get(t, llong<0>{});
    }
    template<typename T, typename U>
    constexpr decltype(auto) second(tuple<T, U> const & t)
    {
        return parser::get(t, llong<1>{});
    }


    // zip

    template<std::size_t I, typename... Tuples>
    constexpr decltype(auto) make_zip_elem(Tuples const &... ts)
    {
        return hl::make_tuple(parser::get(ts, llong<I>{})...);
    }

    template<std::size_t... I, typename... Tuples>
    constexpr auto zip_impl(std::index_sequence<I...>, Tuples const &... ts)
    {
        return hl::make_tuple(hl::make_zip_elem<I>(ts...)...);
    }

    template<typename T>
    struct tuplesize;
    template<typename... Args>
    struct tuplesize<tuple<Args...>>
    {
        constexpr static std::size_t value = sizeof...(Args);
    };

    template<typename Tuple, typename... Tuples>
    constexpr auto zip(Tuple const & t, Tuples const &... ts)
    {
        return hl::zip_impl(
            std::make_integer_sequence<
                std::size_t,
                tuplesize<std::remove_reference_t<Tuple>>::value>(),
            t,
            ts...);
    }


    // append

    template<typename... Args, typename T>
    constexpr auto append(tuple<Args...> && t, T && x)
    {
#if BOOST_PARSER_USE_STD_TUPLE
        return std::tuple_cat(std::move(t), std::make_tuple((T &&) x));
#else
        return hana::append(std::move(t), (T &&) x);
#endif
    }
    template<typename... Args, typename T>
    constexpr auto append(tuple<Args...> const & t, T && x)
    {
#if BOOST_PARSER_USE_STD_TUPLE
        return std::tuple_cat(t, std::make_tuple((T &&) x));
#else
        return hana::append(t, (T &&) x);
#endif
    }


    // prepend

    template<typename... Args, typename T>
    constexpr auto prepend(tuple<Args...> && t, T && x)
    {
#if BOOST_PARSER_USE_STD_TUPLE
        return std::tuple_cat(std::make_tuple((T &&) x), std::move(t));
#else
        return hana::prepend(std::move(t), (T &&) x);
#endif
    }
    template<typename... Args, typename T>
    constexpr auto prepend(tuple<Args...> const & t, T && x)
    {
#if BOOST_PARSER_USE_STD_TUPLE
        return std::tuple_cat(std::make_tuple((T &&) x), t);
#else
        return hana::prepend(t, (T &&) x);
#endif
    }

}}}

#endif
