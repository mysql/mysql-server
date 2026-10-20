///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FREXP_2023_01_07_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FREXP_2023_01_07_HPP

#include <cmath>
#include <type_traits>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

namespace detail {

// LCOV_EXCL_START
template <class Real>
constexpr auto frexp_impl(Real arg, int* expptr) noexcept -> Real
{
   const bool negative_arg { (arg < static_cast<Real>(0)) };

   Real f { negative_arg ? -arg : arg };

   int e2 { };

   constexpr Real two_pow_16_plus { static_cast<Real>(INT32_C(0x10000)) };

   while (f >= two_pow_16_plus)
   {
      f = f / two_pow_16_plus;
      e2 += 16;
   }

   constexpr Real two_pow_16_minus { static_cast<Real>(0.0000152587890625L) };

   while (f <= two_pow_16_minus)
   {
      f = f * two_pow_16_plus;
      e2 -= 16;
   }

   while(f >= static_cast<Real>(INT8_C(1)))
   {
      f = f / static_cast<Real>(INT8_C(2));
      ++e2;
   }

   while(f < static_cast<Real>(0.5L))
   {
      f = f * static_cast<Real>(INT8_C(2));
      --e2;
   }

   *expptr = e2;

   return ((!negative_arg) ? f : -f);
}
// LCOV_EXCL_STOP

} // namespace detail

template <typename Real>
constexpr auto frexp(Real arg, int* expptr) -> Real
{
   if (BOOST_MP_IS_CONST_EVALUATED(arg))
   {
      // LCOV_EXCL_START
      if (arg == static_cast<Real>(0))
      {
         *expptr = 0;

         return arg;
      }
      else
      {
         return detail::frexp_impl(arg, expptr);
      }
      // LCOV_EXCL_STOP
   }
   else
   {
      // Default to the regular frexp function.
      using std::frexp;

      return frexp(arg, expptr);
   }
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FREXP_2023_01_07_HPP
