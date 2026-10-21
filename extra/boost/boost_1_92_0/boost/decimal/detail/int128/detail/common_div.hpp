// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_COMMON_DIV_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_COMMON_DIV_HPP

#include <boost/decimal/detail/int128/detail/config.hpp>
#include <boost/decimal/detail/int128/detail/clz.hpp>

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <cstdint>
#include <cstring>

#endif

namespace boost {
namespace int128 {
namespace detail {

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wassume"
#endif

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void half_word_div(const T& lhs, const std::uint32_t rhs, T& quotient, T& remainder) noexcept
{
    using high_word_type = decltype(T{}.high);

    BOOST_DECIMAL_DETAIL_INT128_ASSUME(rhs != 0); // LCOV_EXCL_LINE

    // Use Barrett reduction-inspired approach
    const std::uint64_t divisor {rhs};

    const auto q_high {static_cast<std::uint64_t>(lhs.high) / divisor};
    auto r {static_cast<std::uint64_t>(lhs.high) % divisor};

    const auto low_high {static_cast<std::uint32_t>(lhs.low >> 32U)};
    const auto low_low {static_cast<std::uint32_t>(lhs.low)};

    r = (r << 32U) | low_high;
    const auto q_mid {r / divisor};
    r %= divisor;

    r = (r << 32U) | low_low;
    const auto q_low {r / divisor};
    r %= divisor;

    quotient.high = static_cast<high_word_type>(q_high);
    quotient.low = (q_mid << 32U) | q_low;
    remainder.low = r;
}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void half_word_div(const T& lhs, const std::uint32_t rhs, T& quotient) noexcept
{
    using high_word_type = decltype(T{}.high);

    BOOST_DECIMAL_DETAIL_INT128_ASSUME(rhs != 0); // LCOV_EXCL_LINE

    quotient.high = static_cast<high_word_type>(static_cast<std::uint64_t>(lhs.high) / rhs);
    auto remainder {((static_cast<std::uint64_t>(lhs.high) % rhs) << 32) | (lhs.low >> 32)};
    quotient.low = (remainder / rhs) << 32;
    remainder = ((remainder % rhs) << 32) | (lhs.low & UINT32_MAX);
    quotient.low |= (remainder / rhs) & UINT32_MAX;
}

namespace impl {

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4127) // Pre c++17 the if constexpr remainder part will hit this
#endif

template <std::size_t v_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void unpack_v(std::uint32_t (&vn)[4], const std::uint32_t (&v)[v_size],
    const bool needs_shift, const int s, const int complement_s, const std::integral_constant<std::size_t, 2>&) noexcept
{
    vn[1] = needs_shift ? ((v[1] << s) | (v[0] >> complement_s)) : v[1];
    vn[0] = needs_shift ? (v[0] << s) : v[0];
}

template <std::size_t v_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void unpack_v(std::uint32_t (&vn)[4], const std::uint32_t (&v)[v_size],
    const bool needs_shift, const int s, const int complement_s, const std::integral_constant<std::size_t, 4>&) noexcept
{
    vn[3] = needs_shift ? ((v[3] << s) | (v[2] >> complement_s)) : v[3];
    vn[2] = needs_shift ? ((v[2] << s) | (v[1] >> complement_s)) : v[2];
    vn[1] = needs_shift ? ((v[1] << s) | (v[0] >> complement_s)) : v[1];
    vn[0] = needs_shift ? (v[0] << s) : v[0];
}

template <std::size_t v_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void unpack_v(std::uint32_t (&vn)[8], const std::uint32_t (&v)[v_size],
    const bool needs_shift, const int s, const int complement_s, const std::integral_constant<std::size_t, 8>&) noexcept
{
    vn[7] = needs_shift ? ((v[7] << s) | (v[6] >> complement_s)) : v[7];
    vn[6] = needs_shift ? ((v[6] << s) | (v[5] >> complement_s)) : v[6];
    vn[5] = needs_shift ? ((v[5] << s) | (v[4] >> complement_s)) : v[5];
    vn[4] = needs_shift ? ((v[4] << s) | (v[3] >> complement_s)) : v[4];
    vn[3] = needs_shift ? ((v[3] << s) | (v[2] >> complement_s)) : v[3];
    vn[2] = needs_shift ? ((v[2] << s) | (v[1] >> complement_s)) : v[2];
    vn[1] = needs_shift ? ((v[1] << s) | (v[0] >> complement_s)) : v[1];
    vn[0] = needs_shift ? (v[0] << s) : v[0];
}

template <std::size_t un_size, std::size_t u_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void unpack_u(std::uint32_t (&un)[un_size], const std::uint32_t (&u)[u_size],
    const bool needs_shift, const int s, const int complement_s, const std::integral_constant<std::size_t, 4>&) noexcept
{
    un[4] = needs_shift ? (u[3] >> complement_s) : 0;
    un[3] = needs_shift ? ((u[3] << s) | (u[2] >> complement_s)) : u[3];
    un[2] = needs_shift ? ((u[2] << s) | (u[1] >> complement_s)) : u[2];
    un[1] = needs_shift ? ((u[1] << s) | (u[0] >> complement_s)) : u[1];
    un[0] = needs_shift ? (u[0] << s) : u[0];
}

template <std::size_t un_size, std::size_t u_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void unpack_u(std::uint32_t (&un)[un_size], const std::uint32_t (&u)[u_size],
    const bool needs_shift, const int s, const int complement_s, const std::integral_constant<std::size_t, 8>&) noexcept
{
    un[8] = needs_shift ? (u[7] >> complement_s) : 0;
    un[7] = needs_shift ? ((u[7] << s) | (u[6] >> complement_s)) : u[7];
    un[6] = needs_shift ? ((u[6] << s) | (u[5] >> complement_s)) : u[6];
    un[5] = needs_shift ? ((u[5] << s) | (u[4] >> complement_s)) : u[5];
    un[4] = needs_shift ? ((u[4] << s) | (u[3] >> complement_s)) : u[4];
    un[3] = needs_shift ? ((u[3] << s) | (u[2] >> complement_s)) : u[3];
    un[2] = needs_shift ? ((u[2] << s) | (u[1] >> complement_s)) : u[2];
    un[1] = needs_shift ? ((u[1] << s) | (u[0] >> complement_s)) : u[1];
    un[0] = needs_shift ? (u[0] << s) : u[0];
}

// See: The Art of Computer Programming Volume 2 (Semi-numerical algorithms) section 4.3.1
// Algorithm D: Division of Non-negative integers
template <bool need_remainder, std::size_t u_size, std::size_t v_size, std::size_t q_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr void knuth_divide(std::uint32_t (&u)[u_size], const std::size_t m,
                            const std::uint32_t (&v)[v_size], const std::size_t n,
                            std::uint32_t (&q)[q_size]) noexcept
{
    // D.1
    const auto s {countl_zero(v[n - 1])};
    const auto complement_s {32 - s};
    const bool needs_shift {s > 0};

    // Create normalized versions of u and v
    constexpr std::size_t un_size {u_size == 8 ? 9 : 5};
    constexpr std::size_t vn_size {v_size == 8 ? 8 : 4};
    std::uint32_t un[un_size] {};
    std::uint32_t vn[vn_size] {};

    static_assert(v_size == 8 || v_size == 4 || v_size == 2, "Unknown size for denominator");
    unpack_u(un, u, needs_shift, s, complement_s, std::integral_constant<std::size_t, u_size>{});
    unpack_v(vn, v, needs_shift, s, complement_s, std::integral_constant<std::size_t, v_size>{});

    // D.2
    for (std::size_t j {m - n}; j != static_cast<std::size_t>(-1); --j)
    {
        // D.3
        const auto dividend {(static_cast<std::uint64_t>(un[j+n]) << 32) | un[j+n-1]};
        const auto divisor {static_cast<std::uint64_t>(vn[n-1])};
        auto q_hat {dividend / divisor};
        auto r_hat {dividend % divisor};

        while (q_hat > UINT32_MAX ||
               (q_hat * vn[n-2]) > ((r_hat << 32) | un[j+n-2]))
        {
            --q_hat;
            r_hat += vn[n-1];
            if (r_hat > UINT32_MAX)
            {
                break;
            }
        }

        // D.4
        std::int64_t borrow {};
        for (std::size_t i {}; i < n; ++i)
        {
            const auto p {q_hat * vn[i]};
            const auto p_lo {static_cast<std::uint32_t>(p & UINT32_MAX)};
            const auto p_hi {static_cast<std::uint32_t>(p >> 32)};

            borrow += static_cast<std::int64_t>(un[j+i]) - static_cast<std::int64_t>(p_lo);
            un[j+i] = static_cast<std::uint32_t>(borrow & UINT32_MAX);
            borrow >>= 32;

            borrow -= p_hi;
        }
        borrow += un[j+n];
        un[j+n] = static_cast<std::uint32_t>(borrow & UINT32_MAX);

        // D.5
        q[j] = static_cast<std::uint32_t>(q_hat & UINT32_MAX);
        if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(borrow < 0))
        {
            // D.6
            // The probability of hitting this path is about 4.7e-10
            --q[j];                                                             // LCOV_EXCL_LINE
            std::uint64_t carry {};                                             // LCOV_EXCL_LINE
            for (std::size_t i = 0; i < n; ++i)                                 // LCOV_EXCL_LINE
            {                                                                   // LCOV_EXCL_LINE
                carry += static_cast<std::uint64_t>(un[j+i]) + vn[i];           // LCOV_EXCL_LINE
                un[j+i] = static_cast<std::uint32_t>(carry & UINT32_MAX);       // LCOV_EXCL_LINE
                carry >>= 32U;                                                  // LCOV_EXCL_LINE
            }                                                                   // LCOV_EXCL_LINE
            un[j+n] += static_cast<std::uint32_t>(carry & UINT32_MAX);          // LCOV_EXCL_LINE
        }
    }

