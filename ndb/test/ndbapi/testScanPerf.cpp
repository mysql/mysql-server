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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <random.h>
#include <getarg.h>

struct Parameter {
  char * name;
  unsigned value;
  unsigned min;
  unsigned max; 
};

#define P_BATCH   0
#define P_PARRA   1
#define P_LOCK    2
#define P_FILT    3
#define P_BOUND   4
#define P_ACCESS  5
#define P_FETCH   6
#define P_ROWS    7
#define P_LOOPS   8
#define P_CREATE  9
#define P_LOAD   10
#define P_RESET  11

#define P_MAX 12

static 
Parameter 
g_paramters[] = {
  { "batch",       0, 0, 1 }, // 0, 15
  { "parallelism", 0, 0, 1 }, // 0,  1
  { "lock",        0, 0, 2 }, // read, exclusive, dirty
  { "filter",      0, 0, 3 }, // all, none, 1, 100
  { "range",       0, 0, 3 }, // all, none, 1, 100
  { "access",      0, 0, 2 }, // scan, idx, idx sorted
  { "fetch",       0, 0, 1 }, // No, yes
  { "size",  1000000, 1, ~0 },
  { "iterations",  3, 1, ~0 },
  { "create_drop", 1, 0, 1 },
  { "data",        1, 0, 1 },
  { "q-reset bounds", 0, 1, 0 }
};

static Ndb* g_ndb = 0;
static const NdbDictionary::Table * g_table;
static const NdbDictionary::Index * g_index;
static char g_tablename[256];
static char g_indexname[256];

int create_table();
int load_table();
int run_scan();
int clear_table();
int drop_table();

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

  g_ndb = new Ndb("TEST_DB");
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
    BaseString::snprintf(g_tablename, sizeof(g_tablename), T);
    BaseString::snprintf(g_indexname, sizeof(g_indexname), "IDX_%s", T);
    if(create_table())
      goto error;
    if(load_table())
      goto error;
    if(run_scan())
      goto error;
    if(clear_table())
      goto error;
    if(drop_table())
      goto error;
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
    const NdbDictionary::Table * pTab = NDBT_Tables::getTable(g_tablename);
    assert(pTab);
    NdbDictionary::Table copy = * pTab;
    copy.setLogging(false);
    if(dict->createTable(copy) != 0){
      g_err << "Failed to create table: " << g_tablename << endl;
      return -1;
    }

    NdbDictionary::Index x(g_indexname);
    x.setTable(g_tablename);
    x.setType(NdbDictionary::Index::OrderedIndex);
    x.setLogging(false);
    for (unsigned k = 0; k < copy.getNoOfColumns(); k++){
      if(copy.getColumn(k)->getPrimaryKey()){
	x.addColumnName(copy.getColumn(k)->getName());
      }
    }

    if(dict->createIndex(x) != 0){
      g_err << "Failed to create index: " << endl;
      return -1;
    }
  }
  g_table = dict->getTable(g_tablename);
  g_index = dict->getIndex(g_indexname, g_tablename);
  assert(g_table);
  assert(g_index);
  return 0;
}

int
drop_table(){
  if(!g_paramters[P_CREATE].value)
    return 0;
  if(g_ndb->getDictionary()->dropTable(g_table->getName()) != 0){
    g_err << "Failed to drop table: " << g_table->getName() << endl;
    return -1;
  }
  g_table = 0;
  return 0;
}

int
load_table(){
  if(!g_paramters[P_LOAD].value)
    return 0;
  
  int rows = g_paramters[P_ROWS].value;
  HugoTransactions hugoTrans(* g_table);
  if (hugoTrans.loadTable(g_ndb, rows)){
    g_err.println("Failed to load %s with %d rows", g_table->getName(), rows);
    return -1;
  }
  return 0;
}

int
clear_table(){
  if(!g_paramters[P_LOAD].value)
    return 0;
  int rows = g_paramters[P_ROWS].value;
  
  UtilTransactions utilTrans(* g_table);
  if (utilTrans.clearTable(g_ndb,  rows) != 0){
    g_err.println("Failed to clear table %s", g_table->getName());
    return -1;
  }
  return 0;
}

inline 
void err(NdbError e){
  ndbout << e << endl;
}

int
run_scan(){
  int iter = g_paramters[P_LOOPS].value;
  NDB_TICKS start1, stop;
  int sum_time= 0;

  int sample_rows = 0;
  NDB_TICKS sample_start = NdbTick_CurrentMillisecond();

  Uint32 tot = g_paramters[P_ROWS].value;

  if(g_paramters[P_BOUND].value == 2 || g_paramters[P_FILT].value == 2)
    iter *= g_paramters[P_ROWS].value;

  NdbScanOperation * pOp = 0;
  NdbIndexScanOperation * pIOp = 0;
  NdbConnection * pTrans = 0;
  NdbResultSet * rs = 0;
  int check = 0;

  for(int i = 0; i<iter; i++){
    start1 = NdbTick_CurrentMillisecond();
    pTrans = pTrans ? pTrans : g_ndb->startTransaction();
    if(!pTrans){
      g_err << "Failed to start transaction" << endl;
      err(g_ndb->getNdbError());
      return -1;
    }
    
    int par = g_paramters[P_PARRA].value;
    int bat = g_paramters[P_BATCH].value;
    NdbScanOperation::LockMode lm;
    switch(g_paramters[P_LOCK].value){
    case 0:
      lm = NdbScanOperation::LM_CommittedRead;
      break;
    case 1:
      lm = NdbScanOperation::LM_Read;
      break;
    case 2:
      lm = NdbScanOperation::LM_Exclusive;
      break;
    default:
      abort();
    }

    if(g_paramters[P_ACCESS].value == 0){
      pOp = pTrans->getNdbScanOperation(g_tablename);
      assert(pOp);
      rs = pOp->readTuples(lm, bat, par);
    } else {
      if(g_paramters[P_RESET].value == 0 || pIOp == 0)
      {
	pOp= pIOp= pTrans->getNdbIndexScanOperation(g_indexname, g_tablename);
	bool ord = g_paramters[P_ACCESS].value == 2;
	rs = pIOp->readTuples(lm, bat, par, ord);
      }
      else
      {
	pIOp->reset_bounds();
      }

      switch(g_paramters[P_BOUND].value){
      case 0: // All
	break;
      case 1: // None
	pIOp->setBound((Uint32)0, NdbIndexScanOperation::BoundEQ, 0);
	break;
      case 2: { // 1 row
      default:  
	assert(g_table->getNoOfPrimaryKeys() == 1); // only impl. so far
	int tot = g_paramters[P_ROWS].value;
	int row = rand() % tot;
#if 0
	fix_eq_bound(pIOp, row);
#else
	pIOp->setBound((Uint32)0, NdbIndexScanOperation::BoundEQ, &row);
#endif
	break;
      }
      }
      if(g_paramters[P_RESET].value == 1)
	goto execute;
    }
    assert(pOp);
    assert(rs);
    
    switch(g_paramters[P_FILT].value){
    case 0: // All
      check = pOp->interpret_exit_ok();
      break;
    case 1: // None
      check = pOp->interpret_exit_nok();
      break;
    case 2: { // 1 row
    default:  
      assert(g_table->getNoOfPrimaryKeys() == 1); // only impl. so far
      abort();
#if 0
      int tot = g_paramters[P_ROWS].value;
      int row = rand() % tot;
      NdbScanFilter filter(pOp) ;   
      filter.begin(NdbScanFilter::AND);
      fix_eq(filter, pOp, row);
      filter.end();
      break;
#endif
    }
    }
    if(check != 0){
      err(pOp->getNdbError());
      return -1;
    }
    assert(check == 0);

    for(int i = 0; i<g_table->getNoOfColumns(); i++){
      pOp->getValue(i);
    }
execute:
    int rows = 0;
    check = pTrans->execute(NoCommit);
    assert(check == 0);
    int fetch = g_paramters[P_FETCH].value;
    while((check = rs->nextResult(true)) == 0){
      do {
	rows++;
      } while(!fetch && ((check = rs->nextResult(false)) == 0));
      if(check == -1){
        err(pTrans->getNdbError());
        return -1;
      }
      assert(check == 2);
    }

    if(check == -1){
      err(pTrans->getNdbError());
      return -1;
    }
    assert(check == 1);
    if(g_paramters[P_RESET].value == 0)
    {
      pTrans->close();
      pTrans = 0;
    }
    stop = NdbTick_CurrentMillisecond();
    
    int time_passed= (int)(stop - start1);
    sample_rows += rows;
    sum_time+= time_passed;
    
    if(sample_rows >= tot)
    {
      int sample_time = (int)(stop - sample_start);
      g_info << "Found " << sample_rows << " rows" << endl;
      g_err.println("Time: %d ms = %u rows/sec", sample_time,
		    (1000*sample_rows)/sample_time);
      sample_rows = 0;
      sample_start = stop;
    }
  }

  g_err.println("Avg time: %d ms = %u rows/sec", sum_time/iter,
                (1000*tot*iter)/sum_time);
  return 0;
}
