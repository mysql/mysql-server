//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/typing/meta_check_context.hpp>
#include <boost/mysql/detail/typing/pos_map.hpp>
#include <boost/mysql/detail/typing/readable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/describe/members.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

//
// Base templates. Every StaticRow type must specialize the row_traits class,
// providing the following members:
//
//    // type of the actual row to be parsed. This supports marker types, like pfr_by_name
//    using underlying_row_type = /* */;
//
//    // MP11 type list with the row's member types
//    using field_types = /* */;
//
//    static constexpr name_table_t name_table() noexcept; // field names
//
//    template <class F> /* Apply F to each member */
//    static void for_each_member(underlying_row_t<StaticRow>& to, F&& function);
//

//
struct row_traits_is_unspecialized
{
};

template <class T, bool is_describe_struct = describe::has_describe_members<T>::value>
class row_traits : public row_traits_is_unspecialized
{
};

//
// Describe structs
//
// Workaround std::array::data not being constexpr in C++14
template <class T, std::size_t N>
struct array_wrapper
{
    T data_[N];

    constexpr boost::span<const T> span() const noexcept { return boost::span<const T>(data_); }
};

template <class T>
struct array_wrapper<T, 0>
{
    struct
    {
    } data_;  // allow empty brace initialization

    constexpr boost::span<const T> span() const noexcept { return boost::span<const T>(); }
};

// Workaround for char_traits::length not being constexpr in C++14
// Only used to retrieve Describe member name lengths
constexpr std::size_t get_length(const char* s) noexcept
{
    const char* p = s;
    while (*p)
        ++p;
    return p - s;
}

template <class DescribeStruct>
using row_members = describe::
    describe_members<DescribeStruct, describe::mod_public | describe::mod_inherited>;

template <template <class...> class ListType, class... MemberDescriptor>
constexpr array_wrapper<string_view, sizeof...(MemberDescriptor)> get_describe_names(ListType<
                                                                                     MemberDescriptor...>)
{
    return {{string_view(MemberDescriptor::name, get_length(MemberDescriptor::name))...}};
}

template <class DescribeStruct>
BOOST_INLINE_CONSTEXPR auto describe_names_storage = get_describe_names(row_members<DescribeStruct>{});

template <class DescribeStruct>
class row_traits<DescribeStruct, true>
{
    // clang-format off
    template <class D>
    using descriptor_to_type = typename
        std::remove_reference<decltype(std::declval<DescribeStruct>().*std::declval<D>().pointer)>::type;
    // clang-format on

public:
    using underlying_row_type = DescribeStruct;
    using field_types = mp11::mp_transform<descriptor_to_type, row_members<DescribeStruct>>;

    static constexpr name_table_t name_table() noexcept
    {
        return describe_names_storage<DescribeStruct>.span();
    }

    template <class F>
    static void for_each_member(DescribeStruct& to, F&& function)
    {
        mp11::mp_for_each<row_members<DescribeStruct>>([function, &to](auto D) { function(to.*D.pointer); });
    }
};

//
// Tuples
//
template <class... ReadableField>
class row_traits<std::tuple<ReadableField...>, false>
{
public:
    using underlying_row_type = std::tuple<ReadableField...>;
    using field_types = std::tuple<ReadableField...>;
    static constexpr name_table_t name_table() noexcept { return name_table_t(); }

    template <class F>
    static void for_each_member(underlying_row_type& to, F&& function)
    {
        mp11::tuple_for_each(to, std::forward<F>(function));
    }
};

//
// Helpers to implement the external interface section
//

// Helpers to check that all the fields satisfy ReadableField
// and produce meaningful error messages, with the offending field type, at least
// Workaround clang 3.6 not liking generic lambdas in the below constexpr function
struct readable_field_checker
{
    template <class TypeIdentity>
    constexpr void operator()(TypeIdentity) const noexcept
    {
        using T = typename TypeIdentity::type;
        static_assert(
            is_readable_field<T>::value,
            "You're trying to use an unsupported field type in a row type. Review your row type definitions."
        );
    }
};

template <class TypeList>
static constexpr bool check_readable_field() noexcept
{
    mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, TypeList>>(readable_field_checker{});
    return true;
}

// Centralize where check_readable_field happens
template <class StaticRow>
struct row_traits_with_check : row_traits<StaticRow>
{
    static_assert(check_readable_field<typename row_traits<StaticRow>::field_types>(), "");
};

// Meta checking
struct meta_check_field_fn
{
    meta_check_context& ctx;

    template <class TypeIdentity>
    void operator()(TypeIdentity)
    {
        meta_check_field<typename TypeIdentity::type>(ctx);
    }
};

