/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CMP_VARLEN_KEYS_INCLUDED
#define CMP_VARLEN_KEYS_INCLUDED

#include <functional>

#include "sort_param.h"
#include "sql_array.h"

/**
  A compare function for variable-length keys used by filesort().
  For record format documentation, @see Sort_param.

  @param  sort_field_array array of field descriptors for sorting
  @param  s1 pointer to record 1
  @param  s2 pointer to record 2
  @return true/false according to sorting order
 */
inline
bool cmp_varlen_keys(Bounds_checked_array<st_sort_field> sort_field_array,
                     const uchar *s1, const uchar *s2)
{
  std::less<int> compare;
  const uchar *kp1= s1 + Sort_param::size_of_varlength_field;
  const uchar *kp2= s2 + Sort_param::size_of_varlength_field;
  int res;
  for (const st_sort_field &sort_field : sort_field_array)
  {
    uint kp1_len, kp2_len, kp_len;
    if (sort_field.maybe_null)
    {
      const int k1_nullbyte= *kp1;
      const int k2_nullbyte= *kp2;
      if (k1_nullbyte != k2_nullbyte)
        return
          compare(k1_nullbyte, k2_nullbyte);
      kp1++;
      kp2++;
      if (k1_nullbyte == 0 || k1_nullbyte == 0xff)
      {
        if (!sort_field.is_varlen)
        {
          kp1+= sort_field.length;
          kp2+= sort_field.length;
        }
        continue; // Both key parts are null, nothing to compare
      }
    }
    if (sort_field.is_varlen)
    {
      kp1_len= uint4korr(kp1) - 4;
      kp1+= 4;

      kp2_len= uint4korr(kp2) - 4;
      kp2+= 4;

      kp_len= std::min(kp1_len, kp2_len);
    }
    else
    {
      kp_len= kp1_len= kp2_len= sort_field.length;
    }

    res= memcmp(kp1, kp2, kp_len);

    if (res)
      return compare(res, 0);
    if (kp1_len != kp2_len)
    {
      if (sort_field.reverse)
        return compare(kp2_len, kp1_len);
      else
        return compare(kp1_len, kp2_len);
    }

    kp1+= kp1_len;
    kp2+= kp2_len;
  }
  // Compare hashes at the end of sort keys
  return compare(memcmp(kp1, kp2, 8), 0);
}

#endif  // CMP_VARLEN_KEYS_INCLUDED
