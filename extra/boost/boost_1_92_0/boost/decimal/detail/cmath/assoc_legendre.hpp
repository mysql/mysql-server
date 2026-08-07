//  (C) Copyright John Maddock 2006.
//  (C) Copyright Matt Borland 2024.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ASSOC_LEGENDRE_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ASSOC_LEGENDRE_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/cmath/pow.hpp>
#include <boost/decimal/detail/cmath/sqrt.hpp>
#include <boost/decimal/detail/cmath/legendre.hpp>
#include <boost/decimal/detail/cmath/impl/assoc_legendre_lookup.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <utility>
#include <type_traits>
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T1,
          BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T2,
          BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T3>
constexpr auto assoc_legendre_next(const unsigned l, const unsigned m, const T1 x, const T2 Pl, const T3 Plm1) noexcept
{
    using result_type = promote_args_t<T1, T2, T3>;
    return ((2 * l + 1) * static_cast<result_type>(x) * static_cast<result_type>(Pl) - (l + m) * static_cast<result_type>(Plm1)) / (l + 1 - m);
}

// Implement Legendre P and Q polynomials via recurrence:
template <typename T>
constexpr auto assoc_legendre_impl(const unsigned l, const unsigned m, const T x, const T sin_theta_power) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    if (x < -1 || x > 1 || l > 128)
    {
        return std::numeric_limits<T>::quiet_NaN();
    }
    else if (isnan(x))
    {
        return x;
    }

    if (l == 1 && m == 0)
    {
        return x;
    }
    else if (m > l)
    {
        return T{0};
    }
    else if (m == 0)
    {
        return legendre(l, x);
    }

    // TODO(mborland): Once the lookup table has been exceeded we can calculate m!! but that is far more
    // complicated and computationally expensive
    BOOST_DECIMAL_ASSERT_MSG(m <= 50, "m > 50 has not been implemented");
    T p0 = assoc_legendre_p0_lookup<T>(2 * m - 1) * sin_theta_power;

    if (m & 1)
    {
        p0 = -p0;
    }

    if (m == l)
    {
        return p0;
    }

    T p1 = x * (2 * m + 1) * p0;

    auto n = m + 1;

    while (n < l)
    {
        std::swap(p0, p1);
        p1 = assoc_legendre_next(n, m, x, p0, p1);
        ++n;
    }

    return p1;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto assoc_legendre(const unsigned n, const unsigned m, const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::assoc_legendre_impl(n, m, static_cast<evaluation_type>(x),
                                       pow(1 - static_cast<evaluation_type>(x)*static_cast<evaluation_type>(x),
                                       evaluation_type{m} / 2)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ASSOC_LEGENDRE_HPP
