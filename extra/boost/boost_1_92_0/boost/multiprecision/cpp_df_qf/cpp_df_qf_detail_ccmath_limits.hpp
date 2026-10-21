///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LIMITS_2023_01_07_HPP
#define BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LIMITS_2023_01_07_HPP

#include <cfloat>
#include <cmath>
#include <limits>
#include <type_traits>

namespace boost { namespace multiprecision { namespace backends { namespace cpp_df_qf_detail { namespace ccmath {

template <class FloatingPointType,
          class EnableType = void>
struct numeric_limits
{
   static constexpr bool is_specialized = false;
};

template <class FloatingPointType>
struct numeric_limits<FloatingPointType,
                      typename std::enable_if<(   std::is_same<FloatingPointType, float>::value
                                               || std::is_same<FloatingPointType, double>::value
                                               || std::is_same<FloatingPointType, long double>::value)>::type>
{
private:
   using float_type = FloatingPointType;

public:
   static constexpr bool                    is_specialized                = std::numeric_limits<float_type>::is_specialized;
   static constexpr bool                    is_signed                     = std::numeric_limits<float_type>::is_signed;
   static constexpr bool                    is_integer                    = std::numeric_limits<float_type>::is_integer;
   static constexpr bool                    is_exact                      = std::numeric_limits<float_type>::is_exact;
   static constexpr bool                    is_bounded                    = std::numeric_limits<float_type>::is_bounded;
   static constexpr bool                    is_modulo                     = std::numeric_limits<float_type>::is_modulo;
   static constexpr bool                    is_iec559                     = std::numeric_limits<float_type>::is_iec559;
   #ifdef _MSC_VER
   #  pragma warning(push)
   #  pragma warning(disable : 4996)
   #elif (defined(__clang__) && (__clang_major__ >= 17))
   #  pragma clang diagnostic push
   #  pragma clang diagnostic ignored "-Wdeprecated-declarations"
   #endif
   static constexpr std::float_denorm_style has_denorm                    = std::numeric_limits<float_type>::has_denorm;
   #ifdef _MSC_VER
   #pragma warning(pop)
   #elif (defined(__clang__) && (__clang_major__ >= 17))
   #  pragma clang diagnostic pop
   #endif
   static constexpr bool                    has_infinity                  = std::numeric_limits<float_type>::has_infinity;
   static constexpr bool                    has_quiet_NaN                 = std::numeric_limits<float_type>::has_quiet_NaN;
   static constexpr bool                    has_signaling_NaN             = std::numeric_limits<float_type>::has_signaling_NaN;
   static constexpr bool                    has_denorm_loss               = std::numeric_limits<float_type>::has_denorm_loss;
   static constexpr bool                    traps                         = std::numeric_limits<float_type>::traps;
   static constexpr bool                    tinyness_before               = std::numeric_limits<float_type>::tinyness_before;
   static constexpr std::float_round_style  round_style                   = std::numeric_limits<float_type>::round_style;

   static constexpr int radix                          = std::numeric_limits<float_type>::radix;
   static constexpr int digits                         = std::numeric_limits<float_type>::digits;
   static constexpr int digits10                       = std::numeric_limits<float_type>::digits10;
   static constexpr int max_digits10                   = std::numeric_limits<float_type>::max_digits10;

   static constexpr int max_exponent                   = std::numeric_limits<float_type>::max_exponent;
   static constexpr int min_exponent                   = std::numeric_limits<float_type>::min_exponent;
   static constexpr int max_exponent10                 = std::numeric_limits<float_type>::max_exponent10;
   static constexpr int min_exponent10                 = std::numeric_limits<float_type>::min_exponent10;

   // LCOV_EXCL_START
   static constexpr auto (min)         () noexcept -> float_type { return (std::numeric_limits<float_type>::min)         (); }
   static constexpr auto (max)         () noexcept -> float_type { return (std::numeric_limits<float_type>::max)         (); }
   static constexpr auto  lowest       () noexcept -> float_type { return  std::numeric_limits<float_type>::lowest       (); }
   static constexpr auto  epsilon      () noexcept -> float_type { return  std::numeric_limits<float_type>::epsilon      (); }
   static constexpr auto  round_error  () noexcept -> float_type { return  std::numeric_limits<float_type>::round_error  (); }
   static constexpr auto  denorm_min   () noexcept -> float_type { return  std::numeric_limits<float_type>::denorm_min   (); }
   static constexpr auto  infinity     () noexcept -> float_type { return  std::numeric_limits<float_type>::infinity     (); }
   static constexpr auto  quiet_NaN    () noexcept -> float_type { return  std::numeric_limits<float_type>::quiet_NaN    (); }
   static constexpr auto  signaling_NaN() noexcept -> float_type { return  std::numeric_limits<float_type>::signaling_NaN(); }
   // LCOV_EXCL_STOP
};

#if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
template <class FloatingPointType>
struct numeric_limits<FloatingPointType,
                      typename std::enable_if<std::is_same<FloatingPointType, ::boost::float128_type>::value>::type>
{
private:
   using self_type = ::boost::float128_type;

public:
   static constexpr bool                    is_specialized                = true;
   static constexpr bool                    is_signed                     = true;
   static constexpr bool                    is_integer                    = false;
   static constexpr bool                    is_exact                      = false;
   static constexpr bool                    is_bounded                    = true;
   static constexpr bool                    is_modulo                     = false;
   static constexpr bool                    is_iec559                     = true;
   #ifdef _MSC_VER
   #  pragma warning(push)
   #  pragma warning(disable : 4996)
   #elif (defined(__clang__) && (__clang_major__ >= 17))
   #  pragma clang diagnostic push
   #  pragma clang diagnostic ignored "-Wdeprecated-declarations"
   #endif
   static constexpr std::float_denorm_style has_denorm                    = std::denorm_present;
   #ifdef _MSC_VER
   #pragma warning(pop)
   #elif (defined(__clang__) && (__clang_major__ >= 17))
   #  pragma clang diagnostic pop
   #endif
   static constexpr bool                    has_infinity                  = true;
   static constexpr bool                    has_quiet_NaN                 = true;
   static constexpr bool                    has_signaling_NaN             = false;
   static constexpr bool                    has_denorm_loss               = true;
   static constexpr bool                    traps                         = false;
   static constexpr bool                    tinyness_before               = false;
   static constexpr std::float_round_style  round_style                   = std::round_to_nearest;

   static constexpr int radix                          = 2;
   static constexpr int digits                         = 113;
   static constexpr int digits10                       = 33;
   static constexpr int max_digits10                   = 36;

   static constexpr int max_exponent                   = 16384;
   static constexpr int min_exponent                   = -16381;
   static constexpr int max_exponent10                 = static_cast<int>(static_cast<long long>(static_cast<long long>(max_exponent) * 301LL) / 1000LL);
   static constexpr int min_exponent10                 = static_cast<int>(static_cast<long long>(static_cast<long long>(min_exponent) * 301LL) / 1000LL);

   static constexpr auto (min)() noexcept -> self_type
   {
     return   static_cast<self_type>(1)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN)
            * static_cast<self_type>(DBL_MIN) / 1073741824;
   }

