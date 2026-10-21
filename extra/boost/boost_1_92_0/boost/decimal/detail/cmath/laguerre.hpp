// Copyright 2006 John Maddock
// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_LAGUERRE_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_LAGUERRE_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/config.hpp>

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
constexpr auto laguerre_next(const unsigned n, const T1 x, const T2 Ln, const T3 Lnm1)
{
    using promoted_type = promote_args_t<T1, T2, T3>;
    return ((2 * n + 1 - static_cast<promoted_type>(x)) * static_cast<promoted_type>(Ln) - n * static_cast<promoted_type>(Lnm1)) / (n + 1);
}

// Implement Laguerre polynomials via recurrence:
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto laguerre_impl(const unsigned n, const T x)
{
    T p0 {UINT64_C(1)};
    T p1 {UINT64_C(1) - x};

    if (n == 0)
    {
        return p0;
    }

    unsigned c = 1;

    while(c < n)
    {
        std::swap(p0, p1);
        p1 = laguerre_next(c, x, p0, p1);
        ++c;
    }

    return p1;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto laguerre(const unsigned n, const T x)
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::laguerre_impl(n, static_cast<evaluation_type>(x)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_LAGUERRE_HPP
