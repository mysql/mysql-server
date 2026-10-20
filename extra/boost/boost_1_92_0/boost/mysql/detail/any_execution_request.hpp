//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ANY_EXECUTION_REQUEST_HPP
#define BOOST_MYSQL_DETAIL_ANY_EXECUTION_REQUEST_HPP

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/core/span.hpp>

#include <cstdint>
#include <type_traits>
#include <vector>

namespace boost {
namespace mysql {

class field_view;
class format_arg;

namespace detail {

struct any_execution_request
{
    enum class type_t
    {
        query,
        query_with_params,
        stmt
    };

    union data_t
    {
        string_view query;
        struct query_with_params_t
        {
            constant_string_view query;
            span<const format_arg> args;
        } query_with_params;
        struct stmt_t
        {
            std::uint32_t stmt_id;
            std::uint16_t num_params;
            span<const field_view> params;
        } stmt;

        data_t(string_view q) noexcept : query(q) {}
        data_t(query_with_params_t v) noexcept : query_with_params(v) {}
        data_t(stmt_t v) noexcept : stmt(v) {}
    };

    type_t type;
    data_t data;

    any_execution_request(string_view q) noexcept : type(type_t::query), data(q) {}
    any_execution_request(data_t::query_with_params_t v) noexcept : type(type_t::query_with_params), data(v)
    {
    }
    any_execution_request(data_t::stmt_t v) noexcept : type(type_t::stmt), data(v) {}
};

struct no_execution_request_traits
{
};

template <class T, class = void>
struct execution_request_traits : no_execution_request_traits
{
};

template <class T>
struct execution_request_traits<T, typename std::enable_if<std::is_convertible<T, string_view>::value>::type>
{
    static any_execution_request make_request(string_view input, std::vector<field_view>&) { return input; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
