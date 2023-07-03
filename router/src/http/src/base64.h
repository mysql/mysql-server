/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_HTTP_BASE64_INCLUDED
#define MYSQLROUTER_HTTP_BASE64_INCLUDED

#include "mysqlrouter/http_common_export.h"

#include <algorithm>  // min
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>  // index_sequence
#include <vector>

enum class Base64Endianess { LITTLE, BIG };

/**
 * Generic Base64 codec.
 *
 * Base64 comes in many flavours:
 *
 * - RFC4648 used by HTTP
 * - crypt
 * - bcrypt
 * - pbkdf2 in MCF
 * - UUencode
 *
 * they differ by
 *
 * - alphabet
 * - endianness
 * - padding
 *
 * Base64Impl provides generic encode and decode methods which are parametrized
 * by Endianness, Padding.
 *
 * Parametrization with templates allows to provide:
 *
 * - one implementation for all combinations
 * - without extra runtime overhead as dead code is removed by the compiler
 *
 * Endianness
 * =========
 *
 * Little Endian
 * -------------
 *
 * using Alphabet=Crypt
 *
 *     octet(hex):        55
 *     uint32:      ........ ........  01010101 (LSB)
 *     uint32:      ...... ...... ....01 010101 (LSB)
 *     sextet(hex):                    1     15
 *     Alphabet:                       /      J
 *
 *     Out: J/
 *
 *
 * Big Endian
 * ----------
 *
 * using Alphabet=Crypt
 *
 *     octet(hex):        55
 *     uint32:      01010101 ........  ........ (LSB)
 *     uint32:      010101 01.... ...... ...... (LSB)
 *     sextet(hex):     15     10
 *     Alphabet:         J      E
 *
 *     Out: JE
 *
 * Padding
 * =======
 *
 * If padding is defined mandatory,
 *
 * - at encode() each group of 4 sextets is filled by with the padding
 *   character.
 * - at decode() input must have padding.
 *
 * If padding is not mandatory,
 *
 * - at encode() no padding is added.
 * - at decode() padding is accepted, but not required.
 */

/*
 * TODO: look into
 *
 * - http://0x80.pl/notesen/2015-12-27-base64-encoding.html
 * - http://0x80.pl/notesen/2016-01-12-sse-base64-encoding.html
 * - http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html
 * - http://alfredklomp.com/programming/sse-base64/
 * - https://github.com/lemire/fastbase64
 */
class Base64Impl {
 public:
  /**
   * type of all alphabet.
   */
  using alphabet_type = std::array<char, 64>;

  /**
   * type of all inverse mappings of alphabets.
   *
   * - -1 invalid
   * - 0-63 position into alphabet
   */
  using inverse_alphabet_type = std::array<int8_t, 256>;

