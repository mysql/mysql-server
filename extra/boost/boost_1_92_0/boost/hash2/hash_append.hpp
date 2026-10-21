#ifndef BOOST_HASH2_HASH_APPEND_HPP_INCLUDED
#define BOOST_HASH2_HASH_APPEND_HPP_INCLUDED

// Copyright 2017, 2018, 2023, 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// Based on
//
// Types Don't Know #
// Howard E. Hinnant, Vinnie Falco, John Bytheway
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3980.html

#include <boost/hash2/hash_append_fwd.hpp>
#include <boost/hash2/is_contiguously_hashable.hpp>
#include <boost/hash2/has_constant_size.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/flavor.hpp>
#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/hash2/detail/bit_cast.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/has_tag_invoke.hpp>
#include <boost/container_hash/is_range.hpp>
#include <boost/container_hash/is_contiguous_range.hpp>
#include <boost/container_hash/is_unordered_range.hpp>
#include <boost/container_hash/is_tuple_like.hpp>
#include <boost/container_hash/is_described_class.hpp>
#include <boost/describe/bases.hpp>
#include <boost/describe/members.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/config.hpp>
#include <cstdint>
#include <type_traits>
#include <iterator>

#if !defined(BOOST_NO_CXX17_HDR_OPTIONAL)
# include <optional>
#endif

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)
# include <variant>
#endif

namespace boost
{

template<class T, std::size_t N> class array;

namespace hash2
{

// hash_append_range

namespace detail
{

template<class Hash, class Flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range_( Hash& h, Flavor const& f, It first, It last )
{
    for( ; first != last; ++first )
    {
        typename std::iterator_traits<It>::value_type const& v = *first;
        hash2::hash_append( h, f, v );
    }
}

template<class Hash, class Flavor> BOOST_CXX14_CONSTEXPR void hash_append_range_( Hash& h, Flavor const& /*f*/, unsigned char* first, unsigned char* last )
{
    h.update( first, last - first );
}

template<class Hash, class Flavor> BOOST_CXX14_CONSTEXPR void hash_append_range_( Hash& h, Flavor const& /*f*/, unsigned char const* first, unsigned char const* last )
{
    h.update( first, last - first );
}

#if defined(BOOST_NO_CXX14_CONSTEXPR)

template<class Hash, class Flavor, class T>
    typename std::enable_if<
        is_contiguously_hashable<T, Flavor::byte_order>::value, void >::type
    hash_append_range_( Hash& h, Flavor const& /*f*/, T* first, T* last )
{
    h.update( first, (last - first) * sizeof(T) );
}

#else

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if<
        is_contiguously_hashable<T, Flavor::byte_order>::value, void >::type
    hash_append_range_( Hash& h, Flavor const& f, T* first, T* last )
{
    if( !detail::is_constant_evaluated() )
    {
        h.update( first, (last - first) * sizeof(T) );
    }
    else
    {
        for( ; first != last; ++first )
        {
            hash2::hash_append( h, f, *first );
        }
    }
}

#endif

} // namespace detail

template<class Hash, class Flavor = default_flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range( Hash& h, Flavor const& f, It first, It last )
{
    detail::hash_append_range_( h, f, first, last );
}

// hash_append_size

template<class Hash, class Flavor = default_flavor, class T> BOOST_CXX14_CONSTEXPR void hash_append_size( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append( h, f, static_cast<typename Flavor::size_type>( v ) );
}

// hash_append_range_and_size

namespace detail
{

template<class Hash, class Flavor, class It> void BOOST_CXX14_CONSTEXPR hash_append_range_and_size_( Hash& h, Flavor const& f, It first, It last, std::input_iterator_tag )
{
    typename std::iterator_traits<It>::difference_type m = 0;

    for( ; first != last; ++first, ++m )
    {
        hash2::hash_append( h, f, *first );
    }

    hash2::hash_append_size( h, f, m );
}

template<class Hash, class Flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range_and_size_( Hash& h, Flavor const& f, It first, It last, std::random_access_iterator_tag )
{
    hash2::hash_append_range( h, f, first, last );
    hash2::hash_append_size( h, f, last - first );
}

} // namespace detail

template<class Hash, class Flavor = default_flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_range_and_size( Hash& h, Flavor const& f, It first, It last )
{
    detail::hash_append_range_and_size_( h, f, first, last, typename std::iterator_traits<It>::iterator_category() );
}

// hash_append_unordered_range

template<class Hash, class Flavor = default_flavor, class It> BOOST_CXX14_CONSTEXPR void hash_append_unordered_range( Hash& h, Flavor const& f, It first, It last )
{
    typename std::iterator_traits<It>::difference_type m = 0;

    std::uint64_t w = 0;

    for( ; first != last; ++first, ++m )
    {
        Hash h2( h );
        hash2::hash_append( h2, f, *first );

        w += hash2::get_integral_result<std::uint64_t>( h2 );
    }

    hash2::hash_append( h, f, w );
    hash2::hash_append_size( h, f, m );
}

// do_hash_append

namespace detail
{

// integral types

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< std::is_integral<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& /*f*/, T const& v )
{
    constexpr auto N = sizeof(T);

    unsigned char tmp[ N ] = {};
    detail::write( v, Flavor::byte_order, tmp );

    h.update( tmp, N );
}

// enum types

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< std::is_enum<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append( h, f, static_cast<typename std::underlying_type<T>::type>( v ) );
}

// pointer types
// never constexpr

template<class Hash, class Flavor, class T>
    typename std::enable_if< std::is_pointer<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append( h, f, reinterpret_cast<std::uintptr_t>( v ) );
}

