/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <ndb_global.h>

#include <NdbOut.hpp>
#include "NdbThread.h"
#include <NdbMem.h>
#include <NdbMain.h>

NDB_COMMAND(ndbmem, "ndbmem", "ndbmem", "Test the ndbmem functionality", 4096){

  ndbout << "Starting test of NdbMem" << endl;
  ndbout << "=======================" << endl;

  ndbout << "Creating NdbMem" << endl;
  NdbMem_Create();

  
  ndbout << "NdbMem - test 1" << endl;
  if (argc == 2){
    int size1 = atoi(argv[1]);
    ndbout << "Allocate and test "<<size1<<" bytes of memory" << endl;
    char* mem1 = (char*)NdbMem_Allocate(size1);
    ndbout << "mem1 = " << hex << (int)mem1 << endl;
    if (mem1 != NULL){
      char* p1;

      // Write to the memory allocated 
      p1 = mem1;
      for(int i = 0; i < size1; i++){
	*p1 = (char)(i%256);
	p1++;
      }

      // Read from the memory and check value
      char read1;
      char* pread1;
      pread1 = mem1;
      for(int i = 0; i < size1; i++){
	read1 = *pread1;
	//ndbout << i << "=" << read1 << endl;
	if (read1 != (i%256))
	  ndbout << "Byte " << i << " was not correct, read1=" << read1 << endl;
	pread1++;
      }

      ndbout << "Freeing NdbMem" << endl;
      NdbMem_Free(mem1);
    }

    ndbout << "Destroying NdbMem" << endl;
    NdbMem_Destroy();
  }else{
    ndbout << "Usage: ndbmem <size(bytes)>"<< endl;
  }

  return NULL;

}



