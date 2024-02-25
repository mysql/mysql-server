/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED
#define MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED

#include <random>
#include <string>

#include "harness_export.h"

namespace mysql_harness {

class HARNESS_EXPORT RandomGeneratorInterface {
 public:
  enum AlphabetContent : unsigned {
    AlphabetDigits = 0x1,
    AlphabetLowercase = 0x2,
    AlphabetUppercase = 0x4,
    AlphabetSpecial = 0x8,
    AlphabetAll = 0xFF
  };

  /** @brief Generates a random string out of selected alphabet
   *
   * @param length length of string requested
   * @param alphabet_mask bitmasmask indicating which alphabet symbol groups
   * should be used for identifier generation (see AlphabetContent enum for
   * possible values that can be or-ed)
   * @return string with the generated random chars
   *
   * @throws std::invalid_argument when the alphabet_mask is empty or invalid
   *
   */
  virtual std::string generate_identifier(
      unsigned length, unsigned alphabet_mask = AlphabetAll) = 0;

  /** @brief Generates a random password that adheres to the STRONG password
   * requirements:
   *         * contains at least 1 digit
   *         * contains at least 1 uppercase letter
   *         * contains at least 1 lowercase letter
   *         * contains at least 1 special character
   *
   * @param length length of requested password (should be at least 8)
   * @return string with the generated password
   *
   * @throws std::invalid_argument when the requested length is less than 8
   *
   */
  virtual std::string generate_strong_password(unsigned length) = 0;

  explicit RandomGeneratorInterface() = default;
  explicit RandomGeneratorInterface(const RandomGeneratorInterface &) = default;
  RandomGeneratorInterface &operator=(const RandomGeneratorInterface &) =
      default;
  virtual ~RandomGeneratorInterface();
};

class HARNESS_EXPORT RandomGenerator : public RandomGeneratorInterface {
  std::mt19937 urng;

 public:
  RandomGenerator() : urng(std::random_device().operator()()) {}
  std::string generate_identifier(
      unsigned length, unsigned alphabet_mask = AlphabetAll) override;
  std::string generate_strong_password(unsigned length) override;
};

class HARNESS_EXPORT FakeRandomGenerator : public RandomGeneratorInterface {
 public:
  // returns "012345678901234567890123...", truncated to password_length
  std::string generate_identifier(unsigned length, unsigned) override;
  // returns "012345678901234567890123...", truncated to password_length
  std::string generate_strong_password(unsigned length) override;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_RANDOM_GENERATOR_INCLUDED
