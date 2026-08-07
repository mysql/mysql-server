#ifndef BOOST_HASH2_DETAIL_HAS_TAG_INVOKE_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_HAS_TAG_INVOKE_HPP_INCLUDED

// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/endian.hpp>
#include <type_traits>
#include <cstdint>
#include <cstddef>

namespace boost
{
namespace hash2
{

struct hash_append_tag;

namespace detail
{

struct provider_archetype
{
    template<class Hash, class Flavor, class T> static void hash_append( Hash& h, Flavor const& f, T const& v );
    template<class Hash, class Flavor, class It> static void hash_append_range( Hash& h, Flavor const& f, It first, It last );
    template<class Hash, class Flavor, class T> static void hash_append_size( Hash& h, Flavor const& f, T const& v );
    template<class Hash, class Flavor, class It> static void hash_append_range_and_size( Hash& h, Flavor const& f, It first, It last );
    template<class Hash, class Flavor, class It> static void hash_append_unordered_range( Hash& h, Flavor const& f, It first, It last );
};

struct hash_archetype
{
    using result_type = std::uint64_t;

    hash_archetype();
    explicit hash_archetype( std::uint64_t );
    hash_archetype( void const*, std::size_t );

    void update( void const*, std::size_t );
    result_type result();
};

struct flavor_archetype
{
    using size_type = std::uint32_t;
    static constexpr auto byte_order = endian::native;
};

template<class T, class En = void> struct has_tag_invoke: std::false_type
{
};

template<class T> struct has_tag_invoke<T, decltype(
    tag_invoke( std::declval<hash_append_tag const&>(), std::declval<provider_archetype&>(), std::declval<hash_archetype&>(), std::declval<flavor_archetype const&>(), std::declval<T const*>() ),
    void())>: std::true_type
{
};

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_HAS_TAG_INVOKE_HPP_INCLUDED
