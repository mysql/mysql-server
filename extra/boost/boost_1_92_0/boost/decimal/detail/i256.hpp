// Copyright 2023 - 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This is not a fully featured implementation of a 256-bit integer like int128::uint128_t is
// i256 only contains the minimum amount that we need to perform operations like decimal128_t add/sub

#ifndef BOOST_DECIMAL_DETAIL_I256_HPP
#define BOOST_DECIMAL_DETAIL_I256_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/u256.hpp>
#include <boost/decimal/detail/int128.hpp>

namespace boost {
namespace decimal {
namespace detail {

namespace impl {

// This impl works regardless of intrinsics or otherwise
BOOST_DECIMAL_CUDA_CONSTEXPR u256 u256_add_impl(const int128::uint128_t& lhs, const int128::uint128_t& rhs) noexcept
{
    u256 result;
    std::uint64_t carry {};

    auto sum {lhs.low + rhs.low};
    result[0] = sum;
    carry = (sum < lhs.low) ? 1 : 0;

    sum = lhs.high + rhs.high + carry;
    result[1] = sum;
    result[2] = static_cast<std::uint64_t>(sum < lhs.high || (sum == lhs.high && carry));

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_ADD_CARRY)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 u256_add(const int128::uint128_t& lhs, const int128::uint128_t& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::u256_add_impl(lhs, rhs);
    }
    else
    {
        unsigned long long result[3] {};
        unsigned char carry {};
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs.low, rhs.low, &result[0]);
        result[2] = static_cast<std::uint64_t>(BOOST_DECIMAL_ADD_CARRY(carry, lhs.high, rhs.high, &result[1]));

