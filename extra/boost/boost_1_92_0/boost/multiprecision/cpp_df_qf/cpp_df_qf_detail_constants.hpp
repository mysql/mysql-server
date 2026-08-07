///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CONSTANTS_2025_06_23_HPP
   #define BOOST_MP_CPP_DF_QF_DETAIL_CONSTANTS_2025_06_23_HPP

   #include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail.hpp>

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

   namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail {

   // N[Pi, 101]
   // 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170680

   // 3.14159250259,                            1.50995788317e-07
   // 3.141592653589793116,                     1.2246467991473529607e-16
   // 3.14159265358979323851281,                -5.01655761266833202345176e-20
   // 3.14159265358979323846264338327950279748, 8.67181013012378102479704402604335225411e-35

   template <typename FloatingPointType> constexpr auto constant_df_pi() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  24)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(3.14159250259L),                            static_cast<FloatingPointType>(1.50995788317e-07L) }; }
   template <typename FloatingPointType> constexpr auto constant_df_pi() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  53)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(3.141592653589793116L),                     static_cast<FloatingPointType>(1.2246467991473529607e-16L) }; }
   template <typename FloatingPointType> constexpr auto constant_df_pi() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  64)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(3.14159265358979323851281L),                static_cast<FloatingPointType>(-5.01655761266833202345176e-20L) }; }
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   template <typename FloatingPointType> constexpr auto constant_df_pi() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(3.14159265358979323846264338327950279748Q), static_cast<FloatingPointType>(8.67181013012378102479704402604335225411e-35Q) }; }
   #else
   template <typename FloatingPointType> constexpr auto constant_df_pi() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(3.14159265358979323846264338327950279748L), static_cast<FloatingPointType>(8.67181013012378102479704402604335225411e-35L) }; }
   #endif

   // N[Log[2], 101]
   // 0.69314718055994530941723212145817656807550013436025525412068000949339362196969471560586332699641868754

   // 0.69314712286,                             5.76999887869e-08
   // 0.6931471805599451752,                     1.3421277060097865271e-16
   // 0.69314718055994530942869,                 -1.14583527267987328094768e-20
   // 0.693147180559945309417232121458176575084, -7.00813947454958516341266200877162272784e-36

   template <typename FloatingPointType> constexpr auto constant_df_ln_two() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  24)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(0.69314712286L),                             static_cast<FloatingPointType>(5.76999887869e-08L) }; }
   template <typename FloatingPointType> constexpr auto constant_df_ln_two() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  53)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(0.6931471805599451752L),                     static_cast<FloatingPointType>(1.3421277060097865271e-16L) }; }
   template <typename FloatingPointType> constexpr auto constant_df_ln_two() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  64)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(0.69314718055994530942869L),                 static_cast<FloatingPointType>(-1.14583527267987328094768e-20L) }; }
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   template <typename FloatingPointType> constexpr auto constant_df_ln_two() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(0.693147180559945309417232121458176575084Q), static_cast<FloatingPointType>(-7.00813947454958516341266200877162272784e-36Q) }; }
   #else
   template <typename FloatingPointType> constexpr auto constant_df_ln_two() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(0.693147180559945309417232121458176575084L), static_cast<FloatingPointType>(-7.00813947454958516341266200877162272784e-36L) }; }
   #endif

   // N[Exp[1], 101]
   // 2.7182818284590452353602874713526624977572470936999595749669676277240766303535475945713821785251664274

   // 2.71828174591,                            8.25483965627e-08
   // 2.7182818284590450908,                    1.4456468917292501578e-16
   // 2.71828182845904523521133,                1.4895979785582304563159e-19
   // 2.71828182845904523536028747135266231436, 1.83398825226506410712297736767396397644e-34

   template <typename FloatingPointType> constexpr auto constant_df_exp1() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  24)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(2.71828174591L),                            static_cast<FloatingPointType>(8.25483965627e-08L) }; }
   template <typename FloatingPointType> constexpr auto constant_df_exp1() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  53)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(2.7182818284590450908L),                    static_cast<FloatingPointType>(1.4456468917292501578e-16L) }; }
   template <typename FloatingPointType> constexpr auto constant_df_exp1() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits ==  64)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(2.71828182845904523521133L),                static_cast<FloatingPointType>(1.4895979785582304563159e-19L) }; }
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   template <typename FloatingPointType> constexpr auto constant_df_exp1() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(2.71828182845904523536028747135266231436Q), static_cast<FloatingPointType>(1.83398825226506410712297736767396397644e-34Q) }; }
   #else
   template <typename FloatingPointType> constexpr auto constant_df_exp1() noexcept -> typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits == 113)), cpp_double_fp_backend<FloatingPointType>>::type { return { static_cast<FloatingPointType>(2.71828182845904523536028747135266231436L), static_cast<FloatingPointType>(1.83398825226506410712297736767396397644e-34L) }; }
   #endif

   } } } } // namespace boost::multiprecision::backends::cpp_df_qf_detail

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CONSTANTS_2025_06_23_HPP
