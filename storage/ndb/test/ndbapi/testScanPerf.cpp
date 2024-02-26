/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <cstring>
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
#define P_MULTI  11

#define P_MAX 12

/* Note that this tool can only be run against Hugo tables with an integer
 * primary key
 */

static 
Parameter 
g_paramters[] = {
  { "batch",       0, 0, 1 }, // 0, 15
  { "parallelism", 0, 0, 1 }, // 0,  1
  { "lock",        0, 0, 2 }, // read, exclusive, dirty
  { "filter",      0, 0, 3 }, // Use ScanFilter to return : all, none, 1, 100
  { "range",       0, 0, 3 }, // Use IndexBounds to return : all, none, 1, 100
  // For range==3, Multiple index scans are used with a number of ranges specified
  // per scan (Number is defined by multi read range.
  { "access",      0, 0, 2 }, // Table, Index or Ordered Index scan
  { "fetch",       0, 0, 1 }, // nextResult fetchAllowed.  No, yes
  { "size",  1000000, 1, UINT_MAX }, // Num rows to operate on
  { "iterations",  3, 1, UINT_MAX }, // Num times to repeat tests
  { "create_drop", 1, 0, 2 }, // Whether to recreate the table
  { "data",        1, 0, 1 }, // Ignored currently
  { "multi read range", 1000, 1, UINT_MAX } // Number of ranges to use in MRR access (range=3)
};

static Ndb* g_ndb = 0;
static const NdbDictionary::Table * g_table;
static const NdbDictionary::Index * g_index;
static char g_tablename[256];
static char g_indexname[256];
static const NdbRecord * g_table_record;
static const NdbRecord * g_index_record;

