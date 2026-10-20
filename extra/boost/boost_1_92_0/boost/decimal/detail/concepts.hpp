// Copyright 2022 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CONCEPTS
#define BOOST_DECIMAL_DETAIL_CONCEPTS

#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/type_traits.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#endif

// GCC-11 yields internal compiler errors when using the concepts

/*
./boost/decimal/detail/concepts.hpp:239:80: note: in definition of macro 'BOOST_DECIMAL_REQUIRES_RETURN'
  239 | #define BOOST_DECIMAL_REQUIRES_RETURN(X, T, ReturnType) -> ReturnType requires X<T>
      |                                                                                ^
0xe3223b internal_error(char const*, ...)
    ???:0
0xf56ed4 duplicate_decls(tree_node*, tree_node*, bool, bool)
    ???:0
0xf60a2b pushdecl_namespace_level(tree_node*, bool)
    ???:0
0x10801ca push_template_decl(tree_node*, bool)
    ???:0
0x1527ec1 do_friend(tree_node*, tree_node*, tree_node*, tree_node*, overload_flags, bool)
    ???:0
0xfc4e1e grokdeclarator(cp_declarator const*, cp_decl_specifier_seq*, decl_context, int, tree_node**)
    ???:0
0x100dcf4 grokfield(cp_declarator const*, cp_decl_specifier_seq*, tree_node*, bool, tree_node*, tree_node*)
    ???:0
0x149dce3 c_parse_file()
    ???:0
0x148d4de c_common_parse_file()
    ???:0
*/
#if (__cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)) && !defined(BOOST_MATH_DISABLE_CONCEPTS) &&\
    (!defined(__GNUC__) || __GNUC__ != 11)

#if __has_include(<concepts>)

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <utility>
#include <algorithm>
#include <concepts>
#include <functional>
#include <limits>
#include <iterator>
#include <complex>
#endif

namespace boost::decimal::detail::concepts {

template <typename X, typename Y, typename Op>
struct op_valid_impl
{
    template <typename U, typename L, typename R>
    static constexpr auto test(int) -> decltype(std::declval<U>()(std::declval<L>(), std::declval<R>()),
                                                void(), std::true_type());

    template <typename U, typename L, typename R>
    static constexpr auto test(...) -> std::false_type;

