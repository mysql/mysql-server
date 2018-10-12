/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <OutputStream.hpp>
#include <NdbTest.hpp>
#include <NdbTick.h>
#include "m_ctype.h"
#include "my_sys.h"

#include <NdbRestarter.hpp>

#include <ndb_rand.h>
#include <NdbHost.h>

struct Chr {
  NdbDictionary::Column::Type m_type;
  bool m_fixed;
  bool m_binary;
  uint m_len; // native
  uint m_bytelen; // in bytes
  uint m_totlen; // plus length bytes
  const char* m_cs;
  CHARSET_INFO* m_csinfo;
  uint m_mblen;
  bool m_caseins; // for latin letters
  Chr() :
    m_type(NdbDictionary::Column::Varchar),
    m_fixed(false),
    m_binary(false),
    m_len(55),
    m_bytelen(0),
    m_totlen(0),
    m_cs("latin1"),
    m_csinfo(0),
    m_caseins(true)
  {}
};

struct Opt {
  unsigned m_batch;
  bool m_core;
  bool m_dbg;
  const char* m_debug;
  bool m_fac;
  bool m_full;
  unsigned m_loop;
  bool m_min;
  unsigned m_parts;
  unsigned m_rows;
  int m_seed;
  const char* m_skip;
  const char* m_test;
  int m_timeout_retries;
  int m_blob_version;
  // metadata
  const char* m_tname;
  const char* m_x1name;  // hash index
  const char* m_x2name;  // ordered index
  unsigned m_pk1off;
  Chr m_pk2chr;
  bool m_pk2part;
  bool m_oneblob;

  int m_rbatch;
  int m_wbatch;
  // perf
  const char* m_tnameperf;
  unsigned m_rowsperf;
  // bugs
  int m_bug;
  int (*m_bugtest)();
  bool m_nodrop;
  Opt() :
    m_batch(7),
    m_core(false),
    m_dbg(false),
    m_debug(0),
    m_fac(false),
    m_full(false),
    m_loop(1),
    m_min(false),
    m_parts(10),
    m_rows(100),
    m_seed(-1),
    m_skip(0),
    m_test(0),
    m_timeout_retries(10),
    m_blob_version(2),
    // metadata
    m_tname("TB1"),
    m_x1name("TB1X1"),
    m_x2name("TB1X2"),
    m_pk1off(0x12340000),
    m_pk2chr(),
    m_pk2part(false),
    m_oneblob(false),
    m_rbatch(-1),
    m_wbatch(-1),
    // perf
    m_tnameperf("TB2"),
    m_rowsperf(10000),
    // bugs
    m_bug(0),
    m_bugtest(0),
    m_nodrop(false)
  {}
};

static void
printusage()
{
  Opt d;
  ndbout
    << "usage: testBlobs options [default/max]" << endl
    << "  -batch N    number of pk ops in batch [" << d.m_batch << "]" << endl
    << "  -core       dump core on error" << endl
    << "  -dbg        print program debug" << endl
    << "  -debug opt  also ndb api DBUG (if no ':' becomes d:t:F:L:o,opt)" << endl
    << "  -fac        fetch across commit in scan delete" << endl
    << "  -full       read/write only full blob values" << endl
    << "  -loop N     loop N times 0=forever [" << d.m_loop << "]" << endl
    << "  -min        small blob sizes" << endl
    << "  -parts N    max parts in blob value [" << d.m_parts << "]" << endl
    << "  -rows N     number of rows [" << d.m_rows << "]" << endl
    << "  -rowsperf N rows for performace test [" << d.m_rowsperf << "]" << endl
    << "  -seed N     random seed 0=loop number -1=random [" << d.m_seed << "]" << endl
    << "  -skip xxx   skip given tests (see list) [no tests]" << endl
    << "  -test xxx   only given tests (see list) [all tests]" << endl
    << "  -nodrop     don't drop tables at end of test" << endl
    << "  -timeoutretries N Number of times to retry in deadlock situations [" 
    << d.m_timeout_retries << "]" << endl
    << "  -version N  blob version 1 or 2 [" << d.m_blob_version << "]" << endl
    << "metadata" << endl
    << "  -pk2len N   native length of PK2, zero omits PK2,PK3 [" << d.m_pk2chr.m_len << "]" << endl
    << "  -pk2fixed   PK2 is Char [default Varchar]" << endl
    << "  -pk2binary  PK2 is Binary or Varbinary" << endl
    << "  -pk2cs      PK2 charset or collation [" << d.m_pk2chr.m_cs << "]" << endl
    << "  -pk2part    partition primary table by PK2" << endl
    << "  -oneblob    only 1 blob attribute [default 2]" << endl
    << "  -rbatch     N Read parts batchsize (bytes) [default -1] -1=random" << endl
    << "  -wbatch     N Write parts batchsize (bytes) [default -1] -1=random" << endl
    << "disk or memory storage for blobs.  Don't apply to performance test" << endl
    << "  m           Blob columns stored in memory" << endl
    << "  h           Blob columns stored on disk" << endl
    << "api styles for test/skip.  Don't apply to performance test" << endl
    << "  a           NdbRecAttr(old) interface" << endl
    << "  b           NdbRecord interface" << endl
    << "test cases for test/skip" << endl
    << "  k           primary key ops" << endl
    << "  i           hash index ops" << endl
    << "  s           table scans" << endl
    << "  r           ordered index scans" << endl
    << "  p           performance test" << endl
    << "operations for test/skip" << endl
    << "  u           update existing blob value" << endl
    << "  n           normal insert and update" << endl
    << "  w           insert and update using writeTuple" << endl
    << "  d           delete, can skip only for one subtest" << endl
    << "  l           read with lock and unlock" << endl
    << "blob operation styles for test/skip" << endl
    << "  0           getValue / setValue" << endl
    << "  1           setActiveHook" << endl
    << "  2           readData / writeData" << endl
    << "example: -test makn0 (need all 4 parts)" << endl
    << "example: -test mhabkisrunwd012 (Everything except performance tests" << endl
    << "bug tests" << endl
    << "  -bug 4088   ndb api hang with mixed ops on index table" << endl
    << "  -bug 27018  middle partial part write clobbers rest of part" << endl
    << "  -bug 27370  Potential inconsistent blob reads for ReadCommitted reads" << endl
    << "  -bug 36756  Handling execute(.., abortOption) and Blobs " << endl
    << "  -bug 45768  execute(Commit) after failing blob batch " << endl
    << "  -bug 62321  Blob obscures ignored error codes in batch" << endl
    ;
}

static Opt g_opt;

static bool
testcase(char x)
{
  if (x < 10)
    x += '0';

  return
    (g_opt.m_test == 0 || strchr(g_opt.m_test, x) != 0) &&
    (g_opt.m_skip == 0 || strchr(g_opt.m_skip, x) == 0);
}

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static NdbConnection* g_con = 0;
static NdbOperation* g_opr = 0;
static const NdbOperation* g_const_opr = 0;
static NdbIndexOperation* g_opx = 0;
static NdbScanOperation* g_ops = 0;
static NdbBlob* g_bh1 = 0;
static NdbBlob* g_bh2 = 0;
static bool g_printerror = true;
static unsigned g_loop = 0;
static NdbRecord *g_key_record= 0;
static NdbRecord *g_blob_record= 0;
static NdbRecord *g_full_record= 0;
static NdbRecord *g_idx_record= 0;
static NdbRecord *g_ord_record= 0;
static unsigned g_pk1_offset= 0;
static unsigned g_pk2_offset= 0;
static unsigned g_pk3_offset= 0;
static unsigned g_blob1_offset= 0;
static unsigned g_blob1_null_offset= 0;
static unsigned g_blob2_offset= 0;
static unsigned g_blob2_null_offset= 0;
static unsigned g_rowsize= 0;
static const char* g_tsName= "DEFAULT-TS";
static Uint32 g_batchSize= 0;
static Uint32 g_scanFlags= 0;
static Uint32 g_parallel= 0;
static Uint32 g_usingDisk= false;
static const Uint32 MAX_FRAGS=48 * 8 * 4; // e.g. 48 nodes, 8 frags/node, 4 replicas
static Uint32 frag_ng_mappings[MAX_FRAGS];


static const char* stylename[3] = {
  "style=getValue/setValue",
  "style=setActiveHook",
  "style=readData/writeData"
};

// Blob API variants
static const char* apiName[2] = {
  "api=NdbRecAttr",
  "api=NdbRecord"
};

static const char apiSymbol[2] = {
  'a',  // RecAttr
  'b'   // NdbRecord
};

static const int API_RECATTR=0;
static const int API_NDBRECORD=1;

static const char* storageName[2] = {
  "storage=memory",
  "storage=disk"
};

static const char storageSymbol[2] = {
  'm',  // Memory storage
  'h'   // Disk storage
};

static const int STORAGE_MEM=0;
static const int STORAGE_DISK=1;

static void
printerror(int line, const char* msg)
{
  ndbout << "line " << line << " FAIL " << msg << endl;
  if (! g_printerror) {
    return;
  }
  if (g_ndb != 0 && g_ndb->getNdbError().code != 0) {
    ndbout << "ndb: " << g_ndb->getNdbError() << endl;
  }
  if (g_dic != 0 && g_dic->getNdbError().code != 0) {
    ndbout << "dic: " << g_dic->getNdbError() << endl;
  }
  if (g_con != 0 && g_con->getNdbError().code != 0) {
    ndbout << "con: " << g_con->getNdbError() << endl;
    if (g_opr != 0 && g_opr->getNdbError().code != 0) {
      ndbout << "opr: table=" << g_opr->getTableName() << " " << g_opr->getNdbError() << endl;
    }
    if (g_const_opr != 0 && g_const_opr->getNdbError().code !=0) {
      ndbout << "const_opr: table=" << g_const_opr->getTableName() << " " << g_const_opr->getNdbError() << endl;
    }
    if (g_opx != 0 && g_opx->getNdbError().code != 0) {
      ndbout << "opx: table=" << g_opx->getTableName() << " " << g_opx->getNdbError() << endl;
    }
    if (g_ops != 0 && g_ops->getNdbError().code != 0) {
      ndbout << "ops: table=" << g_ops->getTableName() << " " << g_ops->getNdbError() << endl;
    }
    NdbOperation* ope = g_con->getNdbErrorOperation();
    if (ope != 0 && ope->getNdbError().code != 0) {
      if (ope != g_opr && ope != g_const_opr && ope != g_opx && ope != g_ops)
        ndbout << "ope: ptr=" << ope << " table=" << ope->getTableName() << " type= "<< ope->getType() << " " << ope->getNdbError() << endl;
    }
  }
  if (g_bh1 != 0 && g_bh1->getNdbError().code != 0) {
    ndbout << "bh1: " << g_bh1->getNdbError() << endl;
  }
  if (g_bh2 != 0 && g_bh2->getNdbError().code != 0) {
    ndbout << "bh2: " << g_bh2->getNdbError() << endl;
  }
  if (g_opt.m_core) {
    abort();
  }
  g_printerror = false;
}

