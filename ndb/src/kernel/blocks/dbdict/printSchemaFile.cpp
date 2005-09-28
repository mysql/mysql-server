#if 0
make -f Makefile -f - printSchemaFile <<'_eof_'
printSchemaFile: printSchemaFile.cpp SchemaFile.hpp
	$(CXXCOMPILE) -o $@ $@.cpp -L../../../common/util/.libs -lgeneral
ifneq ($(MYSQL_HOME),)
	ln -sf `pwd`/$@ $(MYSQL_HOME)/bin/$@
endif
_eof_
exit $?
#endif

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
#include <ndb_version.h>

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <SchemaFile.hpp>

static const char* progname = 0;
static bool allflag = false;
static bool checkonly = false;
static int xitcode = 0;

static void 
usage()
{
  ndbout << "Usage " << progname
         << " [-ac]"
         << " P0.SchemaLog" << endl;  
}

static void
fill(const char * buf, int mod)
{
  int len = strlen(buf)+1;
  ndbout << buf << " ";
  while((len % mod) != 0){
    ndbout << " ";
    len++;
  }
}

static void
print_head(const char * filename, const SchemaFile * sf)
{
  if (! checkonly) {
    ndbout << "----- Schemafile: " << filename << " -----" << endl;
    ndbout_c("Magic: %.*s ByteOrder: %.8x NdbVersion: %d.%d.%d FileSize: %d",
             sizeof(sf->Magic),
             sf->Magic, 
             sf->ByteOrder, 
             sf->NdbVersion >> 16,
             (sf->NdbVersion >> 8) & 0xFF,
             sf->NdbVersion & 0xFF,
             sf->FileSize);
  }
}

static void 
print_old(const char * filename, const SchemaFile * sf)
{
  print_head(filename, sf);

  for (Uint32 i = 0; i < sf->NoOfTableEntries; i++) {
    SchemaFile::TableEntry_old te = sf->TableEntries_old[i];
    if (allflag ||
        (te.m_tableState != SchemaFile::INIT &&
         te.m_tableState != SchemaFile::DROP_TABLE_COMMITTED)) {
      ndbout << "Table " << i << ":"
             << " State = " << te.m_tableState 
	     << " version = " << te.m_tableVersion
             << " type = " << te.m_tableType
	     << " noOfPages = " << te.m_noOfPages
	     << " gcp: " << te.m_gcp << endl;
    }
  }
}

static void 
print(const char * filename, const SchemaFile * xsf, Uint32 sz)
{
  int retcode = 0;

  print_head(filename, xsf);

  assert(sizeof(SchemaFile) == NDB_SF_PAGE_SIZE);
  if (xsf->FileSize != sz || xsf->FileSize % NDB_SF_PAGE_SIZE != 0) {
    ndbout << "***** invalid FileSize " << xsf->FileSize << endl;
    retcode = 1;
  }
  Uint32 noOfPages = xsf->FileSize / NDB_SF_PAGE_SIZE;
  for (Uint32 n = 0; n < noOfPages; n++) {
    if (! checkonly) {
      ndbout << "----- Page: " << n << " (" << noOfPages << ") -----" << endl;
    }
    const SchemaFile * sf = &xsf[n];
    if (sf->FileSize != xsf->FileSize) {
      ndbout << "***** page " << n << " FileSize changed to " << sf->FileSize << "!=" << xsf->FileSize << endl;
      retcode = 1;
    }
    Uint32 cs = 0;
    for (Uint32 j = 0; j < NDB_SF_PAGE_SIZE_IN_WORDS; j++)
      cs ^= ((const Uint32*)sf)[j];
    if (cs != 0) {
      ndbout << "***** page " << n << " invalid CheckSum" << endl;
      retcode = 1;
    }
    if (sf->NoOfTableEntries != NDB_SF_PAGE_ENTRIES) {
      ndbout << "***** page " << n << " invalid NoOfTableEntries " << sf->NoOfTableEntries << endl;
      retcode = 1;
    }
    for (Uint32 i = 0; i < NDB_SF_PAGE_ENTRIES; i++) {
      SchemaFile::TableEntry te = sf->TableEntries[i];
      Uint32 j = n * NDB_SF_PAGE_ENTRIES + i;
      if (allflag ||
          (te.m_tableState != SchemaFile::INIT &&
           te.m_tableState != SchemaFile::DROP_TABLE_COMMITTED)) {
        if (! checkonly)
          ndbout << "Table " << j << ":"
                 << " State = " << te.m_tableState 
                 << " version = " << te.m_tableVersion
                 << " type = " << te.m_tableType
                 << " noOfWords = " << te.m_info_words
                 << " gcp: " << te.m_gcp << endl;
      }
      if (te.m_unused[0] != 0 || te.m_unused[1] != 0 || te.m_unused[2] != 0) {
        ndbout << "***** entry " << j << " garbage in m_unused[3]" << endl;
        retcode = 1;
      }
    }
  }

  if (retcode != 0)
    xitcode = 1;
  else if (checkonly)
    ndbout << "ok: " << filename << endl;
}

NDB_COMMAND(printSchemafile, 
	    "printSchemafile", "printSchemafile", "Prints a schemafile", 16384)
{ 
  progname = argv[0];

  while (argv[1][0] == '-') {
    if (strchr(argv[1], 'a') != 0)
      allflag = true;
    if (strchr(argv[1], 'c') != 0)
      checkonly = true;
    argc--, argv++;
  }

  while (argc > 1) {
    const char * filename = argv[1];
    argc--, argv++;

    struct stat sbuf;
    const int res = stat(filename, &sbuf);
    if (res != 0) {
      ndbout << "Could not find file: \"" << filename << "\"" << endl;
      return 1;
    }
    const Uint32 bytes = sbuf.st_size;
    
    Uint32 * buf = new Uint32[bytes/4+1];
    
    FILE * f = fopen(filename, "rb");
    if (f == 0) {
      ndbout << "Failed to open file" << endl;
      delete [] buf;
      return 1;
    }
    Uint32 sz = fread(buf, 1, bytes, f);
    fclose(f);
    if (sz != bytes) {
      ndbout << "Failure while reading file" << endl;
      delete [] buf;
      return 1;
    }

    SchemaFile* sf = (SchemaFile *)&buf[0];
    if (sf->NdbVersion < NDB_SF_VERSION_5_0_6)
      print_old(filename, sf);
    else
      print(filename, sf, sz);
    delete [] buf;
  }

  return xitcode;
}