    // D.8
    // If we are only calculating division we can completely skip this step
    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (need_remainder)
    {
        if (s > 0)
        {
            for (std::size_t i {}; i < n-1; i++)
            {
                u[i] = (un[i] >> s) | (un[i+1] << (32 - s));
            }
            u[n-1] = un[n-1] >> s;
        }
        else
        {
            for (std::size_t i {}; i < n; i++)
            {
                u[i] = un[i];
            }
        }

        // Clear anything left in u
        for (std::size_t i {n}; i < m; i++)
        {
            u[i] = 0;
        }
    }
}

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr std::size_t to_words(const T& x, std::uint32_t (&words)[4]) noexcept
{
    #if !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(&words, &x, sizeof(T));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x.low & UINT32_MAX);                              // LCOV_EXCL_LINE
        words[1] = static_cast<std::uint32_t>(x.low >> 32);                                     // LCOV_EXCL_LINE
        words[2] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) & UINT32_MAX); // LCOV_EXCL_LINE
        words[3] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) >> 32);        // LCOV_EXCL_LINE
    }

    BOOST_DECIMAL_DETAIL_INT128_ASSERT_MSG(x != static_cast<T>(0), "Division by 0");

    std::size_t word_count {4};
    while (words[word_count - 1U] == 0U)
    {
        word_count--;
    }

    return word_count;
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr std::size_t to_words(const std::uint64_t x, std::uint32_t (&words)[2]) noexcept
{
    #if !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(&words, &x, sizeof(std::uint64_t));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x & UINT32_MAX);  // LCOV_EXCL_LINE
        words[1] = static_cast<std::uint32_t>(x >> 32);         // LCOV_EXCL_LINE
    }

    return x > UINT32_MAX ? 2 : 1;
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr std::size_t to_words(const std::uint32_t x, std::uint32_t (&words)[1]) noexcept
{
    words[0] = x;

    return 1;
}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr T from_words(const std::uint32_t (&words)[4]) noexcept
{
    using high_word_type = decltype(T{}.high);

    const auto low {static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32)};
    const auto high {static_cast<std::uint64_t>(words[2]) | (static_cast<std::uint64_t>(words[3]) << 32)};

    return {static_cast<high_word_type>(high), low};
}

