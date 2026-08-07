// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DPD_CONVERSION_HPP
#define BOOST_DECIMAL_DPD_CONVERSION_HPP

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/bid_conversion.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/uint128_t.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

// See Table 3.4
BOOST_DECIMAL_CUDA_CONSTEXPR auto encode_dpd(const std::uint8_t d1, const std::uint8_t d2, const std::uint8_t d3) -> std::uint16_t
{
    constexpr std::uint8_t b3_mask {0b0001};
    constexpr std::uint8_t b2_mask {0b0010};
    constexpr std::uint8_t b1_mask {0b0100};
    constexpr std::uint8_t b0_mask {0b1000};

    const std::uint8_t b1[4] = {
            static_cast<std::uint8_t>((d1 & b0_mask) >> 3U),
            static_cast<std::uint8_t>((d1 & b1_mask) >> 2U),
            static_cast<std::uint8_t>((d1 & b2_mask) >> 1U),
            static_cast<std::uint8_t>((d1 & b3_mask))
    };
    BOOST_DECIMAL_ASSERT(b1[0] <= 1U && b1[1] <= 1U && b1[2] <= 1U && b1[3] <= 1);

    const std::uint8_t b2[4] = {
            static_cast<std::uint8_t>((d2 & b0_mask) >> 3U),
            static_cast<std::uint8_t>((d2 & b1_mask) >> 2U),
            static_cast<std::uint8_t>((d2 & b2_mask) >> 1U),
            static_cast<std::uint8_t>((d2 & b3_mask))
    };
    BOOST_DECIMAL_ASSERT(b2[0] <= 1U && b2[1] <= 1U && b2[2] <= 1U && b2[3] <= 1);

    const std::uint8_t b3[4] = {
            static_cast<std::uint8_t>((d3 & b0_mask) >> 3U),
            static_cast<std::uint8_t>((d3 & b1_mask) >> 2U),
            static_cast<std::uint8_t>((d3 & b2_mask) >> 1U),
            static_cast<std::uint8_t>((d3 & b3_mask))
    };
    BOOST_DECIMAL_ASSERT(b3[0] <= 1U && b3[1] <= 1U && b3[2] <= 1U && b3[3] <= 1);

    std::uint8_t result_b[10] {};

    const auto table_val {(b1[0] << 2) + (b2[0] << 1) + b3[0]};
    BOOST_DECIMAL_ASSERT(table_val >= 0b000 && table_val <= 0b111);

    // Now that we have dissected the bits of d1, d2, and d3 we can use the lookup table from 3.4 to generate
    // all possible combinations
    switch (table_val)
    {
        case 0b000:
            // b0, b1, b2
            result_b[0] = b1[1];
            result_b[1] = b1[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = b2[1];
            result_b[4] = b2[2];
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(0);

            // b7, b8, b9
            result_b[7] = b3[1];
            result_b[8] = b3[2];
            result_b[9] = b3[3];
            break;

        case 0b001:
            // b0, b1, b2
            result_b[0] = b1[1];
            result_b[1] = b1[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = b2[1];
            result_b[4] = b2[2];
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[9] = b3[3];
            break;

        case 0b010:
            // b0, b1, b2
            result_b[0] = b1[1];
            result_b[1] = b1[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = b3[1];
            result_b[4] = b3[2];
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[7] = static_cast<std::uint8_t>(0);
            result_b[8] = static_cast<std::uint8_t>(1);
            result_b[9] = b3[3];
            break;

        case 0b011:
            // b0, b1, b2
            result_b[0] = b1[1];
            result_b[1] = b1[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = static_cast<std::uint8_t>(1);
            result_b[4] = static_cast<std::uint8_t>(0);
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[7] = static_cast<std::uint8_t>(1);
            result_b[8] = static_cast<std::uint8_t>(1);
            result_b[9] = b3[3];
            break;

        case 0b100:
            // b0, b1, b2
            result_b[0] = b3[1];
            result_b[1] = b3[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = b2[1];
            result_b[4] = b2[2];
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[7] = static_cast<std::uint8_t>(1);
            result_b[8] = static_cast<std::uint8_t>(0);
            result_b[9] = b3[3];
            break;

        case 0b101:
            // b0, b1, b2
            result_b[0] = b2[1];
            result_b[1] = b2[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = static_cast<std::uint8_t>(0);
            result_b[4] = static_cast<std::uint8_t>(1);
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[7] = static_cast<std::uint8_t>(1);
            result_b[8] = static_cast<std::uint8_t>(1);
            result_b[9] = b3[3];
            break;

        case 0b110:
            // b0, b1, b2
            result_b[0] = b3[1];
            result_b[1] = b3[2];
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = static_cast<std::uint8_t>(0);
            result_b[4] = static_cast<std::uint8_t>(0);
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[7] = static_cast<std::uint8_t>(1);
            result_b[8] = static_cast<std::uint8_t>(1);
            result_b[9] = b3[3];
            break;

        case 0b111:
            // b0, b1, b2
            result_b[0] = static_cast<std::uint8_t>(0);
            result_b[1] = static_cast<std::uint8_t>(0);
            result_b[2] = b1[3];

            // b3, b4, b5
            result_b[3] = static_cast<std::uint8_t>(1);
            result_b[4] = static_cast<std::uint8_t>(1);
            result_b[5] = b2[3];

            // b6
            result_b[6] = static_cast<std::uint8_t>(1);

            // b7, b8, b9
            result_b[7] = static_cast<std::uint8_t>(1);
            result_b[8] = static_cast<std::uint8_t>(1);
            result_b[9] = b3[3];
            break;
        // LCOV_EXCL_START
        default:
            BOOST_DECIMAL_UNREACHABLE;
        // LCOV_EXCL_STOP
    }

    // Now that we have the bit pattern of the result we need to write it into uint16_t and return the result
    std::uint16_t result {};

    for (std::uint16_t i {}; i < 10U; ++i)
    {
        result |= static_cast<std::uint16_t>(result_b[i] << (9 - i));
    }

    return result;
}

BOOST_DECIMAL_CUDA_CONSTEXPR auto decode_dpd(const std::uint32_t dpd_bits, std::uint8_t& d3, std::uint8_t& d2, std::uint8_t& d1) -> void
{
    // DPD decoding logic as per IEEE 754-2008
    std::uint8_t b[10] {};
    for (int i = 0; i < 10; ++i)
    {
        b[i] = static_cast<std::uint8_t>((dpd_bits >> (9 - i)) & 0b1);
    }

    // See table 3.3 for the flow of decoding
    // Values are b6, b7, b8, b3, b4
    // 0XXXX
    if (b[6] == 0U)
    {
        d1 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[2]);
        d2 = static_cast<std::uint8_t>((b[3] << 2U) + (b[4] << 1U) + b[5]);
        d3 = static_cast<std::uint8_t>((b[7] << 2U) + (b[8] << 1U) + b[9]);
    }
    // 100XX
    else if (b[6] == 1U && b[7] == 0U && b[8] == 0U)
    {
        d1 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[2]);
        d2 = static_cast<std::uint8_t>((b[3] << 2U) + (b[4] << 1U) + b[5]);
        d3 = static_cast<std::uint8_t>(8U + b[9]);
    }
    // 101XX
    else if (b[6] == 1U && b[7] == 0U && b[8] == 1U)
    {
        d1 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[2]);
        d2 = static_cast<std::uint8_t>(8U + b[5]);
        d3 = static_cast<std::uint8_t>((b[3] << 2U) + (b[4] << 1U) + b[9]);
    }
    // 110XX
    else if (b[6] == 1U && b[7] == 1U && b[8] == 0U)
    {
        d1 = static_cast<std::uint8_t>(8U + b[2]);
        d2 = static_cast<std::uint8_t>((b[3] << 2U) + (b[4] << 1U) + b[5]);
        d3 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[9]);
    }
    // 11100
    else if (b[6] == 1U && b[7] == 1U && b[8] == 1U && b[3] == 0U && b[4] == 0U)
    {
        d1 = static_cast<std::uint8_t>(8U + b[2]);
        d2 = static_cast<std::uint8_t>(8U + b[5]);
        d3 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[9]);
    }
    // 11101
    else if (b[6] == 1U && b[7] == 1U && b[8] == 1U && b[3] == 0U && b[4] == 1U)
    {
        d1 = static_cast<std::uint8_t>(8U + b[2]);
        d2 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[5]);
        d3 = static_cast<std::uint8_t>(8U + b[9]);
    }
    // 11110
    else if (b[6] == 1U && b[7] == 1U && b[8] == 1U && b[3] == 1U && b[4] == 0U)
    {
        d1 = static_cast<std::uint8_t>((b[0] << 2U) + (b[1] << 1U) + b[2]);
        d2 = static_cast<std::uint8_t>(8U + b[5]);
        d3 = static_cast<std::uint8_t>(8U + b[9]);
    }
    // 11111
    else if (b[6] == 1U && b[7] == 1U && b[8] == 1U && b[3] == 1U && b[4] == 1U)
    {
        d1 = static_cast<std::uint8_t>(8U + b[2]);
        d2 = static_cast<std::uint8_t>(8U + b[5]);
        d3 = static_cast<std::uint8_t>(8U + b[9]);
    }
    // LCOV_EXCL_START
    else
    {
        BOOST_DECIMAL_UNREACHABLE;
    }
    // LCOV_EXCL_STOP
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename DecimalType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd_d32(const DecimalType val) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, DecimalType, std::uint32_t)
{
    static_assert(std::is_same<DecimalType, decimal32_t>::value ||
                  std::is_same<DecimalType, decimal_fast32_t>::value, "The input must be a 32-bit decimal type");

    // In the non-finite cases, the encodings are the same
    // 3.5.2.a and 3.5.2.b
    if (!isfinite(val))
    {
        return to_bid(val);
    }

    const auto sign {val.isneg()};
    const auto exp {val.unbiased_exponent()};
    const auto significand {val.full_significand()};

    std::uint32_t dpd {};
    // Set the sign bit as applicable
    if (sign)
    {
        dpd |= detail::d32_sign_mask;
    }

    // Break the significand down into the 7 declets are needed
    std::uint8_t d[std::numeric_limits<DecimalType>::digits10] {};
    auto temp_sig {significand};
    for (int i = 6; i >= 0; --i)
    {
        d[i] = static_cast<std::uint8_t>(temp_sig % 10U);
        temp_sig /= 10U;
    }
    BOOST_DECIMAL_ASSERT(d[0] >= 0 && d[0] <= 9);
    BOOST_DECIMAL_ASSERT(temp_sig == 0);

    // We now need to capture what the leading two bits of the exponent are,
    // since they are stored in the combination field
    constexpr std::uint32_t leading_two_exp_bits_mask {0b11000000};
    const auto leading_two_bits {(exp & leading_two_exp_bits_mask) >> 6U};
    BOOST_DECIMAL_ASSERT(leading_two_bits >= 0 && leading_two_bits <= 2);
    constexpr std::uint32_t trailing_exp_bits_mask {0b00111111};
    const auto trailing_exp_bits {(exp & trailing_exp_bits_mask)};


    std::uint32_t combination_field_bits {};
    // Now based on what the value of d[0] and the leading bits of exp are we can set the value of the combination field
    // See 3.5.2.c.1
    // If d0 is 8 or 9 then we follow section i
    if (d[0] >= 8)
    {
        const auto d0_is_nine {d[0] == 9};
        switch (leading_two_bits)
        {
            case 0U:
                combination_field_bits = d0_is_nine ? 0b11001 : 0b11000;
                break;
            case 1U:
                combination_field_bits = d0_is_nine ? 0b11011 : 0b11010;
                break;
            case 2U:
                combination_field_bits = d0_is_nine ? 0b11101 : 0b11100;
                break;
            // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
            // LCOV_EXCL_STOP
        }
    }
    // If d0 is 0 to 7 then we follow section II
    else
    {
        // In here the value of d[0] = 4*G2 + 2*G3 + G4
        const auto d0_mask {static_cast<std::uint32_t>(d[0])};
        switch (leading_two_bits)
        {
            case 0U:
                // 00XXX
                combination_field_bits |= d0_mask;
                break;
            case 1U:
                // 01XXX
                combination_field_bits = 0b01000;
                combination_field_bits |= d0_mask;
                break;
            case 2U:
                // 10XXX
                combination_field_bits = 0b10000;
                combination_field_bits |= d0_mask;
                break;
            // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
            // LCOV_EXCL_STOP
        }
    }

    // The only thing we have left to compute now is the bit patterns of d[1] - d[6]
    const auto declet_1 {static_cast<std::uint32_t>(detail::encode_dpd(d[1], d[2], d[3]))};
    const auto declet_2 {static_cast<std::uint32_t>(detail::encode_dpd(d[4], d[5], d[6]))};

    // Now we can do final assembly of the number
    dpd |= (combination_field_bits << 26U);
    dpd |= (trailing_exp_bits << 20U);
    dpd |= (declet_1 << 10U);
    dpd |= declet_2;

    return dpd;
}

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal_fast32_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd_d32(const std::uint32_t dpd) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    static_assert(std::is_same<DecimalType, decimal32_t>::value || std::is_same<DecimalType, decimal_fast32_t>::value,
                  "Target decimal type must be 32-bits");

    const auto sign {(dpd & detail::d32_sign_mask) != 0};

    // First we check for non-finite values
    // Since they are in the same initial format as BID it's easy to check with our existing masks
    if ((dpd & detail::d32_inf_mask) == detail::d32_inf_mask)
    {
        if ((dpd & detail::d32_snan_mask) == detail::d32_snan_mask)
        {
            return sign ? -std::numeric_limits<DecimalType>::signaling_NaN() :
                           std::numeric_limits<DecimalType>::signaling_NaN();
        }
        else if ((dpd & detail::d32_nan_mask) == detail::d32_nan_mask)
        {
            return sign ? -std::numeric_limits<DecimalType>::quiet_NaN() :
                           std::numeric_limits<DecimalType>::quiet_NaN();
        }
        else
        {
            return sign ? -std::numeric_limits<DecimalType>::infinity() :
                           std::numeric_limits<DecimalType>::infinity();
        }
    }

    constexpr std::uint32_t dpd_d32_exponent_mask {UINT32_C(0x3F00000)};
    constexpr std::uint32_t dpd_d32_significand_mask {UINT32_C(0xFFFFF)};
    constexpr std::uint32_t dpd_d32_combination_mask {UINT32_C(0x7C000000)};

    // The bit lengths are the same as used in the standard bid format
    const auto combination_field_bits {(dpd & dpd_d32_combination_mask) >> 26U};
    const auto exponent_field_bits {(dpd & dpd_d32_exponent_mask) >> 20U};
    const auto significand_bits {(dpd & dpd_d32_significand_mask)};

    // Case 1: 3.5.2.c.1.i
    // Combination field bits are 110XX or 11110X
    std::uint32_t d0 {};
    std::uint32_t leading_biased_exp_bits {};
    if (combination_field_bits >= 0b11000)
    {
        // d0 = 8 + G4
        // Must be equal to 8 or 9
        d0 = 8U + (combination_field_bits & 0b00001);
        BOOST_DECIMAL_ASSERT(d0 == 8 || d0 == 9);

        // leading exp bits are 2*G2 + G3
        // Must be equal to 0, 1 or 2
        leading_biased_exp_bits = 2U * ((combination_field_bits & 0b00100) >> 2U) + ((combination_field_bits & 0b00010) >> 1U);
        BOOST_DECIMAL_ASSERT(leading_biased_exp_bits <= 2U);
    }
    // Case 2: 3.5.2.c.1.ii
    // Combination field bits are 0XXXX or 10XXX
    else
    {
        // d0 = 4 * G2 + 2 * G3 + G4
        // Must be in the range 0-7
        d0 = combination_field_bits & 0b00111;
        BOOST_DECIMAL_ASSERT(d0 <= 7);

        // Leading exp bits are 2 * G0 + G1
        // Must be equal to 0, 1 or 2
        leading_biased_exp_bits = (combination_field_bits & 0b11000) >> 3U;
        BOOST_DECIMAL_ASSERT(leading_biased_exp_bits <= 2U);
    }

    // Now that we have the bits we can calculate the exponents value
    const auto complete_exp {(leading_biased_exp_bits << 6U) + exponent_field_bits};
    const auto exp {static_cast<std::int32_t>(complete_exp) - detail::bias_v<DecimalType>};

    // We can now decode the remainder of the significand to recover the value
    std::uint8_t digits[7] {};
    digits[0] = static_cast<std::uint8_t>(d0);
    constexpr std::uint32_t seven_digits_mask {UINT32_C(0x3FF)};
    const auto significand_low {significand_bits & seven_digits_mask};
    detail::decode_dpd(significand_low, digits[6], digits[5], digits[4]);
    const auto significand_high {(significand_bits & UINT32_C(0xFFC00)) >> 10U};
    BOOST_DECIMAL_ASSERT(significand_high <= seven_digits_mask);
    detail::decode_dpd(significand_high, digits[3], digits[2], digits[1]);

    // Now we can assemble the significand
    std::uint32_t significand {};
    for (std::uint32_t i {}; i < 7U; ++i)
    {
        significand += digits[i] * detail::pow10(6 - i);
    }

    return DecimalType{significand, exp, sign};
}

