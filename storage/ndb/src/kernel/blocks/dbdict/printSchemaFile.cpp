/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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
#include <util/version.h>
#include "my_sys.h"
#include "my_dir.h"
#include "my_thread_local.h"

#include <NdbOut.hpp>
#include "SchemaFile.hpp"
#include <kernel_types.h>

#define JAM_FILE_ID 463


static const char* progname = 0;
static bool allflag = false;
static bool checkonly = false;
static bool equalcontents = false;
static bool okquiet = false;
static bool transok = false;

static void 
usage()
{
  ndbout
    << "Usage: " << progname << " [-aceq]" << " file ..." << endl
    << "-a      print also unused slots" << endl
    << "-c      check only (return status 1 on error)" << endl
    << "-e      check also that the files have identical contents" << endl
    << "-q      no output if file is ok" << endl
    << "-t      non-zero trans key is not error (has active trans)" << endl
    << "Example: " << progname << " -ceq ndb_*_fs/D[12]/DBDICT/P0.SchemaLog" << endl;
}

#ifdef NOT_USED

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
#endif

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
             (int) sizeof(sf->Magic),
             sf->Magic, 
             sf->ByteOrder, 
             version(sf->NdbVersion),
             sf->FileSize);
  }

  if (memcmp(sf->Magic, "NDBSCHMA", sizeof(sf->Magic)) != 0)
  {
    ndbout << filename << ": invalid header magic" << endl;
    retcode = 1;
  }

  if ((sf->NdbVersion >> 16) < 4 || (sf->NdbVersion >> 16) > 9) {
    ndbout << filename << ": impossible version " << hex << sf->NdbVersion << endl;
    retcode = 1;
  }

  return retcode;
}

[[noreturn]] inline void ndb_end_and_exit(int exitcode)
{
  ndb_end(0);
  exit(exitcode);
}

inline
Uint32
table_version_minor(Uint32 ver)
{
  return ver >> 24;
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
        (te.m_tableState != SchemaFile::SF_UNUSED))
    {
      if (! checkonly)
        ndbout << "Table " << i << ":"
               << " State = " << te.m_tableState 
	       << " version = " << table_version_major(te.m_tableVersion)
	       << "(" << table_version_minor(te.m_tableVersion) << ")"
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
      bool entryerr = false;
      if (! transok && te.m_transId != 0) {
        ndbout << filename << ": entry " << j << ": active transaction, transId: " << hex << te.m_transId << endl;
        entryerr = true;
        retcode = 1;
      }
      const Uint32 num_unused = sizeof(te.m_unused) / sizeof(te.m_unused[0]);
      for (Uint32 k = 0; k < num_unused; k++) {
        if (te.m_unused[k] != 0) {
          ndbout << filename << ": entry " << j << ": garbage in unused word " << k << ": " << te.m_unused[k] << endl;
          entryerr = true;
          retcode = 1;
        }
      }
      if (allflag ||
          (te.m_tableState != SchemaFile::SF_UNUSED) ||
          entryerr) {
        if (! checkonly || entryerr)
          ndbout << "Table " << j << ":"
                 << " State = " << te.m_tableState 
		 << " version = " << table_version_major(te.m_tableVersion)
		 << "(" << table_version_minor(te.m_tableVersion) << ")"
                 << " type = " << te.m_tableType
                 << " noOfWords = " << te.m_info_words
                 << " gcp: " << te.m_gcp
                 << " transId: " << hex << te.m_transId << endl;
      }
    }
  }

  return retcode;
}


int main(int argc, char** argv)
{
  ndb_init();
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
    if (strchr(argv[1], 't') != 0)
      transok = true;
    if (strchr(argv[1], 'h') != 0 || strchr(argv[1], '?') != 0) {
      usage();
      ndb_end_and_exit(0);
    }
    argc--, argv++;
  }

  const char * prevfilename = 0;
  Uint32 * prevbuf = 0;
  Uint32 prevbytes = 0;

  while (argc > 1) {
    const char * filename = argv[1];
    argc--, argv++;

    MY_STAT sbuf,*st;
    if(!(st=my_stat(filename, &sbuf,0)))
    {
      ndbout << filename << ": not found my_errno=" << my_errno() << endl;
      exitcode = 1;
      continue;
    }
    const Uint32 bytes = (Uint32)sbuf.st_size;
    
    Uint32 * buf = new Uint32[bytes/4+1];
    
    FILE * f = fopen(filename, "rb");
    if (f == 0) {
      ndbout << filename << ": open failed errno=" << errno << endl;
      delete [] buf;
      exitcode = 1;
      continue;
    }
    Uint32 sz = (Uint32)fread(buf, 1, bytes, f);
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
  ndb_end_and_exit(exitcode);
}