// HAS_FAST_DIV128 is true on platforms where we expose a hardware 128-by-64
// division primitive: MSVC x64 via _udiv128, and GCC/Clang on x86-64 via
// inline divq. Both flavors share the same Moller-Granlund-style 2x2 division
// scaffolding below, parameterized over portable umul128/udiv128 helpers.
//
// AArch64 stays on the __udivti3 / Knuth-D path -- UDIV is already 7c/0.5
// there and the inline-divq port is x86-only. CUDA device code is excluded
// because inline asm and _udiv128 are not available in that environment.
#if defined(_M_AMD64) && !defined(__GNUC__) && !defined(__clang__) && _MSC_VER >= 1920
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128
#elif defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) \
   && (defined(__GNUC__) || defined(__clang__))      \
   && defined(__x86_64__)                            \
   && !defined(__CUDA_ARCH__)
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128
#endif

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128

// MSVC's _addcarry_u64 takes std::uint64_t* (unsigned __int64*); GCC/Clang's
// _addcarry_u64 takes unsigned long long*, which on LP64 Linux is a distinct
// type from std::uint64_t (typedef'd to unsigned long there). Wrap the macros
// with helpers that accept std::uint64_t* and convert internally.
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE unsigned char int128_addcarry_u64(unsigned char c, std::uint64_t x, std::uint64_t y, std::uint64_t* out) noexcept
{
#if defined(_M_AMD64) && !defined(__GNUC__) && !defined(__clang__)
    return BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY(c, x, y, out);
#else
    unsigned long long tmp {static_cast<unsigned long long>(*out)};
    const unsigned char result {BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY(c, x, y, &tmp)};
    *out = static_cast<std::uint64_t>(tmp);
    return result;
#endif
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE unsigned char int128_subborrow_u64(unsigned char c, std::uint64_t x, std::uint64_t y, std::uint64_t* out) noexcept
{
#if defined(_M_AMD64) && !defined(__GNUC__) && !defined(__clang__)
    return BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW(c, x, y, out);
#else
    unsigned long long tmp {static_cast<unsigned long long>(*out)};
    const unsigned char result {BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW(c, x, y, &tmp)};
    *out = static_cast<std::uint64_t>(tmp);
    return result;
#endif
}

// 64x64 -> 128-bit unsigned multiply. MSVC uses the _umul128 intrinsic;
// GCC/Clang use the native __uint128_t multiply which the compiler lowers
// to mul/mulx on x86-64. Neither path is usable in a constant expression
// (callers gate via IS_CONSTANT_EVALUATED), so this is not constexpr.
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE std::uint64_t int128_umul128_intrinsic(std::uint64_t a, std::uint64_t b, std::uint64_t* hi) noexcept
{
#if defined(_M_AMD64) && !defined(__GNUC__) && !defined(__clang__)
    return _umul128(a, b, hi);
#else
    const __uint128_t prod {static_cast<__uint128_t>(a) * b};
    *hi = static_cast<std::uint64_t>(prod >> 64);
    return static_cast<std::uint64_t>(prod);
#endif
}

// 128-bit-by-64-bit unsigned divide. MSVC uses _udiv128; GCC/Clang use
// inline divq. Caller must guarantee `high < divisor` so divq does not
// raise #DE (the Moller-Granlund normalization in div_mod_intrinsic
// below ensures this). Not constexpr because inline asm and _udiv128
// are runtime-only; div_mod_intrinsic gates its call with
// IS_CONSTANT_EVALUATED.
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE std::uint64_t int128_udiv128_intrinsic(std::uint64_t high, std::uint64_t low, std::uint64_t divisor, std::uint64_t* remainder) noexcept
{
#if defined(_M_AMD64) && !defined(__GNUC__) && !defined(__clang__) && _MSC_VER >= 1920
    return _udiv128(high, low, divisor, remainder);
#else
    std::uint64_t q;
    std::uint64_t r;
    __asm__ ("divq %4"
             : "=a"(q), "=d"(r)
             : "0"(low), "1"(high), "r"(divisor)
             : "cc");
    *remainder = r;
    return q;
#endif
}

template <bool needs_mod, typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr T div_mod_intrinsic(T dividend, T divisor, T& remainder)
{
    using high_word_type = decltype(T{}.high);

    // Skip normalization if divisor is already large enough
    // use direct division and intrinsic
    // This is only possible in the unsigned case. The check uses
    // std::is_unsigned on the high-word type rather than
    // std::numeric_limits<T>::is_signed to avoid forcing implicit
    // instantiation of std::numeric_limits<T> from inside this header (the
    // explicit specialization sits later in uint128_imp.hpp; instantiating
    // first via numeric_limits<T> here causes clang to reject the
    // specialization as "explicit specialization after instantiation").
    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_unsigned<high_word_type>::value)
    {
        constexpr auto divisor_lower_bound{UINT64_MAX >> 1};
        if (divisor.high >= divisor_lower_bound)
        {
            T quotient{};

            quotient.low = static_cast<std::uint64_t>(dividend.high / divisor.high);

            std::uint64_t product0_high{};
            auto product0_low{int128_umul128_intrinsic(quotient.low, divisor.low, &product0_high)};

            std::uint64_t product1_high{};
            auto product1_low{int128_umul128_intrinsic(quotient.low, static_cast<std::uint64_t>(divisor.high), &product1_high)};

            T product{};
            product.low = product0_low;
            std::uint64_t product_high_tmp {static_cast<std::uint64_t>(product.high)};
            auto carry{int128_addcarry_u64(0, product0_high, product1_low, &product_high_tmp)};
            product.high = static_cast<high_word_type>(product_high_tmp);
            product1_high += static_cast<std::uint64_t>(carry);

            if (product1_high > 0 || product > dividend)
            {
                --quotient.low;

                // Recalculate with adjusted quotient
                product0_low = int128_umul128_intrinsic(quotient.low, divisor.low, &product0_high);
                product1_low = int128_umul128_intrinsic(quotient.low, static_cast<std::uint64_t>(divisor.high), &product1_high);

                product.low = product0_low;
                product_high_tmp = static_cast<std::uint64_t>(product.high);
                carry = int128_addcarry_u64(0, product0_high, product1_low, &product_high_tmp);
                product.high = static_cast<high_word_type>(product_high_tmp);
                product1_high += static_cast<std::uint64_t>(carry);
            }

            BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR(needs_mod)
            {
                auto borrow{int128_subborrow_u64(0, dividend.low, product.low, &remainder.low)};
                std::uint64_t remainder_high_tmp {static_cast<std::uint64_t>(remainder.high)};
                int128_subborrow_u64(borrow, static_cast<std::uint64_t>(dividend.high), static_cast<std::uint64_t>(product.high), &remainder_high_tmp);
                remainder.high = static_cast<high_word_type>(remainder_high_tmp);
            }

            return quotient;
        }
    }

    const auto shift_amount {countl_zero(static_cast<std::uint64_t>(divisor.high))};
    divisor <<= shift_amount;

    auto high_digit {static_cast<std::uint64_t>(shift_amount == 0 ? 0 : dividend.high >> (64 - shift_amount))};
    dividend <<= shift_amount;

    // Initial quotient estimate
    T quotient {};
    const bool high_digit_gte_divisor {high_digit >= static_cast<std::uint64_t>(divisor.high)};
    quotient.high = high_digit_gte_divisor ? 1 : 0;
    std::uint64_t remainder_estimate {};

    quotient.low = int128_udiv128_intrinsic(high_digit_gte_divisor ? high_digit - divisor.high : high_digit,
                                            dividend.high, static_cast<std::uint64_t>(divisor.high), &remainder_estimate);

    // Bounded correction loop with early exit
    // Typically 2 is the most number of corrections we need since this is only for 2x2 division
    // Other cases have been filtered out well before we've made it this far
    int correction_steps {};
    constexpr int max_corrections {2};

    while (correction_steps < max_corrections)
    {
        T product{};
        std::uint64_t product_high_tmp {};
        product.low = int128_umul128_intrinsic(quotient.low, divisor.low, &product_high_tmp);
        product.high = static_cast<high_word_type>(product_high_tmp);
        if (product <= T{static_cast<high_word_type>(remainder_estimate), dividend.low})
        {
            break;
        }

        --quotient.low;
        const auto sum {remainder_estimate + divisor.high};
        if (remainder_estimate > sum)
        {
            break;
        }
        remainder_estimate = sum;

        correction_steps++;
    }

    // Final verification and adjustment
    std::uint64_t product0_high{};
    auto product_low {int128_umul128_intrinsic(quotient.low, divisor.low, &product0_high)};
    auto borrow {int128_subborrow_u64(0, dividend.low, product_low, &dividend.low)};

    std::uint64_t product1_high{};
    product_low = int128_umul128_intrinsic(quotient.low, static_cast<std::uint64_t>(divisor.high), &product1_high);
    product1_high += static_cast<std::uint64_t>(int128_addcarry_u64(0, product_low, product0_high, &product_low));

    std::uint64_t dividend_high_tmp {static_cast<std::uint64_t>(dividend.high)};
    borrow = int128_subborrow_u64(borrow, static_cast<std::uint64_t>(dividend.high), product_low, &dividend_high_tmp);
    dividend.high = static_cast<high_word_type>(dividend_high_tmp);
    borrow = int128_subborrow_u64(borrow, high_digit, product1_high, &high_digit);
    quotient.low -= static_cast<std::uint64_t>(borrow);

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (needs_mod)
    {
        if (borrow)
        {
            auto carry { int128_addcarry_u64(0, dividend.low, divisor.low, &dividend.low) };
            std::uint64_t dividend_high_tmp2 {static_cast<std::uint64_t>(dividend.high)};
            int128_addcarry_u64(carry, static_cast<std::uint64_t>(dividend.high), static_cast<std::uint64_t>(divisor.high), &dividend_high_tmp2);
            dividend.high = static_cast<high_word_type>(dividend_high_tmp2);
        }

        dividend >>= shift_amount;
        remainder = dividend;
    }

    return quotient;
}

