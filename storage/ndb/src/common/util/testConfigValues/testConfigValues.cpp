/*
   Copyright (C) 2004-2006 MySQL AB
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

#include <ConfigValues.hpp>
#include <NdbOut.hpp>
#include <stdlib.h>
#include <string.h>

#define CF_NODES     1
#define CF_LOG_PAGES 2
#define CF_MEM_PAGES 3
#define CF_START_TO  4
#define CF_STOP_TO   5

void print(Uint32 i, ConfigValues::ConstIterator & cf){
  ndbout_c("---");
  for(Uint32 j = 2; j<=7; j++){
    switch(cf.getTypeOf(j)){
    case ConfigValues::IntType:
      ndbout_c("Node %d : CFG(%d) : %d", 
	       i, j, cf.get(j, 999));
      break;
    case ConfigValues::Int64Type:
      ndbout_c("Node %d : CFG(%d) : %lld (64)", 
	       i, j, cf.get64(j, 999));
      break;
    case ConfigValues::StringType:
      ndbout_c("Node %d : CFG(%d) : %s",
	       i, j, cf.get(j, "<NOT FOUND>"));
      break;
    default:
      ndbout_c("Node %d : CFG(%d) : TYPE: %d",
	       i, j, cf.getTypeOf(j));
    }
  }
}

void print(Uint32 i, ConfigValues & _cf){
  ConfigValues::ConstIterator cf(_cf);
  print(i, cf);
}

void
print(ConfigValues & _cf){
  ConfigValues::ConstIterator cf(_cf);
  Uint32 i = 0;
  while(cf.openSection(CF_NODES, i)){
    print(i, cf);
    cf.closeSection();
    i++;
  }
}

inline
void
require(bool b){
  if(!b)
    abort();
}

int
main(void){

  {
    ConfigValuesFactory cvf(10, 20);
    cvf.openSection(1, 0);
    cvf.put(2, 12);
    cvf.put64(3, 13);
    cvf.put(4, 14);
    cvf.put64(5, 15);
    cvf.put(6, "Keso");
    cvf.put(7, "Kent");
    cvf.closeSection();

    cvf.openSection(1, 1);
    cvf.put(2, 22);
    cvf.put64(3, 23);
    cvf.put(4, 24);
    cvf.put64(5, 25);
    cvf.put(6, "Kalle");
    cvf.put(7, "Anka");
    cvf.closeSection();
  
    ndbout_c("-- print --");
    print(* cvf.m_cfg);

    cvf.shrink();
    ndbout_c("shrink\n-- print --");
    print(* cvf.m_cfg);
    cvf.expand(10, 10);
    ndbout_c("expand\n-- print --");
    print(* cvf.m_cfg);

    ndbout_c("packed size: %d", cvf.m_cfg->getPackedSize());

    ConfigValues::ConstIterator iter(* cvf.m_cfg);
    iter.openSection(CF_NODES, 0);
    ConfigValues * cfg2 = ConfigValuesFactory::extractCurrentSection(iter);
    print(99, * cfg2);
  
    cvf.shrink();
    ndbout_c("packed size: %d", cfg2->getPackedSize());

    UtilBuffer buf;
    Uint32 l1 = cvf.m_cfg->pack(buf);
    Uint32 l2 = cvf.m_cfg->getPackedSize();
    require(l1 == l2);
  
    ConfigValuesFactory cvf2;
    require(cvf2.unpack(buf));
    UtilBuffer buf2;
    cvf2.shrink();
    Uint32 l3 = cvf2.m_cfg->pack(buf2);
    require(l1 == l3);
  
    ndbout_c("unpack\n-- print --");
    print(* cvf2.m_cfg);

    cfg2->~ConfigValues();;
    cvf.m_cfg->~ConfigValues();
    free(cfg2);
    free(cvf.m_cfg);
  }
  return 0;
}