BOOST_DECIMAL_EXPORT template <typename DecimalType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd_d64(const DecimalType val) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, DecimalType, std::uint64_t)
{
    static_assert(std::is_same<DecimalType, decimal64_t>::value ||
                  std::is_same<DecimalType, decimal_fast64_t>::value, "The input must be a 64-bit decimal type");

    // In the non-finite cases the encodings are the same
    // 3.5.2.a and 3.5.2.b
    if (!isfinite(val))
    {
        return to_bid(val);
    }

    const auto sign {val.isneg()};
    const auto exp {val.unbiased_exponent()};
    const auto significand {val.full_significand()};

    std::uint64_t dpd {};
    // Set the sign bit as applicable
    if (sign)
    {
        dpd |= detail::d64_sign_mask;
    }

    std::uint8_t d[std::numeric_limits<DecimalType>::digits10] {};
    auto temp_sig {significand};
    for (int i = 15; i >= 0; --i)
    {
        d[i] = static_cast<std::uint8_t>(temp_sig % 10U);
        temp_sig /= 10U;
    }
    BOOST_DECIMAL_ASSERT(d[0] >= 0 && d[0] <= 9);
    BOOST_DECIMAL_ASSERT(temp_sig == 0);

    constexpr std::uint64_t leading_two_exp_bits_mask {0b1100000000};
    const auto leading_two_bits {(exp & leading_two_exp_bits_mask) >> 8U};
    BOOST_DECIMAL_ASSERT(leading_two_bits >= 0 && leading_two_bits <= 2);
    constexpr std::uint64_t trailing_exp_bits_mask {0b0011111111};
    const auto trailing_exp_bits {(exp & trailing_exp_bits_mask)};

    std::uint64_t combination_field_bits {};

    // Now based on what the value of d[0] and the leading bits of exp are we can set the value of the combination field
    // See 3.5.2.c.1
    // If d0 is 8 or 9 then we follow section i
    if (d[0] >= 8)
    {
        const auto d0_is_nine {d[0] == 9};
        switch (leading_two_bits)
        {
            case 0U:
                combination_field_bits = d0_is_nine ? 0b11001 : 0b11000;
                break;
            case 1U:
                combination_field_bits = d0_is_nine ? 0b11011 : 0b11010;
                break;
            case 2U:
                combination_field_bits = d0_is_nine ? 0b11101 : 0b11100;
                break;
            // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
            // LCOV_EXCL_STOP
        }
    }
    // If d0 is 0 to 7 then we follow section II
    else
    {
        // In here the value of d[0] = 4*G2 + 2*G3 + G4
        const auto d0_mask {static_cast<std::uint64_t>(d[0])};
        switch (leading_two_bits)
        {
            case 0U:
                // 00XXX
                combination_field_bits |= d0_mask;
                break;
            case 1U:
                // 01XXX
                combination_field_bits = 0b01000;
                combination_field_bits |= d0_mask;
                break;
            case 2U:
                // 10XXX
                combination_field_bits = 0b10000;
                combination_field_bits |= d0_mask;
                break;
            // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
            // LCOV_EXCL_STOP
        }
    }

    // Write the now known combination field and trailing exp bits to the result
    dpd |= (combination_field_bits << 58U);
    dpd |= (trailing_exp_bits << 50U);

    // Now we need to encode all the declets
    // Once we have the declet right it into the result
    int offset {4};
    for (std::size_t i {1}; i < 15; i += 3U)
    {
        const auto declet {static_cast<std::uint64_t>(detail::encode_dpd(d[i], d[i + 1], d[i + 2]))};
        dpd |= (declet << (10 * offset));
        --offset;
    }

    return dpd;
}

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal_fast64_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd_d64(const std::uint64_t dpd) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    static_assert(std::is_same<DecimalType, decimal64_t>::value || std::is_same<DecimalType, decimal_fast64_t>::value,
                  "Target decimal type must be 64-bits");

    const auto sign {(dpd & detail::d64_sign_mask) != 0};

    // First we check for non-finite values
    // Since they are in the same initial format as BID it's easy to check with our existing masks
    if ((dpd & detail::d64_inf_mask) == detail::d64_inf_mask)
    {
        if ((dpd & detail::d64_snan_mask) == detail::d64_snan_mask)
        {
            return sign ? -std::numeric_limits<DecimalType>::signaling_NaN() :
                           std::numeric_limits<DecimalType>::signaling_NaN();
        }
        else if ((dpd & detail::d64_nan_mask) == detail::d64_nan_mask)
        {
            return sign ? -std::numeric_limits<DecimalType>::quiet_NaN() :
                           std::numeric_limits<DecimalType>::quiet_NaN();
        }
        else
        {
            return sign ? -std::numeric_limits<DecimalType>::infinity() :
                           std::numeric_limits<DecimalType>::infinity();
        }
    }

    // The bit lengths are the same as used in the standard bid format

    constexpr std::uint64_t dpd_d64_combination_field_mask {UINT64_C(0x7C00000000000000)};
    constexpr std::uint64_t dpd_d64_exponent_field_mask {UINT64_C(0x3FC000000000000)};
    constexpr std::uint64_t dpd_d64_significand_field_mask {UINT64_C(0x3FFFFFFFFFFFF)};

    const auto combination_field_bits {(dpd & dpd_d64_combination_field_mask) >> 58U};
    const auto exponent_field_bits {(dpd & dpd_d64_exponent_field_mask) >> 50U};
    auto significand_bits {(dpd & dpd_d64_significand_field_mask)};

    // Case 1: 3.5.2.c.1.i
    // Combination field bits are 110XX or 11110X
    std::uint64_t d0 {};
    std::uint64_t leading_biased_exp_bits {};
    if (combination_field_bits >= 0b11000)
    {
        // d0 = 8 + G4
        // Must be equal to 8 or 9
        d0 = 8U + (combination_field_bits & 0b00001);
        BOOST_DECIMAL_ASSERT(d0 == 8 || d0 == 9);

        // leading exp bits are 2*G2 + G3
        // Must be equal to 0, 1 or 2
        leading_biased_exp_bits = 2U * ((combination_field_bits & 0b00100) >> 2U) + ((combination_field_bits & 0b00010) >> 1U);
        BOOST_DECIMAL_ASSERT(leading_biased_exp_bits <= 2U);
    }
        // Case 2: 3.5.2.c.1.ii
        // Combination field bits are 0XXXX or 10XXX
    else
    {
        // d0 = 4 * G2 + 2 * G3 + G4
        // Must be in the range 0-7
        d0 = combination_field_bits & 0b00111;
        BOOST_DECIMAL_ASSERT(d0 <= 7);

        // Leading exp bits are 2 * G0 + G1
        // Must be equal to 0, 1 or 2
        leading_biased_exp_bits = (combination_field_bits & 0b11000) >> 3U;
        BOOST_DECIMAL_ASSERT(leading_biased_exp_bits <= 2U);
    }

    // Now that we have the bits we can calculate the exponents value
    const auto complete_exp {(leading_biased_exp_bits << 8U) + exponent_field_bits};
    const auto exp {static_cast<std::int32_t>(complete_exp) - detail::bias_v<DecimalType>};

    // We can now decode the remainder of the significand to recover the value
    std::uint8_t digits[16] {};
    digits[0] = static_cast<std::uint8_t>(d0);
    for (int i = 15; i > 0; i -= 3)
    {
        constexpr std::uint32_t declet_mask {UINT32_C(0x3FF)};
        const auto declet_bits {static_cast<std::uint32_t>(significand_bits & declet_mask)};
        significand_bits >>= 10U;
        detail::decode_dpd(declet_bits, digits[i], digits[i - 1], digits[i - 2]);
    }

    std::uint64_t significand {};
    for (std::uint64_t i {}; i < 16U; ++i)
    {
        significand += digits[i] * detail::pow10(15 - i);
    }

    return DecimalType{significand, exp, sign};
}

