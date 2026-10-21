///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LOG_2024_12_30_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LOG_2024_12_30_HPP

#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_frexp.hpp>
#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_ccmath_limits.hpp>

#include <cmath>
#include <type_traits>

#if (defined(BOOST_GCC) && defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128))
//
// This is the only way we can avoid
// warning: non-standard suffix on floating constant [-Wpedantic]
// when building with -Wall -pedantic.  Neither __extension__
// nor #pragma diagnostic ignored work :(
//
#pragma GCC system_header
#endif

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

namespace detail {

// LCOV_EXCL_START
template <class Real>
constexpr auto exp_impl(Real x) noexcept -> Real
{
   constexpr int
     my_digits_10
     {
        ::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<Real>::digits10
     };

   constexpr int
     loop_count
     {
          my_digits_10 <  8 ?  7
        : my_digits_10 < 16 ? 15
        : my_digits_10 < 20 ? 19
        :                     33
     };

   // Scale the argument with a single factor of 2.
   // Then square the result upon return.
   x /= 2;

   // Perform a simple Taylor series of the exponent function.
   Real term { x };
   Real sum { Real { 1 } + term };

   for (int loop_index { INT8_C(2) }; loop_index < loop_count; ++loop_index)
   {
      term *= x;
      term /= static_cast<Real>(loop_index);
      sum += term;
   }

   // Scale the result.
   return sum * sum;
}

template<typename Real>
constexpr auto log_impl_pade(Real x) noexcept -> typename std::enable_if<(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<Real>::digits < 54), Real>::type
{
   // PadeApproximant[Log[x], {x, 1, {8, 8}}]
   // FullSimplify[%]

   const Real
      top
      {
         ((static_cast<Real>(-1.0L) + x) * (static_cast<Real>(1.0L) + x) * (static_cast<Real>(761.0L) + x * (static_cast<Real>(28544.0L) + x * (static_cast<Real>(209305.0L) + x * (static_cast<Real>(423680.0L) + x * (static_cast<Real>(209305.0L) + x * (static_cast<Real>(28544.0L) + static_cast<Real>(761.0L) * x)))))))
      };

   const Real
      bot
      {
         (static_cast<Real>(140.0L) * (static_cast<Real>(1.0L) + x * (static_cast<Real>(64.0L) + x * (static_cast<Real>(784.0L) + x * (static_cast<Real>(3136.0L) + x * (static_cast<Real>(4900.0L) + x * (static_cast<Real>(3136.0L) + x * (static_cast<Real>(784.0L) + x * (static_cast<Real>(64.0L) + x)))))))))
      };

   return top / bot;
}

template<typename Real>
constexpr auto log_impl_pade(Real x) noexcept -> typename std::enable_if<(!(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<Real>::digits < 54)), Real>::type
{
   // PadeApproximant[Log[x], {x, 1, {16, 16}}]
   // FullSimplify[%]

   const Real
      top
      {
         (static_cast<Real>(17.0L) * (static_cast<Real>(-1.0L) + x) * (static_cast<Real>(1.0L) + x) * (static_cast<Real>(143327.0L) + x * (static_cast<Real>(25160192.0L) + x * (static_cast<Real>(1069458527.0L) + x * (static_cast<Real>(17931092992.0L) + x * (static_cast<Real>(144291009727.0L) + x * (static_cast<Real>(613705186816.0L) + x * (static_cast<Real>(1446475477311.0L) + x * (static_cast<Real>(1923749922816.0L) + x * (static_cast<Real>(1446475477311.0L) + x * (static_cast<Real>(613705186816.0L) + x * (static_cast<Real>(144291009727.0L) + x * (static_cast<Real>(17931092992.0L) + x * (static_cast<Real>(1069458527.0L) + x * (static_cast<Real>(25160192.0L) + static_cast<Real>(143327.0L) * x)))))))))))))))
      };

   const Real
      bot
      {
         (static_cast<Real>(360360.0L) * (static_cast<Real>(1.0L) + x * (static_cast<Real>(256.0L) + x * (static_cast<Real>(14400.0L) + x * (static_cast<Real>(313600.0L) + x * (static_cast<Real>(3312400.0L) + x * (static_cast<Real>(19079424.0L) + x * (static_cast<Real>(64128064.0L) + x * (static_cast<Real>(130873600.0L) + x * (static_cast<Real>(165636900.0L) + x * (static_cast<Real>(130873600.0L) + x * (static_cast<Real>(64128064.0L) + x * (static_cast<Real>(19079424.0L) + x * (static_cast<Real>(3312400.0L) + x * (static_cast<Real>(313600.0L) + x * (static_cast<Real>(14400.0L) + x * (static_cast<Real>(256.0L) + x)))))))))))))))))
      };

   return top / bot;
}

// N[Log[2], 101]
// 0.69314718055994530941723212145817656807550013436025525412068000949339362196969471560586332699641868754

template <typename FloatingPointType> constexpr auto constant_ln_two() noexcept -> typename ::std::enable_if<(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  24), FloatingPointType>::type { return static_cast<FloatingPointType>(0.69314718055994530941723212145817656807550013436025525412068L); }
template <typename FloatingPointType> constexpr auto constant_ln_two() noexcept -> typename ::std::enable_if<(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  53), FloatingPointType>::type { return static_cast<FloatingPointType>(0.69314718055994530941723212145817656807550013436025525412068L); }
template <typename FloatingPointType> constexpr auto constant_ln_two() noexcept -> typename ::std::enable_if<(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  64), FloatingPointType>::type { return static_cast<FloatingPointType>(0.69314718055994530941723212145817656807550013436025525412068L); }
#if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
template <typename FloatingPointType> constexpr auto constant_ln_two() noexcept -> typename ::std::enable_if<(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113), FloatingPointType>::type { return static_cast<FloatingPointType>(0.69314718055994530941723212145817656807550013436025525412068Q); }
#else
template <typename FloatingPointType> constexpr auto constant_ln_two() noexcept -> typename ::std::enable_if<(::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113), FloatingPointType>::type { return static_cast<FloatingPointType>(0.69314718055994530941723212145817656807550013436025525412068L); }
#endif

template<typename Real>
constexpr auto log_impl(Real x) noexcept -> Real
{
   int n2 { };

   // Scale the argument down.

   Real x2 { ::boost::multiprecision::backends::cpp_df_qf_detail::ccmath::detail::frexp_impl(x, &n2) };

   if (x2 > static_cast<Real>(0.875L))
   {
     x2 /= 2;

     ++n2;
   }

   // Estimate the logarithm of the argument to roughly half
   // the precision of Real.
   const Real s { log_impl_pade(x2) };

   // Compute the exponent function to the full precision of Real.
   const Real E { exp_impl(s) };

   // Perform one single step of Newton-Raphson iteration
   // and scale the result back up.

   return (s + ((x2 - E) / E)) + Real { static_cast<Real>(n2) * constant_ln_two<Real>() };
}
// LCOV_EXCL_STOP

} // namespace detail

template <typename Real>
constexpr auto log(Real x) -> Real
{
   if (BOOST_MP_IS_CONST_EVALUATED(x))
   {
      return detail::log_impl<Real>(x); // LCOV_EXCL_LINE
   }
   else
   {
      using std::log;

      return log(x);
   }
}

} } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LOG_2024_12_30_HPP
