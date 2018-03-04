/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <util/SparseBitmask.hpp>

#ifdef TEST_SPARSEBITMASK
#include <util/NdbTap.hpp>

#include "parse_mask.hpp"

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
    parse_mask
  */
  SparseBitmask mask(256);
  OK(parse_mask("1,2,5-7", mask) == 5);

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
  OK(parse_mask("1-256", mask));

  // Parse invalid spec(s)
  OK(parse_mask("xx", mask) == -1);
  OK(parse_mask("5-", mask) == -1);
  OK(parse_mask("-5", mask) == -1);
  OK(parse_mask("1,-5", mask) == -1);

  // Parse too large spec
  OK(parse_mask("257", mask) == -2);
  OK(parse_mask("1-256,257", mask) == -2);


  return 1; // OK
}

#endif
