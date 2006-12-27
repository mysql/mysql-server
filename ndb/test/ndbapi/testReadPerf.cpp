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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <random.h>
#include <getarg.h>

struct Parameter {
  const char * name;
  unsigned value;
  unsigned min;
  unsigned max; 
};

#define P_OPER    0
#define P_RANGE   1
#define P_ROWS    2
#define P_LOOPS   3
#define P_CREATE  4
#define P_LOAD    5

#define P_MAX 6

/**
 * operation
 * 0 - serial pk
 * 1 - batch pk
 * 2 - serial uniq 
 * 3 - batch uniq
 * 4 - index eq
 * 5 - range scan
 * 6 - ordered range scan
 * 7 - interpreted scan
 */ 
static const char * g_ops[] = {
  "serial pk",
  "batch pk",
  "serial uniq index access",
  "batch uniq index access",
  "index eq-bound",
  "index range",
  "index ordered",
  "interpreted scan"
};

#define P_OP_TYPES 8
static Uint64 g_times[P_OP_TYPES];

static 
Parameter 
g_paramters[] = {
  { "operation",   0, 0, 6 }, // 0 
  { "range",    1000, 1, ~0 },// 1 no of rows to read
  { "size",  1000000, 1, ~0 },// 2 rows in tables
  { "iterations",  3, 1, ~0 },// 3
  { "create_drop", 0, 0, 1 }, // 4
  { "data",        0, 0, 1 }  // 5
};

static Ndb* g_ndb = 0;
static const NdbDictionary::Table * g_tab;
static const NdbDictionary::Index * g_i_unique;
static const NdbDictionary::Index * g_i_ordered;
static char g_table[256];
static char g_unique[256];
static char g_ordered[256];
static char g_buffer[2*1024*1024];

int create_table();
int load_table();
int run_read();
int clear_table();
int drop_table();
void print_result();

int
main(int argc, const char** argv){
  ndb_init();
  int verbose = 1;
  int optind = 0;
  
  struct getargs args[1+P_MAX] = {
    { "verbose", 'v', arg_flag, &verbose, "Print verbose status", "verbose" }
  };
  const int num_args = 1 + P_MAX;
  int i;
  for(i = 0; i<P_MAX; i++){
    args[i+1].long_name = g_paramters[i].name;
    args[i+1].short_name = * g_paramters[i].name;
    args[i+1].type = arg_integer;
    args[i+1].value = &g_paramters[i].value;
    BaseString tmp;
    tmp.assfmt("min: %d max: %d", g_paramters[i].min, g_paramters[i].max);
    args[i+1].help = strdup(tmp.c_str());
    args[i+1].arg_help = 0;
  }
  
  if(getarg(args, num_args, argc, argv, &optind)) {
    arg_printusage(args, num_args, argv[0], "tabname1 tabname2 ...");
    return NDBT_WRONGARGS;
  }
  
  myRandom48Init(NdbTick_CurrentMillisecond());
  memset(g_times, 0, sizeof(g_times));

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1))
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  g_ndb = new Ndb(&con, "TEST_DB");
  if(g_ndb->init() != 0){
    g_err << "init() failed" << endl;
    goto error;
  }
  if(g_ndb->waitUntilReady() != 0){
    g_err << "Wait until ready failed" << endl;
    goto error;
  }
  for(i = optind; i<argc; i++){
    const char * T = argv[i];
    g_info << "Testing " << T << endl;
    BaseString::snprintf(g_table, sizeof(g_table), T);
    BaseString::snprintf(g_ordered, sizeof(g_ordered), "IDX_O_%s", T);
    BaseString::snprintf(g_unique, sizeof(g_unique), "IDX_U_%s", T);
    if(create_table())
      goto error;
    if(load_table())
      goto error;
    for(int l = 0; l<g_paramters[P_LOOPS].value; l++){
      for(int j = 0; j<P_OP_TYPES; j++){
	g_paramters[P_OPER].value = j;
	if(run_read())
	  goto error;
      }
    }
    print_result();
  }
  
  if(g_ndb) delete g_ndb;
  return NDBT_OK;
