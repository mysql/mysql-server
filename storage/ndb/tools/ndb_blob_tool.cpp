/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

static const char* opt_dbname = 0;
static my_bool opt_check_orphans = false;
static my_bool opt_delete_orphans = false;
static const char* opt_dump_file = 0;
static my_bool opt_verbose = false;

static FILE* g_dump_file = 0;
static FileOutputStream* g_dump_out = 0;
static NdbOut g_dump;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;

static const char* g_tabname = 0;
static const NdbDictionary::Table* g_tab = 0;

struct Pk { // table pk
  const char* colname;
  Pk() {
    colname = 0;
  }
  ~Pk() {
    delete [] colname;
  }
};
static Pk* g_pklist = 0;
static int g_pkcount = 0;

struct Blob { // blob column and table
  int blobno;
  int colno;
  const char* colname;
  const char* blobname;
  const NdbDictionary::Table* blobtab;
  Blob() {
    blobno = -1;
    colno = -1;
    colname = 0;
    blobname = 0;
    blobtab = 0;
  }
  ~Blob() {
    delete [] colname;
    delete [] blobname;
  }
};
static Blob* g_bloblist = 0;
static int g_blobcount = 0;

static NdbTransaction* g_scantx = 0;
static NdbScanOperation* g_scanop = 0;

struct Val { // attr value scanned from blob
  const char* colname;
  NdbRecAttr* ra;
  Val() {
    colname = 0;
    ra = 0;
  }
  ~Val() {
    delete [] colname;
  }
};
static Val* g_vallist = 0;
static int g_valcount = 0;

#define CHK1(b) \
  if (!(b)) { \
    ret = -1; \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    ret = -1; \
    break; \
  }

// re-inventing strdup
static const char*
newstr(const char* s)
{
  require(s != 0);
  char* s2 = new char [strlen(s) + 1];
  strcpy(s2, s);
  return s2;
}

static NdbError
getNdbError(Ndb_cluster_connection* ncc)
{
  NdbError err;
  err.code = g_ncc->get_latest_error();
  err.message = g_ncc->get_latest_error_msg();
  return err;
}

static int
doconnect()
{
  int ret = 0;
  do
  {
    g_ncc = new Ndb_cluster_connection(opt_ndb_connectstring);
    CHK2(g_ncc->connect(6, 5) == 0, getNdbError(g_ncc));
    CHK2(g_ncc->wait_until_ready(30, 10) == 0, getNdbError(g_ncc));

    g_ndb = new Ndb(g_ncc, opt_dbname);
    CHK2(g_ndb->init() == 0, g_ndb->getNdbError());
    CHK2(g_ndb->waitUntilReady(30) == 0, g_ndb->getNdbError());
    g_dic = g_ndb->getDictionary();

    g_info << "connected" << endl;
  }
  while (0);
  return ret;
}

static void
dodisconnect()
{
  delete g_ndb;
  delete g_ncc;
  g_info << "disconnected" << endl;
}

static int
scanblobstart(const Blob& b)
{
  int ret = 0;

  do
  {
    require(g_scantx == 0);
    g_scantx = g_ndb->startTransaction();
    CHK2(g_scantx != 0, g_ndb->getNdbError());

    g_scanop = g_scantx->getNdbScanOperation(b.blobtab);
    CHK2(g_scanop != 0, g_scantx->getNdbError());

    const NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
    CHK2(g_scanop->readTuples(lm) == 0, g_scanop->getNdbError());

    for (int i = 0; i < g_valcount; i++)
    {
      Val& v = g_vallist[i];
      v.ra = g_scanop->getValue(v.colname);
      CHK2(v.ra != 0, v.colname << ": " << g_scanop->getNdbError());
    }
    CHK1(ret == 0);

    CHK2(g_scantx->execute(NoCommit) == 0, g_scantx->getNdbError());
  }
  while (0);

  return ret;
}

static int
scanblobnext(const Blob& b, int& res)
{
  int ret = 0;
  do
  {
    res = g_scanop->nextResult();
    CHK2(res == 0 || res == 1, g_scanop->getNdbError());
    g_info << b.blobname << ": nextResult: res=" << res << endl;
  }
  while (0);
  return ret;
}

static void
scanblobclose(const Blob& b)
{
  if (g_scantx != 0)
  {
    g_ndb->closeTransaction(g_scantx);
    g_scantx = 0;
  }
}

