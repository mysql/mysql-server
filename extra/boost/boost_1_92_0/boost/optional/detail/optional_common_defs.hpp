// Copyright (C) 2024 Ryan Malcolm Underwood.
// Copyright (C) 2026 Andrzej Krzemieński.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/optional for documentation.
//
// You are welcome to contact the authors at:
//  akrzemi1@gmail.com
//  typenametea@gmail.com
//
//
// This header provides definitions required by any specialization of
// optional<>.

#ifndef BOOST_OPTIONAL_DETAIL_OPTIONAL_COMMON_DEFS_01FEB2026_HPP
#define BOOST_OPTIONAL_DETAIL_OPTIONAL_COMMON_DEFS_01FEB2026_HPP


#include <boost/config.hpp>
#include <boost/core/addressof.hpp>
#include <type_traits>
#include <boost/optional/detail/optional_factory_support.hpp>

#ifndef BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
#include <boost/type_traits/decay.hpp>
#include <boost/type_traits/is_base_of.hpp>
#endif

// This is needed for C++11, where constexpr functions must contain a single expression.
// We want to assert and then return.
#if defined NDEBUG
# define BOOST_OPTIONAL_ASSERTED_EXPRESSION(CHECK, EXPR) (EXPR)
#else
# define BOOST_OPTIONAL_ASSERTED_EXPRESSION(CHECK, EXPR) ((CHECK) ? (EXPR) : ([]{BOOST_ASSERT(!(#CHECK));}(), (EXPR)))
#endif

#ifdef BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
# define BOOST_OPTIONAL_DECAY(T) typename ::std::decay<T>::type
# define BOOST_OPTIONAL_IS_TAGGED(TAG, U) ::std::is_base_of<TAG, BOOST_OPTIONAL_DECAY(U)>
#else
# define BOOST_OPTIONAL_DECAY(T) BOOST_DEDUCED_TYPENAME boost::decay<T>::type
# define BOOST_OPTIONAL_IS_TAGGED(TAG, U) boost::is_base_of<TAG, BOOST_OPTIONAL_DECAY(U)>
#endif

namespace boost {

template <typename T> class optional;


// Boost-wide tags for recognizing "factories": a C++03 workaround
// for perfect forwarding.
class in_place_factory_base;
class typed_in_place_factory_base;

} // namespace boost


// Traits for recognizing in-place factories
namespace boost { namespace optional_detail {

template <typename U>
struct is_in_place_factory : BOOST_OPTIONAL_IS_TAGGED(boost::in_place_factory_base, U) {};

template <typename U>
struct is_typed_in_place_factory : BOOST_OPTIONAL_IS_TAGGED(boost::typed_in_place_factory_base, U) {};

}}



/** This is a set of declarations that repeat those from the Standard Library
    header <utility> but without having to drag its entire content. They also
    add missing capabilities, like constexpr, in older compiler versions.
 */
namespace boost { namespace optional_detail {

template <typename T>
T declval_();

template <class T>
inline constexpr T&& forward_(typename ::std::remove_reference<T>::type& t) noexcept
{
  return static_cast<T&&>(t);
}

template <class T>
inline constexpr T&& forward_(typename ::std::remove_reference<T>::type&& t) noexcept
{
  static_assert(!::std::is_lvalue_reference<T>::value, "Can not forward an rvalue as an lvalue.");
  return static_cast<T&&>(t);
}

template <class T>
inline constexpr typename ::std::remove_reference<T>::type&& move_(T&& t) noexcept
{
  return static_cast<typename ::std::remove_reference<T>::type&&>(t);
}

}} // namespace boost::optional_detail


/** This is a set of declarations that are not part of this library's interface.
    They are implementation details.
 */
namespace boost { namespace optional_detail {

/** This struct is used for tagging types that want to be recognized as
    `optional`. If your class inherits directly or indirectly from `optional_tag`
    the type traits and overloads will treat it as `optional<>`.
 */
struct optional_tag {};

/** `optional_value_type`: given type `X`:
    * if `X` is an instance of `boost::optional`, returns its value_type,
    * otherwise we get a SFINAE-able error.
 */
template <typename X>
struct optional_value_type
{
};

template <typename U>
struct optional_value_type< ::boost::optional<U> >
{
  typedef U type;
};


/** This is an approximation of a 1-argument C++17 std::invoke_result.
 */
template <typename F, typename Ref, typename Rslt = decltype(declval_<F>()(declval_<Ref>()))>
struct result_of
{
  typedef Rslt type;
};

/** This type trait returns the following given the expression `f(ref)`:
     * if the result is a specialization of `boost::optional`: its value_type,
     * otherwise a SFINAE-able error.
 */
template <typename F, typename Ref, typename Rslt = typename optional_value_type<typename result_of<F, Ref>::type>::type>
struct result_value_type
{
  typedef Rslt type;
};

}} // namespace boost::optional_detail


/** The following two tags are intended to be used by library users.
    The additional namespace is used in order to prevent the ADL from
    dragging all functions from namespace `boost` in any unqualified name lookup
    when these tags are involved.
 */
namespace boost {

namespace optional_ns {

/// a tag for in-place initialization of contained value
struct in_place_init_t
{
  struct init_tag{};
  BOOST_CONSTEXPR explicit in_place_init_t(init_tag){}
};
BOOST_INLINE_CONSTEXPR in_place_init_t in_place_init ((in_place_init_t::init_tag()));

/// a tag for conditional in-place initialization of contained value
struct in_place_init_if_t
{
  struct init_tag{};
  BOOST_CONSTEXPR explicit in_place_init_if_t(init_tag){}
};
BOOST_INLINE_CONSTEXPR in_place_init_if_t in_place_init_if ((in_place_init_if_t::init_tag()));

} // namespace optional_ns

using optional_ns::in_place_init_t;
using optional_ns::in_place_init;
using optional_ns::in_place_init_if_t;
using optional_ns::in_place_init_if;

} // namespace boost


#endif // BOOST_OPTIONAL_DETAIL_OPTIONAL_COMMON_DEFS_01FEB2026_HPP
