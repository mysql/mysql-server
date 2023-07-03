/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ConfigValues.hpp>
#include <NdbOut.hpp>

void print(Uint32 i, ConfigValues::ConstIterator & cf){
  ndbout_c("---");
  for(Uint32 j = 2; j<=7; j++){
    switch(cf.getTypeOf(j)){
    case ConfigSection::IntTypeId:
      ndbout_c("Node %d : CFG(%d) : %d",
	       i, j, cf.get(j, 999));
      break;
    case ConfigSection::Int64TypeId:
      ndbout_c("Node %d : CFG(%d) : %lld (64)",
	       i, j, cf.get64(j, 999));
      break;
    case ConfigSection::StringTypeId:
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
  while(cf.openSection(CONFIG_SECTION_NODE, i)){
    print(i, cf);
    cf.closeSection();
    i++;
  }
}


int
main(void){
  ndb_init();
  ConfigValuesFactory cvf;
  cvf.begin();
  cvf.createSection(CONFIG_SECTION_SYSTEM, 0);
  cvf.put(2, 12);
  cvf.put64(3, 13);
  cvf.put(4, 14);
  cvf.put64(5, 15);
  cvf.put(6, "Keso");
  cvf.put(7, "Kent");
  cvf.closeSection();

  cvf.createSection(CONFIG_SECTION_NODE, DATA_NODE_TYPE);
  cvf.put(CONFIG_NODE_ID, 1);
  cvf.put(2, 22);
  cvf.put(4, 24);
  cvf.put64(5, 25);
  cvf.put(6, "Kalle");
  cvf.put(7, "Anka");
  cvf.closeSection();

  cvf.createSection(CONFIG_SECTION_NODE, API_NODE_TYPE);
  cvf.put(CONFIG_NODE_ID, 10);
  cvf.closeSection();
  cvf.createSection(CONFIG_SECTION_NODE, MGM_NODE_TYPE);
  cvf.put(CONFIG_NODE_ID, 20);
  cvf.closeSection();

  cvf.createSection(CONFIG_SECTION_CONNECTION, TCP_TYPE);
  cvf.put(CONFIG_FIRST_NODE_ID, 1);
  cvf.put(CONFIG_SECOND_NODE_ID, 2);
  cvf.closeSection();
  cvf.commit(false);

  ndbout_c("-- print --");
  print(* cvf.m_cfg);

  ndbout_c("packed size: %d", cvf.m_cfg->get_v1_packed_size());
  ndbout_c("packed size v2: %d", cvf.m_cfg->get_v2_packed_size(0));

  ConfigValues::ConstIterator iter(* cvf.m_cfg);
  require(iter.openSection(CONFIG_SECTION_NODE, 0));
  ConfigValues * cfg2 = ConfigValuesFactory::extractCurrentSection(iter);
  cvf.closeSection();
  print(99, * cfg2);

  ndbout_c("packed size: %d", cfg2->get_v1_packed_size());
  delete cfg2;

  {
    UtilBuffer buf;
    Uint32 l1 = cvf.m_cfg->pack_v1(buf);
    Uint32 l2 = cvf.m_cfg->get_v1_packed_size();
    require(l1 == l2);

    ConfigValuesFactory cvf2;
    require(cvf2.unpack_v1_buf(buf));
    UtilBuffer buf2;
    Uint32 l3 = cvf2.m_cfg->pack_v1(buf2);
    require(l1 == l3);

    ndbout_c("unpack\n-- print --");
    print(*cvf2.m_cfg);
  }
  {
    UtilBuffer buf;
    Uint32 l1 = cvf.m_cfg->pack_v2(buf);
    Uint32 l2 = cvf.m_cfg->get_v2_packed_size(0);
    require(l1 == l2);

    ConfigValuesFactory cvf2;
    require(cvf2.unpack_v2_buf(buf));
    UtilBuffer buf2;
    Uint32 l3 = cvf2.m_cfg->pack_v2(buf2);
    require(l1 == l3);

    ndbout_c("unpack v2 \n-- print --");
    print(*cvf2.m_cfg);
  }
  ndb_end(0);
  return 0;
}
