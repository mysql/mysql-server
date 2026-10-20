#ifndef BOOST_UUID_DETAIL_FROM_CHARS_X86_HPP_INCLUDED
#define BOOST_UUID_DETAIL_FROM_CHARS_X86_HPP_INCLUDED

// Copyright 2025 Andrey Semashev
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/config.hpp>

#if defined(BOOST_UUID_USE_SSE2)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/detail/endian.hpp>
#include <boost/uuid/detail/from_chars_result.hpp>
#include <boost/uuid/detail/simd_vector.hpp>

#if defined(BOOST_UUID_REPORT_IMPLEMENTATION)
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_UUID_USE_AVX10_1)
BOOST_PRAGMA_MESSAGE( "Using from_chars_x86.hpp, AVX10.1" )

#elif defined(BOOST_UUID_USE_AVX512_V1)
BOOST_PRAGMA_MESSAGE( "Using from_chars_x86.hpp, AVX512v1" )

#elif defined(BOOST_UUID_USE_AVX)
BOOST_PRAGMA_MESSAGE( "Using from_chars_x86.hpp, AVX" )

#elif defined(BOOST_UUID_USE_SSE41)
BOOST_PRAGMA_MESSAGE( "Using from_chars_x86.hpp, SSE4.1" )

#elif defined(BOOST_UUID_USE_SSSE3)
BOOST_PRAGMA_MESSAGE( "Using from_chars_x86.hpp, SSSE3" )

#else
BOOST_PRAGMA_MESSAGE( "Using from_chars_x86.hpp, SSE2" )

#endif
#endif // #if defined(BOOST_UUID_REPORT_IMPLEMENTATION)

#if defined(BOOST_UUID_USE_AVX)
#include <immintrin.h>
#elif defined(BOOST_UUID_USE_SSE41)
#include <smmintrin.h>
#elif defined(BOOST_UUID_USE_SSSE3)
#include <tmmintrin.h>
#else
#include <emmintrin.h>
#endif
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#endif

// Unlike the legacy SSE4.1 pblendvb instruction, the VEX-coded vpblendvb is slow on Intel Lion Cove, Skymont and older.
// Newer microarchitectures are unknown at the time of this writing. Also, on Intel Haswell/Broadwell, even the SSE4.1
// pblendvb is slow.
#if !defined(BOOST_UUID_FROM_CHARS_X86_SLOW_PBLENDVB) && \
    (defined(__tune_haswell__) || defined(__tune_broadwell__) || defined(BOOST_UUID_USE_AVX))
#define BOOST_UUID_FROM_CHARS_X86_SLOW_PBLENDVB
#endif

#if !defined(BOOST_UUID_FROM_CHARS_X86_USE_PBLENDVB) && defined(BOOST_UUID_USE_SSE41) && !defined(BOOST_UUID_FROM_CHARS_X86_SLOW_PBLENDVB)
#define BOOST_UUID_FROM_CHARS_X86_USE_PBLENDVB
#endif

#if defined(BOOST_UUID_USE_AVX512_V1) || (defined(BOOST_UUID_USE_SSE41) && defined(BOOST_UUID_FROM_CHARS_X86_USE_PBLENDVB)) || !defined(BOOST_UUID_USE_SSSE3)
#define BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS
#endif

namespace boost {
namespace uuids {
namespace detail {

//! Returns the number of trailing zero bits in a non-zero input integer
BOOST_FORCEINLINE std::uint32_t countr_zero_nz(std::uint32_t n) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return static_cast< std::uint32_t >(__builtin_ctz(n));
#elif defined(_MSC_VER) && !defined(__clang__)
    unsigned long index;
    _BitScanForward(&index, n);
    return static_cast< std::uint32_t >(index);
#else
    std::uint32_t index = 0u;
    if ((n & 0xFFFF) == 0u)
    {
        n >>= 16u;
        index += 16u;
    }
    if ((n & 0xFF) == 0u)
    {
        n >>= 8u;
        index += 8u;
    }
    if ((n & 0xF) == 0u)
    {
        n >>= 4u;
        index += 4u;
    }
    if ((n & 0x3) == 0u)
    {
        n >>= 2u;
        index += 2u;
    }
    if ((n & 0x1) == 0u)
    {
        index += 1u;
    }
    return index;
#endif
}

template<
    typename Char,
    bool IsCharASCIICompatible = ('0' == 0x30 && '9' == 0x39 && 'A' == 0x41 && 'F' == 0x46 && 'a' == 0x61 && 'f' == 0x66 && '-' == 0x2D),
    bool IsWCharASCIICompatible = (L'0' == 0x30 && L'9' == 0x39 && L'A' == 0x41 && L'F' == 0x46 && L'a' == 0x61 && L'f' == 0x66 && L'-' == 0x2D)
>
struct from_chars_simd_char_constants
{
    static const simd_vector128< std::uint8_t > mm_expected_dashes;

    static constexpr std::uint8_t char_code2 = 0x61; // 'a' in ASCII
    static constexpr std::uint8_t char_code2_sub = static_cast< std::uint8_t >(char_code2 - 10u);
    static constexpr std::uint8_t char_code1 = 0x41; // 'A' in ASCII
    static constexpr std::uint8_t char_code1_sub = static_cast< std::uint8_t >(char_code1 - 10u);
    static constexpr std::uint8_t char_code0 = 0x30; // '0' in ASCII
    static constexpr std::uint8_t char_code0_sub = char_code0;

    static constexpr std::uint32_t char_code_sub =
        (static_cast< std::uint32_t >(char_code0_sub) << 16u) | (static_cast< std::uint32_t >(char_code1_sub) << 8u) | char_code2_sub;

    static const simd_vector128< std::uint8_t > mm_char_code2_cmp;
    static const simd_vector128< std::uint8_t > mm_char_code1_cmp;

#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
    static const simd_vector128< std::uint8_t > mm_char_code2_sub;
    static const simd_vector128< std::uint8_t > mm_char_code1_sub;
    static const simd_vector128< std::uint8_t > mm_char_code0_sub;
#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
};

template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_expected_dashes =
    {{ 0x2D, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x2D }}; // 0x2D is '-' in ASCII

template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_code2_cmp =
{{
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u)
}};

template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_code1_cmp =
{{
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u)
}};

#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)

