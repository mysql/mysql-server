/*
   Copyright (C) 2003-2006, 2008 MySQL AB
    Use is subject to license terms.

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

#include "NDBT_Test.hpp"
#include "NDBT_ReturnCodes.h"
#include "HugoTransactions.hpp"
#include "HugoAsynchTransactions.hpp"
#include "UtilTransactions.hpp"

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;

  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTableAsynch(GETNDB(step), records, batchSize, 
				transactions, operations) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runInsert(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;

  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  // Insert records, dont allow any 
  // errors(except temporary) while inserting
  if (hugoTrans.loadTableAsynch(GETNDB(step), records, batchSize, 
				transactions, operations) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runVerifyInsert(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;

  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecordsAsynch(GETNDB(step),  records, batchSize, 
				   transactions, operations) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;
  
  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecordsAsynch(GETNDB(step),  records, batchSize, 
				   transactions, operations) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runPkDelete(NDBT_Context* ctx, NDBT_Step* step){

  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;

  int i = 0;
  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    ndbout << i << ": ";
    if (hugoTrans.pkDelRecordsAsynch(GETNDB(step),  records, batchSize,
				     transactions, operations) != 0){
      return NDBT_FAILED;
    }
    // Load table, don't allow any primary key violations
    if (hugoTrans.loadTableAsynch(GETNDB(step), records, batchSize, 
				  transactions, operations) != 0){
      return NDBT_FAILED;
    }
    i++;
  }  
  return NDBT_OK;
}


int runPkRead(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;

  int i = 0;
  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    ndbout << i << ": ";
    if (hugoTrans.pkReadRecordsAsynch(GETNDB(step), records, batchSize, 
				      transactions, operations) != NDBT_OK){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runPkUpdate(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  int transactions = (records / 100) + 1;
  int operations = (records / transactions) + 1;

  int i = 0;
  HugoAsynchTransactions hugoTrans(*ctx->getTab());
  while (i<loops) {
    ndbout << i << ": ";
    if (hugoTrans.pkUpdateRecordsAsynch(GETNDB(step), records, 
					batchSize, transactions, 
					operations) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

NDBT_TESTSUITE(testBasicAsynch);
TESTCASE("PkInsertAsynch", 
	 "Verify that we can insert and delete from this table using PK"
	 " NOTE! No errors are allowed!" ){
  INITIALIZER(runInsert);
  VERIFIER(runVerifyInsert);
}
TESTCASE("PkReadAsynch", 
	 "Verify that we can insert, read and delete from this table"
	 " using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkRead);
  FINALIZER(runClearTable);
}
TESTCASE("PkUpdateAsynch", 
	 "Verify that we can insert, update and delete from this table"
	 " using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("PkDeleteAsynch", 
	 "Verify that we can delete from this table using PK"){
  INITIALIZER(runLoadTable);
  STEP(runPkDelete);
  FINALIZER(runClearTable);
}

NDBT_TESTSUITE_END(testBasicAsynch);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testBasicAsynch);
  return testBasicAsynch.execute(argc, argv);
}