BOOST_DECIMAL_EXPORT template <typename DecimalType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd_d128(const DecimalType val) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, DecimalType, boost::int128::uint128_t)
{
    static_assert(std::is_same<DecimalType, decimal128_t>::value ||
                  std::is_same<DecimalType, decimal_fast128_t>::value, "The input must be a 128-bit decimal type");

    // In the non-finite cases the encodings are the same
    // 3.5.2.a and 3.5.2.b
    if (!isfinite(val))
    {
        return to_bid(val);
    }

    const auto sign {val.isneg()};
    const auto exp {val.unbiased_exponent()};
    const auto significand {val.full_significand()};

    int128::uint128_t dpd {};

    // Set the sign bit as applicable
    if (sign)
    {
        dpd.high |= detail::d128_sign_mask;
    }

    constexpr int num_digits {std::numeric_limits<DecimalType>::digits10};
    std::uint8_t d[static_cast<std::size_t>(num_digits)] {};
    auto temp_sig {significand};
    for (int i = num_digits - 1; i >= 0; --i)
    {
        d[i] = static_cast<std::uint8_t>(temp_sig % 10U);
        temp_sig /= 10U;
    }
    BOOST_DECIMAL_ASSERT(d[0] >= 0 && d[0] <= 9);
    BOOST_DECIMAL_ASSERT(temp_sig == 0U);

    constexpr std::uint64_t leading_two_exp_bits_mask {0b11000000000000};
    const auto leading_two_bits {(exp & leading_two_exp_bits_mask) >> 12U};
    constexpr std::uint64_t trailing_exp_bits_mask {0b00111111111111};
    const auto trailing_exp_bits {(exp & trailing_exp_bits_mask)};

    std::uint64_t combination_field_bits {};

    // Now based on what the value of d[0] and the leading bits of exp are we can set the value of the combination field
    // See 3.5.2.c.1
    // If d0 is 8 or 9 then we follow section i
    if (d[0] >= 8)
    {
        const auto d0_is_nine {d[0] == 9};
        switch (leading_two_bits)
        {
            // The decimal128_t case never uses the combination field like the other types,
            // since the significand always fits inside the allotted number of bits.
            // I don't believe this path will ever be taken, but it's correct
            case 0U:
                combination_field_bits = d0_is_nine ? 0b11001 : 0b11000;
                break;
            case 1U:
                combination_field_bits = d0_is_nine ? 0b11011 : 0b11010;
                break;
            case 2U:
                combination_field_bits = d0_is_nine ? 0b11101 : 0b11100;
                break;
                // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
                // LCOV_EXCL_STOP
        }
    }
        // If d0 is 0 to 7 then we follow section II
    else
    {
        // In here the value of d[0] = 4*G2 + 2*G3 + G4
        const auto d0_mask {static_cast<std::uint64_t>(d[0])};
        switch (leading_two_bits)
        {
            case 0U:
                // 00XXX
                combination_field_bits |= d0_mask;
                break;
            case 1U:
                // 01XXX
                combination_field_bits = 0b01000;
                combination_field_bits |= d0_mask;
                break;
            case 2U:
                // 10XXX
                combination_field_bits = 0b10000;
                combination_field_bits |= d0_mask;
                break;
                // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
                // LCOV_EXCL_STOP
        }
    }

    // Write the now know combination field and trailing exp bits to the result
    dpd.high |= (combination_field_bits << 58U);
    dpd.high |= (trailing_exp_bits << 46U);

    // Now we have to encode all 11 of the declets
    int offset {10};
    for (std::size_t i {1}; i < num_digits - 1; i += 3U)
    {
        const auto declet {static_cast<int128::uint128_t>(detail::encode_dpd(d[i], d[i + 1], d[i + 2]))};
        dpd |= int128::uint128_t(declet << (10 * offset));
        --offset;
    }

    return dpd;
}

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal_fast128_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd_d128(const int128::uint128_t dpd) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    static_assert(std::is_same<DecimalType, decimal128_t>::value || std::is_same<DecimalType, decimal_fast128_t>::value,
                  "Target decimal type must be 128-bits");

    const auto sign {(dpd.high & detail::d128_sign_mask) != 0};

    if ((dpd & detail::d128_inf_mask) == detail::d128_inf_mask)
    {
        if ((dpd & detail::d128_snan_mask) == detail::d128_snan_mask)
        {
            return sign ? -std::numeric_limits<DecimalType>::signaling_NaN() :
                           std::numeric_limits<DecimalType>::signaling_NaN();
        }
        else if ((dpd & detail::d128_nan_mask) == detail::d128_nan_mask)
        {
            return sign ? -std::numeric_limits<DecimalType>::quiet_NaN() :
                           std::numeric_limits<DecimalType>::quiet_NaN();
        }
        else
        {
            return sign ? -std::numeric_limits<DecimalType>::infinity() :
                           std::numeric_limits<DecimalType>::infinity();
        }
    }

    constexpr std::uint64_t d128_dpd_combination_field_mask_high_bits {UINT64_C(0x7C00000000000000)};
    constexpr std::uint64_t d128_dpd_exponent_mask_high_bits {UINT64_C(0x3FFC00000000000)};
    constexpr int128::uint128_t d128_dpd_significand_mask {UINT64_C(0x3FFFFFFFFFFF), UINT64_MAX};

    // The bit lengths are the same as used in the standard bid format
    const auto combination_field_bits {(dpd.high & d128_dpd_combination_field_mask_high_bits) >> 58U};
    const auto exponent_field_bits {(dpd.high & d128_dpd_exponent_mask_high_bits) >> 46U};
    auto significand_bits {(dpd & d128_dpd_significand_mask)};

    // Case 1: 3.5.2.c.1.i
    // Combination field bits are 110XX or 11110X
    std::uint64_t d0 {};
    std::uint64_t leading_biased_exp_bits {};
    if (combination_field_bits >= 0b11000)
    {
        // d0 = 8 + G4
        // Must be equal to 8 or 9
        d0 = 8U + (combination_field_bits & 0b00001);
        BOOST_DECIMAL_ASSERT(d0 == 8 || d0 == 9);

        // leading exp bits are 2*G2 + G3
        // Must be equal to 0, 1 or 2
        leading_biased_exp_bits = 2U * ((combination_field_bits & 0b00100) >> 2U) + ((combination_field_bits & 0b00010) >> 1U);
        BOOST_DECIMAL_ASSERT(leading_biased_exp_bits <= 2U);
    }
    // Case 2: 3.5.2.c.1.ii
    // Combination field bits are 0XXXX or 10XXX
    else
    {
        // d0 = 4 * G2 + 2 * G3 + G4
        // Must be in the range 0-7
        d0 = combination_field_bits & 0b00111;
        BOOST_DECIMAL_ASSERT(d0 <= 7);

        // Leading exp bits are 2 * G0 + G1
        // Must be equal to 0, 1 or 2
        leading_biased_exp_bits = (combination_field_bits & 0b11000) >> 3U;
        BOOST_DECIMAL_ASSERT(leading_biased_exp_bits <= 2U);
    }

    // Now that we have the bits we can calculate the exponents value
    const auto complete_exp {(leading_biased_exp_bits << 12U) + exponent_field_bits};
    const auto exp {static_cast<std::int32_t>(complete_exp) - detail::bias_v<DecimalType>};

    // We can now decode the remainder of the significand to recover the value
    constexpr auto num_digits {std::numeric_limits<DecimalType>::digits10};
    std::uint8_t digits[static_cast<std::size_t>(num_digits)] {};
    digits[0] = static_cast<std::uint8_t>(d0);
    for (int i = num_digits - 1; i > 0; i -= 3)
    {
        constexpr std::uint32_t declet_mask {UINT32_C(0x3FF)};
        const auto declet_bits {static_cast<std::uint32_t>(significand_bits & declet_mask)};
        significand_bits >>= 10U;
        detail::decode_dpd(declet_bits, digits[i], digits[i - 1], digits[i - 2]);
    }

    int128::uint128_t significand {};
    for (int i {}; i < num_digits; ++i)
    {
        significand += static_cast<int128::uint128_t>(digits[i]) * detail::pow10(static_cast<int128::uint128_t>((num_digits - 1) - i));
    }

    return DecimalType{significand, exp, sign};
}