// Useful for testing
template <class ReadableFieldList>
error_code meta_check_impl(
    name_table_t name_table,
    span<const std::size_t> pos_map,
    metadata_collection_view meta,
    diagnostics& diag
)
{
    BOOST_ASSERT(pos_map.size() == mp11::mp_size<ReadableFieldList>::value);
    meta_check_context ctx(pos_map, name_table, meta);
    mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, ReadableFieldList>>(meta_check_field_fn{ctx});
    return ctx.check_errors(diag);
}

// Parsing
class parse_context
{
    span<const std::size_t> pos_map_;
    span<const field_view> fields_;
    std::size_t index_{};
    error_code ec_;

public:
    parse_context(span<const std::size_t> pos_map, span<const field_view> fields) noexcept
        : pos_map_(pos_map), fields_(fields)
    {
    }

    template <class ReadableField>
    void parse(ReadableField& output)
    {
        auto ec = readable_field_traits<ReadableField>::parse(
            map_field_view(pos_map_, index_++, fields_),
            output
        );
        if (!ec_)
            ec_ = ec;
    }

    error_code error() const noexcept { return ec_; }
};

// Using this instead of a lambda reduces the number of generated instantiations
struct parse_functor
{
    parse_context& ctx;

    template <class ReadableField>
    void operator()(ReadableField& output) const
    {
        ctx.parse(output);
    }
};

//
// External interface. Other Boost.MySQL components should never use row_traits
// directly, but the functions below, instead.
//
template <class T>
BOOST_INLINE_CONSTEXPR bool
    is_static_row = !std::is_base_of<row_traits_is_unspecialized, row_traits<T>>::value;

#ifdef BOOST_MYSQL_HAS_CONCEPTS

// Note that static_row only inspects the shape of the row only (i.e. it's a tuple vs. it's nothing we know),
// and not individual fields. These are static_assert-ed in individual row_traits. This gives us an error
// message that contains the offending types, at least.
template <class T>
concept static_row = is_static_row<T>;

#define BOOST_MYSQL_STATIC_ROW ::boost::mysql::detail::static_row

#else
#define BOOST_MYSQL_STATIC_ROW class
#endif

template <BOOST_MYSQL_STATIC_ROW StaticRow>
using underlying_row_t = typename row_traits_with_check<StaticRow>::underlying_row_type;

template <BOOST_MYSQL_STATIC_ROW StaticRow>
constexpr std::size_t get_row_size() noexcept
{
    return mp11::mp_size<typename row_traits_with_check<StaticRow>::field_types>::value;
}

template <BOOST_MYSQL_STATIC_ROW StaticRow>
constexpr name_table_t get_row_name_table() noexcept
{
    return row_traits_with_check<StaticRow>::name_table();
}

template <BOOST_MYSQL_STATIC_ROW StaticRow>
error_code meta_check(span<const std::size_t> pos_map, metadata_collection_view meta, diagnostics& diag)
{
    using field_types = typename row_traits_with_check<StaticRow>::field_types;
    BOOST_ASSERT(pos_map.size() == get_row_size<StaticRow>());
    return meta_check_impl<field_types>(get_row_name_table<StaticRow>(), pos_map, meta, diag);
}

template <BOOST_MYSQL_STATIC_ROW StaticRow>
error_code parse(
    span<const std::size_t> pos_map,
    span<const field_view> from,
    underlying_row_t<StaticRow>& to
)
{
    BOOST_ASSERT(pos_map.size() == get_row_size<StaticRow>());
    BOOST_ASSERT(from.size() >= get_row_size<StaticRow>());
    parse_context ctx(pos_map, from);
    row_traits_with_check<StaticRow>::for_each_member(to, parse_functor{ctx});
    return ctx.error();
}

using meta_check_fn_t =
    error_code (*)(span<const std::size_t> field_map, metadata_collection_view meta, diagnostics& diag);

// For multi-resultset
template <class... StaticRow>
BOOST_INLINE_CONSTEXPR std::size_t max_num_columns = (std::max)({get_row_size<StaticRow>()...});

BOOST_INLINE_CONSTEXPR std::size_t index_not_found = static_cast<std::size_t>(-1);

template <class UnderlyingRowType, class... RowType>
constexpr std::size_t get_type_index() noexcept
{
    using lunique = mp11::mp_unique<mp11::mp_list<underlying_row_t<RowType>...>>;
    using index_t = mp11::mp_find<lunique, UnderlyingRowType>;
    return index_t::value < mp11::mp_size<lunique>::value ? index_t::value : index_not_found;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif  // BOOST_MYSQL_CXX14

#endif
