// Copyright 2006 John Maddock
// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ASSOC_LAGUERRE_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ASSOC_LAGUERRE_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/laguerre.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T1,
          BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T2,
          BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T3>
constexpr auto assoc_laguerre_next(const unsigned n, const unsigned l, const T1 x, const T2 Pl, const T3 Plm1)
{
    using promoted_type = promote_args_t<T1, T2, T3>;
    return ((2 * n + l + 1 - static_cast<promoted_type>(x)) * static_cast<promoted_type>(Pl) - (n + l) * static_cast<promoted_type>(Plm1)) / (n+1);
}

template <typename T>
constexpr auto assoc_laguerre_impl(const unsigned n, const unsigned m, const T x)
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    // Special cases:
    if(m == 0)
    {
        return boost::decimal::laguerre(n, x);
    }

    T p0 {UINT64_C(1)};

    if(n == 0)
    {
        return p0;
    }

    T p1 {m + 1 - x};

    unsigned c {1U};

    while(c < n)
    {
        std::swap(p0, p1);
        p1 = static_cast<T>(assoc_laguerre_next(c, m, x, p0, p1));
        ++c;
    }

    return p1;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto assoc_laguerre(const unsigned n, const unsigned m, const T x)
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::assoc_laguerre_impl(n, m, static_cast<evaluation_type>(x)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ASSOC_LAGUERRE_HPP
