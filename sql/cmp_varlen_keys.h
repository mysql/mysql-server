/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CMP_VARLEN_KEYS_INCLUDED
#define CMP_VARLEN_KEYS_INCLUDED

#include <assert.h>
#include <stdio.h>
#include <functional>

#include "sql/sort_param.h"
#include "sql/sql_array.h"

/**
  A compare function for variable-length keys used by filesort().
  For record format documentation, @see Sort_param.

  @param  sort_field_array array of field descriptors for sorting
  @param  use_hash compare hash values (for grouping of JSON data)
  @param  s1 pointer to record 1
  @param  s2 pointer to record 2
  @return true/false according to sorting order
          true  : s1 < s2
          false : s1 >= s2
 */
inline bool cmp_varlen_keys(
    Bounds_checked_array<st_sort_field> sort_field_array, bool use_hash,
    const uchar *s1, const uchar *s2) {
  const uchar *kp1 = s1 + Sort_param::size_of_varlength_field;
  const uchar *kp2 = s2 + Sort_param::size_of_varlength_field;
  int res;
  for (const st_sort_field &sort_field : sort_field_array) {
    uint kp1_len, kp2_len, kp_len;
    if (sort_field.maybe_null) {
      const int k1_nullbyte = *kp1++;
      const int k2_nullbyte = *kp2++;
      if (k1_nullbyte != k2_nullbyte) return k1_nullbyte < k2_nullbyte;
      if (k1_nullbyte == 0 || k1_nullbyte == 0xff) {
        if (!sort_field.is_varlen) {
          kp1 += sort_field.length;
          kp2 += sort_field.length;
        }
        continue;  // Both key parts are null, nothing to compare
      }
    }
    if (sort_field.is_varlen) {
      assert(uint4korr(kp1) >= 4);
      assert(uint4korr(kp2) >= 4);

      kp1_len = uint4korr(kp1) - 4;
      kp1 += 4;

      kp2_len = uint4korr(kp2) - 4;
      kp2 += 4;

      kp_len = std::min(kp1_len, kp2_len);
    } else {
      kp_len = kp1_len = kp2_len = sort_field.length;
    }

    res = memcmp(kp1, kp2, kp_len);

    if (res) return res < 0;
    if (kp1_len != kp2_len) {
      if (sort_field.reverse)
        return kp2_len < kp1_len;
      else
        return kp1_len < kp2_len;
    }

    kp1 += kp1_len;
    kp2 += kp2_len;
  }

  if (use_hash) {
    // Compare hashes at the end of sort keys
    return memcmp(kp1, kp2, 8) < 0;
  } else {
    return false;
  }
}

/**
  This struct is used for merging chunks for filesort()
  For filesort() with fixed-size keys we use memcmp to compare rows.
  For variable length keys, we use cmp_varlen_keys to compare rows.
 */
struct Merge_chunk_greater {
  size_t m_len;
  Sort_param *m_param;

  // CTOR for filesort() with fixed-size keys
  explicit Merge_chunk_greater(size_t len) : m_len(len), m_param(nullptr) {}

  // CTOR for filesort() with varlen keys
  explicit Merge_chunk_greater(Sort_param *param) : m_len(0), m_param(param) {}

  bool operator()(Merge_chunk *a, Merge_chunk *b) const {
    return key_is_greater_than(a->current_key(), b->current_key());
  }

  bool key_is_greater_than(uchar *key1, uchar *key2) const {
    // Fixed len keys
    if (m_len) return memcmp(key1, key2, m_len) > 0;

    if (m_param)
      return cmp_varlen_keys(m_param->local_sortorder, m_param->use_hash, key2,
                             key1);

    // We can actually have zero-length sort key for filesort().
    return false;
  }
};

#endif  // CMP_VARLEN_KEYS_INCLUDED
