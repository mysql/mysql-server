/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HELPER_STRING_RANDOM_H_
#define ROUTER_SRC_HELPER_STRING_RANDOM_H_

#include <algorithm>

namespace helper {

/**
 * Base class for generators.
 *
 * Generator must provide only following function:
 *
 *     static char generate()
 *
 *  and doesn't need to inherit from `GeneratorBase`.
 */
struct GeneratorBase {
  /**
   * Static method that generates random number.
   *
   * The methods was mainly introduced for easier changing the randomize
   * algorithm in future.
   */
  static int randomize() { return rand(); }
};

struct GeneratorSmallAlpha : public GeneratorBase {
 protected:
  const static char smallEnd = 'z';
  const static char smallBegin = 'a';
  const static char bigEnd = 'Z';
  const static char bigBegin = 'A';

  const static int smallRange = (smallEnd - smallBegin) + 1;
  const static int bigRange = (bigEnd - bigBegin) + 1;

 public:
  const static int kNumberOfCharacters = smallRange;
  static char generate() {
    auto result = randomize() % kNumberOfCharacters;
    return smallBegin + result;
  }
};

struct GeneratorAlpha : public GeneratorSmallAlpha {
 public:
  const static int kNumberOfCharacters = smallRange + bigRange;

  static char generate() {
    auto result = randomize() % kNumberOfCharacters;
    if (result < smallRange) return smallBegin + result;
    result -= smallRange;
    return bigBegin + result;
  }
};

struct GeneratorAlphaNumeric : public GeneratorSmallAlpha {
 public:
  const static char numericEnd = '9';
  const static char numericBegin = '0';

  const static int numericRange = (numericEnd - numericBegin) + 1;
  const static int kNumberOfCharacters = smallRange + bigRange + numericRange;

  static char generate() {
    auto result = randomize() % kNumberOfCharacters;
    if (result < smallRange) return smallBegin + result;
    result -= smallRange;

    if (result < bigRange) return bigBegin + result;
    result -= bigRange;

    return numericBegin + result;
  }
};

struct Generator8bitsValues : public GeneratorSmallAlpha {
 public:
  static char generate() {
    auto result = randomize() % 255;

    return static_cast<char>(result);
  }
};

template <typename Generator = GeneratorSmallAlpha>
inline std::string generate_string(uint32_t length) {
  std::string result(length, '0');
  std::generate(result.begin(), result.end(), &Generator::generate);

  return result;
}

template <uint32_t length, typename Generator = GeneratorSmallAlpha>
inline std::string generate_string() {
  return generate_string<Generator>(length);
}

}  // namespace helper

#endif  // ROUTER_SRC_HELPER_STRING_RANDOM_H_