#define CHK(x) \
  do { \
    if (x) break; \
    printerror(__LINE__, #x); return -1; \
  } while (0)
#define DBG(x) \
  do { \
    if (! g_opt.m_dbg) break; \
    ndbout << "line " << __LINE__ << " " << x << endl; \
  } while (0)
#define DISP(x) \
  do { \
    ndbout << "line " << __LINE__ << " " << x << endl; \
  } while (0)

struct Bcol {
  int m_type;
  int m_version;
  bool m_nullable;
  uint m_inline;
  uint m_partsize;
  uint m_stripe;
  char m_btname[200];
  Bcol() { memset(this, 0, sizeof(*this)); }
};

static Bcol g_blob1;
static Bcol g_blob2;

enum OpState {Normal, Retrying};

static void
initblobs()
{
  {
    Bcol& b = g_blob1;
    b.m_type = NdbDictionary::Column::Text;
    b.m_version = g_opt.m_blob_version;
    b.m_nullable = false;
    b.m_inline = g_opt.m_min ? 8 : 240;
    b.m_partsize = g_opt.m_min ? 8 : 2000;
    b.m_stripe = b.m_version == 1 ? 4 : 0;
  }
  {
    Bcol& b = g_blob2;
    b.m_type = NdbDictionary::Column::Blob;
    b.m_version = g_opt.m_blob_version;
    b.m_nullable = true;
    b.m_inline = g_opt.m_min ? 9 : 99;
    b.m_partsize = g_opt.m_min ? 5 : 55;
    b.m_stripe = 3;
  }
}

static void
initConstants()
{
  g_pk1_offset= 0;
  g_pk2_offset= g_pk1_offset + 4;
  g_pk3_offset= g_pk2_offset + g_opt.m_pk2chr.m_totlen;
  g_blob1_offset= g_pk3_offset + 2;
  g_blob2_offset= g_blob1_offset + sizeof(NdbBlob *);
  g_blob1_null_offset= g_blob2_offset + sizeof(NdbBlob *);
  g_blob2_null_offset= g_blob1_null_offset + 1;
  g_rowsize= g_blob2_null_offset + 1;
}

static int
createDefaultTableSpace()
{
  /* 'Inspired' by NDBT_Tables::create_default_tablespace */
  int res;
  NdbDictionary::LogfileGroup lg = g_dic->getLogfileGroup("DEFAULT-LG");
  if (strcmp(lg.getName(), "DEFAULT-LG") != 0)
  {
    lg.setName("DEFAULT-LG");
    lg.setUndoBufferSize(8*1024*1024);
    res = g_dic->createLogfileGroup(lg);
    if(res != 0){
      DBG("Failed to create logfilegroup:"
          << endl << g_dic->getNdbError() << endl);
      return -1;
    }
  }
  {
    NdbDictionary::Undofile uf = g_dic->getUndofile(0, "undofile01.dat");
    if (strcmp(uf.getPath(), "undofile01.dat") != 0)
    {
      uf.setPath("undofile01.dat");
      uf.setSize(32*1024*1024);
      uf.setLogfileGroup("DEFAULT-LG");
      
      res = g_dic->createUndofile(uf, true);
      if(res != 0){
	DBG("Failed to create undofile:"
            << endl << g_dic->getNdbError() << endl);
	return -1;
      }
    }
  }
  {
    NdbDictionary::Undofile uf = g_dic->getUndofile(0, "undofile02.dat");
    if (strcmp(uf.getPath(), "undofile02.dat") != 0)
    {
      uf.setPath("undofile02.dat");
      uf.setSize(32*1024*1024);
      uf.setLogfileGroup("DEFAULT-LG");
      
      res = g_dic->createUndofile(uf, true);
      if(res != 0){
	DBG("Failed to create undofile:"
            << endl << g_dic->getNdbError() << endl);
	return -1;
      }
    }
  }
  NdbDictionary::Tablespace ts = g_dic->getTablespace(g_tsName);
  if (strcmp(ts.getName(), g_tsName) != 0)
  {
    ts.setName(g_tsName);
    ts.setExtentSize(1024*1024);
    ts.setDefaultLogfileGroup("DEFAULT-LG");
    
    res = g_dic->createTablespace(ts);
    if(res != 0){
      DBG("Failed to create tablespace:"
          << endl << g_dic->getNdbError() << endl);
      return -1;
    }
  }
  
  {
    NdbDictionary::Datafile df = g_dic->getDatafile(0, "datafile01.dat");
    if (strcmp(df.getPath(), "datafile01.dat") != 0)
    {
      df.setPath("datafile01.dat");
      df.setSize(64*1024*1024);
      df.setTablespace(g_tsName);
      
      res = g_dic->createDatafile(df, true);
      if(res != 0){
	DBG("Failed to create datafile:"
            << endl << g_dic->getNdbError() << endl);
	return -1;
      }
    }
  }

  {
    NdbDictionary::Datafile df = g_dic->getDatafile(0, "datafile02.dat");
    if (strcmp(df.getPath(), "datafile02.dat") != 0)
    {
      df.setPath("datafile02.dat");
      df.setSize(64*1024*1024);
      df.setTablespace(g_tsName);
      
      res = g_dic->createDatafile(df, true);
      if(res != 0){
	DBG("Failed to create datafile:"
            << endl << g_dic->getNdbError() << endl);
	return -1;
      }
    }
  }
  
  return 0;
}

static int
dropTable()
{
  NdbDictionary::Table tab(g_opt.m_tname);
  if (g_dic->getTable(g_opt.m_tname) != 0)
    CHK(g_dic->dropTable(g_opt.m_tname) == 0);

  if (g_key_record != NULL)
    g_dic->releaseRecord(g_key_record);
  if (g_blob_record != NULL)
    g_dic->releaseRecord(g_blob_record);
  if (g_full_record != NULL)
    g_dic->releaseRecord(g_full_record);

  if (g_opt.m_pk2chr.m_len != 0)
  {
    if (g_idx_record != NULL)
      g_dic->releaseRecord(g_idx_record);
    if (g_ord_record != NULL)
      g_dic->releaseRecord(g_ord_record);
  }

  g_key_record= NULL;
  g_blob_record= NULL;
  g_full_record= NULL;
  g_idx_record= NULL;
  g_ord_record= NULL;

  return 0;
}

static unsigned
urandom(unsigned n)
{
  return n == 0 ? 0 : ndb_rand() % n;
}

static int
createTable(int storageType)
{
  /* No logging for memory tables */
  bool loggingRequired=(storageType == STORAGE_DISK);
  NdbDictionary::Column::StorageType blobStorageType= 
    (storageType == STORAGE_MEM)?
    NdbDictionary::Column::StorageTypeMemory : 
    NdbDictionary::Column::StorageTypeDisk;

  NdbDictionary::Table tab(g_opt.m_tname);
  if (storageType == STORAGE_DISK)
    tab.setTablespaceName(g_tsName);
  tab.setLogging(loggingRequired);
  
  /* Choose from the interesting fragmentation types :
   * DistrKeyHash, DistrKeyLin, UserDefined, HashMapPartitioned
   * Others are obsolete fragment-count setting variants 
   * of DistrKeyLin
   * For UserDefined partitioning, we need to set the partition
   * id for all PK operations.
   */
  Uint32 fragTypeRange= 1 + (NdbDictionary::Object::HashMapPartition - 
                             NdbDictionary::Object::DistrKeyHash);
  Uint32 fragType= NdbDictionary::Object::DistrKeyHash + urandom(fragTypeRange);

  /* Value 8 is unused currently, map it to something else */
  if (fragType == 8)
    fragType= NdbDictionary::Object::UserDefined;

  tab.setFragmentType((NdbDictionary::Object::FragmentType)fragType);

  if (fragType == NdbDictionary::Object::UserDefined)
  {
    /* Need to set the FragmentCount and fragment to NG mapping
     * for this partitioning type 
     */
    const Uint32 numNodes= g_ncc->no_db_nodes();
    const Uint32 numReplicas= 2; // Assumption
    const Uint32 guessNumNgs= numNodes/2;
    const Uint32 numNgs= guessNumNgs?guessNumNgs : 1;
    const Uint32 numFragsPerNode= 2 + (rand() % 3);
    const Uint32 numPartitions= numReplicas * numNgs * numFragsPerNode;
    
    tab.setFragmentCount(numPartitions);
    tab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
    for (Uint32 i=0; i<numPartitions; i++)
    {
      frag_ng_mappings[i]= i % numNgs;
    }
    tab.setFragmentData(frag_ng_mappings, numPartitions);
  }
  const Chr& pk2chr = g_opt.m_pk2chr;
  // col PK1 - Uint32
  { NdbDictionary::Column col("PK1");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // col BL1 - Text not-nullable
  { NdbDictionary::Column col("BL1");
    const Bcol& b = g_blob1;
    col.setType((NdbDictionary::Column::Type)b.m_type);
    col.setBlobVersion(b.m_version);
    col.setNullable(b.m_nullable);
    col.setInlineSize(b.m_inline);
    col.setPartSize(b.m_partsize);
    col.setStripeSize(b.m_stripe);
    col.setStorageType(blobStorageType);
    tab.addColumn(col);
  }
  // col PK2 - Char or Varchar
  if (pk2chr.m_len != 0)
  { NdbDictionary::Column col("PK2");
    col.setType(pk2chr.m_type);
    col.setPrimaryKey(true);
    col.setLength(pk2chr.m_bytelen);
    if (pk2chr.m_csinfo != 0)
      col.setCharset(pk2chr.m_csinfo);
    if (g_opt.m_pk2part)
      col.setPartitionKey(true);
    tab.addColumn(col);
  }
  // col BL2 - Blob nullable
  if (! g_opt.m_oneblob)
  { NdbDictionary::Column col("BL2");
    const Bcol& b = g_blob2;
    col.setType((NdbDictionary::Column::Type)b.m_type);
    col.setBlobVersion(b.m_version);
    col.setNullable(b.m_nullable);
    col.setInlineSize(b.m_inline);
    col.setPartSize(b.m_partsize);
    col.setStripeSize(b.m_stripe);
    col.setStorageType(blobStorageType);
    tab.addColumn(col);
  }
  // col PK3 - puts the Var* key PK2 between PK1 and PK3
  if (pk2chr.m_len != 0)
  { NdbDictionary::Column col("PK3");
    col.setType(NdbDictionary::Column::Smallunsigned);
    col.setPrimaryKey(true);
    
    tab.addColumn(col);
  }
  // create table
  CHK(g_dic->createTable(tab) == 0);
  // unique hash index on PK2,PK3
  if (g_opt.m_pk2chr.m_len != 0)
  { NdbDictionary::Index idx(g_opt.m_x1name);
    idx.setType(NdbDictionary::Index::UniqueHashIndex);
    idx.setLogging(loggingRequired);
    idx.setTable(g_opt.m_tname);
    idx.addColumnName("PK2");
    idx.addColumnName("PK3");
    CHK(g_dic->createIndex(idx) == 0);
  }
  // ordered index on PK2
  if (g_opt.m_pk2chr.m_len != 0)
  { NdbDictionary::Index idx(g_opt.m_x2name);
    idx.setType(NdbDictionary::Index::OrderedIndex);
    idx.setLogging(false);
    idx.setTable(g_opt.m_tname);
    idx.addColumnName("PK2");
    CHK(g_dic->createIndex(idx) == 0);
  }

  NdbDictionary::RecordSpecification spec[5];
  unsigned numpks= g_opt.m_pk2chr.m_len == 0 ? 1 : 3;
  unsigned numblobs= g_opt.m_oneblob ? 1 : 2;

  const NdbDictionary::Table *dict_table;
  CHK((dict_table= g_dic->getTable(g_opt.m_tname)) != 0);
  memset(spec, 0, sizeof(spec));
  spec[0].column= dict_table->getColumn("PK1");
  spec[0].offset= g_pk1_offset;
  spec[numpks].column= dict_table->getColumn("BL1");
  spec[numpks].offset= g_blob1_offset;
  spec[numpks].nullbit_byte_offset= g_blob1_null_offset;
  spec[numpks].nullbit_bit_in_byte= 0;
  if (g_opt.m_pk2chr.m_len != 0)
  {
    spec[1].column= dict_table->getColumn("PK2");
    spec[1].offset= g_pk2_offset;
    spec[2].column= dict_table->getColumn("PK3");
    spec[2].offset= g_pk3_offset;
  }
  if (! g_opt.m_oneblob)
  {
    spec[numpks+1].column= dict_table->getColumn("BL2");
    spec[numpks+1].offset= g_blob2_offset;
    spec[numpks+1].nullbit_byte_offset= g_blob2_null_offset;
    spec[numpks+1].nullbit_bit_in_byte= 0;
  }
  CHK((g_key_record= g_dic->createRecord(dict_table, &spec[0], numpks,
                                         sizeof(spec[0]))) != 0);
  CHK((g_blob_record= g_dic->createRecord(dict_table, &spec[numpks], numblobs,
                                         sizeof(spec[0]))) != 0);
  CHK((g_full_record= g_dic->createRecord(dict_table, &spec[0], numpks+numblobs,
                                         sizeof(spec[0]))) != 0);

  if (g_opt.m_pk2chr.m_len != 0)
  {
    const NdbDictionary::Index *dict_index;
    CHK((dict_index= g_dic->getIndex(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK((g_idx_record= g_dic->createRecord(dict_index, &spec[1], 2,
                                           sizeof(spec[0]))) != 0);
    CHK((dict_index= g_dic->getIndex(g_opt.m_x2name, g_opt.m_tname)) != 0);
    CHK((g_ord_record= g_dic->createRecord(dict_index, &spec[1], 1,
                                           sizeof(spec[0]))) != 0);
  }

  return 0;
}

// tuples

struct Bval {
  const Bcol& m_bcol;
  char* m_val;
  unsigned m_len;
  char* m_buf; // read/write buffer
  unsigned m_buflen;
  int m_error_code; // for testing expected error code
  Bval(const Bcol& bcol) :
    m_bcol(bcol),
    m_val(0),
    m_len(0),
    m_buf(0),
    m_buflen(0),
    m_error_code(0)
    {}
  ~Bval() { delete [] m_val; delete [] m_buf; }
  void alloc() {
    alloc(m_bcol.m_inline + m_bcol.m_partsize * g_opt.m_parts);
  }
  void alloc(unsigned buflen) {
    m_buflen = buflen;
    delete [] m_buf;
    m_buf = new char [m_buflen];
    trash();
  }
  void copyfrom(const Bval& v) {
    m_len = v.m_len;
    delete [] m_val;
    if (v.m_val == 0)
      m_val = 0;
    else
      m_val = (char*)memcpy(new char [m_len], v.m_val, m_len);
  }
  void trash() const {
    require(m_buf != 0);
    memset(m_buf, 'x', m_buflen);
  }
private:
  Bval(const Bval&);
  Bval& operator=(const Bval&);
};

NdbOut&
operator<<(NdbOut& out, const Bval& v)
{
  if (g_opt.m_min && v.m_val != 0) {
    out << "[" << v.m_len << "]";
    for (uint i = 0; i < v.m_len; i++) {
      const Bcol& b = v.m_bcol;
      if (i == b.m_inline ||
          (i > b.m_inline && (i - b.m_inline) % b.m_partsize == 0))
        out.print("|");
      out.print("%c", v.m_val[i]);
    }
  }
  return out;
}

struct Tup {
  bool m_exists;        // exists in table
  Uint32 m_pk1;         // in V1 primary keys concatenated like keyinfo
  char* m_pk2;
  char* m_pk2eq;        // equivalent (if case independent)
  Uint16 m_pk3;
  Bval m_bval1;
  Bval m_bval2;
  char *m_key_row;
  char *m_row;
  Uint32 m_frag;
  Tup() :
    m_exists(false),
    m_pk2(new char [g_opt.m_pk2chr.m_totlen + 1]), // nullterm for convenience
    m_pk2eq(new char [g_opt.m_pk2chr.m_totlen + 1]),
    m_bval1(g_blob1),
    m_bval2(g_blob2),
    m_key_row(new char[g_rowsize]),
    m_row(new char[g_rowsize]),
    m_frag(~(Uint32)0)
    {}
  ~Tup() {
    delete [] m_pk2;
    m_pk2 = 0;
    delete [] m_pk2eq;
    m_pk2eq = 0;
    delete [] m_key_row;
    m_key_row= 0;
    delete [] m_row;
    m_row= 0;
  }
  // alloc buffers of max size
  void alloc() {
    m_bval1.alloc();
    m_bval2.alloc();
  }
  void copyAllfrom(const Tup& tup)
  {
    m_exists = tup.m_exists;
    m_pk1 = tup.m_pk1;
    memcpy(m_pk2, tup.m_pk2, g_opt.m_pk2chr.m_totlen + 1);
    memcpy(m_pk2eq, tup.m_pk2eq, g_opt.m_pk2chr.m_totlen + 1);
    m_pk3 = tup.m_pk3;
    memcpy(m_key_row, tup.m_key_row, g_rowsize);
    memcpy(m_row, tup.m_row, g_rowsize);
    m_frag = tup.m_frag;
    copyfrom(tup);
  }
  void copyfrom(const Tup& tup) {
    require(m_pk1 == tup.m_pk1);
    m_bval1.copyfrom(tup.m_bval1);
    m_bval2.copyfrom(tup.m_bval2);
  }
  /*
   * in V2 return pk2 or pk2eq at random
   * in V1 mixed cases do not work in general due to key packing
   * luckily they do work via mysql
   */
  char* pk2() {
    if (g_opt.m_blob_version == 1)
      return m_pk2;
    return urandom(2) == 0 ? m_pk2 : m_pk2eq;
  }
  Uint32 getPartitionId(Uint32 numParts) const {
    /* Only for UserDefined tables really */
    return m_pk1 % numParts; // MySQLD hash(PK1) style partitioning
  }

private:
  Tup(const Tup&);
  Tup& operator=(const Tup&);
};

static Tup* g_tups = 0;

static void
setUDpartId(const Tup& tup, NdbOperation* op)
{
  const NdbDictionary::Table* tab= op->getTable();
  if (tab->getFragmentType() == NdbDictionary::Object::UserDefined)
  {
    Uint32 partId= tup.getPartitionId(tab->getFragmentCount());
    DBG("Setting partition id to " << partId << " out of " << 
        tab->getFragmentCount());
    op->setPartitionId(partId);
  }
}

static void
setUDpartIdNdbRecord(const Tup& tup, 
                     const NdbDictionary::Table* tab, 
                     NdbOperation::OperationOptions& opts)
{
  opts.optionsPresent= 0;
  if (tab->getFragmentType() == NdbDictionary::Object::UserDefined)
  {
    opts.optionsPresent= NdbOperation::OperationOptions::OO_PARTITION_ID;
    opts.partitionId= tup.getPartitionId(tab->getFragmentCount());
  } 
}

static void
calcBval(const Bcol& b, Bval& v, bool keepsize)
{
  if (b.m_nullable && urandom(10) == 0) {
    v.m_len = 0;
    delete [] v.m_val;
    v.m_val = 0;
    delete [] v.m_buf;
    v.m_buf = new char [1];
  } else {
    if (keepsize && v.m_val != 0)
      ;
    else if (urandom(10) == 0)
      v.m_len = urandom(b.m_inline);
    else
      v.m_len = urandom(b.m_inline + g_opt.m_parts * b.m_partsize + 1);
    delete [] v.m_val;
    v.m_val = new char [v.m_len + 1];
    for (unsigned i = 0; i < v.m_len; i++)
      v.m_val[i] = 'a' + urandom(26);
    v.m_val[v.m_len] = 0;
    delete [] v.m_buf;
    v.m_buf = new char [v.m_len];
  }
  v.m_buflen = v.m_len;
  v.trash();
}

static bool
conHasTimeoutError()
{
  Uint32 code= g_con->getNdbError().code;
  /* Indicate timeout for cases where LQH too slow responding
   * (As can happen for disk based tuples with batching or
   *  lots of parts)
   */
  // 296 == Application timeout waiting for SCAN_NEXTREQ from API
  // 297 == Error code in response to SCAN_NEXTREQ for timed-out scan
  bool isTimeout= ((code == 274) || // General TC connection timeout 
                   (code == 266));  // TC Scan frag timeout
  if (!isTimeout)
    ndbout << "Connection error is not timeout, but is "
           << code << endl;
  
  return isTimeout;
}

static
Uint32 conError()
{
  return g_con->getNdbError().code;
}

static void
calcBval(Tup& tup, bool keepsize)
{
  calcBval(g_blob1, tup.m_bval1, keepsize);
  if (! g_opt.m_oneblob)
    calcBval(g_blob2, tup.m_bval2, keepsize);
}

// dont remember what the keepsize was for..
static void
calcTups(bool keys, bool keepsize = false)
{
  for (uint k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    if (keys) {
      tup.m_pk1 = g_opt.m_pk1off + k;
      {
        const Chr& c = g_opt.m_pk2chr;
        char* const p = tup.m_pk2;
        char* const q = tup.m_pk2eq;
        uint len = urandom(c.m_len + 1);
        uint i = 0;
        if (! c.m_fixed) {
          *(uchar*)&p[0] = *(uchar*)&q[0] = len;
          i++;
        }
        uint j = 0;
        while (j < len) {
          // mixed case for distribution check
          if (urandom(3) == 0) {
            uint u = urandom(26);
            p[i] = 'A' + u;
            q[i] = c.m_caseins ? 'a' + u : 'A' + u;
          } else {
            uint u = urandom(26);
            p[i] = 'a' + u;
            q[i] = c.m_caseins ? 'A' + u : 'a' + u;
          }
          i++;
          j++;
        }
        while (j < c.m_bytelen) {
          if (c.m_fixed)
            p[i] = q[i] = 0x20;
          else
            p[i] = q[i] = '#'; // garbage
          i++;
          j++;
        }
        require(i == c.m_totlen);
        p[i] = q[i] = 0; // convenience
      }
      tup.m_pk3 = (Uint16)k;
    }
    calcBval(tup, keepsize);
  }
}

static void setBatchSizes()
{
  if (g_opt.m_rbatch != 0)
  {
    Uint32 byteSize = (g_opt.m_rbatch == -1) ? 
      urandom(~Uint32(0)) :
      g_opt.m_rbatch;
    
    DBG("Setting read batch size to " << byteSize 
        << " bytes.");
    g_con->setMaxPendingBlobReadBytes(byteSize);
  }
  
  if (g_opt.m_wbatch != 0)
  {
    Uint32 byteSize = (g_opt.m_wbatch == -1) ? 
      urandom(~Uint32(0)) :
      g_opt.m_wbatch;
    
    DBG("Setting write batch size to " << byteSize 
        << " bytes.");
    g_con->setMaxPendingBlobWriteBytes(byteSize);
  }
}
    

// blob handle ops
// const version for NdbRecord defined operations
static int
getBlobHandles(const NdbOperation* opr)
{
  CHK((g_bh1 = opr->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = opr->getBlobHandle("BL2")) != 0);

  setBatchSizes();
  return 0;
}

// non-const version for NdbRecAttr defined operations
// and scans
static int
getBlobHandles(NdbOperation* opr)
{
  CHK((g_bh1 = opr->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = opr->getBlobHandle("BL2")) != 0);
  setBatchSizes();
  return 0;
}


static int
getBlobHandles(NdbScanOperation* ops)
{
  CHK((g_bh1 = ops->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = ops->getBlobHandle("BL2")) != 0);
  setBatchSizes();
  return 0;
}

static int
getBlobLength(NdbBlob* h, unsigned& len)
{
  Uint64 len2 = (unsigned)-1;
  CHK(h->getLength(len2) == 0);
  len = (unsigned)len2;
  require(len == len2);
  bool isNull;
  CHK(h->getNull(isNull) == 0);
  DBG("getBlobLength " << h->getColumn()->getName() << " len=" << len << " null=" << isNull);
  return 0;
}

// setValue / getValue

static int
setBlobValue(NdbBlob* h, const Bval& v, int error_code = 0)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("setValue " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null << " " << v);
  if (null) {
    CHK(h->setNull() == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    CHK(h->setValue(v.m_val, v.m_len) == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
  }
  return 0;
}

static int
setBlobValue(const Tup& tup, int error_code = 0)
{
  CHK(setBlobValue(g_bh1, tup.m_bval1, error_code) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobValue(g_bh2, tup.m_bval2, error_code) == 0);
  return 0;
}

static int
getBlobValue(NdbBlob* h, const Bval& v)
{
  DBG("getValue " <<  h->getColumn()->getName() << " buflen=" << v.m_buflen);
  CHK(h->getValue(v.m_buf, v.m_buflen) == 0);
  return 0;
}

static int
getBlobValue(const Tup& tup)
{
  CHK(getBlobValue(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(getBlobValue(g_bh2, tup.m_bval2) == 0);
  return 0;
}

/* 
 * presetBH1
 * This method controls how BL1 is pre-set (using setValue()) for 
 * inserts and writes that later use writeData to set the correct 
 * value.
 * Sometimes it is set to length zero, other times to the value
 * for some other row in the dataset.  This tests that the writeData()
 * functionality correctly overwrites values written in the 
 * prepare phase.
 */
static int presetBH1(int rowNumber)
{
  unsigned int variant = urandom(2);
  DBG("presetBH1 - Variant=" << variant);
  if (variant==0) 
    CHK(g_bh1->setValue("", 0) == 0);
  else
  {
    CHK(setBlobValue(g_tups[(rowNumber+1) % g_opt.m_rows]) == 0); // Pre-set to something else
  };
  return 0;
}

static int
verifyBlobValue(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  if (null) {
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    isNull = true;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
    for (unsigned i = 0; i < v.m_len; i++)
      CHK(v.m_val[i] == v.m_buf[i]);
  }
  return 0;
}

static int
verifyBlobValue(const Tup& tup)
{
  CHK(verifyBlobValue(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(verifyBlobValue(g_bh2, tup.m_bval2) == 0);
  return 0;
}

// readData / writeData

static int
writeBlobData(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("write " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null << " " << v);
  int error_code = v.m_error_code;
  if (null) {
    CHK(h->setNull() == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    CHK(h->truncate(v.m_len) == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    CHK(h->setPos(0) == 0); // Reset write pointer in case there was a previous write.
    unsigned n = 0;
    do {
      unsigned m = g_opt.m_full ? v.m_len : urandom(v.m_len + 1);
      if (m > v.m_len - n)
        m = v.m_len - n;
      DBG("write pos=" << n << " cnt=" << m);
      CHK(h->writeData(v.m_val + n, m) == 0);
      n += m;
    } while (n < v.m_len);
    require(n == v.m_len);
    isNull = true;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
  }
  return 0;
}

static int
writeBlobData(Tup& tup, int error_code = 0)
{
  tup.m_bval1.m_error_code = error_code;
  CHK(writeBlobData(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob) {
    tup.m_bval2.m_error_code = error_code;
    CHK(writeBlobData(g_bh2, tup.m_bval2) == 0);
  }
  return 0;
}

static int
readBlobData(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("read " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
  if (null) {
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    isNull = true;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
    v.trash();
    unsigned n = 0;
    while (n < v.m_len) {
      unsigned m = g_opt.m_full ? v.m_len : urandom(v.m_len + 1);
      if (m > v.m_len - n)
        m = v.m_len - n;
      DBG("read pos=" << n << " cnt=" << m);
      const unsigned m2 = m;
      CHK(h->readData(v.m_buf + n, m) == 0);
      CHK(m2 == m);
      n += m;
    }
    require(n == v.m_len);
    // need to execute to see the data
    CHK(g_con->execute(NoCommit) == 0);
    for (unsigned i = 0; i < v.m_len; i++)
      CHK(v.m_val[i] == v.m_buf[i]);
  }
  return 0;
}

static int
readBlobData(const Tup& tup)
{
  CHK(readBlobData(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(readBlobData(g_bh2, tup.m_bval2) == 0);
  return 0;
}

// hooks

static NdbBlob::ActiveHook blobWriteHook;

static int
blobWriteHook(NdbBlob* h, void* arg)
{
  DBG("blobWriteHook");
  Bval& v = *(Bval*)arg;
  CHK(writeBlobData(h, v) == 0);
  return 0;
}


static int
setBlobWriteHook(NdbBlob* h, Bval& v, int error_code = 0)
{
  DBG("setBlobWriteHook");
  v.m_error_code = error_code;
  CHK(h->setActiveHook(blobWriteHook, &v) == 0);
  return 0;
}

static int
setBlobWriteHook(Tup& tup, int error_code = 0)
{
  CHK(setBlobWriteHook(g_bh1, tup.m_bval1, error_code) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobWriteHook(g_bh2, tup.m_bval2, error_code) == 0);
  return 0;
}

static NdbBlob::ActiveHook blobReadHook;

// no PK yet to identify tuple so just read the value
static int
blobReadHook(NdbBlob* h, void* arg)
{
  DBG("blobReadHook");
  Bval& v = *(Bval*)arg;
  unsigned len;
  CHK(getBlobLength(h, len) == 0);
  v.alloc(len);
  Uint32 maxlen = 0xffffffff;
  CHK(h->readData(v.m_buf, maxlen) == 0);
  DBG("read " << maxlen << " bytes");
  CHK(len == maxlen);
  return 0;
}

static int
setBlobReadHook(NdbBlob* h, Bval& v)
{
  DBG("setBlobReadHook");
  CHK(h->setActiveHook(blobReadHook, &v) == 0);
  return 0;
}

static int
setBlobReadHook(Tup& tup)
{
  CHK(setBlobReadHook(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobReadHook(g_bh2, tup.m_bval2) == 0);
  return 0;
}

static int
tryRowLock(Tup& tup, bool exclusive)
{
  NdbTransaction* testTrans;
  NdbOperation* testOp;
  CHK((testTrans = g_ndb->startTransaction()) != NULL);
  CHK((testOp = testTrans->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(testOp->readTuple(exclusive?
                        NdbOperation::LM_Exclusive:
                        NdbOperation::LM_Read) == 0);
  CHK(testOp->equal("PK1", tup.m_pk1) == 0);
  if (g_opt.m_pk2chr.m_len != 0) {
    CHK(testOp->equal("PK2", tup.m_pk2) == 0);
    CHK(testOp->equal("PK3", tup.m_pk3) == 0);
  }
  setUDpartId(tup, testOp);
  
  if (testTrans->execute(Commit, AbortOnError) == 0)
  {
    /* Successfully claimed lock */
    testTrans->close();
    return 0;
  }
  else
  {
    if (testTrans->getNdbError().code == 266)
    {
      /* Error as expected for lock already claimed */
      testTrans->close();
      return -2;
    }
    else
    {
      DBG("Error on tryRowLock, exclusive = " << exclusive
          << endl << testTrans->getNdbError() << endl);
      testTrans->close();
      return -1;
    }
  }
}
  

static int
verifyRowLocked(Tup& tup)
{
  CHK(tryRowLock(tup, true) == -2);
  return 0;
}

static int
verifyRowNotLocked(Tup& tup)
{
  CHK(tryRowLock(tup, true) == 0);
  return 0;
}

// verify blob data

static int
verifyHeadInline(const Bcol& b, const Bval& v, NdbRecAttr* ra)
{
  if (v.m_val == 0) {
    CHK(ra->isNULL() == 1);
  } else {
    CHK(ra->isNULL() == 0);
    NdbBlob::Head head;
    NdbBlob::unpackBlobHead(head, ra->aRef(), b.m_version);
    CHK(head.length == v.m_len);
    const char* data = ra->aRef() + head.headsize;
    for (unsigned i = 0; i < head.length && i < b.m_inline; i++)
      CHK(data[i] == v.m_val[i]);
  }
  return 0;
}

static int
verifyHeadInline(Tup& tup)
{
  DBG("verifyHeadInline pk1=" << hex << tup.m_pk1);
  CHK((g_con = g_ndb->startTransaction()) != 0);
  CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->readTuple() == 0);
  CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
  if (g_opt.m_pk2chr.m_len != 0) {
    CHK(g_opr->equal("PK2", tup.pk2()) == 0);
    CHK(g_opr->equal("PK3", (char*)&tup.m_pk3) == 0);
  }
  setUDpartId(tup, g_opr);
  NdbRecAttr* ra1 = 0;
  NdbRecAttr* ra2 = 0;
  NdbRecAttr* ra_frag;
  CHK((ra1 = g_opr->getValue("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((ra2 = g_opr->getValue("BL2")) != 0);
  CHK((ra_frag = g_opr->getValue(NdbDictionary::Column::FRAGMENT)) != 0);
  if (tup.m_exists) {
    CHK(g_con->execute(Commit, AbortOnError) == 0);
    tup.m_frag = ra_frag->u_32_value();
    DBG("fragment id: " << tup.m_frag);
    DBG("verifyHeadInline BL1");
    CHK(verifyHeadInline(g_blob1, tup.m_bval1, ra1) == 0);
    if (! g_opt.m_oneblob) {
      DBG("verifyHeadInline BL2");
      CHK(verifyHeadInline(g_blob2, tup.m_bval2, ra2) == 0);
    }
  } else {
    CHK(g_con->execute(Commit, AbortOnError) == -1 && 
	g_con->getNdbError().code == 626);
  }
  g_ndb->closeTransaction(g_con);
  g_opr = 0;
  g_con = 0;
  return 0;
}

static unsigned
getvarsize(const char* buf)
{
  const unsigned char* p = (const unsigned char*)buf;
  return p[0] + (p[1] << 8);
}

static int
verifyBlobTable(const Bval& v, Uint32 pk1, Uint32 frag, bool exists)
{
  const Bcol& b = v.m_bcol;
  DBG("verify " << b.m_btname << " pk1=" << hex << pk1);
  NdbRecAttr* ra_pk = 0; // V1
  NdbRecAttr* ra_pk1 = 0; // V2
  NdbRecAttr* ra_pk2 = 0; // V2
  NdbRecAttr* ra_pk3 = 0; // V2
  NdbRecAttr* ra_part = 0;
  NdbRecAttr* ra_data = 0;
  NdbRecAttr* ra_frag = 0;
  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_ops = g_con->getNdbScanOperation(b.m_btname)) != 0);
    CHK(g_ops->readTuples(NdbScanOperation::LM_Read, 
                          g_scanFlags,
                          g_batchSize,
                          g_parallel) == 0);
    if (b.m_version == 1) {
      CHK((ra_pk = g_ops->getValue("PK")) != 0);
      CHK((ra_part = g_ops->getValue("PART")) != 0);
      CHK((ra_data = g_ops->getValue("DATA")) != 0);
    } else {
      CHK((ra_pk1 = g_ops->getValue("PK1")) != 0);
      if (g_opt.m_pk2chr.m_len != 0) {
        CHK((ra_pk2 = g_ops->getValue("PK2")) != 0);
        CHK((ra_pk3 = g_ops->getValue("PK3")) != 0);
      }
      CHK((ra_part = g_ops->getValue("NDB$PART")) != 0);
      CHK((ra_data = g_ops->getValue("NDB$DATA")) != 0);
    }
    
    /* No partition id set on Blob part table scan so that we
     * find any misplaced parts in other partitions
     */

    CHK((ra_frag = g_ops->getValue(NdbDictionary::Column::FRAGMENT)) != 0);
    CHK(g_con->execute(NoCommit) == 0);
    unsigned partcount;
    if (! exists || v.m_len <= b.m_inline)
      partcount = 0;
    else
      partcount = (v.m_len - b.m_inline + b.m_partsize - 1) / b.m_partsize;
    char* seen = new char [partcount];
    memset(seen, 0, partcount);
    while (1) {
      int ret= g_ops->nextResult();
      if (ret == -1)
      {
        /* Timeout? */
        CHK(conHasTimeoutError());
        
        /* Break out and restart scan unless we've
         * run out of attempts
         */
        DISP("Parts table scan failed due to timeout("
             << conError() <<").  Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);
        
        opState= Retrying;
        sleep(1);
        break;
      }
      CHK(opState == Normal);
      CHK((ret == 0) || (ret == 1));
      if (ret == 1)
        break;
      if (b.m_version == 1) {
        if (pk1 != ra_pk->u_32_value())
          continue;
      } else {
        if (pk1 != ra_pk1->u_32_value())
          continue;
      }
      Uint32 part = ra_part->u_32_value();
      Uint32 frag2 = ra_frag->u_32_value();
      DBG("part " << part << " of " << partcount << " from fragment " << frag2);
      CHK(part < partcount && ! seen[part]);
      seen[part] = 1;
      unsigned n = b.m_inline + part * b.m_partsize;
      require(exists && v.m_val != 0 && n < v.m_len);
      unsigned m = v.m_len - n;
      if (m > b.m_partsize)
        m = b.m_partsize;
      const char* data = ra_data->aRef();
      if (b.m_version == 1)
        ;
      else {
        // Blob v2 stored on disk is currently fixed
        // size, so we skip these tests.
        if (!g_usingDisk)
        {
          unsigned sz = getvarsize(data);
          DBG("varsize " << sz);
          DBG("b.m_partsize " << b.m_partsize);
          CHK(sz <= b.m_partsize);
          data += 2;
          if (part + 1 < partcount)
            CHK(sz == b.m_partsize);
          else
            CHK(sz == m);
        }
      }
      CHK(memcmp(data, v.m_val + n, m) == 0);
      if (b.m_version == 1 || 
          g_usingDisk ) { // Blob v2 stored on disk is currently
        // fixed size, so we do these tests.
        char fillchr;
        if (b.m_type == NdbDictionary::Column::Text)
          fillchr = 0x20;
        else
          fillchr = 0x0;
        uint i = m;
        while (i < b.m_partsize) {
          CHK(data[i] == fillchr);
          i++;
        }
      }
      DBG("frags main=" << frag << " blob=" << frag2 << " stripe=" << b.m_stripe);
      if (b.m_stripe == 0)
        CHK(frag == frag2);
    }
    
    if (opState == Normal)
    {
      for (unsigned i = 0; i < partcount; i++)
        CHK(seen[i] == 1);
    }
    delete [] seen;
    g_ops->close();
    g_ndb->closeTransaction(g_con);
  } while (opState == Retrying);
  
  g_ops = 0;
  g_con = 0;
  return 0;
}

static int
verifyBlobTable(const Tup& tup)
{
  CHK(verifyBlobTable(tup.m_bval1, tup.m_pk1, tup.m_frag, tup.m_exists) == 0);
  if (! g_opt.m_oneblob)
    CHK(verifyBlobTable(tup.m_bval2, tup.m_pk1, tup.m_frag, tup.m_exists) == 0);
  return 0;
}

static int
verifyBlob()
{
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("verifyBlob pk1=" << hex << tup.m_pk1);
    CHK(verifyHeadInline(tup) == 0);
    CHK(verifyBlobTable(tup) == 0);
  }
  return 0;
}

static int
rowIsLocked(Tup& tup)
{
  NdbTransaction* testTrans;
  CHK((testTrans = g_ndb->startTransaction()) != 0);
  
  NdbOperation* testOp;
  CHK((testOp = testTrans->getNdbOperation(g_opt.m_tname)) != 0);
  
  CHK(testOp->readTuple(NdbOperation::LM_Exclusive) == 0);
  CHK(testOp->equal("PK1", tup.m_pk1) == 0);
  if (g_opt.m_pk2chr.m_len != 0)
  {
    CHK(testOp->equal("PK2", tup.m_pk2) == 0);
    CHK(testOp->equal("PK3", tup.m_pk3) == 0);
  }
  setUDpartId(tup, testOp);
  CHK(testOp->getValue("PK1") != 0);
  
  CHK(testTrans->execute(Commit) == -1);
  CHK(testTrans->getNdbError().code == 266);
  
  testTrans->close();
  
  return 0;
}

// operations

// pk ops

static int
insertPk(int style, int api)
{
  DBG("--- insertPk " << stylename[style] << " " << apiName[api] << " ---");
  unsigned n = 0;
  unsigned k = 0;
  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (; k < g_opt.m_rows; k++) {
      Tup& tup = g_tups[k];
      DBG("insertPk pk1=" << hex << tup.m_pk1);
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        CHK(g_opr->insertTuple() ==0);
        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
        setUDpartId(tup, g_opr);
        CHK(getBlobHandles(g_opr) == 0);
      }
      else
      {
        memcpy(&tup.m_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0) {
          memcpy(&tup.m_row[g_pk2_offset], tup.m_pk2, g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        }
        NdbOperation::OperationOptions opts;
        setUDpartIdNdbRecord(tup,
                             g_ndb->getDictionary()->getTable(g_opt.m_tname),
                             opts);
        CHK((g_const_opr = g_con->insertTuple(g_full_record, 
                                              tup.m_row,
                                              NULL,
                                              &opts,
                                              sizeof(opts))) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(setBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(presetBH1(k) == 0);
        CHK(setBlobWriteHook(tup) == 0);
      } else {
        CHK(presetBH1(k) == 0);
        CHK(g_con->execute(NoCommit) == 0);
        if (writeBlobData(tup) == -1)
          CHK((timeout= conHasTimeoutError()) == true);
      }

      if (!timeout &&
          (++n == g_opt.m_batch)) {
        if (g_con->execute(Commit) == 0)
        {
          g_ndb->closeTransaction(g_con);
          CHK((g_con = g_ndb->startTransaction()) != 0);
          n = 0;
        }
        else
        {
          CHK((timeout = conHasTimeoutError()) == true);
          n-= 1;
        }
      }

      if (timeout)
      {
        /* Timeout */
        DISP("Insert failed due to timeout("
             << conError() <<")  "
             << " Operations lost : " << n - 1
             << " Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);
        
        k = k - n;
        n = 0;
        opState= Retrying;
        sleep(1);
        break;
      }

      g_const_opr = 0;
      g_opr = 0;
      tup.m_exists = true;
    }
    if (opState == Normal)
    {
      if (n != 0) {
        CHK(g_con->execute(Commit) == 0);
        n = 0;
      }
    }
    g_ndb->closeTransaction(g_con);
  } while (opState == Retrying);
  g_con = 0;
  return 0;
}

static int
readPk(int style, int api)
{
  DBG("--- readPk " << stylename[style] <<" " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    OpState opState;

    do
    {
      opState= Normal;
      DBG("readPk pk1=" << hex << tup.m_pk1);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      NdbOperation::LockMode lm = NdbOperation::LM_CommittedRead;
      switch(urandom(3))
      {
      case 0:
        lm = NdbOperation::LM_Read;
        break;
      case 1:
        lm = NdbOperation::LM_SimpleRead;
        break;
      default:
        break;
      }
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        CHK(g_opr->readTuple(lm) == 0);
        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
        setUDpartId(tup, g_opr);
        CHK(getBlobHandles(g_opr) == 0);
      }
      else
      { // NdbRecord
        memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0) {
          memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        }
        NdbOperation::OperationOptions opts;
        setUDpartIdNdbRecord(tup,
                             g_ndb->getDictionary()->getTable(g_opt.m_tname),
                             opts);
        CHK((g_const_opr = g_con->readTuple(g_key_record, tup.m_key_row,
                                            g_blob_record, tup.m_row,
                                            lm,
                                            NULL,
                                            &opts,
                                            sizeof(opts))) != 0);

        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(getBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(setBlobReadHook(tup) == 0);
      } else {
        CHK(g_con->execute(NoCommit) == 0);
        if (readBlobData(tup) == -1)
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout)
      {
        if (urandom(200) == 0)
        {
          if (g_con->execute(NoCommit) == 0)
          {
            /* Verify row is locked */
            //ndbout << "Checking row is locked for lm "
            //       << lm << endl;
            CHK(rowIsLocked(tup) == 0);
            CHK(g_con->execute(Commit) == 0);
          }
          else
          {
            CHK((timeout= conHasTimeoutError()) == true);
          }
        }
        else
        {
          if (g_con->execute(Commit) != 0)
          {
            CHK((timeout= conHasTimeoutError()) == true);
          }
        }
      }
      if (timeout)
      {
        DISP("ReadPk failed due to timeout("
             << conError() <<")  Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);
        opState= Retrying;
        sleep(1);
      }
      else
      {
        // verify lock mode upgrade
        CHK((g_opr?g_opr:g_const_opr)->getLockMode() == NdbOperation::LM_Read);
            
        if (style == 0 || style == 1) {
          CHK(verifyBlobValue(tup) == 0);
        }
      }
      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);
    g_opr = 0;
    g_const_opr = 0;
    g_con = 0;
  }
  return 0;
}

static int
readLockPk(int style, int api)
{
  DBG("--- readLockPk " << stylename[style] <<" " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    OpState opState;

    do
    {
      opState= Normal;
      DBG("readLockPk pk1=" << hex << tup.m_pk1);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      NdbOperation::LockMode lm = NdbOperation::LM_CommittedRead;
      switch(urandom(4))
      {
      case 0:
        lm = NdbOperation::LM_Exclusive;
        break;
      case 1:
        lm = NdbOperation::LM_Read;
        break;
      case 2:
        lm = NdbOperation::LM_SimpleRead;
      default:
        break;
      }

      bool manualUnlock = ( (lm == NdbOperation::LM_Read) ||
                            (lm == NdbOperation::LM_Exclusive));

      if (api == API_RECATTR)
      {
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        CHK(g_opr->readTuple(lm) == 0);

        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
        setUDpartId(tup, g_opr);
        CHK(getBlobHandles(g_opr) == 0);
        if (manualUnlock)
        {
          CHK(g_opr->getLockHandle() != NULL);
        }
      }
      else
      { // NdbRecord
        memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0) {
          memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        }
        NdbOperation::OperationOptions opts;
        setUDpartIdNdbRecord(tup,
                             g_ndb->getDictionary()->getTable(g_opt.m_tname),
                             opts);
        if (manualUnlock)
        {
          opts.optionsPresent |= NdbOperation::OperationOptions::OO_LOCKHANDLE;
        }
        CHK((g_const_opr = g_con->readTuple(g_key_record, tup.m_key_row,
                                            g_blob_record, tup.m_row,
                                            lm,
                                            NULL,
                                            &opts,
                                            sizeof(opts))) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(getBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(setBlobReadHook(tup) == 0);
      } else {
        CHK(g_con->execute(NoCommit) == 0);
        if (readBlobData(tup) == -1)
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout)
      {
        if (g_con->execute(NoCommit) == 0)
        {
          /* Ok, read executed ok, now 
           * - Verify the Blob data
           * - Verify the row is locked
           * - Close the Blob handles
           * - Attempt to unlock
           */
          NdbOperation::LockMode lmused = (g_opr?g_opr:g_const_opr)->getLockMode();
          CHK((lmused == NdbOperation::LM_Read) ||
              (lmused == NdbOperation::LM_Exclusive));
          
          if (style == 0 || style == 1) {
            CHK(verifyBlobValue(tup) == 0);
          }

          /* Occasionally check that we are locked */
          if (urandom(200) == 0)
            CHK(verifyRowLocked(tup) == 0);
          
          /* Close Blob handles */
          CHK(g_bh1->close() == 0);
          CHK(g_bh1->getState() == NdbBlob::Closed);
          if (! g_opt.m_oneblob)
          {
            CHK(g_bh2->close() == 0);
            CHK(g_bh2->getState() == NdbBlob::Closed);
          }

          /* Check Blob handle is closed */
          char byte;
          Uint32 len = 1;
          CHK(g_bh1->readData(&byte, len) != 0);
          CHK(g_bh1->getNdbError().code == 4265);
          CHK(g_bh1->close() != 0);
          CHK(g_bh1->getNdbError().code == 4554);
          if(! g_opt.m_oneblob)
          {
            CHK(g_bh2->readData(&byte, len) != 0);
            CHK(g_bh2->getNdbError().code == 4265);
            CHK(g_bh2->close() != 0);
            CHK(g_bh2->getNdbError().code == 4554);
          }
          
          
          if (manualUnlock)
          {
            /* All Blob handles closed, now we can issue an
             * unlock operation and the main row should be
             * unlocked
             */
            const NdbOperation* readOp = (g_opr?g_opr:g_const_opr);
            const NdbLockHandle* lh = readOp->getLockHandle();
            CHK(lh != NULL);
            const NdbOperation* unlockOp = g_con->unlock(lh);
            CHK(unlockOp != NULL);
          }
          
          /* All Blob handles closed - manual or automatic
           * unlock op has been enqueued.  Now execute and
           * check that the row is unlocked.
           */
          CHK(g_con->execute(NoCommit) == 0);
          CHK(verifyRowNotLocked(tup) == 0);
          
          if (g_con->execute(Commit) != 0)
          {
            CHK((timeout= conHasTimeoutError()) == true);
          }
        }
        else
        {
          CHK((timeout= conHasTimeoutError()) == true);
        }
      }
      if (timeout)
      {
        DISP("ReadLockPk failed due to timeout on read("
             << conError() <<")  Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);
        opState= Retrying;
        sleep(1);
      }

      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);
    g_opr = 0;
    g_const_opr = 0;
    g_con = 0;
  }
  return 0;
}

static int
updatePk(int style, int api)
{
  DBG("--- updatePk " << stylename[style] << " " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("updatePk pk1=" << hex << tup.m_pk1);
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    OpState opState;

    do
    {
      opState= Normal;
      int mode = urandom(3);
      int error_code = mode == 0 ? 0 : 4275;
      CHK((g_con = g_ndb->startTransaction()) != 0);
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        if (mode == 0) {
          DBG("using updateTuple");
          CHK(g_opr->updateTuple() == 0);
        } else if (mode == 1) {
          DBG("using readTuple exclusive");
          CHK(g_opr->readTuple(NdbOperation::LM_Exclusive) == 0);
        } else {
          DBG("using readTuple - will fail and retry");
          CHK(g_opr->readTuple() == 0);
        }
        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
        setUDpartId(tup, g_opr);
        CHK(getBlobHandles(g_opr) == 0);
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0) {
          memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        }
        NdbOperation::OperationOptions opts;
        setUDpartIdNdbRecord(tup,
                             g_ndb->getDictionary()->getTable(g_opt.m_tname),
                             opts);
        if (mode == 0) {
          DBG("using updateTuple");
          CHK((g_const_opr= g_con->updateTuple(g_key_record, tup.m_key_row,
                                               g_blob_record, tup.m_row,
                                               NULL, &opts, sizeof(opts))) != 0);
        } else if (mode == 1) {
          DBG("using readTuple exclusive");
          CHK((g_const_opr= g_con->readTuple(g_key_record, tup.m_key_row,
                                             g_blob_record, tup.m_row,
                                             NdbOperation::LM_Exclusive,
                                             NULL, &opts, sizeof(opts))) != 0);
        } else {
          DBG("using readTuple - will fail and retry");
          CHK((g_const_opr= g_con->readTuple(g_key_record, tup.m_key_row,
                                             g_blob_record, tup.m_row,
                                             NdbOperation::LM_Read,
                                             NULL, &opts, sizeof(opts))) != 0);
        }
        CHK(getBlobHandles(g_const_opr) == 0);
      }

      bool timeout= false;
      if (style == 0) {
        CHK(setBlobValue(tup, error_code) == 0);
      } else if (style == 1) {
        CHK(setBlobWriteHook(tup, error_code) == 0);
      } else {
        CHK(g_con->execute(NoCommit) == 0);
        if (writeBlobData(tup, error_code) != 0)
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout &&
          (error_code == 0)) {
        /* Normal success case, try execute commit */
        if (g_con->execute(Commit) != 0)
          CHK((timeout= conHasTimeoutError()) == true);
        else
        {
          g_ndb->closeTransaction(g_con);
          break;
        }
      }
      if (timeout)
      {
        DISP("UpdatePk failed due to timeout("
             << conError() <<")  Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);
        
        opState= Retrying;
        sleep(1);
      }
      if (error_code)
        opState= Retrying;

      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);
    g_const_opr = 0;
    g_opr = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
writePk(int style, int api)
{
  DBG("--- writePk " << stylename[style] << " " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    enum OpState opState;
    
    do
    {
      opState= Normal;
      DBG("writePk pk1=" << hex << tup.m_pk1);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        CHK(g_opr->writeTuple() == 0);
        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
        setUDpartId(tup, g_opr);
        CHK(getBlobHandles(g_opr) == 0);
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        memcpy(&tup.m_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0) {
          memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
          memcpy(&tup.m_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        }
        NdbOperation::OperationOptions opts;
        setUDpartIdNdbRecord(tup,
                             g_ndb->getDictionary()->getTable(g_opt.m_tname),
                             opts);
        CHK((g_const_opr= g_con->writeTuple(g_key_record, tup.m_key_row,
                                            g_full_record, tup.m_row,
                                            NULL, &opts, sizeof(opts))) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(setBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(presetBH1(k) == 0);
        CHK(setBlobWriteHook(tup) == 0);
      } else {
        CHK(presetBH1(k) == 0);
        CHK(g_con->execute(NoCommit) == 0);
        if (writeBlobData(tup) != 0)
          CHK((timeout= conHasTimeoutError()) == true);
      }

      if (!timeout)
      {
        if (g_con->execute(Commit) != 0)
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (timeout)
      {
        DISP("WritePk failed due to timeout("
             << conError() <<")  Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);

        opState= Retrying;
        sleep(1);
      }
      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);

    g_const_opr = 0;
    g_opr = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
deletePk(int api)
{
  DBG("--- deletePk " << apiName[api] << " ---");
  unsigned n = 0;
  unsigned k = 0;
  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (; k < g_opt.m_rows; k++) {
      Tup& tup = g_tups[k];
      DBG("deletePk pk1=" << hex << tup.m_pk1);
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        CHK(g_opr->deleteTuple() == 0);
        /* Must set explicit partitionId before equal() calls as that's
         * where implicit Blob handles are created which need the 
         * partitioning info
         */
        setUDpartId(tup, g_opr);
        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0) {
          memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        }
        NdbOperation::OperationOptions opts;
        setUDpartIdNdbRecord(tup,
                             g_ndb->getDictionary()->getTable(g_opt.m_tname),
                             opts);
        CHK((g_const_opr= g_con->deleteTuple(g_key_record, tup.m_key_row,
                                             g_full_record, NULL,
                                             NULL, &opts, sizeof(opts))) != 0);
      }
      if (++n == g_opt.m_batch) {
        if (g_con->execute(Commit) != 0)
        {
          CHK(conHasTimeoutError());
          DISP("DeletePk failed due to timeout("
               << conError() <<")  Retries left : "
               << opTimeoutRetries -1);
          CHK(--opTimeoutRetries);
          
          opState= Retrying;
          k= k - (n-1);
          n= 0;
          sleep(1);
          break; // Out of for
        }
          
        g_ndb->closeTransaction(g_con);
        CHK((g_con = g_ndb->startTransaction()) != 0);
        n = 0;
      }
      g_const_opr = 0;
      g_opr = 0;
      tup.m_exists = false;
    } // for(
    if (opState == Normal)
    {
      if (n != 0) {
        if (g_con->execute(Commit) != 0)
        {
          CHK(conHasTimeoutError());
          DISP("DeletePk failed on last batch ("
               << conError() <<")  Retries left : "
               << opTimeoutRetries -1);
          CHK(--opTimeoutRetries);
          sleep(1);
          opState= Retrying;
          k= k- (n-1);
        } 
        n = 0;
      }
    }
    g_ndb->closeTransaction(g_con);
    g_con = 0;
  } while (opState == Retrying);

  return 0;
}

static int
deleteNoPk()
{
  DBG("--- deleteNoPk ---");
  Tup no_tup; // bug#24028
  no_tup.m_pk1 = 0xb1ff;
  const Chr& pk2chr = g_opt.m_pk2chr;
  if (pk2chr.m_len != 0) {
    char* const p = no_tup.m_pk2;
    uint len = urandom(pk2chr.m_len + 1);
    uint i = 0;
    if (! pk2chr.m_fixed) {
      *(uchar*)&p[0] = len;
      i++;
    }
    uint j = 0;
    while (j < len) {
      p[i] = "b1ff"[j % 4];
      i++;
      j++;
    }
  }
  no_tup.m_pk3 = 0xb1ff;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  Tup& tup =  no_tup;
  DBG("deletePk pk1=" << hex << tup.m_pk1);
  CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->deleteTuple() == 0);
  setUDpartId(tup, g_opr);
  CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
  if (pk2chr.m_len != 0) {
    CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(g_opr->equal("PK3", (char*)&tup.m_pk2) == 0);
  }
  CHK(g_con->execute(Commit) == -1); // fail
  // BUG: error should be on op but is on con now
  DBG("con: " << g_con->getNdbError());
  DBG("opr: " << g_opr->getNdbError());
  CHK(g_con->getNdbError().code == 626 || g_opr->getNdbError().code == 626);
  g_ndb->closeTransaction(g_con);
  g_opr = 0;
  g_con = 0;
  return 0;
}

// hash index ops

static int
readIdx(int style, int api)
{
  DBG("--- readIdx " << stylename[style] << " " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    enum OpState opState;
    
    do
    {
      opState= Normal;
      DBG("readIdx pk1=" << hex << tup.m_pk1);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      NdbOperation::LockMode lm = NdbOperation::LM_CommittedRead;
      switch(urandom(3))
      {
      case 0:
        lm = NdbOperation::LM_Read;
        break;
      case 1:
        lm = NdbOperation::LM_SimpleRead;
        break;
      default:
        break;
      }
      if (api == API_RECATTR)
      {
        CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
        CHK(g_opx->readTuple(lm) == 0);
        CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
        CHK(g_opx->equal("PK3", tup.m_pk3) == 0);
        /* No need to set partition Id for unique indexes */
        CHK(getBlobHandles(g_opx) == 0);
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
        memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        /* No need to set partition Id for unique indexes */
        CHK((g_const_opr= g_con->readTuple(g_idx_record, tup.m_key_row,
                                           g_blob_record, tup.m_row,
                                           lm)) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }

      bool timeout= false;
      if (style == 0) {
        CHK(getBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(setBlobReadHook(tup) == 0);
      } else {
        if(g_con->execute(NoCommit) ||
           readBlobData(tup))
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout)
      {
        if (g_con->execute(Commit) != 0)
        {
          CHK((timeout= conHasTimeoutError()) == true);
        }
      }
      if (!timeout)
      {
        // verify lock mode upgrade (already done by NdbIndexOperation)
        CHK((g_opx?g_opx:g_const_opr)->getLockMode() == NdbOperation::LM_Read);
        if (style == 0 || style == 1) {
          CHK(verifyBlobValue(tup) == 0);
        }
      }
      else
      {
        DISP("Timeout while reading via index ("
             << conError() <<")  Retries left : "
             << opTimeoutRetries -1);
        CHK(--opTimeoutRetries);
        
        opState= Retrying;
        sleep(1);
      }
      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);
    g_const_opr = 0;
    g_opx = 0;
    g_con = 0;
  }
  return 0;
}

static int
updateIdx(int style, int api)
{
  DBG("--- updateIdx " << stylename[style] << " " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    enum OpState opState;

    do
    {
      opState= Normal;
      DBG("updateIdx pk1=" << hex << tup.m_pk1);
      // skip 4275 testing
      CHK((g_con = g_ndb->startTransaction()) != 0);
      if (api == API_RECATTR)
      {
        CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
        CHK(g_opx->updateTuple() == 0);
        CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
        CHK(g_opx->equal("PK3", tup.m_pk3) == 0);
        /* No need to set partition Id for unique indexes */
        CHK(getBlobHandles(g_opx) == 0);
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
        memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        /* No need to set partition Id for unique indexes */
        CHK((g_const_opr= g_con->updateTuple(g_idx_record, tup.m_key_row,
                                             g_blob_record, tup.m_row)) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(setBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(setBlobWriteHook(tup) == 0);
      } else {
        if (g_con->execute(NoCommit) ||
            writeBlobData(tup))
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout)
      {
        if (g_con->execute(Commit) != 0)
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (timeout)
      {
        DISP("Timeout in Index Update ("
             << conError() <<")  Retries left : "
             << opTimeoutRetries-1);
        CHK(--opTimeoutRetries);
        opState= Retrying;
        sleep(1);
      }
      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);
    g_const_opr = 0;
    g_opx = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
writeIdx(int style, int api)
{
  DBG("--- writeIdx " << stylename[style] << " " << apiName[api] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
    enum OpState opState;
    
    do
    {
      opState= Normal;
      DBG("writeIdx pk1=" << hex << tup.m_pk1);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      if (api == API_RECATTR)
      {
        CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
        CHK(g_opx->writeTuple() == 0);
        CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
        CHK(g_opx->equal("PK3", tup.m_pk3) == 0);
        /* No need to set partition Id for unique indexes */
        CHK(getBlobHandles(g_opx) == 0);
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
        memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        memcpy(&tup.m_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
        memcpy(&tup.m_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
        memcpy(&tup.m_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        /* No need to set partition Id for unique indexes */
        CHK((g_const_opr= g_con->writeTuple(g_idx_record, tup.m_key_row,
                                            g_full_record, tup.m_row)) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(setBlobValue(tup) == 0);
      } else if (style == 1) {
        // non-nullable must be set
        CHK(g_bh1->setValue("", 0) == 0);
        CHK(setBlobWriteHook(tup) == 0);
      } else {
        // non-nullable must be set
        CHK(g_bh1->setValue("", 0) == 0);
        if (g_con->execute(NoCommit) ||
            writeBlobData(tup))
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout)
      {
        if (g_con->execute(Commit))
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (timeout)
      {
        DISP("Timeout in Index Write ("
             << conError() <<")  Retries left : "
             << opTimeoutRetries-1);
        CHK(--opTimeoutRetries);
        opState= Retrying;
        sleep(1);
      }
      g_ndb->closeTransaction(g_con);
    } while (opState == Retrying);
    g_const_opr = 0;
    g_opx = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
deleteIdx(int api)
{
  DBG("--- deleteIdx " << apiName[api] << " ---");
  unsigned n = 0;
  unsigned k = 0;
  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (; k < g_opt.m_rows; k++) {
      Tup& tup = g_tups[k];
      DBG("deleteIdx pk1=" << hex << tup.m_pk1);
      if (api == API_RECATTR)
      {
        CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
        CHK(g_opx->deleteTuple() == 0);
        CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
        CHK(g_opx->equal("PK3", tup.m_pk3) == 0);
        /* No need to set partition Id for unique indexes */
      }
      else
      {
        memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
        memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
        /* No need to set partition Id for unique indexes */
        CHK((g_const_opr= g_con->deleteTuple(g_idx_record, tup.m_key_row,
                                             g_full_record)) != 0);
      }
      if (++n == g_opt.m_batch) {
        if (g_con->execute(Commit))
        {
          CHK(conHasTimeoutError());
          DISP("Timeout deleteing via index ("
               << conError() <<")  Retries left :"
               << opTimeoutRetries-1);
          CHK(--opTimeoutRetries);
          opState= Retrying;
          k= k- (n-1);
          n= 0;
          sleep(1);
          break;
        }

        g_ndb->closeTransaction(g_con);
        CHK((g_con = g_ndb->startTransaction()) != 0);
        n = 0;
      }

      g_const_opr = 0;
      g_opx = 0;
      tup.m_exists = false;
    }
    if ((opState == Normal) &&
        (n != 0)) {
      if(g_con->execute(Commit))
      {
        CHK(conHasTimeoutError());
        DISP("Timeout on last idx delete batch ("
             << conError() <<")  Retries left :"
             << opTimeoutRetries-1);
        CHK(--opTimeoutRetries);
        opState= Retrying;
        k= k-(n-1);
        sleep(1);
      }
      n = 0;
    }
    g_ndb->closeTransaction(g_con);
  } while (opState == Retrying);
  g_con= 0;
  g_opx= 0;
  g_const_opr= 0;
  return 0;
}

// scan ops table and index

static int
readScan(int style, int api, bool idx)
{
  DBG("--- " << "readScan" << (idx ? "Idx" : "") << " " << stylename[style] << " " << apiName[api] << " ---");
  Tup tup;
  tup.alloc();  // allocate buffers

  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    NdbOperation::LockMode lm = NdbOperation::LM_CommittedRead;
    switch(urandom(3))
    {
    case 0:
      lm = NdbOperation::LM_Read;
      break;
    case 1:
      lm = NdbOperation::LM_SimpleRead;
      break;
    default:
      break;
    }
    if (api == API_RECATTR)
    {
      if (! idx) {
        CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
      } else {
        CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
      }
      CHK(g_ops->readTuples(lm,
                            g_scanFlags,
                            g_batchSize,
                            g_parallel) == 0);
      CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
        CHK(g_ops->getValue("PK3", (char *) &tup.m_pk3) != 0);
      }
      /* Don't bother setting UserDefined partitions for scan tests */
      CHK(getBlobHandles(g_ops) == 0);   
    }
    else
    {
      /* Don't bother setting UserDefined partitions for scan tests */
      if (! idx)
        CHK((g_ops= g_con->scanTable(g_full_record,
                                     lm)) != 0);
      else 
        CHK((g_ops= g_con->scanIndex(g_ord_record, g_full_record,
                                     lm)) != 0);
      CHK(getBlobHandles(g_ops) == 0);
    }

    if (style == 0) {
      CHK(getBlobValue(tup) == 0);
    } else if (style == 1) {
      CHK(setBlobReadHook(tup) == 0);
    }
    if (g_con->execute(NoCommit))
    {
      CHK(conHasTimeoutError());
      DISP("Timeout scan read ("
           << conError()
           << ").  Retries left : "
           <<  opTimeoutRetries - 1);
      CHK(--opTimeoutRetries);
      opState= Retrying;
      g_ndb->closeTransaction(g_con);
      continue;
    }
    
    // verify lock mode upgrade
    CHK(g_ops->getLockMode() == NdbOperation::LM_Read);
    unsigned rows = 0;
    while (1) {
      int ret;

      if (api == API_RECATTR)
      {
        tup.m_pk1 = (Uint32)-1;
        memset(tup.m_pk2, 'x', g_opt.m_pk2chr.m_len);
        tup.m_pk3 = -1;
        ret = g_ops->nextResult(true);
      }
      else
      {
        const char *out_row= NULL;

        if (0 == (ret = g_ops->nextResult(&out_row, true, false)))
        {
          memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
          if (g_opt.m_pk2chr.m_len != 0)
          {
            memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
            memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
          }
        }
      }
  
      if (ret == -1)
      {
        /* Timeout? */
        if (conHasTimeoutError())
        {
          /* Break out and restart scan unless we've
           * run out of attempts
           */
          DISP("Scan read failed due to deadlock timeout ("
               << conError() <<") retries left :" 
               << opTimeoutRetries -1);
          CHK(--opTimeoutRetries);

          opState= Retrying;
          sleep(1);
          break;
        }
      }
      CHK(opState == Normal);
      CHK((ret == 0) || (ret == 1));
      if (ret == 1)
        break;

      DBG("readScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
      Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
      CHK(k < g_opt.m_rows && g_tups[k].m_exists);
      tup.copyfrom(g_tups[k]);
      if (style == 0) {
        CHK(verifyBlobValue(tup) == 0);
      } else if (style == 1) {
        // execute ops generated by callbacks, if any
        CHK(verifyBlobValue(tup) == 0);
      } else {
        if (readBlobData(tup))
        {
          CHK(conHasTimeoutError());
          DISP("Timeout in readScan("
               << conError()
               << ") Retries left : "
               << opTimeoutRetries - 1);
          CHK(--opTimeoutRetries);
          opState= Retrying;
          sleep(1);
          continue;
        }
      }
      rows++;
    }
    g_ndb->closeTransaction(g_con);

    if (opState == Normal)
      CHK(g_opt.m_rows == rows);

  } while (opState == Retrying);

  g_con = 0;
  g_ops = 0;
  return 0;
}

static int
updateScan(int style, int api, bool idx)
{
  DBG("--- " << "updateScan" << (idx ? "Idx" : "") << " " << stylename[style] << " " << apiName[api] << " ---");
  Tup tup;
  tup.alloc();  // allocate buffers

  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    if (api == API_RECATTR)
    {
      if (! idx) {
        CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
      } else {
        CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
      }
      CHK(g_ops->readTuples(NdbOperation::LM_Exclusive,
                            g_scanFlags,
                            g_batchSize,
                            g_parallel) == 0);
      CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
        CHK(g_ops->getValue("PK3", (char *) &tup.m_pk3) != 0);
      }
      /* Don't bother setting UserDefined partitions for scan tests */
    }
    else
    {
      /* Don't bother setting UserDefined partitions for scan tests */
      if (! idx)
        CHK((g_ops= g_con->scanTable(g_key_record,
                                     NdbOperation::LM_Exclusive)) != 0);
      else
        CHK((g_ops= g_con->scanIndex(g_ord_record, g_key_record,
                                     NdbOperation::LM_Exclusive)) != 0);
    }
    CHK(g_con->execute(NoCommit) == 0);
    unsigned rows = 0;
    while (1) {
      const char *out_row= NULL;
      int ret;

      if (api == API_RECATTR)
      {
        tup.m_pk1 = (Uint32)-1;
        memset(tup.m_pk2, 'x', g_opt.m_pk2chr.m_totlen);
        tup.m_pk3 = -1;

        ret = g_ops->nextResult(true);
      }
      else
      {
        if(0 == (ret = g_ops->nextResult(&out_row, true, false)))
        {
          memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
          if (g_opt.m_pk2chr.m_len != 0) {
            memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
            memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
          }    
        }
      }
      
      if (ret == -1)
      {
        /* Timeout? */
        if (conHasTimeoutError())
        {
          /* Break out and restart scan unless we've
           * run out of attempts
           */
          DISP("Scan update failed due to deadlock timeout ("
               << conError() <<"), retries left :" 
               << opTimeoutRetries -1);
          CHK(--opTimeoutRetries);

          opState= Retrying;
          sleep(1);
          break;
        }
      }
      CHK(opState == Normal);
      CHK((ret == 0) || (ret == 1));
      if (ret == 1)
        break;      

      DBG("updateScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
      Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
      CHK(k < g_opt.m_rows && g_tups[k].m_exists);
      // calculate new blob values
      calcBval(g_tups[k], false);
      tup.copyfrom(g_tups[k]);
      // cannot do 4275 testing, scan op error code controls execution
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_ops->updateCurrentTuple()) != 0);
        CHK(getBlobHandles(g_opr) == 0);
      }
      else
      {
        CHK((g_const_opr = g_ops->updateCurrentTuple(g_con, g_blob_record, tup.m_row)) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(setBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(setBlobWriteHook(tup) == 0);
      } else {
        CHK(g_con->execute(NoCommit) == 0);
        if (writeBlobData(tup))
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout &&
          (g_con->execute(NoCommit)))
        CHK((timeout= conHasTimeoutError()) == true);

      if (timeout)
      {
        DISP("Scan update timeout("
             << conError()
             << ") Retries left : "
             << opTimeoutRetries-1);
        CHK(opTimeoutRetries--);
        opState= Retrying;
        sleep(1);
        break;
      }

      g_const_opr = 0;
      g_opr = 0;
      rows++;
    }
    if (opState == Normal)
    {
      CHK(g_con->execute(Commit) == 0);
      CHK(g_opt.m_rows == rows);
    }
    g_ndb->closeTransaction(g_con);
  } while (opState == Retrying);
  g_con = 0;
  g_ops = 0;
  return 0;
}

static int
lockUnlockScan(int style, int api, bool idx)
{
  DBG("--- " << "lockUnlockScan" << (idx ? "Idx" : "") << " " << stylename[style] << " " << apiName[api] << " ---");
  Tup tup;
  tup.alloc();  // allocate buffers

  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;

  do
  {
    opState= Normal;
    CHK((g_con = g_ndb->startTransaction()) != 0);
    NdbOperation::LockMode lm = NdbOperation::LM_Read;
    if (urandom(2) == 0)
      lm = NdbOperation::LM_Exclusive;
    
    Uint32 scanFlags = g_scanFlags | NdbScanOperation::SF_KeyInfo;

    if (api == API_RECATTR)
    {
      if (! idx) {
        CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
      } else {
        CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
      }
      CHK(g_ops->readTuples(lm,
                            scanFlags,
                            g_batchSize,
                            g_parallel) == 0);
      CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
        CHK(g_ops->getValue("PK3", (char *) &tup.m_pk3) != 0);
      }
      /* Don't bother setting UserDefined partitions for scan tests */
    }
    else
    {
      NdbScanOperation::ScanOptions opts;
      opts.optionsPresent = NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      opts.scan_flags = scanFlags;
      
      /* Don't bother setting UserDefined partitions for scan tests */
      if (! idx)
        CHK((g_ops= g_con->scanTable(g_key_record,
                                     lm, 0, &opts, sizeof(opts))) != 0);
      else
        CHK((g_ops= g_con->scanIndex(g_ord_record, g_key_record,
                                     lm, 0, 0, &opts, sizeof(opts))) != 0);
    }
    CHK(g_con->execute(NoCommit) == 0);
    unsigned rows = 0;
    while (1) {
      const char *out_row= NULL;
      int ret;

      if (api == API_RECATTR)
      {
        tup.m_pk1 = (Uint32)-1;
        memset(tup.m_pk2, 'x', g_opt.m_pk2chr.m_totlen);
        tup.m_pk3 = -1;

        ret = g_ops->nextResult(true);
      }
      else
      {
        if(0 == (ret = g_ops->nextResult(&out_row, true, false)))
        {
          memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
          if (g_opt.m_pk2chr.m_len != 0) {
            memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
            memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
          }    
        }
      }
      
      if (ret == -1)
      {
        /* Timeout? */
        if (conHasTimeoutError())
        {
          /* Break out and restart scan unless we've
           * run out of attempts
           */
          DISP("Scan failed due to deadlock timeout ("
               << conError() <<"), retries left :" 
               << opTimeoutRetries -1);
          CHK(--opTimeoutRetries);

          opState= Retrying;
          sleep(1);
          break;
        }
      }
      CHK(opState == Normal);
      CHK((ret == 0) || (ret == 1));
      if (ret == 1)
        break;      

      DBG("lockUnlockScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
      /* Get tuple info for current row */
      Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
      CHK(k < g_opt.m_rows && g_tups[k].m_exists);
      tup.copyfrom(g_tups[k]);
      
      if (api == API_RECATTR)
      {
        CHK((g_opr = g_ops->lockCurrentTuple()) != 0);
        CHK(g_opr->getLockHandle() != NULL);
        CHK(getBlobHandles(g_opr) == 0);
      }
      else
      {
        NdbOperation::OperationOptions opts;
        opts.optionsPresent = NdbOperation::OperationOptions::OO_LOCKHANDLE;
        CHK((g_const_opr = g_ops->lockCurrentTuple(g_con, g_blob_record, tup.m_row,
                                                   0, &opts, sizeof(opts))) != 0);
        CHK(getBlobHandles(g_const_opr) == 0);
      }
      bool timeout= false;
      if (style == 0) {
        CHK(getBlobValue(tup) == 0);
      } else if (style == 1) {
        CHK(setBlobReadHook(tup) == 0);
      } else {
        CHK(g_con->execute(NoCommit) == 0);
        if (readBlobData(tup))
          CHK((timeout= conHasTimeoutError()) == true);
      }
      if (!timeout)
      {
        if (g_con->execute(NoCommit) == 0)
        {
          /* Read executed successfully,
           * - Verify the Blob data
           * - Verify the row is locked
           * - Close the Blob handles
           * - Attempt to unlock
           */
          NdbOperation::LockMode lmused = g_ops->getLockMode();
          CHK((lmused == NdbOperation::LM_Read) ||
              (lmused == NdbOperation::LM_Exclusive));

          if (style == 0 || style == 1) {
            CHK(verifyBlobValue(tup) == 0);
          }

          /* Occasionally check that we are locked */
          if (urandom(200) == 0)
            CHK(verifyRowLocked(tup) == 0);
          
          /* Close Blob handles */
          CHK(g_bh1->close() == 0);
          if (! g_opt.m_oneblob)
            CHK(g_bh2->close() == 0);
          
          if (lm != NdbOperation::LM_CommittedRead)
          {
            /* All Blob handles closed, now we can issue an
             * unlock operation and the main row should be
             * unlocked
             */
            const NdbOperation* readOp = (g_opr?g_opr:g_const_opr);
            const NdbLockHandle* lh = readOp->getLockHandle();
            CHK(lh != NULL);
            const NdbOperation* unlockOp = g_con->unlock(lh);
            CHK(unlockOp != NULL);
          }
          
          /* All Blob handles closed - manual or automatic
           * unlock op has been enqueued.  Now execute 
           */
          CHK(g_con->execute(NoCommit) == 0);
        }
        else
        {
          CHK((timeout= conHasTimeoutError()) == true);
        }
      }

      if (timeout)
      {
        DISP("Scan read lock unlock timeout("
             << conError()
             << ") Retries left : "
             << opTimeoutRetries-1);
        CHK(opTimeoutRetries--);
        opState= Retrying;
        sleep(1);
        break;
      }

      g_const_opr = 0;
      g_opr = 0;
      rows++;
    }
    if (opState == Normal)
    {
      /* We've scanned all rows, locked them and then unlocked them
       * All rows should now be unlocked despite the transaction
       * not being committed.
       */
      for (unsigned k = 0; k < g_opt.m_rows; k++) {
        CHK(verifyRowNotLocked(g_tups[k]) == 0);
      }

      CHK(g_con->execute(Commit) == 0);
      CHK(g_opt.m_rows == rows);
    }
    g_ndb->closeTransaction(g_con);
  } while (opState == Retrying);
  g_con = 0;
  g_ops = 0;
  return 0;
}

static int
deleteScan(int api, bool idx)
{
  DBG("--- " << "deleteScan" << (idx ? "Idx" : "") << apiName[api] << " ---");
  Tup tup;
  Uint32 opTimeoutRetries= g_opt.m_timeout_retries;
  enum OpState opState;
  unsigned rows = 0;
  
  do
  {
    opState= Normal;
    
    CHK((g_con = g_ndb->startTransaction()) != 0);
    
    if (api == API_RECATTR)
    {
      if (! idx) {
        CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
      } else {
        CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
      }
      CHK(g_ops->readTuples(NdbOperation::LM_Exclusive,
                            g_scanFlags,
                            g_batchSize,
                            g_parallel) == 0);
      CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
        CHK(g_ops->getValue("PK3", (char *) &tup.m_pk3) != 0);
      }
      /* Don't bother setting UserDefined partitions for scan tests */
    }
    else
    {
      /* Don't bother setting UserDefined partitions for scan tests */
      if (! idx)
        CHK((g_ops= g_con->scanTable(g_key_record,
                                     NdbOperation::LM_Exclusive)) != 0);
      else
        CHK((g_ops= g_con->scanIndex(g_ord_record, g_key_record,
                                     NdbOperation::LM_Exclusive)) != 0);
    }
    CHK(g_con->execute(NoCommit) == 0);
    unsigned n = 0;
    while (1) {
      int ret;
      
      if (api == API_RECATTR)
      {
        tup.m_pk1 = (Uint32)-1;
        memset(tup.m_pk2, 'x', g_opt.m_pk2chr.m_len);
        tup.m_pk3 = -1;
        ret = g_ops->nextResult(true);
      }
      else
      {
        const char *out_row= NULL;
        
        if (0 == (ret = g_ops->nextResult(&out_row, true, false)))
        {
          memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
          if (g_opt.m_pk2chr.m_len != 0)
          {
            memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
            memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
          }
        }
      }

      if (ret == -1)
      {
        /* Timeout? */
        if (conHasTimeoutError())
        {
          /* Break out and restart scan unless we've
           * run out of attempts
           */
          DISP("Scan delete failed due to deadlock timeout ("
               << conError() <<") retries left :" 
               << opTimeoutRetries -1);
          CHK(--opTimeoutRetries);
          
          opState= Retrying;
          sleep(1);
          break;
        }
      }
      CHK(opState == Normal);
      CHK((ret == 0) || (ret == 1));
      if (ret == 1)
        break;
      
      while (1) {
        DBG("deleteScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
        Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
        CHK(k < g_opt.m_rows && g_tups[k].m_exists);
        g_tups[k].m_exists = false;
        if (api == API_RECATTR)
          CHK(g_ops->deleteCurrentTuple() == 0);
        else
          CHK(g_ops->deleteCurrentTuple(g_con, g_key_record) != NULL);
        tup.m_pk1 = (Uint32)-1;
        memset(tup.m_pk2, 'x', g_opt.m_pk2chr.m_len);
        tup.m_pk3 = -1;
        if (api == API_RECATTR)
          ret = g_ops->nextResult(false);
        else
        {      
          const char *out_row= NULL;
          ret = g_ops->nextResult(&out_row, false, false);
          if (ret == 0)
          {
            memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
            if (g_opt.m_pk2chr.m_len != 0)
            {
              memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
              memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
            }
          }
        }
        
        if (ret == -1)
        {
          /* Timeout? */
          if (conHasTimeoutError())
          {
            /* Break out and restart scan unless we've
             * run out of attempts
             */
            DISP("Scan delete failed due to deadlock timeout ("
                 << conError() <<") retries left :" 
                 << opTimeoutRetries -1);
            CHK(--opTimeoutRetries);
            
            opState= Retrying;
            sleep(1);
            break;
          }
        }
        CHK(opState == Normal);
        CHK((ret == 0) || (ret == 1) || (ret == 2));
        
        if (++n == g_opt.m_batch || ret == 2) {
          DBG("execute batch: n=" << n << " ret=" << ret);
          if (! g_opt.m_fac) {
            CHK(g_con->execute(NoCommit) == 0);
          } else {
            CHK(g_con->execute(Commit) == 0);
            CHK(g_con->restart() == 0);
          }
          rows+= n;
          n = 0;
        }
        if (ret == 2)
          break;
      }
      if (opState == Retrying)
        break;
    }
    if (opState == Normal)
    {
      rows+= n;
      CHK(g_con->execute(Commit) == 0);
      CHK(g_opt.m_rows == rows);
    }
    g_ndb->closeTransaction(g_con);
    
  } while (opState == Retrying);
  g_con = 0;
  g_ops = 0;
  return 0;
}


enum OpTypes { 
  PkRead,
  PkInsert,
  PkUpdate,
  PkWrite,
  PkDelete,
  UkRead,
  UkUpdate,
  UkWrite,
  UkDelete};

static const char*
operationName(OpTypes optype)
{
  switch(optype){
  case PkRead:
    return "Pk Read";
  case PkInsert:
    return "Pk Insert";
  case PkUpdate:
    return "Pk Update";
  case PkWrite:
    return "Pk Write";
  case PkDelete:
    return "Pk Delete";
  case UkRead:
    return "Uk Read";
  case UkUpdate:
    return "Uk Update";
  case UkWrite:
    return "Uk Write";
  case UkDelete:
    return "Uk Delete";
  default:
    return "Bad operation type";
  }
}

static const char*
aoName(int abortOption)
{
  if (abortOption == 0)
    return "AbortOnError";
  return "IgnoreError";
}

static int
setupOperation(NdbOperation*& op, OpTypes optype, Tup& tup)
{
  bool pkop;
  switch(optype){
  case PkRead: case PkInsert : case PkUpdate: 
  case PkWrite : case PkDelete :
    pkop=true;
    break;
  default:
    pkop= false;
  }
  
  if (pkop)
    CHK((op= g_con->getNdbOperation(g_opt.m_tname)) != 0);
  else
    CHK((op = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);

  switch(optype){
  case PkRead:
  case UkRead:
    CHK(op->readTuple() == 0);
    break;
  case PkInsert:
    CHK(op->insertTuple() == 0);
    break;
  case PkUpdate:
  case UkUpdate:
    CHK(op->updateTuple() == 0);
    break;
  case PkWrite:
  case UkWrite:
    CHK(op->writeTuple() == 0);
    break;
  case PkDelete:
  case UkDelete:
    CHK(op->deleteTuple() == 0);
    break;
  default:
    CHK(false);
    return -1;
  }
  
  if (pkop)
  {
    setUDpartId(tup, op);
    CHK(op->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(op->equal("PK2", tup.m_pk2) == 0);
      CHK(op->equal("PK3", tup.m_pk3) == 0);
    }
  }
  else
  {
    CHK(op->equal("PK2", tup.m_pk2) == 0);
    CHK(op->equal("PK3", tup.m_pk3) == 0);
  }
  
  CHK(getBlobHandles(op) == 0);
  
  switch(optype){
  case PkRead:
  case UkRead:
    CHK(getBlobValue(tup) == 0);
    break;
  case PkInsert:
  case PkUpdate:
  case UkUpdate:
    /* Fall through */
  case PkWrite:
  case UkWrite:
    CHK(setBlobValue(tup) == 0);
    break;
  case PkDelete:
  case UkDelete:
    /* Nothing */
    break;
  default:
    CHK(false);
    return -1;
  }

  return 0;
}

static int
bugtest_36756()
{
  /* Transaction which had accessed a Blob table was ignoring
   * abortOption passed in the execute() call.
   * Check that option passed in execute() call overrides 
   * default / manually set operation abortOption, even in the
   * presence of Blobs in the transaction
   */

  /* Operation         AbortOnError             IgnoreError
   * PkRead            NoDataFound*             NoDataFound
   * PkInsert          Duplicate key            Duplicate key*
   * PkUpdate          NoDataFound              NoDataFound*
   * PkWrite           NoDataFound              NoDataFound*
   * PkDelete          NoDataFound              NoDataFound*
   * UkRead            NoDataFound*             NoDataFound
   * UkUpdate          NoDataFound              NoDataFound*
   * UkWrite           NoDataFound              NoDataFound*
   * UkDelete          NoDataFound              NoDataFound*
   * 
   * * Are interesting, where non-default behaviour is requested.
   */
  
  struct ExpectedOutcome
  {
    int executeRc;
    int transactionErrorCode;
    int opr1ErrorCode;
    int opr2ErrorCode;
    int commitStatus;
  };

  /* Generally, AbortOnError sets the transaction error
   * but not the Operation error codes
   * IgnoreError sets the transaction error and the
   * failing operation error code(s)
   * Odd cases : 
   *   Pk Write : Can't fail due to key presence, just
   *              incorrect NULLs etc.
   *   Uk Write : Key must exist, so not really different
   *              to Update?
   */
  ExpectedOutcome outcomes[9][2]=
  {
    // PkRead
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}, // IE
    // PkInsert
    // Note operation order reversed for insert
    {{-1, 630, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 630, 0, 630, NdbTransaction::Started}}, // IE
    // PkUpdate
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}, // IE
    // PkWrite
    {{0, 0, 0, 0, NdbTransaction::Started},      // AE
     {0, 0, 0, 0, NdbTransaction::Started}},     // IE
    // PkDelete
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}, // IE
    // UkRead
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}, // IE
    // UkUpdate
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}, // IE
    // UkWrite
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}, // IE
    // UkDelete
    {{-1, 626, 0, 0, NdbTransaction::Aborted},   // AE
     {0, 626, 0, 626, NdbTransaction::Started}}  // IE
  };

  DBG("bugtest_36756 : IgnoreError Delete of nonexisting tuple aborts");
  DBG("                Also 36851 : Insert IgnoreError of existing tuple aborts");

  for (int iterations=0; iterations < 50; iterations++)
  {
    /* Recalculate and insert different tuple every time to 
     * get different keys(and therefore nodes), and
     * different length Blobs, including zero length
     * and NULL
     */
    calcTups(true);
    
    Tup& tupExists = g_tups[0];
    Tup& tupDoesNotExist = g_tups[1];
    
    /* Setup table with just 1 row present */
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->insertTuple() == 0);
    CHK(g_opr->equal("PK1", tupExists.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(g_opr->equal("PK2", tupExists.m_pk2) == 0);
      CHK(g_opr->equal("PK3", tupExists.m_pk3) == 0);
    }
    setUDpartId(tupExists, g_opr);
    CHK(getBlobHandles(g_opr) == 0);
    
    CHK(setBlobValue(tupExists) == 0);
    
    CHK(g_con->execute(Commit) == 0);
    g_con->close();
    
    DBG("Iteration : " << iterations);
    for (int optype=PkRead; optype <= UkDelete; optype++)
    {
      DBG("  " << operationName((OpTypes)optype));

      Tup* tup1= &tupExists;
      Tup* tup2= &tupDoesNotExist;

      if (optype == PkInsert)
      {
        /* Inserts - we want the failing operation to be second
         * rather than first to avoid hitting bugs with IgnoreError
         * and the first DML in a transaction
         * So we swap them
         */
        tup1= &tupDoesNotExist; // (Insert succeeds)
        tup2= &tupExists; //(Insert fails)
      }

      for (int abortOption=0; abortOption < 2; abortOption++)
      {
        DBG("    " << aoName(abortOption));
        NdbOperation *opr1, *opr2;
        NdbOperation::AbortOption ao= (abortOption==0)?
          NdbOperation::AbortOnError : 
          NdbOperation::AO_IgnoreError;
        
        CHK((g_con= g_ndb->startTransaction()) != 0);
        
        /* Operation 1 */
        CHK(setupOperation(opr1, (OpTypes)optype, *tup1) == 0);
        
        /* Operation2 */
        CHK(setupOperation(opr2, (OpTypes)optype, *tup2) == 0);

        ExpectedOutcome eo= outcomes[optype][abortOption];
        
        int rc = g_con->execute(NdbTransaction::NoCommit, ao);

        DBG("execute returned " << rc <<
            " Trans err " << g_con->getNdbError().code <<
            " Opr1 err " << opr1->getNdbError().code <<
            " Opr2 err " << opr2->getNdbError().code <<
            " CommitStatus " << g_con->commitStatus());

        CHK(rc == eo.executeRc);        
        CHK(g_con->getNdbError().code == eo.transactionErrorCode);
        CHK(opr1->getNdbError().code == eo.opr1ErrorCode);
        CHK(opr2->getNdbError().code == eo.opr2ErrorCode);
        CHK(g_con->commitStatus() == eo.commitStatus);
        
        g_con->close();
      }
    }
    
    /* Now delete the 'existing'row */
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->deleteTuple() == 0);
    setUDpartId(tupExists, g_opr);
    CHK(g_opr->equal("PK1", tupExists.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(g_opr->equal("PK2", tupExists.m_pk2) == 0);
      CHK(g_opr->equal("PK3", tupExists.m_pk3) == 0);
    }

    CHK(g_con->execute(Commit) == 0);
    g_con->close();
  }

  g_opr= 0;
  g_con= 0;
  g_bh1= 0;

  return 0;
}


static int
bugtest_45768()
{
  /* Transaction inserting using blobs has an early error 
     resulting in kernel-originated rollback.
     Api then calls execute(Commit) which chokes on Blob
     objects
     
   */
  DBG("bugtest_45768 : Batched blob transaction with abort followed by commit");
  
  const int numIterations = 5;

  for (int iteration=0; iteration < numIterations; iteration++)
  {
    /* Recalculate and insert different tuple every time to 
     * get different keys(and therefore nodes), and
     * different length Blobs, including zero length
     * and NULL
     */
    calcTups(true);
    
    const Uint32 totalRows = 100; 
    const Uint32 preExistingTupNum =  totalRows / 2;
    
    Tup& tupExists = g_tups[ preExistingTupNum ];
    
    /* Setup table with just 1 row present */
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->insertTuple() == 0);
    CHK(g_opr->equal("PK1", tupExists.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(g_opr->equal("PK2", tupExists.m_pk2) == 0);
      CHK(g_opr->equal("PK3", tupExists.m_pk3) == 0);
    }
    setUDpartId(tupExists, g_opr);
    CHK(getBlobHandles(g_opr) == 0);
    
    CHK(setBlobValue(tupExists) == 0);
    
    CHK(g_con->execute(Commit) == 0);
    g_con->close();

    DBG("Iteration : " << iteration);
    
    /* Now do batched insert, including a TUP which already
     * exists
     */
    int rc = 0;
    int retries = 10;

    do
    {
      CHK((g_con = g_ndb->startTransaction()) != 0);
      
      for (Uint32 tupNum = 0; tupNum < totalRows ; tupNum++)
      {
        Tup& tup = g_tups[ tupNum ];
        CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
        CHK(g_opr->insertTuple() == 0);
        CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
        if (g_opt.m_pk2chr.m_len != 0)
        {
          CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
          CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
        }
        setUDpartId(tup, g_opr);

        CHK(getBlobHandles(g_opr) == 0);
        CHK(setBlobValue(tup) == 0);
      }
      
      /* Now execute NoCommit */
      int rc = g_con->execute(NdbTransaction::NoCommit);
      
      CHK(rc == -1);

      if (g_con->getNdbError().code == 630)
        break; /* Expected */
      
      CHK(g_con->getNdbError().code == 1218); // Send buffers overloaded
     
      DBG("Send Buffers overloaded, retrying");
      sleep(1);
      g_con->close();
    } while (retries--);

    CHK(g_con->getNdbError().code == 630);
            
    /* Now execute Commit */
    rc = g_con->execute(NdbTransaction::Commit);

    CHK(rc == -1);
    /* Transaction aborted already */
    CHK(g_con->getNdbError().code == 4350);

    g_con->close();
    
    /* Now delete the 'existing'row */
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->deleteTuple() == 0);
    setUDpartId(tupExists, g_opr);
    CHK(g_opr->equal("PK1", tupExists.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(g_opr->equal("PK2", tupExists.m_pk2) == 0);
      CHK(g_opr->equal("PK3", tupExists.m_pk3) == 0);
    }

    CHK(g_con->execute(Commit) == 0);
    g_con->close();
  }

  g_opr= 0;
  g_con= 0;
  g_bh1= 0;

  return 0;
}

static int bugtest_48040()
{
  /* When batch of operations triggers unique index 
   * maint triggers (which fire back to TC) and 
   * TC is still receiving ops in batch from the API
   * TC uses ContinueB to self to defer trigger
   * processing until all operations have been
   * received.
   * If the transaction starts aborting (due to some
   * problem in the original operations) while the
   * ContinueB is 'in-flight', the ContinueB never
   * terminates and causes excessive CPU consumption
   *
   * This testcase sets an ERROR INSERT to detect
   * the excessive ContinueB use in 1 transaction,
   * and runs bugtest_bug45768 to generate the 
   * scenario
   */
  NdbRestarter restarter;
  
  DBG("bugtest 48040 - Infinite ContinueB loop in TC abort + unique");

  restarter.waitConnected();

  int rc = restarter.insertErrorInAllNodes(8082);

  DBG(" Initial error insert rc" << rc << endl);
  
  rc = bugtest_45768();

  /* Give time for infinite loop to build */
  sleep(10);
  restarter.insertErrorInAllNodes(0);

  return rc;
}


static int bugtest_62321()
{
  /* Having a Blob operation in a batch with other operations
   * causes the other operation's ignored error not to be
   * set as the transaction error code after execution.
   * This is used (e.g in MySQLD) to check for conflicts
   */
  DBG("bugtest_62321 : Error code from other ops in batch obscured");

  /*
     1) Setup table : 1 row exists, another doesnt
     2) Start transaction
     3) Define failing before op
     4) Define Blob op with/without post-exec part
     5) Define failing after op
     6) Execute
     7) Check results
  */
  calcTups(true);

  /* Setup table */
  Tup& tupExists = g_tups[0];
  Tup& notExists = g_tups[1];
  {
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->insertTuple() == 0);
    CHK(g_opr->equal("PK1", tupExists.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(g_opr->equal("PK2", tupExists.m_pk2) == 0);
      CHK(g_opr->equal("PK3", tupExists.m_pk3) == 0);
    }
    setUDpartId(tupExists, g_opr);
    CHK(getBlobHandles(g_opr) == 0);

    CHK(setBlobValue(tupExists) == 0);

    CHK(g_con->execute(Commit) == 0);
    g_con->close();
  }

  for (int scenario = 0; scenario < 4; scenario++)
  {
    DBG(" Scenario : " << scenario);
    CHK((g_con= g_ndb->startTransaction()) != 0);
    NdbOperation* failOp = NULL;
    if ((scenario & 0x1) == 0)
    {
      DBG("  Fail op before");
      /* Define failing op in batch before Blob op */
      failOp= g_con->getNdbOperation(g_opt.m_tname);
      CHK(failOp != 0);
      CHK(failOp->readTuple() == 0);
      CHK(failOp->equal("PK1", notExists.m_pk1) == 0);
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(failOp->equal("PK2", notExists.m_pk2) == 0);
        CHK(failOp->equal("PK3", notExists.m_pk3) == 0);
      }
      setUDpartId(notExists, failOp);
      CHK(failOp->getValue("PK1") != 0);
      CHK(failOp->setAbortOption(NdbOperation::AO_IgnoreError) == 0);
    }

    /* Now define successful Blob op */
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->readTuple() == 0);
    CHK(g_opr->equal("PK1", tupExists.m_pk1) == 0);
    if (g_opt.m_pk2chr.m_len != 0)
    {
      CHK(g_opr->equal("PK2", tupExists.m_pk2) == 0);
      CHK(g_opr->equal("PK3", tupExists.m_pk3) == 0);
    }
    setUDpartId(tupExists, g_opr);
    CHK(getBlobHandles(g_opr) == 0);

    CHK(getBlobValue(tupExists) == 0);


    /* Define failing batch op after Blob op if not defined before */
    if (failOp == 0)
    {
      DBG("  Fail op after");
      failOp= g_con->getNdbOperation(g_opt.m_tname);
      CHK(failOp != 0);
      CHK(failOp->readTuple() == 0);
      CHK(failOp->equal("PK1", notExists.m_pk1) == 0);
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(failOp->equal("PK2", notExists.m_pk2) == 0);
        CHK(failOp->equal("PK3", notExists.m_pk3) == 0);
      }
      setUDpartId(notExists, failOp);
      CHK(failOp->getValue("PK1") != 0);
      CHK(failOp->setAbortOption(NdbOperation::AO_IgnoreError) == 0);
    }

    /* Now execute and check rc etc */
    NdbTransaction::ExecType et = (scenario & 0x2) ?
      NdbTransaction::NoCommit:
      NdbTransaction::Commit;

    DBG("  Executing with execType = " << ((et == NdbTransaction::NoCommit)?
                                           "NoCommit":"Commit"));
    int rc = g_con->execute(NdbTransaction::NoCommit);

    CHK(rc == 0);
    CHK(g_con->getNdbError().code == 626);
    CHK(failOp->getNdbError().code == 626);
    CHK(g_opr->getNdbError().code == 0);
    DBG("  Error code on transaction as expected");

    g_con->close();
  }

  return 0;
}

static int bugtest_28746560()
{
  /**
   * Testing of Blob behaviour when batching operations on the same
   * key.
   * This is generally done by the replication slave
   */
  ndbout_c("bugtest_28746560");

  calcTups(true, false);

  /* TODO :
   * - Use IgnoreError sometimes
   */

  /* Some options to debug... */
  const bool serial = false; // Batching
  const bool serialInsert = false; // Batching after an insert
  const Uint32 MaxBatchedModifies = 30;
  Tup values[MaxBatchedModifies];

  for (Uint32 pass=0; pass < 2; pass ++)
  {
    ndbout_c("pass %s", (pass == 0?"INSERT":"DELETE"));

    for (Uint32 row=0; row < g_opt.m_rows; row++)
    {
      g_con = g_ndb->startTransaction();
      CHK(g_con != NULL);

      DBG("Row " << row);
      if (pass == 0)
      {
        OpTypes insType = ((urandom(2) == 1)?
                           PkInsert:
                           PkWrite);
        NdbOperation* op;
        CHK(setupOperation(op, insType, g_tups[row]) == 0);
        DBG("  " << (insType == PkInsert? "INS" : "WRI") <<
            "    \t" << (void*) op);

        if (serial || serialInsert)
        {
          CHK(g_con->execute(NoCommit) == 0);
        }
      }

      const Uint32 numBatchedModifies = urandom(MaxBatchedModifies);
      for (Uint32 mod = 0; mod < numBatchedModifies; mod++)
      {
        /* Calculate new value */
        values[mod].copyAllfrom(g_tups[row]);
        calcBval(values[mod], false);

        int modifyStyle = urandom(4);

        if (modifyStyle == 0 || modifyStyle == 1)
        {
          NdbOperation* op;
          CHK(setupOperation(op,
                             (modifyStyle == 0 ? PkUpdate : PkWrite),
                             values[mod]) == 0);
          DBG("  " << (modifyStyle == 0 ? "UPD":"WRI")
              << "    \t"
              << (void*) op);
        }
        else
        {
          OpTypes insOpType = PkInsert;
          const char* name = "INS";
          if (modifyStyle == 3)
          {
            insOpType = PkWrite;
            name = "WRI";
          }

          NdbOperation* delOp;
          CHK(setupOperation(delOp,
                             PkDelete,
                             values[mod]) == 0);

          NdbOperation* insOp;
          CHK(setupOperation(insOp,
                             insOpType,
                             values[mod]) == 0);

          DBG("  DEL" << name << " \t"
              << (void*) delOp
              << (void*) insOp);
        }

        if (serial || serialInsert)
        {
          CHK(g_con->execute(NoCommit) == 0);
        }
      }

      if (pass == 1)
      {
        // define deleteRow
        NdbOperation* op;
        CHK(setupOperation(op,
                           PkDelete,
                           g_tups[row]) == 0);

        DBG("  DEL    \t" << (void*) op);

        if (serial)
        {
          CHK(g_con->execute(NoCommit) == 0);
        }
      }

      CHK(g_con->execute(Commit) == 0);

      g_con->close();
      g_con = NULL;

      Tup& finalValue = (numBatchedModifies ?
                         values[numBatchedModifies - 1] :
                         g_tups[row]);

      g_con=g_ndb->startTransaction();
      CHK(g_con != NULL);

      NdbOperation* readOp;
      CHK(setupOperation(readOp, PkRead, finalValue) == 0);

      DBG("  READ   \t" << (void*) readOp);

      CHK(g_con->execute(Commit) == 0);

      if (pass == 0)
      {
        CHK(verifyBlobValue(finalValue) == 0);
        DBG("  READ OK");
      }
      else if (pass == 1)
      {
        if (readOp->getNdbError().code != 626)
        {
          ndbout_c("Error, expected 626 but found %u %s",
                   readOp->getNdbError().code,
                   readOp->getNdbError().message);
          return -1;
        }
        DBG("  READ DEL OK");
      }

      g_con->close();
      g_con = NULL;
    } // row
  } // pass

  return 0;
}

// main

// from here on print always
#undef DBG
#define DBG(x) \
  do { \
    ndbout << "line " << __LINE__ << " " << x << endl; \
  } while (0)

static int
testmain()
{
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  CHK(g_ndb->init(20) == 0);
  CHK(g_ndb->waitUntilReady() == 0);
  g_dic = g_ndb->getDictionary();
  initblobs();
  initConstants();
  g_tups = new Tup [g_opt.m_rows];

  // Create tablespace if we're going to use disk based data
  if (testcase('h'))
    createDefaultTableSpace(); 

  if (g_opt.m_seed == -1)
    g_opt.m_seed = NdbHost_GetProcessId();
  if (g_opt.m_seed != 0) {
    DBG("random seed = " << g_opt.m_seed);
    ndb_srand(g_opt.m_seed);
  }
  for (g_loop = 0; g_opt.m_loop == 0 || g_loop < g_opt.m_loop; g_loop++) {
    for (int storage= 0; storage < 2; storage++) {
      if (!testcase(storageSymbol[storage]))
        continue;
      
      DBG("Create table " << storageName[storage]);
      CHK(dropTable() == 0);
      CHK(createTable(storage) == 0);
      { /* Dump created table information */
        Bcol& b1 = g_blob1;
        DBG("FragType: " << g_dic->getTable(g_opt.m_tname)->getFragmentType()); 
        CHK(NdbBlob::getBlobTableName(b1.m_btname, g_ndb, g_opt.m_tname, "BL1") == 0);
        DBG("BL1: inline=" << b1.m_inline << " part=" << b1.m_partsize << " table=" << b1.m_btname);
        if (! g_opt.m_oneblob) {
          Bcol& b2 = g_blob2;
          CHK(NdbBlob::getBlobTableName(b2.m_btname, g_ndb, g_opt.m_tname, "BL2") == 0);
          DBG("BL2: inline=" << b2.m_inline << " part=" << b2.m_partsize << " table=" << b2.m_btname);
        }
      }

      /* Capability to adjust disk scan parameters to avoid scan
       * timeouts with disk based Blobs (Error 274)
       */
      if (storage == STORAGE_DISK)
      {
        g_usingDisk= true;
        // TODO : Resolve whether we need to adjust these for disk data
        // Currently the scans are passing ok without this.
        g_batchSize= 0;
        g_parallel= 0;
        g_scanFlags= 0; //NdbScanOperation::SF_DiskScan;
      }
      else
      {
        g_usingDisk= false;
        g_batchSize= 0;
        g_parallel= 0;
        g_scanFlags= 0;
      }

      // TODO Remove/resolve
      DBG("Settings : usingdisk " << g_usingDisk
          << " batchSize " << g_batchSize
          << " parallel " << g_parallel
          << " scanFlags " << g_scanFlags);

      int style;
      int api;
      DBG("=== loop " << g_loop << " ===");
      if (g_opt.m_seed == 0)
        ndb_srand(g_loop);
      if (g_opt.m_bugtest != 0) {
        // test some bug# instead
        CHK((*g_opt.m_bugtest)() == 0);
        continue;
      }    
      /* Loop over API styles */
      for (api = 0; api <=1; api++) {
        // pk
        if (! testcase(apiSymbol[api]))
          continue;
        for (style = 0; style <= 2; style++) {
          if (! testcase('k') || ! testcase(style) )
            continue;
          DBG("--- pk ops " << stylename[style] << " " << apiName[api] << " ---");
          if (testcase('n')) {
            calcTups(true);
            CHK(insertPk(style, api) == 0);
            CHK(verifyBlob() == 0);
            CHK(readPk(style, api) == 0);
            if (testcase('u')) {
              calcTups(false);
              CHK(updatePk(style, api) == 0);
              CHK(verifyBlob() == 0);
              CHK(readPk(style, api) == 0);
            }
            if (testcase('l')) {
              CHK(readLockPk(style,api) == 0);
            }
            if (testcase('d')) {
              CHK(deletePk(api) == 0);
              CHK(deleteNoPk() == 0);
              CHK(verifyBlob() == 0);
            }
          }
          if (testcase('w')) {
            calcTups(true);
            CHK(writePk(style, api) == 0);
            CHK(verifyBlob() == 0);
            CHK(readPk(style, api) == 0);
            if (testcase('u')) {
              calcTups(false);
              CHK(writePk(style, api) == 0);
              CHK(verifyBlob() == 0);
              CHK(readPk(style, api) == 0);
            }
            if (testcase('l')) {
              CHK(readLockPk(style,api) == 0);
            }
            if (testcase('d')) {
              CHK(deletePk(api) == 0);
              CHK(deleteNoPk() == 0);
              CHK(verifyBlob() == 0);
            }
          }
        }
        
        // hash index
        for (style = 0; style <= 2; style++) {
          if (! testcase('i') || ! testcase(style))
            continue;
          DBG("--- idx ops " << stylename[style] << " " << apiName[api] << " ---");
          if (testcase('n')) {
            calcTups(true);
            CHK(insertPk(style, api) == 0);
            CHK(verifyBlob() == 0);
            CHK(readIdx(style, api) == 0);
            if (testcase('u')) {
              calcTups(false);
              CHK(updateIdx(style, api) == 0);
              CHK(verifyBlob() == 0);
              CHK(readIdx(style, api) == 0);
            }
            if (testcase('d')) {
              CHK(deleteIdx(api) == 0);
              CHK(verifyBlob() == 0);
            }
          }
          if (testcase('w')) {
            calcTups(false);
            CHK(writePk(style, api) == 0);
            CHK(verifyBlob() == 0);
            CHK(readIdx(style, api) == 0);
            if (testcase('u')) {
              calcTups(false);
              CHK(writeIdx(style, api) == 0);
              CHK(verifyBlob() == 0);
              CHK(readIdx(style, api) == 0);
            }
            if (testcase('d')) {
              CHK(deleteIdx(api) == 0);
              CHK(verifyBlob() == 0);
            }
          }
        }
        // scan table
        for (style = 0; style <= 2; style++) {
          if (! testcase('s') || ! testcase(style))
            continue;
          DBG("--- table scan " << stylename[style] << " " << apiName[api] << " ---");
          calcTups(true);
          CHK(insertPk(style, api) == 0);
          CHK(verifyBlob() == 0);
          CHK(readScan(style, api, false) == 0);
          if (testcase('u')) {
            CHK(updateScan(style, api, false) == 0);
            CHK(verifyBlob() == 0);
          }
          if (testcase('l')) {
            CHK(lockUnlockScan(style, api, false) == 0);
          }
          if (testcase('d')) {
            CHK(deleteScan(api, false) == 0);
            CHK(verifyBlob() == 0);
          }
        }
        // scan index
        for (style = 0; style <= 2; style++) {
          if (! testcase('r') || ! testcase(style))
            continue;
          DBG("--- index scan " << stylename[style] << " " << apiName[api] << " ---");
          calcTups(true);
          CHK(insertPk(style, api) == 0);
          CHK(verifyBlob() == 0);
          CHK(readScan(style, api, true) == 0);
          if (testcase('u')) {
            CHK(updateScan(style, api, true) == 0);
            CHK(verifyBlob() == 0);
          }
          if (testcase('l')) {
            CHK(lockUnlockScan(style, api, true) == 0);
          }
          if (testcase('d')) {
            CHK(deleteScan(api, true) == 0);
            CHK(verifyBlob() == 0);
          }
        }
      } // for (api
    } // for (storage
  } // for (loop
  if (g_opt.m_nodrop == false)
  {
    dropTable();
  }
  delete [] g_tups;
  g_tups = 0;
  delete g_ndb;
  g_ndb = 0;
  return 0;
}

// separate performance test

struct Tmr {    // stolen from testOIBasic
  Tmr() {
    clr();
  }
  void clr() {
    m_on = m_ms = m_cnt = m_time[0] = m_text[0] = 0;
  }
  void on() {
    require(m_on == 0);
    m_on = NdbTick_CurrentMillisecond();
  }
  void off(unsigned cnt = 0) {
    const Uint64 off = NdbTick_CurrentMillisecond();
    require(m_on != 0 && off >= m_on);
    m_ms += off - m_on;
    m_cnt += cnt;
    m_on = 0;
  }
  const char* time() {
    if (m_cnt == 0)
      sprintf(m_time, "%u ms", (Uint32)m_ms);
    else
      sprintf(m_time, "%u ms per %u ( %llu ms per 1000 )", (Uint32)m_ms, m_cnt, (1000 * m_ms) / m_cnt);
    return m_time;
  }
  const char* pct (const Tmr& t1) {
    if (0 < t1.m_ms)
      sprintf(m_text, "%llu pct", (100 * m_ms) / t1.m_ms);
    else
      sprintf(m_text, "[cannot measure]");
    return m_text;
  }
  const char* over(const Tmr& t1) {
    if (0 < t1.m_ms) {
      if (t1.m_ms <= m_ms)
        sprintf(m_text, "%llu pct", (100 * (m_ms - t1.m_ms)) / t1.m_ms);
      else
        sprintf(m_text, "-%llu pct", (100 * (t1.m_ms - m_ms)) / t1.m_ms);
    } else
      sprintf(m_text, "[cannot measure]");
    return m_text;
  }
  Uint64 m_on;
  Uint64 m_ms;
  unsigned m_cnt;
  char m_time[100];
  char m_text[100];
};

static int
testperf()
{
  if (! testcase('p'))
    return 0;
  DBG("=== perf test ===");
  g_bh1 = g_bh2 = 0;
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  CHK(g_ndb->init() == 0);
  CHK(g_ndb->waitUntilReady() == 0);
  g_dic = g_ndb->getDictionary();
  NdbDictionary::Table tab(g_opt.m_tnameperf);
  if (g_dic->getTable(tab.getName()) != 0)
    CHK(g_dic->dropTable(tab.getName()) == 0);
  // col A - pk
  { NdbDictionary::Column col("A");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // col B - char 20
  { NdbDictionary::Column col("B");
    col.setType(NdbDictionary::Column::Char);
    col.setLength(20);
    col.setNullable(true);
    tab.addColumn(col);
  }
  // col C - text
  { NdbDictionary::Column col("C");
    col.setType(NdbDictionary::Column::Text);
    col.setBlobVersion(g_opt.m_blob_version);
    col.setInlineSize(20);
    col.setPartSize(512);
    col.setStripeSize(1);
    col.setNullable(true);
    tab.addColumn(col);
  }
  // create
  CHK(g_dic->createTable(tab) == 0);
  Uint32 cA = 0, cB = 1, cC = 2;
  // timers
  Tmr t1;
  Tmr t2;
  // insert char (one trans)
  {
    DBG("--- insert char ---");
    char b[20];
    t1.on();
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->insertTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      memset(b, 0x20, sizeof(b));
      b[0] = 'b';
      CHK(g_opr->setValue(cB, b) == 0);
      CHK(g_con->execute(NoCommit) == 0);
    }
    t1.off(g_opt.m_rowsperf);
    CHK(g_con->execute(Rollback) == 0);
    DBG(t1.time());
    g_opr = 0;
    g_ndb->closeTransaction(g_con);
    g_con = 0;
  }
  // insert text (one trans)
  {
    DBG("--- insert text ---");
    t2.on();
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->insertTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      CHK((g_bh1 = g_opr->getBlobHandle(cC)) != 0);
      CHK((g_bh1->setValue("c", 1) == 0));
      CHK(g_con->execute(NoCommit) == 0);
    }
    t2.off(g_opt.m_rowsperf);
    CHK(g_con->execute(Rollback) == 0);
    DBG(t2.time());
    g_bh1 = 0;
    g_opr = 0;
    g_ndb->closeTransaction(g_con);
    g_con = 0;
  }
  // insert overhead
  DBG("insert overhead: " << t2.over(t1));
  t1.clr();
  t2.clr();
  // insert
  {
    DBG("--- insert for read test ---");
    unsigned n = 0;
    char b[20];
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->insertTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      memset(b, 0x20, sizeof(b));
      b[0] = 'b';
      CHK(g_opr->setValue(cB, b) == 0);
      CHK((g_bh1 = g_opr->getBlobHandle(cC)) != 0);
      CHK((g_bh1->setValue("c", 1) == 0));
      if (++n == g_opt.m_batch) {
        CHK(g_con->execute(Commit) == 0);
        g_ndb->closeTransaction(g_con);
        CHK((g_con = g_ndb->startTransaction()) != 0);
        n = 0;
      }
    }
    if (n != 0) {
      CHK(g_con->execute(Commit) == 0);
      g_ndb->closeTransaction(g_con); g_con = 0;
      n = 0;
    }
    g_bh1 = 0;
    g_opr = 0;
  }
  // pk read char (one trans)
  {
    DBG("--- pk read char ---");
    CHK((g_con = g_ndb->startTransaction()) != 0);
    Uint32 a;
    char b[20];
    t1.on();
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->readTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      CHK(g_opr->getValue(cA, (char*)&a) != 0);
      CHK(g_opr->getValue(cB, b) != 0);
      a = (Uint32)-1;
      b[0] = 0;
      CHK(g_con->execute(NoCommit) == 0);
      CHK(a == k && b[0] == 'b');
    }
    CHK(g_con->execute(Commit) == 0);
    t1.off(g_opt.m_rowsperf);
    DBG(t1.time());
    g_opr = 0;
    g_ndb->closeTransaction(g_con); g_con = 0;
  }
  // pk read text (one trans)
  {
    DBG("--- pk read text ---");
    CHK((g_con = g_ndb->startTransaction()) != 0);
    Uint32 a;
    char c[20];
    t2.on();
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->readTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      CHK(g_opr->getValue(cA, (char*)&a) != 0);
      CHK((g_bh1 = g_opr->getBlobHandle(cC)) != 0);
      a = (Uint32)-1;
      c[0] = 0;
      CHK(g_con->execute(NoCommit) == 0);
      Uint32 m = 20;
      CHK(g_bh1->readData(c, m) == 0);
      CHK(a == k && m == 1 && c[0] == 'c');
    }
    CHK(g_con->execute(Commit) == 0);
    t2.off(g_opt.m_rowsperf);
    DBG(t2.time());
    g_ndb->closeTransaction(g_con); g_opr = 0;
    g_con = 0;
  }
  // pk read overhead
  DBG("pk read overhead: " << t2.over(t1));
  t1.clr();
  t2.clr();
  // scan read char
  const uint scan_loops = 10;
  {
    DBG("--- scan read char ---");
    Uint32 a;
    char b[20];
    uint i;
    for (i = 0; i < scan_loops; i++) {
      CHK((g_con = g_ndb->startTransaction()) != 0);
      CHK((g_ops = g_con->getNdbScanOperation(tab.getName())) != 0);
      CHK(g_ops->readTuples(NdbOperation::LM_Read) == 0);
      CHK(g_ops->getValue(cA, (char*)&a) != 0);
      CHK(g_ops->getValue(cB, b) != 0);
      CHK(g_con->execute(NoCommit) == 0);
      unsigned n = 0;
      t1.on();
      while (1) {
        a = (Uint32)-1;
        b[0] = 0;
        int ret;
        CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
        if (ret == 1)
          break;
        CHK(a < g_opt.m_rowsperf && b[0] == 'b');
        n++;
      }
      CHK(n == g_opt.m_rowsperf);
      t1.off(g_opt.m_rowsperf);
      g_ndb->closeTransaction(g_con); g_ops = 0;
      g_con = 0;
    }
    DBG(t1.time());
  }
  // scan read text
  {
    DBG("--- read text ---");
    Uint32 a;
    char c[20];
    uint i;
    for (i = 0; i < scan_loops; i++) {
      CHK((g_con = g_ndb->startTransaction()) != 0);
      CHK((g_ops = g_con->getNdbScanOperation(tab.getName())) != 0);
      CHK(g_ops->readTuples(NdbOperation::LM_Read) == 0);
      CHK(g_ops->getValue(cA, (char*)&a) != 0);
      CHK((g_bh1 = g_ops->getBlobHandle(cC)) != 0);
      CHK(g_con->execute(NoCommit) == 0);
      unsigned n = 0;
      t2.on();
      while (1) {
        a = (Uint32)-1;
        c[0] = 0;
        int ret;
        CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
        if (ret == 1)
          break;
        Uint32 m = 20;
        CHK(g_bh1->readData(c, m) == 0);
        CHK(a < g_opt.m_rowsperf && m == 1 && c[0] == 'c');
        n++;
      }
      CHK(n == g_opt.m_rowsperf);
      t2.off(g_opt.m_rowsperf);
      g_bh1 = 0;
      g_ops = 0;
      g_ndb->closeTransaction(g_con); g_con = 0;
    }
    DBG(t2.time());
  }
  // scan read overhead
  DBG("scan read overhead: " << t2.over(t1));
  t1.clr();
  t2.clr();
  if (g_opt.m_nodrop == false)
  {
    g_dic->dropTable(tab.getName());
  }
  delete g_ndb;
  g_ndb = 0;
  return 0;
}

// bug tests

static int
bugtest_4088()
{
  unsigned i;
  DBG("bug test 4088 - ndb api hang with mixed ops on index table");
  // insert rows
  calcTups(true);
  CHK(insertPk(0, API_NDBRECORD) == 0);
  // new trans
  CHK((g_con = g_ndb->startTransaction()) != 0);
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    // read table pk via index as a table
    const unsigned pkcnt = 2;
    Tup pktup[pkcnt];
    for (i = 0; i < pkcnt; i++) {
      char name[20];
      // XXX guess table id
      sprintf(name, "%d/%s", 4, g_opt.m_x1name);
      CHK((g_opr = g_con->getNdbOperation(name)) != 0);
      CHK(g_opr->readTuple() == 0);
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
      setUDpartId(tup, g_opr);
      CHK(g_opr->getValue("NDB$PK", (char*)&pktup[i].m_pk1) != 0);
    }
    // read blob inline via index as an index
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->readTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    require(tup.m_bval1.m_buf != 0);
    CHK(g_opx->getValue("BL1", (char*)tup.m_bval1.m_buf) != 0);
    // execute
    // BUG 4088: gets 1 tckeyconf, 1 tcindxconf, then hangs
    CHK(g_con->execute(Commit) == 0);
    // verify
    for (i = 0; i < pkcnt; i++) {
      CHK(pktup[i].m_pk1 == tup.m_pk1);
      CHK(memcmp(pktup[i].m_pk2, tup.m_pk2, g_opt.m_pk2chr.m_len) == 0);
    }
    CHK(memcmp(tup.m_bval1.m_val, tup.m_bval1.m_buf, 8 + g_blob1.m_inline) == 0);
  }
  return 0;
}

static int
bugtest_27018()
{
  DBG("bug test 27018 - middle partial part write clobbers rest of part");

  // insert rows
  calcTups(true);
  CHK(insertPk(0, API_NDBRECORD) == 0);
  // new trans
  for (unsigned k= 0; k < g_opt.m_rows; k++)
  {
    Tup& tup= g_tups[k];

    /* Update one byte in random position. */
    Uint32 offset= urandom(tup.m_bval1.m_len + 1);
    if (offset == tup.m_bval1.m_len) {
      // testing write at end is another problem..
      continue;
    }
    //DBG("len=" << tup.m_bval1.m_len << " offset=" << offset);

    CHK((g_con= g_ndb->startTransaction()) != 0);
    memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(&tup.m_key_row[g_pk2_offset], tup.m_pk2, g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    }
    NdbOperation::OperationOptions opts;
    setUDpartIdNdbRecord(tup,
                         g_ndb->getDictionary()->getTable(g_opt.m_tname),
                         opts);
    CHK((g_const_opr= g_con->updateTuple(g_key_record, tup.m_key_row,
                                         g_blob_record, tup.m_row,
                                         NULL, // mask
                                         &opts,
                                         sizeof(opts))) != 0);
    CHK(getBlobHandles(g_const_opr) == 0);
    CHK(g_con->execute(NoCommit) == 0);

    tup.m_bval1.m_buf[0]= 0xff ^ tup.m_bval1.m_val[offset];
    CHK(g_bh1->setPos(offset) == 0);
    CHK(g_bh1->writeData(&(tup.m_bval1.m_buf[0]), 1) == 0);
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);

    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_const_opr= g_con->readTuple(g_key_record, tup.m_key_row,
                                       g_blob_record, tup.m_row,
                                       NdbOperation::LM_Read,
                                       NULL, // mask
                                       &opts,
                                       sizeof(opts))) != 0);
    CHK(getBlobHandles(g_const_opr) == 0);

    CHK(g_bh1->getValue(tup.m_bval1.m_buf, tup.m_bval1.m_len) == 0);
    CHK(g_con->execute(Commit) == 0);

    Uint64 len= ~0;
    CHK(g_bh1->getLength(len) == 0 && len == tup.m_bval1.m_len);
    tup.m_bval1.m_buf[offset]^= 0xff;
    //CHK(memcmp(tup.m_bval1.m_buf, tup.m_bval1.m_val, tup.m_bval1.m_len) == 0);
    Uint32 i = 0;
    while (i < tup.m_bval1.m_len) {
      CHK(tup.m_bval1.m_buf[i] == tup.m_bval1.m_val[i]);
      i++;
    }

    g_ndb->closeTransaction(g_con);
    g_con=0;
    g_const_opr=0;
  }
  CHK(deletePk(API_NDBRECORD) == 0);

  return 0;
}


struct bug27370_data {
  bug27370_data() :
    m_ndb(NULL), m_writebuf(NULL), m_key_row(NULL)
  {
  }

  ~bug27370_data()
  {
    delete m_ndb;
    delete [] m_writebuf;
    delete [] m_key_row;
  }

  Ndb *m_ndb;
  char m_current_write_value;
  char *m_writebuf;
  Uint32 m_blob1_size;
  char *m_key_row;
  char *m_read_row;
  char *m_write_row;
  bool m_thread_stop;
  NdbOperation::OperationOptions* opts;
};

void *bugtest_27370_thread(void *arg)
{
  bug27370_data *data= (bug27370_data *)arg;

  while (!data->m_thread_stop)
  {
    memset(data->m_writebuf, data->m_current_write_value, data->m_blob1_size);
    data->m_current_write_value++;

    NdbConnection *con;
    if ((con= data->m_ndb->startTransaction()) == 0)
      return (void *)"Failed to create transaction";
    const NdbOperation *opr;
    memcpy(data->m_write_row, data->m_key_row, g_rowsize);
    if ((opr= con->writeTuple(g_key_record, data->m_key_row,
                              g_full_record, data->m_write_row,
                              NULL, //mask
                              data->opts,
                              sizeof(NdbOperation::OperationOptions))) == 0)
      return (void *)"Failed to create operation";
    NdbBlob *bh;
    if ((bh= opr->getBlobHandle("BL1")) == 0)
      return (void *)"getBlobHandle() failed";
    if (bh->setValue(data->m_writebuf, data->m_blob1_size) != 0)
      return (void *)"setValue() failed";
    if (con->execute(Commit, AbortOnError, 1) != 0)
      return (void *)"execute() failed";
    data->m_ndb->closeTransaction(con);
  }

  return NULL;                                  // Success
}

static int
bugtest_27370()
{
  DBG("bug test 27370 - Potential inconsistent blob reads for ReadCommitted reads");

  bug27370_data data;

  CHK((data.m_key_row= new char[g_rowsize*3]) != 0);
  data.m_read_row= data.m_key_row + g_rowsize;
  data.m_write_row= data.m_read_row + g_rowsize;

  data.m_ndb= new Ndb(g_ncc, "TEST_DB");
  CHK(data.m_ndb->init(20) == 0);
  CHK(data.m_ndb->waitUntilReady() == 0);

  data.m_current_write_value= 0;
  data.m_blob1_size= g_blob1.m_inline + 10 * g_blob1.m_partsize;
  CHK((data.m_writebuf= new char [data.m_blob1_size]) != 0);
  Uint32 pk1_value= 27370;

  const NdbDictionary::Table* t= g_ndb->getDictionary()->getTable(g_opt.m_tname);
  bool isUserDefined= (t->getFragmentType() == NdbDictionary::Object::UserDefined); 
  Uint32 partCount= t->getFragmentCount();
  Uint32 udPartId= pk1_value % partCount;
  NdbOperation::OperationOptions opts;
  opts.optionsPresent= 0;
  data.opts= &opts;
  if (isUserDefined)
  {
    opts.optionsPresent= NdbOperation::OperationOptions::OO_PARTITION_ID;
    opts.partitionId= udPartId;
  }
  memcpy(&data.m_key_row[g_pk1_offset], &pk1_value, sizeof(pk1_value));
  if (g_opt.m_pk2chr.m_len != 0)
  {
    memset(&data.m_key_row[g_pk2_offset], 'x', g_opt.m_pk2chr.m_totlen);
    if (!g_opt.m_pk2chr.m_fixed)
      data.m_key_row[g_pk2_offset]= urandom(g_opt.m_pk2chr.m_len + 1);
    Uint16 pk3_value= 27370;
    memcpy(&data.m_key_row[g_pk3_offset], &pk3_value, sizeof(pk3_value));
  }
  data.m_thread_stop= false;

  memset(data.m_writebuf, data.m_current_write_value, data.m_blob1_size);
  data.m_current_write_value++;

  CHK((g_con= g_ndb->startTransaction()) != 0);
  memcpy(data.m_write_row, data.m_key_row, g_rowsize);
  CHK((g_const_opr= g_con->writeTuple(g_key_record, data.m_key_row,
                                      g_full_record, data.m_write_row,
                                      NULL, // mask
                                      &opts,
                                      sizeof(opts))) != 0);
  CHK((g_bh1= g_const_opr->getBlobHandle("BL1")) != 0);
  CHK(g_bh1->setValue(data.m_writebuf, data.m_blob1_size) == 0);
  CHK(g_con->execute(Commit) == 0);
  g_ndb->closeTransaction(g_con);
  g_con= NULL;

  my_thread_handle thread_handle;
  CHK(my_thread_create(&thread_handle, NULL, bugtest_27370_thread, &data) == 0);

  DBG("bug test 27370 - PK blob reads");
  Uint32 seen_updates= 0;
  while (seen_updates < 50)
  {
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_const_opr= g_con->readTuple(g_key_record, data.m_key_row,
                                       g_blob_record, data.m_read_row,
                                       NdbOperation::LM_CommittedRead,
                                       NULL, // mask
                                       &opts,
                                       sizeof(opts))) != 0);
    CHK((g_bh1= g_const_opr->getBlobHandle("BL1")) != 0);
    CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);

    const Uint32 loop_max= 10;
    char read_char = 0;
    char original_read_char= 0;
    Uint32 readloop;
    for (readloop= 0;; readloop++)
    {
      if (readloop > 0)
      {
        if (readloop > 1)
        {
          /* Compare against first read. */
          CHK(read_char == original_read_char);
        }
        else
        {
          /*
            We count the number of times we see the other thread had the
            chance to update, so that we can be sure it had the opportunity
            to run a reasonable number of times before we stop.
          */
          if (original_read_char != read_char)
            seen_updates++;
          original_read_char= read_char;
        }
      }
      if (readloop > loop_max)
        break;
      Uint32 readSize= 1;
      CHK(g_bh1->setPos(urandom(data.m_blob1_size)) == 0);
      CHK(g_bh1->readData(&read_char, readSize) == 0);
      CHK(readSize == 1);
      ExecType commitType= readloop == loop_max ? Commit : NoCommit;
      CHK(g_con->execute(commitType, AbortOnError, 1) == 0);
    }
    g_ndb->closeTransaction(g_con);
    g_con= NULL;
  }

  DBG("bug test 27370 - table scan blob reads");
  seen_updates= 0;
  while (seen_updates < 50)
  {
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_ops= g_con->scanTable(g_full_record,
                                 NdbOperation::LM_CommittedRead)) != 0);
    CHK((g_bh1= g_ops->getBlobHandle("BL1")) != 0);
    CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);
    const char *out_row= NULL;
    CHK(g_ops->nextResult(&out_row, true, false) == 0);

    const Uint32 loop_max= 10;
    char read_char = 0;
    char original_read_char= 0;
    Uint32 readloop;
    for (readloop= 0;; readloop++)
    {
      if (readloop > 0)
      {
        if (readloop > 1)
        {
          /* Compare against first read. */
          CHK(read_char == original_read_char);
        }
        else
        {
          /*
            We count the number of times we see the other thread had the
            chance to update, so that we can be sure it had the opportunity
            to run a reasonable number of times before we stop.
          */
          if (original_read_char != read_char)
            seen_updates++;
          original_read_char= read_char;
        }
      }
      if (readloop > loop_max)
        break;
      Uint32 readSize= 1;
      CHK(g_bh1->setPos(urandom(data.m_blob1_size)) == 0);
      CHK(g_bh1->readData(&read_char, readSize) == 0);
      CHK(readSize == 1);
      CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);
    }

    CHK(g_ops->nextResult(&out_row, true, false) == 1);
    g_ndb->closeTransaction(g_con);
    g_con= NULL;
  }

  data.m_thread_stop= true;
  void *thread_return;
  CHK(my_thread_join(&thread_handle, &thread_return) == 0);
  DBG("bug 27370 - thread return status: " <<
      (thread_return ? (char *)thread_return : "<null>"));
  CHK(thread_return == 0);

  g_con= NULL;
  g_const_opr= NULL;
  g_bh1= NULL;
  return 0;
}

static int
bugtest_28116()
{
  DBG("bug test 28116 - Crash in getBlobHandle() when called without full key");

  if (g_opt.m_pk2chr.m_len == 0)
  {
    DBG("  ... skipped, requires multi-column primary key.");
    return 0;
  }

  calcTups(true);

  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    int reqType = urandom(4);
    switch(reqType) {
    case 0:
    {
      DBG("Read");
      CHK(g_opr->readTuple() == 0);
      break;
    }
    case 1:
    {
      DBG("Insert");
      CHK(g_opr->insertTuple() == 0);
      break;
    }
    case 2:
    {
      DBG("Update");
      CHK(g_opr->updateTuple() == 0);
      break;
    }
    case 3:
    default:
    {
      DBG("Delete");
      CHK(g_opr->deleteTuple() == 0);
      break;
    }
    }
    switch (urandom(3)) {
    case 0:
    {
      DBG("  No keys");
      break;
    }
    case 1:
    {
      DBG("  Pk1 only");
      CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
      break;
    }
    case 2:
    default:
    {
      DBG("  Pk2/3 only");
      if (g_opt.m_pk2chr.m_len != 0)
      {
        CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
        CHK(g_opr->equal("PK3", tup.m_pk3) == 0);
      }
      break;
    }
    }
    /* Deliberately no equal() on rest of primary key, to provoke error. */
    CHK(g_opr->getBlobHandle("BL1") == 0);

    /* 4264 - Invalid usage of Blob attribute */
    CHK(g_con->getNdbError().code == 4264);
    CHK(g_opr->getNdbError().code == 4264);

    g_ndb->closeTransaction(g_con);
    g_opr = 0;
    g_con = 0;
  }
  return 0;
}

static struct {
  int m_bug;
  int (*m_test)();
} g_bugtest[] = {
  { 4088, bugtest_4088 },
  { 27018, bugtest_27018 },
  { 27370, bugtest_27370 },
  { 36756, bugtest_36756 },
  { 45768, bugtest_45768 },
  { 48040, bugtest_48040 },
  { 28116, bugtest_28116 },
  { 62321, bugtest_62321 },
  { 28746560, bugtest_28746560 }
};

int main(int argc, char** argv)
{
  ndb_init();
  // log the invocation
  char cmdline[512];
  {
    const char* progname =
      strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
    strcpy(cmdline, progname);
    for (int i = 1; i < argc; i++) {
      strcat(cmdline, " ");
      strcat(cmdline, argv[i]);
    }
  }
  Chr& pk2chr = g_opt.m_pk2chr;
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
    if (strcmp(arg, "-batch") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_batch = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-core") == 0) {
      g_opt.m_core = true;
      continue;
    }
    if (strcmp(arg, "-dbg") == 0) {
      g_opt.m_dbg = true;
      continue;
    }
    if (strcmp(arg, "-debug") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_dbg = true;
        g_opt.m_debug = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-fac") == 0) {
      g_opt.m_fac = true;
      continue;
    }
    if (strcmp(arg, "-full") == 0) {
      g_opt.m_full = true;
      continue;
    }
    if (strcmp(arg, "-loop") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_loop = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-min") == 0) {
      g_opt.m_min = true;
      continue;
    }
    if (strcmp(arg, "-parts") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_parts = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-rows") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_rows = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-rowsperf") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_rowsperf = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-seed") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_seed = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-skip") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_skip = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-test") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_test = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-timeoutretries") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_timeout_retries = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-version") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_blob_version = atoi(argv[0]);
        if (g_opt.m_blob_version == 1 || g_opt.m_blob_version == 2)
          continue;
      }
    }
    // metadata
    if (strcmp(arg, "-pk2len") == 0) {
      if (++argv, --argc > 0) {
	pk2chr.m_len = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-pk2fixed") == 0) {
      pk2chr.m_fixed = true;
      continue;
    }
    if (strcmp(arg, "-pk2binary") == 0) {
      pk2chr.m_binary = true;
      continue;
    }
    if (strcmp(arg, "-pk2cs") == 0) {
      if (++argv, --argc > 0) {
        pk2chr.m_cs = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-pk2part") == 0) {
      g_opt.m_pk2part = true;
      continue;
    }
    if (strcmp(arg, "-oneblob") == 0) {
      g_opt.m_oneblob = true;
      continue;
    }
    if (strcmp(arg, "-rbatch") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_rbatch = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-wbatch") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_wbatch = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-nodrop") == 0) {
      g_opt.m_nodrop = 1;
      continue;
    }

    // bugs
    if (strcmp(arg, "-bug") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_bug = atoi(argv[0]);
        for (unsigned i = 0; i < sizeof(g_bugtest)/sizeof(g_bugtest[0]); i++) {
          if (g_opt.m_bug == g_bugtest[i].m_bug) {
            g_opt.m_bugtest = g_bugtest[i].m_test;
            break;
          }
        }
        if (g_opt.m_bugtest != 0)
          continue;
      }
    }
    if (strcmp(arg, "-?") == 0 || strcmp(arg, "-h") == 0) {
      printusage();
      goto success;
    }
    ndbout << "unknown option " << arg << endl;
    goto wrongargs;
  }
  if (g_opt.m_debug != 0) {
    if (strchr(g_opt.m_debug, ':') == 0) {
      const char* s = "d:t:F:L:o,";
      char* t = new char [strlen(s) + strlen(g_opt.m_debug) + 1];
      strcpy(t, s);
      strcat(t, g_opt.m_debug);
      g_opt.m_debug = t;
    }
    DBUG_PUSH(g_opt.m_debug);
    ndbout.m_out = new FileOutputStream(DBUG_FILE);
  }
  if (pk2chr.m_len == 0) {
    char b[100];
    b[0] = 0;
    if (g_opt.m_skip != 0)
      strcpy(b, g_opt.m_skip);
    strcat(b, "i");
    strcat(b, "r");
    g_opt.m_skip = strdup(b);
  }
  if (pk2chr.m_len != 0) {
    Chr& c = pk2chr;
    if (c.m_binary) {
      if (c.m_fixed)
        c.m_type = NdbDictionary::Column::Binary;
      else
        c.m_type = NdbDictionary::Column::Varbinary;
      c.m_mblen = 1;
      c.m_cs = 0;
    } else {
      require(c.m_cs != 0);
      if (c.m_fixed)
        c.m_type = NdbDictionary::Column::Char;
      else
        c.m_type = NdbDictionary::Column::Varchar;
      c.m_csinfo = get_charset_by_name(c.m_cs, MYF(0));
      if (c.m_csinfo == 0)
        c.m_csinfo = get_charset_by_csname(c.m_cs, MY_CS_PRIMARY, MYF(0));
      if (c.m_csinfo == 0) {
        ndbout << "unknown charset " << c.m_cs << endl;
        goto wrongargs;
      }
      c.m_mblen = c.m_csinfo->mbmaxlen;
      if (c.m_mblen == 0)
        c.m_mblen = 1;
    }
    c.m_bytelen = c.m_len * c.m_mblen;
    if (c.m_bytelen > 255) {
      ndbout << "length of pk2 in bytes exceeds 255" << endl;
      goto wrongargs;
    }
    if (c.m_fixed)
      c.m_totlen = c.m_bytelen;
    else
      c.m_totlen = 1 + c.m_bytelen;
    c.m_caseins = false;
    if (c.m_cs != 0) {
      CHARSET_INFO* info = c.m_csinfo;
      const char* p = "ABCxyz";
      const char* q = "abcXYZ";
      int e;
      if ((*info->cset->well_formed_len)(info, p, p + 6, 999, &e) != 6) {
        ndbout << "charset does not contain ascii" << endl;
        goto wrongargs;
      }
      if ((*info->coll->strcasecmp)(info, p, q) == 0) {
        c.m_caseins = true;
      }
      ndbout << "charset: " << c.m_cs << " caseins: " << c.m_caseins << endl;
    }
  }
  ndbout << cmdline << endl;
  g_ncc = new Ndb_cluster_connection();
  if (g_ncc->connect(30) != 0 || testmain() == -1 || testperf() == -1) {
    ndbout << "line " << __LINE__ << " FAIL loop=" << g_loop << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  delete g_ncc;
  g_ncc = 0;
success:
  ndb_end(0);
  return NDBT_ProgramExit(NDBT_OK);
wrongargs:
  ndb_end(0);
  return NDBT_ProgramExit(NDBT_WRONGARGS);
}

// vim: set sw=2 et:
