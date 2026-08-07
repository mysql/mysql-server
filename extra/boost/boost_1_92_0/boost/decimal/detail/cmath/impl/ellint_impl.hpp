// Copyright 2024 Matt Borland
// Copyright 2002 - 2011, 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_ELLINT_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_ELLINT_IMPL_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <cstddef>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace ellint_detail {

namespace elliptic_series {

template <typename T>
constexpr auto agm(T  phi,
                   T  m2,
                   T& Fpm,
                   T& Km,
                   T* pEm,
                   T* pEpm) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, void)
{
  // See Chapter/Section 19.8(i) Elliptic Integrals: Quadratic Transformations:
  // Arithmetic Geometric Mean (AGM), pp. 492-493 of:
  // F.W.J. Olver et al., NIST Handbook of Mathematical Functions,
  // Cambridge University Press. The same can also be found
  // online at https://dlmf.nist.gov/19.8

  // In particular, use the AGM algorithm implemented in e_float:
  // C.M. Kormanyos, "Algorithm 910: A Portable C++ Multiple-Precision
  // System for Special-Function Calculations", ACM TOMS (37) 4, February 2011.

  // See also the AGM algorithm as described in "Computation of Special Functions",
  // Zhang & Jin, 18.3.2, pages 663-665. The implementation is based on the
  // sample code therein. However, the Mathematica argument convention with
  // (k^2 --> m) is used, as described in Stephen Wolfram's Mathematica Book,
  // 4th Ed., Ch. 3.2.11, Page 773.

  // Make use of the following properties:
  // F(m | phi + pi*j) = F(m) + 2j * K(m)
  // E(m | phi + pi*j) = E(m) + 2j * E(m)

  // as well as (reflection of phi):
  // F(m, -phi) = -F(m, phi)
  // E(m, -phi) = -E(m, phi)

  // The calculations which are needed for EllipticE(...) are only performed if
  // the results from these will actually be used, in other words only if non-zero
  // pointers pEm or pEpm have been supplied to this subroutine.

  // Note that there is special handling for the angular argument phi if this
  // argument is equal to pi/2.

  constexpr T my_pi_half { numbers::pi_v<T> / 2 };

  const bool phi_is_pi_half { phi == my_pi_half };

  constexpr T one  { 1 };

  const bool has_e { ((pEm  != nullptr) || (pEpm != nullptr)) };

  if(m2 == one)
  {
    Km = std::numeric_limits<T>::quiet_NaN();

    const T sp { sin(phi) };

    Fpm = phi_is_pi_half ? std::numeric_limits<T>::quiet_NaN() : ::boost::decimal::log((one + sp) / (one - sp)) / 2;

    if(has_e)
    {
      if(pEm != nullptr)
      {
        *pEm = one;
      }

      if(pEpm != nullptr)
      {
        *pEpm = phi_is_pi_half ? one : sp;
      }
    }
  }
  else
  {
    constexpr T zero { 0 };
    constexpr T half { 5 , -1 };

    T a0    { one };
    T b0    { sqrt(one - m2) };
    T phi_n { phi };

    std::uint32_t p2 { UINT32_C(1) };

    T an { };

    T cn_2ncn_inner_prod      = (has_e ? m2 / 2 : zero);
    T sin_phi_n_cn_inner_prod = zero;

    const T break_check { b0 * T { 1, -1 - (std::numeric_limits<T>::digits / 2) } };

    for(int n = 0; n < std::numeric_limits<std::uint32_t>::digits; ++n)
    {
      an = (a0 + b0) / 2;

      if(!phi_is_pi_half)
      {
        const T atan_arg { (b0 * tan(phi_n)) / a0 };

        phi_n += atan(atan_arg);
      }

      const T cn_term = (a0 - b0) / 2;

      if(has_e)
      {
        cn_2ncn_inner_prod += ((cn_term * cn_term) * p2);

        if(pEpm != nullptr)
        {
          const T spn_term = ((!phi_is_pi_half) ? sin(phi_n) : zero);

          sin_phi_n_cn_inner_prod += (cn_term * spn_term);
        }
      }

      p2 = p2 << 1U;

      if(fabs(cn_term) < break_check)
      {
        break;
      }

      b0 = sqrt(a0 * b0);
      a0 = an;

      if(!phi_is_pi_half)
      {
        phi_n += numbers::pi_v<T> * static_cast<int>((phi_n / numbers::pi_v<T>) + half);
      }
    }

    Fpm = phi_n / an;

    if(!phi_is_pi_half) { Fpm /= p2; }

    Km = my_pi_half / an;

    if(has_e)
    {
      const T one_minus_cn_2ncn_inner_prod_half = one - cn_2ncn_inner_prod;

      if(pEm != nullptr)
      {
        *pEm = Km * one_minus_cn_2ncn_inner_prod_half;
      }

      if(pEpm != nullptr)
      {
        *pEpm = (Fpm * one_minus_cn_2ncn_inner_prod_half) + sin_phi_n_cn_inner_prod;
      }
    }
  }
}

} // namespace elliptic_series

} //namespace ellint_detail


} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_ELLINT_IMPL_HPP
