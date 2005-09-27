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


#include <ndb_global.h>

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <SchemaFile.hpp>

void 
usage(const char * prg){
  ndbout << "Usage " << prg 
	 << " P0.SchemaLog" << endl;  
}

void
fill(const char * buf, int mod){
  int len = strlen(buf)+1;
  ndbout << buf << " ";
  while((len % mod) != 0){
    ndbout << " ";
    len++;
  }
}

void 
print(const char * filename, const SchemaFile * file){
  ndbout << "----- Schemafile: " << filename << " -----" << endl;
  ndbout_c("Magic: %.*s ByteOrder: %.8x NdbVersion: %d FileSize: %d",
	   sizeof(file->Magic), file->Magic, 
	   file->ByteOrder, 
	   file->NdbVersion,
	   file->FileSize);

  for(Uint32 i = 0; i<file->NoOfTableEntries; i++){
    SchemaFile::TableEntry te = file->TableEntries[i];
    if(te.m_tableState != SchemaFile::INIT){
      ndbout << "Table " << i << ": State = " << te.m_tableState 
	     << " version = " << te.m_tableVersion
             << " type = " << te.m_tableType
	     << " noOfPages = " << te.m_noOfPages
	     << " gcp: " << te.m_gcp << endl;
    }
  }
}

NDB_COMMAND(printSchemafile, 
	    "printSchemafile", "printSchemafile", "Prints a schemafile", 16384){ 
  if(argc < 2){
    usage(argv[0]);
    return 0;
  }

  const char * filename = argv[1];

  struct stat sbuf;
  const int res = stat(filename, &sbuf);
  if(res != 0){
    ndbout << "Could not find file: \"" << filename << "\"" << endl;
    return 0;
  }
  const Uint32 bytes = sbuf.st_size;
  
  Uint32 * buf = new Uint32[bytes/4+1];
  
  FILE * f = fopen(filename, "rb");
  if(f == 0){
    ndbout << "Failed to open file" << endl;
    delete [] buf;
    return 0;
  }
  Uint32 sz = fread(buf, 1, bytes, f);
  fclose(f);
  if(sz != bytes){
    ndbout << "Failure while reading file" << endl;
    delete [] buf;
    return 0;
  }
  
  print(filename, (SchemaFile *)&buf[0]);

  Uint32 chk = 0, i;
  for (i = 0; i < bytes/4; i++)
    chk ^= buf[i];
  if (chk != 0)
    ndbout << "Invalid checksum!" << endl;

  delete [] buf;
  return 0;
}
