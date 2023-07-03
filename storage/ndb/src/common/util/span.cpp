/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifdef TEST_SPAN

#include "util/require.h"
#include <array>
#include <cstddef>
#include <cstring>
#include "util/span.h"

template <std::size_t Extent = ndb::dynamic_extent>
int f(ndb::span<char, Extent> buf) noexcept
{
  return buf.size();
}

int main()
{
  char buf[100];
  ndb::span sp(buf);
  f(sp);

  std::array<char, 20> arr;
  f(ndb::span(arr));

  char* p = buf;
  std::size_t len = 8;
  f({p, len});  // Note extra braces {} around buffer argument.

  char* begin = &buf[0];
  char* end = &buf[100];
  f({begin, end});  // Note extra braces {} around buffer argument.

  ndb::span vec(buf);
  std::memset(vec.data(), 0, vec.size());
  for (auto e : vec) require(e == 0);
  for (unsigned i = 0; i < vec.size(); i++) vec[i] = i;

  return 0;
}

#endif