        return {UINT64_C(0), result[2], result[1], result[0]};
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(BOOST_DECIMAL_ADD_CARRY)

BOOST_DECIMAL_CUDA_CONSTEXPR u256 u256_add(const int128::uint128_t& lhs, const int128::uint128_t& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::u256_add_impl(lhs, rhs);
    }
    else
    {
        unsigned long long result[3] {};
        bool carry {};
        carry = impl::add_carry_u64(carry, lhs.low, rhs.low, &result[0]);
        result[2] = static_cast<std::uint64_t>(impl::add_carry_u64(carry, lhs.high, rhs.high, &result[1]));

        return {UINT64_C(0), result[2], result[1], result[0]};
    }
}

#endif

namespace impl {

BOOST_DECIMAL_CUDA_CONSTEXPR std::uint64_t sub_borrow_u64(const std::uint64_t borrow_in, const std::uint64_t a, const std::uint64_t b, std::uint64_t& result) noexcept
{
    const auto diff {a - b};
    const auto b1 {static_cast<std::uint64_t>(a < b)};
    result = diff - borrow_in;
    const auto borrow_out {b1 | (diff < borrow_in)};

    return borrow_out;
}

BOOST_DECIMAL_CUDA_CONSTEXPR bool i256_sub_impl(const u256& a, const u256& b, u256& result) noexcept
{
    if (a >= b)
    {
        auto borrow {impl::sub_borrow_u64(0, a[0], b[0], result[0])};
        borrow = impl::sub_borrow_u64(borrow, a[1], b[1], result[1]);
        borrow = impl::sub_borrow_u64(borrow, a[2], b[2], result[2]);
        impl::sub_borrow_u64(borrow, a[3], b[3], result[3]);
        return false;
    }
    else
    {
        // a < b: negative result, |a - b| = b - a
        auto borrow {impl::sub_borrow_u64(0, b[0], a[0], result[0])};
        borrow = impl::sub_borrow_u64(borrow, b[1], a[1], result[1]);
        borrow = impl::sub_borrow_u64(borrow, b[2], a[2], result[2]);
        impl::sub_borrow_u64(borrow, b[3], a[3], result[3]);
        return true;
    }
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_SUB_BORROW)

BOOST_DECIMAL_CUDA_CONSTEXPR bool i256_sub(const u256& a, const u256& b, u256& res) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::i256_sub_impl(a, b, res);
    }
    else
    {
        if (a >= b)
        {
            unsigned long long result[4] {};
            unsigned char borrow {};

            borrow = BOOST_DECIMAL_SUB_BORROW(borrow, a[0], b[0], &result[0]);
            borrow = BOOST_DECIMAL_SUB_BORROW(borrow, a[1], b[1], &result[1]);
            borrow = BOOST_DECIMAL_SUB_BORROW(borrow, a[2], b[2], &result[2]);
            BOOST_DECIMAL_SUB_BORROW(borrow, a[3], b[3], &result[3]);

            res = u256{result[3], result[2], result[1], result[0]};
            return false;
        }
        else
        {
            unsigned long long result[4] {};
            unsigned char borrow {};

            borrow = BOOST_DECIMAL_SUB_BORROW(borrow, b[0], a[0], &result[0]);
            borrow = BOOST_DECIMAL_SUB_BORROW(borrow, b[1], a[1], &result[1]);
            borrow = BOOST_DECIMAL_SUB_BORROW(borrow, b[2], a[2], &result[2]);
            BOOST_DECIMAL_SUB_BORROW(borrow, b[3], a[3], &result[3]);

            res = u256{result[3], result[2], result[1], result[0]};
            return true;
        }
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(BOOST_DECIMAL_SUB_BORROW) && BOOST_DECIMAL_HAS_BUILTIN(__builtin_subcll)

BOOST_DECIMAL_CUDA_CONSTEXPR bool i256_sub(const u256& a, const u256& b, u256& result) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::i256_sub_impl(a, b, result);
    }
    else
    {
        if (a >= b)
        {
            unsigned long long borrow {};

            result[0] = __builtin_subcll(a[0], b[0], borrow, &borrow);
            result[1] = __builtin_subcll(a[1], b[1], borrow, &borrow);
            result[2] = __builtin_subcll(a[2], b[2], borrow, &borrow);
            result[3] = __builtin_subcll(a[3], b[3], borrow, &borrow);

            return false;
        }
        else
        {
            unsigned long long borrow {};

            result[0] = __builtin_subcll(b[0], a[0], borrow, &borrow);
            result[1] = __builtin_subcll(b[1], a[1], borrow, &borrow);
            result[2] = __builtin_subcll(b[2], a[2], borrow, &borrow);
            result[3] = __builtin_subcll(b[3], a[3], borrow, &borrow);

            return true;
        }
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_HAS_x86_INTRINSICS)

namespace impl {

// __builtin_subcll is missing from some platforms but __builtin_sub_overflow is present.
// Implement exactly as shown on the GCC page: https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html

inline std::uint64_t subcll(const std::uint64_t a, const std::uint64_t b, const std::uint64_t carry_in, std::uint64_t* carry_out) noexcept
{
    std::uint64_t s;
    const auto c1 {__builtin_sub_overflow(a, b, &s)};
    const auto c2 {__builtin_sub_overflow(s, carry_in, &s)};
    *carry_out = static_cast<std::uint64_t>(c1 | c2);

    return s;
}

} // namespace impl

BOOST_DECIMAL_CUDA_CONSTEXPR bool i256_sub(const u256& a, const u256& b, u256& result) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::i256_sub_impl(a, b, result);
    }
    else
    {
        if (a >= b)
        {
            unsigned long long borrow {};

            result[0] = impl::subcll(a[0], b[0], borrow, &borrow);
            result[1] = impl::subcll(a[1], b[1], borrow, &borrow);
            result[2] = impl::subcll(a[2], b[2], borrow, &borrow);
            result[3] = impl::subcll(a[3], b[3], borrow, &borrow);

            return false;
        }
        else
        {
            unsigned long long borrow {};

            result[0] = impl::subcll(b[0], a[0], borrow, &borrow);
            result[1] = impl::subcll(b[1], a[1], borrow, &borrow);
            result[2] = impl::subcll(b[2], a[2], borrow, &borrow);
            result[3] = impl::subcll(b[3], a[3], borrow, &borrow);

            return true;
        }
    }
}

#else

BOOST_DECIMAL_CUDA_CONSTEXPR bool i256_sub(const u256& a, const u256& b, u256& result) noexcept
{
    return impl::i256_sub_impl(a, b, result);
}

#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_I256_HPP
