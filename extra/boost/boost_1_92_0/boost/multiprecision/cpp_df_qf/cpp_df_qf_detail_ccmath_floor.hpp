///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FLOOR_2024_12_30_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FLOOR_2024_12_30_HPP

#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_limits.hpp>

#include <cmath>
#include <type_traits>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

namespace detail {

// LCOV_EXCL_START
template <typename Real>
constexpr auto floor_pos_impl(Real arg) noexcept -> Real
{
   constexpr auto
      max_comp_val
      {
         Real(1) / ::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<Real>::epsilon()
      };

   if (arg >= max_comp_val)
   {
      return arg;
   }

   Real result { 1 };

   if(result <= arg)
   {
      while(result < arg)
      {
         result *= 2;
      }

      while(result > arg)
      {
         --result;
      }

      return result;
   }
   else
   {
      return Real(0);
   }
}

template <typename Real>
constexpr auto floor_neg_impl(Real arg) noexcept -> Real
{
   Real result { -1 };

   if(result > arg)
   {
      while(result > arg)
      {
         result *= 2;
      }

      while(result < arg)
      {
         ++result;
      }

      if(result != arg)
      {
         --result;
      }
   }

   return result;
}

template <typename Real>
constexpr auto floor_impl(Real arg) noexcept -> Real
{
   if(arg > 0)
   {
      return floor_pos_impl(arg);
   }
   else
   {
      return floor_neg_impl(arg);
   }
}
// LCOV_EXCL_STOP

} // namespace detail

template <typename Real>
constexpr auto floor(Real x) -> Real
{
   if (BOOST_MP_IS_CONST_EVALUATED(x))
   {
      return detail::floor_impl<Real>(x); // LCOV_EXCL_LINE
   }
   else
   {
      using std::floor;

      return floor(x);
   }
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_FLOOR_2024_12_30_HPP