BOOST_DECIMAL_EXPORT BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const decimal32_t val) noexcept -> std::uint32_t
{
    return to_dpd_d32(val);
}

BOOST_DECIMAL_EXPORT BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const decimal_fast32_t val) noexcept -> std::uint32_t
{
    return to_dpd_d32(val);
}

BOOST_DECIMAL_EXPORT BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const decimal64_t val) noexcept -> std::uint64_t
{
    return to_dpd_d64(val);
}

BOOST_DECIMAL_EXPORT BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const decimal_fast64_t val) noexcept -> std::uint64_t
{
    return to_dpd_d64(val);
}

BOOST_DECIMAL_EXPORT BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const decimal128_t val) noexcept -> int128::uint128_t
{
    return to_dpd_d128(val);
}

BOOST_DECIMAL_EXPORT BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const decimal_fast128_t& val) noexcept -> int128::uint128_t
{
    return to_dpd_d128(val);
}

BOOST_DECIMAL_EXPORT template <typename DecimalType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_dpd(const DecimalType val) noexcept
{
    static_assert(detail::is_decimal_floating_point_v<DecimalType>, "Must be a decimal floating point type.");
    return to_dpd(val);
}

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal32_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd(const std::uint32_t bits) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    return from_dpd_d32<DecimalType>(bits);
}

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal64_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd(const std::uint64_t bits) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    return from_dpd_d64<DecimalType>(bits);
}

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal128_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd(const int128::uint128_t bits) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    return from_dpd_d128<DecimalType>(bits);
}

#ifdef BOOST_DECIMAL_HAS_INT128

BOOST_DECIMAL_EXPORT template <typename DecimalType = decimal128_t>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_dpd(const detail::builtin_uint128_t bits) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    const int128::uint128_t converted_bits {bits};
    return from_dpd_d128<DecimalType>(converted_bits);
}

#endif // BOOST_DECIMAL_HAS_INT128

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DPD_CONVERSION_HPP
