//  Copyright 2002 2011, 2024 Christopher Kormanyos
//  Copyright 2024 Matt Borland
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ELLINT_1_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ELLINT_1_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/ellint_impl.hpp>
#include <boost/decimal/detail/cmath/atan.hpp>
#include <boost/decimal/detail/cmath/fabs.hpp>
#include <boost/decimal/detail/cmath/log.hpp>
#include <boost/decimal/detail/cmath/sin.hpp>
#include <boost/decimal/detail/cmath/sqrt.hpp>
#include <boost/decimal/detail/cmath/tan.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto ellint_1_impl(const T m, const T phi) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
  constexpr T one { 1 };

  T result { };

  const auto fpc_m   = fpclassify(m);
  const auto fpc_phi = fpclassify(phi);

  if((fpc_m == FP_ZERO) && (fpc_phi == FP_NORMAL))
  {
    result = phi;
  }
  else if((fpc_phi == FP_ZERO) && (fpc_m == FP_NORMAL))
  {
    constexpr T zero { 0 };

    result = zero;
  }
  else if((fabs(m) > one) || (fpc_phi != FP_NORMAL) || (fpc_m != FP_NORMAL))
  {
    #ifndef BOOST_DECIMAL_FAST_MATH
    result = std::numeric_limits<T>::quiet_NaN();
    #else
    result = T{0};
    #endif
  }
  else if(signbit(phi))
  {
    result = -ellint_1_impl(m, -phi);
  }
  else if(signbit(m))
  {
    result = ellint_1_impl(-m, phi);
  }
  else
  {
    constexpr T small_phi_limit =
          std::numeric_limits<T>::digits10 < 10 ? T { 1, -2 }
        : std::numeric_limits<T>::digits10 < 20 ? T { 1, -3 }
        :                                         T { 1, -5 };

    if (phi < small_phi_limit)
    {
      // PadeApproximant[EllipticF[phi, m2], {phi, 0, {4, 3}}]
      // FullSimplify[%]

      const T phi_sq { phi * phi };

      const T m2 { m * m };

      const T top { phi * (-60 + (-12 + 17 * m2) * phi_sq) };
      const T bot { -60 + 3 * (-4 + 9 * m2) * phi_sq };

      result = top / bot;
    }
    else
    {
      constexpr T my_pi_half { numbers::pi_v<T> / 2 };

      int k_pi = static_cast<int>(phi / numbers::pi_v<T>);

      T phi_scaled = phi - (k_pi * numbers::pi_v<T>);

      const bool b_medium_phi_scale_and_negate { phi_scaled > my_pi_half };

      if(b_medium_phi_scale_and_negate)
      {
        ++k_pi;

        phi_scaled = -(phi_scaled - numbers::pi_v<T>);
      }

      T Km { };

      const T m2 { m * m };

      const bool m2_is_one { m2 == one };

      detail::ellint_detail::elliptic_series::agm(phi_scaled, m2, result, Km, static_cast<T*>(nullptr), static_cast<T*>(nullptr));

      if(b_medium_phi_scale_and_negate)
      {
        result = -result;
      }

      if(!m2_is_one)
      {
        result += ((k_pi * Km) * 2);
      }
    }
  }

  return result;
}

template <typename T>
constexpr auto comp_ellint_1_impl(const T m) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
  constexpr T one  { 1 };

  T result { };

  const auto fpc_m   = fpclassify(m);

  if(fpc_m == FP_ZERO)
  {
    result = numbers::pi_v<T> / 2;
  }
  else if((fabs(m) > one) || (fpc_m != FP_NORMAL))
  {
    result = std::numeric_limits<T>::quiet_NaN();
  }
  else if(signbit(m))
  {
    result = comp_ellint_1_impl(-m);
  }
  else
  {
    constexpr T zero { 0 };

    T Fpm { };

    detail::ellint_detail::elliptic_series::agm(zero, m * m, Fpm, result, static_cast<T*>(nullptr), static_cast<T*>(nullptr));
  }

  return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto ellint_1(const T k, const T phi) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::ellint_1_impl(static_cast<evaluation_type>(k), static_cast<evaluation_type>(phi)));
}

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto comp_ellint_1(const T k) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::comp_ellint_1_impl(static_cast<evaluation_type>(k)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ELLINT_1_HPP