static int
checkorphan(const Blob&b, int& res)
{
  int ret = 0;

  NdbTransaction* tx = 0;
  NdbOperation* op = 0;

  do
  {
    tx = g_ndb->startTransaction();
    CHK2(tx != 0, g_ndb->getNdbError());

    op = tx->getNdbOperation(g_tab);
    CHK2(op != 0, tx->getNdbError());

    const NdbOperation::LockMode lm = NdbOperation::LM_Read;
    CHK2(op->readTuple(lm) == 0, op->getNdbError());

    for (int i = 0; i < g_pkcount; i++)
    {
      Val& v = g_vallist[i];
      require(v.ra != 0);
      require(v.ra->isNULL() == 0);
      const char* data = v.ra->aRef();
      CHK2(op->equal(v.colname, data) == 0, op->getNdbError());
    }
    CHK1(ret == 0);

    // read something to be safe
    NdbRecAttr* ra0 = op->getValue(g_vallist[0].colname);
    require(ra0 != 0);

    // not sure about the rules
    require(tx->getNdbError().code == 0);
    tx->execute(Commit);
    if (tx->getNdbError().code == 626)
    {
      g_info << "parent not found" << endl;
      res = 1; // not found
    }
    else
    {
      CHK2(tx->getNdbError().code == 0, tx->getNdbError());
      res = 0; // found
    }
  }
  while (0);

  if (tx != 0)
    g_ndb->closeTransaction(tx);
  return ret;
}

static int
deleteorphan(const Blob& b)
{
  int ret = 0;

  NdbTransaction* tx = 0;

  do
  {
    tx = g_ndb->startTransaction();
    CHK2(tx != 0, g_ndb->getNdbError());

    CHK2(g_scanop->deleteCurrentTuple(tx) == 0, g_scanop->getNdbError());
    CHK2(tx->execute(Commit) == 0, tx->getNdbError());
  }
  while (0);

  if (tx != 0)
    g_ndb->closeTransaction(tx);
  return ret;
}

static int
doorphan(const Blob& b)
{
  int ret = 0;
  do
  {
    g_err << "processing blob #" << b.blobno << " " << b.colname
          << " " << b.blobname << endl;

    if (opt_dump_file)
    {
      g_dump << "column: " << b.colname << endl;
      g_dump << "blob: " << b.blobname << endl;
      g_dump << "orphans (table key; blob part number):" << endl;
    }

    int totcount = 0;
    int orphancount = 0;

    CHK1(scanblobstart(b) == 0);
    while (1)
    {
      int res;
      res = -1;
      CHK1(scanblobnext(b, res) == 0);
      if (res != 0)
        break;
      totcount++;
      res = -1;
      CHK1(checkorphan(b, res) == 0);
      if (res != 0)
      {
        orphancount++;
        if (opt_dump_file)
        {
          g_dump << "key: ";
          for (int i = 0; i < g_valcount; i++)
          {
            const Val& v = g_vallist[i];
            g_dump << *v.ra;
            if (i + 1 < g_valcount)
              g_dump << ";";
          }
          g_dump << endl;
        }
        if (opt_delete_orphans)
        {
          CHK1(deleteorphan(b) == 0);
        }
      }
    }
    CHK1(ret == 0);

    g_err << "total parts: " << totcount << endl;
    g_err << "orphan parts: " << orphancount << endl;
    if (opt_dump_file)
    {
      g_dump << "total parts: " << totcount << endl;
      g_dump << "orphan parts: " << orphancount << endl;
    }
  }
  while (0);

  scanblobclose(b);
  return ret;
}

static int
doorphans()
{
  int ret = 0;
  g_err << "processing " << g_blobcount << " blobs" << endl;
  for (int i = 0; i < g_blobcount; i++)
  {
    const Blob& b = g_bloblist[i];
    CHK1(doorphan(b) == 0);
  }
  return ret;
}

static int
isblob(const NdbDictionary::Column* c)
{
  if (c->getType() == NdbDictionary::Column::Blob ||
      c->getType() == NdbDictionary::Column::Text)
    if (c->getPartSize() != 0)
      return 1;
  return 0;
}

