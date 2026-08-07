//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_FORMAT_SQL_HPP
#define BOOST_MYSQL_DETAIL_FORMAT_SQL_HPP

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/writable_field_traits.hpp>

#include <iterator>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

// Forward decls
template <class T>
struct formatter;

class format_context_base;
class formattable_ref;
class format_arg;

namespace detail {

class format_state;

struct formatter_is_unspecialized
{
};

template <class T>
constexpr bool has_specialized_formatter()
{
    return !std::is_base_of<formatter_is_unspecialized, formatter<typename std::decay<T>::type>>::value;
}

template <class T>
struct is_writable_field_ref : is_writable_field<typename std::decay<T>::type>
{
};

template <class T>
struct is_formattable_ref : std::is_same<typename std::decay<T>::type, formattable_ref>
{
};

// Is T suitable for being the element type of a formattable range?
template <class T>
constexpr bool is_formattable_range_elm_type()
{
    return is_writable_field_ref<T>::value || has_specialized_formatter<T>() || is_formattable_ref<T>::value;
}

template <class T, class = void>
struct is_formattable_range : std::false_type
{
};

// Note: T might be a reference.
// Using T& + reference collapsing gets the right semantics for non-const ranges
template <class T>
struct is_formattable_range<
    T,
    typename std::enable_if<
        // std::begin and std::end can be called on it, and we can compare values
        std::is_convertible<decltype(std::begin(std::declval<T&>()) != std::end(std::declval<T&>())), bool>::
            value &&

        // value_type is either a writable field or a type with a specialized formatter.
        // We don't support sequences of sequences out of the box (no known use case)
        is_formattable_range_elm_type<decltype(*std::begin(std::declval<T&>()))>()

        // end of conditions
        >::type> : std::true_type
{
};

template <class T>
constexpr bool is_formattable_type()
{
    return is_formattable_range_elm_type<T>() || is_formattable_range<T>::value;
}

#ifdef BOOST_MYSQL_HAS_CONCEPTS

// If you're getting an error referencing this concept,
// it means that you are attempting to format a type that doesn't support it.
template <class T>
concept formattable =
    // This covers basic types and optionals
    is_writable_field_ref<T>::value ||
    // This covers custom types that specialized boost::mysql::formatter
    has_specialized_formatter<T>() ||
    // This covers ranges of formattable types
    is_formattable_range<T>::value ||
    // This covers passing formattable_ref as a format argument
    is_formattable_ref<T>::value;

#define BOOST_MYSQL_FORMATTABLE ::boost::mysql::detail::formattable

#else

#define BOOST_MYSQL_FORMATTABLE class

#endif

// A type-erased argument passed to format. Built-in types are passed
// directly in the struct (as a field_view), instead of by pointer,
// to reduce the number of do_format instantiations
struct formattable_ref_impl
{
    enum class type_t
    {
        field,
        field_with_specs,
        fn_and_ptr
    };

    struct fn_and_ptr
    {
        const void* obj;
        bool (*format_fn)(const void*, const char*, const char*, format_context_base&);
    };

    union data_t
    {
        field_view fv;
        fn_and_ptr custom;

        data_t(field_view fv) noexcept : fv(fv) {}
        data_t(fn_and_ptr v) noexcept : custom(v) {}
    };

    type_t type;
    data_t data;
};

// Create a type-erased formattable_ref_impl from a formattable value
template <class T>
formattable_ref_impl make_formattable_ref(T&& v);

BOOST_MYSQL_DECL
void vformat_sql_to(format_context_base& ctx, constant_string_view format_str, span<const format_arg> args);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