  template <Base64Endianess endianess, bool PaddingMandatory, char PaddingChar>
  static std::vector<uint8_t> decode(
      const std::string &encoded,
      const inverse_alphabet_type &inverse_alphabet) {
    std::vector<uint8_t> out((encoded.size() + 3) / 4 * 3);

    constexpr unsigned int shift_pos_0 =
        endianess == Base64Endianess::BIG ? 16 : 0;
    constexpr unsigned int shift_pos_1 =
        endianess == Base64Endianess::BIG ? 8 : 8;
    constexpr unsigned int shift_pos_2 =
        endianess == Base64Endianess::BIG ? 0 : 16;

    auto out_it = out.begin();
    auto data_it = encoded.cbegin();
    const auto data_end_it = encoded.cend();
    while (const size_t data_left = std::distance(data_it, data_end_it)) {
      if (data_left < 2) {
        throw std::runtime_error("invalid sequence");
      }

      if (PaddingMandatory && (data_left < 4)) {
        throw std::runtime_error("missing padding");
      }

      uint32_t v = 0;
      bool is_padding = false;
      const size_t max_rounds = std::min(size_t{4}, data_left);
      uint32_t sextets = 0;
      for (size_t cnt = 0; cnt < max_rounds; ++cnt) {
        const uint8_t b64 = *(data_it++);
        if (is_padding && b64 != PaddingChar) {
          throw std::runtime_error("invalid char, expected padding");
        }
        const int8_t c = (inverse_alphabet[b64]);

        if (c == -1) {
          if (data_left <= 4 && cnt >= 2 && b64 == PaddingChar) {
            // padding is ok at the end
            is_padding = true;
          } else {
            throw std::runtime_error(std::string("invalid char"));
          }
        }

        // add new 6 bits
        if (!is_padding) {
          if (endianess == Base64Endianess::BIG) {
            v |= c << (6 * (3 - cnt));
          } else {
            v |= c << (6 * cnt);
          }
          sextets++;
        }
      }

      // 3 * 6bit b64 = 18bits translates to 16bit (2 bits extra)
      // 2 * 6bit b64 = 12bits translates to 8bit (4 bits extra)
      //
      // They must be 0b0 to ensure only one b64 value
      // maps to one 8bit version and the other way around.
      //
      // Example
      // ------
      //
      // WWU= -> Ye -> WWU=
      //
      //                   0x14
      //     ...... ...... 010100
      //     ........ ........ xx
      //
      // WWW= -> Ye -> WWU=
      //
      //                   0x16
      //     ...... ...... 010110
      //     ........ ........ xx
      //
      switch (sextets) {
        case 2:
          *(out_it++) = static_cast<uint8_t>(v >> shift_pos_0);

          if (0 != static_cast<uint8_t>(v >> shift_pos_1)) {
            throw std::runtime_error("unused bits");
          }
          break;
        case 3:
          *(out_it++) = static_cast<uint8_t>(v >> shift_pos_0);
          *(out_it++) = static_cast<uint8_t>(v >> shift_pos_1);

          if (0 != static_cast<uint8_t>(v >> shift_pos_2)) {
            throw std::runtime_error("unused bits");
          }
          break;
        case 4:
          *(out_it++) = static_cast<uint8_t>(v >> shift_pos_0);
          *(out_it++) = static_cast<uint8_t>(v >> shift_pos_1);
          *(out_it++) = static_cast<uint8_t>(v >> shift_pos_2);
          break;
      }
    }

    out.resize(std::distance(out.begin(), out_it));

    return out;
  }

  template <Base64Endianess endianess, bool PaddingMandatory, char PaddingChar>
  static std::string encode(const std::vector<uint8_t> &data,
                            const alphabet_type &alphabet) {
    std::string out;
    // ensure we have enough space
    out.resize((data.size() + 2) / 3 * 4);

    constexpr unsigned int shift_pos_0 =
        endianess == Base64Endianess::BIG ? 16 : 0;
    constexpr unsigned int shift_pos_1 =
        endianess == Base64Endianess::BIG ? 8 : 8;
    constexpr unsigned int shift_pos_2 =
        endianess == Base64Endianess::BIG ? 0 : 16;

    auto out_it = out.begin();
    auto data_it = data.begin();
    const auto data_end_it = data.end();
    while (const size_t data_left = std::distance(data_it, data_end_it)) {
      // consume 3 bytes, if we have them
      uint32_t v = 0;

      size_t padding_pos = 4;  // no padding
      switch (data_left) {
        case 1:
          v |= (*(data_it++)) << shift_pos_0;
          padding_pos = 2;  // out-byte 2 and 3 are padding
          break;
        case 2:
          v |= (*(data_it++)) << shift_pos_0;
          v |= (*(data_it++)) << shift_pos_1;
          padding_pos = 3;  // out-byte 3 is padding
          break;
        default:
          v |= (*(data_it++)) << shift_pos_0;
          v |= (*(data_it++)) << shift_pos_1;
          v |= (*(data_it++)) << shift_pos_2;
          break;
      }

      // and base64-encode them
      size_t cnt;
      for (cnt = 0; cnt < 4; ++cnt) {
        if (cnt >= padding_pos) {
          break;
        }

        size_t pos;
        if (endianess == Base64Endianess::BIG) {
          // take the upper 6 bit and shift left each round
          pos = (v & (0x3f << (3 * 6))) >> 3 * 6;
          v <<= 6;
        } else {
          // take the lower 6 bit and shift right each round
          pos = v & 0x3f;
          v >>= 6;
        }
        *(out_it++) = alphabet.at(pos);
      }

      if (PaddingMandatory) {
        // apply padding if needed
        for (; cnt < 4; ++cnt) {
          *(out_it++) = PaddingChar;
        }
      }
    }
    out.resize(std::distance(out.begin(), out_it));

    return out;
  }
};

