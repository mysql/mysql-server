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

#include "NDBT_Test.hpp"
#include "NDBT_ReturnCodes.h"
#include "HugoTransactions.hpp"
#include "UtilTransactions.hpp"
#include "NdbRestarter.hpp"
#include <Vector.hpp>
#include "ScanFilter.hpp"
#include "ScanInterpretTest.hpp"

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearResTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  const NdbDictionary::Table* pResTab = 
    GETNDB(step)->getDictionary()->getTable(ctx->getProperty("ResultTabName", "NULL"));
  
  UtilTransactions utilTrans(*pResTab);
  if (utilTrans.clearTable2(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runScanRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, parallelism) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadResTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  const NdbDictionary::Table* pResTab = 
    NDBT_Table::discoverTableFromDb(GETNDB(step), 
				    ctx->getProperty("ResultTabName", "NULL"));
  
  HugoTransactions hugoTrans(*pResTab);
  if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, parallelism) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateResultTable(NDBT_Context* ctx, NDBT_Step* step){

  const NdbDictionary::Table* pTab = ctx->getTab();
  char newTabName[256];
  BaseString::snprintf(newTabName, 256, "%s_RES", pTab->getName());
  ctx->setProperty("ResultTabName", newTabName);

  NdbDictionary::Table resTab(* pTab);
  resTab.setName(newTabName);
  
  if (GETNDB(step)->getDictionary()->createTable(resTab) != 0){
    g_err << newTabName << " creation failed!"<< endl;
    return NDBT_FAILED;
  }else{
    g_info << newTabName << " created!"<< endl;
    return NDBT_OK;
  }
}

int scanWithFilter(NDBT_Context* ctx, NDBT_Step* step, ScanFilter& filt){
  int records = ctx->getNumRecords();
  const char* resTabName = ctx->getProperty("ResultTabName", "NULL");
  if (strcmp(resTabName, "NULL") == 0)
    return NDBT_FAILED;
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table* pResTab = NDBT_Table::discoverTableFromDb(GETNDB(step), resTabName);
  if (pResTab == NULL)
    return NDBT_FAILED;
  
  ScanInterpretTest interpretTest(*pTab, *pResTab);
  if (interpretTest.scanRead(GETNDB(step), 
			     records, 
			     16, 
			     filt) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}
int runScanLessThan(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  LessThanFilter filt(records);
  return scanWithFilter(ctx, step, filt);
}
int runScanEqual(NDBT_Context* ctx, NDBT_Step* step){
  EqualFilter filt;
  return scanWithFilter(ctx, step, filt);
}

int scanVerifyWithFilter(NDBT_Context* ctx, NDBT_Step* step, ScanFilter& filt){
  int records = ctx->getNumRecords();
  const char* resTabName = ctx->getProperty("ResultTabName", "NULL");
  if (strcmp(resTabName, "NULL") == 0)
    return NDBT_FAILED;
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table* pResTab = NDBT_Table::discoverTableFromDb(GETNDB(step), resTabName);
  if (pResTab == NULL)
    return NDBT_FAILED;
  
  ScanInterpretTest interpretTest(*pTab, *pResTab);
  if (interpretTest.scanReadVerify(GETNDB(step), 
				   records, 
				   16, 
				   filt) != NDBT_OK){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}
int runScanLessThanVerify(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  LessThanFilter filt(records);
  return scanVerifyWithFilter(ctx, step, filt);
}
int runScanEqualVerify(NDBT_Context* ctx, NDBT_Step* step){
  EqualFilter filt;
  return scanVerifyWithFilter(ctx, step, filt);
}

int runScanEqualLoop(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int l = 0;
  EqualFilter filt;
  while(l < loops){
    if (scanWithFilter(ctx, step, filt) != NDBT_OK)
      return NDBT_FAILED;
    if (runClearResTable(ctx, step) != NDBT_OK)
      return NDBT_FAILED;
    l++;
  }
  return NDBT_OK;
}


int runScanEqualVerifyLoop(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int l = 0;
  EqualFilter filt;
  while(l < loops){
    if (scanWithFilter(ctx, step, filt) != NDBT_OK)
      return NDBT_FAILED;
    if (scanVerifyWithFilter(ctx, step, filt) != NDBT_OK)
      return NDBT_FAILED;
    if (runClearResTable(ctx, step) != NDBT_OK)
      return NDBT_FAILED;
    l++;
  }
  return NDBT_OK;
}

int runScanLessThanLoop(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int l = 0;
  LessThanFilter filt(records);
  while(l < loops){
    if (scanWithFilter(ctx, step, filt) != NDBT_OK)
      return NDBT_FAILED;
    if (runClearResTable(ctx, step) != NDBT_OK)
      return NDBT_FAILED;
    l++;
  }
  return NDBT_OK;
}

NDBT_TESTSUITE(testScanInterpreter);
TESTCASE("ScanLessThan", 
	 "Read all records in table TX with attrX less "\
	 "than a value and store the resultset in TX_RES."\
	 "Then compare records in TX_RES with records in TX."){
  //  TABLE("T1");
  //  TABLE("T2");
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateResultTable);
  STEP(runScanLessThan);
  VERIFIER(runScanLessThanVerify);
  FINALIZER(runClearTable);
  FINALIZER(runClearResTable);
}
TESTCASE("ScanEqual", 
	 "Read all records in table TX with attrX equal "\
	 "to a value and store the resultset in TX_RES."\
	 "Then compare records in TX_RES with records in TX."){
  //  TABLE("T1");
  //  TABLE("T2");
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateResultTable);
  STEP(runScanEqual);
  VERIFIER(runScanEqualVerify);
  FINALIZER(runClearTable);
  FINALIZER(runClearResTable);
}
TESTCASE("ScanEqualLoop", 
	 "Scan all records in TX equal to a value."\
	 "Do this loop number of times"){
  //  TABLE("T1");
  //  TABLE("T2");
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateResultTable);
  STEP(runScanEqualLoop);
  FINALIZER(runClearTable);
  FINALIZER(runClearResTable);
}
TESTCASE("ScanEqualVerifyLoop", 
	 "Scan all records in TX equal to a value."\
	 "Verify record in TX_RES table"\
	 "Do this loop number of times"){
  //  TABLE("T1");
  //  TABLE("T2");
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateResultTable);
  STEP(runScanEqualVerifyLoop);
  FINALIZER(runClearTable);
  FINALIZER(runClearResTable);
}
TESTCASE("ScanLessThanLoop", 
	 "Scan all records in TX less than a value."\
	 "Do this loop number of times"){
  //  TABLE("T1");
  //  TABLE("T2");
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateResultTable);
  STEP(runScanLessThanLoop);
  FINALIZER(runClearTable);
  FINALIZER(runClearResTable);
}
NDBT_TESTSUITE_END(testScanInterpreter);

int main(int argc, const char** argv){
  ndb_init();
  return testScanInterpreter.execute(argc, argv);
}



