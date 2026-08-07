// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_PROMOTION_HPP
#define BOOST_DECIMAL_DETAIL_PROMOTION_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/components.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <limits>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace impl {
// Assign explicit decimal values because the decimal_fast32_t size could be greater than that
// of decimal64_t even though the precision is worse

template <typename T>
struct decimal_val
{
    static constexpr int value = 0;
};

template <>
struct decimal_val<decimal32_t>
{
    static constexpr int value = 32;
};

template <>
struct decimal_val<decimal32_t_components>
{
    static constexpr int value = 32;
};

// Assign a higher value to the fast type for consistency of promotion
// Side effect is the same calculation will be faster with the same precision
template <>
struct decimal_val<decimal_fast32_t>
{
    static constexpr int value = 33;
};

template <>
struct decimal_val<decimal64_t>
{
    static constexpr int value = 64;
};

template <>
struct decimal_val<decimal64_t_components>
{
    static constexpr int value = 64;
};

template <>
struct decimal_val<decimal_fast64_t>
{
    static constexpr int value = 65;
};

template <>
struct decimal_val<decimal128_t>
{
    static constexpr int value = 128;
};

template <>
struct decimal_val<decimal128_t_components>
{
    static constexpr int value = 128;
};

template <>
struct decimal_val<decimal_fast128_t>
{
    static constexpr int value = 129;
};

} // namespace impl

template <typename T>
constexpr int decimal_val_v = impl::decimal_val<T>::value;

namespace impl {

// Promotes a single argument to double if it is an integer type
template<typename T>
struct promote_arg
{
    using type = std::conditional_t<detail::is_integral_v<T>, double, T>;
};

template<typename T>
using promote_arg_t = typename promote_arg<T>::type;

// Promote two arguments picking in order:
// 1) the highest precision decimal type
// 2) any decimal type
// 3) or the highest precision type in that order
template<typename T1, typename T2>
struct promote_2_args
{
    using type = std::conditional_t<(is_decimal_floating_point_v<T1> && is_decimal_floating_point_v<T2>),
                 std::conditional_t<(decimal_val_v<T1> > decimal_val_v<T2>), T1, T2>,
                 std::conditional_t<is_decimal_floating_point_v<T1>, T1,
                        std::conditional_t<is_decimal_floating_point_v<T2>, T2,
                                std::conditional_t<(sizeof(promote_arg_t<T1>) > sizeof(promote_arg_t<T2>)),
                                                    promote_arg_t<T1>, promote_arg_t<T2>>>>>;
};

template<typename T1, typename T2>
using promote_2_args_t = typename promote_2_args<T1, T2>::type;

// Promote N args using the rules of promote_2_args
template <typename... Args>
struct promote_args;

template <typename T>
struct promote_args<T>
{
    using type = promote_arg_t<T>;
};

template <typename T, typename... Args>
struct promote_args<T, Args...>
{
    using type = promote_2_args_t<promote_arg_t<T>, typename promote_args<Args...>::type>;
};

} //namespace impl

template <typename... Args>
using promote_args_t = typename impl::promote_args<Args...>::type;

#if BOOST_DECIMAL_DEC_EVAL_METHOD == 0

template <typename T>
using evaluation_type_t = T;

#elif BOOST_DECIMAL_DEC_EVAL_METHOD == 1

template <typename T>
using evaluation_type_t = detail::promote_args_t<T, decimal64_t>;

#else

template <typename T>
using evaluation_type_t = detail::promote_args_t<T, decimal128_t>;

#endif

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_PROMOTION_HPP
