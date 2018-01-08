/*
   Copyright (C) 2003-2006 MySQL AB
    Use is subject to license terms.

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

#include "SimpleProperties.hpp"
#include <NdbOut.hpp>

Uint32 page[8192];

int writer();
int reader(Uint32 *, Uint32 len);
int unpack(Uint32 *, Uint32 len);

int main(){ 
  int len = writer();
  reader(page, len);
  unpack(page, len);
  
  return 0; 
}

int
writer(){
  LinearWriter w(&page[0], 8192);
  
  w.first();
  w.add(1, 2);
  w.add(7, 3);
  w.add(3, "jonas");
  w.add(5, "0123456789");
  w.add(7, 4);
  w.add(3, "e cool");
  w.add(5, "9876543210");
  
  ndbout_c("WordsUsed = %d", w.getWordsUsed());
  
  return w.getWordsUsed();
}

int 
reader(Uint32 * pages, Uint32 len){
  SimplePropertiesLinearReader it(pages, len);
  
  it.printAll(ndbout);
  return 0;
}

struct Test {
  Uint32 val1;
  Uint32 val7;
  char val3[100];
  Test() : val1(0xFFFFFFFF), val7(0xFFFFFFFF) { sprintf(val3, "bad");}
};

static const
SimpleProperties::SP2StructMapping
test_map [] = {
  { 1, offsetof(Test, val1), SimpleProperties::Uint32Value, 0, ~0 },
  { 7, offsetof(Test, val7), SimpleProperties::Uint32Value, 0, ~0 },
  { 3, offsetof(Test, val3), SimpleProperties::StringValue, 0, sizeof(100) },
  { 5,                    0, SimpleProperties::InvalidValue, 0, 0 }
};

static unsigned
test_map_sz = sizeof(test_map)/sizeof(test_map[0]);

int 
unpack(Uint32 * pages, Uint32 len){
  Test test;
  SimplePropertiesLinearReader it(pages, len);
  SimpleProperties::UnpackStatus status;
  while((status = SimpleProperties::unpack(it, &test, test_map, test_map_sz, 
					   true, false)) == SimpleProperties::Break){
    ndbout << "test.val1 = " << test.val1 << endl;
    ndbout << "test.val7 = " << test.val7 << endl;
    ndbout << "test.val3 = " << test.val3 << endl;
    it.next();
  }
  assert(status == SimpleProperties::Eof);
  return 0;
}
