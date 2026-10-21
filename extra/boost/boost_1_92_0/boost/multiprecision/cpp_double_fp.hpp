///////////////////////////////////////////////////////////////////////////////
//  Copyright 2021 - 2025 Fahad Syed.
//  Copyright 2021 - 2025 Christopher Kormanyos.
//  Copyright 2021 - 2025 Janek Kozicki.
//  Copyright 2025 Matt Borland.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MP_CPP_DOUBLE_FP_2021_06_05_HPP
#define BOOST_MP_CPP_DOUBLE_FP_2021_06_05_HPP

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail.hpp>
#include <boost/multiprecision/detail/hash.hpp>
#include <boost/multiprecision/traits/max_digits10.hpp>
#include <boost/multiprecision/traits/std_integer_traits.hpp>

#ifdef BOOST_MP_MATH_AVAILABLE
//
// Headers required for Boost.Math integration:
//
#include <boost/math/policies/policy.hpp>
//
// Some includes we need from Boost.Math, since we rely on that library to provide these functions:
//
#include <boost/math/special_functions/acosh.hpp>
#include <boost/math/special_functions/asinh.hpp>
#include <boost/math/special_functions/atanh.hpp>
#include <boost/math/special_functions/cbrt.hpp>
#include <boost/math/special_functions/expm1.hpp>
#include <boost/math/special_functions/gamma.hpp>
#endif

#include <limits>
#include <string>
#include <type_traits>

#if (defined(BOOST_CLANG) && defined(BOOST_CLANG_VERSION) && (BOOST_CLANG_VERSION <= 90000))
#define BOOST_MP_DF_QF_NUM_LIMITS_CLASS_TYPE struct
#else
#define BOOST_MP_DF_QF_NUM_LIMITS_CLASS_TYPE class
#endif