error:
  if(g_ndb) delete g_ndb;
  return NDBT_FAILED;
}

int
create_table(){
  NdbDictionary::Dictionary* dict = g_ndb->getDictionary();
  assert(dict);
  if(g_paramters[P_CREATE].value){
    const NdbDictionary::Table * pTab = NDBT_Tables::getTable(g_table);
    assert(pTab);
    NdbDictionary::Table copy = * pTab;
    copy.setLogging(false);
    if(dict->createTable(copy) != 0){
      g_err << "Failed to create table: " << g_table << endl;
      return -1;
    }

    NdbDictionary::Index x(g_ordered);
    x.setTable(g_table);
    x.setType(NdbDictionary::Index::OrderedIndex);
    x.setLogging(false);
    for (unsigned k = 0; k < copy.getNoOfColumns(); k++){
      if(copy.getColumn(k)->getPrimaryKey()){
	x.addColumn(copy.getColumn(k)->getName());
      }
    }

    if(dict->createIndex(x) != 0){
      g_err << "Failed to create index: " << endl;
      return -1;
    }

    x.setName(g_unique);
    x.setType(NdbDictionary::Index::UniqueHashIndex);
    if(dict->createIndex(x) != 0){
      g_err << "Failed to create index: " << endl;
      return -1;
    }
  }
  g_tab = dict->getTable(g_table);
  g_i_unique = dict->getIndex(g_unique, g_table);
  g_i_ordered = dict->getIndex(g_ordered, g_table);
  assert(g_tab);
  assert(g_i_unique);
  assert(g_i_ordered);
  return 0;
}

int
drop_table(){
  if(!g_paramters[P_CREATE].value)
    return 0;
  if(g_ndb->getDictionary()->dropTable(g_tab->getName()) != 0){
    g_err << "Failed to drop table: " << g_tab->getName() << endl;
    return -1;
  }
  g_tab = 0;
  return 0;
}

int
load_table(){
  if(!g_paramters[P_LOAD].value)
    return 0;
  
  int rows = g_paramters[P_ROWS].value;
  HugoTransactions hugoTrans(* g_tab);
  if (hugoTrans.loadTable(g_ndb, rows)){
    g_err.println("Failed to load %s with %d rows", g_tab->getName(), rows);
    return -1;
  }
  return 0;
}

int
clear_table(){
  if(!g_paramters[P_LOAD].value)
    return 0;
  int rows = g_paramters[P_ROWS].value;
  
  UtilTransactions utilTrans(* g_tab);
  if (utilTrans.clearTable(g_ndb,  rows) != 0){
    g_err.println("Failed to clear table %s", g_tab->getName());
    return -1;
  }
  return 0;
}

inline 
void err(NdbError e){
  ndbout << e << endl;
}

