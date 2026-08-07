// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_WRITE_PAYLOAD_HPP
#define BOOST_DECIMAL_DETAIL_WRITE_PAYLOAD_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/promotion.hpp>

namespace boost {
namespace decimal {

BOOST_DECIMAL_CUDA_CONSTEXPR auto from_bits(const std::uint32_t rhs) noexcept -> decimal32_t;
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_bits(const std::uint64_t rhs) noexcept -> decimal64_t;
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_bits(const int128::uint128_t rhs) noexcept -> decimal128_t;

namespace detail {

template <typename TargetDecimalType, bool is_snan>
BOOST_DECIMAL_CUDA_CONSTEXPR auto write_payload(typename TargetDecimalType::significand_type payload_value)
    BOOST_DECIMAL_REQUIRES(detail::is_fast_type_v, TargetDecimalType)
{
    using sig_type = typename TargetDecimalType::significand_type;

    constexpr TargetDecimalType nan_type {is_snan ? std::numeric_limits<TargetDecimalType>::signaling_NaN() :
                                                    std::numeric_limits<TargetDecimalType>::quiet_NaN()};

    constexpr std::uint32_t significand_field_bits {decimal_val_v<TargetDecimalType> < 64 ? 23U :
                                                    decimal_val_v<TargetDecimalType> < 128 ? 53U : 110U};

    constexpr sig_type max_payload_value {(static_cast<sig_type>(1) << significand_field_bits) - 1U};

    TargetDecimalType return_value {nan_type};
    if (payload_value < max_payload_value)
    {
        return_value.significand_ |= payload_value;
    }

    return return_value;
}

template <typename TargetDecimalType, bool is_snan>
BOOST_DECIMAL_CUDA_CONSTEXPR auto write_payload(typename TargetDecimalType::significand_type payload_value)
    BOOST_DECIMAL_REQUIRES(detail::is_ieee_type_v, TargetDecimalType)
{
    using sig_type = typename TargetDecimalType::significand_type;

    constexpr TargetDecimalType nan_type {is_snan ? std::numeric_limits<TargetDecimalType>::signaling_NaN() :
                                                    std::numeric_limits<TargetDecimalType>::quiet_NaN()};

    constexpr std::uint32_t significand_field_bits {decimal_val_v<TargetDecimalType> < 64 ? 23U :
                                                    decimal_val_v<TargetDecimalType> < 128 ? 53U : 110U};

    constexpr sig_type max_payload_value {(static_cast<sig_type>(1) << significand_field_bits) - 1U};

    auto return_value {nan_type.bits_};
    if (payload_value < max_payload_value)
    {
        return_value = payload_value | return_value;
    }

    return from_bits(return_value);
}

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_WRITE_PAYLOAD_HPP
