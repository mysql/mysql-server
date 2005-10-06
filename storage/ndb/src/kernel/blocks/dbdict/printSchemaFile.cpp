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
static bool equalcontents = false;
static bool okquiet = false;

static void 
usage()
{
  ndbout
    << "Usage: " << progname << " [-aceq]" << " file ..." << endl
    << "-a      print also unused slots" << endl
    << "-c      check only (return status 1 on error)" << endl
    << "-e      check also that the files have identical contents" << endl
    << "-q      no output if file is ok" << endl
    << "Example: " << progname << " -ceq ndb_*_fs/D[12]/DBDICT/P0.SchemaLog" << endl;
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

static const char*
version(Uint32 v)
{
  static char buf[40];
  sprintf(buf, "%d.%d.%d", v >> 16, (v >> 8) & 0xFF, v & 0xFF);
  return buf;
}

static int
print_head(const char * filename, const SchemaFile * sf)
{
  int retcode = 0;

  if (! checkonly) {
    ndbout << "----- Schemafile: " << filename << " -----" << endl;
    ndbout_c("Magic: %.*s ByteOrder: %.8x NdbVersion: %s FileSize: %d",
             sizeof(sf->Magic),
             sf->Magic, 
             sf->ByteOrder, 
             version(sf->NdbVersion),
             sf->FileSize);
  }

  if (memcmp(sf->Magic, "NDBSCHMA", sizeof(sf->Magic) != 0)) {
    ndbout << filename << ": invalid header magic" << endl;
    retcode = 1;
  }

  if ((sf->NdbVersion >> 16) < 4 || (sf->NdbVersion >> 16) > 9) {
    ndbout << filename << ": impossible version " << hex << sf->NdbVersion << endl;
    retcode = 1;
  }

  return retcode;
}

static int
print_old(const char * filename, const SchemaFile * sf, Uint32 sz)
{
  int retcode = 0;

  if (print_head(filename, sf) != 0)
    retcode = 1;

  for (Uint32 i = 0; i < sf->NoOfTableEntries; i++) {
    SchemaFile::TableEntry_old te = sf->TableEntries_old[i];
    if (allflag ||
        (te.m_tableState != SchemaFile::INIT &&
         te.m_tableState != SchemaFile::DROP_TABLE_COMMITTED)) {
      if (! checkonly)
        ndbout << "Table " << i << ":"
               << " State = " << te.m_tableState 
               << " version = " << te.m_tableVersion
               << " type = " << te.m_tableType
               << " noOfPages = " << te.m_noOfPages
               << " gcp: " << te.m_gcp << endl;
    }
  }
  return retcode;
}

static int
print(const char * filename, const SchemaFile * xsf, Uint32 sz)
{
  int retcode = 0;

  if (print_head(filename, xsf) != 0)
    retcode = 1;

  assert(sizeof(SchemaFile) == NDB_SF_PAGE_SIZE);
  if (xsf->FileSize != sz || xsf->FileSize % NDB_SF_PAGE_SIZE != 0) {
    ndbout << filename << ": invalid FileSize " << xsf->FileSize << endl;
    retcode = 1;
  }
  Uint32 noOfPages = xsf->FileSize / NDB_SF_PAGE_SIZE;
  for (Uint32 n = 0; n < noOfPages; n++) {
    if (! checkonly) {
      ndbout << "----- Page: " << n << " (" << noOfPages << ") -----" << endl;
    }
    const SchemaFile * sf = &xsf[n];
    if (memcmp(sf->Magic, xsf->Magic, sizeof(sf->Magic)) != 0) {
      ndbout << filename << ": page " << n << " invalid magic" << endl;
      retcode = 1;
    }
    if (sf->FileSize != xsf->FileSize) {
      ndbout << filename << ": page " << n << " FileSize changed to " << sf->FileSize << "!=" << xsf->FileSize << endl;
      retcode = 1;
    }
    Uint32 cs = 0;
    for (Uint32 j = 0; j < NDB_SF_PAGE_SIZE_IN_WORDS; j++)
      cs ^= ((const Uint32*)sf)[j];
    if (cs != 0) {
      ndbout << filename << ": page " << n << " invalid CheckSum" << endl;
      retcode = 1;
    }
    if (sf->NoOfTableEntries != NDB_SF_PAGE_ENTRIES) {
      ndbout << filename << ": page " << n << " invalid NoOfTableEntries " << sf->NoOfTableEntries << endl;
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
        ndbout << filename << ": entry " << j << " garbage in m_unused[3]" << endl;
        retcode = 1;
      }
    }
  }

  return retcode;
}

NDB_COMMAND(printSchemafile, 
	    "printSchemafile", "printSchemafile", "Prints a schemafile", 16384)
{ 
  progname = argv[0];
  int exitcode = 0;

  while (argc > 1 && argv[1][0] == '-') {
    if (strchr(argv[1], 'a') != 0)
      allflag = true;
    if (strchr(argv[1], 'c') != 0)
      checkonly = true;
    if (strchr(argv[1], 'e') != 0)
      equalcontents = true;
    if (strchr(argv[1], 'q') != 0)
      okquiet = true;
    if (strchr(argv[1], 'h') != 0 || strchr(argv[1], '?') != 0) {
      usage();
      return 0;
    }
    argc--, argv++;
  }

  const char * prevfilename = 0;
  Uint32 * prevbuf = 0;
  Uint32 prevbytes = 0;

  while (argc > 1) {
    const char * filename = argv[1];
    argc--, argv++;

    struct stat sbuf;
    const int res = stat(filename, &sbuf);
    if (res != 0) {
      ndbout << filename << ": not found errno=" << errno << endl;
      exitcode = 1;
      continue;
    }
    const Uint32 bytes = sbuf.st_size;
    
    Uint32 * buf = new Uint32[bytes/4+1];
    
    FILE * f = fopen(filename, "rb");
    if (f == 0) {
      ndbout << filename << ": open failed errno=" << errno << endl;
      delete [] buf;
      exitcode = 1;
      continue;
    }
    Uint32 sz = fread(buf, 1, bytes, f);
    fclose(f);
    if (sz != bytes) {
      ndbout << filename << ": read failed errno=" << errno << endl;
      delete [] buf;
      exitcode = 1;
      continue;
    }

    if (sz < 32) {
      ndbout << filename << ": too short (no header)" << endl;
      delete [] buf;
      exitcode = 1;
      continue;
    }

    SchemaFile* sf = (SchemaFile *)&buf[0];
    int ret;
    if (sf->NdbVersion < NDB_SF_VERSION_5_0_6)
      ret = print_old(filename, sf, sz);
    else
      ret = print(filename, sf, sz);

    if (ret != 0) {
      ndbout << filename << ": check failed"
             << " version=" << version(sf->NdbVersion) << endl;
      exitcode = 1;
    } else if (! okquiet) {
      ndbout << filename << ": ok"
             << " version=" << version(sf->NdbVersion) << endl;
    }

    if (equalcontents && prevfilename != 0) {
      if (prevbytes != bytes || memcmp(prevbuf, buf, bytes) != 0) {
        ndbout << filename << ": differs from " << prevfilename << endl;
        exitcode = 1;
      }
    }

    prevfilename = filename;
    delete [] prevbuf;
    prevbuf = buf;
    prevbytes = bytes;
  }

  delete [] prevbuf;
  return exitcode;
}
