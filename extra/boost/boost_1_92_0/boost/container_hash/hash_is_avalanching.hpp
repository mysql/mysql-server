// Copyright 2025 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_HASH_HASH_IS_AVALANCHING_HPP_INCLUDED
#define BOOST_HASH_HASH_IS_AVALANCHING_HPP_INCLUDED

#include <type_traits>

namespace boost
{
namespace hash_detail
{

template<class... Ts> struct make_void
{
    using type = void;
};

template<class... Ts> using void_t = typename make_void<Ts...>::type;

template<class IsAvalanching> struct avalanching_value
{
    static constexpr bool value = IsAvalanching::value;
};

// may be explicitly marked as BOOST_DEPRECATED in the future
template<> struct avalanching_value<void>
{
    static constexpr bool value = true;
};

template<class Hash, class = void> struct hash_is_avalanching_impl: std::false_type
{
};

template<class Hash> struct hash_is_avalanching_impl<Hash, void_t<typename Hash::is_avalanching> >:
    std::integral_constant<bool, avalanching_value<typename Hash::is_avalanching>::value>
{
};

template<class Hash>
struct hash_is_avalanching_impl<Hash, typename std::enable_if< ((void)Hash::is_avalanching, true) >::type>
{
  // Hash::is_avalanching is not a type: we don't define value to produce
  // a compile error downstream
};

} // namespace hash_detail

template<class Hash> struct hash_is_avalanching: hash_detail::hash_is_avalanching_impl<Hash>::type
{
};

} // namespace boost

#endif // #ifndef BOOST_HASH_HASH_IS_AVALANCHING_HPP_INCLUDED
