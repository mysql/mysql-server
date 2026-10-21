//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FORMAT_SQL_HPP
#define BOOST_MYSQL_IMPL_FORMAT_SQL_HPP

#pragma once

#include <boost/mysql/format_sql.hpp>

#include <boost/mysql/detail/format_sql.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_DECL
std::pair<bool, string_view> parse_range_specifiers(const char* spec_begin, const char* spec_end);

// To use with arguments with a custom formatter
template <class T>
bool do_format_custom_formatter(
    const void* obj,
    const char* spec_begin,
    const char* spec_end,
    format_context_base& ctx
)
{
    // T here may be the actual type U or const U
    using U = typename std::remove_const<T>::type;

    formatter<U> fmt;

    // Parse the spec
    const char* it = fmt.parse(spec_begin, spec_end);
    if (it != spec_end)
    {
        return false;
    }

    // Retrieve the object
    auto& value = *const_cast<T*>(static_cast<const T*>(obj));

    // Format
    fmt.format(value, ctx);

    // Done
    return true;
}

// To use with ranges
template <class T>
bool do_format_range(const void* obj, const char* spec_begin, const char* spec_end, format_context_base& ctx)
{
    // Parse specifiers
    auto res = detail::parse_range_specifiers(spec_begin, spec_end);
    if (!res.first)
        return false;
    auto spec = runtime(res.second);

    // Retrieve the object. T here may be the actual type U or const U
    auto& value = *const_cast<T*>(static_cast<const T*>(obj));

    // Output the sequence
    bool is_first = true;
    for (auto it = std::begin(value); it != std::end(value); ++it)
    {
        if (!is_first)
            ctx.append_raw(", ");
        is_first = false;
        ctx.append_value(*it, spec);
    }
    return true;
}

// Make formattable_ref formattable
inline formattable_ref_impl make_formattable_ref_custom(
    formattable_ref v,
    std::true_type  // is format ref
)
{
    return access::get_impl(v);
}

// Make types with custom formatters formattable
template <class T>
formattable_ref_impl make_formattable_ref_custom(
    T&& v,
    std::false_type  // is format ref
)
{
    // If you're getting an error here, it means that you're passing a type
    // that is not formattable to a SQL formatting function.
    static_assert(
        has_specialized_formatter<T>(),
        "T is not formattable. Please use a formattable type or specialize formatter<T> to make it "
        "formattable"
    );

    // Although everything is passed as const void*, do_format_custom_formatter
    // can bypass const-ness for non-const values. This helps with non-const ranges (e.g. filter_view)
    return {
        formattable_ref_impl::type_t::fn_and_ptr,
        formattable_ref_impl::
            fn_and_ptr{&v, &do_format_custom_formatter<typename std::remove_reference<T>::type>}
    };
}

// Make ranges formattable
template <class T>
formattable_ref_impl make_formattable_ref_range(
    T&& v,
    std::true_type  // formattable range
)
{
    // Although everything is passed as const void*, do_format_range
    // can bypass const-ness for non-const ranges (e.g. filter_view)
    return {
        formattable_ref_impl::type_t::fn_and_ptr,
        formattable_ref_impl::fn_and_ptr{&v, &do_format_range<typename std::remove_reference<T>::type>}
    };
}

template <class T>
formattable_ref_impl make_formattable_ref_range(
    T&& v,
    std::false_type  // formattable range
)
{
    return make_formattable_ref_custom(std::forward<T>(v), is_formattable_ref<T>());
}

// Used for types having is_writable_field<T>
template <class T>
formattable_ref_impl make_formattable_ref_writable(
    const T& v,
    std::true_type  // is_writable_field
)
{
    // Only string types (and not field_views or optionals) support the string specifiers
    return {
        std::is_convertible<T, string_view>::value ? formattable_ref_impl::type_t::field_with_specs
                                                   : formattable_ref_impl::type_t::field,
        to_field(v)
    };
}

template <class T>
formattable_ref_impl make_formattable_ref_writable(
    T&& v,
    std::false_type  // is_writable_field
)
{
    return make_formattable_ref_range(std::forward<T>(v), is_formattable_range<T>());
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class T>
boost::mysql::detail::formattable_ref_impl boost::mysql::detail::make_formattable_ref(T&& v)
{
    // Hierarchy:
    //    1. writable field?
    //    2. formattable range?
    //    3. custom formatter or formattable_ref?
    return make_formattable_ref_writable(std::forward<T>(v), is_writable_field_ref<T>());
}

template <BOOST_MYSQL_FORMATTABLE... Formattable>
std::string boost::mysql::format_sql(
    format_options opts,
    constant_string_view format_str,
    Formattable&&... args
)
{
    std::initializer_list<format_arg> args_il{
        {string_view(), std::forward<Formattable>(args)}
        ...
    };
    return format_sql(opts, format_str, args_il);
}

#endif