namespace Base64Alphabet {

/**
 * type of all alphabet.
 */
using alphabet_type = Base64Impl::alphabet_type;

/**
 * type of all inverse mappings of alphabets.
 *
 * - -1 invalid
 * - 0-63 position into alphabet
 */
using inverse_alphabet_type = Base64Impl::inverse_alphabet_type;

namespace detail {
/**
 * find position of char in alphabet.
 *
 * @returns position in alphabet
 * @retval -1 if not found
 */
constexpr int8_t find_pos_of_char(const alphabet_type &v, uint8_t character,
                                  size_t v_ndx = 0) {
  // ensure that -1 and 255 don't overlap
  static_assert(alphabet_type().size() <= 128,
                "alphabet MUST less <= 128 chars");
  return (v_ndx >= v.size() ? -1
                            : (v[v_ndx] == static_cast<char>(character))
                                  ? static_cast<uint8_t>(v_ndx)
                                  : find_pos_of_char(v, character, v_ndx + 1));
}

// hand-roll std::make_index_sequence<256> as SunCC 12.6 runs into:
//
//     Error: Templates nested too deeply (recursively?).
//
// for std::make_index_sequence<n> with n > 255
//
// see: https://community.oracle.com/thread/4070120
//
// There exists a workaround:
//
// > The compiler has an undocumented "Q option" (or "Wizard option") that
// > lets you increase the allowed depth:
// >
// >     -Qoption ccfe -tmpldepth=N
// >     -W0,-tmpldepth=N
//
// but here we go the hand-rolled version of what std::make_index_sequence<256>
// would have generated instead.

using ndx_256 = std::index_sequence<
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
    78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
    113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
    158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
    173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
    188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
    203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217,
    218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232,
    233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247,
    248, 249, 250, 251, 252, 253, 254, 255>;

/**
 * build inverse-alphabet.
 *
 * calls find_pos_by_char() for each element of ndx_256
 *
 * based on C++11 parameter pack.
 */
template <std::size_t... I>
constexpr inverse_alphabet_type make_inverse_array(const alphabet_type &v,
                                                   std::index_sequence<I...>) {
  return {{find_pos_of_char(v, I)...}};
}

/**
 * inverse
 */
constexpr inverse_alphabet_type inverse(const alphabet_type &v) {
  return make_inverse_array(v, ndx_256{});
}
}  // namespace detail

class Base64 {
 public:
  static constexpr alphabet_type HTTP_COMMON_EXPORT alphabet{
      {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',  // 0x00
       'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',  //
       'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',  // 0x10
       'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',  //
       'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',  // 0x20
       'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  //
       'w', 'x', 'y', 'z', '0', '1', '2', '3',  // 0x30
       '4', '5', '6', '7', '8', '9', '+', '/'}};

  static constexpr inverse_alphabet_type HTTP_COMMON_EXPORT inverse_alphabet =
      detail::inverse(alphabet);
};

class Base64Url {
 public:
  static constexpr alphabet_type HTTP_COMMON_EXPORT alphabet{
      {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',  // 0x00
       'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',  //
       'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',  // 0x10
       'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',  //
       'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',  // 0x20
       'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  //
       'w', 'x', 'y', 'z', '0', '1', '2', '3',  // 0x30
       '4', '5', '6', '7', '8', '9', '-', '_'}};

  static constexpr inverse_alphabet_type HTTP_COMMON_EXPORT inverse_alphabet =
      detail::inverse(alphabet);
};

/**
 * Base64 alphabet for MCF.
 *
 * same as Base64 from RFC4648, but different altchars to fit the needs of MCF
 *
 * altchars
 * :  . and /
 *
 * paddingchar
 * :  =
 *
 * padding mandatory
 * :  no
 */
class Mcf {
 public:
  static constexpr alphabet_type HTTP_COMMON_EXPORT alphabet{
      {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',  // 0x00
       'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',  //
       'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',  // 0x10
       'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',  //
       'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',  // 0x20
       'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  //
       'w', 'x', 'y', 'z', '0', '1', '2', '3',  // 0x30
       '4', '5', '6', '7', '8', '9', '.', '/'}};

  static constexpr inverse_alphabet_type HTTP_COMMON_EXPORT inverse_alphabet =
      detail::inverse(alphabet);
};

class Crypt {
 public:
  static constexpr alphabet_type HTTP_COMMON_EXPORT alphabet{
      {'.', '/', '0', '1', '2', '3', '4', '5',  // 0x00
       '6', '7', '8', '9', 'A', 'B', 'C', 'D',  //
       'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',  // 0x10
       'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',  //
       'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',  // 0x20
       'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',  //
       'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',  // 0x30
       's', 't', 'u', 'v', 'w', 'x', 'y', 'z'}};