    using type = decltype(test<Op, X, Y>(0));
};

template <typename X, typename Y, typename Op>
using op_valid_t = typename op_valid_impl<X, Y, Op>::type;

template <typename X, typename Y, typename Op>
inline constexpr bool op_valid_v = op_valid_t<X, Y, Op>::value;

// Detector for class member functions
struct nonesuch
{
    nonesuch(const nonesuch&) = delete;
    ~nonesuch() = delete;
    void operator=(const nonesuch&) = delete;
};

template <typename Default, typename AlwaysVoid, template<typename...> typename Op, typename... Args>
struct detector
{
    using value_t = std::false_type;
    using type = Default;
};

template <typename Default, template<typename...> typename Op, typename... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
{
    using value_t = std::true_type;
    using type = Op<Args...>;
};

template <template<typename...> typename Op, typename... Args>
using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

template <template<typename...> typename Op, typename... Args>
using detected_t = typename detector<nonesuch, void, Op, Args...>::type;

#define BOOST_DECIMAL_HAS_MEMBER_FUNCTION(member)                                      \
template <typename T>                                                               \
using has_##member##_t = decltype(std::declval<T&>().member());                     \
                                                                                    \
template <typename T>                                                               \
inline constexpr bool has_##member##_v = is_detected<has_##member##_t, T>::value;

BOOST_DECIMAL_HAS_MEMBER_FUNCTION(begin)
BOOST_DECIMAL_HAS_MEMBER_FUNCTION(end)
BOOST_DECIMAL_HAS_MEMBER_FUNCTION(real)
BOOST_DECIMAL_HAS_MEMBER_FUNCTION(imag)

template <typename T>
concept integral = boost::decimal::detail::is_integral_v<T>;

template <typename T>
concept signed_integral = integral<T> && boost::decimal::detail::is_signed_v<T>;

template <typename T>
concept unsigned_integral = integral<T> && boost::decimal::detail::is_unsigned_v<T>;

template <typename T>
concept real = boost::decimal::detail::is_floating_point_v<T>;

template <typename T>
concept complex = std::is_same_v<T, std::complex<float>>
                  || std::is_same_v<T, std::complex<double>>
                  #ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
                  || std::is_same_v<T, std::complex<long double>>
                  #endif
                  ;

template <typename T>
concept real_or_complex = real<T> || complex<T>;

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept numerical = arithmetic<T> || real_or_complex<T>;

template <typename T>
concept signed_arithmetic = arithmetic<T> && std::is_signed_v<T>;

template <typename T>
concept unsigned_arithmetic = arithmetic<T> && std::is_unsigned_v<T>;

template <typename T>
concept arbitrary_unsigned_arithmetic_type = unsigned_arithmetic<T> ||
                                             (op_valid_v<T, T, std::equal_to<>> &&
                                              op_valid_v<T, T, std::not_equal_to<>> &&
                                              op_valid_v<T, T, std::greater<>> &&
                                              op_valid_v<T, T, std::less<>> &&
                                              op_valid_v<T, T, std::greater_equal<>> &&
                                              op_valid_v<T, T, std::less_equal<>> &&
                                              op_valid_v<T, T, std::plus<>> &&
                                              op_valid_v<T, T, std::minus<>> &&
                                              op_valid_v<T, T, std::multiplies<>> &&
                                              op_valid_v<T, T, std::divides<>>);

template <typename T>
concept arbitrary_signed_arithmetic_type = signed_arithmetic<T> ||
                                           (arbitrary_unsigned_arithmetic_type<T> &&
                                            (op_valid_v<T, T, std::negate<>> ||
                                             std::numeric_limits<T>::is_signed));

template <typename T>
concept arbitrary_arithmetic_type = arbitrary_unsigned_arithmetic_type<T> ||
                                    arbitrary_signed_arithmetic_type<T>;

template <typename T>
concept arbitrary_unsigned_integer_type = arbitrary_unsigned_arithmetic_type<T> &&
                                           std::numeric_limits<T>::is_integer;

template <typename T>
concept arbitrary_signed_integer_type = arbitrary_signed_arithmetic_type<T> &&
                                         std::numeric_limits<T>::is_integer;

template <typename T>
concept arbitrary_integer_type = arbitrary_unsigned_integer_type<T> ||
                                  arbitrary_signed_integer_type<T>;

template <typename T>
concept arbitrary_real_type = arbitrary_arithmetic_type<T> &&
                               !std::numeric_limits<T>::is_integer;

template <typename T>
concept arbitrary_complex_type = complex<T> ||
                                 (has_real_v<T> &&
                                  has_imag_v<T>);

template <typename T>
concept arbitrary_real_or_complex_type = arbitrary_real_type<T> ||
                                         arbitrary_complex_type<T>;

template <typename T>
concept arbitrary_numerical_type = arbitrary_real_or_complex_type<T> ||
                                   arbitrary_arithmetic_type<T>;

template <typename Derived, typename Base>
concept derived_from = std::is_base_of_v<Base, Derived> &&
                       std::is_convertible_v<const volatile Derived*, const volatile Base*>;

template <typename T>
concept forward_iterator = derived_from<typename std::iterator_traits<T>::iterator_category, std::forward_iterator_tag>;

template <typename T>
concept bidirectional_iterator = derived_from<typename std::iterator_traits<T>::iterator_category, std::bidirectional_iterator_tag>;

template <typename T>
concept random_access_iterator = derived_from<typename std::iterator_traits<T>::iterator_category, std::random_access_iterator_tag>;

template <typename I, typename T>
concept output_iterator = derived_from<typename std::iterator_traits<I>::iterator_category, std::input_iterator_tag> &&
                          derived_from<typename std::iterator_traits<T>::iterator_category, std::output_iterator_tag>;

template <typename T>
concept is_container = has_begin_v<T> &&
                       has_end_v<T>;

template <typename T>
concept random_access_container = is_container<T> &&
                                  boost::decimal::detail::concepts::random_access_iterator<typename T::iterator>;

template <typename T>
concept decimal_floating_point_type = boost::decimal::detail::is_decimal_floating_point_v<T>;

template <typename T>
concept fast_decimal_floating_point_type = boost::decimal::detail::is_fast_type_v<T>;

} // boost::decimal::detail::concepts

