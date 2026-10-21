//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_SEQUENCE_HPP
#define BOOST_MYSQL_DETAIL_SEQUENCE_HPP

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/format_sql.hpp>

#include <boost/compat/to_array.hpp>
#include <boost/compat/type_traits.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <iterator>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {
namespace detail {

template <class T>
struct sequence_range_impl
{
    using type = T;
};

template <class T>
struct sequence_range_impl<std::reference_wrapper<T>>
{
    using type = T&;
};

template <class T, std::size_t N>
struct sequence_range_impl<T[N]>
{
    using type = std::array<T, N>;
};

template <class T>
using sequence_range_type = sequence_range_impl<compat::remove_cvref_t<T>>;

template <class Range>
Range&& cast_range(Range&& range)
{
    return std::forward<Range>(range);
}

template <class T, std::size_t N>
std::array<compat::remove_cv_t<T>, N> cast_range(T (&a)[N])
{
    return compat::to_array(a);
}

template <class T, std::size_t N>
std::array<compat::remove_cv_t<T>, N> cast_range(T (&&a)[N])
{
    return compat::to_array(std::move(a));
}

// TODO: should this be Range&&?
template <class Range, class FormatFn>
void do_format_sequence(Range& range, const FormatFn& fn, constant_string_view glue, format_context_base& ctx)
{
    bool is_first = true;
    for (auto it = std::begin(range); it != std::end(range); ++it)
    {
        if (!is_first)
            ctx.append_raw(glue);
        is_first = false;
        fn(*it, ctx);
    }
}

#ifdef BOOST_MYSQL_HAS_CONCEPTS
template <class FormatFn, class Range>
concept format_fn_for_range = requires(const FormatFn& format_fn, Range&& range, format_context_base& ctx) {
    { std::begin(range) != std::end(range) } -> std::convertible_to<bool>;
    format_fn(*std::begin(range), ctx);
    std::end(range);
};
#endif

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