template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_code2_sub =
{{
    char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub,
    char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub
}};

template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_code1_sub =
{{
    char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub,
    char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub
}};

template< typename Char, bool IsCharASCIICompatible, bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< Char, IsCharASCIICompatible, IsWCharASCIICompatible >::mm_char_code0_sub =
{{
    char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub,
    char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub
}};

#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)

template< bool IsWCharASCIICompatible >
struct from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >
{
    static_assert(static_cast< std::int8_t >('0') > -128 && static_cast< std::int8_t >('A') > -128 && static_cast< std::int8_t >('a') > -128,
        "Boost.UUID: Unsupported char encoding, hexadecimal character codes are expected to be greater than -128");

    static const simd_vector128< std::uint8_t > mm_expected_dashes;

    static constexpr std::uint8_t char_code2 = static_cast< std::uint8_t >
    (
        static_cast< std::int8_t >('a') > static_cast< std::int8_t >('A') ?
        (
            static_cast< std::int8_t >('a') > static_cast< std::int8_t >('0') ? 'a' : '0'
        ) :
        (
            static_cast< std::int8_t >('A') > static_cast< std::int8_t >('0') ? 'A' : '0'
        )
    );
    static constexpr std::uint8_t char_code2_sub = char_code2 == static_cast< std::uint8_t >('0') ?
        static_cast< std::uint8_t >('0') : static_cast< std::uint8_t >(char_code2 - 10u);

    static constexpr std::uint8_t char_code1 = static_cast< std::uint8_t >
    (
        static_cast< std::int8_t >('a') > static_cast< std::int8_t >('A') ?
        (
            static_cast< std::int8_t >('a') < static_cast< std::int8_t >('0') ? 'a' : '0'
        ) :
        (
            static_cast< std::int8_t >('A') < static_cast< std::int8_t >('0') ? 'A' : '0'
        )
    );
    static constexpr std::uint8_t char_code1_sub = char_code1 == static_cast< std::uint8_t >('0') ?
        static_cast< std::uint8_t >('0') : static_cast< std::uint8_t >(char_code1 - 10u);

    static constexpr std::uint8_t char_code0 = static_cast< std::uint8_t >
    (
        static_cast< std::int8_t >('a') < static_cast< std::int8_t >('A') ?
        (
            static_cast< std::int8_t >('a') < static_cast< std::int8_t >('0') ? 'a' : '0'
        ) :
        (
            static_cast< std::int8_t >('A') < static_cast< std::int8_t >('0') ? 'A' : '0'
        )
    );
    static constexpr std::uint8_t char_code0_sub = char_code0 == static_cast< std::uint8_t >('0') ?
        static_cast< std::uint8_t >('0') : static_cast< std::uint8_t >(char_code0 - 10u);

    static constexpr std::uint32_t char_code_sub =
        (static_cast< std::uint32_t >(char_code0_sub) << 16u) | (static_cast< std::uint32_t >(char_code1_sub) << 8u) | char_code2_sub;

    static const simd_vector128< std::uint8_t > mm_char_code2_cmp;
    static const simd_vector128< std::uint8_t > mm_char_code1_cmp;

#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
    static const simd_vector128< std::uint8_t > mm_char_code2_sub;
    static const simd_vector128< std::uint8_t > mm_char_code1_sub;
    static const simd_vector128< std::uint8_t > mm_char_code0_sub;
#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
};

template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_expected_dashes =
{{
    static_cast< std::uint8_t >('-'), 0x00, 0x00, 0x00, 0x00, static_cast< std::uint8_t >('-'), 0x00, 0x00,
    0x00, 0x00, static_cast< std::uint8_t >('-'), 0x00, 0x00, 0x00, 0x00, static_cast< std::uint8_t >('-')
}};

template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_code2_cmp =
{{
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u)
}};

template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_code1_cmp =
{{
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u)
}};

#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)

template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_code2_sub =
{{
    char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub,
    char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub
}};

template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_code1_sub =
{{
    char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub,
    char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub
}};

template< bool IsWCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< char, false, IsWCharASCIICompatible >::mm_char_code0_sub =
{{
    char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub,
    char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub
}};

#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)

template< bool IsCharASCIICompatible >
struct from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >
{
    static_assert(static_cast< wchar_t >(static_cast< std::uint8_t >(L'0')) == L'0' && static_cast< wchar_t >(static_cast< std::uint8_t >(L'9')) == L'9' &&
        static_cast< wchar_t >(static_cast< std::uint8_t >(L'a')) == L'a' && static_cast< wchar_t >(static_cast< std::uint8_t >(L'f')) == L'f' &&
        static_cast< wchar_t >(static_cast< std::uint8_t >(L'-')) == L'-',
        "Boost.UUID: Unsupported wchar_t encoding, hexadecimal and dash character codes are expected to be representable by a single byte");

    static_assert(static_cast< std::int8_t >(L'0') > -128 && static_cast< std::int8_t >(L'A') > -128 && static_cast< std::int8_t >(L'a') > -128,
        "Boost.UUID: Unsupported wchar_t encoding, hexadecimal character codes are expected to be greater than -128");

    static const simd_vector128< std::uint8_t > mm_expected_dashes;

    static constexpr std::uint8_t char_code2 = static_cast< std::uint8_t >
    (
        static_cast< std::int8_t >(L'a') > static_cast< std::int8_t >(L'A') ?
        (
            static_cast< std::int8_t >(L'a') > static_cast< std::int8_t >(L'0') ? L'a' : L'0'
        ) :
        (
            static_cast< std::int8_t >(L'A') > static_cast< std::int8_t >(L'0') ? L'A' : L'0'
        )
    );
    static constexpr std::uint8_t char_code2_sub = char_code2 == static_cast< std::uint8_t >(L'0') ?
        static_cast< std::uint8_t >(L'0') : static_cast< std::uint8_t >(char_code2 - 10u);

    static constexpr std::uint8_t char_code1 = static_cast< std::uint8_t >
    (
        static_cast< std::int8_t >(L'a') > static_cast< std::int8_t >(L'A') ?
        (
            static_cast< std::int8_t >(L'a') < static_cast< std::int8_t >(L'0') ? L'a' : L'0'
        ) :
        (
            static_cast< std::int8_t >(L'A') < static_cast< std::int8_t >(L'0') ? L'A' : L'0'
        )
    );
    static constexpr std::uint8_t char_code1_sub = char_code1 == static_cast< std::uint8_t >(L'0') ?
        static_cast< std::uint8_t >(L'0') : static_cast< std::uint8_t >(char_code1 - 10u);

