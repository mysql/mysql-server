// Copyright 2023 - 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This is not a fully featured implementation of a 256-bit integer like int128::uint128_t is
// u256 only contains the minimum amount that we need to perform operations like decimal128_t mul

#ifndef BOOST_DECIMAL_DETAIL_U256_HPP
#define BOOST_DECIMAL_DETAIL_U256_HPP

#include <boost/decimal/detail/config.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <cmath>
#endif

namespace boost {
namespace decimal {
namespace detail {

// 32-bit MSVC 14.5 mis-codegens reads of bool members in over-aligned structs; use natural alignment there.
#if defined(_MSC_VER) && !defined(_M_X64) && !defined(_M_ARM64)
struct
#else
struct alignas(sizeof(std::uint64_t) * 4)
#endif
u256
{
    std::uint64_t bytes[4] {};

    // Constructors
    // Defaulted special members are left as plain constexpr.
    // NVCC implicitly treats them as__host__ __device__,
    // and annotating them triggers warning #20012-D in consumer code.
    constexpr u256() noexcept = default;
    constexpr u256(const u256& other) noexcept = default;
    constexpr u256(u256&& other) noexcept = default;
    constexpr u256& operator=(const u256& other) noexcept = default;
    constexpr u256& operator=(u256&& other) noexcept = default;

    BOOST_DECIMAL_CUDA_CONSTEXPR u256(std::uint64_t byte3, std::uint64_t byte2, std::uint64_t byte1, std::uint64_t byte0) noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256(const int128::uint128_t x) noexcept { bytes[0] = x.low; bytes[1] = x.high; }
    BOOST_DECIMAL_CUDA_CONSTEXPR u256(const std::uint64_t x) noexcept { bytes[0] = x; }

    explicit BOOST_DECIMAL_CUDA_CONSTEXPR operator std::uint64_t() const noexcept { return bytes[0]; }

    template<typename T = std::size_t>
    explicit BOOST_DECIMAL_CUDA_CONSTEXPR operator std::enable_if_t<
        !std::is_same<T, std::uint64_t>::value, T>() const noexcept
    {
        return static_cast<std::size_t>(bytes[0]);
    }

    // Conversion to/from int128::uint128_t
    BOOST_DECIMAL_CUDA_CONSTEXPR u256(const int128::uint128_t& high_, const int128::uint128_t& low_) noexcept;
    explicit BOOST_DECIMAL_CUDA_CONSTEXPR operator int128::uint128_t() const noexcept;

    BOOST_DECIMAL_CUDA_CONSTEXPR std::uint64_t operator[](std::size_t i) const noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR std::uint64_t& operator[](std::size_t i) noexcept;

    // Compound operators
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator<<=(int amount) noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator>>=(int amount) noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator|=(const u256& rhs) noexcept;

    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator*=(const u256& rhs) noexcept;

    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator/=(const u256& rhs) noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator/=(const int128::uint128_t& rhs) noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator/=(std::uint64_t rhs) noexcept;

    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator%=(const u256& rhs) noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator%=(std::uint64_t rhs) noexcept;

    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator++() noexcept;
    BOOST_DECIMAL_CUDA_CONSTEXPR u256& operator++(int) noexcept;
};

} // namespace detail
} // namespace decimal
} // namespace boost

namespace boost {
namespace decimal {
namespace detail {

BOOST_DECIMAL_CUDA_CONSTEXPR u256::u256(const std::uint64_t byte3, const std::uint64_t byte2, const std::uint64_t byte1, const std::uint64_t byte0) noexcept
{
    bytes[0] = byte0;
    bytes[1] = byte1;
    bytes[2] = byte2;
    bytes[3] = byte3;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256::u256(const int128::uint128_t& high_, const int128::uint128_t& low_) noexcept
{
    bytes[0] = low_.low;
    bytes[1] = low_.high;
    bytes[2] = high_.low;
    bytes[3] = high_.high;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256::operator int128::uint128_t() const noexcept
{
    return int128::uint128_t {bytes[1], bytes[0]};
}

BOOST_DECIMAL_CUDA_CONSTEXPR std::uint64_t u256::operator[](const std::size_t i) const noexcept
{
    BOOST_DECIMAL_ASSERT(i < 4);
    return bytes[i];
}

BOOST_DECIMAL_CUDA_CONSTEXPR std::uint64_t& u256::operator[](const std::size_t i) noexcept
{
    BOOST_DECIMAL_ASSERT(i < 4);
    return bytes[i];
}

//=====================================
// Equality Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR bool basic_equality_impl(const u256& lhs, const u256& rhs) noexcept
{
    return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
}

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR bool basic_inequality_impl(const u256& lhs, const u256& rhs) noexcept
{
    return lhs[0] != rhs[0] || lhs[1] != rhs[1] || lhs[2] != rhs[2] || lhs[3] != rhs[3];
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator==(const u256& lhs, const u256& rhs) noexcept
{
    // Start comp from low word since they will most likely be filled
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_equality_impl(lhs, rhs);
    }
    else
    {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.bytes));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.bytes));

        __m256i cmp = _mm256_cmpeq_epi64(a, b);

        const int mask = _mm256_movemask_epi8(cmp);