// Moller-Granlund 3/2 reciprocal division for u256 by uint128.
//
// Computes (q, r) such that u = q*d + r, where u is 256-bit (3 limbs effective),
// d is 128-bit (2 limbs, normalized so d.high MSB is set), and v is the
// 64-bit reciprocal of d.
//
// The algorithm requires `d` to be normalized (d.high >= 2^63) and the top
// 128 bits of `u` to be strictly less than `d` so the quotient fits in one
// limb. The d128 division flow satisfies both: the divisor is post-
// expand_significand, so it is in [10^33, 10^34) and shifts to fill 128 bits;
// the dividend top 128 bits after the same shift remain well under 2^127.
//
// The reciprocal v is from reciprocal_2by1 below and must be precomputed.
// References: Moller & Granlund, "Improved Division by Invariant Integers"
// (2010), Algorithm 4.
template <typename U128>
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE std::uint64_t mg32_div_3by2(
    U128 u_high, std::uint64_t u_low, U128 d, std::uint64_t v, U128& r) noexcept
{
    // q = u_high.high * v + u_high  (a 128-bit fused-multiply-add).
    // int128_umul128_intrinsic returns the low 64 bits and writes the high
    // 64 bits through the out pointer.
    std::uint64_t q_high{};
    std::uint64_t q_low {int128_umul128_intrinsic(u_high.high, v, &q_high)};
    // Add u_high to (q_high, q_low). Carry from low into high.
    {
        const std::uint64_t low_sum {q_low + u_high.low};
        const unsigned char carry_into_high {static_cast<unsigned char>(low_sum < q_low ? 1U : 0U)};
        q_low = low_sum;
        q_high += u_high.high + carry_into_high;
    }

    // r1 = u_high.low - q_high * d.high  (single 64-bit limb)
    const std::uint64_t r1 {u_high.low - q_high * d.high};

    // (t_high, t_low) = q_high * d.low (full 128-bit product)
    std::uint64_t t_high{};
    const std::uint64_t t_low {int128_umul128_intrinsic(q_high, d.low, &t_high)};

    // r = (r1, u_low) - (t_high, t_low) - d
    std::uint64_t r_low {u_low};
    std::uint64_t r_high {r1};
    unsigned char borrow {int128_subborrow_u64(0, r_low, t_low, &r_low)};
    int128_subborrow_u64(borrow, r_high, t_high, &r_high);
    borrow = int128_subborrow_u64(0, r_low, d.low, &r_low);
    int128_subborrow_u64(borrow, r_high, d.high, &r_high);

    // q' = q_high + 1
    std::uint64_t quotient {q_high + 1U};

    // First correction: if r_high >= q_low, q'-- and r += d.
    if (r_high >= q_low)
    {
        --quotient;
        const unsigned char carry {int128_addcarry_u64(0, r_low, d.low, &r_low)};
        int128_addcarry_u64(carry, r_high, d.high, &r_high);
    }

    // Second correction (very rare): if r >= d, q'++ and r -= d.
    const bool r_ge_d {r_high > d.high || (r_high == d.high && r_low >= d.low)};
    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(r_ge_d))
    {
        ++quotient;
        const unsigned char b2 {int128_subborrow_u64(0, r_low, d.low, &r_low)};
        int128_subborrow_u64(b2, r_high, d.high, &r_high);
    }

    r.low = r_low;
    r.high = static_cast<decltype(r.high)>(r_high);
    return quotient;
}