#define BOOST_DECIMAL_HAS_CONCEPTS 1

#define BOOST_DECIMAL_INTEGRAL boost::decimal::detail::concepts::integral
#define BOOST_DECIMAL_SIGNED_INTEGRAL boost::decimal::detail::concepts::signed_integral
#define BOOST_DECIMAL_UNSIGNED_INTEGRAL boost::decimal::detail::concepts::unsigned_integral
#define BOOST_DECIMAL_REAL boost::decimal::detail::concepts::real
#define BOOST_DECIMAL_COMPLEX boost::decimal::detail::concepts::complex
#define BOOST_DECIMAL_REAL_OR_COMPLEX boost::decimal::detail::concepts::real_or_complex
#define BOOST_DECIMAL_ARITHMETIC boost::decimal::detail::concepts::arithmetic
#define BOOST_DECIMAL_NUMERICAL boost::decimal::detail::concepts::numerical
#define BOOST_DECIMAL_SIGNED_ARITHMETIC boost::decimal::detail::concepts::signed_arithmetic
#define BOOST_DECIMAL_UNSIGNED_ARITHMETIC boost::decimal::detail::concepts::unsigned_arithmetic
#define BOOST_DECIMAL_ARBITRARY_UNSIGNED_ARITHMETIC boost::decimal::detail::concepts::arbitrary_unsigned_arithmetic_type
#define BOOST_DECIMAL_ARBITRARY_SIGNED_ARITHMETIC boost::decimal::detail::concepts::arbitrary_signed_arithmetic_type
#define BOOST_DECIMAL_ARBITRARY_ARITHMETIC boost::decimal::detail::concepts::arbitrary_arithmetic_type
#define BOOST_DECIMAL_ARBITRARY_UNSIGNED_INTEGER boost::decimal::detail::concepts::arbitrary_unsigned_integer_type
#define BOOST_DECIMAL_ARBITRARY_SIGNED_INTEGER boost::decimal::detail::concepts::arbitrary_signed_integer_type
#define BOOST_DECIMAL_ARBITRARY_INTEGER boost::decimal::detail::concepts::arbitrary_integer_type
#define BOOST_DECIMAL_ARBITRARY_REAL boost::decimal::detail::concepts::arbitrary_real_type
#define BOOST_DECIMAL_ARBITRARY_COMPLEX boost::decimal::detail::concepts::arbitrary_complex_type
#define BOOST_DECIMAL_ARBITRARY_REAL_OR_COMPLEX boost::decimal::detail::concepts::arbitrary_real_or_complex_type
#define BOOST_DECIMAL_ARBITRARY_NUMERICAL boost::decimal::detail::concepts::arbitrary_numerical_type
#define BOOST_DECIMAL_DECIMAL_FLOATING_TYPE boost::decimal::detail::concepts::decimal_floating_point_type
#define BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE boost::decimal::detail::concepts::fast_decimal_floating_point_type

#define BOOST_DECIMAL_CONTAINER boost::decimal::detail::concepts::is_container
#define BOOST_DECIMAL_RANDOM_ACCESS_CONTAINER boost::decimal::detail::concepts::random_access_container

