// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_APPLY_SIGN_HPP
#define BOOST_DECIMAL_DETAIL_APPLY_SIGN_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <limits>
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4146)
#endif

namespace boost { namespace decimal { namespace detail {

template <typename Integer, typename Unsigned_Integer = detail::make_unsigned_t<Integer>,
          std::enable_if_t<detail::is_signed_v<Integer>, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR auto apply_sign(Integer val) noexcept -> Unsigned_Integer
{
    return static_cast<Unsigned_Integer>(-(static_cast<Unsigned_Integer>(val)));
}

template <typename Unsigned_Integer, std::enable_if_t<!detail::is_signed_v<Unsigned_Integer>, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR auto apply_sign(Unsigned_Integer val) noexcept -> Unsigned_Integer
{
    return val;
}

template <typename Integer, typename Unsigned_Integer = detail::make_unsigned_t<Integer>,
          std::enable_if_t<detail::is_signed_v<Integer>, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR auto make_positive_unsigned(Integer val) noexcept -> Unsigned_Integer
{
    return static_cast<Unsigned_Integer>(val < static_cast<Integer>(static_cast<std::int8_t>(0)) ? apply_sign(val) : static_cast<Unsigned_Integer>(val));
}

template <typename Unsigned_Integer, std::enable_if_t<!detail::is_signed_v<Unsigned_Integer>, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR auto make_positive_unsigned(Unsigned_Integer val) noexcept -> Unsigned_Integer
{
    return val;
}

template <typename Integer, std::enable_if_t<detail::is_signed_v<Integer>, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR auto make_signed_value(Integer val, bool sign) noexcept -> Integer
{
    return sign ? -val : val;
}

template <typename Unsigned_Integer, typename Integer = detail::make_signed_t<Unsigned_Integer>,
          std::enable_if_t<!detail::is_signed_v<Unsigned_Integer>, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR auto make_signed_value(Unsigned_Integer val, bool sign) noexcept -> Integer
{
    const auto signed_val {static_cast<Integer>(val)};
    return sign ? -signed_val : signed_val;
}


} // namespace detail
} // namespace decimal
} // namespace boost

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#endif // BOOST_DECIMAL_DETAIL_APPLY_SIGN_HPP
