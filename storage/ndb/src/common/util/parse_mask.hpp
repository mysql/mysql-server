/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <util/BaseString.hpp>

/**
 * Parse string with numbers format
 *   1,2,3-5
 * @return -1 if unparsable chars found,
 *         -2 str has number > bitmask size
 *            else returns number of bits set
 */

template <typename T>
static inline int parse_mask(const char * src, T& mask)
{
  int cnt = 0;
  BaseString tmp(src);
  Vector<BaseString> list;
  tmp.split(list, ",");
  for (unsigned i = 0; i<list.size(); i++)
  {
    list[i].trim();
    if (list[i].empty())
      continue;
    char * delim = (char*)strchr(list[i].c_str(), '-');
    unsigned first = 0;
    unsigned last = 0;
    if (delim == 0)
    {
      int res = sscanf(list[i].c_str(), "%u", &first);
      if (res != 1)
      {
        return -1;
      }
      last = first;
    }
    else
    {
      * delim = 0;
      delim++;
      int res0 = sscanf(list[i].c_str(), "%u", &first);
      if (res0 != 1)
      {
        return -1;
      }
      int res1 = sscanf(delim, "%u", &last);
      if (res1 != 1)
      {
        return -1;
      }
      if (first > last)
      {
        unsigned tmp = first;
        first = last;
        last = tmp;
      }
    }

    for (unsigned j = first; j<(last+1); j++)
    {
      if (j > mask.max_size())
        return -2;

      cnt++;
      mask.set(j);
    }
  }
  return cnt;
}