    static constexpr std::uint8_t char_code0 = static_cast< std::uint8_t >
    (
        static_cast< std::int8_t >(L'a') < static_cast< std::int8_t >(L'A') ?
        (
            static_cast< std::int8_t >(L'a') < static_cast< std::int8_t >(L'0') ? L'a' : L'0'
        ) :
        (
            static_cast< std::int8_t >(L'A') < static_cast< std::int8_t >(L'0') ? L'A' : L'0'
        )
    );
    static constexpr std::uint8_t char_code0_sub = char_code0 == static_cast< std::uint8_t >(L'0') ?
        static_cast< std::uint8_t >(L'0') : static_cast< std::uint8_t >(char_code0 - 10u);

    static constexpr std::uint32_t char_code_sub =
        (static_cast< std::uint32_t >(char_code0_sub) << 16u) | (static_cast< std::uint32_t >(char_code1_sub) << 8u) | char_code2_sub;

    static const simd_vector128< std::uint8_t > mm_char_code2_cmp;
    static const simd_vector128< std::uint8_t > mm_char_code1_cmp;

#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
    static const simd_vector128< std::uint8_t > mm_char_code2_sub;
    static const simd_vector128< std::uint8_t > mm_char_code1_sub;
    static const simd_vector128< std::uint8_t > mm_char_code0_sub;
#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
};

template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_expected_dashes =
{{
    static_cast< std::uint8_t >(L'-'), 0x00, 0x00, 0x00, 0x00, static_cast< std::uint8_t >(L'-'), 0x00, 0x00,
    0x00, 0x00, static_cast< std::uint8_t >(L'-'), 0x00, 0x00, 0x00, 0x00, static_cast< std::uint8_t >(L'-')
}};

template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_code2_cmp =
{{
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u),
    static_cast< std::uint8_t >(char_code2 - 1u), static_cast< std::uint8_t >(char_code2 - 1u)
}};

template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_code1_cmp =
{{
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u),
    static_cast< std::uint8_t >(char_code1 - 1u), static_cast< std::uint8_t >(char_code1 - 1u)
}};

#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)

template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_code2_sub =
{{
    char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub,
    char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub, char_code2_sub
}};

template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_code1_sub =
{{
    char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub,
    char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub, char_code1_sub
}};

template< bool IsCharASCIICompatible >
const simd_vector128< std::uint8_t > from_chars_simd_char_constants< wchar_t, IsCharASCIICompatible, false >::mm_char_code0_sub =
{{
    char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub,
    char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub, char_code0_sub
}};

#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)


template< typename >
struct from_chars_simd_constants
{
    static const simd_vector128< std::uint8_t > mm_dashes_mask;

#if defined(BOOST_UUID_USE_AVX10_1) && defined(BOOST_UUID_FROM_CHARS_X86_USE_VPERMI2B)
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_pattern1;
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_pattern2;
#else
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_pattern1;
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_pattern2;
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_pattern3;
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_blend_mask1;
#if !defined(BOOST_UUID_USE_SSE41)
    static const simd_vector128< std::uint8_t > mm_split_half_bytes_blend_mask2;
#endif
#if !defined(BOOST_UUID_USE_SSSE3)
    static const simd_vector128< std::uint8_t > mm_split_half_byte_chars_mask;
#endif
#endif

    static const simd_vector128< std::uint8_t > mm_F0;

#if !defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
    static const simd_vector128< std::uint8_t > mm_2;
#endif
};

template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_dashes_mask =
    {{ 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF }};
#if defined(BOOST_UUID_USE_AVX10_1) && defined(BOOST_UUID_FROM_CHARS_X86_USE_VPERMI2B)
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_pattern1 =
    {{ 0x01, 0x03, 0x05, 0x07, 0x0A, 0x0C, 0x0F, 0x11, 0x00, 0x02, 0x04, 0x06, 0x09, 0x0B, 0x0E, 0x10 }};
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_pattern2 =
    {{ 0x04, 0x06, 0x09, 0x0B, 0x0D, 0x0F, 0x11, 0x13, 0x03, 0x05, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12 }};
#else
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_pattern1 =
    {{ 0x01, 0x03, 0x05, 0x07, 0x0A, 0x0C, 0x0F, 0x80, 0x00, 0x02, 0x04, 0x06, 0x09, 0x0B, 0x0E, 0x80 }};
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_pattern2 =
    {{ 0x04, 0x06, 0x09, 0x0B, 0x0D, 0x0F, 0x80, 0x01, 0x03, 0x05, 0x08, 0x0A, 0x0C, 0x0E, 0x80, 0x00 }};
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_pattern3 =
    {{ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0x03, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x02 }};
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_blend_mask1 =
    {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF }};
#if !defined(BOOST_UUID_USE_SSE41)
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_bytes_blend_mask2 =
    {{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00 }};
#endif
#if !defined(BOOST_UUID_USE_SSSE3)
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_split_half_byte_chars_mask =
    {{ 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00 }};
#endif
#endif
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_F0 =
    {{ 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0 }};
#if !defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
template< typename T >
const simd_vector128< std::uint8_t > from_chars_simd_constants< T >::mm_2 =
    {{ 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 }};
#endif


#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
// array subscript N is outside array bounds of '<array>'
// In the partial loads below, masked loads may be used with pointers beyond the input array of characters.
// In all such instances, the actual loads are prevented by the generated masks, don't generate
// hardware faults and therefore are safe.
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

template< typename Char, std::size_t Size = sizeof(Char) >
struct from_chars_simd_load_traits;

template< typename Char >
struct from_chars_simd_load_traits< Char, 1u >
{
    static BOOST_FORCEINLINE __m128i load_packed_16(const Char* p) noexcept
    {
        return _mm_loadu_si128(reinterpret_cast< const __m128i* >(p));
    }

    static BOOST_FORCEINLINE __m128i load_packed_4(const Char* p) noexcept
    {
        return _mm_cvtsi32_si128(static_cast< int >(detail::load_native_u32(p)));
    }

