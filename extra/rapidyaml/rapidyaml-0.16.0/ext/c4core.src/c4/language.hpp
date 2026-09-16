#ifndef C4_LANGUAGE_HPP_
#define C4_LANGUAGE_HPP_

/** @file language.hpp Provides language standard information macros and
 * compiler agnostic utility macros: namespace facilities, function attributes,
 * variable attributes, etc.
 * @ingroup basic_headers */

#include "c4/preprocessor.hpp"
#include "c4/compiler.hpp"
#include "c4/export.hpp"

/* Detect C++ standard.
 * @see http://stackoverflow.com/a/7132549/5875572 */
#ifndef C4_CPP
#   if defined(_MSC_VER) && !defined(__clang__)
#       if _MSC_VER >= 1910  // >VS2015: VS2017, VS2019, VS2022
#           if (!defined(_MSVC_LANG))
#               error _MSVC not defined
#           endif
#           if _MSVC_LANG >= 202302L
#               define C4_CPP 23
#           elif _MSVC_LANG >= 202002L
#               define C4_CPP 20
#           elif _MSVC_LANG == 201703L
#               define C4_CPP 17
#           elif _MSVC_LANG >= 201402L
#               define C4_CPP 14
#           elif _MSVC_LANG >= 201103L
#               define C4_CPP 11
#           else
#               error C++ lesser than C++11 not supported
#           endif
#       else
#           if _MSC_VER == 1900
#               define C4_CPP 14  // VS2015 is c++14 https://devblogs.microsoft.com/cppblog/c111417-features-in-vs-2015-rtm/
#           elif _MSC_VER == 1800 // VS2013
#               define C4_CPP 11
#           else
#               error C++ lesser than C++11 not supported
#           endif
#       endif
#   elif defined(__INTEL_COMPILER) // not sure about this https://software.intel.com/en-us/node/524490
#       ifdef __INTEL_CXX23_MODE__
#           define C4_CPP 23
#       elif defined __INTEL_CXX20_MODE__
#           define C4_CPP 20
#       elif defined __INTEL_CXX17_MODE__
#           define C4_CPP 17
#       elif defined __INTEL_CXX14_MODE__
#           define C4_CPP 14
#       elif defined __INTEL_CXX11_MODE__
#           define C4_CPP 11
#       else
#           error C++ lesser than C++11 not supported
#       endif
#   else
#       ifndef __cplusplus
#           error __cplusplus is not defined?
#       endif
#       if __cplusplus == 1
#           error cannot handle __cplusplus==1
#       elif __cplusplus >= 202302L
#           define C4_CPP 23
#       elif __cplusplus >= 202002L
#           define C4_CPP 20
#       elif __cplusplus >= 201703L
#           define C4_CPP 17
#       elif __cplusplus >= 201402L
#           define C4_CPP 14
#       elif __cplusplus >= 201103L
#           define C4_CPP 11
#       elif __cplusplus >= 199711L
#           error C++ lesser than C++11 not supported
#       endif
#   endif
#endif
#if C4_CPP >= 23
#   define C4_CPP23
#endif
#if C4_CPP >= 20
#   define C4_CPP20
#endif
#if C4_CPP >= 17
#   define C4_CPP17
#endif
#if C4_CPP >= 14
#   define C4_CPP14
#endif
#if C4_CPP >= 11
#   define C4_CPP11
#endif


/** lifted from this answer: http://stackoverflow.com/a/20170989/5875572 */
#if defined(_MSC_VER) && !defined(__clang__)
#  if _MSC_VER < 1900
#    define C4_CONSTEXPR11
#    define C4_CONSTEXPR14
#  elif _MSC_VER < 2000
#    define C4_CONSTEXPR11 constexpr
#    define C4_CONSTEXPR14
#  else
#    define C4_CONSTEXPR11 constexpr
#    define C4_CONSTEXPR14 constexpr
#  endif
#else
#  if __cplusplus < 201103
#    define C4_CONSTEXPR11
#    define C4_CONSTEXPR14
#  elif __cplusplus == 201103
#    define C4_CONSTEXPR11 constexpr
#    define C4_CONSTEXPR14
#  else
#    define C4_CONSTEXPR11 constexpr
#    define C4_CONSTEXPR14 constexpr
#  endif
#endif  // _MSC_VER


#if C4_CPP < 17
#define C4_IF_CONSTEXPR
#define C4_INLINE_CONSTEXPR constexpr
#else
#define C4_IF_CONSTEXPR constexpr
#define C4_INLINE_CONSTEXPR inline constexpr
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#  if (defined(_CPPUNWIND) && (_CPPUNWIND == 1))
#    define C4_EXCEPTIONS
#  endif
#else
#  if defined(__EXCEPTIONS) || defined(__cpp_exceptions)
#    define C4_EXCEPTIONS
#  endif
#endif

#ifdef C4_EXCEPTIONS
#  define C4_IF_EXCEPTIONS_(exc_code, setjmp_code) exc_code
#  define C4_IF_EXCEPTIONS(exc_code, setjmp_code) do { exc_code } while(0)
#else
#  define C4_IF_EXCEPTIONS_(exc_code, setjmp_code) setjmp_code
#  define C4_IF_EXCEPTIONS(exc_code, setjmp_code) do { setjmp_code } while(0)
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#  if defined(_CPPRTTI)
#    define C4_RTTI
#  endif
#else
#  if defined(__GXX_RTTI)
#    define C4_RTTI
#  endif
#endif

#ifdef C4_RTTI
#  define C4_IF_RTTI_(code_rtti, code_no_rtti) code_rtti
#  define C4_IF_RTTI(code_rtti, code_no_rtti) do { code_rtti } while(0)
#else
#  define C4_IF_RTTI_(code_rtti, code_no_rtti) code_no_rtti
#  define C4_IF_RTTI(code_rtti, code_no_rtti) do { code_no_rtti } while(0)
#endif


//------------------------------------------------------------

#define C4_BEGIN_NAMESPACE(ns) namespace ns {
#define C4_END_NAMESPACE(ns)   }

// MSVC cant handle the C4_FOR_EACH macro... need to fix this
//#define C4_BEGIN_NAMESPACE(...) C4_FOR_EACH_SEP(C4_BEGIN_NAMESPACE, , __VA_ARGS__)
//#define C4_END_NAMESPACE(...) C4_FOR_EACH_SEP(C4_END_NAMESPACE, , __VA_ARGS__)
#define C4_BEGIN_NAMESPACE(ns) namespace ns {
#define C4_END_NAMESPACE(ns) }

#define C4_BEGIN_HIDDEN_NAMESPACE namespace /*hidden*/ {
#define C4_END_HIDDEN_NAMESPACE } /* namespace hidden */


//------------------------------------------------------------

#ifndef C4_API
#   if defined(_MSC_VER) && !defined(__clang__)
#       if defined(C4_EXPORT)
#           define C4_API __declspec(dllexport)
#       elif defined(C4_IMPORT)
#           define C4_API __declspec(dllimport)
#       else
#           define C4_API
#       endif
#   else
#       define C4_API
#   endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#   define C4_RESTRICT __restrict
#   define C4_RESTRICT_FN __declspec(restrict)
#   define C4_NO_INLINE __declspec(noinline)
#   define C4_ALWAYS_INLINE inline __forceinline
/** these are not available in VS AFAIK */
#   define C4_CONST
#   define C4_PURE
#   define C4_FLATTEN
#   define C4_HOT         /** @todo */
#   define C4_COLD        /** @todo */
#   define C4_ASSUME(...) __assume(__VA_ARGS__)
#   define C4_EXPECT(x, y) x /** @todo */
#   define C4_UNREACHABLE() _c4_msvc_unreachable()
#   define C4_ATTR_FORMAT(...) /** */
#   define C4_NORETURN [[noreturn]]
#   if _MSC_VER >= 1700 // VS2012
#       define C4_NODISCARD _Check_return_
#   else
#       define C4_NODISCARD
#   endif
#if C4_CPP >= 17
#   define C4_MAYBE_UNUSED [[maybe_unused]]
#else
#   define C4_MAYBE_UNUSED
#endif
[[noreturn]] __forceinline void _c4_msvc_unreachable() { __assume(false); } ///< https://stackoverflow.com/questions/60802864/emulating-gccs-builtin-unreachable-in-visual-studio
#   define C4_UNREACHABLE_AFTER_ERR() /* */
#else
    ///< @todo assuming gcc-like compiler. check it is actually so.
/** for function attributes in GCC,
 * @see https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes */
/** for __builtin functions in GCC,
 * @see https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html */
#   define C4_RESTRICT __restrict__
#   define C4_RESTRICT_FN __attribute__((restrict))
#   define C4_NO_INLINE __attribute__((noinline))
#   define C4_ALWAYS_INLINE inline __attribute__((always_inline))
#   define C4_CONST __attribute__((const))
#   define C4_PURE __attribute__((pure))
/** force inlining of every callee function */
#   define C4_FLATTEN __atribute__((flatten))
/** mark a function as hot, ie as having a visible impact in CPU time
 * thus making it more likely to inline, etc
 * @see http://stackoverflow.com/questions/15028990/semantics-of-gcc-hot-attribute */
#   define C4_HOT __attribute__((hot))
/** mark a function as cold, ie as NOT having a visible impact in CPU time
 * @see http://stackoverflow.com/questions/15028990/semantics-of-gcc-hot-attribute */
#   define C4_COLD __attribute__((cold))
#   define C4_EXPECT(x, y) __builtin_expect(x, y) ///< @see https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
#   define C4_UNREACHABLE() __builtin_unreachable()
#   define C4_ATTR_FORMAT(...) //__attribute__((format (__VA_ARGS__))) ///< @see https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes
#   define C4_NORETURN __attribute__((noreturn))
#   define C4_NODISCARD __attribute__((warn_unused_result))
#if C4_CPP >= 17
#   define C4_MAYBE_UNUSED [[maybe_unused]]
#else
#   define C4_MAYBE_UNUSED __attribute__((unused))
#endif
#   define C4_UNREACHABLE_AFTER_ERR() C4_UNREACHABLE()
// C4_ASSUME
// see https://stackoverflow.com/questions/63493968/reproducing-clangs-builtin-assume-for-gcc
// preferred option: C++ standard attribute
#   ifdef __has_cpp_attribute
#       if __has_cpp_attribute(assume) >= 202207L
#           define C4_ASSUME(...) [[assume(__VA_ARGS__)]]
#       endif
#   endif
// first fallback: compiler intrinsics/attributes for assumptions
#   ifndef C4_ASSUME
#       if defined(__clang__)
#           define C4_ASSUME(...) __builtin_assume(__VA_ARGS__)
#       elif defined(__GNUC__)
#       if __GNUC__ >= 13
#           define C4_ASSUME(...) __attribute__((__assume__(__VA_ARGS__)))
#       endif
#       endif
#   endif
// second fallback: possibly evaluating uses of unreachable()
// Set this to 1 if you want to allow assumptions to possibly evaluate.
#   ifndef C4_ASSUME_ALLOW_EVAL
#       define C4_ASSUME_ALLOW_EVAL 0
#   endif
#   if !defined(C4_ASSUME) && (C4_ASSUME_ALLOW_EVAL)
#       define C4_ASSUME(...) do { if (!bool(__VA_ARGS__)) C4_UNREACHABLE(); ) while(0)
#   endif
// last fallback: define macro as doing nothing
#   ifndef C4_ASSUME
#       define C4_ASSUME(...)
#   endif
#endif


#ifdef __has_cpp_attribute
#   if (__has_cpp_attribute(unlikely) >= 201803L) && (C4_CPP >= 20)
#       define C4_UNLIKELY_IS_ATTR_
#   endif
#endif

#ifdef C4_UNLIKELY_IS_ATTR_
#   define C4_LIKELY(x)   (x) [[likely]]
#   define C4_UNLIKELY(x) (x) [[unlikely]]
#   define C4_LIKELY20   [[likely]]
#   define C4_UNLIKELY20 [[unlikely]]
#elif defined(__clang__) || defined(__GNUC__)
#   define C4_LIKELY(x)   (__builtin_expect(x, 1))
#   define C4_UNLIKELY(x) (__builtin_expect(x, 0))
#   define C4_LIKELY20
#   define C4_UNLIKELY20
#elif defined(_MSC_VER)
#   define C4_LIKELY(x)   (x)
#   define C4_UNLIKELY(x) (x)
#   define C4_LIKELY20
#   define C4_UNLIKELY20
#endif


#if C4_CPP >= 14
#   define C4_DEPRECATED(msg) [[deprecated(msg)]]
#else
#   if defined(_MSC_VER)
#       define C4_DEPRECATED(msg) __declspec(deprecated(msg))
#   else // defined(__GNUC__) || defined(__clang__)
#       define C4_DEPRECATED(msg) __attribute__((deprecated(msg)))
#   endif
#endif


#ifdef _MSC_VER
#   define C4_FUNC __FUNCTION__
#   define C4_PRETTY_FUNC __FUNCSIG__
#else /// @todo assuming gcc-like compiler. check it is actually so.
#   define C4_FUNC __FUNCTION__
#   define C4_PRETTY_FUNC __PRETTY_FUNCTION__
#endif


/** prevent compiler warnings about a specific var being unused */
#define C4_UNUSED(var) (void)var

#if C4_CPP >= 17
#define C4_STATIC_ASSERT(cond) static_assert(cond)
#else
#define C4_STATIC_ASSERT(cond) static_assert((cond), #cond)
#endif
#define C4_STATIC_ASSERT_MSG(cond, msg) static_assert((cond), #cond ": " msg)


/** @def C4_DONT_OPTIMIZE() idea taken from GoogleBenchmark.
 * @see https://github.com/google/benchmark/blob/master/include/benchmark/benchmark_api.h */
namespace c4 {
namespace detail {
#if defined(__GNUC__)
#   define C4_DONT_OPTIMIZE(var) c4::detail::dont_optimize(var)
template< class T >
C4_ALWAYS_INLINE void dont_optimize(T const& value) { asm volatile("" : : "g"(value) : "memory"); } // NOLINT
#else
#   define C4_DONT_OPTIMIZE(var) c4::detail::use_char_pointer(reinterpret_cast< const char* >(&var))
C4CORE_EXPORT void use_char_pointer(char const volatile*);
#endif
} // namespace detail
} // namespace c4


/** @def C4_KEEP_EMPTY_LOOP() prevent an empty loop from being optimized out.
 * @see http://stackoverflow.com/a/7084193/5875572  */
#if defined(_MSC_VER) && !defined(__clang__)
#   define C4_KEEP_EMPTY_LOOP { char c; C4_DONT_OPTIMIZE(c); }
#else
#   define C4_KEEP_EMPTY_LOOP { asm(""); }
#endif

#endif /* C4_LANGUAGE_HPP_ */