        return mask == -1;
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator==(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_equality_impl(lhs, rhs);
}

#endif

//=====================================
// Inequality Operators
//=====================================

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator!=(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_inequality_impl(lhs, rhs);
    }
    else
    {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.bytes));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.bytes));

        __m256i cmp = _mm256_cmpeq_epi64(a, b);

        const int mask = _mm256_movemask_epi8(cmp);

        return mask != -1;
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator!=(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_inequality_impl(lhs, rhs);
}

#endif // !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

//=====================================
// Less Than Operator
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR bool basic_lt_impl(const u256& lhs, const u256& rhs) noexcept
{
    if (lhs[3] != rhs[3])
    {
        return lhs[3] < rhs[3];
    }
    if (lhs[2] != rhs[2])
    {
        return lhs[2] < rhs[2];
    }
    if (lhs[1] != rhs[1])
    {
        return lhs[1] < rhs[1];
    }

    return lhs[0] < rhs[0];
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_lt_impl(lhs, rhs);
    }
    else
    {
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        __m256i eq_mask = _mm256_cmpeq_epi64(lhs_vec, rhs_vec);
        uint32_t eq_bits = _mm256_movemask_pd(_mm256_castsi256_pd(eq_mask));

        if ((eq_bits & 0x8) == 0)
        {
            return lhs[3] < rhs[3];
        }
        else if ((eq_bits & 0x4) == 0)
        {
            return lhs[2] < rhs[2];
        }
        else if ((eq_bits & 0x2) == 0)
        {
            return lhs[1] < rhs[1];
        }
        else if ((eq_bits & 0x1) == 0)
        {
            return lhs[0] < rhs[0];
        }

        return false;
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_lt_impl(lhs, rhs);
}