    static BOOST_FORCEINLINE __m128i load_packed_n(const Char* p, unsigned int n) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
        return _mm_maskz_loadu_epi8(_cvtu32_mask16((static_cast< std::uint32_t >(1u) << n) - 1u), p);
#else
        std::uint32_t chars = 0u;
        p += n;
        if ((n & 1u) != 0u)
        {
            p -= 1;
            chars = *reinterpret_cast< BOOST_MAY_ALIAS std::uint8_t const* >(p);
        }

        if ((n & 2u) != 0u)
        {
            p -= 2;
            chars = (chars << 16u) | static_cast< std::uint32_t >(detail::load_native_u16(p));
        }

        __m128i mm_chars = _mm_cvtsi32_si128(static_cast< int >(chars));
        if ((n & 4u) != 0u)
        {
            p -= 4;
            mm_chars = _mm_unpacklo_epi32(_mm_cvtsi32_si128(static_cast< int >(detail::load_native_u32(p))), mm_chars);
        }

        if ((n & 8u) != 0u)
        {
            p -= 8;
            mm_chars = _mm_unpacklo_epi64(_mm_loadl_epi64(reinterpret_cast< const __m128i* >(p)), mm_chars);
        }

        return mm_chars;
#endif
    }
};

template< typename Char >
struct from_chars_simd_load_traits< Char, 2u >
{
    static BOOST_FORCEINLINE __m128i load_packed_16(const Char* p) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
        return _mm256_cvtusepi16_epi8(_mm256_loadu_si256(reinterpret_cast< const __m256i* >(p)));
#else
        return _mm_packus_epi16(_mm_loadu_si128(reinterpret_cast< const __m128i* >(p)), _mm_loadu_si128(reinterpret_cast< const __m128i* >(p + 8)));
#endif
    }

    static BOOST_FORCEINLINE __m128i load_packed_4(const Char* p) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
        return _mm_cvtusepi16_epi8(_mm_loadl_epi64(reinterpret_cast< const __m128i* >(p)));
#else
        return _mm_packus_epi16(_mm_loadl_epi64(reinterpret_cast< const __m128i* >(p)), _mm_setzero_si128());
#endif
    }

    static BOOST_FORCEINLINE __m128i load_packed_n(const Char* p, unsigned int n) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
        return _mm256_cvtusepi16_epi8(_mm256_maskz_loadu_epi16(_cvtu32_mask16((static_cast< std::uint32_t >(1u) << n) - 1u), p));
#else
        __m128i mm_chars1 = _mm_setzero_si128();
        __m128i mm_chars2 = _mm_setzero_si128();
        p += n;
        if ((n & 1u) != 0u)
        {
            p -= 1;
            mm_chars1 = _mm_cvtsi32_si128(detail::load_native_u16(p));
        }

        if ((n & 2u) != 0u)
        {
            p -= 2;
            mm_chars1 = _mm_unpacklo_epi32(_mm_cvtsi32_si128(static_cast< int >(detail::load_native_u32(p))), mm_chars1);
        }

        if ((n & 4u) != 0u)
        {
            p -= 4;
            mm_chars1 = _mm_unpacklo_epi64(_mm_loadl_epi64(reinterpret_cast< const __m128i* >(p)), mm_chars1);
        }

        if ((n & 8u) != 0u)
        {
            p -= 8;
            mm_chars2 = mm_chars1;
            mm_chars1 = _mm_loadu_si128(reinterpret_cast< const __m128i* >(p));
        }

        return _mm_packus_epi16(mm_chars1, mm_chars2);
#endif
    }
};

template< typename Char >
struct from_chars_simd_load_traits< Char, 4u >
{
#if !defined(BOOST_UUID_USE_SSE41) && defined(BOOST_UUID_USE_SSSE3)
    static const simd_vector128< std::uint8_t > mm_deinterleave_epi16_pattern;
#endif

    static BOOST_FORCEINLINE __m128i mm_packus_epi32(__m128i mm1, __m128i mm2) noexcept
    {
#if defined(BOOST_UUID_USE_SSE41)
        return _mm_packus_epi32(mm1, mm2);
#else // defined(BOOST_UUID_USE_SSE41)
#if defined(BOOST_UUID_USE_SSSE3)
        mm1 = _mm_shuffle_epi8(mm1, mm_deinterleave_epi16_pattern);
        mm2 = _mm_shuffle_epi8(mm2, mm_deinterleave_epi16_pattern);

        __m128i mm_lo = _mm_unpacklo_epi64(mm1, mm2);
        __m128i mm_hi = _mm_unpackhi_epi64(mm1, mm2);
#else // defined(BOOST_UUID_USE_SSSE3)
        mm1 = _mm_shufflelo_epi16(mm1, _MM_SHUFFLE(3, 1, 2, 0));
        mm2 = _mm_shufflelo_epi16(mm2, _MM_SHUFFLE(3, 1, 2, 0));
        mm1 = _mm_shufflehi_epi16(mm1, _MM_SHUFFLE(3, 1, 2, 0));
        mm2 = _mm_shufflehi_epi16(mm2, _MM_SHUFFLE(3, 1, 2, 0));

        __m128i mm_lo = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(mm1), _mm_castsi128_ps(mm2), _MM_SHUFFLE(2, 0, 2, 0)));
        __m128i mm_hi = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(mm1), _mm_castsi128_ps(mm2), _MM_SHUFFLE(3, 1, 3, 1)));
#endif // defined(BOOST_UUID_USE_SSSE3)
        const __m128i mm_0 = _mm_setzero_si128();
        const __m128i mm_FF = _mm_cmpeq_epi32(mm_0, mm_0);

        __m128i mm_sat = _mm_xor_si128(_mm_cmpeq_epi16(mm_hi, mm_0), mm_FF);
        return _mm_or_si128(mm_lo, mm_sat);
#endif // defined(BOOST_UUID_USE_SSE41)
    }

    static BOOST_FORCEINLINE __m128i load_packed_16(const Char* p) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
#if defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
        // Slower than the 256-bit version below on Intel Golden Cove.
        return _mm512_cvtusepi32_epi8(_mm512_loadu_epi32(p));
