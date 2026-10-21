///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_ISINF_2023_01_07_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_ISINF_2023_01_07_HPP

#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_limits.hpp>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

template <typename FloatingPointType>
constexpr auto isinf(FloatingPointType x) noexcept -> bool
{
   return (   ( x == cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::infinity())
           || (-x == cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::infinity()));
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_ISINF_2023_01_07_HPP