// floating point

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< std::is_floating_point<T>::value && sizeof(T) == 4, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append( h, f, detail::bit_cast<std::uint32_t>( v + 0 ) );
}

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< std::is_floating_point<T>::value && sizeof(T) == 8, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append( h, f, detail::bit_cast<std::uint64_t>( v + 0 ) );
}

// std::nullptr_t
// not constexpr for consistency with T*

template<class Hash, class Flavor, class T>
    typename std::enable_if< std::is_same<T, std::nullptr_t>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append( h, f, static_cast<void*>( v ) );
}

// C arrays

template<class Hash, class Flavor, class T, std::size_t N> BOOST_CXX14_CONSTEXPR void do_hash_append( Hash& h, Flavor const& f, T const (&v)[ N ] )
{
    hash2::hash_append_range( h, f, v + 0, v + N );
}

// contiguous containers and ranges, w/ size

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< container_hash::is_contiguous_range<T>::value && !has_constant_size<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append_range( h, f, v.data(), v.data() + v.size() );
    hash2::hash_append_size( h, f, v.size() );
}

// containers and ranges, w/ size

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< container_hash::is_range<T>::value && !has_constant_size<T>::value && !container_hash::is_contiguous_range<T>::value && !container_hash::is_unordered_range<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append_range_and_size( h, f, v.begin(), v.end() );
}

#if defined(BOOST_MSVC)
# pragma warning(push)
# pragma warning(disable: 4702) // unreachable code
#endif

// constant size contiguous containers and ranges (std::array, boost::array, hash2::digest)

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< container_hash::is_contiguous_range<T>::value && has_constant_size<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    if( v.size() == 0 )
    {
        // A hash_append call must always result in a call to Hash::update
        hash2::hash_append( h, f, '\x00' );
    }
    else
    {
        // std::array<>::data() is only constexpr in C++17; boost::array<>::operator[] isn't constexpr
        hash2::hash_append_range( h, f, &v.front(), &v.front() + v.size() );
    }
}

// constant size non-contiguous containers and ranges

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< container_hash::is_range<T>::value && has_constant_size<T>::value && !container_hash::is_contiguous_range<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    if( v.begin() == v.end() )
    {
        // A hash_append call must always result in a call to Hash::update
        hash2::hash_append( h, f, '\x00' );
    }
    else
    {
        hash2::hash_append_range( h, f, v.begin(), v.end() );
    }
}

#if defined(BOOST_MSVC)
# pragma warning(pop)
#endif

// unordered containers (is_unordered_range implies is_range)

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< container_hash::is_unordered_range<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    hash2::hash_append_unordered_range( h, f, v.begin(), v.end() );
}

// tuple-likes

template<class Hash, class Flavor, class T, std::size_t... J> BOOST_CXX14_CONSTEXPR void hash_append_tuple( Hash& h, Flavor const& f, T const& v, mp11::integer_sequence<std::size_t, J...> )
{
    using std::get;
    int a[] = { ((void)hash2::hash_append( h, f, get<J>(v) ), 0)... };
    (void)a;
}

template<class Hash, class Flavor, class T> BOOST_CXX14_CONSTEXPR void hash_append_tuple( Hash& h, Flavor const& f, T const& /*v*/, mp11::integer_sequence<std::size_t> )
{
    // A hash_append call must always result in a call to Hash::update
    hash2::hash_append( h, f, '\x00' );
}

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< !container_hash::is_range<T>::value && container_hash::is_tuple_like<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    using Seq = mp11::make_index_sequence<std::tuple_size<T>::value>;
    detail::hash_append_tuple( h, f, v, Seq() );
}