// Precompute the 64-bit reciprocal for a normalized 128-bit divisor.
// Returns v such that floor((2^192 - 1) / d) = 2^128 + v.
// Precondition: d.high has its MSB set (d >= 2^127).
// Reference: Moller & Granlund 2010, Algorithm 2.
template <typename U128>
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE std::uint64_t mg32_reciprocal_2by1(U128 d) noexcept
{
    // v_1 = floor((2^128 - 1) / d.high) - 2^64, computed via one 128/64 divide.
    // The trick: divq(UINT64_MAX - d.high, UINT64_MAX, d.high) sidesteps the
    // overflow trap (the divq precondition that high < divisor) because
    // d.high >= 2^63 guarantees UINT64_MAX - d.high < d.high.
    std::uint64_t dummy{};
    std::uint64_t v {int128_udiv128_intrinsic(~d.high, UINT64_MAX, d.high, &dummy)};

    // p = d.high * v + d.low (modular, low 64 bits only; the high part is
    // implicit in the residue analysis below).
    std::uint64_t p {d.high * v + d.low};

    // Adjust if the addition wrapped (p < d.low).
    if (p < d.low)
    {
        --v;
        if (p >= d.high)
        {
            --v;
            p -= d.high;
        }
        p -= d.high;
    }

    // (t_high, t_low) = v * d.low.
    // int128_umul128_intrinsic returns the low limb and writes the high
    // limb through the out pointer.
    std::uint64_t t_high{};
    const std::uint64_t t_low {int128_umul128_intrinsic(v, d.low, &t_high)};

    // p += t_high (with overflow detection)
    const std::uint64_t p_new {p + t_high};
    if (p_new < p)
    {
        // Overflow; adjust.
        --v;
        // If (p_new, t_low) >= d, decrement again.
        if (p_new > d.high || (p_new == d.high && t_low >= d.low))
        {
            --v;
        }
    }
    return v;
}

