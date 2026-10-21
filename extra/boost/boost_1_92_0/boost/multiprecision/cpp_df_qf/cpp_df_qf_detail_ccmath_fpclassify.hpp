///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FPCLASSIFY_2024_12_26_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FPCLASSIFY_2024_12_26_HPP

#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_fabs.hpp>
#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_isinf.hpp>
#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_isnan.hpp>
#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_limits.hpp>

#include <cmath>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

template <typename Real>
constexpr auto fpclassify(Real x) noexcept -> int
{
   if ((::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::isnan)(x))
   {
      return FP_NAN;
   }
   else if ((::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::isinf)(x))
   {
      return FP_INFINITE;
   }
   else
   {
      const Real fabs_x { ::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::fabs(x) };

      if (fabs_x == Real(0))
      {
         return FP_ZERO;
      }
      else if ((fabs_x > 0) && (fabs_x < (::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<Real>::min)()))
      {
         return FP_SUBNORMAL;
      }
      else
      {
         return FP_NORMAL;
      }
   }
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FPCLASSIFY_2024_12_26_HPP