#else // defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
        __m128i mm1 = _mm256_cvtusepi32_epi8(_mm256_loadu_si256(reinterpret_cast< const __m256i* >(p)));
        __m128i mm2 = _mm256_cvtusepi32_epi8(_mm256_loadu_si256(reinterpret_cast< const __m256i* >(p + 8)));
        return _mm_unpacklo_epi64(mm1, mm2);
#endif // defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
#else
        __m128i mm1 = mm_packus_epi32(_mm_loadu_si128(reinterpret_cast< const __m128i* >(p)), _mm_loadu_si128(reinterpret_cast< const __m128i* >(p + 4)));
        __m128i mm2 = mm_packus_epi32(_mm_loadu_si128(reinterpret_cast< const __m128i* >(p + 8)), _mm_loadu_si128(reinterpret_cast< const __m128i* >(p + 12)));
        return _mm_packus_epi16(mm1, mm2);
#endif
    }

    static BOOST_FORCEINLINE __m128i load_packed_4(const Char* p) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
        return _mm_cvtusepi32_epi8(_mm_loadu_si128(reinterpret_cast< const __m128i* >(p)));
#else
        __m128i mm1 = _mm_loadu_si128(reinterpret_cast< const __m128i* >(p));
        __m128i mm2 = _mm_setzero_si128();
        return _mm_packus_epi16(mm_packus_epi32(mm1, mm2), mm2);
#endif
    }

    static BOOST_FORCEINLINE __m128i load_packed_n(const Char* p, unsigned int n) noexcept
    {
#if defined(BOOST_UUID_USE_AVX512_V1)
        const std::uint32_t mask = (static_cast< std::uint32_t >(1u) << n) - 1u;
#if defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
        // Slower than the 256-bit version below on Intel Golden Cove.
        return _mm512_cvtusepi32_epi8(_mm512_maskz_loadu_epi32(_cvtu32_mask16(mask), p));
#else // defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
        __m128i mm1 = _mm256_cvtusepi32_epi8(_mm256_maskz_loadu_epi32(_cvtu32_mask8(mask & 0xFF), p));
        __m128i mm2 = _mm256_cvtusepi32_epi8(_mm256_maskz_loadu_epi32(_cvtu32_mask8(mask >> 8u), p + 8));
        return _mm_unpacklo_epi64(mm1, mm2);
#endif // defined(BOOST_UUID_TO_FROM_CHARS_X86_USE_ZMM)
#else
        __m128i mm_chars1 = _mm_setzero_si128();
        __m128i mm_chars2 = _mm_setzero_si128();
        __m128i mm_chars3 = _mm_setzero_si128();
        __m128i mm_chars4 = _mm_setzero_si128();
        p += n;
        if ((n & 1u) != 0u)
        {
            p -= 1;
            mm_chars1 = _mm_cvtsi32_si128(static_cast< int >(detail::load_native_u32(p)));
        }

        if ((n & 2u) != 0u)
        {
            p -= 2;
            mm_chars1 = _mm_unpacklo_epi64(_mm_loadl_epi64(reinterpret_cast< const __m128i* >(p)), mm_chars1);
        }

        if ((n & 4u) != 0u)
        {
            p -= 4;
            mm_chars2 = mm_chars1;
            mm_chars1 = _mm_loadu_si128(reinterpret_cast< const __m128i* >(p));
        }

        if ((n & 8u) != 0u)
        {
            p -= 8;
            mm_chars4 = mm_chars3;
            mm_chars3 = mm_chars2;
            mm_chars1 = _mm_loadu_si128(reinterpret_cast< const __m128i* >(p));
            mm_chars2 = _mm_loadu_si128(reinterpret_cast< const __m128i* >(p + 4));
        }
        mm_chars1 = mm_packus_epi32(mm_chars1, mm_chars2);
        mm_chars3 = mm_packus_epi32(mm_chars3, mm_chars4);
        return _mm_packus_epi16(mm_chars1, mm_chars3);
#endif
    }
};

#if !defined(BOOST_UUID_USE_SSE41) && defined(BOOST_UUID_USE_SSSE3)
template< typename Char >
const simd_vector128< std::uint8_t > from_chars_simd_load_traits< Char, 4u >::mm_deinterleave_epi16_pattern =
    {{ 0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0C, 0x0D, 0x02, 0x03, 0x06, 0x07, 0x0A, 0x0B, 0x0E, 0x0F }};
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

/*!
 * Converts a string of 36 hexadecimal UUID characters in `mm_charsN` (with the last 4 characters in the lowest 32 bits of `mm_chars3`)
 * to a 16-byte binary value and, if successful, stores it into `data`. If not successful, stores the failure character position
 * to `end_pos` and error code to `ec`.
 */
