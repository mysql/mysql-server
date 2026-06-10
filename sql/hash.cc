/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstdint>

#include <ankerl/unordered_dense.h>

#include "my_hash_combine.h"
#include "sql/hash.h"

uint64_t HashCString(const char *str) {
  if (str == nullptr) {
    return kNullStrHash;
  }
  return ankerl::unordered_dense::hash<std::string_view>()(str);
}

uint64_t HashString(const std::string_view str) {
  return ankerl::unordered_dense::hash<std::string_view>()(str);
}

template <typename T>
uint64_t HashNumber(T num) {
  return ankerl::unordered_dense::hash<T>()(num);
}

template uint64_t HashNumber<int>(int num);
template uint64_t HashNumber<double>(double num);
template uint64_t HashNumber<uint64_t>(uint64_t num);
template uint64_t HashNumber<long long>(long long num);
template uint64_t HashNumber<long>(long num);
template uint64_t HashNumber<unsigned int>(unsigned int num);
template uint64_t HashNumber<unsigned char>(unsigned char num);

/**  Non-commutative hash combination
 * NULL hashes (i.e. hash value of zero) do not propagate, meaning that the
 * combination of a non-zero hash and a zero hash will yield a non-zero hash.
 */
uint64_t CombineNonCommutativeSigs(uint64_t h1, uint64_t h2) {
  uint64_t ret = h1;
  my_hash_combine(ret, h2);
  return ret;
}

/** Commutative hash combination
 * NULL hashes (i.e. hash value of zero) do not propagate, meaning that the
 * combination of a non-zero hash and a zero hash will yield a non-zero hash.
 */
uint64_t CombineCommutativeSigs(uint64_t h1, uint64_t h2) {
  if (h1 != h2) {
    return h1 ^ h2;
  }
  // when h1 and h2 are equal, XOR with 2 instead to avoid 0.
  return h1 ^ HashNumber(2);
}
