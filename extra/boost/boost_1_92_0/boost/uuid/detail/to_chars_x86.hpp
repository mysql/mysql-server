#ifndef BOOST_UUID_DETAIL_TO_CHARS_X86_HPP_INCLUDED
#define BOOST_UUID_DETAIL_TO_CHARS_X86_HPP_INCLUDED

// Copyright 2025 Andrey Semashev
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/config.hpp>

#if defined(BOOST_UUID_USE_SSE2)

#include <cstdint>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/detail/endian.hpp>
#include <boost/uuid/detail/simd_vector.hpp>

#if defined(BOOST_UUID_REPORT_IMPLEMENTATION)
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_UUID_USE_AVX512_V1)
BOOST_PRAGMA_MESSAGE( "Using to_chars_x86.hpp, AVX512v1" )

#elif defined(BOOST_UUID_USE_AVX2)
BOOST_PRAGMA_MESSAGE( "Using to_chars_x86.hpp, AVX2" )

#elif defined(BOOST_UUID_USE_SSE41)
BOOST_PRAGMA_MESSAGE( "Using to_chars_x86.hpp, SSE4.1" )

#elif defined(BOOST_UUID_USE_SSSE3)
BOOST_PRAGMA_MESSAGE( "Using to_chars_x86.hpp, SSSE3" )

#else
BOOST_PRAGMA_MESSAGE( "Using to_chars_x86.hpp, SSE2" )

#endif
#endif // #if defined(BOOST_UUID_REPORT_IMPLEMENTATION)

#if defined(BOOST_UUID_USE_AVX2)
#include <immintrin.h>
#elif defined(BOOST_UUID_USE_SSE41)
#include <smmintrin.h>
#elif defined(BOOST_UUID_USE_SSSE3)
#include <tmmintrin.h>
#else
#include <emmintrin.h>
#endif

namespace boost {
namespace uuids {
namespace detail {

template<
    typename Char,
    bool IsCharASCIICompatible = ('0' == 0x30 && '9' == 0x39 && 'a' == 0x61 && 'f' == 0x66 && '-' == 0x2D),
    bool IsWCharASCIICompatible = (L'0' == 0x30 && L'9' == 0x39 && L'a' == 0x61 && L'f' == 0x66 && L'-' == 0x2D)
>
struct to_chars_simd_char_constants
{
#if defined(BOOST_UUID_USE_SSSE3)
    static const simd_vector128< std::uint8_t > mm_char_table;
#else
    static constexpr std::uint8_t char_a_add = static_cast< std::uint8_t >((0x61 - 10) - 0x30); // ('a' - 10) - '0' in ASCII
    static const simd_vector128< std::uint8_t > mm_char_0_add;
    static const simd_vector128< std::uint8_t > mm_char_a_add;
#endif
    static const simd_vector128< std::uint8_t > mm_char_dash;
};

#if defined(BOOST_UUID_USE_SSSE3)
template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_table =
    {{ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66 }}; // 0123456789abcdef in ASCII
#else
template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_0_add =
    {{ 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30 }}; // 0x30 is '0' in ASCII
template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_a_add =
{{
    char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add,
    char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add
}};
#endif
template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_dash =
    {{ 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D }}; // ---------------- in ASCII

template< bool IsWCharASCIICompatible >
struct to_chars_simd_char_constants< char, false, IsWCharASCIICompatible >
{
    // This requirement is necessary for the _mm_max_epu8 trick in to_chars_simd_core below to work
    static_assert(static_cast< std::uint8_t >('-') < static_cast< std::uint8_t >('0') && static_cast< std::uint8_t >('-') < static_cast< std::uint8_t >('a'),
        "Boost.UUID: Unsupported char encoding, '-' character code is expected to be less than any hexadecimal characters");

#if defined(BOOST_UUID_USE_SSSE3)
    static const simd_vector128< std::uint8_t > mm_char_table;
#else
    static constexpr std::uint8_t char_a_add = static_cast< std::uint8_t >(('a' - 10) - '0');
    static const simd_vector128< std::uint8_t > mm_char_0_add;
    static const simd_vector128< std::uint8_t > mm_char_a_add;
#endif
    static const simd_vector128< std::uint8_t > mm_char_dash;
};

#if defined(BOOST_UUID_USE_SSSE3)
template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_table =
{{
    static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('1'), static_cast< std::uint8_t >('2'), static_cast< std::uint8_t >('3'),
    static_cast< std::uint8_t >('4'), static_cast< std::uint8_t >('5'), static_cast< std::uint8_t >('6'), static_cast< std::uint8_t >('7'),
    static_cast< std::uint8_t >('8'), static_cast< std::uint8_t >('9'), static_cast< std::uint8_t >('a'), static_cast< std::uint8_t >('b'),
    static_cast< std::uint8_t >('c'), static_cast< std::uint8_t >('d'), static_cast< std::uint8_t >('e'), static_cast< std::uint8_t >('f')
}};
#else
template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_0_add =
{{
    static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'),
    static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'),
    static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'),
    static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0'), static_cast< std::uint8_t >('0')
}};
template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_a_add =
{{
    char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add,
    char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add
}};
#endif
template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_dash =
{{
    static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'),
    static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'),
    static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'),
    static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-'), static_cast< std::uint8_t >('-')
}};

template< bool IsCharASCIICompatible >
struct to_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >
{
    static_assert(static_cast< wchar_t >(static_cast< std::uint8_t >(L'0')) == L'0' && static_cast< wchar_t >(static_cast< std::uint8_t >(L'9')) == L'9' &&
        static_cast< wchar_t >(static_cast< std::uint8_t >(L'a')) == L'a' && static_cast< wchar_t >(static_cast< std::uint8_t >(L'f')) == L'f' &&
        static_cast< wchar_t >(static_cast< std::uint8_t >(L'-')) == L'-',
        "Boost.UUID: Unsupported wchar_t encoding, hexadecimal and dash character codes are expected to be representable by a single byte");