BOOST_FORCEINLINE void from_chars_simd_core
(
    __m128i mm_chars1, __m128i mm_chars2, __m128i mm_chars3,
    __m128i const& mm_expected_dashes,
    __m128i const& mm_char_code1_cmp,
    __m128i const& mm_char_code2_cmp,
#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
    __m128i const& mm_char_code0_sub,
    __m128i const& mm_char_code1_sub,
    __m128i const& mm_char_code2_sub,
#else
    std::uint32_t char_code_sub,
#endif
    std::uint8_t* data, unsigned int& end_pos, from_chars_error& ec
)
{
    using constants = uuids::detail::from_chars_simd_constants< void >;

    //      mm_chars1        mm_chars2        mm_chars3
    // |01234567-89ab-cd|ef-0123-456789ab|cdefXXXXXXXXXXXX|
    //
    // Check if dashes are in the expected positions
    //
    //      mm_middle
    // |-89ab-cdef-0123-|
    const __m128i mm_middle = _mm_castpd_si128(_mm_shuffle_pd(_mm_castsi128_pd(mm_chars1), _mm_castsi128_pd(mm_chars2), _MM_SHUFFLE2(0, 1)));
    {
#if defined(BOOST_UUID_USE_SSE41)
        if (BOOST_UNLIKELY(!_mm_test_all_zeros(_mm_xor_si128(mm_middle, mm_expected_dashes), constants::mm_dashes_mask)))
#else
        __m128i mm_dashes = _mm_and_si128(mm_middle, constants::mm_dashes_mask);
        std::uint32_t dash_mask = static_cast< std::uint32_t >(_mm_movemask_epi8(_mm_cmpeq_epi8(mm_dashes, mm_expected_dashes)));
        if (BOOST_UNLIKELY(dash_mask != 0xFFFF))
#endif
        {
            // Some of the dashes are missing
#if defined(BOOST_UUID_USE_SSE41)
            __m128i mm_dashes = _mm_and_si128(mm_middle, constants::mm_dashes_mask);
            std::uint32_t dash_mask = static_cast< std::uint32_t >(_mm_movemask_epi8(_mm_cmpeq_epi8(mm_dashes, mm_expected_dashes)));
#endif
            unsigned int pos = detail::countr_zero_nz(~dash_mask) + 8u;
            if (pos < end_pos)
            {
                end_pos = pos;
                ec = from_chars_error::dash_expected;
            }
        }
    }

    // Remove the dashes, deinterleave low and high half-byte digit characters
#if defined(BOOST_UUID_USE_AVX10_1) && defined(BOOST_UUID_FROM_CHARS_X86_USE_VPERMI2B)
    // Note: This code path is disabled by default, unless BOOST_UUID_FROM_CHARS_X86_USE_VPERMI2B is defined, because vpermi2b/vpermt2b
    //       instructions are slow on Intel Golden Cove and older microarchitectures, making the alternative version below faster.
    //       This code path may still be beneficial on AMD CPUs or when Intel optimizes vpermi2b/vpermt2b.
    // mm_chars1: |02468ace13579bdf|
    // mm_chars2: |02468ace13579bdf|
    mm_chars1 = _mm_permutex2var_epi8(mm_chars1, constants::mm_split_half_bytes_pattern1, mm_chars2);
    mm_chars2 = _mm_permutex2var_epi8(mm_chars2, constants::mm_split_half_bytes_pattern2, mm_chars3);

    // Group half-byte characters
    __m128i mm_lo = _mm_unpacklo_epi64(mm_chars1, mm_chars2);
    __m128i mm_hi = _mm_unpackhi_epi64(mm_chars1, mm_chars2);
#elif defined(BOOST_UUID_USE_SSSE3)
    // mm_chars1: |02468acZ13579bdZ|
    // mm_chars2: |02468aZe13579bZf|
    // mm_chars3: |ZZZZZZceZZZZZZdf|
    mm_chars1 = _mm_shuffle_epi8(mm_chars1, constants::mm_split_half_bytes_pattern1);
    mm_chars2 = _mm_shuffle_epi8(mm_chars2, constants::mm_split_half_bytes_pattern2);
    mm_chars3 = _mm_shuffle_epi8(mm_chars3, constants::mm_split_half_bytes_pattern3);

    // mm_chars1: |02468ace13579bdf|
    // mm_chars2: |02468ace13579bdf|
#if defined(BOOST_UUID_USE_AVX512_V1)
    // Avoid using vpblendvb, which is slow on Intel
    mm_chars1 = _mm_ternarylogic_epi64(mm_chars1, mm_chars2, constants::mm_split_half_bytes_blend_mask1, 0xD8); // (_MM_TERNLOG_A & ~_MM_TERNLOG_C) | (_MM_TERNLOG_B & _MM_TERNLOG_C)
#elif defined(BOOST_UUID_USE_SSE41) && defined(BOOST_UUID_FROM_CHARS_X86_USE_PBLENDVB)
    mm_chars1 = _mm_blendv_epi8(mm_chars1, mm_chars2, constants::mm_split_half_bytes_blend_mask1);
#else
    mm_chars1 = _mm_or_si128(mm_chars1, _mm_and_si128(mm_chars2, constants::mm_split_half_bytes_blend_mask1));
#endif
#if defined(BOOST_UUID_USE_SSE41)
    mm_chars2 = _mm_blend_epi16(mm_chars2, mm_chars3, 0x88);
#else
    mm_chars2 = _mm_or_si128(_mm_and_si128(mm_chars2, constants::mm_split_half_bytes_blend_mask2), mm_chars3);
#endif

    // Group half-byte characters
    __m128i mm_lo = _mm_unpacklo_epi64(mm_chars1, mm_chars2);
    __m128i mm_hi = _mm_unpackhi_epi64(mm_chars1, mm_chars2);
#else
    __m128i mm_lo, mm_hi;
    {
        // Remove dashes
        __m128i mm_group1 = _mm_srli_epi64(mm_middle, 8);
        __m128i mm_group2 = _mm_srli_si128(mm_middle, 6);
        __m128i mm_group3 = _mm_srli_si128(mm_middle, 11);

        mm_chars1 = _mm_unpacklo_epi64(mm_chars1, _mm_unpacklo_epi32(mm_group1, mm_group2));

        mm_chars2 = _mm_castpd_si128(_mm_move_sd(_mm_castsi128_pd(mm_chars2), _mm_castsi128_pd(_mm_unpacklo_epi32(mm_group3, mm_chars3))));
        mm_chars2 = _mm_shuffle_epi32(mm_chars2, _MM_SHUFFLE(1, 3, 2, 0));

        // Deinterleave half-byte characters
        __m128i mm_lo1 = _mm_srli_epi16(mm_chars1, 8);
        __m128i mm_lo2 = _mm_srli_epi16(mm_chars2, 8);

        __m128i mm_hi1 = _mm_and_si128(mm_chars1, constants::mm_split_half_byte_chars_mask);
        __m128i mm_hi2 = _mm_and_si128(mm_chars2, constants::mm_split_half_byte_chars_mask);

        mm_lo = _mm_packus_epi16(mm_lo1, mm_lo2);
        mm_hi = _mm_packus_epi16(mm_hi1, mm_hi2);
    }
#endif

    // Convert characters to 8-bit integers. The algorithm is basically as follows:
    //
    // - Order the '0'-'9', 'A'-'F' and 'a'-'f' groups of characters in the order of increasing their character code values. From them, pick the two with
    //   the highest character codes. E.g. in ASCII, that would be 'A'-'F' and 'a'-'f'. This is handled at compile time in from_chars_simd_char_constants.
    // - Let mm_char_code2_cmp be a vector of the smallest character code of the second picked group minus 1 (i.e. 'a' - 1), and mm_char_code1_cmp - that
    //   of the first picked group ('A' - 1).
    // - Compare the input hex characters for being greater than mm_char_code2_cmp and mm_char_code1_cmp. This gives the masks where the input contains
    //   hexadecimal characters of the two greatest character groups, with the mask for mm_char_code1_cmp always including the one for mm_char_code2_cmp.
    //   Call those masks mm_char_code1_mask and mm_char_code2_mask.
    // - For each of the three groups of characters, have a corresponding vector of subtrahends, such that when it is subtracted from the input character codes,
    //   the characters in the group are mapped onto the corresponding value in the range 0-15. I.e. these would be '0', 'A' + 10 and 'a' + 10. Those are called
    //   mm_char_code0_sub, mm_char_code1_sub and mm_char_code2_sub, corresponding to the ordered list of groups of characters.
    // - Combine the subtrahends such that for elements where mm_char_code2_mask is non-zero, mm_char_code2_sub is used, otherwise where
    //   mm_char_code1_mask is non-zero, mm_char_code1_sub is used, otherwise mm_char_code0_sub is used.
    // - Subtract the combined subtrahends from the input character codes.
    //
    // The result will be a vector of bytes, where the values 0-15 correspond the hexadecimal characters on input.
    //
    // Note that there is one caveat due to the fact that there are only signed byte comparisons until AVX-512. This is a problem if the character encoding has
    // hexadecimal character codes with the highest bit set to 1. This is handled in from_chars_simd_char_constants by constructing mm_char_code1 and
    // mm_char_code2 in such a way that signed comparisons work as described. We also use signed comparisons in AVX-512 to reuse the same constants.
#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
#if defined(BOOST_UUID_USE_AVX512_V1)
    __mmask16 k_char_code2_mask_lo = _mm_cmpgt_epi8_mask(mm_lo, mm_char_code2_cmp);
    __mmask16 k_char_code2_mask_hi = _mm_cmpgt_epi8_mask(mm_hi, mm_char_code2_cmp);

    __mmask16 k_char_code1_mask_lo = _mm_cmpgt_epi8_mask(mm_lo, mm_char_code1_cmp);
    __mmask16 k_char_code1_mask_hi = _mm_cmpgt_epi8_mask(mm_hi, mm_char_code1_cmp);

    __m128i mm_char_code_sub_lo = _mm_mask_blend_epi8(k_char_code2_mask_lo, mm_char_code1_sub, mm_char_code2_sub);
    __m128i mm_char_code_sub_hi = _mm_mask_blend_epi8(k_char_code2_mask_hi, mm_char_code1_sub, mm_char_code2_sub);

    mm_char_code_sub_lo = _mm_mask_blend_epi8(k_char_code1_mask_lo, mm_char_code0_sub, mm_char_code_sub_lo);
    mm_char_code_sub_hi = _mm_mask_blend_epi8(k_char_code1_mask_hi, mm_char_code0_sub, mm_char_code_sub_hi);
#else
    __m128i mm_char_code2_mask_lo = _mm_cmpgt_epi8(mm_lo, mm_char_code2_cmp);
    __m128i mm_char_code2_mask_hi = _mm_cmpgt_epi8(mm_hi, mm_char_code2_cmp);

    __m128i mm_char_code1_mask_lo = _mm_cmpgt_epi8(mm_lo, mm_char_code1_cmp);
    __m128i mm_char_code1_mask_hi = _mm_cmpgt_epi8(mm_hi, mm_char_code1_cmp);

#if defined(BOOST_UUID_USE_SSE41) && defined(BOOST_UUID_FROM_CHARS_X86_USE_PBLENDVB)
    __m128i mm_char_code_sub_lo = _mm_blendv_epi8(mm_char_code1_sub, mm_char_code2_sub, mm_char_code2_mask_lo);
    __m128i mm_char_code_sub_hi = _mm_blendv_epi8(mm_char_code1_sub, mm_char_code2_sub, mm_char_code2_mask_hi);

    mm_char_code_sub_lo = _mm_blendv_epi8(mm_char_code0_sub, mm_char_code_sub_lo, mm_char_code1_mask_lo);
    mm_char_code_sub_hi = _mm_blendv_epi8(mm_char_code0_sub, mm_char_code_sub_hi, mm_char_code1_mask_hi);
#else
    __m128i mm_char_code_sub_lo = _mm_or_si128(_mm_andnot_si128(mm_char_code2_mask_lo, mm_char_code1_sub), _mm_and_si128(mm_char_code2_mask_lo, mm_char_code2_sub));
    __m128i mm_char_code_sub_hi = _mm_or_si128(_mm_andnot_si128(mm_char_code2_mask_hi, mm_char_code1_sub), _mm_and_si128(mm_char_code2_mask_hi, mm_char_code2_sub));

    mm_char_code_sub_lo = _mm_or_si128(_mm_andnot_si128(mm_char_code1_mask_lo, mm_char_code0_sub), _mm_and_si128(mm_char_code1_mask_lo, mm_char_code_sub_lo));
    mm_char_code_sub_hi = _mm_or_si128(_mm_andnot_si128(mm_char_code1_mask_hi, mm_char_code0_sub), _mm_and_si128(mm_char_code1_mask_hi, mm_char_code_sub_hi));
#endif
#endif
#else // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
    // Use a different approach:
    // - Each vpcmpgtb produces a mask, where 0 indicates false and -1 - true.
    // - mm_char_code1_mask_* always overlaps with the corresponding mm_char_code2_mask_*, which means adding them
    //   produces a vector where 0 means none of the vpcmpgtb matched the value, -1 - where mm_char_code1_mask_* matched
    //   and -2 - where mm_char_code2_mask_* matched.
    // - Shift that mask to the positive range by adding 2.
    // - Use it as a pattern for vpshufb to place one of the 3 lowest bytes in char_code_sub to the positions corresponding
    //   to the matched characters. This will be the mm_char_code_sub_* subtrahends.
    __m128i mm_char_code2_mask_lo = _mm_cmpgt_epi8(mm_lo, mm_char_code2_cmp);
    __m128i mm_char_code2_mask_hi = _mm_cmpgt_epi8(mm_hi, mm_char_code2_cmp);

    __m128i mm_char_code1_mask_lo = _mm_cmpgt_epi8(mm_lo, mm_char_code1_cmp);
    __m128i mm_char_code1_mask_hi = _mm_cmpgt_epi8(mm_hi, mm_char_code1_cmp);

    __m128i mm_char_code_pattern_lo = _mm_add_epi8(mm_char_code1_mask_lo, mm_char_code2_mask_lo);
    __m128i mm_char_code_pattern_hi = _mm_add_epi8(mm_char_code1_mask_hi, mm_char_code2_mask_hi);

    mm_char_code_pattern_lo = _mm_add_epi8(mm_char_code_pattern_lo, constants::mm_2);
    mm_char_code_pattern_hi = _mm_add_epi8(mm_char_code_pattern_hi, constants::mm_2);

    const __m128i mm_char_code_sub = _mm_cvtsi32_si128(static_cast< int >(char_code_sub));
    __m128i mm_char_code_sub_lo = _mm_shuffle_epi8(mm_char_code_sub, mm_char_code_pattern_lo);
    __m128i mm_char_code_sub_hi = _mm_shuffle_epi8(mm_char_code_sub, mm_char_code_pattern_hi);
#endif // defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)

    mm_lo = _mm_sub_epi8(mm_lo, mm_char_code_sub_lo);
    mm_hi = _mm_sub_epi8(mm_hi, mm_char_code_sub_hi);

    // Check hexadecimal character validity. Proper hexadecimal characters always convert to values of 0-15 and any other characters convert
    // to values outside that range. Which means if the upper 4 bits of a resulting integer are non-zero then the corresponding character was invalid.
#if defined(BOOST_UUID_USE_SSE41)
    if (BOOST_LIKELY(_mm_test_all_zeros(_mm_or_si128(mm_lo, mm_hi), constants::mm_F0)))
#else
    const __m128i mm_0 = _mm_setzero_si128();
    if (BOOST_LIKELY(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_and_si128(_mm_or_si128(mm_lo, mm_hi), constants::mm_F0), mm_0)) == 0xFFFF))
