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
#include <ndb_opts.h>

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NdbMain.h>
#include <NDBT.hpp> 
#include <NdbSleep.h>
#include <NdbScanFilter.hpp>
 
int scanReadRecords(Ndb*, 
		    const NdbDictionary::Table*, 
		    const NdbDictionary::Index*,
		    int parallel,
		    int lockType,
		    bool headers,
		    bool useHexFormat,
		    char delim,
		    bool orderby);

static const char* opt_connect_str= 0;
static const char* _dbname = "TEST_DB";
static const char* _delimiter = "\t";
static int _unqualified, _header, _parallelism, _useHexFormat, _lock,
  _order;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (gptr*) &_dbname, (gptr*) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "parallelism", 'p', "parallelism",
    (gptr*) &_parallelism, (gptr*) &_parallelism, 0,
    GET_INT, REQUIRED_ARG, 240, 0, 0, 0, 0, 0 }, 
  { "lock", 'l', "Read(0), Read-hold(1), Exclusive(2)",
    (gptr*) &_lock, (gptr*) &_lock, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "order", 'o', "Sort resultset according to index",
    (gptr*) &_order, (gptr*) &_order, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "header", 'h', "Print header",
    (gptr*) &_header, (gptr*) &_header, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 }, 
  { "useHexFormat", 'x', "Output numbers in hexadecimal format",
    (gptr*) &_useHexFormat, (gptr*) &_useHexFormat, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "delimiter", 'D', "Column delimiter",
    (gptr*) &_delimiter, (gptr*) &_delimiter, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void print_version()
{
  printf("MySQL distrib %s, for %s (%s)\n",MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}
static void usage()
{
  char desc[] = 
    "tabname\n"\
    "This program reads all records from one table in NDB Cluster\n"\
    "and print them to stdout.  This is performed using a scan read.\n"\
    "(It only print error messages if it encounters a permanent error.)\n"\
    "It can also be used to dump the content of a table to file \n"\
    "  ex: select_all --no-header --delimiter=';' T4 > T4.data\n";
  print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:O,/tmp/ndb_select_all.trace");
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "ndb_tools",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  const char* _tabname;
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if ((_tabname = argv[0]) == 0) {
    usage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb::setConnectString(opt_connect_str);
  // Connect to Ndb
  Ndb MyNdb(_dbname);

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  // Check if table exists in db
  const NdbDictionary::Table* pTab = NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
  const NdbDictionary::Index * pIdx = 0;
  if(argc > 1){
    pIdx = MyNdb.getDictionary()->getIndex(argv[0], _tabname);
  }

  if(pTab == NULL){
    ndbout << " Table " << _tabname << " does not exist!" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  
  if(_order && pIdx == NULL){
    ndbout << " Order flag given without an index" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  if (scanReadRecords(&MyNdb, 
		      pTab, 
		      pIdx,
		      _parallelism, 
		      _lock,
		      _header, 
		      _useHexFormat, 
		      (char)*_delimiter, _order) != 0){
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  return NDBT_ProgramExit(NDBT_OK);

}

int scanReadRecords(Ndb* pNdb, 
		    const NdbDictionary::Table* pTab, 
		    const NdbDictionary::Index* pIdx,
		    int parallel,
		    int _lock,
		    bool headers,
		    bool useHexFormat,
		    char delimiter, bool order){

  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbConnection	       *pTrans;
  NdbScanOperation	       *pOp;
  NdbIndexScanOperation * pIOp= 0;

  NDBT_ResultRow * row = new NDBT_ResultRow(*pTab, delimiter);

  while (true){

    if (retryAttempt >= retryMax){
      ndbout << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return -1;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return -1;
    }

    
    pOp = (!pIdx) ? pTrans->getNdbScanOperation(pTab->getName()) : 
      pIOp=pTrans->getNdbIndexScanOperation(pIdx->getName(), pTab->getName());
    
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return -1;
    }

    NdbResultSet * rs;
    switch(_lock + (3 * order)){
    case 1:
      rs = pOp->readTuples(NdbScanOperation::LM_Read, 0, parallel);
      break;
    case 2:
      rs = pOp->readTuples(NdbScanOperation::LM_Exclusive, 0, parallel);
      break;
    case 3:
      rs = pIOp->readTuples(NdbScanOperation::LM_CommittedRead, 0, parallel, 
			    true);
      break;
    case 4:
      rs = pIOp->readTuples(NdbScanOperation::LM_Read, 0, parallel, true);
      break;
    case 5:
      rs = pIOp->readTuples(NdbScanOperation::LM_Exclusive, 0, parallel, true);
      break;
    case 0:
    default:
      rs = pOp->readTuples(NdbScanOperation::LM_CommittedRead, 0, parallel);
      break;
    }
    if( rs == 0 ){
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return -1;
    }
    
    if(0){
      NdbScanFilter sf(pOp);
#if 0
      sf.begin(NdbScanFilter::AND);
      sf.le(0, (Uint32)10);
      
      sf.end();
#elif 0
      sf.begin(NdbScanFilter::OR);
      sf.begin(NdbScanFilter::AND);
      sf.ge(0, (Uint32)10);
      sf.lt(0, (Uint32)20);
      sf.end();
      sf.begin(NdbScanFilter::AND);
      sf.ge(0, (Uint32)30);
      sf.lt(0, (Uint32)40);
      sf.end();
      sf.end();
#elif 1
      sf.begin(NdbScanFilter::AND);
      sf.begin(NdbScanFilter::OR);
      sf.begin(NdbScanFilter::AND);
      sf.ge(0, (Uint32)10);
      sf.lt(0, (Uint32)20);
      sf.end();
      sf.begin(NdbScanFilter::AND);
      sf.ge(0, (Uint32)30);
      sf.lt(0, (Uint32)40);
      sf.end();
      sf.end();
      sf.begin(NdbScanFilter::OR);
      sf.begin(NdbScanFilter::AND);
      sf.ge(0, (Uint32)0);
      sf.lt(0, (Uint32)50);
      sf.end();
      sf.begin(NdbScanFilter::AND);
      sf.ge(0, (Uint32)100);
      sf.lt(0, (Uint32)200);
      sf.end();
      sf.end();
      sf.end();
#endif
    } else {
      check = pOp->interpret_exit_ok();
      if( check == -1 ) {
	ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return -1;
      }
    }
    
    for(int a = 0; a<pTab->getNoOfColumns(); a++){
      if((row->attributeStore(a) = 
	  pOp->getValue(pTab->getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return -1;
      }
    }

    check = pTrans->execute(NoCommit);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return -1;
    }

    if (headers)
      row->header(ndbout) << endl;
    
    int eof;
    int rows = 0;
    eof = rs->nextResult();
    
    while(eof == 0){
      rows++;

      if (useHexFormat) {
	ndbout.setHexFormat(1) << (*row) << endl;
      } else {
	ndbout << (*row) << endl;
      }

      eof = rs->nextResult();
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return -1;
    }

    pNdb->closeTransaction(pTrans);

    ndbout << rows << " rows returned" << endl;

    return 0;
  }
  return -1;
}