#endif

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<(const u256& lhs, const int128::uint128_t& rhs) noexcept
{
    return lhs[3] == 0U && lhs[2] == 0U && int128::uint128_t{lhs[1], lhs[0]} < rhs;
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<(const int128::uint128_t& lhs, const u256& rhs) noexcept
{
    return rhs[3] == 0U && rhs[2] == 0U && lhs < int128::uint128_t{rhs[1], rhs[0]};
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<(const u256& lhs, const std::uint64_t rhs) noexcept
{
    return lhs[3] == 0 && lhs[2] == 0 && lhs[1] == 0 && lhs[0] < rhs;
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<(const std::uint64_t lhs, const u256& rhs) noexcept
{
    return rhs[3] == 0 && rhs[2] == 0 && rhs[1] == 0 && lhs < rhs[0];
}

//=====================================
// Less Equal Operator
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR bool basic_le_impl(const u256& lhs, const u256& rhs) noexcept
{
    return !(rhs < lhs);
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<=(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_le_impl(lhs, rhs);
    }
    else
    {
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        // Compare all elements for equality
        __m256i eq_mask = _mm256_cmpeq_epi64(lhs_vec, rhs_vec);
        uint32_t eq_bits = _mm256_movemask_pd(_mm256_castsi256_pd(eq_mask));

        // Check each position from most significant to least significant
        if ((eq_bits & 0x8) == 0)
        {
            return (rhs[3] > lhs[3]);
        }
        else if ((eq_bits & 0x4) == 0)
        {
            return (rhs[2] > lhs[2]);
        }
        else if ((eq_bits & 0x2) == 0)
        {
            return (rhs[1] > lhs[1]);
        }
        else if ((eq_bits & 0x1) == 0)
        {
            return (rhs[0] > lhs[0]);
        }
        else
        {
            return true;
        }
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<=(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_le_impl(lhs, rhs);
}

#endif

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<=(const u256& lhs, const std::uint64_t rhs) noexcept
{
    return lhs[3] == 0 && lhs[2] == 0 && lhs[1] == 0 && lhs[0] <= rhs;
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator<=(const std::uint64_t lhs, const u256& rhs) noexcept
{
    return rhs[3] == 0 && rhs[2] == 0 && rhs[1] == 0 && lhs <= rhs[0];
}

//=====================================
// Greater Than Operator
//=====================================

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator>(const u256& lhs, const u256& rhs) noexcept
{
    return rhs < lhs;
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator>(const u256& lhs, const int128::uint128_t& rhs) noexcept
{
    return lhs[3] > 0U || lhs[2] > 0U || int128::uint128_t{lhs[1], lhs[0]} > rhs;
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator>(const u256& lhs, const std::uint64_t rhs) noexcept
{
    return lhs[3] != 0U || lhs[2] != 0U || lhs[1] != 0U || lhs[0] > rhs;
}

//=====================================
// Greater Equal Operator
//=====================================

BOOST_DECIMAL_CUDA_CONSTEXPR bool operator>=(const u256& lhs, const u256& rhs) noexcept
{
    return !(lhs < rhs);
}

//=====================================
// Left Shift Operators
//=====================================

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator<<(const u256& lhs, const int shift) noexcept
{
    u256 result {};

    if (shift >= 256 || shift < 0)
    {
        return result;
    }

    const auto word_shift {static_cast<std::size_t>(shift / 64)};
    const auto bit_shift {static_cast<std::size_t>(shift % 64)};

    // Only moving whole words
    if (bit_shift == 0)
    {
        for (std::size_t i {word_shift}; i < 4; ++i)
        {
            result[i] = lhs[i - word_shift];
        }

        return result;
    }

    if (word_shift < 4U)
    {
        result[word_shift] = lhs[0U] << bit_shift;
    }

    for (std::size_t i {word_shift + 1U}; i < 4U; ++i)
    {
        result[i] = (lhs[i - word_shift] << bit_shift) |
                    (lhs[i - word_shift - 1] >> (64 - bit_shift));
    }

    return result;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator<<=(const int amount) noexcept
{
    *this = *this << amount;
    return *this;
}

//=====================================
// Right Shift Operators
//=====================================

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator>>(const u256& lhs, const int shift) noexcept
{
    u256 result {};

    if (shift >= 256)
    {
        return result;
    }

    const auto word_shift {static_cast<std::size_t>(shift / 64)};
    const auto bit_shift {static_cast<std::size_t>(shift % 64)};

    // Only moving whole words
    if (bit_shift == 0)
    {
        for (std::size_t i {}; i < 4 - word_shift; ++i)
        {
            result[i] = lhs[i + word_shift];
        }

        return result;
    }

    // Handle partial shifts across word boundaries
    for (std::size_t i {}; i < 4 - word_shift - 1; ++i)
    {
        result[i] = (lhs[i + word_shift] >> bit_shift) |
                    (lhs[i + word_shift + 1] << (64 - bit_shift));
    }

    // Handle the last word that has a partial shift
    if (word_shift < 4)
    {
        result[3 - word_shift] = lhs[3] >> bit_shift;
    }

    return result;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator>>=(const int amount) noexcept
{
    *this = *this >> amount;
    return *this;
}

//=====================================
// Or Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 basic_or_impl(const u256& lhs, const u256& rhs) noexcept
{
    u256 result;

    result[3] = lhs[3] | rhs[3];
    result[2] = lhs[2] | rhs[2];
    result[1] = lhs[1] | rhs[1];
    result[0] = lhs[0] | rhs[0];

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator|(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_or_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        // Load 256 bits from each operand into AVX2 registers
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        // Perform the bitwise OR operation
        __m256i result_vec = _mm256_or_si256(lhs_vec, rhs_vec);

        // Store the result back to memory
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result), result_vec);

        return result;
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_HAS_ARM_INTRINSICS)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator|(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_or_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        uint64x2_t lhs_low = vld1q_u64(&lhs.bytes[0]);
        uint64x2_t lhs_high = vld1q_u64(&lhs.bytes[2]);

        uint64x2_t rhs_low = vld1q_u64(&rhs.bytes[0]);
        uint64x2_t rhs_high = vld1q_u64(&rhs.bytes[2]);

        // Perform bitwise OR in parallel
        uint64x2_t result_low = vorrq_u64(lhs_low, rhs_low);
        uint64x2_t result_high = vorrq_u64(lhs_high, rhs_high);

        // Store results back
        vst1q_u64(&result[0], result_low);
        vst1q_u64(&result[2], result_high);

        return result;
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator|(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_or_impl(lhs, rhs);
}

#endif

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator|=(const u256& rhs) noexcept
{
    *this = *this | rhs;
    return *this;
}

//=====================================
// And Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 basic_and_impl(const u256& lhs, const u256& rhs) noexcept
{
    u256 result;

    result[3] = lhs[3] & rhs[3];
    result[2] = lhs[2] & rhs[2];
    result[1] = lhs[1] & rhs[1];
    result[0] = lhs[0] & rhs[0];

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator&(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_and_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        // Load 256 bits from each operand into AVX2 registers
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        // Perform the bitwise AND operation
        __m256i result_vec = _mm256_and_si256(lhs_vec, rhs_vec);

        // Store the result back to memory
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result), result_vec);

        return result;
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_HAS_ARM_INTRINSICS)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator&(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_and_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        uint64x2_t lhs_low = vld1q_u64(&lhs.bytes[0]);
        uint64x2_t lhs_high = vld1q_u64(&lhs.bytes[2]);

        uint64x2_t rhs_low = vld1q_u64(&rhs.bytes[0]);
        uint64x2_t rhs_high = vld1q_u64(&rhs.bytes[2]);

        // Perform bitwise AND in parallel
        uint64x2_t result_low = vandq_u64(lhs_low, rhs_low);
        uint64x2_t result_high = vandq_u64(lhs_high, rhs_high);

        // Store results back
        vst1q_u64(&result[0], result_low);
        vst1q_u64(&result[2], result_high);

        return result;
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator&(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_and_impl(lhs, rhs);
}

#endif

//=====================================
// Addition Operators
//=====================================

namespace impl {

BOOST_DECIMAL_CUDA_CONSTEXPR u256 basic_add_impl(const u256& lhs, const u256& rhs) noexcept
{
    u256 result;
    std::uint64_t carry {};

    auto sum {lhs[0] + rhs[0]};
    result[0] = sum;
    carry = (sum < lhs[0]) ? 1 : 0;

    sum = lhs[1] + rhs[1] + carry;
    result[1] = sum;
    carry = (sum < lhs[1] || (sum == lhs[1] && carry)) ? 1 : 0;

    sum = lhs[2] + rhs[2] + carry;
    result[2] = sum;
    carry = (sum < lhs[2] || (sum == lhs[2] && carry)) ? 1 : 0;

    result[3] = lhs[3] + rhs[3] + carry;

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_ADD_CARRY)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator+(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_add_impl(lhs, rhs);
    }
    else
    {
        unsigned long long result[4] {};
        unsigned char carry {};
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[0], rhs[0], &result[0]);
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[1], rhs[1], &result[1]);
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[2], rhs[2], &result[2]);
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[3], rhs[3], &result[3]);

        return {result[3], result[2], result[1], result[0]};
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(BOOST_DECIMAL_ADD_CARRY)

namespace impl {

inline bool add_carry_u64(const bool carry_in, const std::uint64_t a, const std::uint64_t b, unsigned long long* sum) noexcept
{
    unsigned long long res;
    auto c = __builtin_uaddll_overflow(a, b, &res);
    c |= __builtin_uaddll_overflow(res, static_cast<unsigned long long>(carry_in), &res);
    *sum = res;
    return c;
}

} // namespace impl

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator+(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_add_impl(lhs, rhs);
    }
    else
    {
        unsigned long long result[4] {};
        bool carry {};
        carry = impl::add_carry_u64(carry, lhs[0], rhs[0], &result[0]);
        carry = impl::add_carry_u64(carry, lhs[1], rhs[1], &result[1]);
        carry = impl::add_carry_u64(carry, lhs[2], rhs[2], &result[2]);
        carry = impl::add_carry_u64(carry, lhs[3], rhs[3], &result[3]);

        return {result[3], result[2], result[1], result[0]};
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator+(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_add_impl(lhs, rhs);
}

#endif

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator++() noexcept
{
    *this = *this + static_cast<std::uint64_t>(1);
    return *this;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator++(int) noexcept
{
    *this = *this + static_cast<std::uint64_t>(1);
    return *this;
}


//=====================================
// Multiplication Operators
//=====================================

namespace impl {

#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

template <std::size_t word_size>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 from_words(const std::uint32_t (&words)[word_size]) noexcept
{
    static_assert(word_size >= 8, "Not enough words to convert to u256");

    u256 result {};

    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(words))
    {
        std::memcpy(&result, words, sizeof(result));
    }
    else
    #endif
    {
        result[0] = static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32);
        result[1] = static_cast<std::uint64_t>(words[2]) | (static_cast<std::uint64_t>(words[3]) << 32);
        result[2] = static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32);
        result[3] = static_cast<std::uint64_t>(words[6]) | (static_cast<std::uint64_t>(words[7]) << 32);
    }

    return result;
}

#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif

template <std::size_t u_size, std::size_t v_size>
BOOST_DECIMAL_CUDA_CONSTEXPR u256 knuth_mulitply(const std::uint32_t (&u)[u_size],
                              const std::uint32_t (&v)[v_size]) noexcept
{
    std::uint32_t w[u_size + v_size] {};

    // M.1
    for (std::size_t j {}; j < v_size; ++j)
    {
        // M.2
        if (v[j] == 0)
        {
            w[j + u_size] = 0u;
            continue;
        }

        // M.3
        std::uint64_t t {};
        for (std::size_t i {}; i < u_size; ++i)
        {
            // M.4
            t += static_cast<std::uint64_t>(u[i]) * v[j] + w[i + j];
            w[i + j] = static_cast<std::uint32_t>(t);
            t >>= 32u;
        }

        // M.5
        w[j + u_size] = static_cast<std::uint32_t>(t);
    }

    return from_words(w);
}

BOOST_DECIMAL_CUDA_CONSTEXPR void to_words(const u256& x, std::uint32_t (&words)[8]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(x));
    }
    else
    #endif
    {

        words[0] = static_cast<std::uint32_t>(x[0] & UINT32_MAX);
        words[1] = static_cast<std::uint32_t>(x[0] >> 32U);
        words[2] = static_cast<std::uint32_t>(x[1] & UINT32_MAX);
        words[3] = static_cast<std::uint32_t>(x[1] >> 32U);
        words[4] = static_cast<std::uint32_t>(x[2] & UINT32_MAX);
        words[5] = static_cast<std::uint32_t>(x[2] >> 32U);
        words[6] = static_cast<std::uint32_t>(x[3] & UINT32_MAX);
        words[7] = static_cast<std::uint32_t>(x[3] >> 32U);
    }
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 default_mul(const u256& lhs, const UnsignedInteger& rhs) noexcept
{
    using boost::decimal::detail::impl::to_words;
    using boost::int128::detail::to_words;

    constexpr std::size_t rhs_words_needed {sizeof(UnsignedInteger) / sizeof(std::uint32_t)};

    std::uint32_t lhs_words[8] {};
    std::uint32_t rhs_words[rhs_words_needed] {};

    to_words(lhs, lhs_words);
    to_words(rhs, rhs_words);

    return knuth_mulitply(lhs_words, rhs_words);
}

} // namespace impl

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator*(const u256& lhs, const u256& rhs) noexcept
{
    return impl::default_mul(lhs, rhs);
}

template <typename UnsignedInteger>
BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator*(const u256& lhs, const UnsignedInteger rhs) noexcept
{
    return impl::default_mul(lhs, rhs);
}

template <typename UnsignedInteger>
BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator*(const UnsignedInteger lhs, const u256& rhs) noexcept
{
    return impl::default_mul(rhs, lhs);
}
BOOST_DECIMAL_CUDA_CONSTEXPR u256 mul128_by_64(const int128::uint128_t& a, const std::uint64_t b) noexcept;

BOOST_DECIMAL_CUDA_CONSTEXPR u256 umul256(const int128::uint128_t& a, const int128::uint128_t& b) noexcept
{
    if (BOOST_DECIMAL_UNLIKELY(b.high == 0U))
    {
        return mul128_by_64(a, b.low);
    }
    if (BOOST_DECIMAL_UNLIKELY(a.high == 0U))
    {
        return mul128_by_64(b, a.low);
    }

    u256 result{};

    const int128::uint128_t a_low {a.low};
    const int128::uint128_t a_high {a.high};
    const auto b_low {b.low};
    const auto b_high {b.high};

    const auto p0 = a_low * b_low;
    const auto p1 = a_low * b_high;
    const auto p2 = a_high * b_low;
    const auto p3 = a_high * b_high;

    const auto p1_plus_p2 = p1 + p2;
    const std::uint64_t carry_p1p2 = (p1_plus_p2 < p1) ? UINT64_C(1) : UINT64_C(0);

    const auto middle = p1_plus_p2 + p0.high;
    const std::uint64_t carry_mid = (middle < p1_plus_p2) ? UINT64_C(1) : UINT64_C(0);

    result.bytes[0] = p0.low;
    result.bytes[1] = middle.low;

    auto high_sum = p3 + int128::uint128_t{0, middle.high};
    high_sum += int128::uint128_t{carry_p1p2 + carry_mid, 0};

    result.bytes[2] = high_sum.low;
    result.bytes[3] = high_sum.high;

    return result;
}

// Returns the high 256 bits of a u256 * u256 -> u512 product
BOOST_DECIMAL_CUDA_CONSTEXPR u256 umul512_hi(const u256& a, const u256& b) noexcept
{
    // Decompose each operand into two uint128 halves.
    const int128::uint128_t a_lo {a.bytes[1], a.bytes[0]};
    const int128::uint128_t a_hi {a.bytes[3], a.bytes[2]};
    const int128::uint128_t b_lo {b.bytes[1], b.bytes[0]};
    const int128::uint128_t b_hi {b.bytes[3], b.bytes[2]};

    // Four uint128 * uint128 -> u256 partial products.
    const u256 p_ll {umul256(a_lo, b_lo)};
    const u256 p_lh {umul256(a_lo, b_hi)};
    const u256 p_hl {umul256(a_hi, b_lo)};
    const u256 p_hh {umul256(a_hi, b_hi)};

    const int128::uint128_t p_ll_hi {p_ll.bytes[3], p_ll.bytes[2]};
    const int128::uint128_t p_lh_lo {p_lh.bytes[1], p_lh.bytes[0]};
    const int128::uint128_t p_lh_hi {p_lh.bytes[3], p_lh.bytes[2]};
    const int128::uint128_t p_hl_lo {p_hl.bytes[1], p_hl.bytes[0]};
    const int128::uint128_t p_hl_hi {p_hl.bytes[3], p_hl.bytes[2]};
    const int128::uint128_t p_hh_lo {p_hh.bytes[1], p_hh.bytes[0]};
    const int128::uint128_t p_hh_hi {p_hh.bytes[3], p_hh.bytes[2]};

    int128::uint128_t w1 {p_ll_hi};
    w1 += p_lh_lo;
    std::uint64_t carry_w1 {(w1 < p_lh_lo) ? UINT64_C(1) : UINT64_C(0)};
    w1 += p_hl_lo;
    carry_w1 += (w1 < p_hl_lo) ? UINT64_C(1) : UINT64_C(0);

    int128::uint128_t w2 {p_lh_hi};
    w2 += p_hl_hi;
    std::uint64_t carry_w2 {(w2 < p_hl_hi) ? UINT64_C(1) : UINT64_C(0)};
    w2 += p_hh_lo;
    carry_w2 += (w2 < p_hh_lo) ? UINT64_C(1) : UINT64_C(0);
    const int128::uint128_t w2_before_carry {w2};
    w2 += int128::uint128_t{0, carry_w1};
    carry_w2 += (w2 < w2_before_carry) ? UINT64_C(1) : UINT64_C(0);

    const int128::uint128_t w3 {p_hh_hi + int128::uint128_t{0, carry_w2}};

    return u256{w3, w2};
}

// 128x64 -> 256 multiplication (SoftFloat-style lightweight primitive)
// Used when rhs is 64-bit (e.g. r_scaled from approx_recip_sqrt64)
BOOST_DECIMAL_CUDA_CONSTEXPR u256 mul128_by_64(const int128::uint128_t& a, const std::uint64_t b) noexcept
{
    const int128::uint128_t p0 = int128::uint128_t{a.low} * b;   // 64x64 -> 128
    const int128::uint128_t p1 = int128::uint128_t{a.high} * b;  // 64x64 -> 128
    const auto mid = p1.low + p0.high;
    const std::uint64_t carry1 = (mid < p0.high) ? 1U : 0U;
    const auto hi = p1.high + carry1;
    const std::uint64_t carry2 = (hi < carry1) ? 1U : 0U;

    u256 result{};
    result.bytes[0] = p0.low;
    result.bytes[1] = mid;
    result.bytes[2] = hi;
    result.bytes[3] = carry2;
    return result;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator*=(const u256& rhs) noexcept
{
    *this = *this * rhs;
    return *this;
}

//=====================================
// Division Operators
//=====================================

namespace impl {

BOOST_DECIMAL_CUDA_CONSTEXPR std::size_t div_to_words(const u256& x, std::uint32_t (&words)[8]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(x));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x[0] & UINT32_MAX);
        words[1] = static_cast<std::uint32_t>(x[0] >> 32U);
        words[2] = static_cast<std::uint32_t>(x[1] & UINT32_MAX);
        words[3] = static_cast<std::uint32_t>(x[1] >> 32U);
        words[4] = static_cast<std::uint32_t>(x[2] & UINT32_MAX);
        words[5] = static_cast<std::uint32_t>(x[2] >> 32U);
        words[6] = static_cast<std::uint32_t>(x[3] & UINT32_MAX);
        words[7] = static_cast<std::uint32_t>(x[3] >> 32U);
    }

    std::size_t word_count {8};
    while (words[word_count - 1U] == 0U)
    {
        --word_count;
    }

    return word_count;
}

BOOST_DECIMAL_CUDA_CONSTEXPR std::size_t div_to_words(const boost::int128::uint128_t& x, std::uint32_t (&words)[8]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(boost::int128::uint128_t));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x.low & UINT32_MAX);                              // LCOV_EXCL_LINE
        words[1] = static_cast<std::uint32_t>(x.low >> 32);                                     // LCOV_EXCL_LINE
        words[2] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) & UINT32_MAX); // LCOV_EXCL_LINE
        words[3] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) >> 32);        // LCOV_EXCL_LINE
    }

    BOOST_DECIMAL_DETAIL_INT128_ASSERT_MSG(x != 0U, "Division by 0"); // LCOV_EXCL_LINE : False Negative

    std::size_t word_count {4};
    while (words[word_count - 1U] == 0U)
    {
        word_count--;
    }

    return word_count;
}