#endif
    {
        if (BOOST_LIKELY(ec == from_chars_error::none))
        {
            __m128i mm = _mm_or_si128(mm_lo, _mm_slli_epi32(mm_hi, 4));
            _mm_storeu_si128(reinterpret_cast< __m128i* >(data), mm);
        }
    }
    else
    {
        // Some of the hex digits are invalid
#if defined(BOOST_UUID_USE_SSE41)
        const __m128i mm_0 = _mm_setzero_si128();
#endif
        __m128i mm_hi_bits_lo = _mm_and_si128(mm_lo, constants::mm_F0);
        __m128i mm_hi_bits_hi = _mm_and_si128(mm_hi, constants::mm_F0);
        mm_hi_bits_lo = _mm_cmpeq_epi8(mm_hi_bits_lo, mm_0);
        mm_hi_bits_hi = _mm_cmpeq_epi8(mm_hi_bits_hi, mm_0);
        std::uint32_t digits_mask_lo = static_cast< std::uint32_t >(_mm_movemask_epi8(mm_hi_bits_lo));
        std::uint32_t digits_mask_hi = static_cast< std::uint32_t >(_mm_movemask_epi8(mm_hi_bits_hi));

        unsigned int pos_lo = detail::countr_zero_nz(~digits_mask_lo) * 2u + 1u;
        unsigned int pos_hi = detail::countr_zero_nz(~digits_mask_hi) * 2u;
        unsigned int pos = pos_lo < pos_hi ? pos_lo : pos_hi;
        if (pos >= 8u)
        {
            unsigned int dash_count = (pos - 4u) / 4u;
            if (dash_count > 4u)
                dash_count = 4u;
            pos += dash_count;
        }

        if (pos < end_pos)
        {
            end_pos = pos;
            ec = from_chars_error::hex_digit_expected;
        }
    }
}

