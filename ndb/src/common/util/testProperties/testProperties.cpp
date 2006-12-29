/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include "Properties.hpp"
#include <NdbOut.hpp>

#include "uucode.h"

bool
writeToFile(const Properties & p, const char * fname, bool uu = true){
  Uint32 sz = p.getPackedSize();
  char * buffer = (char*)malloc(sz);
  
  FILE * f = fopen(fname, "wb");
  bool res = p.pack((Uint32*)buffer);
  if(res != true){
    ndbout << "Error packing" << endl;
    ndbout << "p.getPropertiesErrno() = " << p.getPropertiesErrno() << endl;
    ndbout << "p.getOSErrno()         = " << p.getOSErrno() << endl;
  }
  if(uu)
    uuencode(buffer, sz, f);
  else {
    fwrite(buffer, 1, sz, f);
  }
      
  fclose(f);
  free(buffer);
  return res;
}

bool
readFromFile(Properties & p, const char *fname, bool uu = true){
  Uint32 sz = 30000;
  char * buffer = (char*)malloc(sz);
  FILE * f = fopen(fname, "rb");
  if(uu)
    uudecode(f, buffer, sz);
  else 
    fread(buffer, 1, sz, f);
  fclose(f);
  bool res = p.unpack((Uint32*)buffer, sz);
  if(res != true){
    ndbout << "Error unpacking" << endl;
    ndbout << "p.getPropertiesErrno() = " << p.getPropertiesErrno() << endl;
    ndbout << "p.getOSErrno()         = " << p.getOSErrno() << endl;
  }
  free(buffer);
  return res;
}

void putALot(Properties & tmp){
  int i = 123;
  tmp.put("LockPagesInMainMemory", i++);
  tmp.put("SleepWhenIdle", i++);
  tmp.put("NoOfSignalsToExecuteBetweenCommunicationInterfacePoll", i++);
  tmp.put("TimeBetweenWatchDogCheck", i++);
  tmp.put("StopOnError", i++);
  
  tmp.put("MaxNoOfConcurrentOperations", i++);
  tmp.put("MaxNoOfConcurrentTransactions", i++);
  tmp.put("MemorySpaceIndexes", i++);
  tmp.put("MemorySpaceTuples", i++);
  tmp.put("MemoryDiskPages", i++);
  tmp.put("NoOfFreeDiskClusters", i++);
  tmp.put("NoOfDiskClusters", i++);
  
  tmp.put("TimeToWaitAlive", i++);
  tmp.put("HeartbeatIntervalDbDb", i++);
  tmp.put("HeartbeatIntervalDbApi", i++);
  tmp.put("TimeBetweenInactiveTransactionAbortCheck", i++);
  
  tmp.put("TimeBetweenLocalCheckpoints", i++);
  tmp.put("TimeBetweenGlobalCheckpoints", i++);
  tmp.put("NoOfFragmentLogFiles", i++);
  tmp.put("NoOfConcurrentCheckpointsDuringRestart", i++);
  tmp.put("TransactionInactiveTimeBeforeAbort", i++);
  tmp.put("NoOfConcurrentProcessesHandleTakeover", i++);
  
  tmp.put("NoOfConcurrentCheckpointsAfterRestart", i++);
  
  tmp.put("NoOfDiskPagesToDiskDuringRestartTUP", i++);
  tmp.put("NoOfDiskPagesToDiskAfterRestartTUP", i++);
  tmp.put("NoOfDiskPagesToDiskDuringRestartACC", i++);
  tmp.put("NoOfDiskPagesToDiskAfterRestartACC", i++);
  
  tmp.put("NoOfDiskClustersPerDiskFile", i++);
  tmp.put("NoOfDiskFiles", i++);

  // Always found
  tmp.put("NoOfReplicas",      33);
  tmp.put("MaxNoOfAttributes", 34);
  tmp.put("MaxNoOfTables",     35);
}

int
main(void){
  Properties p;

  p.put("Kalle", 1);
  p.put("Ank1", "anka");
  p.put("Ank2", "anka");
  p.put("Ank3", "anka");
  p.put("Ank4", "anka");
  putALot(p);

  Properties tmp;
  tmp.put("Type", "TCP");
  tmp.put("OwnNodeId", 1);
  tmp.put("RemoteNodeId", 2);
  tmp.put("OwnHostName", "local");
  tmp.put("RemoteHostName", "remote");
  
  tmp.put("SendSignalId", 1);
  tmp.put("Compression", (Uint32)false);
  tmp.put("Checksum", 1);
  
  tmp.put64("SendBufferSize", 2000);
  tmp.put64("MaxReceiveSize", 1000);
  
  tmp.put("PortNumber", 1233);
  putALot(tmp);

  p.put("Connection", 1, &tmp);

  p.put("NoOfConnections", 2);
  p.put("NoOfConnection2", 2);

  p.put("kalle", 3);
  p.put("anka", "kalle");
  
  Properties p2;
  p2.put("kalle", "anka");

  p.put("prop", &p2);

  p.put("Connection", 2, &tmp);

  p.put("Connection", 3, &tmp);

  p.put("Connection", 4, &tmp);
  /*
  */
  
  Uint32 a = 99;
  const char * b;
  const Properties * p3;
  Properties * p4;
  
  bool bb = p.get("kalle", &a);
  bool cc = p.get("anka", &b);
  bool dd = p.get("prop", &p3);
  if(p.getCopy("prop", &p4))
    delete p4;
  
  p2.put("p2", &p2);
  
  p.put("prop2", &p2);
  /* */  

  p.print(stdout, "testing 1: ");
  
  writeToFile(p, "A_1");
  writeToFile(p, "B_1", false);
  
  Properties r1;
  readFromFile(r1, "A_1");
  writeToFile(r1, "A_3");
  
  //r1.print(stdout, "testing 2: ");
  Properties r2;
  readFromFile(r2, "A_1");
  writeToFile(r2, "A_4");
  
  Properties r3;
  readFromFile(r3, "B_1", false);
  writeToFile(r3, "A_5");
  r3.print(stdout, "testing 3: ");  

  return 0;
}
