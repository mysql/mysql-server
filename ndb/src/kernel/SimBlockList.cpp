/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "SimBlockList.hpp"
#include <SimulatedBlock.hpp>
#include <Cmvmi.hpp>
#include <Ndbfs.hpp>
#include <Dbacc.hpp>
#include <Dbdict.hpp>
#include <Dbdih.hpp>
#include <Dblqh.hpp>
#include <Dbtc.hpp>
#include <Dbtup.hpp>
#include <Ndbcntr.hpp>
#include <Qmgr.hpp>
#include <Trix.hpp>
#include <Backup.hpp>
#include <DbUtil.hpp>
#include <Suma.hpp>
#include <Grep.hpp>
#include <Dbtux.hpp>
#include <NdbEnv.h>

enum SIMBLOCKLIST_DUMMY { A_VALUE = 0 };

static
void * operator new (size_t sz, SIMBLOCKLIST_DUMMY dummy){
  char * tmp = (char *)malloc(sz);

#ifndef NDB_PURIFY
#ifdef VM_TRACE
  const int initValue = 0xf3;
#else
  const int initValue = 0x0;
#endif
  
  const int p = (sz / 4096);
  const int r = (sz % 4096);
  
  for(int i = 0; i<p; i++)
    memset(tmp+(i*4096), initValue, 4096);
  
  if(r > 0)
    memset(tmp+p*4096, initValue, r);

#endif
  
  return tmp;
}

void 
SimBlockList::load(const Configuration & conf){
  noOfBlocks = 16;
  theList = new SimulatedBlock * [noOfBlocks];
  for(int i = 0; i<noOfBlocks; i++)
    theList[i] = 0;
  Dbdict* dbdict = 0;
  Dbdih* dbdih = 0;

  SimulatedBlock * fs = 0;
  {
    char buf[100];
    if(NdbEnv_GetEnv("NDB_NOFS", buf, 100) == 0){
      fs = new (A_VALUE) Ndbfs(conf);
    } else { 
      fs = new (A_VALUE) VoidFs(conf);
    }
  }

  theList[0]  = new (A_VALUE) Dbacc(conf);
  theList[1]  = new (A_VALUE) Cmvmi(conf);
  theList[2]  = fs;
  theList[3]  = dbdict = new (A_VALUE) Dbdict(conf);
  theList[4]  = dbdih = new (A_VALUE) Dbdih(conf);
  theList[5]  = new (A_VALUE) Dblqh(conf);
  theList[6]  = new (A_VALUE) Dbtc(conf);
  theList[7]  = new (A_VALUE) Dbtup(conf);
  theList[8]  = new (A_VALUE) Ndbcntr(conf);
  theList[9]  = new (A_VALUE) Qmgr(conf);
  theList[10] = new (A_VALUE) Trix(conf);
  theList[11] = new (A_VALUE) Backup(conf);
  theList[12] = new (A_VALUE) DbUtil(conf);
  theList[13] = new (A_VALUE) Suma(conf);
  theList[14] = new (A_VALUE) Grep(conf);
  theList[15] = new (A_VALUE) Dbtux(conf);

  // Metadata common part shared by block instances
  ptrMetaDataCommon = new MetaData::Common(*dbdict, *dbdih);
  for (int i = 0; i < noOfBlocks; i++)
    theList[i]->setMetaDataCommon(ptrMetaDataCommon);
}

void
SimBlockList::unload(){
  if(theList != 0){
    for(int i = 0; i<noOfBlocks; i++){
      if(theList[i] != 0){
	theList[i]->~SimulatedBlock();
	free(theList[i]);
	theList[i] = 0;
      }
    }
    delete [] theList;
    delete ptrMetaDataCommon;
    theList    = 0;
    noOfBlocks = 0;
    ptrMetaDataCommon = 0;
  }
}
