//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_WITH_PARAMS_HPP
#define BOOST_MYSQL_IMPL_WITH_PARAMS_HPP

#pragma once

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/integer_sequence.hpp>

// Execution request traits
namespace boost {
namespace mysql {
namespace detail {

template <std::size_t N>
struct with_params_proxy
{
    constant_string_view query;
    std::array<format_arg, N> args;

    operator detail::any_execution_request() const { return any_execution_request({query, args}); }
};

template <class... T>
struct execution_request_traits<with_params_t<T...>>
{
    template <class WithParamsType, std::size_t... I>
    static with_params_proxy<sizeof...(T)> make_request_impl(WithParamsType&& input, mp11::index_sequence<I...>)
    {
        boost::ignore_unused(input);  // MSVC gets confused for tuples of size 0
        // clang-format off
        return {
            input.query,
            {{
                {
                    string_view(),
                    formattable_ref(std::get<I>(std::forward<WithParamsType>(input).args))
                }...
            }}
        };
        // clang-format on
    }

    // Allow the value category of the object to be deduced
    template <class WithParamsType>
    static with_params_proxy<sizeof...(T)> make_request(WithParamsType&& input, std::vector<field_view>&)
    {
        return make_request_impl(
            std::forward<WithParamsType>(input),
            mp11::make_index_sequence<sizeof...(T)>()
        );
    }
};

// Old MSVCs fail to process the above when sizeof...(T) is zero
template <>
struct execution_request_traits<with_params_t<>>
{
    static any_execution_request make_request(with_params_t<> input, std::vector<field_view>&)
    {
        return any_execution_request({input.query, span<format_arg>{}});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
