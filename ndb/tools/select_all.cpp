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

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NdbMain.h>
#include <NDBT.hpp> 
#include <NdbSleep.h>
#include <getarg.h>
#include <NdbScanFilter.hpp>
 
#ifndef DBUG_OFF
const char *debug_option= 0;
#endif

int scanReadRecords(Ndb*, 
		    const NdbDictionary::Table*, 
		    const NdbDictionary::Index*,
		    int parallel,
		    int lockType,
		    bool headers,
		    bool useHexFormat,
		    char delim,
		    bool orderby);

int main(int argc, const char** argv){
  ndb_init();
  int _parallelism = 240;
  const char* _delimiter = "\t";
  int _header = true;
  int _useHexFormat = false;
  const char* _tabname = NULL;
  const char* _dbname = "TEST_DB";
  int _help = 0;
  int _lock = 0;
  int _order = 0;

  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"},
    { "parallelism", 'p', arg_integer, &_parallelism, "parallelism", 
      "parallelism" },
    { "header", 'h', arg_flag, &_header, "Print header", "header" },
    { "useHexFormat", 'x', arg_flag, &_useHexFormat, 
      "Output numbers in hexadecimal format", "useHexFormat" },
    { "delimiter", 'd', arg_string, &_delimiter, "Column delimiter", 
      "delimiter" },
#ifndef DBUG_OFF
    { "debug", 0, arg_string, &debug_option,
      "Specify debug options e.g. d:t:i:o,out.trace", "options" },
#endif
    { "usage", '?', arg_flag, &_help, "Print help", "" },
    { "lock", 'l', arg_integer, &_lock, 
      "Read(0), Read-hold(1), Exclusive(2)", "lock"},
    { "order", 'o', arg_flag, &_order, "Sort resultset according to index", ""}
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program reads all records from one table in NDB Cluster\n"\
    "and print them to stdout.  This is performed using a scan read.\n"\
    "(It only print error messages if it encounters a permanent error.)\n"\
    "It can also be used to dump the content of a table to file \n"\
    "  ex: select_all --no-header --delimiter=';' T4 > T4.data\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];

#ifndef DBUG_OFF
  if (debug_option)
    DBUG_PUSH(debug_option);
#endif

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
  if(optind+1 < argc){
    pIdx = MyNdb.getDictionary()->getIndex(argv[optind+1], _tabname);
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
