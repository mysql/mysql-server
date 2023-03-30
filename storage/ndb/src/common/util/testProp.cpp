/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include "Properties.hpp"
#include <NdbOut.hpp>
#include <algorithm>
#include <iostream>
#include <util/NdbTap.hpp>

TAPTEST(Properties)
{
  ndb_init();

  Properties outer_p;
  Properties nested_p;
  const Properties* p;

  nested_p.put("1", 1);
  nested_p.put("2", 2);
  nested_p.put64("3", 1, (Uint64)3);
  nested_p.put("four", "fourValue");
  nested_p.put("5", 5);

  outer_p.put("random1", 92392);
  outer_p.put("testNested", &nested_p);
  outer_p.put("random2", 2323);
  outer_p.remove("random1");
  OK(outer_p.contains("random1") == false);

  OK(outer_p.get("testNested", &p));

  // check if the iterator has the elements inserted
  Uint32 count = 0;
  Uint32 elem_inserted = 5;

  Properties::Iterator it(p);
  const char * name;
  for (name = it.first(); name != nullptr; name = it.next())
  {
    count++;
  }

  OK(elem_inserted == count);

  // check if all values inserted can be fetched
  Uint32 ret = -1;

  OK(p->get("1", &ret));
  OK(ret == 1);

  OK(p->get("2", &ret));
  OK(ret == 2);

  OK(p->get("3_1", &ret));
  OK(ret == 3);
  PropertiesType type = PropertiesType_Undefined;
  OK(p->getTypeOf("3", 1, &type));
  OK(type == PropertiesType_Uint64);

  const char* sret;
  p->get("four", &sret);
  OK(!strcmp(sret, "fourValue"));

  OK(p->get("5", &ret));
  OK(ret == 5);

  outer_p.clear();
  OK(outer_p.contains("testNested") == false);
  ndb_end(0);
  return 1;
}