static int
getobjs()
{
  int ret = 0;
  do
  {
    g_tab = g_dic->getTable(g_tabname);
    CHK2(g_tab != 0, g_tabname << ": " << g_dic->getNdbError());
    const int tabid = g_tab->getObjectId();
    const int ncol = g_tab->getNoOfColumns();

    g_pklist = new Pk [NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
    for (int i = 0; i < ncol; i++)
    {
      const NdbDictionary::Column* c = g_tab->getColumn(i);
      if (c->getPrimaryKey())
      {
        Pk& p = g_pklist[g_pkcount++];
        const char* colname = c->getName();
        p.colname = newstr(colname);
      }
    }
    require(g_pkcount != 0 && g_pkcount == g_tab->getNoOfPrimaryKeys());

    g_valcount = g_pkcount + 1;
    g_vallist = new Val [g_valcount];
    for (int i = 0; i < g_valcount; i++)
    {
      Val& v = g_vallist[i];
      if (i < g_pkcount)
      {
        const Pk& p = g_pklist[i];
        v.colname = newstr(p.colname);
      }
      if (i == g_pkcount + 0) // first blob attr to scan
      {
        v.colname = newstr("NDB$PART");
      }
    }

    if (g_blobcount == 0)
    {
      for (int i = 0; i < ncol; i++)
      {
        const NdbDictionary::Column* c = g_tab->getColumn(i);
        if (isblob(c))
        {
          Blob& b = g_bloblist[g_blobcount++];
          const char* colname = c->getName();
          b.colname = newstr(colname);
        }
      }
    }

    for (int i = 0; i < g_blobcount; i++)
    {
      Blob& b = g_bloblist[i];
      b.blobno = i;
      const NdbDictionary::Column* c = g_tab->getColumn(b.colname);
      CHK2(c != 0, g_tabname << ": " << b.colname << ": no such column");
      CHK2(isblob(c), g_tabname << ": " << b.colname << ": not a blob");
      b.colno = c->getColumnNo();
      {
        char blobname[100];
        sprintf(blobname, "NDB$BLOB_%d_%d", tabid, b.colno);
        b.blobname = newstr(blobname);
      }
      b.blobtab = g_dic->getTable(b.blobname);
      CHK2(b.blobtab != 0, g_tabname << ": " << b.colname << ": " << b.blobname << ": " << g_dic->getNdbError());
    }
    CHK1(ret == 0);
  }
  while (0);
  return ret;
}

static int
doall()
{
  int ret = 0;
  do
  {
    if (opt_dump_file)
    {
      g_dump_file = fopen(opt_dump_file, "w");
      CHK2(g_dump_file != 0, opt_dump_file << ": " << strerror(errno));
      g_dump_out = new FileOutputStream(g_dump_file);
      new (&g_dump) NdbOut(*g_dump_out);

      const char* action = 0;
      if (opt_check_orphans)
        action = "check";
      if (opt_delete_orphans)
        action = "delete";

      g_dump << "table: " << g_tabname << endl;
      g_dump << "action: " << action << endl;
    }
    CHK1(doconnect() == 0);
    CHK1(getobjs() == 0);
    if (g_blobcount == 0)
    {
      g_err << g_tabname << ": no blob columns" << endl;
      break;
    }
    CHK1(doorphans() == 0);
  }
  while (0);

  dodisconnect();
  if (g_dump_file != 0)
  {
    g_dump << "result: "<< (ret == 0 ? "ok" : "failed") << endl;
    flush(g_dump);
    if (fclose(g_dump_file) != 0)
    {
      g_err << opt_dump_file << ": write failed: " << strerror(errno) << endl;
    }
    g_dump_file = 0;
  }
  return ret;
}

static struct my_option
my_long_options[] =
{
  NDB_STD_OPTS("ndb_blob_tool"),
  { "database", 'd',
    "Name of database table is in",
    (uchar**) &opt_dbname, (uchar**) &opt_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "check-orphans", NDB_OPT_NOSHORT,
    "Check for orphan blob parts",
    (uchar **)&opt_check_orphans, (uchar **)&opt_check_orphans, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "delete-orphans", NDB_OPT_NOSHORT,
    "Delete orphan blob parts",
    (uchar **)&opt_delete_orphans, (uchar **)&opt_delete_orphans, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "dump-file", NDB_OPT_NOSHORT,
    "Write orphan keys (table key and part number) into file",
    (uchar **)&opt_dump_file, (uchar **)&opt_dump_file, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", 'v',
    "Verbose messages",
    (uchar **)&opt_verbose, (uchar **)&opt_verbose, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0,
    0,
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

const char*
load_default_groups[]= { "mysql_cluster", 0 };

static void
short_usage_sub()
{
  ndb_short_usage_sub("table [blobcolumn..]");
  printf("Default is to process all blob columns\n");
  printf("(1) Check orphans with --check --dump=out1.txt\n");
  printf("(2) Delete orphans with --delete --dump=out2.txt\n");
}

static void
usage()
{
  printf("%s: check and repair blobs\n", my_progname);
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

static int
checkopts(int argc, char** argv)
{
  if (opt_dbname == 0)
    opt_dbname = "TEST_DB";

  if (argc < 1)
  {
    g_err << "Table name required" << endl;
    return 1;
  }
  g_tabname = newstr(argv[0]);

  g_bloblist = new Blob [NDB_MAX_ATTRIBUTES_IN_TABLE];
  g_blobcount = argc - 1;
  for (int i = 0; i < g_blobcount; i++)
  {
    Blob& b = g_bloblist[i];
    b.colname = newstr(argv[1 + i]);
  }

  if (opt_check_orphans ||
      opt_delete_orphans)
  {
    if (opt_check_orphans &&
        opt_delete_orphans)
    {
      g_err << "Specify only one action (--check-orphans etc)" << endl;
      return 1;
    }
  }
  else
  {
    g_err << "Action (--check-orphans etc) required" << endl;
    return 1;
  }
  return 0;
}

static void
freeall()
{
  delete [] g_tabname;
  delete [] g_pklist;
  delete [] g_bloblist;
  delete [] g_vallist;

  delete g_dump_out;
  if (g_dump_file != 0)
    (void)fclose(g_dump_file);
}

int
main(int argc, char** argv)
{
  NDB_INIT("ndb_blob_tool");
  int ret;
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
  if (ret != 0 || checkopts(argc, argv) != 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  setOutputLevel(opt_verbose ? 2 : 0);

  ret = doall();
  freeall();
  if (ret == -1)
    return NDBT_ProgramExit(NDBT_FAILED);
  return NDBT_ProgramExit(NDBT_OK);
}