int
run_read(){
  int iter = g_paramters[P_LOOPS].value;
  NDB_TICKS start1, stop;
  int sum_time= 0;
  
  const Uint32 rows = g_paramters[P_ROWS].value;
  const Uint32 range = g_paramters[P_RANGE].value;
  
  start1 = NdbTick_CurrentMillisecond();
  NdbConnection * pTrans = g_ndb->startTransaction();
  if(!pTrans){
    g_err << "Failed to start transaction" << endl;
    err(g_ndb->getNdbError());
    return -1;
  }
  
  NdbOperation * pOp;
  NdbScanOperation * pSp;
  NdbIndexOperation * pUp;
  NdbIndexScanOperation * pIp;
  
  Uint32 start_row = rand() % (rows - range);
  Uint32 stop_row = start_row + range;

  /**
   * 0 - serial pk
   * 1 - batch pk
   * 2 - serial uniq 
   * 3 - batch uniq
   * 4 - index eq
   * 5 - range scan
   * 6 - interpreted scan
   */
  int check = 0;
  void* res = (void*)~0;
  const Uint32 pk = 0;
  Uint32 cnt = 0;
  for(; start_row < stop_row; start_row++){
    switch(g_paramters[P_OPER].value){
    case 0:
      pOp = pTrans->getNdbOperation(g_table);
      check = pOp->readTuple();
      check = pOp->equal(pk, start_row);
      break;
    case 1:
      for(; start_row<stop_row; start_row++){
	pOp = pTrans->getNdbOperation(g_table);
	check = pOp->readTuple();
	check = pOp->equal(pk, start_row);
	for(int j = 0; j<g_tab->getNoOfColumns(); j++){
	  res = pOp->getValue(j);
	  assert(res);
	}
      }
      break;
    case 2:
      pOp = pTrans->getNdbIndexOperation(g_unique, g_table);
      check = pOp->readTuple();
      check = pOp->equal(pk, start_row);
      break;
    case 3:
      for(; start_row<stop_row; start_row++){
	pOp = pTrans->getNdbIndexOperation(g_unique, g_table);
	check = pOp->readTuple();
	check = pOp->equal(pk, start_row);
	for(int j = 0; j<g_tab->getNoOfColumns(); j++){
	  res = pOp->getValue(j);
	  assert(res);
	}
      }
      break;
    case 4:
      pOp = pSp = pIp = pTrans->getNdbIndexScanOperation(g_ordered,g_table);
      pIp->readTuples(NdbScanOperation::LM_CommittedRead, 0, 0);
      check = pIp->setBound(pk, NdbIndexScanOperation::BoundEQ, &start_row);
      break;
    case 5:
      pOp = pSp = pIp = pTrans->getNdbIndexScanOperation(g_ordered,g_table);
      pIp->readTuples(NdbScanOperation::LM_CommittedRead, 0, 0);
      check = pIp->setBound(pk, NdbIndexScanOperation::BoundLE, &start_row);
      check = pIp->setBound(pk, NdbIndexScanOperation::BoundGT, &stop_row);
      start_row = stop_row;
      break;
    case 6:
      pOp = pSp = pIp = pTrans->getNdbIndexScanOperation(g_ordered,g_table);
      pIp->readTuples(NdbScanOperation::LM_CommittedRead, 0, 0, true);
      check = pIp->setBound(pk, NdbIndexScanOperation::BoundLE, &start_row);
      check = pIp->setBound(pk, NdbIndexScanOperation::BoundGT, &stop_row);
      start_row = stop_row;
      break;
    case 7:
      pOp = pSp = pTrans->getNdbScanOperation(g_table);
      pSp->readTuples(NdbScanOperation::LM_CommittedRead, 0, 0);
      NdbScanFilter filter(pOp) ;   
      filter.begin(NdbScanFilter::AND);
      filter.ge(pk, start_row);
      filter.lt(pk, stop_row);
      filter.end();
      start_row = stop_row;
      break;
    }
      
    assert(res);
    if(check != 0){
      ndbout << pOp->getNdbError() << endl;
      ndbout << pTrans->getNdbError() << endl;
    }
    assert(check == 0);

    for(int j = 0; j<g_tab->getNoOfColumns(); j++){
      res = pOp->getValue(j);
      assert(res);
    }
      
    check = pTrans->execute(NoCommit);
    if(check != 0){
      ndbout << pTrans->getNdbError() << endl;
    }
    assert(check == 0);
    if(g_paramters[P_OPER].value >= 4){
      while((check = pSp->nextResult(true)) == 0){
	cnt++;
      }
	
      if(check == -1){
	err(pTrans->getNdbError());
	return -1;
      }
      assert(check == 1);
      pSp->close();
    }
  }
  assert(g_paramters[P_OPER].value < 4 || (cnt == range));
  
  pTrans->close();
  
  stop = NdbTick_CurrentMillisecond();
  g_times[g_paramters[P_OPER].value] += (stop - start1);
  return 0;
}

void
print_result(){
  int tmp = 1;
  tmp *= g_paramters[P_RANGE].value;
  tmp *= g_paramters[P_LOOPS].value;

  int t, t2;
  for(int i = 0; i<P_OP_TYPES; i++){
    g_err << g_ops[i] << " avg: "
	  << (int)((1000*g_times[i])/tmp)
	  << " us/row (" 
	  << (1000 * tmp)/g_times[i] << " rows / sec)" << endl;
  }
}