BOOST_DECIMAL_CUDA_CONSTEXPR std::size_t div_to_words(const std::uint64_t x, std::uint32_t (&words)[2]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(std::uint64_t));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x & UINT32_MAX);
        words[1] = static_cast<std::uint32_t>(x >> 32);
    }

    BOOST_DECIMAL_DETAIL_INT128_ASSERT_MSG(x != 0U, "Division by 0");

    std::size_t word_count {2};
    while (words[word_count - 1U] == 0U)
    {
        word_count--;
    }

    return word_count;
}

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 default_div(const u256& lhs, const std::uint64_t rhs) noexcept
{
    u256 quotient;

    int128::uint128_t current {lhs[3]};
    quotient[3] = static_cast<std::uint64_t>(current / rhs);
    auto remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[2];
    quotient[2] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[1];
    quotient[1] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[0];
    quotient[0] = static_cast<std::uint64_t>(current / rhs);

    return quotient;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128

// MG 3/2 fast path for u256 divided by a uint128 divisor.
//
// Normalizes the divisor so its MSB is set, applies the same shift to the
// dividend (which never overflows past 256 bits for the d128 division flow
// -- the top uint64 of `big_sig = lhs.sig * 10^34` is at most ~34 bits, and
// the shift amount is at most ~17), runs two chained 3/2 inner divides, and
// un-normalizes the remainder.
//
// Returns true when the fast path applied (`q` and `r` populated), false
// when the divisor's high half is zero (caller should use the single-limb
// path) or when the dividend's top 128 bits are not strictly less than the
// normalized divisor (the 3/2 algorithm's precondition for a single-limb
// quotient).
BOOST_DECIMAL_FORCE_INLINE bool mg32_u256_by_u128(
    u256 u, int128::uint128_t d,
    int128::uint128_t& q, int128::uint128_t& r) noexcept
{
    if (d.high == 0U)
    {
        return false; // single-limb divisor: caller takes the fast 4x uint128/uint64 path
    }

    // Normalize d so its MSB is set.
    const int shift_amount {int128::detail::countl_zero(d.high)};
    if (shift_amount > 0)
    {
        d <<= shift_amount;
        const int complement {64 - shift_amount};
        // Shift u left by `shift_amount` across all 4 limbs. If the top limb
        // would overflow past 256 bits, bail out; the caller will use the
        // existing Knuth-D path.
        if ((u.bytes[3] >> complement) != 0U)
        {
            return false;
        }
        u.bytes[3] = (u.bytes[3] << shift_amount) | (u.bytes[2] >> complement);
        u.bytes[2] = (u.bytes[2] << shift_amount) | (u.bytes[1] >> complement);
        u.bytes[1] = (u.bytes[1] << shift_amount) | (u.bytes[0] >> complement);
        u.bytes[0] = (u.bytes[0] << shift_amount);
    }

    // Precondition: top 128 bits of u must be strictly less than d (the 3/2
    // algorithm computes a one-limb quotient and breaks otherwise). When this
    // doesn't hold the result would need a 3-limb quotient and we fall back.
    const int128::uint128_t u_top {u.bytes[3], u.bytes[2]};
    if (!(u_top < d))
    {
        return false;
    }

    const std::uint64_t v {int128::detail::impl::mg32_reciprocal_2by1(d)};

    int128::uint128_t r1{};
    const std::uint64_t q_high {int128::detail::impl::mg32_div_3by2(u_top, u.bytes[1], d, v, r1)};

    int128::uint128_t r2{};
    const std::uint64_t q_low {int128::detail::impl::mg32_div_3by2(r1, u.bytes[0], d, v, r2)};

    q = int128::uint128_t{q_high, q_low};
    // Un-normalize the remainder.
    r = (shift_amount > 0) ? (r2 >> shift_amount) : r2;
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128

// True when the divisor fits in 128 bits, i.e. the MG u256-by-u128 fast path
// is applicable. A u256 divisor only qualifies when its upper 128 bits are
// clear; static_cast<uint128_t> would otherwise truncate it silently and the
// fast path would divide by the wrong value. Narrower integer types always
// qualify.
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR bool divisor_fits_u128(const u256& rhs) noexcept
{
    return rhs.bytes[2] == UINT64_C(0) && rhs.bytes[3] == UINT64_C(0);
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR bool divisor_fits_u128(const UnsignedInteger&) noexcept
{
    return true;
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 default_div(const u256& lhs, const UnsignedInteger& rhs) noexcept
{
    if (rhs <= UINT64_MAX)
    {
        return default_div(lhs, static_cast<std::uint64_t>(rhs));
    }
    else if (lhs < rhs)
    {
        return u256{};
    }

    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)
    // MG 3/2 fast path. Replaces ~3 hardware divides in the 32-bit Knuth-D
    // outer loop with one reciprocal-compute plus two cheap 3/2 inner steps.
    // Bails out (returns false) for inputs that don't fit the algorithm's
    // single-limb-quotient shape; those fall through to Knuth-D below.
    // Runtime-only: the MG helpers call hardware intrinsics that are not
    // usable in a constant expression, so this path is gated behind
    // IS_CONSTANT_EVALUATED (and compiled out when detection is unavailable).
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs) && divisor_fits_u128(rhs))
    {
        const int128::uint128_t rhs_u128 {static_cast<int128::uint128_t>(rhs)};
        int128::uint128_t mg_q{};
        int128::uint128_t mg_r{};
        if (mg32_u256_by_u128(lhs, rhs_u128, mg_q, mg_r))
        {
            u256 result{};
            result.bytes[0] = mg_q.low;
            result.bytes[1] = mg_q.high;
            return result;
        }
    }
    #endif

    std::uint32_t u[8] {};
    std::uint32_t v[8] {};
    std::uint32_t q[8] {};

    const auto m {div_to_words(lhs, u)};
    const auto n {div_to_words(rhs, v)};

    BOOST_DECIMAL_DETAIL_INT128_ASSERT(m >= n);

    int128::detail::impl::knuth_divide<false>(u, m, v, n, q);

    return from_words(q);
}

struct u256_divmod_result
{
    u256 quotient;
    u256 remainder;
};

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto div_mod(const u256& lhs, const UnsignedInteger& rhs) noexcept -> u256_divmod_result
{
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)
    // MG 3/2 fast path for u256/uint128. See default_div for rationale.
    // Single-limb divisors fall through to the 32-bit Knuth-D path below,
    // which has a dedicated n==1 short-circuit.
    // Runtime-only: gated behind IS_CONSTANT_EVALUATED because the MG helpers
    // call hardware intrinsics that are not usable in a constant expression.
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs) && divisor_fits_u128(rhs))
    {
        const int128::uint128_t rhs_u128 {static_cast<int128::uint128_t>(rhs)};
        if (rhs_u128.high != 0U)
        {
            int128::uint128_t mg_q{};
            int128::uint128_t mg_r{};
            if (mg32_u256_by_u128(lhs, rhs_u128, mg_q, mg_r))
            {
                u256_divmod_result out{};
                out.quotient.bytes[0] = mg_q.low;
                out.quotient.bytes[1] = mg_q.high;
                out.remainder.bytes[0] = mg_r.low;
                out.remainder.bytes[1] = mg_r.high;
                return out;
            }
        }
    }
    #endif

    std::uint32_t u[8] {};
    std::uint32_t v[8] {};
    std::uint32_t q[8] {};

    const auto m {div_to_words(lhs, u)};
    const auto n {div_to_words(rhs, v)};

    BOOST_DECIMAL_DETAIL_INT128_ASSERT(m >= n);

    // Simplify handling of single word division
    // We run into this case with dividing by powers of 10 while rounding u256
    if (n == 1U)
    {
        std::uint64_t remainder {};

        for (std::size_t j = m; j-- > 0;)
        {
            const auto dividend {(remainder << 32) | u[j]};
            q[j] = static_cast<std::uint32_t>(dividend / v[0]);
            remainder = dividend % v[0];
        }

        u[0] = static_cast<std::uint32_t>(remainder);
    }
    else
    {
        int128::detail::impl::knuth_divide<true>(u, m, v, n, q);
    }

     return {from_words(q), from_words(u)};
}

} // namespace impl

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator/(const u256& lhs, const u256& rhs) noexcept
{
    return impl::default_div(lhs, rhs);
}

