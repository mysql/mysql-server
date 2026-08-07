//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_STATEMENT_HPP

#pragma once

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>

#include <tuple>
#include <vector>

template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple>
class boost::mysql::bound_statement_tuple
{
    friend class statement;
    friend struct detail::access;

    struct impl
    {
        statement stmt;
        WritableFieldTuple params;
    } impl_;

    template <typename TupleType>
    bound_statement_tuple(const statement& stmt, TupleType&& t) : impl_{stmt, std::forward<TupleType>(t)}
    {
    }
};

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
class boost::mysql::bound_statement_iterator_range
{
    friend class statement;
    friend struct detail::access;

    struct impl
    {
        statement stmt;
        FieldViewFwdIterator first;
        FieldViewFwdIterator last;
    } impl_;

    bound_statement_iterator_range(
        const statement& stmt,
        FieldViewFwdIterator first,
        FieldViewFwdIterator last
    )
        : impl_{stmt, first, last}
    {
    }
};

template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple, typename EnableIf>
boost::mysql::bound_statement_tuple<typename std::decay<WritableFieldTuple>::type> boost::mysql::statement::
    bind(WritableFieldTuple&& args) const

{
    BOOST_ASSERT(valid());
    return bound_statement_tuple<typename std::decay<WritableFieldTuple>::type>(
        *this,
        std::forward<WritableFieldTuple>(args)
    );
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator, typename EnableIf>
boost::mysql::bound_statement_iterator_range<FieldViewFwdIterator> boost::mysql::statement::bind(
    FieldViewFwdIterator first,
    FieldViewFwdIterator last
) const
{
    BOOST_ASSERT(valid());
    return bound_statement_iterator_range<FieldViewFwdIterator>(*this, first, last);
}

// Execution request traits
namespace boost {
namespace mysql {
namespace detail {

// Tuple
template <std::size_t N>
struct stmt_tuple_request_proxy
{
    statement stmt;
    std::array<field_view, N> params;

    operator any_execution_request() const
    {
        return any_execution_request({stmt.id(), static_cast<std::uint16_t>(stmt.num_params()), params});
    }
};

template <class... T>
struct execution_request_traits<bound_statement_tuple<std::tuple<T...>>>
{
    template <std::size_t... I>
    static std::array<field_view, sizeof...(T)> tuple_to_array(const std::tuple<T...>& t, mp11::index_sequence<I...>)
    {
        boost::ignore_unused(t);  // MSVC gets confused if sizeof...(T) == 0
        return {{to_field(std::get<I>(t))...}};
    }

    static stmt_tuple_request_proxy<sizeof...(T)> make_request(const bound_statement_tuple<std::tuple<T...>>& input, std::vector<field_view>&)
    {
        auto& impl = access::get_impl(input);
        return {impl.stmt, tuple_to_array(impl.params, mp11::make_index_sequence<sizeof...(T)>())};
    }
};

// Iterator range
template <class FieldViewFwdIterator>
struct execution_request_traits<bound_statement_iterator_range<FieldViewFwdIterator>>
{
    static any_execution_request make_request(
        const bound_statement_iterator_range<FieldViewFwdIterator>& input,
        std::vector<field_view>& shared_fields
    )
    {
        auto& impl = access::get_impl(input);
        shared_fields.assign(impl.first, impl.last);
        return any_execution_request(
            {impl.stmt.id(), static_cast<std::uint16_t>(impl.stmt.num_params()), shared_fields}
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