#define BOOST_DECIMAL_FORWARD_ITER boost::decimal::detail::concepts::forward_iterator
#define BOOST_DECIMAL_BIDIRECTIONAL_ITER boost::decimal::detail::concepts::bidirectional_iterator
#define BOOST_DECIMAL_RANDOM_ACCESS_ITER boost::decimal::detail::concepts::random_access_iterator
#define BOOST_DECIMAL_OUTPUT_ITER(I, T) boost::decimal::detail::concepts::output_iterator<I, T>
#define BOOST_DECIMAL_REQUIRES_ITER(X) requires X

#define BOOST_DECIMAL_REQUIRES(X, T) -> T requires (X<T>)
#define BOOST_DECIMAL_REQUIRES_TWO(X1, T1, X2, T2) -> detail::promote_args_t<T1, T2> requires (X1<T1> && X2<T2>)
#define BOOST_DECIMAL_REQUIRES_TWO_RETURN(X1, T1, X2, T2, ReturnType) -> ReturnType requires (X1<T1> && X2<T2>)
#define BOOST_DECIMAL_REQUIRES_THREE(X1, T1, X2, T2, X3, T3) -> detail::promote_args_t<T1, T2, T3> requires (X1<T1> && X2<T2> && X3<T3>)
#define BOOST_DECIMAL_REQUIRES_RETURN(X, T, ReturnType) -> ReturnType requires (X<T>)

#ifdef BOOST_DECIMAL_EXEC_COMPATIBLE
#include <execution>

namespace boost::decimal::detail::concepts {

template <typename T>
concept execution_policy = std::is_execution_policy_v<std::remove_cvref_t<T>>;

} // Namespace boost::decimal::detail::concepts

#define BOOST_DECIMAL_EXECUTION_POLICY boost::decimal::detail::concepts::execution_policy

#endif // Has <execution>

#endif // Has <concepts>
#endif // C++20

// If concepts are unavailable replace them with typename for compatibility

#ifndef BOOST_DECIMAL_INTEGRAL
#  define BOOST_DECIMAL_INTEGRAL typename
#endif

#ifndef BOOST_DECIMAL_SIGNED_INTEGRAL
#  define BOOST_DECIMAL_SIGNED_INTEGRAL typename
#endif

#ifndef BOOST_DECIMAL_UNSIGNED_INTEGRAL
#  define BOOST_DECIMAL_UNSIGNED_INTEGRAL typename
#endif

#ifndef BOOST_DECIMAL_REAL
#  define BOOST_DECIMAL_REAL typename
#endif

#ifndef BOOST_DECIMAL_COMPLEX
#  define BOOST_DECIMAL_COMPLEX typename
#endif

#ifndef BOOST_DECIMAL_REAL_OR_COMPLEX
#  define BOOST_DECIMAL_REAL_OR_COMPLEX typename
#endif

#ifndef BOOST_DECIMAL_ARITHMETIC
#  define BOOST_DECIMAL_ARITHMETIC typename
#endif

#ifndef BOOST_DECIMAL_NUMERICAL
#  define BOOST_DECIMAL_NUMERICAL typename
#endif

#ifndef BOOST_DECIMAL_SIGNED_ARITHMETIC
#  define BOOST_DECIMAL_SIGNED_ARITHMETIC typename
#endif

