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

#include <util/SparseBitmask.hpp>
#include "parse_mask.hpp"

int
SparseBitmask::parseMask(const char* src)
{
  return parse_mask(src, *this);
}

#ifdef TEST_SPARSEBITMASK
#include <util/NdbTap.hpp>

TAPTEST(SparseBitmask)
{
  SparseBitmask b;
  OK(b.isclear());
  for (unsigned i = 0; i < 100; i++)
  {
    if (i > 60)
      continue;
    switch(i)
    {
    case 2:case 3:case 5:case 7:case 11:case 13:case 17:case 19:case 23:
    case 29:case 31:case 37:case 41:case 43:
      break;
    default:
      b.set(i);
    }
  }

  unsigned found = 0;
  for (unsigned i = 0; i < 100; i++)
  {
    if (b.get(i))
      found++;
  }
  OK(found == b.count());
  OK(found == 47);

  // Set already set bit again
  b.set(6);
  OK(found == b.count());

  /*
    Bitmask::parseMask
  */
  SparseBitmask mask(256);
  OK(mask.parseMask("1,2,5-7") == 5);

  // Check all specified bits set
  OK(mask.get(1));
  OK(mask.get(2));
  OK(mask.get(5));
  OK(mask.get(6));
  OK(mask.get(7));

  // Check some random bits not set
  OK(!mask.get(0));
  OK(!mask.get(4));
  OK(!mask.get(3));
  OK(!mask.get(8));
  OK(!mask.get(22));

  // Parse some more...
  OK(mask.parseMask("1-256"));

  // Parse invalid spec(s)
  OK(mask.parseMask("xx") == -1);
  OK(mask.parseMask("5-") == -1);
  OK(mask.parseMask("-5") == -1);
  OK(mask.parseMask("1,-5") == -1);

  // Parse too large spec
  OK(mask.parseMask("257") == -2);
  OK(mask.parseMask("1-256,257") == -2);


  return 1; // OK
}

template class Vector<unsigned>;

#endif