// std::optional

#if !defined(BOOST_NO_CXX17_HDR_OPTIONAL)

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    void do_hash_append( Hash& h, Flavor const& f, std::optional<T> const& v )
{
    hash2::hash_append( h, f, v.has_value() );

    if( v )
    {
        hash2::hash_append( h, f, *v );
    }
}

#endif

// std::variant, std::monostate

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)

template<class Hash, class Flavor, class... T>
    BOOST_CXX14_CONSTEXPR
    void do_hash_append( Hash& h, Flavor const& f, std::variant<T...> const& v )
{
    if( v.valueless_by_exception() )
    {
        hash2::hash_append( h, f, '\x00' );
    }
    else
    {
        hash2::hash_append_size( h, f, v.index() );
        std::visit( [&]( auto const& w ){ hash2::hash_append( h, f, w ); }, v );
    }
}

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< std::is_same<T, std::monostate>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& /*v*/ )
{
    hash2::hash_append( h, f, '\x00' );
}

#endif

// described classes

#if defined(BOOST_DESCRIBE_CXX14)

#if defined(_MSC_VER) && _MSC_VER == 1900
# pragma warning(push)
# pragma warning(disable: 4100) // unreferenced formal parameter
#endif

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< container_hash::is_described_class<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    static_assert( !std::is_union<T>::value, "Described unions are not supported" );

    std::size_t r = 0;

    using Bd = describe::describe_bases<T, describe::mod_any_access>;

    mp11::mp_for_each<Bd>([&](auto D){

        using B = typename decltype(D)::type;
        hash2::hash_append( h, f, (B const&)v );
        ++r;

    });

    using Md = describe::describe_members<T, describe::mod_any_access>;

    mp11::mp_for_each<Md>([&](auto D){

        hash2::hash_append( h, f, v.*D.pointer );
        ++r;

    });

    // A hash_append call must always result in a call to Hash::update

    if( r == 0 )
    {
        hash2::hash_append( h, f, '\x00' );
    }
}

#if defined(_MSC_VER) && _MSC_VER == 1900
# pragma warning(pop)
#endif

#endif // defined(BOOST_DESCRIBE_CXX14)

// classes with tag_invoke

} // namespace detail

struct hash_append_tag
{
};

struct hash_append_provider
{
    template<class Hash, class Flavor, class T>
    static BOOST_CXX14_CONSTEXPR void hash_append( Hash& h, Flavor const& f, T const& v )
    {
        hash2::hash_append( h, f, v );
    }

    template<class Hash, class Flavor, class It>
    static BOOST_CXX14_CONSTEXPR void hash_append_range( Hash& h, Flavor const& f, It first, It last )
    {
        hash2::hash_append_range( h, f, first, last );
    }

    template<class Hash, class Flavor, class T>
    static BOOST_CXX14_CONSTEXPR void hash_append_size( Hash& h, Flavor const& f, T const& v )
    {
        hash2::hash_append_size( h, f, v );
    }

    template<class Hash, class Flavor, class It>
    static BOOST_CXX14_CONSTEXPR void hash_append_range_and_size( Hash& h, Flavor const& f, It first, It last )
    {
        hash2::hash_append_range_and_size( h, f, first, last );
    }

    template<class Hash, class Flavor, class It>
    static BOOST_CXX14_CONSTEXPR void hash_append_unordered_range( Hash& h, Flavor const& f, It first, It last )
    {
        hash2::hash_append_unordered_range( h, f, first, last );
    }
};

namespace detail
{

template<class Hash, class Flavor, class T>
    BOOST_CXX14_CONSTEXPR
    typename std::enable_if< detail::has_tag_invoke<T>::value, void >::type
    do_hash_append( Hash& h, Flavor const& f, T const& v )
{
    tag_invoke( hash_append_tag(), hash_append_provider(), h, f, &v );
}

} // namespace detail

// hash_append

template<class Hash, class Flavor = default_flavor, class T>
BOOST_CXX14_CONSTEXPR void hash_append( Hash& h, Flavor const& f, T const& v )
{
    if( !detail::is_constant_evaluated() && is_contiguously_hashable<T, Flavor::byte_order>::value )
    {
        h.update( &v, sizeof(T) );
    }
    else
    {
        detail::do_hash_append( h, f, v );
    }
}

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_HASH_APPEND_HPP_INCLUDED
