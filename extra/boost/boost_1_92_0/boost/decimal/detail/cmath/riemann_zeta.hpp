// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_RIEMANN_ZETA_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_RIEMANN_ZETA_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/riemann_zeta_impl.hpp>
#include <boost/decimal/detail/cmath/log10.hpp>
#include <boost/decimal/detail/cmath/pow.hpp>
#include <boost/decimal/detail/cmath/sin.hpp>
#include <boost/decimal/detail/cmath/tgamma.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto riemann_zeta_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc = fpclassify(x);

    constexpr T one  { 1, 0 };

    const bool is_neg { signbit(x) };

    T result { };

    if (fpc == FP_ZERO)
    {
        // The value of riemann_zeta(0) is 1/2.

        result = T { 5U, -1, true };
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc != FP_NORMAL)
    {
        result = ((fpc == FP_INFINITE) ? (is_neg ? -std::numeric_limits<T>::infinity() : one) : x);
    }
    #endif
    else
    {
        if (is_neg)
        {
            // Handle Riemann-zeta reflection.

            const T dmx         { one - x };
            const T two_pi_term { pow(numbers::pi_v<T> * 2, x) / numbers::pi_v<T> };
            const T chi         { (two_pi_term * sin((numbers::pi_v<T> * x) / 2)) * tgamma(dmx) };

            result = chi * riemann_zeta(dmx);
        }
        else
        {
            constexpr int asymp_cutoff
            {
                  std::numeric_limits<T>::digits10 < 10 ? T {  2, 1 } //  20
                : std::numeric_limits<T>::digits10 < 20 ? T {  5, 1 } //  50
                :                                         T { 15, 1 } // 150
            };

            if(x > asymp_cutoff)
            {
                // For large argument the power series is irrelevant.
                // In such cases, simply return 1.

                result = one;
            }
            else if((x > T { 9, -1 }) && (x < T { 11, -1 }))
            {
                // Arguments near +1 receive special treatment.

                if((x > one) || (x < one))
                {
                    // Use a Taylor series (or Pade approximation) near the discontinuity at x=1.

                    result = detail::riemann_zeta_series_or_pade_expansion(x);
                }
                else
                {
                    // The argument is exaclty one. The result is complex-infinity.
                    #ifndef BOOST_DECIMAL_FAST_MATH
                    result = std::numeric_limits<T>::quiet_NaN();
                    #else
                    result = T{0};
                    #endif
                }
            }
            else
            {
                // Test the conditions for the expansion of the product of primes. Set up a
                // test for the product of primes. The expansion in the product of primes can
                // be used if the number of prime-power terms remains reasonably small.
                // This test for being reasonably "small" is checked in relation
                // to the precision of the type and the size of primes available in the table.

                using prime_table_type = detail::prime_table_t<T>;

                constexpr std::size_t n_primes = std::tuple_size<prime_table_type>::value;

                const T lg10_max_prime { ::boost::decimal::log10(detail::prime_table<T>::primes[n_primes - 1U]) };

                if((x * lg10_max_prime) > std::numeric_limits<T>::digits10)
                {
                    // Perform the product of primes.
                    result = one;

                    for(std::size_t p = static_cast<std::size_t>(UINT8_C(0)); p < n_primes; ++p)
                    {
                      const T prime_p_pow_s = pow(detail::prime_table<T>::primes[p], x);

                      const T pps_term { prime_p_pow_s / (prime_p_pow_s - one) };

                      if((pps_term - one) < std::numeric_limits<T>::epsilon())
                      {
                        break;
                      }

                      result *= pps_term;
                    }
                }
                else
                {
                    // Use the accelerated alternating converging series for Zeta as shown in:
                    // http://numbers.computation.free.fr/Constants/Miscellaneous/zetaevaluations.html
                    // taken from P. Borwein, "An Efficient Algorithm for the Riemann Zeta Function",
                    // January 1995.

                    // Compute the coefficients dk in a loop and calculate the zeta function sum
                    // within the same loop on the fly.

                    // Set up the factorials and powers for the calculation of the coefficients dk.
                    // Note that j = n at this stage in the calculation. Also note that the value of
                    // dn is equal to the value of d0 at the end of the loop.

                    // Use nd = (digits * 1.45) + {|imag(s)| * 1.1}
                    // Here we have, however, only real valued arguments so this
                    // is streamlined a tad. Also instead of 1.45, we simply use 1.5.

                    constexpr int nd { static_cast<int>(std::numeric_limits<T>::digits10 * 1.5F) };

                    bool neg_term { (nd % 2) == 0 };

                    T n_plus_j_minus_one_fact = riemann_zeta_factorial<T>(2 * nd - 1);
                    T four_pow_j              = pow(T { 4 }, nd);
                    T n_minus_j_fact          = one;
                    T two_j_fact              = n_plus_j_minus_one_fact * (2 * nd);

                    T dn = (n_plus_j_minus_one_fact * four_pow_j) / (n_minus_j_fact * two_j_fact);

                    T jps = pow(T { nd }, x);

                    result = ((!neg_term) ? dn : -dn) / jps;

                    for(auto j = nd - 1; j >= 0; --j)
                    {
                      const bool j_is_zero { j == 0 };

                      const int two_jp1_two_j { ((2 * j) + 1) * (2 * ((!j_is_zero) ? j : 1)) };

                      n_plus_j_minus_one_fact /= (nd + j);
                      four_pow_j              /= 4;
                      n_minus_j_fact          *= (nd - j);
                      two_j_fact              /= two_jp1_two_j;

                      dn += ((n_plus_j_minus_one_fact * four_pow_j) / (n_minus_j_fact * two_j_fact));

                      if(!j_is_zero)
                      {
                        // Increment the zeta function sum.
                        jps = pow(T { j }, x);

                        neg_term = (!neg_term);

                        result += ((!neg_term) ? dn : -dn) / jps;
                      }
                    }

                    const T two_pow_one_minus_s { pow(T { 2 }, one - x) };

                    result /= (dn * (one - two_pow_one_minus_s));
                }
            }
        }
    }

    return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto riemann_zeta(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::riemann_zeta_impl(static_cast<evaluation_type>(x)));
}

BOOST_DECIMAL_EXPORT template <typename T, typename IntegralType>
constexpr auto riemann_zeta(const IntegralType n) noexcept
    BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, T, detail::is_integral_v, IntegralType, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    // TODO(ckormanyos) Consider making an integral-argument specialization.
    // Some exact values are known. Some simplifications for small-n are possible.

    return static_cast<T>(detail::riemann_zeta_impl(static_cast<evaluation_type>(n)));
}

} //namespace decimal
} //namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_RIEMANN_ZETA_HPP