int create_table();
int run_scan();

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

  myRandom48Init((long)NdbTick_CurrentMillisecond());

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
    BaseString::snprintf(g_tablename, sizeof(g_tablename), "%s", T);
    BaseString::snprintf(g_indexname, sizeof(g_indexname), "IDX_%s", T);
    if(create_table())
      goto error;
    if(g_paramters[P_CREATE].value != 2 && run_scan())
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
  require(dict);
  if(g_paramters[P_CREATE].value){
    g_ndb->getDictionary()->dropTable(g_tablename);
    const NdbDictionary::Table * pTab = NDBT_Tables::getTable(g_tablename);
    require(pTab);
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
    for (unsigned k = 0; k < (unsigned) copy.getNoOfColumns(); k++){
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
  require(g_table);
  require(g_index);

  /* Obtain NdbRecord instances for the table and index */
  {
    NdbDictionary::RecordSpecification spec[ NDB_MAX_ATTRIBUTES_IN_TABLE ];
    
    Uint32 offset=0;
    Uint32 cols= g_table->getNoOfColumns();
    for (Uint32 colNum=0; colNum<cols; colNum++)
    {
      const NdbDictionary::Column* col= g_table->getColumn(colNum);
      Uint32 colLength= col->getLength();
      
      spec[colNum].column= col;
      spec[colNum].offset= offset;

      offset+= colLength;

      spec[colNum].nullbit_byte_offset= offset++;
      spec[colNum].nullbit_bit_in_byte= 0;
    }
  
    g_table_record= dict->createRecord(g_table,
                                       &spec[0],
                                       cols,
                                       sizeof(NdbDictionary::RecordSpecification));

    require(g_table_record);
  }
  {
    NdbDictionary::RecordSpecification spec[ NDB_MAX_ATTRIBUTES_IN_TABLE ];
    
    Uint32 offset=0;
    Uint32 cols= g_index->getNoOfColumns();
    for (Uint32 colNum=0; colNum<cols; colNum++)
    {
      /* Get column from the underlying table */
      // TODO : Add this mechanism to dict->createRecord
      // TODO : Add NdbRecord queryability methods so that an NdbRecord can
      // be easily built and later used to read out data.
      const NdbDictionary::Column* col= 
        g_table->getColumn(g_index->getColumn(colNum)->getName());
      Uint32 colLength= col->getLength();
      
      spec[colNum].column= col;
      spec[colNum].offset= offset;

      offset+= colLength;

      spec[colNum].nullbit_byte_offset= offset++;
      spec[colNum].nullbit_bit_in_byte= 0;
    }
  
    g_index_record= dict->createRecord(g_index,
                                       &spec[0],
                                       cols,
                                       sizeof(NdbDictionary::RecordSpecification));

    require(g_index_record);
  }


  if(g_paramters[P_CREATE].value)
  {
    int rows = g_paramters[P_ROWS].value;
    HugoTransactions hugoTrans(* g_table);
    if (hugoTrans.loadTable(g_ndb, rows)){
      g_err.println("Failed to load %s with %d rows", 
		    g_table->getName(), rows);
      return -1;
    }
  }
  
  return 0;
}

inline 
void err(NdbError e){
  ndbout << e << endl;
}

int
setEqBound(NdbIndexScanOperation *isop,
           const NdbRecord *key_record,
           Uint32 value,
           Uint32 rangeNum)
{
  Uint32 space[2];
  space[0]= value;
  space[1]= 0; // Null bit set to zero.

  NdbIndexScanOperation::IndexBound ib;
  ib.low_key= ib.high_key= (char*) &space;
  ib.low_key_count= ib.high_key_count= 1;
  ib.low_inclusive= ib.high_inclusive= true;
  ib.range_no= rangeNum;

  return isop->setBound(key_record, ib);
}

int
run_scan(){
  int iter = g_paramters[P_LOOPS].value;
  Uint64 start1, stop;
  int sum_time= 0;

  Uint32 sample_rows = 0;
  int tot_rows = 0;
  Uint64 sample_start = NdbTick_CurrentMillisecond();

  Uint32 tot = g_paramters[P_ROWS].value;

  if(g_paramters[P_BOUND].value >= 2 || g_paramters[P_FILT].value == 2)
    iter *= g_paramters[P_ROWS].value;

  NdbScanOperation * pOp = 0;
  NdbIndexScanOperation * pIOp = 0;
  NdbConnection * pTrans = 0;
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

    NdbScanOperation::ScanOptions options;
    std::memset(&options, 0, sizeof(options));

    options.optionsPresent= 
      NdbScanOperation::ScanOptions::SO_SCANFLAGS |
      NdbScanOperation::ScanOptions::SO_PARALLEL |
      NdbScanOperation::ScanOptions::SO_BATCH;

    bool ord= g_paramters[P_ACCESS].value == 2;
    bool mrr= (g_paramters[P_ACCESS].value != 0) &&
      (g_paramters[P_BOUND].value == 3);

    options.scan_flags|= 
      ( ord ? NdbScanOperation::SF_OrderBy:0 ) |
      ( mrr ? NdbScanOperation::SF_MultiRange:0 );
    options.parallel= par;
    options.batch= bat;

    switch(g_paramters[P_FILT].value){
    case 0: // All
      break;
    case 1: // None
      break;
    case 2:  // 1 row
    default: {
      require(g_table->getNoOfPrimaryKeys() == 1); // only impl. so far
      abort();
#if 0
      int tot = g_paramters[P_ROWS].value;
      int row = rand() % tot;
      NdbInterpretedCode* ic= new NdbInterpretedCode(g_table);
      NdbScanFilter filter(ic);   
      filter.begin(NdbScanFilter::AND);
      filter.eq(0, row);
      filter.end();

      options.scan_flags|= NdbScanOperation::SF_Interpreted;
      options.interpretedCode= &ic;
      break;
#endif
    }
    }
    
    if(g_paramters[P_ACCESS].value == 0){
      pOp = pTrans->scanTable(g_table_record,
                              lm,
                              NULL, // Mask
                              &options,
                              sizeof(NdbScanOperation::ScanOptions));
      require(pOp);
    } else {
      pOp= pIOp= pTrans->scanIndex(g_index_record,
                                   g_table_record,
                                   lm,
                                   NULL, // Mask
                                   NULL, // First IndexBound
                                   &options,
                                   sizeof(NdbScanOperation::ScanOptions));
      if (pIOp == NULL)
      {
        err(pTrans->getNdbError());
        abort();
      }
        
      require(pIOp);

      switch(g_paramters[P_BOUND].value){
      case 0: // All
	break;
      case 1: // None
        check= setEqBound(pIOp, g_index_record, 0, 0);
        require(check == 0);
	break;
      case 2: { // 1 row
      default:  
	require(g_table->getNoOfPrimaryKeys() == 1); // only impl. so far
	int tot = g_paramters[P_ROWS].value;
	int row = rand() % tot;

        check= setEqBound(pIOp, g_index_record, row, 0);
        require(check == 0);
	break;
      }
      case 3: { // read multi
	int multi = g_paramters[P_MULTI].value;
	int tot = g_paramters[P_ROWS].value;
        int rangeStart= i;
        for(; multi > 0 && i < iter; --multi, i++)
	{
	  int row = rand() % tot;
          /* Set range num relative to this set of bounds */
          check= setEqBound(pIOp, g_index_record, row, i- rangeStart);
          if (check != 0)
          {
            err(pIOp->getNdbError());
            abort();
          }
          require(check == 0);
	}
	break;
      }
      }
    }
    require(pOp);
    
    require(check == 0);

    int rows = 0;
    check = pTrans->execute(NoCommit);
    require(check == 0);
    int fetch = g_paramters[P_FETCH].value;

    const char * result_row_ptr;

    while((check = pOp->nextResult(&result_row_ptr, true, false)) == 0){
      do {
	rows++;
      } while(!fetch && ((check = pOp->nextResult(&result_row_ptr, false, false)) == 0));
      if(check == -1){
        err(pTrans->getNdbError());
        return -1;
      }
      require(check == 2);
    }

    if(check == -1){
      err(pTrans->getNdbError());
      return -1;
    }
    require(check == 1);

    pTrans->close();
    pTrans = 0;

    stop = NdbTick_CurrentMillisecond();
    
    int time_passed= (int)(stop - start1);
    sample_rows += rows;
    sum_time+= time_passed;
    tot_rows+= rows;
    
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
  
  g_err.println("Avg time: %d ms = %u rows/sec", sum_time/tot_rows,
                (1000*tot_rows)/sum_time);
  return 0;
}
