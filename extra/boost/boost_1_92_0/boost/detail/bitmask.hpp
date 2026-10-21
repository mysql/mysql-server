//  boost/detail/bitmask.hpp  ------------------------------------------------//

//  Copyright Beman Dawes 2006
//  Copyright Andrey Semashev 2025

//  Distributed under the Boost Software License, Version 1.0
//  http://www.boost.org/LICENSE_1_0.txt

//  Usage:  enum foo { a=1, b=2, c=4 };
//          BOOST_BITMASK( foo )
//
//          void f( foo arg );
//          ...
//          f( a | c );
//
//  See [bitmask.types] in the C++ standard for the formal specification

#ifndef BOOST_BITMASK_HPP
#define BOOST_BITMASK_HPP

#include <boost/config.hpp>

#if defined(__has_builtin)
#if __has_builtin(__underlying_type)
#define BOOST_BITMASK_DETAIL_UNDERLYING_TYPE(enum_type) __underlying_type(enum_type)
#endif
#endif

#if !defined(BOOST_BITMASK_DETAIL_UNDERLYING_TYPE) && \
    ((defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION >= 40700)) || (defined(_MSC_VER) && (_MSC_VER >= 1700)))
#define BOOST_BITMASK_DETAIL_UNDERLYING_TYPE(enum_type) __underlying_type(enum_type)
#endif

#if !defined(BOOST_BITMASK_DETAIL_UNDERLYING_TYPE)
#include <type_traits>
#endif

namespace boost {
namespace detail {
namespace bitmask {

#if defined(BOOST_BITMASK_DETAIL_UNDERLYING_TYPE)
template< typename Enum >
using underlying_type_t = BOOST_BITMASK_DETAIL_UNDERLYING_TYPE(Enum);
#elif (BOOST_CXX_VERSION >= 201402)
using std::underlying_type_t;
#else
template< typename Enum >
using underlying_type_t = typename std::underlying_type< Enum >::type;
#endif

}}}

#undef BOOST_BITMASK_DETAIL_UNDERLYING_TYPE

#define BOOST_BITMASK(Bitmask)                                                                                  \
                                                                                                                \
  inline BOOST_CONSTEXPR Bitmask operator| (Bitmask x, Bitmask y) BOOST_NOEXCEPT                                \
  { return static_cast< Bitmask >(static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(x)      \
      | static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(y)); }                            \
                                                                                                                \
  inline BOOST_CONSTEXPR Bitmask operator& (Bitmask x, Bitmask y) BOOST_NOEXCEPT                                \
  { return static_cast< Bitmask >(static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(x)      \
      & static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(y)); }                            \
                                                                                                                \
  inline BOOST_CONSTEXPR Bitmask operator^ (Bitmask x, Bitmask y) BOOST_NOEXCEPT                                \
  { return static_cast< Bitmask >(static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(x)      \
      ^ static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(y)); }                            \
                                                                                                                \
  inline BOOST_CONSTEXPR Bitmask operator~ (Bitmask x) BOOST_NOEXCEPT                                           \
  { return static_cast< Bitmask >(~static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(x)); } \
                                                                                                                \
  inline BOOST_CXX14_CONSTEXPR Bitmask& operator&=(Bitmask& x, Bitmask y) BOOST_NOEXCEPT                        \
  { x = x & y; return x; }                                                                                      \
                                                                                                                \
  inline BOOST_CXX14_CONSTEXPR Bitmask& operator|=(Bitmask& x, Bitmask y) BOOST_NOEXCEPT                        \
  { x = x | y; return x; }                                                                                      \
                                                                                                                \
  inline BOOST_CXX14_CONSTEXPR Bitmask& operator^=(Bitmask& x, Bitmask y) BOOST_NOEXCEPT                        \
  { x = x ^ y; return x; }                                                                                      \
                                                                                                                \
  /* Boost extensions to [bitmask.types] */                                                                     \
                                                                                                                \
  inline BOOST_CONSTEXPR bool operator!(Bitmask x) BOOST_NOEXCEPT                                               \
  { return !static_cast< ::boost::detail::bitmask::underlying_type_t< Bitmask > >(x); }                         \
                                                                                                                \
  BOOST_DEPRECATED("bitmask_set(enum) is deprecated, use !!enum or comparison operators instead")               \
  inline BOOST_CONSTEXPR bool bitmask_set(Bitmask x) BOOST_NOEXCEPT                                             \
  { return !!x; }

#endif // BOOST_BITMASK_HPP