template< typename Char >
BOOST_FORCEINLINE from_chars_result< Char > from_chars_simd(const Char* begin, const Char* end, uuid& u) noexcept
{
    static_assert(sizeof(Char) == 1u || sizeof(Char) == 2u || sizeof(Char) == 4u, "Boost.UUID: Unsupported output character type for from_chars");

    using char_constants = uuids::detail::from_chars_simd_char_constants< Char >;

    unsigned int end_pos = 36u;
    from_chars_error ec = from_chars_error::none;
    __m128i mm_chars1, mm_chars2, mm_chars3;
    if (BOOST_LIKELY((end - begin) >= 36))
    {
        mm_chars1 = from_chars_simd_load_traits< Char >::load_packed_16(begin);
        mm_chars2 = from_chars_simd_load_traits< Char >::load_packed_16(begin + 16);
        mm_chars3 = from_chars_simd_load_traits< Char >::load_packed_4(begin + 32);
    }
    else
    {
        end_pos = static_cast< unsigned int >(end - begin);
        ec = from_chars_error::unexpected_end_of_input;

        const Char* p = begin;
        unsigned int n = static_cast< unsigned int >(end - begin);
        if (n >= 16u)
        {
            mm_chars1 = from_chars_simd_load_traits< Char >::load_packed_16(p);
            p += 16;
            n -= 16u;
        }
        else
        {
            mm_chars1 = from_chars_simd_load_traits< Char >::load_packed_n(p, n);
            p += n;
            n = 0u;
        }

        if (n >= 16u)
        {
            mm_chars2 = from_chars_simd_load_traits< Char >::load_packed_16(p);
            p += 16;
            n -= 16u;
        }
        else
        {
            mm_chars2 = from_chars_simd_load_traits< Char >::load_packed_n(p, n);
            p += n;
            n = 0u;
        }

        mm_chars3 = from_chars_simd_load_traits< Char >::load_packed_n(p, n);
    }

    from_chars_simd_core
    (
        mm_chars1, mm_chars2, mm_chars3,
        char_constants::mm_expected_dashes,
        char_constants::mm_char_code1_cmp,
        char_constants::mm_char_code2_cmp,
#if defined(BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS)
        char_constants::mm_char_code0_sub,
        char_constants::mm_char_code1_sub,
        char_constants::mm_char_code2_sub,
#else
        char_constants::char_code_sub,
#endif
        u.data(), end_pos, ec
    );

    return { begin + end_pos, ec };
}

} // namespace detail
} // namespace uuids
} // namespace boost

#undef BOOST_UUID_DETAIL_FROM_CHARS_X86_USE_BLENDS

#endif // defined(BOOST_UUID_USE_SSE2)

#endif // BOOST_UUID_DETAIL_FROM_CHARS_X86_HPP_INCLUDED