#endif

} // namespace impl

// We only need to take the time to process the remainder in the modulo case
// In the division case it is a waste of cycles

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void one_word_div(const T& lhs, const std::uint64_t rhs, T& quotient) noexcept
{
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        using high_word_type = decltype(T{}.high);

        quotient.high = static_cast<high_word_type>(static_cast<std::uint64_t>(lhs.high) / rhs);
        auto remainder {static_cast<std::uint64_t>(lhs.high) % rhs};
        quotient.low = impl::int128_udiv128_intrinsic(remainder, lhs.low, rhs, &remainder);
        return;
    }

    #endif

    if (rhs <= UINT32_MAX)
    {
        half_word_div(lhs, static_cast<std::uint32_t>(rhs), quotient);
    }
    else
    {
        std::uint32_t u[4] {};
        std::uint32_t v[2] {};
        std::uint32_t q[4] {};

        const auto m {impl::to_words(lhs, u)};
        const auto n {impl::to_words(rhs, v)};

        impl::knuth_divide<false>(u, m, v, n, q);

        quotient = impl::from_words<T>(q);
    }
}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void one_word_div(const T& lhs, const std::uint64_t rhs, T& quotient, T& remainder) noexcept
{
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        using high_word_type = decltype(T{}.high);

        quotient.high = static_cast<high_word_type>(static_cast<std::uint64_t>(lhs.high) / rhs);
        remainder.low = static_cast<std::uint64_t>(lhs.high) % rhs;
        quotient.low = impl::int128_udiv128_intrinsic(remainder.low, lhs.low, rhs, &remainder.low);
        return;
    }

    #else

    if (rhs <= UINT32_MAX)
    {
        half_word_div(lhs, static_cast<std::uint32_t>(rhs), quotient, remainder);
    }
    else
    {
        std::uint32_t u[4] {};
        std::uint32_t v[2] {};
        std::uint32_t q[4] {};

        const auto m {impl::to_words(lhs, u)};
        const auto n {impl::to_words(rhs, v)};

        impl::knuth_divide<true>(u, m, v, n, q);

        quotient = impl::from_words<T>(q);
        remainder = impl::from_words<T>(u);
    }

    #endif
}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void one_word_div(const T& lhs, const std::uint32_t rhs, T& quotient, T& remainder) noexcept
{
    half_word_div(lhs, rhs, quotient, remainder);
}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void one_word_div(const T& lhs, const std::uint32_t rhs, T& quotient) noexcept
{
    half_word_div(lhs, rhs, quotient);
}

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127) // Conditional expression is constant is true pre-C++17
#  pragma warning(disable : 4804) // Unsafe comparison with bool
#endif

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr T knuth_div(const T& dividend, const T& divisor) noexcept
{
    BOOST_DECIMAL_DETAIL_INT128_ASSUME(divisor != static_cast<T>(0));

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR(std::is_unsigned<decltype(T{}.high)>::value)
    {
        if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(dividend))
        {
            T remainder{};
            return impl::div_mod_intrinsic<false>(dividend, divisor, remainder);
        }
    }

    #endif

    std::uint32_t u[4]{};
    std::uint32_t v[4]{};
    std::uint32_t q[4]{};

    const auto m{ impl::to_words(dividend, u) };
    const auto n{ impl::to_words(divisor, v) };

    impl::knuth_divide<false>(u, m, v, n, q);

    return impl::from_words<T>(q);

}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr T knuth_div(const T& dividend, const T& divisor, T& remainder) noexcept
{
    BOOST_DECIMAL_DETAIL_INT128_ASSUME(divisor != static_cast<T>(0));

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_FAST_DIV128

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR(std::is_unsigned<decltype(T{}.high)>::value)
    {
        if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(dividend))
        {
            return impl::div_mod_intrinsic<true>(dividend, divisor, remainder);
        }
    }


    #endif

    std::uint32_t u[4]{};
    std::uint32_t v[4]{};
    std::uint32_t q[4]{};

    const auto m{ impl::to_words(dividend, u) };
    const auto n{ impl::to_words(divisor, v) };

    impl::knuth_divide<true>(u, m, v, n, q);

    remainder = impl::from_words<T>(u);

    return impl::from_words<T>(q);
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_COMMON_DIV_HPP
