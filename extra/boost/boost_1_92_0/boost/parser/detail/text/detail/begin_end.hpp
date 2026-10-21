// Copyright (C) 2022 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_BEGIN_END_HPP
#define BOOST_PARSER_DETAIL_TEXT_BEGIN_END_HPP

#include <boost/parser/detail/detection.hpp>

#include <initializer_list>


namespace boost::parser::detail { namespace text { namespace detail {

    template<typename T>
    T decay_copy(T) noexcept;

    template<typename T>
    struct static_const
    {
        static constexpr T value{};
    };
    template<typename T>
    constexpr T static_const<T>::value;

    namespace begin_impl {
        template<typename T>
        void begin(T &&) = delete;
        template<typename T>
        void begin(std::initializer_list<T>) = delete;

        template<typename T>
        using member_begin_expr = decltype(std::declval<T &>().begin());
        template<typename T>
        using adl_begin_expr = decltype(begin(std::declval<T &>()));
        template<typename T>
        constexpr bool has_member_begin_v = is_detected_v<member_begin_expr, T>;
        template<typename T>
        constexpr bool has_adl_begin_v = is_detected_v<adl_begin_expr, T>;

        template<typename R>
        using member_return_t =
            decltype(detail::decay_copy(std::declval<R &>().begin()));
        template<typename R>
        using adl_return_t =
            decltype(detail::decay_copy(begin(std::declval<R &>)));

        struct impl
        {
            template<typename R, std::size_t N>
            void operator()(R (&&)[N]) const = delete;

            template<typename R, std::size_t N>
            constexpr R * operator()(R (&array)[N]) const
            {
                return array;
            }

            template<typename R>
            constexpr std::
                enable_if_t<has_member_begin_v<R>, member_return_t<R>>
                operator()(R && r) const
            {
                return r.begin();
            }

            template<typename R>
            constexpr std::enable_if_t<
                !has_member_begin_v<R> && has_adl_begin_v<R>,
                adl_return_t<R>>
            operator()(R && r) const
            {
                return begin(r);
            }
        };
    }

#if 201703L <= __cplusplus
    namespace _ {
        inline constexpr begin_impl::impl begin;
    }
    using namespace _;
#else
    namespace {
        constexpr auto & begin = static_const<begin_impl::impl>::value;
    }
#endif

    namespace end_impl {
        template<typename T>
        void end(T &&) = delete;
        template<typename T>
        void end(std::initializer_list<T>) = delete;

        template<typename T>
        using member_end_expr = decltype(std::declval<T &>().end());
        template<typename T>
        using adl_end_expr = decltype(end(std::declval<T &>()));
        template<typename T>
        constexpr bool has_member_end_v = is_detected_v<member_end_expr, T>;
        template<typename T>
        constexpr bool has_adl_end_v = is_detected_v<adl_end_expr, T>;

        template<typename R>
        using member_return_t =
            decltype(detail::decay_copy(std::declval<R &>().end()));
        template<typename R>
        using adl_return_t =
            decltype(detail::decay_copy(end(std::declval<R &>)));

        struct impl
        {
            template<typename R, std::size_t N>
            void operator()(R (&&)[N]) const = delete;

            template<typename R, std::size_t N>
            constexpr R * operator()(R (&array)[N]) const
            {
                return array + N;
            }

            template<typename R>
            constexpr std::enable_if_t<has_member_end_v<R>, member_return_t<R>>
            operator()(R && r) const
            {
                return r.end();
            }

            template<typename R>
            constexpr std::enable_if_t<
                !has_member_end_v<R> && has_adl_end_v<R>,
                adl_return_t<R>>
            operator()(R && r) const
            {
                return end(r);
            }
        };
    }

#if 201703L <= __cplusplus
    namespace _ {
        inline constexpr end_impl::impl end;
    }
    using namespace _;
#else
    namespace {
        constexpr auto & end = static_const<end_impl::impl>::value;
    }
#endif

}}}

#endif
