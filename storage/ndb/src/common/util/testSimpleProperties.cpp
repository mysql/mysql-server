/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
void pack();
void testBuffered();

int main(){ 
  ndb_init();
  int len = writer();
  reader(page, len);
  unpack(page, len);
  pack();
  testBuffered();
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
  w.add(9, "elephantastic allostatic acrobat (external)");
  
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
};

static const
SimpleProperties::SP2StructMapping
test_map [] = {
  { 1, offsetof(Test, val1), SimpleProperties::Uint32Value,  0, 0 },
  { 7, offsetof(Test, val7), SimpleProperties::Uint32Value,  0, 0 },
  { 3, offsetof(Test, val3), SimpleProperties::StringValue,  0, 0 },
  { 5,                    0, SimpleProperties::InvalidValue, 0, 0 },
  { 9,                    0, SimpleProperties::StringValue,  0,
                             SimpleProperties::SP2StructMapping::ExternalData }
};

static unsigned
test_map_sz = sizeof(test_map)/sizeof(test_map[0]);

void indirectReader(SimpleProperties::Reader & it, void * ) {
  char buf[80];
  it.getString(buf);
  ndbout << "indirectReader: key= " << it.getKey() << " length= " <<
    it.getValueLen() << endl;
}

int 
unpack(Uint32 * pages, Uint32 len){
  Test test;
  test.val1 = 0xFFFFFFFF;
  test.val7 = 0xFFFFFFFF;
  sprintf(test.val3, "bad");

  SimplePropertiesLinearReader it(pages, len);
  SimpleProperties::UnpackStatus status;
  while((status = SimpleProperties::unpack(it, &test, test_map,
                                           test_map_sz, indirectReader))
          == SimpleProperties::Break){
    ndbout << "test.val1 = " << test.val1 << endl;
    ndbout << "test.val7 = " << test.val7 << endl;
    ndbout << "test.val3 = " << test.val3 << endl;
    it.next();
  }
  require(status == SimpleProperties::Eof);
  return 0;
}

bool
indirectWriter(SimpleProperties::Writer & it, Uint16 key, const void *) {
  ndbout << "indirectWriter: key= " << key << endl;
  it.add(9, "109");
  return true;
}

void pack() {
  ndbout << " -- test pack --" << endl;
  Uint32 buf[8192];
  Test test;

  test.val1 = 101;
  test.val7 = 107;
  sprintf(test.val3, "103");

  LinearWriter w(&buf[0], 8192);

  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w, &test, test_map, test_map_sz, indirectWriter);
  require(s == SimpleProperties::Eof);

  SimplePropertiesLinearReader r(buf, w.getWordsUsed());
  r.printAll(ndbout);
}

void testBuffered() {
  ndbout << " -- test buffered --" << endl;
  char smallbuf[8];
  char test2[40];
  int nwritten, nread, nreadcalls;

  LinearWriter w(page, 8192);

  /* write key 1 */
  w.addKey(1, SimpleProperties::StringValue, 11);
  snprintf(smallbuf, 8, "AbcdEfg");
  smallbuf[7] = 'h';
  nwritten = w.append(smallbuf, sizeof(smallbuf));
  require(nwritten == 8);

  sprintf(smallbuf,"Ij");
  smallbuf[2] = '\0';
  nwritten = w.append(smallbuf, sizeof(smallbuf));
  require(nwritten == 3);

  nwritten = w.append(smallbuf, sizeof(smallbuf));
  require(nwritten == 0);

  /* write key 2 */
  memset(test2, '\0', sizeof(test2));
  sprintf(test2, "In Xanadu did Kubla Khan a stately");
  printf("Length for key 2: %zu/%zu \n", strlen(test2)+1, sizeof(test2));
  w.add(2, test2);

  SimplePropertiesLinearReader r(page, w.getWordsUsed());

  /* read key 1 */
  r.first();
  require(r.valid());
  require(r.getKey() == 1);
  require(r.getValueType() == SimpleProperties::StringValue);
  require(r.getValueLen() == 11);
  memset(test2, '\0', sizeof(test2));
  r.getString(test2);
  require(strncmp(test2, "AbcdEfghIj", 11) == 0);

  /* read key 2 */
  r.next();
  require(r.valid());
  require(r.getKey() == 2);
  require(r.getValueType() == SimpleProperties::StringValue);

  nreadcalls = 0;
  memset(smallbuf, '\0', sizeof(smallbuf));
  while((nread = r.getBuffered(smallbuf, 8)) > 0) {
    nreadcalls++;
    printf("%d => %c%c%c%c%c%c%c%c \n",
           nread, smallbuf[0], smallbuf[1], smallbuf[2], smallbuf[3],
                  smallbuf[4], smallbuf[5], smallbuf[6], smallbuf[7]);
    memset(smallbuf, '\0', sizeof(smallbuf));
  }
  printf("Total buffered read calls: %d \n", nreadcalls);
}