namespace boost { namespace multiprecision { namespace backends {

template <typename FloatingPointType>
class cpp_double_fp_backend;

template <typename FloatingPointType>
constexpr auto eval_add(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;
template <typename FloatingPointType>
constexpr auto eval_add(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void;
template <typename FloatingPointType>
constexpr auto eval_subtract(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;
template <typename FloatingPointType>
constexpr auto eval_subtract(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void;
template <typename FloatingPointType>
constexpr auto eval_multiply(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;
template <typename FloatingPointType>
constexpr auto eval_multiply(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void;
template <typename FloatingPointType>
constexpr auto eval_divide(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;
template <typename FloatingPointType>
constexpr auto eval_divide(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void;
template <typename FloatingPointType>
constexpr auto eval_eq(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> bool;
template <typename FloatingPointType>
constexpr auto eval_lt(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> bool;
template <typename FloatingPointType>
constexpr auto eval_gt(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> bool;
template <typename FloatingPointType>
constexpr auto eval_is_zero(const cpp_double_fp_backend<FloatingPointType>& x) -> bool;
template <typename FloatingPointType>
constexpr auto eval_signbit(const cpp_double_fp_backend<FloatingPointType>& x) -> int;

template <typename FloatingPointType>
constexpr auto eval_fabs(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a) -> void;
template <typename FloatingPointType>
constexpr auto eval_frexp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, int* v) -> void;
template <typename FloatingPointType>
constexpr auto eval_ldexp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, int v) -> void;
template <typename FloatingPointType>
constexpr auto eval_floor(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;
template <typename FloatingPointType>
constexpr auto eval_ceil(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;
template <typename FloatingPointType>
constexpr auto eval_fpclassify(const cpp_double_fp_backend<FloatingPointType>& o) -> int;

template <typename FloatingPointType>
constexpr auto eval_sqrt(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& o) -> void;

template <typename FloatingPointType>
constexpr auto eval_pow(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x, const cpp_double_fp_backend<FloatingPointType>& a) -> void;

template <typename FloatingPointType,
          typename IntegralType>
constexpr auto eval_pow(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x, IntegralType n) -> typename ::std::enable_if<boost::multiprecision::detail::is_integral<IntegralType>::value, void>::type;

template <typename FloatingPointType,
          typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && ((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) < 16))>::type const* = nullptr>
constexpr auto eval_exp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;

template <typename FloatingPointType,
          typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) >= 16) && ((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) <= 36)))>::type const* = nullptr>
constexpr auto eval_exp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;

template <typename FloatingPointType,
          typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && ((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) > 36))>::type const* = nullptr>
constexpr auto eval_exp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;

template <typename FloatingPointType>
constexpr auto eval_log(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void;

template <typename FloatingPointType>
constexpr auto eval_convert_to(signed long long* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> void;

template <typename FloatingPointType>
constexpr auto eval_convert_to(unsigned long long* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> void;

#ifdef BOOST_HAS_INT128
template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::int128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type;

template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::int128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<!(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type;

template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::uint128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type;

template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::uint128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<!(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type;
#endif

template <typename FloatingPointType,
          typename OtherFloatingPointType>
constexpr auto eval_convert_to(OtherFloatingPointType* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename ::std::enable_if<cpp_df_qf_detail::is_floating_point<OtherFloatingPointType>::value>::type;

template <typename FloatingPointType>
constexpr auto hash_value(const cpp_double_fp_backend<FloatingPointType>& a) -> ::std::size_t;

template <typename FloatingPointType>
constexpr auto fabs(const cpp_double_fp_backend<FloatingPointType>& a) -> cpp_double_fp_backend<FloatingPointType>;

} } } // namespace boost::multiprecision::backends

namespace std {

// Foward declarations of various specializations of std::numeric_limits

template <typename FloatingPointType,
          const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
BOOST_MP_DF_QF_NUM_LIMITS_CLASS_TYPE numeric_limits<boost::multiprecision::number<boost::multiprecision::backends::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >;

} // namespace std

namespace boost { namespace multiprecision {

template <typename FloatingPointType>
struct number_category<backends::cpp_double_fp_backend<FloatingPointType>> : public std::integral_constant<int, number_kind_floating_point> { };

namespace backends {

// A cpp_double_fp_backend is represented by an unevaluated sum of two
// floating-point units, a0 and a1, which satisfy |a1| <= (1 / 2) * ulp(a0).
// The type of the floating-point constituents should adhere to IEEE754.
// Although the constituent parts (a0 and a1) satisfy |a1| <= (1 / 2) * ulp(a0),
// the composite type does not adhere to these strict error bounds. Its error
// bounds are larger.

// This class has been tested with floats having single-precision (4 byte),
// double-precision (8 byte) and quad precision (16 byte, such as GCC's __float128).

template <typename FloatingPointType>
class cpp_double_fp_backend
{
 public:
   using float_type = FloatingPointType;

   static_assert
   (
         cpp_df_qf_detail::is_floating_point<float_type>::value
      && bool
         {
               ((cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits ==  24) && cpp_df_qf_detail::ccmath::numeric_limits<float_type>::is_specialized && cpp_df_qf_detail::ccmath::numeric_limits<float_type>::is_iec559)
            || ((cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits ==  53) && cpp_df_qf_detail::ccmath::numeric_limits<float_type>::is_specialized && cpp_df_qf_detail::ccmath::numeric_limits<float_type>::is_iec559)
            || ((cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits ==  64) && cpp_df_qf_detail::ccmath::numeric_limits<float_type>::is_specialized && cpp_df_qf_detail::ccmath::numeric_limits<float_type>::is_iec559)
            ||  (cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits == 113)
         }, "Error: float_type does not fulfill the backend requirements of cpp_double_fp_backend"
   );

   using rep_type   = cpp_df_qf_detail::pair<float_type, float_type>;
   using arithmetic = cpp_df_qf_detail::exact_arithmetic<float_type>;

   using signed_types   = std::tuple<signed char, signed short, signed int, signed long, signed long long, std::intmax_t>;
   using unsigned_types = std::tuple<unsigned char, unsigned short, unsigned int, unsigned long, unsigned long long, std::uintmax_t>;
   using float_types    = std::tuple<float, double, long double>;
   using exponent_type  = int;

   static constexpr int my_digits         = static_cast<int>(cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits * static_cast<int>(INT8_C(2)));
   static constexpr int my_digits10       = static_cast<int>(boost::multiprecision::detail::calc_digits10    <static_cast<unsigned>(my_digits)>::value);
   static constexpr int my_max_digits10   = static_cast<int>(boost::multiprecision::detail::calc_max_digits10<static_cast<unsigned>(my_digits)>::value);
   static constexpr int my_max_exponent   = cpp_df_qf_detail::ccmath::numeric_limits<float_type>::max_exponent;
   static constexpr int my_max_exponent10 = cpp_df_qf_detail::ccmath::numeric_limits<float_type>::max_exponent10;

   static constexpr int my_min_exponent =
      static_cast<int>
      (
           cpp_df_qf_detail::ccmath::numeric_limits<float_type>::min_exponent
         + cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits
      );

   static constexpr int my_min_exponent10 =
      static_cast<int>
      (
         -static_cast<int>
          (
             boost::multiprecision::detail::calc_digits10<static_cast<unsigned>(-my_min_exponent)>::value
          )
      );

   // Default constructor.
   constexpr cpp_double_fp_backend() noexcept { }

   // Copy constructor.
   constexpr cpp_double_fp_backend(const cpp_double_fp_backend& other) : data(other.data) { }

   // Move constructor.
   constexpr cpp_double_fp_backend(cpp_double_fp_backend&& other) noexcept : data(static_cast<rep_type&&>(other.data)) { }

   // Constructors from other floating-point types.
   template <typename OtherFloatType,
             typename ::std::enable_if<(    cpp_df_qf_detail::is_floating_point<OtherFloatType>::value
                                        && (cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatType>::digits <= cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(const OtherFloatType& f)
      : data(f, static_cast<float_type>(0.0F)) { }

   template <typename OtherFloatType,
             typename ::std::enable_if<(    cpp_df_qf_detail::is_floating_point<OtherFloatType>::value
                                        && (cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatType>::digits > cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(const OtherFloatType& f)
      : data(static_cast<float_type>(f),
             static_cast<float_type>(f - static_cast<OtherFloatType>(static_cast<float_type>(f)))) { }

   // Construtor from another kind of cpp_double_fp_backend<> object.

   template <typename OtherFloatType,
             typename ::std::enable_if<(    cpp_df_qf_detail::is_floating_point<OtherFloatType>::value
                                        && (!std::is_same<FloatingPointType, OtherFloatType>::value))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(const cpp_double_fp_backend<OtherFloatType>& a)
      : cpp_double_fp_backend(cpp_double_fp_backend(a.my_first()) += a.my_second()) { }

   // Constructors from integers.
   template <typename SignedIntegralType,
             typename ::std::enable_if<(     boost::multiprecision::detail::is_integral<SignedIntegralType>::value
                                        && (!boost::multiprecision::detail::is_unsigned<SignedIntegralType>::value)
                                        && ((static_cast<int>(sizeof(SignedIntegralType) * 8) - 1) <= cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(const SignedIntegralType& n)
      : data(static_cast<float_type>(n), static_cast<float_type>(0.0F)) { }

   template <typename UnsignedIntegralType,
             typename ::std::enable_if<(    boost::multiprecision::detail::is_integral<UnsignedIntegralType>::value
                                        &&  boost::multiprecision::detail::is_unsigned<UnsignedIntegralType>::value
                                        && (static_cast<int>(sizeof(UnsignedIntegralType) * 8u) <= cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(const UnsignedIntegralType& u)
      : data(static_cast<float_type>(u), static_cast<float_type>(0.0F)) { }

   // Constructors from integers which hold more information than *this can contain.
   template <typename UnsignedIntegralType,
             typename ::std::enable_if<(    boost::multiprecision::detail::is_integral<UnsignedIntegralType>::value
                                        &&  boost::multiprecision::detail::is_unsigned<UnsignedIntegralType>::value
                                        && (static_cast<int>(sizeof(UnsignedIntegralType) * 8u) > cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(UnsignedIntegralType u)
      : data(static_cast<float_type>(u & cpp_df_qf_detail::float_mask<UnsignedIntegralType, float_type>()),
             static_cast<float_type>(0.0F))
   {
      using local_unsigned_integral_type = UnsignedIntegralType;

      if (u > cpp_df_qf_detail::float_mask<local_unsigned_integral_type, float_type>())
      {
         local_unsigned_integral_type
            local_flt_mask
            {
               cpp_df_qf_detail::float_mask<local_unsigned_integral_type, float_type>()
            };

         for (int     index_mask_lsb =  cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits;
                     (index_mask_lsb <  static_cast<int>(sizeof(local_unsigned_integral_type) * 8u))
                  && (local_flt_mask != local_unsigned_integral_type { UINT8_C(0) });
                      index_mask_lsb += cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits)
         {
            local_flt_mask <<= static_cast<unsigned>(cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits);

            add_unchecked_limb(static_cast<float_type>(u & local_flt_mask));
         }
      }
   }

   template <typename SignedIntegralType,
             typename ::std::enable_if<(     boost::multiprecision::detail::is_integral<SignedIntegralType>::value
                                        && (!boost::multiprecision::detail::is_unsigned<SignedIntegralType>::value)
                                        && ((static_cast<int>(sizeof(SignedIntegralType) * 8) - 1) > cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits))>::type const* = nullptr>
   constexpr cpp_double_fp_backend(SignedIntegralType n)
   {
      const bool is_neg { (n < SignedIntegralType { INT8_C(0) }) };

      using local_unsigned_integral_type = typename boost::multiprecision::detail::make_unsigned<SignedIntegralType>::type;

      const local_unsigned_integral_type
         u_val
         {
            (!is_neg)
               ? static_cast<local_unsigned_integral_type>(n)
               : static_cast<local_unsigned_integral_type>
                 (
                      static_cast<local_unsigned_integral_type>(~n)
                    + static_cast<local_unsigned_integral_type>(UINT8_C(1))
                 )
         };

      data = cpp_double_fp_backend(u_val).data;

      if(is_neg) { negate(); }
   }

   constexpr cpp_double_fp_backend(const float_type& a, const float_type& b) noexcept : data(a, b) { }

   constexpr cpp_double_fp_backend(const cpp_df_qf_detail::pair<float_type, float_type>& p) noexcept : data(p) { }

   cpp_double_fp_backend(const char* p_str)
   {
      *this = p_str;
   }

   // Assignment operator.
   constexpr auto operator=(const cpp_double_fp_backend& other) -> cpp_double_fp_backend&
   {
      if (this != &other)
      {
         data = other.data;
      }

      return *this;
   }

   // Move assignment operator.
   constexpr auto operator=(cpp_double_fp_backend&& other) noexcept -> cpp_double_fp_backend&
   {
      data = static_cast<rep_type&&>(other.data);

      return *this;
   }

   // Assignment operator from another kind of cpp_double_fp_backend<> object.
   template <typename OtherFloatType,
             typename ::std::enable_if<(   cpp_df_qf_detail::is_floating_point<OtherFloatType>::value
                                        && (!std::is_same<FloatingPointType, OtherFloatType>::value))>::type const* = nullptr>
   constexpr auto operator=(const cpp_double_fp_backend<OtherFloatType>& other) -> cpp_double_fp_backend&
   {
     return operator=(cpp_double_fp_backend(other));
   }

   template <typename OtherFloatType>
   constexpr auto operator=(const OtherFloatType f) -> typename ::std::enable_if<cpp_df_qf_detail::is_floating_point<OtherFloatType>::value, cpp_double_fp_backend&>::type
   {
     return operator=(cpp_double_fp_backend(f));
   }

   template <typename IntegralType>
   constexpr auto operator=(const IntegralType n) -> typename ::std::enable_if<boost::multiprecision::detail::is_integral<IntegralType>::value, cpp_double_fp_backend&>::type
   {
     return operator=(cpp_double_fp_backend(n));
   }

   auto operator=(const char* p_str) -> cpp_double_fp_backend&
   {
      rd_string(p_str);

      return *this;
   }

   constexpr auto hash() const -> ::std::size_t
   {
      #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
      using local_float_type = typename std::conditional<::std::is_same<float_type, ::boost::float128_type>::value,
                                                         long double,
                                                         float_type>::type;
      #else
      using local_float_type = float_type;
      #endif

      std::size_t result { UINT8_C(0) };

      int n_first { };
      int n_second { };

      boost::multiprecision::detail::hash_combine(result, static_cast<local_float_type>(cpp_df_qf_detail::ccmath::frexp(data.first, &n_first)));
      boost::multiprecision::detail::hash_combine(result, static_cast<local_float_type>(cpp_df_qf_detail::ccmath::frexp(data.second, &n_second)));
      boost::multiprecision::detail::hash_combine(result, n_first);
      boost::multiprecision::detail::hash_combine(result, n_second);

      return result;
   }

   // The public methods follow.

   constexpr auto isneg_unchecked() const noexcept -> bool { return (data.first < 0); }

   constexpr auto iszero_unchecked() const noexcept -> bool { return (data.first  == float_type { 0.0F }); }

   constexpr auto is_one() const noexcept -> bool 
   {
      return
         (
               (data.second == float_type { 0.0F })
            && (data.first == float_type { 1.0F })
         );
   }

   constexpr auto negate() -> void
   {
      const bool isinf_u { (cpp_df_qf_detail::ccmath::isinf)(data.first) };
      const bool isnan_u { (cpp_df_qf_detail::ccmath::isnan)(data.first) };

      if      (isnan_u) { }
      else if (isinf_u)
      {
         data.first = -data.first;
      }
      else
      {
         if (!iszero_unchecked())
         {
            data = arithmetic::normalize(-data.first, -data.second);
         }
      }
   }

   // Getters/Setters
   constexpr auto my_first () const noexcept -> const float_type& { return data.first; }
   constexpr auto my_second() const noexcept -> const float_type& { return data.second; }

   constexpr auto rep() noexcept -> rep_type& { return data; }

   constexpr auto rep() const noexcept -> const rep_type& { return data; }

   constexpr auto crep() const noexcept -> const rep_type& { return data; }

   // Unary add/sub/mul/div follow in the upcoming paragraphs.

   constexpr auto operator+=(const cpp_double_fp_backend& v) -> cpp_double_fp_backend&
   {
      const int fpc_u { eval_fpclassify(*this) };
      const int fpc_v { eval_fpclassify(v) };

      if ((fpc_u != FP_NORMAL) || (fpc_v != FP_NORMAL))
      {
         // Handle special cases like zero, inf and NaN.

         if (fpc_u == FP_NAN)
         {
            return *this;
         }

         const bool isinf_v { (fpc_v == FP_INFINITE) };

         if (fpc_u == FP_INFINITE)
         {
            if (isinf_v && (isneg_unchecked() != v.isneg_unchecked()))
            {
               *this = cpp_double_fp_backend::my_value_nan();
            }

            return *this;
         }

         const bool iszero_u { ((fpc_u == FP_ZERO) || (fpc_u == FP_SUBNORMAL)) };
         const bool isnan_v  { (fpc_v == FP_NAN) };

         if (iszero_u || (isnan_v || isinf_v))
         {
            if (iszero_u)
            {
               data.first  = float_type { 0.0F };
               data.second = float_type { 0.0F };
            }

            const bool iszero_v { ((fpc_v == FP_ZERO) || (fpc_v == FP_SUBNORMAL)) };

            return ((!iszero_v) ? operator=(v) : *this);
         }
      }

      add_unchecked(v);

      return *this;
   }

   constexpr auto operator-=(const cpp_double_fp_backend& v) -> cpp_double_fp_backend&
   {
      const int fpc_u { eval_fpclassify(*this) };
      const int fpc_v { eval_fpclassify(v) };

      if ((fpc_u != FP_NORMAL) || (fpc_v != FP_NORMAL))
      {
         // Handle special cases like zero, inf and NaN.

         if (fpc_u == FP_NAN)
         {
            return *this;
         }

         const bool isinf_v { (fpc_v == FP_INFINITE) };

         if (fpc_u == FP_INFINITE)
         {
            if (isinf_v && (isneg_unchecked() == v.isneg_unchecked()))
            {
               *this = cpp_double_fp_backend::my_value_nan();
            }

            return *this;
         }

         const bool iszero_u { ((fpc_u == FP_ZERO) || (fpc_u == FP_SUBNORMAL)) };
         const bool isnan_v  { (fpc_v == FP_NAN) };

         if (iszero_u || (isnan_v || isinf_v))
         {
            if (iszero_u)
            {
               data.first  = float_type { 0.0F };
               data.second = float_type { 0.0F };
            }

            const bool iszero_v { ((fpc_v == FP_ZERO) || (fpc_v == FP_SUBNORMAL)) };

            return ((!iszero_v) ? operator=(-v) : *this);
         }
      }

      if (this == &v)
      {
         data.first  = float_type { 0.0F };
         data.second = float_type { 0.0F };

         return *this;
      }

      const rep_type thi_tlo { arithmetic::two_diff(data.second, v.data.second) };

      data = arithmetic::two_diff(data.first, v.data.first);

      if (cpp_df_qf_detail::ccmath::isinf(data.first))
      {
         // Handle overflow.
         const bool b_neg { (data.first < float_type { 0.0F }) };

         *this = cpp_double_fp_backend::my_value_inf();

         if (b_neg)
         {
            negate();
         }

         return *this;
      }

      data = arithmetic::two_hilo_sum(data.first, data.second + thi_tlo.first);

      data = arithmetic::two_hilo_sum(data.first, thi_tlo.second + data.second);

      return *this;
   }

   constexpr auto operator*=(const cpp_double_fp_backend& v) -> cpp_double_fp_backend&
   {
      // Evaluate the sign of the result.

      const int fpc_u { eval_fpclassify(*this) };
      const int fpc_v { eval_fpclassify(v) };

      if ((fpc_u != FP_NORMAL) || (fpc_v != FP_NORMAL))
      {
         // Handle special cases like zero, inf and NaN.
         const bool isinf_u  { (fpc_u == FP_INFINITE) };
         const bool isinf_v  { (fpc_v == FP_INFINITE) };
         const bool iszero_u { (fpc_u == FP_ZERO) };
         const bool iszero_v { (fpc_v == FP_ZERO) };

         if (((fpc_u == FP_NAN) || (fpc_v == FP_NAN)) || (isinf_u && iszero_v) || (isinf_v && iszero_u))
         {
            return operator=( cpp_double_fp_backend::my_value_nan());
         }

         if (isinf_u || isinf_v)
         {
            const bool b_neg { (isneg_unchecked() != v.isneg_unchecked()) };

            *this = cpp_double_fp_backend::my_value_inf();

            if (b_neg)
            {
               negate();
            }

            return *this;
         }

         if (iszero_u || iszero_v)
         {
            return operator=(cpp_double_fp_backend(0));
         }
      }

      mul_unchecked(v);

      return *this;
   }

   constexpr auto operator/=(const cpp_double_fp_backend& v) -> cpp_double_fp_backend&
   {
      const int fpc_u { eval_fpclassify(*this) };
      const int fpc_v { eval_fpclassify(v) };

      if ((fpc_u != FP_NORMAL) || (fpc_v != FP_NORMAL))
      {
         // Handle special cases like zero, inf and NaN.
         const bool isnan_u { (fpc_u == FP_NAN) };
         const bool isnan_v { (fpc_v == FP_NAN) };

         if (isnan_u || isnan_v)
         {
            return operator=(cpp_double_fp_backend::my_value_nan());
         }

         const bool iszero_u { (fpc_u == FP_ZERO) };
         const bool iszero_v { (fpc_v == FP_ZERO) };

         if (iszero_u)
         {
            if (iszero_v)
            {
               return operator=(cpp_double_fp_backend::my_value_nan());
            }
            else
            {
               return operator=(cpp_double_fp_backend(0));
            }
         }

         // Handle more special cases like zero, inf and NaN.
         if (iszero_v)
         {
            const bool b_neg = isneg_unchecked();

            *this = cpp_double_fp_backend::my_value_inf();

            if (b_neg)
            {
               negate();
            }

            return *this;
         }

         const bool isinf_v { (fpc_v == FP_INFINITE) };
         const bool isinf_u { (fpc_u == FP_INFINITE) };

         if (isinf_u)
         {
            if (isinf_v)
            {
               return operator=(cpp_double_fp_backend::my_value_nan());
            }
            else
            {
               const bool b_neg { isneg_unchecked() };

               return operator=((!b_neg) ? cpp_double_fp_backend::my_value_inf() : -cpp_double_fp_backend::my_value_inf());
            }
         }

         if (isinf_v)
         {
            return operator=(cpp_double_fp_backend(0));
         }
      }

      if (this == &v)
      {
         data.first  = float_type { 1.0F };
         data.second = float_type { 0.0F };

         return *this;
      }

      // The division algorithm has been taken from Victor Shoup,
      // package WinNTL-5_3_2. It might originally be related to the
      // K. Briggs work. The algorithm has been significantly simplified
      // while still attempting to retain proper rounding corrections.
      // Checks for overflow and underflow have been added.

      const float_type C { data.first / v.data.first };

      float_type c { cpp_df_qf_detail::split_maker<float_type>::value * C };

      float_type hc { };

      if (cpp_df_qf_detail::ccmath::isinf(c))
      {
         // Handle overflow by scaling down (and then back up) with the split.

         hc =
            cpp_df_qf_detail::ccmath::ldexp
            (
               C - float_type { C - cpp_df_qf_detail::ccmath::ldexp(C, -cpp_df_qf_detail::split_maker<float_type>::n_shl) },
               cpp_df_qf_detail::split_maker<float_type>::n_shl
            );
      }
      else
      {
         hc = c - float_type { c - C };
      }

      float_type u { cpp_df_qf_detail::split_maker<float_type>::value * v.data.first };

      const float_type hv =
      (
         cpp_df_qf_detail::ccmath::isinf(u)
            ? cpp_df_qf_detail::ccmath::ldexp
              (
                 // Handle overflow by scaling down (and then back up) with the split.
                 v.data.first - float_type { v.data.first - cpp_df_qf_detail::ccmath::ldexp(v.data.first, -cpp_df_qf_detail::split_maker<float_type>::n_shl) },
                 cpp_df_qf_detail::split_maker<float_type>::n_shl
              )
            : u - float_type { u - v.data.first }
      );

      const float_type U { C * v.data.first };

      u = cpp_df_qf_detail::ccmath::unsafe::fma(hc, hv, -U);

      {
         const float_type tv { v.data.first - hv };

         u = cpp_df_qf_detail::ccmath::unsafe::fma(hc, tv, u);

         const float_type tc { C - hc };

         u = cpp_df_qf_detail::ccmath::unsafe::fma(tc, hv, u);
         u = cpp_df_qf_detail::ccmath::unsafe::fma(tc, tv, u);
      }

      c = float_type { (data.first - U) - u } + data.second;

      c = (c - float_type { C * v.data.second }) / v.data.first;

      // Perform even more simplifications compared to Victor Shoup.
      data.first  = C + c;
      data.second = float_type { C - data.first } + c;

      return *this;
   }

   // Unary minus operator.
   constexpr auto operator-() const -> cpp_double_fp_backend
   {
      cpp_double_fp_backend v { *this };

      v.negate();

      return v;
   }

   // Public helper functions.
   static constexpr auto pown(cpp_double_fp_backend& result, const cpp_double_fp_backend& x, int p) -> void
   {
      using local_float_type = cpp_double_fp_backend;

      if (p == 2)
      {
         result = x; result.mul_unchecked(x);
      }
      else if (p == 3)
      {
         result = x; result.mul_unchecked(x); result.mul_unchecked(x);
      }
      else if (p == 4)
      {
         local_float_type x2 { x };
         x2.mul_unchecked(x);

         result = x2;
         result.mul_unchecked(x2);
      }
      else if (p == 1)
      {
         result = x;
      }
      else if (p < 0)
      {
         // The case p == 0 is checked in a higher calling layer.

         pown(result, local_float_type(1U) / x, -p);
      }
      else
      {
         result = local_float_type(1U);

         local_float_type y(x);

         auto p_local = static_cast<std::uint32_t>(p);

         for (;;)
         {
            if (static_cast<std::uint_fast8_t>(p_local & static_cast<std::uint32_t>(UINT8_C(1))) != static_cast<std::uint_fast8_t>(UINT8_C(0)))
            {
               result.mul_unchecked(y);
            }

            p_local >>= 1U;

            if (p_local == static_cast<std::uint32_t>(UINT8_C(0)))
            {
               break;
            }
            else
            {
               y.mul_unchecked(y);
            }
         }
      }
   }

   constexpr auto swap(cpp_double_fp_backend& other) -> void
   {
      if (this != &other)
      {
         const rep_type tmp { data };

         data = other.data;

         other.data = tmp;
      }
   }

   constexpr auto swap(cpp_double_fp_backend&& other) noexcept -> void
   {
      const rep_type tmp { static_cast<typename cpp_double_fp_backend::rep_type&&>(data) };

      data = other.data;

      other.data = tmp;
   }

   constexpr auto compare(const cpp_double_fp_backend& other) const noexcept -> int
   {
      // Return 1 for *this > other, -1 for *this < other, 0 for *this = other.

      if ((cpp_df_qf_detail::ccmath::isnan)(data.first))
      {
        return -1;
      }
      else
      {
        return (my_first() > other.my_first()) ?  1 : (my_first()  < other.my_first())
                                               ? -1 : (my_second() > other.my_second())
                                               ?  1 : (my_second() < other.my_second())
                                               ? -1 : 0;
      }
   }

   // TBD: Exactly what compilers/language-standards are needed to make this constexpr?
   // TBD: It odes not really become constexpr until we stop using an intermediate
   // cpp_bin_float anyway. But I will leave this comment for future library evolution.

   auto str(std::streamsize number_of_digits, const std::ios::fmtflags format_flags) const -> std::string
   {
      // Use cpp_bin_float when writing to string. This is similar
      // to the use of cpp_bin_float when reading from string.

      cpp_bin_float_read_write_backend_type f_bin { data.first };

      eval_add(f_bin, cpp_bin_float_read_write_backend_type(data.second));

      return f_bin.str(number_of_digits, format_flags);
   }

   constexpr auto order02() const -> int
   {
      // TBD: Is there another option to get the base-2 log
      // that's more unequivocally closer to constexpr?

      // TBD: Either way, integrate this (or something like it)
      // into any potential implementation of eval_ilogb().

      int e2 { };

      cpp_double_fp_backend dummy { };

      eval_frexp(dummy, *this, &e2);

      return e2;
   }

   static constexpr auto my_value_max() noexcept -> cpp_double_fp_backend
   {
      // Use the non-normalized sum of two maximum values, where the lower
      // value is "shifted" right in the sense of floating-point ldexp.

      return
         cpp_double_fp_backend
         (
            arithmetic::two_hilo_sum
            (
               float_type
               (
                    (cpp_df_qf_detail::ccmath::numeric_limits<float_type>::max)()
                  * (
                         static_cast<float_type>(1.0F)
                       - static_cast<float_type>(1.5F) * cpp_df_qf_detail::ccmath::sqrt(cpp_df_qf_detail::ccmath::numeric_limits<float_type>::epsilon())
                    )
               ),
               float_type
               (
                  cpp_df_qf_detail::ccmath::ldexp
                  (
                     (cpp_df_qf_detail::ccmath::numeric_limits<float_type>::max)(),
                     -cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits
                  )
               )
            )
         );
   }

   static constexpr auto my_value_min() noexcept -> cpp_double_fp_backend
   {
      // Use the non-normalized minimum value, where the lower value
      // is "shifted" left in the sense of floating-point ldexp.

      return
         cpp_double_fp_backend
         (
            float_type
            (
               cpp_df_qf_detail::ccmath::ldexp
               (
                  (cpp_df_qf_detail::ccmath::numeric_limits<float_type>::min)(),
                   cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits
               )
            )
         );
   }

   static constexpr auto my_value_eps() noexcept -> cpp_double_fp_backend
   {
      return
         cpp_double_fp_backend
         (
            float_type(cpp_df_qf_detail::ccmath::ldexp(float_type { 1 }, int { 3 - my_digits }))
         );
   }

   static constexpr auto my_value_nan() noexcept -> cpp_double_fp_backend
   {
      return cpp_double_fp_backend(static_cast<float_type>(NAN), static_cast<float_type>(0.0F));
   }

   static constexpr auto my_value_inf() noexcept -> cpp_double_fp_backend
   {
      return cpp_double_fp_backend(static_cast<float_type>(HUGE_VAL), static_cast<float_type>(0.0F)); // conversion from double infinity OK
   }

   static constexpr auto my_value_logmax() -> cpp_double_fp_backend
   {
      return
         cpp_double_fp_backend
         (
            cpp_df_qf_detail::ccmath::log
            (
               float_type
               (
                    (cpp_df_qf_detail::ccmath::numeric_limits<float_type>::max)()
                  * (
                         static_cast<float_type>(1.0F)
                       - static_cast<float_type>(1.5F) * cpp_df_qf_detail::ccmath::sqrt(cpp_df_qf_detail::ccmath::numeric_limits<float_type>::epsilon())
                    )
               )
            )
         );
   }

   static constexpr auto my_value_logmin() -> cpp_double_fp_backend
   {
      return
         cpp_double_fp_backend
         (
            cpp_df_qf_detail::ccmath::log
            (
               float_type
               (
                  cpp_df_qf_detail::ccmath::ldexp
                  (
                     (cpp_df_qf_detail::ccmath::numeric_limits<float_type>::min)(),
                      cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits
                  )
               )
            )
         );
   }

   constexpr auto add_unchecked_limb(const float_type v_first) -> void
   {
      const float_type thi { data.second };

      data = arithmetic::two_sum(data.first, v_first);

      data = arithmetic::two_hilo_sum(data.first, data.second + thi);

      data = arithmetic::two_hilo_sum(data.first, data.second);
   }

private:
   rep_type data;

   using cpp_bin_float_read_write_backend_type = boost::multiprecision::backends::cpp_bin_float<static_cast<unsigned>(my_digits), digit_base_2, void, int, cpp_df_qf_detail::ccmath::numeric_limits<float_type>::min_exponent, cpp_df_qf_detail::ccmath::numeric_limits<float_type>::max_exponent>;

   constexpr auto rd_string(const char* p_str) -> bool;

   constexpr auto add_unchecked(const cpp_double_fp_backend& v) -> void
   {
      const rep_type thi_tlo { arithmetic::two_sum(data.second, v.data.second) };

      data = arithmetic::two_sum(data.first, v.data.first);

      if (cpp_df_qf_detail::ccmath::isinf(data.first))
      {
         // Handle overflow.
         const bool b_neg { (data.first < float_type { 0.0F }) };

         *this = cpp_double_fp_backend::my_value_inf();

         if (b_neg)
         {
            negate();
         }

         return;
      }

      data = arithmetic::two_hilo_sum(data.first, data.second + thi_tlo.first);

      data = arithmetic::two_hilo_sum(data.first, thi_tlo.second + data.second);
   }

   constexpr auto mul_unchecked(const cpp_double_fp_backend& v) -> void
   {
      // The multiplication algorithm has been taken from Victor Shoup,
      // package WinNTL-5_3_2. It might originally be related to the
      // K. Briggs work. The algorithm has been significantly simplified
      // while still attempting to retain proper rounding corrections.
      // Checks for overflow and underflow have been added.

      float_type C { cpp_df_qf_detail::split_maker<float_type>::value * data.first };

      float_type hu { };

      if (cpp_df_qf_detail::ccmath::isinf(C))
      {
         // Handle overflow by scaling down (and then back up) with the split.

         C = data.first - cpp_df_qf_detail::ccmath::ldexp(data.first, -cpp_df_qf_detail::split_maker<float_type>::n_shl);

         hu = cpp_df_qf_detail::ccmath::ldexp(data.first - C, cpp_df_qf_detail::split_maker<float_type>::n_shl);
      }
      else
      {
         hu = C - float_type { C - data.first };
      }

      C = data.first * v.data.first;

      if (cpp_df_qf_detail::ccmath::isinf(C))
      {
         // Handle overflow.
         const bool b_neg { (isneg_unchecked() != v.isneg_unchecked()) };

         *this = cpp_double_fp_backend::my_value_inf();

         if (b_neg)
         {
            negate();
         }

         return;
      }

      float_type c { cpp_df_qf_detail::split_maker<float_type>::value * v.data.first };

      float_type hv { };

      if (cpp_df_qf_detail::ccmath::isinf(c))
      {
         // Handle overflow by scaling down (and then back up) with the split.

         c = v.data.first - cpp_df_qf_detail::ccmath::ldexp(v.data.first, -cpp_df_qf_detail::split_maker<float_type>::n_shl);

         hv = cpp_df_qf_detail::ccmath::ldexp(v.data.first - c, cpp_df_qf_detail::split_maker<float_type>::n_shl);
      }
      else
      {
         hv = c - float_type { c - v.data.first };
      }

      {
         const float_type tv { v.data.first - hv };

         const float_type
            t1
            {
               cpp_df_qf_detail::ccmath::unsafe::fma
               (
                  hu,
                  tv,
                  cpp_df_qf_detail::ccmath::unsafe::fma(hu, hv, -C)
               )
            };

         const float_type tu { data.first - hu };

         c =    cpp_df_qf_detail::ccmath::unsafe::fma(tu, tv, cpp_df_qf_detail::ccmath::unsafe::fma(tu, hv, t1))
             + (data.first * v.data.second)
             + (data.second * v.data.first);
      }

      // Perform even more simplifications compared to Victor Shoup.
      data.first  = C + c;
      data.second = float_type { C - data.first } + c;
   }

   template <typename OtherFloatingPointType,
             typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<OtherFloatingPointType>::value && ((cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatingPointType>::digits10 * 2) < 16))>::type const*>
   friend constexpr auto eval_exp(cpp_double_fp_backend<OtherFloatingPointType>& result, const cpp_double_fp_backend<OtherFloatingPointType>& x) -> void;

   template <typename OtherFloatingPointType,
             typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<OtherFloatingPointType>::value && (((cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatingPointType>::digits10 * 2) >= 16) && ((cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatingPointType>::digits10 * 2) <= 36)))>::type const*>
   friend constexpr auto eval_exp(cpp_double_fp_backend<OtherFloatingPointType>& result, const cpp_double_fp_backend<OtherFloatingPointType>& x) -> void;

   template <typename OtherFloatingPointType,
             typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<OtherFloatingPointType>::value && ((cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatingPointType>::digits10 * 2) > 36))>::type const*>
   friend constexpr auto eval_exp(cpp_double_fp_backend<OtherFloatingPointType>& result, const cpp_double_fp_backend<OtherFloatingPointType>& x) -> void;
};

template <typename FloatingPointType>
constexpr auto cpp_double_fp_backend<FloatingPointType>::rd_string(const char* p_str) -> bool
{
   // Use an intermediate cpp_bin_float backend type for reading string input.

   cpp_bin_float_read_write_backend_type f_bin { };

   f_bin = p_str;

   const int fpc { eval_fpclassify(f_bin) };

   const bool is_definitely_nan { (fpc == FP_NAN) };

   using local_double_fp_type = cpp_double_fp_backend<FloatingPointType>;

   if (is_definitely_nan)
   {
      static_cast<void>(operator=(local_double_fp_type::my_value_nan()));

      return true;
   }

   const bool b_neg { (eval_signbit(f_bin) == 1) };

   if (b_neg) { f_bin.negate(); }

   const int
      expval_from_f_bin
      {
         [&f_bin]()
         {
            int expval { };

            cpp_bin_float_read_write_backend_type dummy { };

            eval_frexp(dummy, f_bin, &expval);

            return expval;
         }()
      };

   const auto is_zero_or_subnormal =
   (
         (fpc == FP_ZERO)
      || (expval_from_f_bin < static_cast<typename cpp_bin_float_read_write_backend_type::exponent_type>(local_double_fp_type::my_min_exponent))
   );

   if (is_zero_or_subnormal)
   {
      data.first  = float_type { 0.0F };
      data.second = float_type { 0.0F };

      return true;
   }

   float_type flt_inf_check_first { };

   eval_convert_to(&flt_inf_check_first, f_bin);

   bool is_definitely_inf { ((fpc == FP_INFINITE) || cpp_df_qf_detail::ccmath::isinf(flt_inf_check_first)) };

   if ((!is_definitely_inf) && (flt_inf_check_first > my_value_max().my_first()))
   {
      cpp_bin_float_read_write_backend_type f_bin_inf_check(f_bin);

      eval_subtract(f_bin_inf_check, cpp_bin_float_read_write_backend_type(flt_inf_check_first));

      float_type flt_inf_check_second { };

      eval_convert_to(&flt_inf_check_second, f_bin_inf_check);

      is_definitely_inf = eval_gt(local_double_fp_type(flt_inf_check_first, flt_inf_check_second), my_value_max());
   };

   if (is_definitely_inf)
   {
      static_cast<void>(operator=(local_double_fp_type::my_value_inf()));

      if (b_neg)
      {
        negate();
      }

      return true;
   }

   // The input string is normal. We will now extract its value.

   data.first  = float_type { 0.0F };
   data.second = float_type { 0.0F };

   // Handle small input values. Scale (and re-scale them below) if possible.

   constexpr int pow2_scaling_for_small_input { cpp_df_qf_detail::ccmath::numeric_limits<float_type>::digits };

   const bool
      has_pow2_scaling_for_small_input
      {
         (expval_from_f_bin < static_cast<int>(local_double_fp_type::my_min_exponent + pow2_scaling_for_small_input))
      };

   if (has_pow2_scaling_for_small_input)
   {
      eval_ldexp(f_bin, f_bin, pow2_scaling_for_small_input);
   }

   using local_builtin_float_type = typename std::conditional<(sizeof(float_type) <= sizeof(double)), double, float_type>::type;

   constexpr unsigned
      digit_limit
      {
         static_cast<unsigned>
         (
            static_cast<int>
            (
                   (local_double_fp_type::my_digits / cpp_df_qf_detail::ccmath::numeric_limits<local_builtin_float_type>::digits)
               + (((local_double_fp_type::my_digits % cpp_df_qf_detail::ccmath::numeric_limits<local_builtin_float_type>::digits) != 0) ? 1 : 0)
            )
            * cpp_df_qf_detail::ccmath::numeric_limits<local_builtin_float_type>::digits
         )
      };

   for(auto i = static_cast<unsigned>(UINT8_C(0));
            i < digit_limit;
            i = static_cast<unsigned>(i + static_cast<unsigned>(cpp_df_qf_detail::ccmath::numeric_limits<local_builtin_float_type>::digits)))
   {
      local_builtin_float_type flt_part { };

      eval_convert_to(&flt_part, f_bin);

      eval_subtract(f_bin, cpp_bin_float_read_write_backend_type(flt_part));

      eval_add(*this, local_double_fp_type { flt_part });
   }

   if (has_pow2_scaling_for_small_input)
   {
      eval_ldexp(*this, *this, -pow2_scaling_for_small_input);
   }

   if (b_neg) { negate(); }

   return true;
}

} } } // namespace boost::multiprecision::backends

#include <boost/multiprecision/cpp_df_qf/cpp_df_qf_detail_constants.hpp>

namespace boost { namespace multiprecision { namespace backends {

template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_digits;
template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_digits10;
template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_max_digits10;
template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_max_exponent;
template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_min_exponent;
template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_max_exponent10;
template <typename FloatingPointType>
constexpr int cpp_double_fp_backend<FloatingPointType>::my_min_exponent10;

template <typename FloatingPointType>
constexpr cpp_double_fp_backend<FloatingPointType> operator+(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) { return cpp_double_fp_backend<FloatingPointType>(a) += b; }
template <typename FloatingPointType>
constexpr cpp_double_fp_backend<FloatingPointType> operator-(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) { return cpp_double_fp_backend<FloatingPointType>(a) -= b; }
template <typename FloatingPointType>
constexpr cpp_double_fp_backend<FloatingPointType> operator*(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) { return cpp_double_fp_backend<FloatingPointType>(a) *= b; }
template <typename FloatingPointType>
constexpr cpp_double_fp_backend<FloatingPointType> operator/(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) { return cpp_double_fp_backend<FloatingPointType>(a) /= b; }

template <typename FloatingPointType>
constexpr auto eval_add(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void { result += x; }
template <typename FloatingPointType>
constexpr auto eval_add(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void { result = cpp_double_fp_backend<FloatingPointType>(a) += b; }
template <typename FloatingPointType>
constexpr auto eval_subtract(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void { result -= x; }
template <typename FloatingPointType>
constexpr auto eval_subtract(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void { result = cpp_double_fp_backend<FloatingPointType>(a) -= b; }
template <typename FloatingPointType>
constexpr auto eval_multiply(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void { result *= x; }
template <typename FloatingPointType>
constexpr auto eval_multiply(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void { result = cpp_double_fp_backend<FloatingPointType>(a) *= b; }
template <typename FloatingPointType>
constexpr auto eval_divide(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void { result /= x; }
template <typename FloatingPointType>
constexpr auto eval_divide(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> void { result = cpp_double_fp_backend<FloatingPointType>(a) /= b; }
template <typename FloatingPointType>
constexpr auto eval_eq(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> bool { return (a.compare(b) == 0); }
template <typename FloatingPointType>
constexpr auto eval_lt(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> bool { return (a.compare(b) == -1); }
template <typename FloatingPointType>
constexpr auto eval_gt(const cpp_double_fp_backend<FloatingPointType>& a, const cpp_double_fp_backend<FloatingPointType>& b) -> bool { return (a.compare(b) == 1); }
template <typename FloatingPointType>

constexpr auto eval_is_zero(const cpp_double_fp_backend<FloatingPointType>& x) -> bool
{
   return x.iszero_unchecked();
}

template <typename FloatingPointType>
constexpr auto eval_signbit(const cpp_double_fp_backend<FloatingPointType>& x) -> int
{
   return (x.isneg_unchecked() ? 1 : 0);
}

template <typename FloatingPointType>
constexpr auto eval_fabs(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a) -> void
{
   result = a;

   if (a.isneg_unchecked())
   {
      result.negate();
   }
}

template <typename FloatingPointType>
constexpr auto eval_frexp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, int* v) -> void
{
   using local_backend_type = cpp_double_fp_backend<FloatingPointType>;
   using local_float_type = typename local_backend_type::float_type;

   int expptr { };

   const local_float_type fhi { cpp_df_qf_detail::ccmath::frexp(a.rep().first, &expptr) };
   const local_float_type flo { cpp_df_qf_detail::ccmath::ldexp(a.rep().second, -expptr) };

   if (v != nullptr)
   {
      *v = expptr;
   }

   result.rep() = local_backend_type::arithmetic::normalize(fhi, flo);
}

template <typename FloatingPointType>
constexpr auto eval_ldexp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& a, int v) -> void
{
   using local_backend_type = cpp_double_fp_backend<FloatingPointType>;
   using local_float_type = typename local_backend_type::float_type;

   const local_float_type fhi { cpp_df_qf_detail::ccmath::ldexp(a.crep().first,  v) };
   const local_float_type flo { cpp_df_qf_detail::ccmath::ldexp(a.crep().second, v) };

   result.rep() = local_backend_type::arithmetic::normalize(fhi, flo);
}

template <typename FloatingPointType>
constexpr auto eval_floor(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void
{
   using local_backend_type = cpp_double_fp_backend<FloatingPointType>;
   using local_float_type = typename local_backend_type::float_type;

   const local_float_type fhi { cpp_df_qf_detail::ccmath::floor(x.my_first()) };

   if (fhi != x.my_first())
   {
      result.rep().first  = fhi;
      result.rep().second = local_float_type { 0 };
   }
   else
   {
      const local_float_type flo = { cpp_df_qf_detail::ccmath::floor(x.my_second()) };

      result.rep() = local_backend_type::arithmetic::normalize(fhi, flo);
   }
}

template <typename FloatingPointType>
constexpr auto eval_ceil(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void
{
   // Compute -floor(-x);
   eval_floor(result, -x);

   result.negate();
}

template <typename FloatingPointType>
constexpr auto eval_fpclassify(const cpp_double_fp_backend<FloatingPointType>& o) -> int
{
   if      (cpp_df_qf_detail::ccmath::isnan(o.crep().first)) { return FP_NAN; }
   else if (cpp_df_qf_detail::ccmath::isinf(o.crep().first)) { return FP_INFINITE; }
   else if (eval_is_zero(o)) { return FP_ZERO; }
   else
   {
      using local_backend_type = cpp_double_fp_backend<FloatingPointType>;
      using local_float_type = typename local_backend_type::float_type;

      const local_float_type fabs_x { cpp_df_qf_detail::ccmath::fabs(o.crep().first) };

      if ((fabs_x > 0) && (fabs_x < (cpp_df_qf_detail::ccmath::numeric_limits<local_float_type>::min)()))
      {
         return FP_SUBNORMAL;
      }
      else
      {
         return FP_NORMAL;
      }
   }
}

template <typename FloatingPointType>
constexpr auto eval_sqrt(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& o) -> void
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;
   using local_float_type = typename double_float_type::float_type;

   const int fpc { eval_fpclassify(o) };

   const bool isneg_o { o.isneg_unchecked() };

   if ((fpc != FP_NORMAL) || isneg_o)
   {
      if (fpc == FP_ZERO)
      {
         result = double_float_type(0);

         return;
      }
      else if ((fpc == FP_NAN) || isneg_o)
      {
         result = double_float_type::my_value_nan();

         return;
      }
      else if (fpc == FP_INFINITE)
      {
         result = double_float_type::my_value_inf();

         return;
      }
   }

   // TBD: Do we need any overflow/underflow guards when multiplying
   // by the split or when multiplying (hx * tx) and/or (hx * hx)?

   const local_float_type c { cpp_df_qf_detail::ccmath::sqrt(o.crep().first) };

   local_float_type p { cpp_df_qf_detail::split_maker<local_float_type>::value * c };

   const local_float_type hx { local_float_type { c - p } + p };
   const local_float_type tx { c  - hx };

   local_float_type q  = hx * tx;

   q = q + q;

   p = hx * hx;

   const local_float_type u { p + q };

   const local_float_type uu { cpp_df_qf_detail::ccmath::unsafe::fma(tx, tx, local_float_type { p - u } + q) };

   const local_float_type
      cc
      {
        local_float_type { local_float_type { o.crep().first - u } - uu + o.crep().second } / local_float_type { c + c }
      };

   result.rep().first  = c + cc;
   result.rep().second = local_float_type { c - result.my_first() } + cc;
}

template <typename FloatingPointType>
constexpr auto eval_pow(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x, const cpp_double_fp_backend<FloatingPointType>& a) -> void
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;

   constexpr double_float_type zero { 0 };

   result = zero;

   signed long long val_nll { };

   eval_convert_to(&val_nll, a);

   const int na { static_cast<int>(val_nll) };

   const int fpc_a { eval_fpclassify(a) };

   if((fpc_a == FP_NORMAL) && (a.compare(double_float_type(na)) == 0))
   {
      eval_pow(result, x, na);
   }
   else
   {
      constexpr double_float_type one { 1 };

      const int fpc_x { eval_fpclassify(x) };

      if (fpc_a == FP_ZERO)
      {
         // pow(base, +/-0) returns 1 for any base, even when base is NaN.

         result = one;
      }
      else if (fpc_x == FP_ZERO)
      {
         if ((fpc_a == FP_NORMAL) || (fpc_a == FP_INFINITE))
         {
            // pow(+/-0, exp), where exp is negative and finite, returns +infinity.
            // pow(+/-0, exp), where exp is positive non-integer, returns +0.

            // pow(+/-0, -infinity) returns +infinity.
            // pow(+/-0, +infinity) returns +0.

            result = (eval_signbit(a) ? double_float_type::my_value_inf() : zero);
         }
         else if (fpc_a == FP_NAN)
         {
            result = double_float_type::my_value_nan();
         }
      }
      else if (fpc_x == FP_INFINITE)
      {
         if ((fpc_a == FP_NORMAL) || (fpc_a == FP_INFINITE))
         {
            // pow(+infinity, exp) returns +0 for any negative exp.
            // pow(-infinity, exp) returns +infinity for any positive exp.

            result = (eval_signbit(a) ? zero : double_float_type::my_value_inf());
         }
         else if (fpc_a == FP_NAN)
         {
            result = double_float_type::my_value_nan();
         }
      }
      else if (fpc_x != FP_NORMAL)
      {
         result = x;
      }
      else
      {
         if (fpc_a == FP_INFINITE)
         {
            constexpr double_float_type one_minus { -1 };

            if (x.compare(one_minus) == 0)
            {
               result = one;
            }
            else
            {
               double_float_type xabs { };

               eval_fabs(xabs, x);

               const int compare_one_result { xabs.compare(one) };

               result =
                  (
                       (compare_one_result < 0) ? (eval_signbit(a) ? double_float_type::my_value_inf() : zero)
                     : (compare_one_result > 0) ? (eval_signbit(a) ? zero : double_float_type::my_value_inf())
                     : one
                  );
            }
         }
         else if (fpc_a == FP_NAN)
         {
            result = (x.compare(one) == 0) ? one : double_float_type::my_value_nan();
         }
         else
         {
            double_float_type log_x { };

            eval_log(log_x, x);

            double_float_type a_log_x { };

            eval_multiply(a_log_x, a, log_x);

            eval_exp(result, a_log_x);
         }
      }
   }
}

template <typename FloatingPointType,
          typename IntegralType>
constexpr auto eval_pow(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x, IntegralType p) -> typename ::std::enable_if<::boost::multiprecision::detail::is_integral<IntegralType>::value, void>::type
{
   const int fpc { eval_fpclassify(x) };

   using double_float_type = cpp_double_fp_backend<FloatingPointType>;

   using local_integral_type = IntegralType;

   const bool p_is_odd { (static_cast<local_integral_type>(p & 1) != static_cast<local_integral_type>(0)) };

   if (p == static_cast<local_integral_type>(0))
   {
      // pow(base, +/-0) returns 1 for any base, even when base is NaN.

      result = double_float_type { unsigned { UINT8_C(1) } };
   }
   else if (fpc != FP_NORMAL)
   {
      if (fpc == FP_ZERO)
      {
         // pow(  +0, exp), where exp is a negative odd  integer, returns +infinity.
         // pow(  -0, exp), where exp is a negative odd  integer, returns +infinity.
         // pow(+/-0, exp), where exp is a negative even integer, returns +infinity.

         // pow(  +0, exp), where exp is a positive odd  integer, returns +0.
         // pow(  -0, exp), where exp is a positive odd  integer, returns -0.
         // pow(+/-0, exp), where exp is a positive even integer, returns +0.

         result = ((p < static_cast<local_integral_type>(0)) ? double_float_type::my_value_inf() : double_float_type { unsigned { UINT8_C(0) } });
      }
      else if (fpc == FP_INFINITE)
      {
         if (eval_signbit(x))
         {
            if (p < static_cast<local_integral_type>(0))
            {
               // pow(-infinity, exp) returns -0 if exp is a negative odd integer.
               // pow(-infinity, exp) returns +0 if exp is a negative even integer.

               result = double_float_type { unsigned { UINT8_C(0) } };
            }
            else
            {
               // pow(-infinity, exp) returns -infinity if exp is a positive odd integer.
               // pow(-infinity, exp) returns +infinity if exp is a positive even integer.

               result = (p_is_odd ? -double_float_type::my_value_inf() : double_float_type::my_value_inf());
            }
         }
         else
         {
            // pow(+infinity, exp) returns +0 for any negative exp.
            // pow(+infinity, exp) returns +infinity for any positive exp.

            result = ((p < static_cast<local_integral_type>(0)) ? double_float_type { unsigned { UINT8_C(0) } } : double_float_type::my_value_inf());
         }
      }
      else
      {
         result = double_float_type::my_value_nan();
      }
   }
   else
   {
      double_float_type::pown(result, x, p);
   }
}

template <typename FloatingPointType,
          typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && ((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) < 16))>::type const*>
constexpr auto eval_exp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;

   constexpr double_float_type one { 1 };

   const int fpc { eval_fpclassify(x) };

   const bool b_neg { x.isneg_unchecked() };

   if (fpc == FP_ZERO)
   {
      result = one;
   }
   else if (fpc != FP_NORMAL)
   {
      if (fpc == FP_INFINITE)
      {
         result = (b_neg ? double_float_type { 0.0F } : double_float_type::my_value_inf());
      }
      else if (fpc == FP_NAN)
      {
         result = x;
      }
   }
   else
   {
      using local_float_type = typename double_float_type::float_type;

      // Get a local copy of the argument and force it to be positive.
      const double_float_type xx { (!b_neg) ? x : -x };

      // Check the range of the input.
      if (eval_lt(x, double_float_type::my_value_logmin()))
      {
         result = double_float_type(0U);
      }
      else if (eval_gt(xx, double_float_type::my_value_logmax()))
      {
         result = double_float_type::my_value_inf();
      }
      else if (xx.is_one())
      {
         if(!b_neg)
         {
            result = cpp_df_qf_detail::constant_df_exp1<local_float_type>();
         }
         else
         {
            eval_divide(result, one, cpp_df_qf_detail::constant_df_exp1<local_float_type>());
         }
      }
      else
      {
         // Use an argument reduction algorithm for exp() in classic MPFUN-style.

         double_float_type nf { };

         // Prepare the scaled variables.
         const bool b_scale { (xx.order02() > -1) };

         double_float_type r { };

         if (b_scale)
         {
            eval_floor(nf, xx / cpp_df_qf_detail::constant_df_ln_two<local_float_type>());

            eval_ldexp(r, xx - (nf * cpp_df_qf_detail::constant_df_ln_two<local_float_type>()), -2);
         }
         else
         {
            r = xx;
         }

         // PadeApproximant[Exp[x] - 1, {x, 0, {6, 6}}]
         // FullSimplify[%]
         //   (84 x (7920 + 240 x^2 + x^4))
         // / (665280 + x (-332640 + x (75600 + x (-10080 + x (840 + (-42 + x) x)))))

         constexpr double_float_type n84(84);
         constexpr double_float_type n240(240);
         constexpr double_float_type n7920(7920);

         constexpr double_float_type n665280(665280);
         constexpr double_float_type n332640(332640);
         constexpr double_float_type n75600(75600);
         constexpr double_float_type n10080(10080);
         constexpr double_float_type n840(840);
         constexpr double_float_type n42(42);

         const double_float_type r2 { r * r };

         // Use the small-argument Pade approximation having coefficients shown above.
         result = (n84 * r * (n7920 + (n240 + r2) * r2));
         const double_float_type bot = (n665280 + r * (-n332640 + r * (n75600 + r * (-n10080 + r * (n840 + (-n42 + r) * r)))));

         eval_divide(result, bot);
         result.add_unchecked_limb(local_float_type { 1.0F });

         // Rescale the result.
         if (b_scale)
         {
            result *= result;
            result *= result;

            signed long long lln { };

            eval_convert_to(&lln, nf);

            const int n { static_cast<int>(lln) };

            if (n > 0)
            {
               eval_ldexp(result, result, n);
            }
         }

         if (b_neg)
         {
            eval_divide(result, one, result);
         }
      }
   }
}

template <typename FloatingPointType,
          typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && (((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) >= 16) && ((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) <= 36)))>::type const*>
constexpr auto eval_exp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;

   constexpr double_float_type one { 1 };

   const int fpc { eval_fpclassify(x) };

   const bool b_neg { x.isneg_unchecked() };

   if (fpc == FP_ZERO)
   {
      result = one;
   }
   else if (fpc != FP_NORMAL)
   {
      if (fpc == FP_INFINITE)
      {
         result = (b_neg ? double_float_type(0) : double_float_type::my_value_inf());
      }
      else if (fpc == FP_NAN)
      {
         result = x;
      }
   }
   else
   {
      using local_float_type  = typename double_float_type::float_type;

      // Get a local copy of the argument and force it to be positive.
      const double_float_type xx { (!b_neg) ? x : -x };

      // Check the range of the input.
      if (eval_lt(x, double_float_type::my_value_logmin()))
      {
         result = double_float_type(0U);
      }
      else if (eval_gt(xx, double_float_type::my_value_logmax()))
      {
         result = double_float_type::my_value_inf();
      }
      else if (xx.is_one())
      {
         if(!b_neg)
         {
            result = cpp_df_qf_detail::constant_df_exp1<local_float_type>();
         }
         else
         {
            eval_divide(result, one, cpp_df_qf_detail::constant_df_exp1<local_float_type>());
         }
      }
      else
      {
         // Use an argument reduction algorithm for exp() in classic MPFUN-style.

         double_float_type nf { };

         // Prepare the scaled variables.
         const bool b_scale { (xx.order02() > -4) };

         double_float_type r { };

         if (b_scale)
         {
            eval_floor(nf, xx / cpp_df_qf_detail::constant_df_ln_two<local_float_type>());

            eval_ldexp(r, xx - (nf * cpp_df_qf_detail::constant_df_ln_two<local_float_type>()), -4);
         }
         else
         {
            r = xx;
         }

         // PadeApproximant[Exp[r], {r, 0, 8, 8}]
         // FullSimplify[%]

         constexpr double_float_type n144(144U);
         constexpr double_float_type n3603600(3603600UL);
         constexpr double_float_type n120120(120120UL);
         constexpr double_float_type n770(770U);

         constexpr double_float_type n518918400(518918400UL);
         constexpr double_float_type n259459200(259459200UL);
         constexpr double_float_type n60540480(60540480UL);
         constexpr double_float_type n8648640(8648640UL);
         constexpr double_float_type n831600(831600UL);
         constexpr double_float_type n55440(55440U);
         constexpr double_float_type n2520(2520U);
         constexpr double_float_type n72(72U);

         const double_float_type r2 { r * r };

         result = (n144 * r) * (n3603600 + r2 * (n120120 + r2 * (n770 + r2)));
         const double_float_type bot = (n518918400 + r * (-n259459200 + r * (n60540480 + r * (-n8648640 + r * (n831600 + r * (-n55440 + r * (n2520 + r * (-n72 + r))))))));

         eval_divide(result, bot);
         result.add_unchecked_limb(local_float_type { 1.0F });

         // Rescale the result.
         if (b_scale)
         {
            result *= result;
            result *= result;
            result *= result;
            result *= result;

            signed long long lln { };

            eval_convert_to(&lln, nf);

            const int n { static_cast<int>(lln) };

            if (n > 0)
            {
               eval_ldexp(result, result, n);
            }
         }

         if (b_neg)
         {
            eval_divide(result, one, result);
         }
      }
   }
}

template <typename FloatingPointType,
          typename ::std::enable_if<(cpp_df_qf_detail::is_floating_point<FloatingPointType>::value && ((cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits10 * 2) > 36))>::type const*>
constexpr auto eval_exp(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;

   constexpr double_float_type one { 1 };

   const int fpc { eval_fpclassify(x) };

   const bool b_neg { x.isneg_unchecked() };

   if (fpc == FP_ZERO)
   {
      result = one;
   }
   else if (fpc != FP_NORMAL)
   {
      if (fpc == FP_INFINITE)
      {
         result = (b_neg ? double_float_type(0) : double_float_type::my_value_inf());
      }
      else if (fpc == FP_NAN)
      {
         result = x;
      }
   }
   else
   {
      using local_float_type = typename double_float_type::float_type;

      // Get a local copy of the argument and force it to be positive.
      const double_float_type xx { (!b_neg) ? x : -x };

      // Check the range of the input.
      if (eval_lt(x, double_float_type::my_value_logmin()))
      {
         result = double_float_type(0U);
      }
      else if (eval_gt(xx, double_float_type::my_value_logmax()))
      {
         result = double_float_type::my_value_inf();
      }
      else if (xx.is_one())
      {
         if(!b_neg)
         {
            result = cpp_df_qf_detail::constant_df_exp1<local_float_type>();
         }
         else
         {
            eval_divide(result, one, cpp_df_qf_detail::constant_df_exp1<local_float_type>());
         }
      }
      else
      {
         // Use an argument reduction algorithm for exp() in classic MPFUN-style.

         double_float_type nf { };

         // Prepare the scaled variables.
         const bool b_scale { (xx.order02() > -4) };

         double_float_type xh { };

         if (b_scale)
         {
            eval_floor(nf, xx / cpp_df_qf_detail::constant_df_ln_two<local_float_type>());

            eval_ldexp(xh, xx - (nf * cpp_df_qf_detail::constant_df_ln_two<local_float_type>()), -4);
         }
         else
         {
            xh = xx;
         }

         double_float_type x_pow_n_div_n_fact(xh);

         result = x_pow_n_div_n_fact;
         result.add_unchecked_limb(local_float_type { 1.0F });

         double_float_type dummy { };

         // Use the Taylor series expansion of hypergeometric_0f0(; ; x).
         // For this high(er) digit count, a scaled argument with subsequent
         // Taylor series expansion is actually more precise than Pade approximation.
         for (unsigned n { UINT8_C(2) }; n < unsigned { UINT8_C(64) }; ++n)
         {
            eval_multiply(x_pow_n_div_n_fact, xh);

            eval_divide(x_pow_n_div_n_fact, double_float_type(n));

            int n_tol { };

            eval_frexp(dummy, x_pow_n_div_n_fact, &n_tol);

            if ((n > 4U) && (n_tol < -(double_float_type::my_digits - 1)))
            {
               break;
            }

            eval_add(result, x_pow_n_div_n_fact);
         }

         // Rescale the result.
         if (b_scale)
         {
            result *= result;
            result *= result;
            result *= result;
            result *= result;

            signed long long lln { };

            eval_convert_to(&lln, nf);

            const int n { static_cast<int>(lln) };

            if (n > 0)
            {
               eval_ldexp(result, result, n);
            }
         }

         if (b_neg)
         {
            eval_divide(result, one, result);
         }
      }
   }
}

template <typename FloatingPointType>
constexpr auto eval_log(cpp_double_fp_backend<FloatingPointType>& result, const cpp_double_fp_backend<FloatingPointType>& x) -> void
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;

   constexpr double_float_type one { 1 };

   const int result_compare_with_one { x.compare(one) };

   const int fpc { eval_fpclassify(x) };

   if (fpc == FP_ZERO)
   {
      result = -double_float_type::my_value_inf();
   }
   else if (x.isneg_unchecked() || (fpc == FP_NAN))
   {
      result = double_float_type::my_value_nan();
   }
   else if (fpc == FP_INFINITE)
   {
      result = double_float_type::my_value_inf();
   }
   else if (result_compare_with_one == -1)
   {
      // Use argument inversion and negation of the result.

      double_float_type x_inv { };

      eval_divide(x_inv, one, x);
      eval_log(result, x_inv);

      result.negate();
   }
   else if (result_compare_with_one == 1)
   {
      // Optimize to only use eval_frexp if (and only if) the exponent is
      // actually large/small in the sense of above/below a defined cutoff.

      double_float_type x2 { };

      int n2 { };

      eval_frexp(x2, x, &n2);

      // Get initial estimate using the self-written, detail math function log.
      using local_float_type = typename double_float_type::float_type;

      const local_float_type s { cpp_df_qf_detail::ccmath::log(x2.my_first()) };

      double_float_type E { };

      eval_exp(E, double_float_type(s));

      // Perform one single step of Newton-Raphson iteration.
      // result = s + (x2 - E) / E;

      eval_subtract(result, x2, E);
      eval_divide(result, E);
      result.add_unchecked_limb(s);

      double_float_type xn2 { n2 };

      eval_multiply(xn2, cpp_df_qf_detail::constant_df_ln_two<local_float_type>());

      eval_add(result, xn2);
   }
   else
   {
      result = double_float_type { 0 };
   }
}

namespace detail {

template<typename DestType, typename FloatingPointType>
constexpr auto extract(cpp_double_fp_backend<FloatingPointType>& source) -> DestType
{
   using double_float_type = cpp_double_fp_backend<FloatingPointType>;
   using local_float_type =  typename double_float_type::float_type;

   using destination_type = DestType;

   destination_type result { };

   constexpr double_float_type zero { 0 };

   result = static_cast<destination_type>(0);

   unsigned fail_safe { UINT32_C(32) };

   constexpr bool
      destination_type_is_longer
      {
         (std::numeric_limits<signed long long>::digits > cpp_df_qf_detail::ccmath::numeric_limits<local_float_type>::digits)
      };

   using float_extract_type = typename std::conditional<destination_type_is_longer, local_float_type, float>::type;

   while((source.compare(zero) != 0) && (fail_safe > unsigned { UINT8_C(0) }))
   {
      const float_extract_type next_flt_val { static_cast<float_extract_type>(source.my_first()) };

      result += static_cast<destination_type>(next_flt_val);

      eval_subtract(source, double_float_type(next_flt_val));

      --fail_safe;
   }

   return result;
}

} // namespace detail

template <typename FloatingPointType>
constexpr auto eval_convert_to(signed long long* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> void
{
   const auto fpc = eval_fpclassify(backend);

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<signed long long>(backend.crep().first);
   }
   else
   {
      using double_float_type = cpp_double_fp_backend<FloatingPointType>;
      using local_float_type =  typename double_float_type::float_type;

      static_assert(std::is_same<local_float_type, FloatingPointType>::value, "Error something went wrong with the limb type");

      constexpr bool
         long_long_is_longer
         {
            (std::numeric_limits<signed long long>::digits > cpp_df_qf_detail::ccmath::numeric_limits<local_float_type>::digits)
         };

      using longer_type = typename ::std::conditional<long_long_is_longer, signed long long, double_float_type>::type;

      constexpr longer_type my_max_val { static_cast<longer_type>((std::numeric_limits<signed long long>::max)()) };
      constexpr longer_type my_min_val { static_cast<longer_type>((std::numeric_limits<signed long long>::min)()) };

      constexpr double_float_type my_max_val_dd { static_cast<double_float_type>(my_max_val) };
      constexpr double_float_type my_min_val_dd { static_cast<double_float_type>(my_min_val) };

      if (backend.compare(my_max_val_dd) >= 0)
      {
         *result = (std::numeric_limits<signed long long>::max)();
      }
      else if (backend.compare(my_min_val_dd) <= 0)
      {
         *result = (std::numeric_limits<signed long long>::min)();
      }
      else
      {
         double_float_type source { backend };

         *result = detail::extract<signed long long>(source);

         #if !defined(__x86_64__) && !defined(_M_X64)

         // It has been "empirically found" that non-X64 needs this workaround.
         // Even though the same conditions are met for x86_64 on GCC and MSVC,
         // this workaround will actually break the long long conversion tests
         // on those platforms.
         //
         // Our assumption is that on x64 there is x87 math (double -> long double)
         // being performed in the background. Seemingly these might "aid" the conversion
         // of double value to long long. Somehow I get the feeling this issue will arise
         // in future evolution of the cpp_double_fp_backend.
         //
         // This workaround has been tested on: ARM64 (linux and mac), s390x and PPC64LE.

         constexpr bool
            needs_workaround
            {
                  (sizeof(signed long long) == 8U)
               && (std::is_same<local_float_type, double>::value || (std::is_same<local_float_type, long double>::value))
            };

         BOOST_IF_CONSTEXPR (needs_workaround)
         {
            // This is the last value stored in a double as 9223372036854775808
            constexpr signed long long upper_bound = 9223372036854775296LL;

            if (!eval_signbit(backend) && *result >= upper_bound)
            {
               // LONG_MAX is stored with .second = -1, so we compensate for the offset
               // We also only need this at the upper end where the values aren't exactly
               // representable in double. Below a certain point we are fine
               *result += static_cast<signed long long>(1);
            }
         }

         #endif // non-x64
      }
   }
}

template <typename FloatingPointType>
constexpr auto eval_convert_to(unsigned long long* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> void
{
   const auto fpc = eval_fpclassify(backend);

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<unsigned long long>(backend.crep().first);
   }
   else
   {
      using double_float_type = cpp_double_fp_backend<FloatingPointType>;
      using local_float_type =  typename double_float_type::float_type;

      static_assert(std::is_same<local_float_type, FloatingPointType>::value, "Error something went wrong with the limb type");

      constexpr bool
         ulong_long_is_longer
         {
            (std::numeric_limits<unsigned long long>::digits > cpp_df_qf_detail::ccmath::numeric_limits<local_float_type>::digits)
         };

      using longer_type = typename ::std::conditional<ulong_long_is_longer, unsigned long long, double_float_type>::type;

      constexpr longer_type my_max_val { static_cast<longer_type>((std::numeric_limits<unsigned long long>::max)()) };

      constexpr double_float_type my_max_val_dd { static_cast<double_float_type>(my_max_val) };

      if (backend.compare(my_max_val_dd) >= 0)
      {
         *result = (std::numeric_limits<unsigned long long>::max)();
      }
      else
      {
         double_float_type source { backend };

         *result = detail::extract<unsigned long long>(source);
      }
   }
}

#ifdef BOOST_HAS_INT128
template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::int128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type
{
   const auto fpc = eval_fpclassify(backend);

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<boost::int128_type>(backend.crep().first);
   }
   else
   {
      using double_float_type = cpp_double_fp_backend<FloatingPointType>;
      using local_float_type =  typename double_float_type::float_type;

      static_assert(std::is_same<local_float_type, FloatingPointType>::value, "Error something went wrong with the limb type");

      constexpr bool
         n128_is_longer
         {
            ((static_cast<int>(sizeof(boost::int128_type) * static_cast<std::size_t>(CHAR_BIT)) - 1) > cpp_df_qf_detail::ccmath::numeric_limits<local_float_type>::digits)
         };

      using longer_type = typename ::std::conditional<n128_is_longer, boost::int128_type, double_float_type>::type;

      constexpr boost::int128_type my_max_val_n128 = (((static_cast<boost::int128_type>(1) << (sizeof(boost::int128_type) * CHAR_BIT - 2)) - 1) << 1) + 1;
      constexpr boost::int128_type my_min_val_n128 = static_cast<boost::int128_type>(-my_max_val_n128 - 1);

      constexpr longer_type my_max_val(static_cast<longer_type>(my_max_val_n128));
      constexpr longer_type my_min_val(static_cast<longer_type>(my_min_val_n128));

      constexpr double_float_type my_max_val_dd(static_cast<double_float_type>(my_max_val));
      constexpr double_float_type my_min_val_dd(static_cast<double_float_type>(my_min_val));

      if (backend.compare(my_max_val_dd) >= 0)
      {
         *result = my_max_val;
      }
      else if (backend.compare(my_min_val_dd) <= 0)
      {
         *result = my_min_val;
      }
      else
      {
         double_float_type source { backend };

         *result = detail::extract<boost::int128_type>(source);
      }
   }
}

template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::int128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<!(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type
{
   const auto fpc = eval_fpclassify(backend);

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<boost::int128_type>(backend.crep().first);
   }
   else
   {
      using double_float_type = cpp_double_fp_backend<FloatingPointType>;

      double_float_type source { backend };

      *result = detail::extract<boost::int128_type>(source);
   }
}

template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::uint128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type
{
   const auto fpc = eval_fpclassify(backend);

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<boost::int128_type>(backend.crep().first);
   }
   else
   {
      using double_float_type = cpp_double_fp_backend<FloatingPointType>;
      using local_float_type =  typename double_float_type::float_type;

      static_assert(std::is_same<local_float_type, FloatingPointType>::value, "Error something went wrong with the limb type");

      constexpr bool
         u128_is_longer
         {
            (static_cast<int>(sizeof(boost::uint128_type) * static_cast<std::size_t>(CHAR_BIT)) > cpp_df_qf_detail::ccmath::numeric_limits<local_float_type>::digits)
         };

      using longer_type = typename ::std::conditional<u128_is_longer, boost::uint128_type, double_float_type>::type;

      constexpr boost::uint128_type my_max_val_u128 = static_cast<boost::uint128_type>(~static_cast<boost::uint128_type>(0));

      constexpr longer_type my_max_val(static_cast<longer_type>(my_max_val_u128));

      constexpr double_float_type my_max_val_dd(static_cast<double_float_type>(my_max_val));

      if (backend.compare(my_max_val_dd) >= 0)
      {
         *result = my_max_val;
      }
      else
      {
         double_float_type source { backend };

         *result = detail::extract<boost::uint128_type>(source);
      }
   }
}

template <typename FloatingPointType>
constexpr auto eval_convert_to(boost::uint128_type* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename std::enable_if<!(cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits > 24), void>::type
{
   const auto fpc = eval_fpclassify(backend);

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<boost::uint128_type>(backend.crep().first);
   }
   else
   {
      using double_float_type = cpp_double_fp_backend<FloatingPointType>;

      double_float_type source { backend };

      *result = detail::extract<boost::uint128_type>(source);
   }
}
#endif

template <typename FloatingPointType,
          typename OtherFloatingPointType>
constexpr auto eval_convert_to(OtherFloatingPointType* result, const cpp_double_fp_backend<FloatingPointType>& backend) -> typename ::std::enable_if<cpp_df_qf_detail::is_floating_point<OtherFloatingPointType>::value>::type
{
   const auto fpc = eval_fpclassify(backend);

   // TBD: Implement min/max check for the destination floating-point type result.

   if (fpc != FP_NORMAL)
   {
      *result = static_cast<OtherFloatingPointType>(backend.my_first());
   }
   else
   {
      BOOST_IF_CONSTEXPR(cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatingPointType>::digits > cpp_df_qf_detail::ccmath::numeric_limits<FloatingPointType>::digits)
      {
         *result  = static_cast<OtherFloatingPointType>(backend.my_first());
         *result += static_cast<OtherFloatingPointType>(backend.my_second());
      }
      else
      {
         cpp_double_fp_backend<FloatingPointType> source = backend;

         *result = 0;

         for(auto digit_count  = static_cast<int>(0);
                  digit_count  < cpp_double_fp_backend<FloatingPointType>::my_digits;
                  digit_count += cpp_df_qf_detail::ccmath::numeric_limits<OtherFloatingPointType>::digits)
         {
            const auto next = static_cast<OtherFloatingPointType>(source.my_first());

            *result += next;

            eval_subtract(source, cpp_double_fp_backend<FloatingPointType>(next));
         }
      }
   }
}

template <typename FloatingPointType>
constexpr auto hash_value(const cpp_double_fp_backend<FloatingPointType>& a) -> ::std::size_t
{
   return a.hash();
}

} // namespace backends

using backends::cpp_double_fp_backend;

using cpp_double_float       = number<cpp_double_fp_backend<float>,       ::boost::multiprecision::et_off>;
using cpp_double_double      = number<cpp_double_fp_backend<double>,      ::boost::multiprecision::et_off>;
using cpp_double_long_double = number<cpp_double_fp_backend<long double>, ::boost::multiprecision::et_off>;
#ifdef BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128
using cpp_double_float128    = number<cpp_double_fp_backend<::boost::float128_type>, ::boost::multiprecision::et_off>;
#endif

} } // namespace boost::multiprecision

namespace std {

// Specialization of numeric_limits for boost::multiprecision::number<cpp_double_fp_backend<>>
template <typename FloatingPointType,
          const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
BOOST_MP_DF_QF_NUM_LIMITS_CLASS_TYPE numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >
{
 private:
   using local_float_type = FloatingPointType;
   using inner_self_type  = boost::multiprecision::cpp_double_fp_backend<local_float_type>;

   using self_type = boost::multiprecision::number<inner_self_type, ExpressionTemplatesOption>;

 public:
   static constexpr bool                    is_specialized                = true;
   static constexpr bool                    is_signed                     = true;
   static constexpr bool                    is_integer                    = false;
   static constexpr bool                    is_exact                      = false;
   static constexpr bool                    is_bounded                    = true;
   static constexpr bool                    is_modulo                     = false;
   static constexpr bool                    is_iec559                     = false;
   static constexpr bool                    has_infinity                  = true;
   static constexpr bool                    has_quiet_NaN                 = true;
   static constexpr bool                    has_signaling_NaN             = false;

   // These members were deprecated in C++23, but only MSVC warns (as of June 25)
   #ifdef BOOST_MSVC
   #pragma warning(push)
   #pragma warning(disable:4996)
   #endif

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4996)
#elif (defined(__clang__) && (__clang_major__ >= 17))
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
   static constexpr float_denorm_style      has_denorm                    = denorm_absent;
#ifdef _MSC_VER
#pragma warning(pop)
#elif (defined(__clang__) && (__clang_major__ >= 17))
#  pragma clang diagnostic pop
#endif
   static constexpr bool                    has_denorm_loss               = true;

   #ifdef BOOST_MSVC
   #pragma warning(pop)
   #endif // deprecated members

   static constexpr bool                    traps                         = false;
   static constexpr bool                    tinyness_before               = false;
   static constexpr float_round_style       round_style                   = round_to_nearest;

   static constexpr int radix                          = 2;
   static constexpr int digits                         = inner_self_type::my_digits;
   static constexpr int digits10                       = inner_self_type::my_digits10;
   static constexpr int max_digits10                   = inner_self_type::my_max_digits10;

   static constexpr int max_exponent                   = inner_self_type::my_max_exponent;
   static constexpr int min_exponent                   = inner_self_type::my_min_exponent;
   static constexpr int max_exponent10                 = inner_self_type::my_max_exponent10;
   static constexpr int min_exponent10                 = inner_self_type::my_min_exponent10;

   static constexpr auto(min)         () noexcept -> self_type { return static_cast<self_type>(inner_self_type::my_value_min()); }
   static constexpr auto(max)         () noexcept -> self_type { return static_cast<self_type>(inner_self_type::my_value_max()); }
   static constexpr auto lowest       () noexcept -> self_type { return static_cast<self_type>(-(max)()); }
   static constexpr auto epsilon      () noexcept -> self_type { return static_cast<self_type>(inner_self_type::my_value_eps()); }
   static constexpr auto round_error  () noexcept -> self_type { return static_cast<self_type>(static_cast<local_float_type>(0.5)); }
   static constexpr auto denorm_min   () noexcept -> self_type { return static_cast<self_type>((min)()); }
   static constexpr auto infinity     () noexcept -> self_type { return static_cast<self_type>(inner_self_type::my_value_inf()); }
   static constexpr auto quiet_NaN    () noexcept -> self_type { return static_cast<self_type>(inner_self_type::my_value_nan()); }
   static constexpr auto signaling_NaN() noexcept -> self_type { return static_cast<self_type>(static_cast<local_float_type>(0.0)); }
};

} // namespace std

template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_specialized;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_signed;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_integer;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_exact;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_bounded;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_modulo;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::is_iec559;

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4996)
#endif

template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr std::float_denorm_style std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::has_denorm;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::has_denorm_loss;

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif // deprecated members

template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::has_infinity;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::has_quiet_NaN;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::has_signaling_NaN;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::traps;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::tinyness_before;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr std::float_round_style  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::round_style;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::radix;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::digits;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::digits10;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::max_digits10;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::max_exponent;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::min_exponent;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::max_exponent10;
template <typename FloatingPointType, const boost::multiprecision::expression_template_option ExpressionTemplatesOption>
constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplatesOption> >::min_exponent10;

#if defined(BOOST_MP_MATH_AVAILABLE)
namespace boost { namespace math { namespace policies {

template <class FloatingPointType, class Policy, boost::multiprecision::expression_template_option ExpressionTemplates>
struct precision<boost::multiprecision::number<boost::multiprecision::cpp_double_fp_backend<FloatingPointType>, ExpressionTemplates>, Policy>
{
private:
   using my_multiprecision_backend_type = boost::multiprecision::cpp_double_fp_backend<FloatingPointType>;

   using digits2_type = digits2<my_multiprecision_backend_type::my_digits>;

   static constexpr auto use_full_precision() noexcept -> bool
   {
      return ((digits2_type::value <= precision_type::value) || (precision_type::value <= 0));
   }

public:
   using precision_type = typename Policy::precision_type;

   using type =
      typename std::conditional<use_full_precision(),
                                digits2_type,           // This is the default case: Use full precision for FloatingPointType.
                                precision_type>::type;  // Here we find (and use) user-customized precision.
};

} } } // namespace boost::math::policies
#endif

#endif // BOOST_MP_CPP_DOUBLE_FP_2021_06_05_HPP