  static constexpr inverse_alphabet_type HTTP_COMMON_EXPORT inverse_alphabet =
      detail::inverse(alphabet);
};

class Bcrypt {
 public:
  static constexpr alphabet_type HTTP_COMMON_EXPORT alphabet{
      {'.', '/', 'A', 'B', 'C', 'D', 'E', 'F',  // 0x00
       'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',  //
       'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',  // 0x10
       'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',  //
       'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',  // 0x20
       'm', 'n', 'o', 'p', 'q', 'r', 's', 't',  //
       'u', 'v', 'w', 'x', 'y', 'z', '0', '1',  // 0x30
       '2', '3', '4', '5', '6', '7', '8', '9'}};

  static constexpr inverse_alphabet_type HTTP_COMMON_EXPORT inverse_alphabet =
      detail::inverse(alphabet);
};

class Uuencode {
 public:
  static constexpr alphabet_type HTTP_COMMON_EXPORT alphabet{{
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // 0x00
      0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // 0x10
      0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
      0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  // 0x20
      0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  // 0x30
      0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  }};

  static constexpr inverse_alphabet_type HTTP_COMMON_EXPORT inverse_alphabet =
      detail::inverse(alphabet);
};
}  // namespace Base64Alphabet

/**
 * Base64 codec base class.
 */
template <class Alphabet, Base64Endianess E, bool PaddingMandatory,
          char PaddingChar>
class Base64Base {
 public:
  /**
   * decode a base64 encoded string to binary.
   *
   * @pre encoded only contains alphabet
   *
   * @throws std::runtime_error if preconditions are not met
   * @returns binary representation
   */
  static std::vector<uint8_t> decode(const std::string &encoded) {
    return Base64Impl::decode<E, PaddingMandatory, PaddingChar>(
        encoded, Alphabet::inverse_alphabet);
  }

  /**
   * encode binary to base64.
   */
  static std::string encode(const std::vector<uint8_t> &decoded) {
    return Base64Impl::encode<E, PaddingMandatory, PaddingChar>(
        decoded, Alphabet::alphabet);
  }
};

/**
 * 'base64' alphabet from RFC4648.
 *
 * also used by:
 *
 * - uuencode-base64
 * - data URI scheme (RFC2397)
 *
 * alphabet
 * :  `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/`
 *
 * padding mandatory
 * :  yes, with =
 */
using Base64 =
    Base64Base<Base64Alphabet::Base64, Base64Endianess::BIG, true, '='>;

/**
 * 'base64url' URL and Filename-safe Base64 alphabet from RFC4648.
 *
 * '+' and '/' in 'base64' have special meaning in URLs and would need
 * to be URL-encoded.
 *
 * alphabet
 * :  `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_`
 *
 * padding mandatory
 * :  yes, with =
 */
using Base64Url =
    Base64Base<Base64Alphabet::Base64Url, Base64Endianess::BIG, true, '='>;

/**
 * Base64 alphabet for MCF's pbkdf2 methods.
 *
 * alphabet
 * :  `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./`
 *
 * padding mandatory
 * :  no
 */
using Radix64Mcf =
    Base64Base<Base64Alphabet::Mcf, Base64Endianess::BIG, false, ' '>;

/**
 * Radix64 for crypt (little-endian).
 *
 * alphabet
 * :  `./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`
 *
 * padding mandatory
 * :  no
 */
using Radix64Crypt =
    Base64Base<Base64Alphabet::Crypt, Base64Endianess::LITTLE, false, ' '>;

/**
 * Radix64 for crypt (big-endian).
 *
 * @see Radix64Crypt
 */
using Radix64CryptBE =
    Base64Base<Base64Alphabet::Crypt, Base64Endianess::BIG, false, ' '>;

/**
 * Radix64 for bcrypt.
 *
 * alphabet
 * :  `./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`
 *
 * padding mandatory
 * :  no
 */
using Radix64Bcrypt =
    Base64Base<Base64Alphabet::Bcrypt, Base64Endianess::LITTLE, false, ' '>;

/**
 * Radix64 for traditional Uuencode.
 *
 * alphabet
 * :  0x32...0x95
 *
 * padding mandatory
 * :  yes, with accent
 */
using Radix64Uuencode =
    Base64Base<Base64Alphabet::Uuencode, Base64Endianess::BIG, true, '`'>;

#endif