#ifndef BOOST_DECIMAL_UNSIGNED_ARITHMETIC
#  define BOOST_DECIMAL_UNSIGNED_ARITHMETIC typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_UNSIGNED_ARITHMETIC
#  define BOOST_DECIMAL_ARBITRARY_UNSIGNED_ARITHMETIC typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_SIGNED_ARITHMETIC
#  define BOOST_DECIMAL_ARBITRARY_SIGNED_ARITHMETIC typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_ARITHMETIC
#  define BOOST_DECIMAL_ARBITRARY_ARITHMETIC typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_UNSIGNED_INTEGER
#  define BOOST_DECIMAL_ARBITRARY_UNSIGNED_INTEGER typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_SIGNED_INTEGER
#  define BOOST_DECIMAL_ARBITRARY_SIGNED_INTEGER typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_INTEGER
#  define BOOST_DECIMAL_ARBITRARY_INTEGER typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_REAL
#  define BOOST_DECIMAL_ARBITRARY_REAL typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_COMPLEX
#  define BOOST_DECIMAL_ARBITRARY_COMPLEX typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_REAL_OR_COMPLEX
#  define BOOST_DECIMAL_ARBITRARY_REAL_OR_COMPLEX typename
#endif

#ifndef BOOST_DECIMAL_ARBITRARY_NUMERICAL
#  define BOOST_DECIMAL_ARBITRARY_NUMERICAL typename
#endif

#ifndef BOOST_DECIMAL_POLICY
#  define BOOST_DECIMAL_POLICY typename
#endif

#ifndef BOOST_DECIMAL_FORWARD_ITER
#  define BOOST_DECIMAL_FORWARD_ITER typename
#endif

#ifndef BOOST_DECIMAL_BIDIRECTIONAL_ITER
#  define BOOST_DECIMAL_BIDIRECTIONAL_ITER typename
#endif

#ifndef BOOST_DECIMAL_RANDOM_ACCESS_ITER
#  define BOOST_DECIMAL_RANDOM_ACCESS_ITER typename
#endif

#ifndef BOOST_DECIMAL_DECIMAL_FLOATING_TYPE
#  define BOOST_DECIMAL_DECIMAL_FLOATING_TYPE typename
#endif

#ifndef BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE
#  define BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE typename
#endif

#ifndef BOOST_DECIMAL_OUTPUT_ITER
#  define BOOST_DECIMAL_OUTPUT_ITER(I, T)
#endif

#ifndef BOOST_DECIMAL_REQUIRES_ITER
#  define BOOST_DECIMAL_REQUIRES_ITER(X)
#endif

#ifndef BOOST_DECIMAL_CONTAINER
#  define BOOST_DECIMAL_CONTAINER typename
#endif

#ifndef BOOST_DECIMAL_RANDOM_ACCESS_CONTAINER
#  define BOOST_DECIMAL_RANDOM_ACCESS_CONTAINER typename
#endif

#ifndef BOOST_DECIMAL_EXECUTION_POLICY
#  define BOOST_DECIMAL_EXECUTION_POLICY typename
#endif

#ifndef BOOST_DECIMAL_REQUIRES
#  define BOOST_DECIMAL_REQUIRES(X, T) -> std::enable_if_t<X<T>, T>
#endif

#ifndef BOOST_DECIMAL_REQUIRES_TWO
#  define BOOST_DECIMAL_REQUIRES_TWO(X1, T1, X2, T2) -> std::enable_if_t<X1<T1> && X2<T2>, detail::promote_args_t<T1, T2>>
#endif

#ifndef BOOST_DECIMAL_REQUIRES_TWO_RETURN
#  define BOOST_DECIMAL_REQUIRES_TWO_RETURN(X1, T1, X2, T2, ReturnType) -> std::enable_if_t<X1<T1> && X2<T2>, ReturnType>
#endif

#ifndef BOOST_DECIMAL_REQUIRES_THREE
#  define BOOST_DECIMAL_REQUIRES_THREE(X1, T1, X2, T2, X3, T3) -> std::enable_if_t<X1<T1> && X2<T2> && X3<T3>, detail::promote_args_t<T1, T2, T3>>
#endif

#ifndef BOOST_DECIMAL_REQUIRES_RETURN
#  define BOOST_DECIMAL_REQUIRES_RETURN(X, T, ReturnType) -> std::enable_if_t<X<T>, ReturnType>
#endif

#endif //BOOST_DECIMAL_DETAIL_CONCEPTS