template <typename UnsignedInteger>
BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator/(const u256& lhs, const UnsignedInteger rhs) noexcept
{
    return impl::default_div(lhs, rhs);
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator/=(const u256& rhs) noexcept
{
    *this = *this / rhs;
    return *this;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator/=(const int128::uint128_t& rhs) noexcept
{
    *this = *this / rhs;
    return *this;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator/=(const std::uint64_t rhs) noexcept
{
    *this = impl::default_div(*this, rhs);
    return *this;
}

//=====================================
// Modulo Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 default_mod(const u256& lhs, const std::uint64_t rhs) noexcept
{
    u256 quotient;

    int128::uint128_t current {lhs[3]};
    quotient[3] = static_cast<std::uint64_t>(current / rhs);
    auto remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[2];
    quotient[2] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[1];
    quotient[1] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[0];
    quotient[0] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    return remainder;
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR u256 default_mod(const u256& lhs, const UnsignedInteger& rhs) noexcept
{
    if (rhs <= UINT64_MAX)
    {
        return default_mod(lhs, static_cast<std::uint64_t>(rhs));
    }

    std::uint32_t u[8] {};
    std::uint32_t v[8] {};
    std::uint32_t q[8] {};

    const auto m {div_to_words(lhs, u)};
    const auto n {div_to_words(rhs, v)};

    int128::detail::impl::knuth_divide<true>(u, m, v, n, q);

    return from_words(u);
}

} // namespace impl

BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator%(const u256& lhs, const u256& rhs) noexcept
{
    return impl::default_mod(lhs, rhs);
}

template <typename UnsignedInteger>
BOOST_DECIMAL_CUDA_CONSTEXPR u256 operator%(const u256& lhs, const UnsignedInteger rhs) noexcept
{
    return impl::default_mod(lhs, rhs);
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator%=(const u256& rhs) noexcept
{
    *this = *this % rhs;
    return *this;
}

BOOST_DECIMAL_CUDA_CONSTEXPR u256& u256::operator%=(const std::uint64_t rhs) noexcept
{
    *this = impl::default_mod(*this, rhs);
    return *this;
}

namespace impl {

inline auto u256_to_buffer(char (&buffer)[128], u256 v) noexcept
{
    constexpr u256 zero {0, 0, 0, 0};

    char* p = buffer + 128;
    *--p = '\0';

    do
    {
        const auto index {static_cast<std::size_t>(v % UINT64_C(10))};
        BOOST_DECIMAL_ASSERT(index <= 10);
        *--p = "0123456789"[index];
        v /= UINT64_C(10);
    }
    while ( v != zero );

    return p;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_DISABLE_IOSTREAM)
template <typename charT, typename traits>
auto operator<<(std::basic_ostream<charT, traits>& os, const u256& val) -> std::basic_ostream<charT, traits>&
{
    char buffer[128];

    os << impl::u256_to_buffer(buffer, val);

    return os;
}
#endif

template <bool>
class numeric_limits_impl256
{
public:

    // Member constants
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;

    // These members were deprecated in C++23
    #if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    #endif

    static constexpr std::float_round_style round_style = std::round_toward_zero;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr int digits = 256;
    static constexpr int digits10 = 78;
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = std::numeric_limits<std::uint64_t>::traps;
    static constexpr bool tinyness_before = false;

    // Member functions
    static constexpr u256 (min)() { return {0, 0, 0, 0}; }
    static constexpr u256 lowest() { return {0, 0, 0, 0}; }
    static constexpr u256 (max)() { return {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX}; }
    static constexpr u256 epsilon() { return {0, 0, 0, 0}; }
    static constexpr u256 round_error() { return {0, 0, 0, 0}; }
    static constexpr u256 infinity() { return {0, 0, 0, 0}; }
    static constexpr u256 quiet_NaN() { return {0, 0, 0, 0}; }
    static constexpr u256 signaling_NaN() { return {0, 0, 0, 0}; }
    static constexpr u256 denorm_min() { return {0, 0, 0, 0}; }
};

#if !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

template <bool b> constexpr bool numeric_limits_impl256<b>::is_specialized;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_signed;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_integer;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_exact;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_infinity;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_quiet_NaN;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_signaling_NaN;

// These members were deprecated in C++23
#if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
template <bool b> constexpr std::float_denorm_style numeric_limits_impl256<b>::has_denorm;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_denorm_loss;
#endif

template <bool b> constexpr std::float_round_style numeric_limits_impl256<b>::round_style;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_iec559;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_bounded;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_modulo;
template <bool b> constexpr int numeric_limits_impl256<b>::digits;
template <bool b> constexpr int numeric_limits_impl256<b>::digits10;
template <bool b> constexpr int numeric_limits_impl256<b>::max_digits10;
template <bool b> constexpr int numeric_limits_impl256<b>::radix;
template <bool b> constexpr int numeric_limits_impl256<b>::min_exponent;
template <bool b> constexpr int numeric_limits_impl256<b>::min_exponent10;
template <bool b> constexpr int numeric_limits_impl256<b>::max_exponent;
template <bool b> constexpr int numeric_limits_impl256<b>::max_exponent10;
template <bool b> constexpr bool numeric_limits_impl256<b>::traps;
template <bool b> constexpr bool numeric_limits_impl256<b>::tinyness_before;

#endif // !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

} // namespace detail
} // namespace decimal
} // namespace boost

namespace std {

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <>
class numeric_limits<boost::decimal::detail::u256> :
    public boost::decimal::detail::numeric_limits_impl256<true> {};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

} // namespace std

#endif // BOOST_DECIMAL_DETAIL_U256_HPP
