///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LDEXP_2023_01_07_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LDEXP_2023_01_07_HPP

#include <cmath>
#include <type_traits>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

namespace detail {

// LCOV_EXCL_START
template <class Real>
constexpr auto ldexp_impl(Real arg, int expval) noexcept -> Real
{
   constexpr Real two_pow_16_plus { static_cast<Real>(INT32_C(0x10000)) };

   while(expval > 16)
   {
      arg *= two_pow_16_plus;
      expval -= 16;
   }

   while(expval < -16)
   {
      arg /= two_pow_16_plus;
      expval += 16;
   }

   while(expval > 0)
   {
      arg *= 2;
      --expval;
   }

   while(expval < 0)
   {
      arg /= 2;
      ++expval;
   }

   return arg;
}
// LCOV_EXCL_STOP

} // Namespace detail

template <typename Real>
constexpr auto ldexp(Real arg, int expval) noexcept -> Real
{
   if (BOOST_MP_IS_CONST_EVALUATED(arg))
   {
      return detail::ldexp_impl<Real>(arg, expval); // LCOV_EXCL_LINE
   }
   else
   {
      using std::ldexp;

      return ldexp(arg, expval);
   }
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LDEXP_2023_01_07_HPP
