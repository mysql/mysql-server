// Copyright (C) 2024 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_DETAIL_ALL_T_HPP
#define BOOST_PARSER_DETAIL_TEXT_DETAIL_ALL_T_HPP

#include <boost/parser/detail/stl_interfaces/view_interface.hpp>
#include <boost/parser/detail/text/detail/begin_end.hpp>
#include <boost/parser/detail/detection.hpp>

#include <array>
#if BOOST_PARSER_USE_CONCEPTS
#include <ranges>
#endif


namespace boost::parser::detail::text::detail {

    template<typename T>
    using iterator_ = decltype(text::detail::begin(std::declval<T &>()));
    template<typename T>
    using sentinel_ = decltype(text::detail::end(std::declval<T &>()));

    template<typename T>
    constexpr bool range_ =
        is_detected_v<iterator_, T> && is_detected_v<sentinel_, T>;

    template<typename T>
    using has_insert_ = decltype(std::declval<T &>().insert(
        std::declval<T>().begin(), *std::declval<T>().begin()));

    template<typename T>
    constexpr bool container_ = is_detected_v<has_insert_, T>;

    template<typename T>
    constexpr bool is_std_array_v = false;
    template<typename T, size_t N>
    constexpr bool is_std_array_v<std::array<T, N>> = false;

    template<typename R>
    constexpr bool view =
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS ||                                   \
    (defined(__cpp_lib_concepts) &&                                            \
     (!defined(BOOST_PARSER_GCC) || 12 <= __GNUC__))
        std::ranges::view<R>
#else
        range_<R> && !container_<R> &&
        !std::is_array_v<std::remove_reference_t<R>> &&
        !is_std_array_v<std::remove_reference_t<R>>
#endif
        ;

    template<
        typename R,
        typename Enable = std::enable_if_t<range_<R> && std::is_object_v<R>>>
    struct ref_view : stl_interfaces::view_interface<ref_view<R>>
    {
    private:
        static void rvalue_poison(R &);
        static void rvalue_poison(R &&) = delete;

    public:
        template<
            typename T,
            typename Enable2 = std::enable_if_t<
                !std::
                    is_same_v<remove_cv_ref_t<T>, remove_cv_ref_t<ref_view>> &&
                std::is_convertible_v<T, R &>>,
            typename Enable3 = decltype(rvalue_poison(std::declval<T>()))>
        constexpr ref_view(T && t) :
            r_(std::addressof(static_cast<R &>((T &&) t)))
        {}
        constexpr R & base() const { return *r_; }
        constexpr iterator_<R> begin() const
        {
            return text::detail::begin(*r_);
        }
        constexpr sentinel_<R> end() const { return text::detail::end(*r_); }

    private:
        R * r_;
    };

    template<typename R>
    ref_view(R &) -> ref_view<R>;

    template<typename R>
    struct owning_view : stl_interfaces::view_interface<owning_view<R>>
    {
        owning_view() = default;
        constexpr owning_view(R && t) : r_(std::move(t)) {}

        owning_view(owning_view &&) = default;
        owning_view & operator=(owning_view &&) = default;

        constexpr R & base() & noexcept { return r_; }
        constexpr const R & base() const & noexcept { return r_; }
        constexpr R && base() && noexcept { return std::move(r_); }
        constexpr const R && base() const && noexcept { return std::move(r_); }

        constexpr iterator_<R> begin() { return text::detail::begin(r_); }
        constexpr sentinel_<R> end() { return text::detail::end(r_); }

        constexpr auto begin() const { return text::detail::begin(r_); }
        constexpr auto end() const { return text::detail::end(r_); }

    private:
        R r_ = R();
    };

    template<typename T>
    using can_ref_view_expr = decltype(ref_view(std::declval<T>()));
    template<typename T>
    constexpr bool can_ref_view = is_detected_v<can_ref_view_expr, T>;

    struct all_impl
    {
        template<typename R, typename Enable = std::enable_if_t<range_<R>>>
        [[nodiscard]] constexpr auto operator()(R && r) const
        {
            using T = remove_cv_ref_t<R>;
            if constexpr (view<T>)
                return (R &&) r;
            else if constexpr (can_ref_view<R>)
                return ref_view(r);
            else
                return owning_view<T>((R &&)r);
        }
    };

    constexpr all_impl all;

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<typename R>
    using all_t = std::views::all_t<R>;
#else
    template<typename R>
    using all_t = decltype(all(std::declval<R>()));
#endif

}

#endif
