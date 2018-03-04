/*
   Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbOut.hpp>

#define JAM_FILE_ID 358

inline void ndb_end_and_exit(int exitcode)
{
  ndb_end(0);
  exit(exitcode);
}

void 
usage(const char * prg){
  ndbout << "Usage " << prg 
	 << " S[0-20000].FragList" << endl;  
}

void
fill(const char * buf, int mod){
  int len = (int)(strlen(buf)+1);
  ndbout << buf << " ";
  while((len % mod) != 0){
    ndbout << " ";
    len++;
  }
}

Uint32 readWord(Uint32 &index, Uint32 *buf)
{
  if ((index % 2048) == 0)
  {
    index += 32;
  }
  index++;
  return buf[index];
}

void readFragment(Uint32 &numStoredReplicas,
                  Uint32 &index,
                  Uint32 *buf)
{
  Uint32 fragId = readWord(index, buf);
  Uint32 prefPrimary = readWord(index, buf);
  numStoredReplicas = readWord(index, buf);
  Uint32 numOldStoredReplicas = readWord(index, buf);
  Uint32 distKey = readWord(index, buf);
  Uint32 logPartId = readWord(index, buf);

  ndbout << "------ Fragment with FragId: " << fragId;
  ndbout << " --------" << endl;
  ndbout << "Preferred Primary: " << prefPrimary;
  ndbout << " numStoredReplicas: " << numStoredReplicas;
  ndbout << " numOldStoredReplicas: " << numOldStoredReplicas;
  ndbout << " distKey: " << distKey;
  ndbout << " LogPartId: " << logPartId << endl;
}

void readReplica(Uint32 &index, Uint32 *buf)
{
  Uint32 i;
  Uint32 procNode = readWord(index, buf);
  Uint32 initialGci = readWord(index, buf);
  Uint32 numCrashedReplicas = readWord(index, buf);
  Uint32 nextLcp = readWord(index, buf);

  ndbout << "Replica node is: " << procNode;
  ndbout << " initialGci: " << initialGci;
  ndbout << " numCrashedReplicas = " << numCrashedReplicas;
  ndbout << " nextLcpNo = " << nextLcp << endl;
  for (i = 0; i < 3; i++)
  {
    Uint32 maxGciCompleted = readWord(index, buf);
    Uint32 maxGciStarted = readWord(index, buf);
    Uint32 lcpId = readWord(index, buf);
    Uint32 lcpStatus = readWord(index, buf);

    if (i == 2)
      continue;

    ndbout << "LcpNo[" << i << "]: ";
    ndbout << "maxGciCompleted: " << maxGciCompleted;
    ndbout << " maxGciStarted: " << maxGciStarted;
    ndbout << " lcpId: " << lcpId;
    ndbout << " lcpStatus: ";
    if (lcpStatus == 1)
    {
      ndbout << "valid";
    }
    else if (lcpStatus == 2)
    {
      ndbout << "invalid";
    }
    else
    {
      ndbout << "error: set to " << lcpStatus;
    }
    ndbout << endl;
  }
  for (i = 0; i < 8; i++)
  {
    Uint32 createGci = readWord(index, buf);
    Uint32 replicaLastGci = readWord(index, buf);
    
    if (i < numCrashedReplicas)
    {
      ndbout << "Crashed_replica[" << i << "]: ";
      ndbout << "CreateGci: " << createGci;
      ndbout << " replicaLastGci:" << replicaLastGci << endl;
    }
  }
}

static void
print(const char *filename, Uint32 *buf, Uint32 size)
{
  Uint32 index = 0;
  ndbout << "Filename: " << filename << " with size " << size << endl;

  Uint32 noOfPages = readWord(index, buf);
  if (noOfPages * 8192 != size)
  {
    ndbout << "noOfPages is wrong: noOfPages = " << noOfPages << endl;
    return;
  }
  Uint32 noOfWords = readWord(index, buf);
  ndbout << "noOfPages = " << noOfPages;
  ndbout << " noOfWords = " << noOfWords << endl;
  ndbout << "Table Data" << endl;
  ndbout << "----------" << endl;
  Uint32 totalfrags = readWord(index, buf);
  Uint32 noOfBackups = readWord(index, buf);
  Uint32 hashpointer = readWord(index, buf);
  Uint32 kvalue = readWord(index, buf);
  Uint32 mask = readWord(index, buf);
  Uint32 tab_method = readWord(index, buf);
  Uint32 tab_storage = readWord(index, buf);

  ndbout << "Num Frags: " << totalfrags;
  ndbout << " NoOfReplicas: " << (noOfBackups + 1);
  ndbout << " hashpointer: " << hashpointer << endl;

  ndbout << "kvalue: " << kvalue;
  ndbout << " mask: " << hex << mask;
  ndbout << " method: ";
  switch (tab_method)
  {
  case 0:
    ndbout << "LinearHash";
    break;
  case 2:
    ndbout << "Hash";
    break;
  case 3:
    ndbout << "User Defined";
    break;
  case 4:
    ndbout << "HashMap";
    break;
  default:
    ndbout << "set to:" <<  tab_method;
    break;
  }
  ndbout << endl;

  ndbout << "Storage is on ";
  switch (tab_storage)
  {
    case 0:
      ndbout << "Logged, not checkpointed, doesn't survive SR";
      break;
    case 1:
      ndbout << "Logged and checkpointed, survives SR";
      break;
    case 2:
      ndbout << "Table is lost after SR";
      break;
    default:
      ndbout << "set to:" <<  tab_storage;
      break;
  }
  ndbout << endl;
  for (Uint32 i = 0; i < totalfrags; i++)
  {
    Uint32 j;
    Uint32 numStoredReplicas;
    readFragment(numStoredReplicas, index, buf);
    for (j = 0; j < numStoredReplicas; j++)
    {
      ndbout << "-------Stored Replica----------" << endl;
      readReplica(index, buf);
    }
    for ( ; j < (1 + noOfBackups); j++)
    {
      ndbout << "-------Old Stored Replica------" << endl;
      readReplica(index, buf);
    }
  }
}


int main(int argc, char** argv)
{
  ndb_init();
  if(argc != 2){
    usage(argv[0]);
    ndb_end_and_exit(0);
  }

  for (int i = 1; i<argc; i++)
  {
    const char * filename = argv[i];

    struct stat sbuf;

    if(stat(filename, &sbuf) != 0)
    {
      ndbout << "Could not find file: \"" << filename << "\"" << endl;
      continue;
    }
    const Uint32 bytes = sbuf.st_size;
    
    Uint32 * buf = new Uint32[bytes/4+1];
    
    FILE * f = fopen(filename, "rb");
    if(f == 0)
    {
      ndbout << "Failed to open file" << endl;
      delete [] buf;
      continue;
    }
    Uint32 sz = (Uint32)fread(buf, 1, bytes, f);
    fclose(f);
    if(sz != bytes)
    {
      ndbout << "Failure while reading file" << endl;
      delete [] buf;
      continue;
    }
    if (sz % 8192 != 0)
    {
      ndbout << "Size of file should be multiple of 8192" << endl;
      delete [] buf;
      continue;
    }
    
    print(filename, buf, sz);
    delete [] buf;
    continue;
  }
  ndb_end_and_exit(0);
}
