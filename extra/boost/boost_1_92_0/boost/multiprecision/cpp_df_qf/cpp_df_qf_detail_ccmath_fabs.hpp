///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FABS_2023_01_07_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FABS_2023_01_07_HPP

#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_isnan.hpp>
#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_limits.hpp>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

template <class Real>
constexpr auto fabs(Real x) noexcept -> Real
{
   return   (cpp_df_qf_detail::ccmath::isnan(x)) ? cpp_df_qf_detail::ccmath::numeric_limits<Real>::quiet_NaN()
          : (x == static_cast<Real>(-0))         ? static_cast<Real>(0)
          : (x >= 0)                             ? x
          : -x;
};

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FABS_2023_01_07_HPP
