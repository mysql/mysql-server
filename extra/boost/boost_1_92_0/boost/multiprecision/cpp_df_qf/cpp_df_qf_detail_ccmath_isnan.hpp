///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_ISNAN_2023_01_07_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_ISNAN_2023_01_07_HPP

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

template <class FloatingPointType>
constexpr auto isnan(FloatingPointType x) noexcept -> bool
{
   return (x != x);
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_ISNAN_2023_01_07_HPP