   static constexpr auto (max)() noexcept -> self_type
   {
      // This has one bit set only.
      constexpr double dbl_mult = 8.9884656743115795386e+307;

      return   (static_cast<self_type>(1) - 9.62964972193617926527988971292463659e-35) // This now has all bits sets to 1
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult)
             *  static_cast<self_type>(dbl_mult) * 65536;
   }

   static constexpr auto lowest() noexcept -> self_type { return -(max)(); }

   static constexpr auto epsilon() -> self_type
   {
     // This double value has only one bit set and so is exact.
     return 1.92592994438723585305597794258492732e-34;
   }

   static constexpr auto round_error() noexcept -> self_type { return static_cast<self_type>(0.5F); }

   static constexpr auto denorm_min() noexcept -> self_type
   {
      return   static_cast<self_type>(1)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             * static_cast<self_type>(DBL_MIN)
             / 5.5751862996326557854e+42;
   }

   static constexpr auto infinity     () noexcept -> self_type { return HUGE_VAL; }
   static constexpr auto quiet_NaN    () noexcept -> self_type { return NAN; }
   static constexpr auto signaling_NaN() noexcept -> self_type { return 0; }
};
#endif

} } } } } // namespace boost::backends::cpp_df_qf_detail::ccmath

#endif // BOOST_MP_CPP_DF_QF_DETAIL_CCMATH_LIMITS_2023_01_07_HPP