    // This requirement is necessary for the _mm_max_epu8 trick in to_chars_simd_core below to work
    static_assert(static_cast< std::uint8_t >(L'-') < static_cast< std::uint8_t >(L'0') && static_cast< std::uint8_t >(L'-') < static_cast< std::uint8_t >(L'a'),
        "Boost.UUID: Unsupported wchar_t encoding, L'-' character code is expected to be less than any hexadecimal characters");

#if defined(BOOST_UUID_USE_SSSE3)
    static const simd_vector128< std::uint8_t > mm_char_table;
#else
    static constexpr std::uint8_t char_a_add = static_cast< std::uint8_t >((L'a' - 10) - L'0');
    static const simd_vector128< std::uint8_t > mm_char_0_add;
    static const simd_vector128< std::uint8_t > mm_char_a_add;
#endif
    static const simd_vector128< std::uint8_t > mm_char_dash;
};

#if defined(BOOST_UUID_USE_SSSE3)
template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_table =
{{
    static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'1'), static_cast< std::uint8_t >(L'2'), static_cast< std::uint8_t >(L'3'),
    static_cast< std::uint8_t >(L'4'), static_cast< std::uint8_t >(L'5'), static_cast< std::uint8_t >(L'6'), static_cast< std::uint8_t >(L'7'),
    static_cast< std::uint8_t >(L'8'), static_cast< std::uint8_t >(L'9'), static_cast< std::uint8_t >(L'a'), static_cast< std::uint8_t >(L'b'),
    static_cast< std::uint8_t >(L'c'), static_cast< std::uint8_t >(L'd'), static_cast< std::uint8_t >(L'e'), static_cast< std::uint8_t >(L'f')
}};
#else
template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_0_add =
{{
    static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'),
    static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'),
    static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'),
    static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0'), static_cast< std::uint8_t >(L'0')
}};
template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_a_add =
{{
    char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add,
    char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add, char_a_add
}};
#endif
template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > to_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_dash =
{{
    static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'),
    static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'),
    static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'),
    static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-'), static_cast< std::uint8_t >(L'-')
}};

template< typename >
struct to_chars_simd_constants
{
    static const simd_vector128< std::uint8_t > mm_0F;
#if defined(BOOST_UUID_USE_SSSE3)
    static const simd_vector128< std::uint8_t > mm_shuffle_pattern1;
    static const simd_vector128< std::uint8_t > mm_shuffle_pattern2;
#else
    static const simd_vector128< std::uint8_t > mm_9;
    static const simd_vector128< std::uint8_t > mm_group1_mask;
    static const simd_vector128< std::uint8_t > mm_group2_mask;
    static const simd_vector128< std::uint8_t > mm_group3_mask;
#endif
};

template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_0F =
    {{ 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F }};
#if defined(BOOST_UUID_USE_SSSE3)
template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_shuffle_pattern1 =
    {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x80, 0x08, 0x09, 0x0A, 0x0B, 0x80, 0x0C, 0x0D }};
template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_shuffle_pattern2 =
    {{ 0x00, 0x01, 0x80, 0x02, 0x03, 0x04, 0x05, 0x80, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D }};
#else
template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_9 =
    {{ 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09 }};
template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_group1_mask =
    {{ 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_group2_mask =
    {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
template< typename T >
const simd_vector128< std::uint8_t > to_chars_simd_constants< T >::mm_group3_mask =
    {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 }};
#endif

/*!
 * Converts UUID to a string of 36 characters, where the first 32 characters are returned in mm_chars1 and mm_chars2.
 * When SSSE3 is supported, last 4 characters are returned in the highest 32 bits of mm_chars3, otherwise in the lowest 32 bits.
 */
BOOST_FORCEINLINE void to_chars_simd_core
(
    const std::uint8_t* data,
#if defined(BOOST_UUID_USE_SSSE3)
    __m128i const& mm_char_table,
#else
    __m128i const& mm_char_0_add, __m128i const& mm_char_a_add,
#endif
    __m128i const& mm_char_dash,
    __m128i& mm_chars1, __m128i& mm_chars2, __m128i& mm_chars3
) noexcept
{
    using constants = uuids::detail::to_chars_simd_constants< void >;

    __m128i mm_input = _mm_loadu_si128(reinterpret_cast< const __m128i* >(data));

    // Split half-bytes
    __m128i mm_input_hi = _mm_and_si128(_mm_srli_epi32(mm_input, 4), constants::mm_0F);
    __m128i mm_input_lo = _mm_and_si128(mm_input, constants::mm_0F);

    // Stringize each of the halves
#if defined(BOOST_UUID_USE_SSSE3)
    mm_input_hi = _mm_shuffle_epi8(mm_char_table, mm_input_hi);
    mm_input_lo = _mm_shuffle_epi8(mm_char_table, mm_input_lo);
#else
    {
        __m128i mm_add_mask_hi = _mm_cmpgt_epi8(mm_input_hi, constants::mm_9);
        __m128i mm_add_mask_lo = _mm_cmpgt_epi8(mm_input_lo, constants::mm_9);

        __m128i mm_add_hi = _mm_add_epi8(mm_char_0_add, _mm_and_si128(mm_add_mask_hi, mm_char_a_add));
        __m128i mm_add_lo = _mm_add_epi8(mm_char_0_add, _mm_and_si128(mm_add_mask_lo, mm_char_a_add));

        mm_input_hi = _mm_add_epi8(mm_input_hi, mm_add_hi);
        mm_input_lo = _mm_add_epi8(mm_input_lo, mm_add_lo);
    }
#endif

    // Join them back together
    __m128i mm_1 = _mm_unpacklo_epi8(mm_input_hi, mm_input_lo);
    __m128i mm_2 = _mm_unpackhi_epi8(mm_input_hi, mm_input_lo);

#if defined(BOOST_UUID_USE_SSSE3)
    // Insert dashes at positions 8, 13, 18 and 23
    //        mm_1             mm_2
    // |0123456789abcdef|0123456789abcdef|
    // |01234567-89ab-cd|ef-0123-456789ab|
    //
    // Note that the last "cdef" characters are already available at the end of mm_2
    mm_chars1 = _mm_shuffle_epi8(mm_1, constants::mm_shuffle_pattern1);
    mm_chars2 = _mm_shuffle_epi8(_mm_alignr_epi8(mm_2, mm_1, 14), constants::mm_shuffle_pattern2);

    mm_chars1 = _mm_max_epu8(mm_chars1, mm_char_dash);
    mm_chars2 = _mm_max_epu8(mm_chars2, mm_char_dash);
    mm_chars3 = mm_2;
#else
    // Split groups of characters between dashes and shift them into their places
    // mm_middle: |89abcdef01234567|
    // mm_group1: |Z89abZZZZZZZZZZZ|
    // mm_group2: |ZZZZZZcdefZZZZZZ|
    // mm_group3: |ZZZZZZZZZZZ0123Z|
    __m128i mm_middle = _mm_castpd_si128(_mm_shuffle_pd(_mm_castsi128_pd(mm_1), _mm_castsi128_pd(mm_2), _MM_SHUFFLE2(0, 1)));
    __m128i mm_group1 = _mm_slli_epi64(mm_middle, 8);
    __m128i mm_group2 = _mm_slli_si128(mm_middle, 2);
    __m128i mm_group3 = _mm_slli_epi64(mm_middle, 24);
    mm_group1 = _mm_and_si128(mm_group1, constants::mm_group1_mask);
    mm_group2 = _mm_and_si128(mm_group2, constants::mm_group2_mask);
    mm_group3 = _mm_and_si128(mm_group3, constants::mm_group3_mask);

    // Merge them back and insert dashes
    // mm_middle: |-89ab-cdef-0123-|
    mm_middle = _mm_or_si128(_mm_or_si128(mm_group1, mm_group2), mm_group3);
    mm_middle = _mm_max_epu8(mm_middle, mm_char_dash);

    // mm_2:      |cdef0123456789ab|
    mm_2 = _mm_shuffle_epi32(mm_2, _MM_SHUFFLE(2, 1, 0, 3));

    mm_chars1 = _mm_unpacklo_epi64(mm_1, mm_middle);
    mm_chars2 = _mm_unpackhi_epi64(mm_middle, mm_2);
    mm_chars3 = mm_2;
#endif
}

#if defined(BOOST_MSVC)
#pragma warning(push)
// conditional expression is constant
#pragma warning(disable: 4127)
#endif

template< typename Char >
BOOST_FORCEINLINE Char* to_chars_simd(uuid const& u, Char* out) noexcept
{
    using char_constants = uuids::detail::to_chars_simd_char_constants< Char >;

    __m128i mm_chars1, mm_chars2, mm_chars3;
    uuids::detail::to_chars_simd_core
    (
        u.data(),
#if defined(BOOST_UUID_USE_SSSE3)
        char_constants::mm_char_table,
#else
        char_constants::mm_char_0_add,
        char_constants::mm_char_a_add,
#endif
        char_constants::mm_char_dash,
        mm_chars1, mm_chars2, mm_chars3
    );

    static_assert(sizeof(Char) == 1u || sizeof(Char) == 2u || sizeof(Char) == 4u, "Boost.UUID: Unsupported output character type for to_chars");
    BOOST_IF_CONSTEXPR (sizeof(Char) == 1u)
    {
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out), mm_chars1);
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 16), mm_chars2);
        detail::store_native_u32
        (
            out + 32,
#if defined(BOOST_UUID_USE_SSE41)
            static_cast< std::uint32_t >(_mm_extract_epi32(mm_chars3, 3))
#elif defined(BOOST_UUID_USE_SSSE3)
            static_cast< std::uint32_t >(_mm_cvtsi128_si32(_mm_srli_si128(mm_chars3, 12)))
#else
            static_cast< std::uint32_t >(_mm_cvtsi128_si32(mm_chars3))
#endif
        );
    }
    else BOOST_IF_CONSTEXPR (sizeof(Char) == 2u)
    {
        const __m128i mm_0 = _mm_setzero_si128();
#if defined(BOOST_UUID_USE_AVX2)
        _mm256_storeu_si256(reinterpret_cast< __m256i* >(out), _mm256_cvtepu8_epi16(mm_chars1));
        _mm256_storeu_si256(reinterpret_cast< __m256i* >(out + 16), _mm256_cvtepu8_epi16(mm_chars2));
#else
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out), _mm_unpacklo_epi8(mm_chars1, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 8), _mm_unpackhi_epi8(mm_chars1, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 16), _mm_unpacklo_epi8(mm_chars2, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 24), _mm_unpackhi_epi8(mm_chars2, mm_0));
#endif
#if defined(BOOST_UUID_USE_SSE41) && (defined(__x86_64__) || defined(_M_X64))
        detail::store_native_u64(out + 32, static_cast< std::uint64_t >(_mm_extract_epi64(_mm_unpackhi_epi8(mm_chars3, mm_0), 1)));
