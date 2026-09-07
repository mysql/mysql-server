#ifndef C4_BASE64_HPP_
#include "c4/base64.hpp"
#endif
#ifndef C4_ERROR_HPP_
#include "c4/error.hpp"
#endif

#include <stdint.h>
#include <string.h>
#include <type_traits>

#define C4_PREFER_BSWAP

#if defined(C4_PREFER_BSWAP) && C4_LITTLE_ENDIAN && defined(_MSC_VER)
#include <intrin.h>
#endif

C4_SUPPRESS_WARNING_PUSH
C4_SUPPRESS_WARNING_GCC("-Wtype-limits")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")
C4_SUPPRESS_WARNING_GCC_CLANG("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC_CLANG("-Wchar-subscripts")


// NOLINTBEGIN(bugprone-signed-char-misuse,cert-str34-c,hicpp-signed-bitwise)

namespace c4 {

namespace {

const char base64_sextet_to_char_[64] = {
    /* 0/ 65*/ 'A', /* 1/ 66*/ 'B', /* 2/ 67*/ 'C', /* 3/ 68*/ 'D',
    /* 4/ 69*/ 'E', /* 5/ 70*/ 'F', /* 6/ 71*/ 'G', /* 7/ 72*/ 'H',
    /* 8/ 73*/ 'I', /* 9/ 74*/ 'J', /*10/ 75*/ 'K', /*11/ 74*/ 'L',
    /*12/ 77*/ 'M', /*13/ 78*/ 'N', /*14/ 79*/ 'O', /*15/ 78*/ 'P',
    /*16/ 81*/ 'Q', /*17/ 82*/ 'R', /*18/ 83*/ 'S', /*19/ 82*/ 'T',
    /*20/ 85*/ 'U', /*21/ 86*/ 'V', /*22/ 87*/ 'W', /*23/ 88*/ 'X',
    /*24/ 89*/ 'Y', /*25/ 90*/ 'Z', /*26/ 97*/ 'a', /*27/ 98*/ 'b',
    /*28/ 99*/ 'c', /*29/100*/ 'd', /*30/101*/ 'e', /*31/102*/ 'f',
    /*32/103*/ 'g', /*33/104*/ 'h', /*34/105*/ 'i', /*35/106*/ 'j',
    /*36/107*/ 'k', /*37/108*/ 'l', /*38/109*/ 'm', /*39/110*/ 'n',
    /*40/111*/ 'o', /*41/112*/ 'p', /*42/113*/ 'q', /*43/114*/ 'r',
    /*44/115*/ 's', /*45/116*/ 't', /*46/117*/ 'u', /*47/118*/ 'v',
    /*48/119*/ 'w', /*49/120*/ 'x', /*50/121*/ 'y', /*51/122*/ 'z',
    /*52/ 48*/ '0', /*53/ 49*/ '1', /*54/ 50*/ '2', /*55/ 51*/ '3',
    /*56/ 52*/ '4', /*57/ 53*/ '5', /*58/ 54*/ '6', /*59/ 55*/ '7',
    /*60/ 56*/ '8', /*61/ 57*/ '9', /*62/ 43*/ '+', /*63/ 47*/ '/',
};

using dectype = uint8_t;

#define s_ dectype(-1) // undefined below

// https://www.cs.cmu.edu/~pattis/15-1XX/common/handouts/ascii.html
const dectype base64_char_to_sextet_[128] = {
    /*  0 NUL*/ s_, /*  1 SOH*/ s_, /*  2 STX*/ s_, /*  3 ETX*/ s_,
    /*  4 EOT*/ s_, /*  5 ENQ*/ s_, /*  6 ACK*/ s_, /*  7 BEL*/ s_,
    /*  8 BS */ s_, /*  9 TAB*/ s_, /* 10 LF */ s_, /* 11 VT */ s_,
    /* 12 FF */ s_, /* 13 CR */ s_, /* 14 SO */ s_, /* 15 SI */ s_,
    /* 16 DLE*/ s_, /* 17 DC1*/ s_, /* 18 DC2*/ s_, /* 19 DC3*/ s_,
    /* 20 DC4*/ s_, /* 21 NAK*/ s_, /* 22 SYN*/ s_, /* 23 ETB*/ s_,
    /* 24 CAN*/ s_, /* 25 EM */ s_, /* 26 SUB*/ s_, /* 27 ESC*/ s_,
    /* 28 FS */ s_, /* 29 GS */ s_, /* 30 RS */ s_, /* 31 US */ s_,
    /* 32 SPC*/ s_, /* 33 !  */ s_, /* 34 "  */ s_, /* 35 #  */ s_,
    /* 36 $  */ s_, /* 37 %  */ s_, /* 38 &  */ s_, /* 39 '  */ s_,
    /* 40 (  */ s_, /* 41 )  */ s_, /* 42 *  */ s_, /* 43 +  */ 62,
    /* 44 ,  */ s_, /* 45 -  */ s_, /* 46 .  */ s_, /* 47 /  */ 63,
    /* 48 0  */ 52, /* 49 1  */ 53, /* 50 2  */ 54, /* 51 3  */ 55,
    /* 52 4  */ 56, /* 53 5  */ 57, /* 54 6  */ 58, /* 55 7  */ 59,
    /* 56 8  */ 60, /* 57 9  */ 61, /* 58 :  */ s_, /* 59 ;  */ s_,
    /* 60 <  */ s_, /* 61 =  */ s_, /* 62 >  */ s_, /* 63 ?  */ s_,
    /* 64 @  */ s_, /* 65 A  */  0, /* 66 B  */  1, /* 67 C  */  2,
    /* 68 D  */  3, /* 69 E  */  4, /* 70 F  */  5, /* 71 G  */  6,
    /* 72 H  */  7, /* 73 I  */  8, /* 74 J  */  9, /* 75 K  */ 10,
    /* 76 L  */ 11, /* 77 M  */ 12, /* 78 N  */ 13, /* 79 O  */ 14,
    /* 80 P  */ 15, /* 81 Q  */ 16, /* 82 R  */ 17, /* 83 S  */ 18,
    /* 84 T  */ 19, /* 85 U  */ 20, /* 86 V  */ 21, /* 87 W  */ 22,
    /* 88 X  */ 23, /* 89 Y  */ 24, /* 90 Z  */ 25, /* 91 [  */ s_,
    /* 92 \  */ s_, /* 93 ]  */ s_, /* 94 ^  */ s_, /* 95 _  */ s_,
    /* 96 `  */ s_, /* 97 a  */ 26, /* 98 b  */ 27, /* 99 c  */ 28,
    /*100 d  */ 29, /*101 e  */ 30, /*102 f  */ 31, /*103 g  */ 32,
    /*104 h  */ 33, /*105 i  */ 34, /*106 j  */ 35, /*107 k  */ 36,
    /*108 l  */ 37, /*109 m  */ 38, /*110 n  */ 39, /*111 o  */ 40,
    /*112 p  */ 41, /*113 q  */ 42, /*114 r  */ 43, /*115 s  */ 44,
    /*116 t  */ 45, /*117 u  */ 46, /*118 v  */ 47, /*119 w  */ 48,
    /*120 x  */ 49, /*121 y  */ 50, /*122 z  */ 51, /*123 {  */ s_,
    /*124 |  */ s_, /*125 }  */ s_, /*126 ~  */ s_, /*127 DEL*/ s_,
};
} // namespace

#ifndef NDEBUG
namespace detail {
C4CORE_EXPORT void base64_test_tables() // NOLINT(*use-internal-linkage*)
{
    for(size_t i = 0; i < C4_COUNTOF(base64_sextet_to_char_); ++i)
    {
        char s2c = base64_sextet_to_char_[i];
        dectype c2s = base64_char_to_sextet_[(unsigned)s2c];
        C4_CHECK((size_t)c2s == i);
    }
    for(size_t i = 0; i < C4_COUNTOF(base64_char_to_sextet_); ++i)
    {
        dectype c2s = base64_char_to_sextet_[i];
        if(c2s == s_)
            continue;
        char s2c = base64_sextet_to_char_[(unsigned)c2s];
        C4_CHECK((size_t)s2c == i);
    }
}
} // namespace detail
#endif


//-----------------------------------------------------------------------------

namespace {
#if C4_CPP >= 17
C4_HOT C4_ALWAYS_INLINE bool is_valid_encoded_char_(char c) noexcept
{
    if constexpr (std::is_unsigned_v<char>)
        return ((c < 128) && (base64_char_to_sextet_[c] != s_));
    else
        return ((c >= 0) && (base64_char_to_sextet_[c] != s_));
}
#else // pre c++-17 implementation requires SFINAE
template<class Char>
C4_HOT C4_ALWAYS_INLINE auto is_valid_encoded_char_(Char c) noexcept
    -> typename std::enable_if<std::is_unsigned<Char>::value, bool>::type
{
    return ((c < 128) && (base64_char_to_sextet_[c] != s_));
}
template<class Char>
C4_HOT C4_ALWAYS_INLINE auto is_valid_encoded_char_(Char c) noexcept
    -> typename std::enable_if< ! std::is_unsigned<Char>::value, bool>::type
{
    return ((c >= 0) && (base64_char_to_sextet_[c] != s_));
}
#endif

#undef s_


C4_HOT C4_ALWAYS_INLINE bool is_valid_encoded_group4_(const char *C4_RESTRICT c) noexcept
{
    return is_valid_encoded_char_(c[0])
        && is_valid_encoded_char_(c[1])
        && is_valid_encoded_char_(c[2])
        && is_valid_encoded_char_(c[3]);
}
C4_HOT C4_ALWAYS_INLINE bool is_valid_encoded_group8_(const char *C4_RESTRICT c) noexcept
{
    return is_valid_encoded_char_(c[0])
        && is_valid_encoded_char_(c[1])
        && is_valid_encoded_char_(c[2])
        && is_valid_encoded_char_(c[3])
        && is_valid_encoded_char_(c[4])
        && is_valid_encoded_char_(c[5])
        && is_valid_encoded_char_(c[6])
        && is_valid_encoded_char_(c[7]);
}
#if (C4_WORDSIZE >= 8)
C4_HOT C4_ALWAYS_INLINE bool is_valid_encoded_group16_(const char *C4_RESTRICT c, size_t num) noexcept
{
    C4_ASSERT(num >= 16);
    C4_ASSERT(!(num & 15)); // must be multiple of 16
    size_t rem = num;
    for( ; rem >= 16; rem -= 16, c += 16)
        if C4_UNLIKELY(!is_valid_encoded_group8_(c)
                    || !is_valid_encoded_group8_(c + 8))
            return false;
    return true;
}
#endif


#ifdef C4_PREFER_BSWAP
#    if C4_BIG_ENDIAN || (C4_MIXED_ENDIAN                               \
                          && defined(__BYTE_ORDER__)                    \
                          && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
#       define BSWAP_TO_BIG_ENDIAN64_(x)
#       define BSWAP_TO_BIG_ENDIAN32_(x)
#    elif C4_LITTLE_ENDIAN || (C4_MIXED_ENDIAN                          \
                               && defined(__BYTE_ORDER__)               \
                               && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#       ifdef _MSC_VER
#           define BSWAP_TO_BIG_ENDIAN64_(x) (x) = _byteswap_uint64(x)
#           define BSWAP_TO_BIG_ENDIAN32_(x) (x) = _byteswap_ulong(x)
#       else
#           define BSWAP_TO_BIG_ENDIAN64_(x) (x) = __builtin_bswap64(x)
#           define BSWAP_TO_BIG_ENDIAN32_(x) (x) = __builtin_bswap32(x)
#       endif
#    else
#       error not implemented
#    endif
#endif


enum : uint32_t { mask32 = uint32_t((1 << 6u) - 1u) }; // NOLINT
#if (C4_WORDSIZE >= 8)
enum : uint64_t { mask64 = uint64_t((1 << 6u) - 1u) }; // NOLINT
C4_HOT C4_ALWAYS_INLINE void base64_encode_block64_(const uint8_t *C4_RESTRICT const data, char *C4_RESTRICT const encoded) noexcept
{
    #if defined(C4_PREFER_BSWAP)
    uint64_t val;                    // MSB    ->     LSB
    memcpy(&val, data, sizeof(val)); // |.|.|5|4|3|2|1|0|
    BSWAP_TO_BIG_ENDIAN64_(val);     // |0|1|2|3|4|5|.|.|
    encoded[0] = base64_sextet_to_char_[(val >> 58) & mask64];
    encoded[1] = base64_sextet_to_char_[(val >> 52) & mask64];
    encoded[2] = base64_sextet_to_char_[(val >> 46) & mask64];
    encoded[3] = base64_sextet_to_char_[(val >> 40) & mask64];
    encoded[4] = base64_sextet_to_char_[(val >> 34) & mask64];
    encoded[5] = base64_sextet_to_char_[(val >> 28) & mask64];
    encoded[6] = base64_sextet_to_char_[(val >> 22) & mask64];
    encoded[7] = base64_sextet_to_char_[(val >> 16) & mask64];
    #else
    const uint64_t val = ((uint64_t(data[0]) << 40) | // |.|.|0|1|2|3|4|5|
                          (uint64_t(data[1]) << 32) |
                          (uint64_t(data[2]) << 24) |
                          (uint64_t(data[3]) << 16) |
                          (uint64_t(data[4]) <<  8) |
                          (uint64_t(data[5])));
    encoded[0] = base64_sextet_to_char_[(val >> 42) & mask64];
    encoded[1] = base64_sextet_to_char_[(val >> 36) & mask64];
    encoded[2] = base64_sextet_to_char_[(val >> 30) & mask64];
    encoded[3] = base64_sextet_to_char_[(val >> 24) & mask64];
    encoded[4] = base64_sextet_to_char_[(val >> 18) & mask64];
    encoded[5] = base64_sextet_to_char_[(val >> 12) & mask64];
    encoded[6] = base64_sextet_to_char_[(val >>  6) & mask64];
    encoded[7] = base64_sextet_to_char_[(val      ) & mask64];
    #endif
}
#endif

C4_HOT void base64_encode_block32_(const uint8_t *C4_RESTRICT const data, char *C4_RESTRICT const encoded) noexcept
{
    #if defined(C4_PREFER_BSWAP)
    uint32_t val = 0;
    memcpy(&val, data, sizeof(val)); // MSB: |.|2|1|0| :LSB
    BSWAP_TO_BIG_ENDIAN32_(val);     // MSB: |0|1|2|.| :LSB
    encoded[0] = base64_sextet_to_char_[(val >> 26) & mask32];
    encoded[1] = base64_sextet_to_char_[(val >> 20) & mask32];
    encoded[2] = base64_sextet_to_char_[(val >> 14) & mask32];
    encoded[3] = base64_sextet_to_char_[(val >>  8) & mask32];
    #else
    // MSB: |.|0|1|2| :LSB
    const uint32_t val = ((uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8) | (uint32_t(data[2])));
    encoded[0] = base64_sextet_to_char_[(val >> 18) & mask32];
    encoded[1] = base64_sextet_to_char_[(val >> 12) & mask32];
    encoded[2] = base64_sextet_to_char_[(val >>  6) & mask32];
    encoded[3] = base64_sextet_to_char_[(val      ) & mask32];
    #endif
}
void base64_encode_block32_term2_(const uint8_t *C4_RESTRICT data, char *C4_RESTRICT encoded) noexcept
{
    // MSB: |.|.|0|1| :LSB
    const uint32_t val = ((uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8));
    encoded[0] = base64_sextet_to_char_[(val >> 18) & mask32];
    encoded[1] = base64_sextet_to_char_[(val >> 12) & mask32];
    encoded[2] = base64_sextet_to_char_[(val >>  6) & mask32];
    encoded[3] = '=';
}
void base64_encode_block32_term1_(const uint8_t *C4_RESTRICT data, char *C4_RESTRICT encoded) noexcept
{
    // MSB: |.|.|.|0| :LSB
    const uint32_t val = ((uint32_t(data[0]) << 16));
    encoded[0] = base64_sextet_to_char_[(val >> 18) & mask32];
    encoded[1] = base64_sextet_to_char_[(val >> 12) & mask32];
    encoded[2] = '=';
    encoded[3] = '=';
}


//-----------------------------------------------------------------------------

enum : uint32_t { dmask32 = 0xff }; // NOLINT
#if (C4_WORDSIZE >= 8)
enum : uint64_t { dmask64 = 0xff }; // NOLINT
void base64_decode_block64_(const char *C4_RESTRICT encoded, dectype *C4_RESTRICT data) noexcept
{
    uint64_t val =
          (((uint64_t)base64_char_to_sextet_[encoded[0]]) << 42)
        | (((uint64_t)base64_char_to_sextet_[encoded[1]]) << 36)
        | (((uint64_t)base64_char_to_sextet_[encoded[2]]) << 30)
        | (((uint64_t)base64_char_to_sextet_[encoded[3]]) << 24)
        | (((uint64_t)base64_char_to_sextet_[encoded[4]]) << 18)
        | (((uint64_t)base64_char_to_sextet_[encoded[5]]) << 12)
        | (((uint64_t)base64_char_to_sextet_[encoded[6]]) <<  6)
        | (((uint64_t)base64_char_to_sextet_[encoded[7]])      );
    data[0] = (dectype)((val >> 40) & dmask64);
    data[1] = (dectype)((val >> 32) & dmask64);
    data[2] = (dectype)((val >> 24) & dmask64);
    data[3] = (dectype)((val >> 16) & dmask64);
    data[4] = (dectype)((val >>  8) & dmask64);
    data[5] = (dectype)((val      ) & dmask64);
}
#endif
C4_HOT void base64_decode_block32_(const char *C4_RESTRICT encoded, dectype *C4_RESTRICT data) noexcept
{
    const uint32_t val =
          (((uint32_t)base64_char_to_sextet_[encoded[0]]) << 18)
        | (((uint32_t)base64_char_to_sextet_[encoded[1]]) << 12)
        | (((uint32_t)base64_char_to_sextet_[encoded[2]]) <<  6)
        | (((uint32_t)base64_char_to_sextet_[encoded[3]])      );
    data[0] = (dectype)((val >> 16) & dmask32);
    data[1] = (dectype)((val >>  8) & dmask32);
    data[2] = (dectype)((val      ) & dmask32);
}
void base64_decode_block32_term1_(const char *C4_RESTRICT encoded, dectype *C4_RESTRICT data) noexcept
{
    const uint32_t val =
          (((uint32_t)base64_char_to_sextet_[encoded[0]]) << 18)
        | (((uint32_t)base64_char_to_sextet_[encoded[1]]) << 12);
    data[0] = (dectype)((val >> 16) & dmask32);
}
void base64_decode_block32_term2_(const char *C4_RESTRICT encoded, dectype *C4_RESTRICT data) noexcept
{
    const uint32_t val =
          (((uint32_t)base64_char_to_sextet_[encoded[0]]) << 18)
        | (((uint32_t)base64_char_to_sextet_[encoded[1]]) << 12)
        | (((uint32_t)base64_char_to_sextet_[encoded[2]]) <<  6);
    data[0] = (dectype)((val >> 16) & dmask32);
    data[1] = (dectype)((val >>  8) & dmask32);
}

} // namespace


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool base64_valid(const char *encoded_, size_t encoded_sz)
{
    if(!encoded_sz)
        return true;
    if((encoded_sz & size_t(3u))) // is it not a multiple of 4?
        return false;
    const char *C4_RESTRICT encoded = encoded_;
    size_t i = 0;
    #if C4_WORDSIZE >= 8
    for( ; i + 8 < encoded_sz; i += 8)
        if(!is_valid_encoded_group8_(encoded + i))
            return false;
    #endif
    for( ; i + 4 < encoded_sz; i += 4)
        if(!is_valid_encoded_group4_(encoded + i))
            return false;
    if(!is_valid_encoded_char_(encoded[i])
       || !is_valid_encoded_char_(encoded[i + 1]))
        return false;
    if(!is_valid_encoded_char_(encoded[i + 2]))
        return (encoded[i + 2] == '=' && encoded[i + 3] == '=');
    if(!is_valid_encoded_char_(encoded[i + 3]))
        return (encoded[i + 3] == '=');
    return true;
}


//-----------------------------------------------------------------------------

size_t base64_encode(char *encoded_, size_t encoded_sz, const void *data_, size_t data_sz)
{
    C4_ASSERT(encoded_ != nullptr || encoded_sz == 0);
    C4_ASSERT(data_ != nullptr || data_sz == 0);
    //                     ....................... how many groups of 3 bytes to read
    //                                            .... each group results in 4 bytes written
    size_t required_sz = ((data_sz + 3 - 1) / 3) * 4;
    if(encoded_sz < required_sz)
        return required_sz;
    size_t rem = data_sz;
    char *C4_RESTRICT encoded = encoded_;
    const uint8_t *C4_RESTRICT data = (const uint8_t *) data_; // cast to unsigned to avoid wrapping high-bits
#if (C4_WORDSIZE >= 8)
    for( ; rem >= 15; rem -= 12) // leave 3 at the end (15=12+3)
    {
        base64_encode_block64_(data, encoded); data += 6; encoded += 8;
        base64_encode_block64_(data, encoded); data += 6; encoded += 8;
    }
    for( ; rem >= 9; rem -= 6) // leave 3 at the end (9=6+3)
    {
        base64_encode_block64_(data, encoded); data += 6; encoded += 8;
    }
#else
    for( ; rem >= 15; rem -= 12) // leave 3 at the end (15=12+3)
    {
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
    }
    for( ; rem >= 9; rem -= 6) // leave 3 at the end (9=6+3)
    {
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
    }
#endif
    for( ; rem >= 3; rem -= 3)
    {
        base64_encode_block32_(data, encoded); data += 3; encoded += 4;
    }
    C4_ASSERT(rem < 3);
    if(rem == 2)
        base64_encode_block32_term2_(data, encoded);
    else if(rem == 1)
        base64_encode_block32_term1_(data, encoded);
    return required_sz;
}


//-----------------------------------------------------------------------------

bool base64_decode(char const* encoded_, size_t encoded_sz,
                   void * data_, size_t data_sz,
                   size_t *data_sz_required)
{
    C4_ASSERT(encoded_ != nullptr || encoded_sz == 0);
    C4_ASSERT(data_ != nullptr || data_sz == 0);
    C4_ASSERT(data_sz_required != nullptr);
    if(!encoded_sz)
    {
        *data_sz_required = 0;
        return true;
    }
    else if(encoded_sz & 3u) // is encoded_sz not a multiple of 4?
    {
        return false;
    }
    // compute the required size for the decoded buffer:
    //                  ................ how many 4-byte groups of encoded data to decode
    //                                  .... each group results in 3 decoded bytes
    *data_sz_required = (encoded_sz / 4) * 3;
    const char *C4_RESTRICT encoded = encoded_;
    // account for padded bytes at the end
    C4_ASSERT(encoded_sz >= 4);
    if(encoded[encoded_sz - 1] == '=')
    {
        C4_ASSERT(*data_sz_required >= 3);
        if(encoded[encoded_sz - 2] == '=')
            *data_sz_required -= 2;
        else
            *data_sz_required -= 1;
    }
    if(data_sz < *data_sz_required)
        return false;
    // we have enough room
    size_t rem = *data_sz_required; // numbytes remaining to write
    dectype *C4_RESTRICT data = (dectype *)data_;
    C4_STATIC_ASSERT(sizeof(dectype) == 1);
#if (C4_WORDSIZE >= 8)
    for( ; rem >= 15; rem -= 12)
    {
        if C4_UNLIKELY(!is_valid_encoded_group16_(encoded, 16))
            return false;
        base64_decode_block64_(encoded, data); encoded += 8; data += 6;
        base64_decode_block64_(encoded, data); encoded += 8; data += 6;
    }
    for( ; rem >= 9; rem -= 6)
    {
        if C4_UNLIKELY(!is_valid_encoded_group8_(encoded))
            return false;
        base64_decode_block64_(encoded, data); encoded += 8; data += 6;
    }
#else
    for( ; rem >= 9; rem -= 6)
    {
        if C4_UNLIKELY(!is_valid_encoded_group8_(encoded))
            return false;
        base64_decode_block32_(encoded, data); encoded += 4; data += 3;
        base64_decode_block32_(encoded, data); encoded += 4; data += 3;
    }
#endif
    for( ; rem >= 3; rem -= 3)
    {
        if C4_UNLIKELY(!is_valid_encoded_group4_(encoded))
            return false;
        base64_decode_block32_(encoded, data); encoded += 4; data += 3;
    }
    C4_ASSERT(rem < 3);
    // the last quartet requires dealing with padded chars
    if(rem == 1) // 1 remaining byte, 2 padding chars
    {
        if(!is_valid_encoded_char_(encoded[0])
           || !is_valid_encoded_char_(encoded[1])
           || encoded[2] != '='
           || encoded[3] != '=')
            return false;
        base64_decode_block32_term1_(encoded, data);
    }
    else if(rem == 2) // 2 remaining bytes, 1 padding char
    {
        if(!is_valid_encoded_char_(encoded[0])
           || !is_valid_encoded_char_(encoded[1])
           || !is_valid_encoded_char_(encoded[2])
           || encoded[3] != '=')
            return false;
        base64_decode_block32_term2_(encoded, data);
    }
    return true;
}

} // namespace c4

// NOLINTEND(bugprone-signed-char-misuse,cert-str34-c,hicpp-signed-bitwise)

C4_SUPPRESS_WARNING_POP