#elif defined(BOOST_UUID_USE_SSSE3)
        _mm_storeh_pd(reinterpret_cast< BOOST_MAY_ALIAS double* >(out + 32), _mm_castsi128_pd(_mm_unpackhi_epi8(mm_chars3, mm_0)));
#else
        _mm_storel_epi64(reinterpret_cast< __m128i* >(out + 32), _mm_unpacklo_epi8(mm_chars3, mm_0));
#endif
    }
    else
    {
        const __m128i mm_0 = _mm_setzero_si128();
#if defined(BOOST_UUID_USE_AVX512_V1) && defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
        // Slower than the AVX2 version below on Intel Golden Cove. Perhaps, it will become beneficial on newer microarchitectures.
        _mm512_storeu_epi32(out, _mm512_cvtepu8_epi32(mm_chars1));
        _mm512_storeu_epi32(out + 16, _mm512_cvtepu8_epi32(mm_chars2));
#elif defined(BOOST_UUID_USE_AVX2)
        _mm256_storeu_si256(reinterpret_cast< __m256i* >(out), _mm256_cvtepu8_epi32(mm_chars1));
        _mm256_storeu_si256(reinterpret_cast< __m256i* >(out + 8), _mm256_cvtepu8_epi32(_mm_unpackhi_epi64(mm_chars1, mm_chars1)));
        _mm256_storeu_si256(reinterpret_cast< __m256i* >(out + 16), _mm256_cvtepu8_epi32(mm_chars2));
        _mm256_storeu_si256(reinterpret_cast< __m256i* >(out + 24), _mm256_cvtepu8_epi32(_mm_unpackhi_epi64(mm_chars2, mm_chars2)));
#else
        __m128i mm = _mm_unpacklo_epi8(mm_chars1, mm_0);
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out), _mm_unpacklo_epi16(mm, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 4), _mm_unpackhi_epi16(mm, mm_0));
        mm = _mm_unpackhi_epi8(mm_chars1, mm_0);
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 8), _mm_unpacklo_epi16(mm, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 12), _mm_unpackhi_epi16(mm, mm_0));
        mm = _mm_unpacklo_epi8(mm_chars2, mm_0);
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 16), _mm_unpacklo_epi16(mm, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 20), _mm_unpackhi_epi16(mm, mm_0));
        mm = _mm_unpackhi_epi8(mm_chars2, mm_0);
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 24), _mm_unpacklo_epi16(mm, mm_0));
        _mm_storeu_si128(reinterpret_cast< __m128i* >(out + 28), _mm_unpackhi_epi16(mm, mm_0));
#endif
        _mm_storeu_si128
        (
            reinterpret_cast< __m128i* >(out + 32),
#if defined(BOOST_UUID_USE_SSSE3)
            _mm_unpackhi_epi16(_mm_unpackhi_epi8(mm_chars3, mm_0), mm_0)
#else
            _mm_unpacklo_epi16(_mm_unpacklo_epi8(mm_chars3, mm_0), mm_0)
#endif
        );
    }

    return out + 36;
}

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

} // namespace detail
} // namespace uuids
} // namespace boost

#endif // defined(BOOST_UUID_USE_SSE2)

#endif // BOOST_UUID_DETAIL_TO_CHARS_X86_HPP_INCLUDED
