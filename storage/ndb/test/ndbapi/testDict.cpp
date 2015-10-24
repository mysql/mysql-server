/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <../../include/kernel/ndb_limits.h>
#include <../../include/kernel/trigger_definitions.h>
#include <signaldata/DictTabInfo.hpp>
#include <random.h>
#include <NdbAutoPtr.hpp>
#include <NdbMixRestarter.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbEnv.h>
#include <ndb_rand.h>
#include <Bitmask.hpp>
#include <../src/kernel/ndbd.hpp>

#define ERR_INSERT_MASTER_FAILURE1 6013
#define ERR_INSERT_MASTER_FAILURE2 6014
#define ERR_INSERT_MASTER_FAILURE3 6015

#define ERR_INSERT_PARTIAL_START_FAIL 6140
#define ERR_INSERT_PARTIAL_PARSE_FAIL 6141
#define ERR_INSERT_PARTIAL_FLUSH_PREPARE_FAIL 6142
#define ERR_INSERT_PARTIAL_PREPARE_FAIL 6143
#define ERR_INSERT_PARTIAL_ABORT_PARSE_FAIL 6144
#define ERR_INSERT_PARTIAL_ABORT_PREPARE_FAIL 6145
#define ERR_INSERT_PARTIAL_FLUSH_COMMIT_FAIL 6146
#define ERR_INSERT_PARTIAL_COMMIT_FAIL 6147
#define ERR_INSERT_PARTIAL_FLUSH_COMPLETE_FAIL 6148
#define ERR_INSERT_PARTIAL_COMPLETE_FAIL 6149
#define ERR_INSERT_PARTIAL_END_FAIL 6150

#define FAIL_BEGIN 0
#define FAIL_CREATE 1
#define FAIL_END 2
#define SUCCEED_COMMIT 3
#define SUCCEED_ABORT 4

#define ndb_master_failure 1

char f_tablename[256];
 
#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; } 

#define CHECK2(b, c) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << ": " << c << endl; \
  result = NDBT_FAILED; \
  goto end; }

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(pNdb, records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateInvalidTables(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;

  char failTabName[256];
  
  const int expectedDictErrors[6]= {720, 
                                    4317, 
                                    737, 
                                    739, 
                                    736, 
                                    740 };

  for (int i = 0; i < 10; i++){
    BaseString::snprintf(failTabName, 256, "F%d", i);
  
    const NdbDictionary::Table* pFailTab = NDBT_Tables::getTable(failTabName);
    if (pFailTab != NULL){
      ndbout << "|- " << failTabName << endl;

      // Try to create table in db
      if (pFailTab->createTableInDb(pNdb) == 0){
        ndbout << failTabName << " created, this was not expected"<< endl;
        result = NDBT_FAILED;
      }

      // Ensure any error is roughly as expected
      int errorCode=pNdb->getDictionary()->getNdbError().code;
      bool errorOk= false;
      for (int e=0; e < 6; e++)
        errorOk |= (errorCode == expectedDictErrors[e]);

      if (!errorOk)
      {
        ndbout << "Failure, got dict error : " << pNdb->getDictionary()->
          getNdbError().code << endl;
        return NDBT_FAILED;
      }

      // Verify that table is not in db    
      const NdbDictionary::Table* pTab2 = 
	NDBT_Table::discoverTableFromDb(pNdb, failTabName) ;
      if (pTab2 != NULL){
        ndbout << failTabName << " was found in DB, this was not expected"<< endl;
        result = NDBT_FAILED;
	if (pFailTab->equal(*pTab2) == true){
	  ndbout << "It was equal" << endl;
	} else {
	  ndbout << "It was not equal" << endl;
	}
	int records = 1000;
	HugoTransactions hugoTrans(*pTab2);
	if (hugoTrans.loadTable(pNdb, records) != 0){
	  ndbout << "It can NOT be loaded" << endl;
	} else{
	  ndbout << "It can be loaded" << endl;
	  
	  UtilTransactions utilTrans(*pTab2);
	  if (utilTrans.clearTable(pNdb, records, 64) != 0){
	    ndbout << "It can NOT be cleared" << endl;
	  } else{
	    ndbout << "It can be cleared" << endl;
	  }	  
	}
	
	if (pNdb->getDictionary()->dropTable(pTab2->getName()) == -1){
	  ndbout << "It can NOT be dropped" << endl;
	} else {
	  ndbout << "It can be dropped" << endl;
	}
      }
    }
  }
  return result;
}

int runCreateTheTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  const NdbDictionary::Table* pTab = ctx->getTab();

  // Try to create table in db
  if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
    return NDBT_FAILED;
  }

  // Verify that table is in db     
  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }
  ctx->setTab(pTab2);

  BaseString::snprintf(f_tablename, sizeof(f_tablename), 
                       "%s", pTab->getName());

  return NDBT_OK;
}

int runDropTheTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  
  // Drop table
  pNdb->getDictionary()->dropTable(f_tablename);
  
  return NDBT_OK;
}

int runSetDropTableConcurrentLCP(NDBT_Context *ctx, NDBT_Step *step)
{
  NdbRestarter restarter;
  if(restarter.insertErrorInAllNodes(5088) != 0)
  {
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runSetMinTimeBetweenLCP(NDBT_Context *ctx, NDBT_Step *step)
{
  NdbRestarter restarter;
  int result;
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if (restarter.dumpStateAllNodes(&val, 1) != 0)
  {
    do { CHECK(0); } while(0);
    g_err << "Failed to set LCP to min value" << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runResetMinTimeBetweenLCP(NDBT_Context *ctx, NDBT_Step *step)
{
  NdbRestarter restarter;
  int result;
  int val2[] = { DumpStateOrd::DihMinTimeBetweenLCP, 0 };
  if (restarter.dumpStateAllNodes(val2, 2) != 0)
  {
    do { CHECK(0); } while(0);
    g_err << "Failed to set LCP to min value" << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runSetDropTableConcurrentLCP2(NDBT_Context *ctx, NDBT_Step *step)
{
  NdbRestarter restarter;
  if(restarter.insertErrorInAllNodes(5089) != 0)
  {
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

/*******
 * Precondition: 
 *    'DataMemory' has been filled until insertion failed
 *    due to 'DbIsFull'. The table 'TRANSACTION' should
 *    not exist in the DB
 *
 * Test:
 *    Creation of the (empty) table 'TRANSACTION'
 *    should succeed even if 'DbIsFull'. However, 
 *    insertion of the first row should fail.
 *
 * Postcond:
 *    The created table 'TRANSACTION is removed.
 *    DataMemory is still full.
 */
int runCreateTableWhenDbIsFull(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  const char* tabName = "TRANSACTION"; //Use a util table

  // Precondition is that 'DataMemory' filled to max.
  // So we skip test if a DiskStorage table was filled
  for (Uint32 i = 0; i<(Uint32)ctx->getTab()->getNoOfColumns(); i++)
  {
    if (ctx->getTab()->getColumn(i)->getStorageType() == 
        NdbDictionary::Column::StorageTypeDisk)
    {
      ndbout << "Skip test for *disk* tables" << endl;
      return NDBT_OK;
    }
  }

  const NdbDictionary::Table* pTab = NDBT_Tables::getTable(tabName);
  while (pTab != NULL){ //Always 'break' without looping
    ndbout << "|- " << tabName << endl;
    
    // Verify that table is not in db     
    if (NDBT_Table::discoverTableFromDb(pNdb, tabName) != NULL){
      ndbout << tabName << " was found in DB"<< endl;
      result = NDBT_FAILED;
      break;
    }

    // Create (empty) table in db, should succeed even if 'DbIsFull'
    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      ndbout << tabName << " was not created when DB is full"<< endl;
      result = NDBT_FAILED;
      break;
    }

    // Verify that table is now in db     
    if (NDBT_Table::discoverTableFromDb(pNdb, tabName) == NULL){
      ndbout << tabName << " was not visible in DB"<< endl;
      result = NDBT_FAILED;
      break;
    }

    // As 'DbIsFull', insert of a single record should fail
    HugoOperations hugoOps(*pTab);
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) != 0); //Should fail
    CHECK(hugoOps.closeTransaction(pNdb) == 0);  

    break;
  }

  // Drop table (if exist, so we dont care about errors)
  pNdb->getDictionary()->dropTable(tabName);
  return result;
}

int runDropTableWhenDbIsFull(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  const char* tabName = "TRANSACTION"; //Use a util table
  
  const NdbDictionary::Table* pTab = NDBT_Table::discoverTableFromDb(pNdb, tabName);
  if (pTab != NULL){
    ndbout << "|- TRANSACTION" << endl;
    
    // Try to drop table in db
    if (pNdb->getDictionary()->dropTable(pTab->getName()) == -1){
      result = NDBT_FAILED;
    }

    // Verify that table is not in db     
    if (NDBT_Table::discoverTableFromDb(pNdb, tabName) != NULL){
      ndbout << tabName << " was found in DB"<< endl;
      result = NDBT_FAILED;
    }
  }

  return result;

}


int runCreateAndDrop(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();
  int i = 0;
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  while (i < loops){

    ndbout << i << ": ";    
    // Try to create table in db
    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }

    if (pNdb->getDictionary()->dropTable(pTab2->getName())){
      ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
      return NDBT_FAILED;
    }
    
    // Verify that table is not in db     
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab3 != NULL){
      ndbout << pTab3->getName() << " was found in DB"<< endl;
      return NDBT_FAILED;
    }
    i++;
  }

  return NDBT_OK;
}

int runCreateAndDropAtRandom(NDBT_Context* ctx, NDBT_Step* step)
{
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();

  int numAllTables = NDBT_Tables::getNumTables();
  struct TabList {
    int exists; // -1 = skip, 0 = no, 1 = yes
    const NdbDictionary::Table* pTab; // retrieved
    TabList() { exists = -1; pTab = 0; }
  };
  TabList* tabList = new TabList [ numAllTables ];
  int numTables = 0;
  int num;
  for (num = 0; num < numAllTables; num++) {
    const NdbDictionary::Table* pTab = NDBT_Tables::getTable(num);
    if (pTab->checkColumns(0, 0) & 2) // skip disk
      continue;
    tabList[num].exists = 0;
    (void)pDic->dropTable(pTab->getName());
    numTables++;
  }
  int numExists = 0;

  const bool createIndexes = ctx->getProperty("CreateIndexes");
  const bool loadData = ctx->getProperty("LoadData");

  NdbRestarter restarter;
  int result = NDBT_OK;
  int bias = 1; // 0-less 1-more
  int i = 0;
  
  while (i < loops && result == NDBT_OK) {
    num = myRandom48(numAllTables);
    if (tabList[num].exists == -1)
      continue;
    g_info << "loop " << i << " tabs " << numExists << "/" << numTables << endl;
    const NdbDictionary::Table* pTab = NDBT_Tables::getTable(num);
    char tabName[200];
    strcpy(tabName, pTab->getName());

    if (tabList[num].exists == 0) {
      if (bias == 0 && myRandom48(100) < 80)
        continue;
      g_info << tabName << ": create" << endl;
      if (pDic->createTable(*pTab) != 0) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": create failed: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      const NdbDictionary::Table* pTab2 = pDic->getTable(tabName);
      if (pTab2 == NULL) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": verify create failed: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      tabList[num].pTab = pTab2;
      if (loadData) {
        g_info << tabName << ": load data" << endl;
        HugoTransactions hugoTrans(*pTab2);
        if (hugoTrans.loadTable(pNdb, records) != 0) {
          g_err << tabName << ": loadTable failed" << endl;
          result = NDBT_FAILED;
          break;
        }
      }
      if (createIndexes) {
        int icount = myRandom48(10);
        int inum;
        for (inum = 0; inum < icount; inum++) {
          const int tcols = pTab2->getNoOfColumns();
          require(tcols != 0);
          int icols = 1 + myRandom48(tcols);
          if (icols > NDB_MAX_ATTRIBUTES_IN_INDEX)
            icols = NDB_MAX_ATTRIBUTES_IN_INDEX;
          char indName[200];
          sprintf(indName, "%s_X%d", tabName, inum);
          NdbDictionary::Index ind(indName);
          ind.setTable(tabName);
          ind.setType(NdbDictionary::Index::OrderedIndex);
          ind.setLogging(false);
          Bitmask<MAX_ATTRIBUTES_IN_TABLE> mask;
          char ilist[200];
          ilist[0] = 0;
          int ic;
          for (ic = 0; ic < icols; ic++) {
            int tc = myRandom48(tcols);
            const NdbDictionary::Column* c = pTab2->getColumn(tc);
            require(c != 0);
            if (mask.get(tc) ||
                c->getType() == NdbDictionary::Column::Blob ||
                c->getType() == NdbDictionary::Column::Text ||
                c->getType() == NdbDictionary::Column::Bit ||
                c->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
              continue;
            ind.addColumn(*c);
            mask.set(tc);
            sprintf(ilist + strlen(ilist), " %d", tc);
          }
          if (mask.isclear())
            continue;
          g_info << indName << ": columns:" << ilist << endl;
          if (pDic->createIndex(ind) == 0) {
            g_info << indName << ": created" << endl;
          } else {
            const NdbError err = pDic->getNdbError();
            g_err << indName << ": create index failed: " << err << endl;
            if (err.code != 826 && // Too many tables and attributes..
                err.code != 903 && // Too many ordered indexes..
                err.code != 904 && // Out of fragment records..
                err.code != 905 && // Out of attribute records..
                err.code != 707 && // No more table metadata records..
                err.code != 708)   // No more attribute metadata records..
            {
              result = NDBT_FAILED;
              break;
            }
          }
        }
      }
      if (loadData) {
        // first update a random table to flush global variables
        int num3 = 0;
        while (1) {
          num3 = myRandom48(numAllTables);
          if (num == num3 || tabList[num3].exists == 1)
            break;
        }
        const NdbDictionary::Table* pTab3 = tabList[num3].pTab;
        require(pTab3 != 0);
        char tabName3[200];
        strcpy(tabName3, pTab3->getName());
        HugoTransactions hugoTrans(*pTab3);
        g_info << tabName3 << ": update data" << endl;
        if (hugoTrans.pkUpdateRecords(pNdb, records) != 0) {
          g_err << tabName3 << ": pkUpdateRecords failed" << endl;
          result = NDBT_FAILED;
          break;
        }
      }
      if (loadData) {
        HugoTransactions hugoTrans(*pTab2);
        g_info << tabName << ": update data" << endl;
        if (hugoTrans.pkUpdateRecords(pNdb, records) != 0) {
          g_err << "pkUpdateRecords failed" << endl;
          result = NDBT_FAILED;
          break;
        }
      }
      tabList[num].exists = 1;
      require(numExists < numTables);
      numExists++;
      if (numExists == numTables)
        bias = 0;
    }
    else if (tabList[num].exists == 1) {
      if (bias == 1 && myRandom48(100) < 80)
        continue;
      g_info << tabName << ": drop" << endl;
      if (restarter.insertErrorInAllNodes(4013) != 0) {
        g_err << "error insert failed" << endl;
        result = NDBT_FAILED;
        break;
      }
      if (pDic->dropTable(tabName) != 0) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": drop failed: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      const NdbDictionary::Table* pTab2 = pDic->getTable(tabName);
      if (pTab2 != NULL) {
        g_err << tabName << ": verify drop: table exists" << endl;
        result = NDBT_FAILED;
        break;
      }
      if (pDic->getNdbError().code != 709 &&
          pDic->getNdbError().code != 723) {
        const NdbError err = pDic->getNdbError();
        g_err << tabName << ": verify drop: " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      tabList[num].exists = 0;
      require(numExists > 0);
      numExists--;
      if (numExists == 0)
        bias = 1;
    }
    i++;
  }

  for (num = 0; num < numAllTables; num++)
    if (tabList[num].exists == 1)
      pDic->dropTable(NDBT_Tables::getTable(num)->getName());

  delete [] tabList;
  return result;
}


int runCreateAndDropWithData(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int i = 0;
  
  NdbRestarter restarter;
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if(restarter.dumpStateAllNodes(&val, 1) != 0){
    int result;
    do { CHECK(0); } while (0);
    g_err << "Unable to change timebetween LCP" << endl;
    return NDBT_FAILED;
  }
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  while (i < loops){
    ndbout << i << ": ";
    // Try to create table in db
    
    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }

    HugoTransactions hugoTrans(*pTab2);
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    int count = 0;
    UtilTransactions utilTrans(*pTab2);
    if (utilTrans.selectCount(pNdb, 64, &count) != 0){
      return NDBT_FAILED;
    }
    if (count != records){
      ndbout << count <<" != "<<records << endl;
      return NDBT_FAILED;
    }

    if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
      ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
      return NDBT_FAILED;
    }
    
    // Verify that table is not in db     
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab3 != NULL){
      ndbout << pTab3->getName() << " was found in DB"<< endl;
      return NDBT_FAILED;
    }
    

    i++;
  }

  return NDBT_OK;
}

int runFillTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.fillTable(pNdb) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(pNdb,  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateAndDropDuring(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int i = 0;
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  while (i < loops && result == NDBT_OK){
    ndbout << i << ": " << endl;    
    // Try to create table in db

    Ndb* pNdb = GETNDB(step);
    g_debug << "Creating table" << endl;

    if (NDBT_Tables::createTable(pNdb, pTab->getName()) != 0){
      g_err << "createTableInDb failed" << endl;
      result =  NDBT_FAILED;
      continue;
    }
    
    g_debug << "Verifying creation of table" << endl;

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      g_err << pTab->getName() << " was not found in DB"<< endl;
      result =  NDBT_FAILED;
      continue;
    }
    
    NdbSleep_MilliSleep(3000);

    g_debug << "Dropping table" << endl;

    if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
      g_err << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
      result =  NDBT_FAILED;
      continue;
    }
    
    g_debug << "Verifying dropping of table" << endl;

    // Verify that table is not in db     
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab3 != NULL){
      g_err << pTab3->getName() << " was found in DB"<< endl;
      result =  NDBT_FAILED;
      continue;
    }
    i++;
  }
  ctx->stopTest();
  
  return result;
}


int runUseTableUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();

  const NdbDictionary::Table* pTab = ctx->getTab();

  while (ctx->isTestStopped() == false) {
    // g_info << i++ << ": ";


    // Delete and recreate Ndb object
    // Otherwise you always get Invalid Schema Version
    // It would be a nice feature to remove this two lines
    //step->tearDown();
    //step->setUp();

    Ndb* pNdb = GETNDB(step);

    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL)
      continue;

    int res;
    HugoTransactions hugoTrans(*pTab2);
    if ((res = hugoTrans.loadTable(pNdb, records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
    
    if ((res = hugoTrans.clearTable(pNdb,  records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
  }
  g_info << endl;
  return NDBT_OK;
}

int runUseTableUntilStopped2(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());

  if (pTab2 == NULL) {
    g_err << "Table: " << pTab->getName() 
          << ", not 'discovered' on line " << __LINE__
          << endl;
    return NDBT_FAILED;
  }
  HugoTransactions hugoTrans(*pTab2);

  int i = 0;
  while (ctx->isTestStopped() == false) 
  {
    ndbout_c("loop: %u", i++);


    // Delete and recreate Ndb object
    // Otherwise you always get Invalid Schema Version
    // It would be a nice feature to remove this two lines
    //step->tearDown();
    //step->setUp();


    int res;
    if ((res = hugoTrans.loadTable(pNdb, records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
    
    if ((res = hugoTrans.scanUpdateRecords(pNdb, records)) != 0)
    {
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }

    if ((res = hugoTrans.clearTable(pNdb,  records)) != 0){
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
  }
  g_info << endl;
  return NDBT_OK;
}

int runUseTableUntilStopped3(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  const NdbDictionary::Table* pTab2 =
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL) {
    g_err << "Table : " << pTab->getName() 
          << ", not 'discovered' on line " << __LINE__
          << endl;
    return NDBT_FAILED;
  }
  HugoTransactions hugoTrans(*pTab2);

  int i = 0;
  while (ctx->isTestStopped() == false)
  {
    ndbout_c("loop: %u", i++);


    // Delete and recreate Ndb object
    // Otherwise you always get Invalid Schema Version
    // It would be a nice feature to remove this two lines
    //step->tearDown();
    //step->setUp();


    int res;
    if ((res = hugoTrans.scanUpdateRecords(pNdb, records)) != 0)
    {
      NdbError err = pNdb->getNdbError(res);
      if(err.classification == NdbError::SchemaError){
	pNdb->getDictionary()->invalidateTable(pTab->getName());
      }
      continue;
    }
  }
  g_info << endl;
  return NDBT_OK;
}

/**
 * This is a regression test for bug 14190114 
 * "CLUSTER CRASH DUE TO NDBREQUIRE IN ./LOCALPROXY.HPP DBLQH (LINE: 234)".
 * This bug occurs if there is a takeover (i.e. the master node crashes) 
 * while an LQH block is executing a DROP_TAB_REQ signal. It only affects
 * multi-threaded ndb.
 */
static int
runDropTakeoverTest(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() == 1)
  {
    g_info << "Cannot do this test with just one datanode." << endl;
    return NDBT_OK;
  }

  Ndb* const ndb = GETNDB(step);
  NdbDictionary::Dictionary* const dict = ndb->getDictionary();

  // First we create a table that is a copy of ctx->getTab().
  NdbDictionary::Table copyTab(*ctx->getTab());
  const char* copyName = "copyTab";

  copyTab.setName(copyName);
  if (dict->createTable(copyTab) != 0)
  {
    g_err << "Failed to create table " << copyName << endl
          << dict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  /**
   * Find the node id of the master node and another data node that is not 
   * the master.
   */
  const int masterNodeId = restarter.getMasterNodeId();
  const int nonMasterNodeId =
    masterNodeId == restarter.getDbNodeId(0) ?
    restarter.getDbNodeId(1) : 
    restarter.getDbNodeId(0);

  /**
   * This error insert makes LQH resend the DROP_TAB_REQ to itself (with a
   * long delay) rather than executing it.
   * This makes it appear as if though the LQH block spends a long time 
   * executing the DROP_TAB_REQ signal.
   */
  g_info << "Insert error 5076 in node " << nonMasterNodeId << endl;
  restarter.insertErrorInNode(nonMasterNodeId, 5076);
  /**
   * This error insert makes the master node crash when one of its LQH 
   * blocks tries to execute a DROP_TAB_REQ signal. This will then trigger
   * a takeover.
   */
  g_info << "Insert error 5077 in node " << masterNodeId << endl;
  restarter.insertErrorInNode(masterNodeId, 5077);

  // dropTable should succeed with the new master.
  g_info << "Trying to drop table " << copyName << endl;
  if (dict->dropTable(copyName))
  {
    g_err << "Unexpectedly failed to drop table " << copyName << endl;
    return NDBT_FAILED;
  }

  /** 
   * Check that only old master is dead. Bug 14190114 would cause other nodes
   * to die as well.
   */
  const int deadNodeId = restarter.checkClusterAlive(&masterNodeId, 1);
  if (deadNodeId != 0)
  {
    g_err << "NodeId " << deadNodeId << " is down." << endl;
    return NDBT_FAILED;
  }
  
  // Verify that old master comes back up, and that no other node crashed.
  g_info << "Waiting for all nodes to be up." << endl;
  if (restarter.waitClusterStarted() != 0)
  {
    g_err << "One or more cluster nodes are not up." << endl;
    return NDBT_FAILED;
  }

  /**
   * The 'drop table' operation should have been rolled forward, since the
   * node crash happened in the complete phase. Verify that the table is 
   * gone.
   */
  g_info << "Verifying that table " << copyName << " was deleted." << endl;
  if (dict->getTable(copyName) == NULL)
  {
    if (dict->getNdbError().code != 723) // 723 = no such table existed.
    {
      g_err << "dict->getTable() for " << copyName 
            << " failed in unexpedted way:" << endl
            << dict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    g_err << "Transaction dropping " << copyName << " was not rolled forward"
          << endl;
    return NDBT_FAILED;
  }
  
  /** 
   * Do another dictionary transaction, to verify that the cluster allows that.
   */
  NdbDictionary::Table extraTab(*ctx->getTab());
  const char* extraName = "extraTab";

  extraTab.setName(extraName);
  g_info << "Trying to create table " << extraName << endl;
  if (dict->createTable(extraTab) != 0)
  {
    g_err << "Failed to create table " << extraName << endl
          << dict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  // Clean up by dropping extraTab.
  g_info << "Trying to drop table " << extraName << endl;
  if (dict->dropTable(extraName) != 0)
  {
    g_err << "Failed to drop table " << extraName << endl
          << dict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runCreateMaxTables(NDBT_Context* ctx, NDBT_Step* step)
{
  char tabName[256];
  int numTables = ctx->getProperty("tables", 1000);
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int i = 0;
  for (i = 0; i < numTables; i++) {
    BaseString::snprintf(tabName, 256, "MAXTAB%d", i);
    if (pNdb->waitUntilReady(30) != 0) {
      // Db is not ready, return with failure
      return NDBT_FAILED;
    }
    const NdbDictionary::Table* pTab = ctx->getTab();
    //ndbout << "|- " << tabName << endl;
    // Set new name for T1
    NdbDictionary::Table newTab(* pTab);
    newTab.setName(tabName);
    // Drop any old (or try to)
    (void)pDic->dropTable(newTab.getName());
    // Try to create table in db
    if (newTab.createTableInDb(pNdb) != 0) {
      ndbout << tabName << " could not be created: "
             << pDic->getNdbError() << endl;
      if (pDic->getNdbError().code == 707 ||
          pDic->getNdbError().code == 708 ||
          pDic->getNdbError().code == 826 ||
          pDic->getNdbError().code == 827)
        break;
      return NDBT_FAILED;
    }
    // Verify that table exists in db    
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, tabName) ;
    if (pTab3 == NULL){
      ndbout << tabName << " was not found in DB: "
             << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    if (! newTab.equal(*pTab3)) {
      ndbout << "It was not equal" << endl; abort();
      return NDBT_FAILED;
    }
    int records = ctx->getNumRecords();
    HugoTransactions hugoTrans(*pTab3);
    if (hugoTrans.loadTable(pNdb, records) != 0) {
      ndbout << "It can NOT be loaded" << endl;
      return NDBT_FAILED;
    }
    UtilTransactions utilTrans(*pTab3);
    if (utilTrans.clearTable(pNdb, records, 64) != 0) {
      ndbout << "It can NOT be cleared" << endl;
      return NDBT_FAILED;
    }
  }
  if (pNdb->waitUntilReady(30) != 0) {
    // Db is not ready, return with failure
    return NDBT_FAILED;
  }
  ctx->setProperty("maxtables", i);
  // HURRAAA!
  return NDBT_OK;
}

int runDropMaxTables(NDBT_Context* ctx, NDBT_Step* step)
{
  char tabName[256];
  int numTables = ctx->getProperty("maxtables", (Uint32)0);
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  for (int i = 0; i < numTables; i++) {
    BaseString::snprintf(tabName, 256, "MAXTAB%d", i);
    if (pNdb->waitUntilReady(30) != 0) {
      // Db is not ready, return with failure
      return NDBT_FAILED;
    }
    // Verify that table exists in db    
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, tabName) ;
    if (pTab3 == NULL) {
      ndbout << tabName << " was not found in DB: "
             << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    // Try to drop table in db
    if (pDic->dropTable(pTab3->getName()) != 0) {
      ndbout << tabName << " could not be dropped: "
             << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runTestFragmentTypes(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int fragTtype = ctx->getProperty("FragmentType");
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  NdbRestarter restarter;

  if (pNdb->waitUntilReady(30) != 0){
    // Db is not ready, return with failure
    return NDBT_FAILED;
  }
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  pNdb->getDictionary()->dropTable(pTab->getName());

  NdbDictionary::Table newTab(* pTab);
  // Set fragment type for table    
  newTab.setFragmentType((NdbDictionary::Object::FragmentType)fragTtype);
  
  // Try to create table in db
  if (newTab.createTableInDb(pNdb) != 0){
    ndbout << newTab.getName() << " could not be created"
	   << ", fragmentType = "<<fragTtype <<endl;
    ndbout << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  // Verify that table exists in db    
  const NdbDictionary::Table* pTab3 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName()) ;
  if (pTab3 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
    
  }
  
  if (pTab3->getFragmentType() != fragTtype){
    ndbout << pTab->getName() << " fragmentType error "<< endl;
    result = NDBT_FAILED;
    goto drop_the_tab;
  }
/**
   This test does not work since fragmentation is
   decided by the kernel, hence the fragementation
   attribute on the column will differ

  if (newTab.equal(*pTab3) == false){
    ndbout << "It was not equal" << endl;
    result = NDBT_FAILED;
    goto drop_the_tab;
  } 
*/
  do {
    
    HugoTransactions hugoTrans(*pTab3);
    UtilTransactions utilTrans(*pTab3);
    int count;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    // restart all
    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    int timeout = 120;
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    // Verify content
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records, 64) == 0);
    
  } while(false);
  
 drop_the_tab:
  
  // Try to drop table in db
  if (pNdb->getDictionary()->dropTable(pTab3->getName()) != 0){
    ndbout << pTab3->getName()  << " could not be dropped"<< endl;
    result =  NDBT_FAILED;
  }
  
  return result;
}


int runTestTemporaryTables(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int i = 0;
  NdbRestarter restarter;
  
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  
  NdbDictionary::Table newTab(* pTab);
  // Set table as temporary
  newTab.setStoredTable(false);
  
  // Try to create table in db
  if (newTab.createTableInDb(pNdb) != 0){
    return NDBT_FAILED;
  }
  
  // Verify that table is in db     
  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }

  if (pTab2->getStoredTable() != false){
    ndbout << pTab->getName() << " was not temporary in DB"<< endl;
    result = NDBT_FAILED;
    goto drop_the_tab;
  }

  
  while (i < loops && result == NDBT_OK){
    ndbout << i << ": ";

    HugoTransactions hugoTrans(*pTab2);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);

    int count = 0;
    UtilTransactions utilTrans(*pTab2);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);

    // restart all
    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    int timeout = 120;
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    i++;
  }

 drop_the_tab:

   
  if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
    ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
    result = NDBT_FAILED;
  }
  
  // Verify that table is not in db     
  const NdbDictionary::Table* pTab3 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab3 != NULL){
    ndbout << pTab3->getName() << " was found in DB"<< endl;
    result = NDBT_FAILED;
  }
    
  return result;
}

int runPkSizes(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  char tabName[256];
  int minPkSize = 1;
  ndbout << "minPkSize=" <<minPkSize<<endl;
  int maxPkSize = MAX_KEY_SIZE_IN_WORDS * 4;
  ndbout << "maxPkSize=" <<maxPkSize<<endl;
  Ndb* pNdb = GETNDB(step);
  int numRecords = ctx->getNumRecords();

  for (int i = minPkSize; i < maxPkSize; i++){
    BaseString::snprintf(tabName, 256, "TPK_%d", i);

    int records = numRecords;
    int max = ~0;
    // Limit num records for small PKs
    if (i == 1)
      max = 99;
    if (i == 2)
      max = 999;
    if (i == 3)
      max = 9999;
    if (records > max)
      records = max;
    ndbout << "records =" << records << endl;

    if (pNdb->waitUntilReady(30) != 0){
      // Db is not ready, return with failure
      return NDBT_FAILED;
    }
  
    ndbout << "|- " << tabName << endl;

    if (NDBT_Tables::createTable(pNdb, tabName) != 0){
      ndbout << tabName << " could not be created"<< endl;
      return NDBT_FAILED;
    }
    
    // Verify that table exists in db    
    const NdbDictionary::Table* pTab3 = 
      NDBT_Table::discoverTableFromDb(pNdb, tabName) ;
    if (pTab3 == NULL){
      g_err << tabName << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }

    //    ndbout << *pTab3 << endl;

    if (pTab3->equal(*NDBT_Tables::getTable(tabName)) == false){
      g_err << "It was not equal" << endl;
      return NDBT_FAILED;
    }

    do {
      // Do it all
      HugoTransactions hugoTrans(*pTab3);
      UtilTransactions utilTrans(*pTab3);
      int count;
      CHECK(hugoTrans.loadTable(pNdb, records) == 0);
      CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == records);
      CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
      CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == (records/2));
      CHECK(utilTrans.clearTable(pNdb, records) == 0);
      
#if 0
      // Fill table
      CHECK(hugoTrans.fillTable(pNdb) == 0);        
      CHECK(utilTrans.clearTable2(pNdb, records) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == 0);
#endif
    } while(false);

    // Drop table
    if (pNdb->getDictionary()->dropTable(pTab3->getName()) != 0){
      ndbout << "Failed to drop "<<pTab3->getName()<<" in db" << endl;
      return NDBT_FAILED;
    }
  }
  return result;
}

int runStoreFrm(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  const NdbDictionary::Table* pTab = ctx->getTab();
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();

  for (int l = 0; l < loops && result == NDBT_OK ; l++){

    Uint32 dataLen = (Uint32)myRandom48(MAX_FRM_DATA_SIZE);
    // size_t dataLen = 10;
    unsigned char data[MAX_FRM_DATA_SIZE];

    char start = l + 248;
    for(Uint32 i = 0; i < dataLen; i++){
      data[i] = start;
      start++;
    }
#if 0
    ndbout << "dataLen="<<dataLen<<endl;
    for (Uint32 i = 0; i < dataLen; i++){
      unsigned char c = data[i];
      ndbout << hex << c << ", ";
    }
    ndbout << endl;
#endif
        
    NdbDictionary::Table newTab(* pTab);
    void* pData = &data;
    newTab.setFrm(pData, dataLen);
    
    // Try to create table in db
    if (newTab.createTableInDb(pNdb) != 0){
      result = NDBT_FAILED;
      continue;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      g_err << pTab->getName() << " was not found in DB"<< endl;
      result = NDBT_FAILED;
      continue;
    }
    
    const void* pData2 = pTab2->getFrmData();
    Uint32 resultLen = pTab2->getFrmLength();
    if (dataLen != resultLen){
      g_err << "Length of data failure" << endl
	    << " expected = " << dataLen << endl
	    << " got = " << resultLen << endl;
      result = NDBT_FAILED;      
    }
    
    // Verfiy the frm data
    if (memcmp(pData, pData2, resultLen) != 0){
      g_err << "Wrong data received" << endl;
      for (size_t i = 0; i < dataLen; i++){
	unsigned char c = ((unsigned char*)pData2)[i];
	g_err << hex << c << ", ";
      }
      g_err << endl;
      result = NDBT_FAILED;
    }
    
    if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
      g_err << "It can NOT be dropped" << endl;
      result = NDBT_FAILED;
    } 
  }
  
  return result;
}

int runStoreFrmError(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  const NdbDictionary::Table* pTab = ctx->getTab();
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();

  for (int l = 0; l < loops && result == NDBT_OK ; l++){

    const Uint32 dataLen = MAX_FRM_DATA_SIZE + 10;
    unsigned char data[dataLen];

    char start = l + 248;
    for(Uint32 i = 0; i < dataLen; i++){
      data[i] = start;
      start++;
    }
#if 0
    ndbout << "dataLen="<<dataLen<<endl;
    for (Uint32 i = 0; i < dataLen; i++){
      unsigned char c = data[i];
      ndbout << hex << c << ", ";
    }
    ndbout << endl;
#endif

    NdbDictionary::Table newTab(* pTab);
        
    void* pData = &data;
    newTab.setFrm(pData, dataLen);
    
    // Try to create table in db
    if (newTab.createTableInDb(pNdb) == 0){
      result = NDBT_FAILED;
      continue;
    }
    
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 != NULL){
      g_err << pTab->getName() << " was found in DB"<< endl;
      result = NDBT_FAILED;
      if (pNdb->getDictionary()->dropTable(pTab2->getName()) != 0){
	g_err << "It can NOT be dropped" << endl;
	result = NDBT_FAILED;
      } 
      
      continue;
    } 
    
  }

  return result;
}

int verifyTablesAreEqual(const NdbDictionary::Table* pTab, const NdbDictionary::Table* pTab2){
  // Verify that getPrimaryKey only returned true for primary keys
  for (int i = 0; i < pTab2->getNoOfColumns(); i++){
    const NdbDictionary::Column* col = pTab->getColumn(i);
    const NdbDictionary::Column* col2 = pTab2->getColumn(i);
    if (col->getPrimaryKey() != col2->getPrimaryKey()){
      g_err << "col->getPrimaryKey() != col2->getPrimaryKey()" << endl;
      return NDBT_FAILED;
    }
  }
  
  if (!pTab->equal(*pTab2)){
    g_err << "equal failed" << endl;
    g_info << *(NDBT_Table*)pTab; // gcc-4.1.2
    g_info << *(NDBT_Table*)pTab2;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runGetPrimaryKey(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  ndbout << "|- " << pTab->getName() << endl;
  g_info << *(NDBT_Table*)pTab;
  // Try to create table in db
  if (pTab->createTableInDb(pNdb) != 0){
    return NDBT_FAILED;
  }

  const NdbDictionary::Table* pTab2 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab2 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }

  int result = NDBT_OK;
  if (verifyTablesAreEqual(pTab, pTab2) != NDBT_OK)
    result = NDBT_FAILED;
  
  
#if 0
  // Create an index on the table and see what 
  // the function returns now
  char name[200];
  sprintf(name, "%s_X007", pTab->getName());
  NDBT_Index* pInd = new NDBT_Index(name);
  pInd->setTable(pTab->getName());
  pInd->setType(NdbDictionary::Index::UniqueHashIndex);
  //  pInd->setLogging(false);
  for (int i = 0; i < 2; i++){
    const NDBT_Attribute* pAttr = pTab->getAttribute(i);
    pInd->addAttribute(*pAttr);
  }
  g_info << "Create index:" << endl << *pInd;
  if (pInd->createIndexInDb(pNdb, false) != 0){
    result = NDBT_FAILED;
  }  
  delete pInd;

  const NdbDictionary::Table* pTab3 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab3 == NULL){
    ndbout << pTab->getName() << " was not found in DB"<< endl;
    return NDBT_FAILED;
  }

  if (verifyTablesAreEqual(pTab, pTab3) != NDBT_OK)
    result = NDBT_FAILED;
  if (verifyTablesAreEqual(pTab2, pTab3) != NDBT_OK)
    result = NDBT_FAILED;
#endif

#if 0  
  if (pTab2->getDictionary()->dropTable(pNdb) != 0){
    ndbout << "Failed to drop "<<pTab2->getName()<<" in db" << endl;
    return NDBT_FAILED;
  }
  
  // Verify that table is not in db     
  const NdbDictionary::Table* pTab4 = 
    NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
  if (pTab4 != NULL){
    ndbout << pTab4->getName() << " was found in DB"<< endl;
    return NDBT_FAILED;
  }
#endif

  return result;
}

#define APIERROR(error) \
  { g_err << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << endl; \
  }

int
runCreateAutoincrementTable(NDBT_Context* ctx, NDBT_Step* step){

  Uint32 startvalues[5] = {256-2, 0, 256*256-2, ~Uint32(0), 256*256*256-2};

  int ret = NDBT_OK;

  for (int jj = 0; jj < 5 && ret == NDBT_OK; jj++) {
    char tabname[] = "AUTOINCTAB";
    Uint32 startvalue = startvalues[jj];

    NdbDictionary::Table myTable;
    NdbDictionary::Column myColumn;

    Ndb* myNdb = GETNDB(step);
    NdbDictionary::Dictionary* myDict = myNdb->getDictionary();


    if (myDict->getTable(tabname) != NULL) {
      g_err << "NDB already has example table: " << tabname << endl;
      APIERROR(myNdb->getNdbError());
      return NDBT_FAILED;
    }

    myTable.setName(tabname);

    myColumn.setName("ATTR1");
    myColumn.setType(NdbDictionary::Column::Unsigned);
    myColumn.setLength(1);
    myColumn.setPrimaryKey(true);
    myColumn.setNullable(false);
    myColumn.setAutoIncrement(true);
    if (startvalue != ~Uint32(0)) // check that default value starts with 1
      myColumn.setAutoIncrementInitialValue(startvalue);
    myTable.addColumn(myColumn);

    if (myDict->createTable(myTable) == -1) {
      g_err << "Failed to create table " << tabname << endl;
      APIERROR(myNdb->getNdbError());
      return NDBT_FAILED;
    }


    if (startvalue == ~Uint32(0)) // check that default value starts with 1
      startvalue = 1;

    for (int i = 0; i < 16; i++) {

      Uint64 value;
      if (myNdb->getAutoIncrementValue(tabname, value, 1) == -1) {
        g_err << "getAutoIncrementValue failed on " << tabname << endl;
        APIERROR(myNdb->getNdbError());
        return NDBT_FAILED;
      }
      else if (value != (startvalue+i)) {
        g_err << "value = " << value << " expected " << startvalue+i << endl;;
        APIERROR(myNdb->getNdbError());
        //      ret = NDBT_FAILED;
        //      break;
      }
    }

    if (myDict->dropTable(tabname) == -1) {
      g_err << "Failed to drop table " << tabname << endl;
      APIERROR(myNdb->getNdbError());
      ret = NDBT_FAILED;
    }
  }

  return ret;
}

int
runTableRename(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    const NdbDictionary::Table* pTab = ctx->getTab();

    // Try to create table in db
    if (pTab->createTableInDb(pNdb) != 0){
      return NDBT_FAILED;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    // Load table
    HugoTransactions hugoTrans(*ctx->getTab());
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    // Rename table
    BaseString pTabName(pTab->getName());
    BaseString pTabNewName(pTabName);
    pTabNewName.append("xx");
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable = *oldTable;
      newTable.setName(pTabNewName.c_str());
      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableRename failed");
    }
    else {
      result = NDBT_FAILED;
    }
    
    // Verify table contents
    NdbDictionary::Table pNewTab(pTabNewName.c_str());
    
    UtilTransactions utilTrans(pNewTab);
    if (utilTrans.clearTable(pNdb,  records) != 0){
      continue;
    }    

    // Drop table
    dict->dropTable(pNewTab.getName());
  }
 end:

  return result;
}

int
runTableRenameSR(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  if(restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    // Rename table
    const NdbDictionary::Table* pTab = ctx->getTab();

    // Try to create table in db
    if (pTab->createTableInDb(pNdb) != 0){
      return NDBT_FAILED;
    }
    
    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, pTab->getName());
    if (pTab2 == NULL){
      ndbout << pTab->getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    // Load table
    HugoTransactions hugoTrans(*ctx->getTab());
    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    BaseString pTabName(pTab->getName());
    BaseString pTabNewName(pTabName);
    pTabNewName.append("xx");
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable = *oldTable;
      newTable.setName(pTabNewName.c_str());
      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableRename failed");
    }
    else {
      result = NDBT_FAILED;
    }
    
    // Restart cluster
    
    /**
     * Need to run LCP at high rate otherwise
     * packed replicas become "to many"
     */
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    if(restarter.dumpStateAllNodes(&val, 1) != 0){
      do { CHECK(0); } while(0);
      g_err << "Failed to set LCP to min value" << endl;
      return NDBT_FAILED;
    }
    
    CHECK2(restarter.restartAll() == 0,
	   "failed to set restartOneDbNode");
    
    CHECK2(restarter.waitClusterStarted() == 0,
	   "waitClusterStarted failed");
    
    // Verify table contents
    NdbDictionary::Table pNewTab(pTabNewName.c_str());
    
    UtilTransactions utilTrans(pNewTab);
    if (utilTrans.clearTable(pNdb,  records) != 0){
      continue;
    }    

    // Drop table
    dict->dropTable(pTabNewName.c_str());
  }
 end:    
  return result;
}

/*
  Run online alter table add attributes.
 */
int
runTableAddAttrs(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  NdbDictionary::Table myTab= *(ctx->getTab());

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    // Try to create table in db

    if (NDBT_Tables::createTable(pNdb, myTab.getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db     
    const NdbDictionary::Table* pTab2 = 
      NDBT_Table::discoverTableFromDb(pNdb, myTab.getName());
    if (pTab2 == NULL){
      ndbout << myTab.getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

    /*
      Check that table already has a varpart, otherwise add attr is
      not possible.
    */
    if (pTab2->getForceVarPart() == false)
    {
      const NdbDictionary::Column *col;
      for (Uint32 i= 0; (col= pTab2->getColumn(i)) != 0; i++)
      {
        if (col->getStorageType() == NDB_STORAGETYPE_MEMORY &&
            (col->getDynamic() || col->getArrayType() != NDB_ARRAYTYPE_FIXED))
          break;
      }
      if (col == 0)
      {
        /* Alter table add attribute not applicable, just mark success. */
        dict->dropTable(pTab2->getName());
        break;
      }
    }

    // Load table
    HugoTransactions beforeTrans(*ctx->getTab());
    if (beforeTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }

    // Add attributes to table.
    BaseString pTabName(pTab2->getName());
    
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());
    if (oldTable) {
      NdbDictionary::Table newTable= *oldTable;

      NDBT_Attribute newcol1("NEWKOL1", NdbDictionary::Column::Unsigned, 1,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol1);
      NDBT_Attribute newcol2("NEWKOL2", NdbDictionary::Column::Char, 14,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol2);
      NDBT_Attribute newcol3("NEWKOL3", NdbDictionary::Column::Bit, 20,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol3);
      NDBT_Attribute newcol4("NEWKOL4", NdbDictionary::Column::Varbinary, 42,
                            false, true, 0,
                            NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol4);

      CHECK2(dict->alterTable(*oldTable, newTable) == 0,
	     "TableAddAttrs failed");
      /* Need to purge old version and reload new version after alter table. */
      dict->invalidateTable(pTabName.c_str());
    }
    else {
      result = NDBT_FAILED;
    }

    {
      const NdbDictionary::Table* pTab = dict->getTable(pTabName.c_str());
      CHECK2(pTab != NULL, "Table not found");
      HugoTransactions afterTrans(*pTab);

      ndbout << "delete...";
      if (afterTrans.clearTable(pNdb) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "insert...";
      if (afterTrans.loadTable(pNdb, records) != 0){
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "update...";
      if (afterTrans.scanUpdateRecords(pNdb, records) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "delete...";
      if (afterTrans.clearTable(pNdb) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;
    }
    
    // Drop table.
    dict->dropTable(pTabName.c_str());
  }
 end:

  return result;
}

/*
  Run online alter table add attributes while running simultaneous
  transactions on it in separate thread.
 */
int
runTableAddAttrsDuring(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  int abortAlter = ctx->getProperty("AbortAlter", Uint32(0));

  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();
  NdbRestarter res;

  ndbout << "|- " << ctx->getTab()->getName() << endl;  

  NdbDictionary::Table myTab= *(ctx->getTab());

  if (myTab.getForceVarPart() == false)
  {
    const NdbDictionary::Column *col;
    for (Uint32 i= 0; (col= myTab.getColumn(i)) != 0; i++)
    {
      if (col->getStorageType() == NDB_STORAGETYPE_MEMORY &&
          (col->getDynamic() || col->getArrayType() != NDB_ARRAYTYPE_FIXED))
        break;
    }
    if (col == 0)
    {
      ctx->stopTest();
      return NDBT_OK;
    }
  }

  //if 

  for (int l = 0; l < loops && result == NDBT_OK ; l++){
    ndbout << l << ": " << endl;    

    Ndb* pNdb = GETNDB(step);
    NdbDictionary::Dictionary* dict = pNdb->getDictionary();

    /*
      Check that table already has a varpart, otherwise add attr is
      not possible.
    */

    // Add attributes to table.
    ndbout << "Altering table" << endl;
    
    const NdbDictionary::Table * oldTable = dict->getTable(myTab.getName());
    if (oldTable) {
      NdbDictionary::Table newTable= *oldTable;
      
      char name[256];
      BaseString::snprintf(name, sizeof(name), "NEWCOL%d", l);
      NDBT_Attribute newcol1(name, NdbDictionary::Column::Unsigned, 1,
                             false, true, 0,
                             NdbDictionary::Column::StorageTypeMemory, true);
      newTable.addColumn(newcol1);
      //ToDo: check #loops, how many columns l

      if (abortAlter == 0)
      {
        CHECK2(dict->alterTable(*oldTable, newTable) == 0,
               "TableAddAttrsDuring failed");
      }
      else
      {
        int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
        res.insertErrorInNode(nodeId, 4029);
        CHECK2(dict->alterTable(*oldTable, newTable) != 0,
               "TableAddAttrsDuring failed");
      }

      dict->invalidateTable(myTab.getName());
      const NdbDictionary::Table * newTab = dict->getTable(myTab.getName());
      CHECK2(newTab != NULL, "'newTab' not found");
      HugoTransactions hugoTrans(* newTab);
      hugoTrans.scanUpdateRecords(pNdb, records);
    }
    else {
      result= NDBT_FAILED;
      break;
    }
  }
 end:

  ctx->stopTest();

  return result;
}

static void
f(const NdbDictionary::Column * col){
  if(col == 0){
    abort();
  }
}

int
runTestDictionaryPerf(NDBT_Context* ctx, NDBT_Step* step){
  Vector<char*> cols;
  Vector<const NdbDictionary::Table*> tabs;
  int i;

  Ndb* pNdb = GETNDB(step);  

  const Uint32 count = NDBT_Tables::getNumTables();
  for (i=0; i < (int)count; i++){
    const NdbDictionary::Table * tab = NDBT_Tables::getTable(i);
    pNdb->getDictionary()->createTable(* tab);
    
    const NdbDictionary::Table * tab2 = pNdb->getDictionary()->getTable(tab->getName());
    
    for(int j = 0; j<tab->getNoOfColumns(); j++){
      cols.push_back((char*)tab2);
      cols.push_back(strdup(tab->getColumn(j)->getName()));
    }
  }

  const Uint32 times = 10000000;

  ndbout_c("%d tables and %d columns", 
	   NDBT_Tables::getNumTables(), cols.size()/2);

  char ** tcols = cols.getBase();

  srand((unsigned int)time(0));
  Uint32 size = cols.size() / 2;
  //char ** columns = &cols[0];
  Uint64 start = NdbTick_CurrentMillisecond();
  for(i = 0; i<(int)times; i++){
    int j = 2 * (rand() % size);
    const NdbDictionary::Table* tab = (const NdbDictionary::Table*)tcols[j];
    const char * col = tcols[j+1];
    const NdbDictionary::Column* column = tab->getColumn(col);
    f(column);
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  stop -= start;

  Uint64 per = stop;
  per *= 1000;
  per /= times;
  
  ndbout_c("%d random getColumn(name) in %lld ms -> %u us/get",
	   times, stop, Uint32(per));

  return NDBT_OK;
}

int
runCreateLogfileGroup(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::LogfileGroup lg;
  lg.setName("DEFAULT-LG");
  lg.setUndoBufferSize(8*1024*1024);
  
  int res;
  res = pNdb->getDictionary()->createLogfileGroup(lg);
  if(res != 0){
    g_err << "Failed to create logfilegroup:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Undofile uf;
  uf.setPath("undofile01.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");
  
  res = pNdb->getDictionary()->createUndofile(uf);
  if(res != 0){
    g_err << "Failed to create undofile:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  uf.setPath("undofile02.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");
  
  res = pNdb->getDictionary()->createUndofile(uf);
  if(res != 0){
    g_err << "Failed to create undofile:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runCreateTablespace(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::Tablespace lg;
  lg.setName("DEFAULT-TS");
  lg.setExtentSize(1024*1024);
  lg.setDefaultLogfileGroup("DEFAULT-LG");

  int res;
  res = pNdb->getDictionary()->createTablespace(lg);
  if(res != 0){
    g_err << "Failed to create tablespace:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Datafile uf;
  uf.setPath("datafile01.dat");
  uf.setSize(10*1024*1024);
  uf.setTablespace("DEFAULT-TS");

  res = pNdb->getDictionary()->createDatafile(uf);
  if(res != 0){
    g_err << "Failed to create datafile:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}
int
runCreateDiskTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);  

  NdbDictionary::Table tab = *ctx->getTab();
  tab.setTablespaceName("DEFAULT-TS");
  
  for(Uint32 i = 0; i<(Uint32)tab.getNoOfColumns(); i++)
    if(!tab.getColumn(i)->getPrimaryKey())
      tab.getColumn(i)->setStorageType(NdbDictionary::Column::StorageTypeDisk);
  
  int res;
  res = pNdb->getDictionary()->createTable(tab);
  if(res != 0){
    g_err << "Failed to create table:"
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int getColumnMaxLength(const NdbDictionary::Column* c)
{
  int length= c->getLength();
  if (c->getArrayType() == NDB_ARRAYTYPE_FIXED)
  {
    /* Not yet set - need to calculate from type etc. */
    DictTabInfo::Attribute attrDesc;

    attrDesc.init();
    attrDesc.AttributeExtType= c->getType();
    attrDesc.AttributeExtLength= c->getLength();
    attrDesc.AttributeExtPrecision= c->getPrecision();
    attrDesc.AttributeExtScale= c->getScale();

    if (!attrDesc.translateExtType())
    {
      return 0;
    }

    if (attrDesc.AttributeSize == 0)
    {
      // bits...
      length = 4 * ((c->getLength() + 31) / 32);
    }
    else
    {
      length = ((1 << attrDesc.AttributeSize) * c->getLength()) >> 3;
    }
  }

  return length;
}

#include <NDBT_Tables.hpp>

#define SAFTY 300

int runFailAddFragment(NDBT_Context* ctx, NDBT_Step* step){
  static int acclst[] = { 3001, 6200, 6202 };
  static int tuplst[] = { 4007, 4008, 4009, 4010, 4032, 4033, 4034 };
  static int tuxlst[] = { 12001, 12002, 12003, 12004, 
                          6201, 6203 };
  static unsigned acccnt = sizeof(acclst)/sizeof(acclst[0]);
  static unsigned tupcnt = sizeof(tuplst)/sizeof(tuplst[0]);
  static unsigned tuxcnt = sizeof(tuxlst)/sizeof(tuxlst[0]);

  NdbRestarter restarter;
  int nodeId = restarter.getMasterNodeId();
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbDictionary::Table tab(*ctx->getTab());
  tab.setFragmentType(NdbDictionary::Object::FragAllLarge);

  int errNo = 0;
#ifdef NDB_USE_GET_ENV
  char buf[100];
  if (NdbEnv_GetEnv("ERRNO", buf, sizeof(buf)))
  {
    errNo = atoi(buf);
    ndbout_c("Using errno: %u", errNo);
  }
#endif
  const NdbDictionary::Table* origTab= ctx->getTab();
  HugoCalculator calc(*origTab);

  // Add defaults to some columns
  for (int colNum= 0; colNum < tab.getNoOfColumns(); colNum++)
  {
    const NdbDictionary::Column* origCol= origTab->getColumn(colNum);
    NdbDictionary::Column* col= tab.getColumn(colNum);
    if (!origCol->getPrimaryKey())
    {
      if (myRandom48(2) == 0)
      {
        char defaultBuf[ NDB_MAX_TUPLE_SIZE ];
        Uint32 real_len;
        Uint32 updatesVal = myRandom48(1 << 16);
        const char* def= calc.calcValue(0, colNum, updatesVal, 
                                        defaultBuf,
                                        getColumnMaxLength(origCol),
                                        &real_len);
        if (col->setDefaultValue(def, real_len) != 0)
        {
          ndbout_c("Error setting default value\n");
          return NDBT_FAILED;
        }
        NdbDictionary::NdbDataPrintFormat dpf;
        ndbout << "Set default for column " << origCol->getName()
               << " to ";
        
        NdbDictionary::printFormattedValue(ndbout,
                                           dpf,
                                           col,
                                           def);
        ndbout << endl;
      }
    }
  }

  // ordered index on first few columns
  NdbDictionary::Index idx("X");
  idx.setTable(tab.getName());
  idx.setType(NdbDictionary::Index::OrderedIndex);
  idx.setLogging(false);
  for (int cnt = 0, i_hate_broken_compilers = 0;
       cnt < 3 &&
       i_hate_broken_compilers < tab.getNoOfColumns();
       i_hate_broken_compilers++) {
    if (NdbSqlUtil::check_column_for_ordered_index
        (tab.getColumn(i_hate_broken_compilers)->getType(), 0) == 0 &&
        tab.getColumn(i_hate_broken_compilers)->getStorageType() != 
        NdbDictionary::Column::StorageTypeDisk)
    {
      idx.addColumn(*tab.getColumn(i_hate_broken_compilers));
      cnt++;
    }
  }

  for (Uint32 i = 0; i<(Uint32)tab.getNoOfColumns(); i++)
  {
    if (tab.getColumn(i)->getStorageType() == 
        NdbDictionary::Column::StorageTypeDisk)
    {
      NDBT_Tables::create_default_tablespace(pNdb);
      break;
    }
  }

  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  (void)pDic->dropTable(tab.getName());

  int dump1 = DumpStateOrd::SchemaResourceSnapshot;
  int dump2 = DumpStateOrd::SchemaResourceCheckLeak;

  for (int l = 0; l < loops; l++) {
    for (unsigned i0 = 0; i0 < acccnt; i0++) {
      unsigned j = (l == 0 ? i0 : myRandom48(acccnt));
      int errval = acclst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_err << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived
      CHECK2(pDic->createTable(tab) != 0,
             "failed to fail after error insert " << errval);
      CHECK2(restarter.insertErrorInNode(nodeId, 0) == 0,
             "failed to clean error insert value");
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived
      CHECK2(pDic->createTable(tab) == 0,
             pDic->getNdbError());
      CHECK2(pDic->dropTable(tab.getName()) == 0,
             pDic->getNdbError());
    }
    for (unsigned i1 = 0; i1 < tupcnt; i1++) {
      unsigned j = (l == 0 ? i1 : myRandom48(tupcnt));
      int errval = tuplst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_err << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived
      CHECK2(pDic->createTable(tab) != 0,
             "failed to fail after error insert " << errval);
      CHECK2(restarter.insertErrorInNode(nodeId, 0) == 0,
             "failed to clean error insert value");
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived
      CHECK2(pDic->createTable(tab) == 0,
             pDic->getNdbError());
      CHECK2(pDic->dropTable(tab.getName()) == 0,
             pDic->getNdbError());
    }
    for (unsigned i2 = 0; i2 < tuxcnt; i2++) {
      unsigned j = (l == 0 ? i2 : myRandom48(tuxcnt));
      int errval = tuxlst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      CHECK2(pDic->createTable(tab) == 0,
             pDic->getNdbError());

      g_err << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived

      CHECK2(pDic->createIndex(idx) != 0,
             "failed to fail after error insert " << errval);
      CHECK2(restarter.insertErrorInNode(nodeId, 0) == 0,
             "failed to clean error insert value");
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived
      CHECK2(pDic->createIndex(idx) == 0,
             pDic->getNdbError());
      CHECK2(pDic->dropTable(tab.getName()) == 0,
             pDic->getNdbError());
    }
  }
end:
  return result;
}

// NFNR

// Restarter controls dict ops : 1-run 2-pause 3-stop
// synced by polling...

static bool
send_dict_ops_cmd(NDBT_Context* ctx, Uint32 cmd)
{
  ctx->setProperty("DictOps_CMD", cmd);
  while (1) {
    if (ctx->isTestStopped())
      return false;
    if (ctx->getProperty("DictOps_ACK") == cmd)
      break;
    NdbSleep_MilliSleep(100);
  }
  return true;
}

static bool
recv_dict_ops_run(NDBT_Context* ctx)
{
  while (1) {
    if (ctx->isTestStopped())
      return false;
    Uint32 cmd = ctx->getProperty("DictOps_CMD");
    ctx->setProperty("DictOps_ACK", cmd);
    if (cmd == 1)
      break;
    if (cmd == 3)
      return false;
    NdbSleep_MilliSleep(100);
  }
  return true;
}

int
runRestarts(NDBT_Context* ctx, NDBT_Step* step)
{
  static int errlst_master[] = {   // non-crashing
    7175,       // send one fake START_PERMREF
    0 
  };
  static int errlst_node[] = {
    7174,       // crash before sending DICT_LOCK_REQ
    7176,       // pretend master does not support DICT lock
    7121,       // crash at receive START_PERMCONF
    0
  };
  const uint errcnt_master = sizeof(errlst_master)/sizeof(errlst_master[0]);
  const uint errcnt_node = sizeof(errlst_node)/sizeof(errlst_node[0]);

  myRandom48Init((long)NdbTick_CurrentMillisecond());
  NdbRestarter restarter;
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();

  for (int l = 0; l < loops && result == NDBT_OK; l++) {
    g_info << "1: === loop " << l << " ===" << endl;

    // assuming 2-way replicated

    int numnodes = restarter.getNumDbNodes();
    CHECK(numnodes >= 1);
    if (numnodes == 1)
      break;

    int masterNodeId = restarter.getMasterNodeId();
    CHECK(masterNodeId != -1);

    // for more complex cases need more restarter support methods

    int nodeIdList[2] = { 0, 0 };
    int nodeIdCnt = 0;

    if (numnodes >= 2) {
      int rand = myRandom48(numnodes);
      int nodeId = restarter.getRandomNotMasterNodeId(rand);
      CHECK(nodeId != -1);
      nodeIdList[nodeIdCnt++] = nodeId;
    }

    if (numnodes >= 4 && myRandom48(2) == 0) {
      int rand = myRandom48(numnodes);
      int nodeId = restarter.getRandomNodeOtherNodeGroup(nodeIdList[0], rand);
      CHECK(nodeId != -1);
      if (nodeId != masterNodeId)
        nodeIdList[nodeIdCnt++] = nodeId;
    }

    g_info << "1: master=" << masterNodeId << " nodes=" << nodeIdList[0] << "," << nodeIdList[1] << endl;

    const uint timeout = 60; //secs for node wait
    const unsigned maxsleep = 2000; //ms

    bool NF_ops = ctx->getProperty("Restart_NF_ops");
    uint NF_type = ctx->getProperty("Restart_NF_type");
    bool NR_ops = ctx->getProperty("Restart_NR_ops");
    bool NR_error = ctx->getProperty("Restart_NR_error");

    g_info << "1: " << (NF_ops ? "run" : "pause") << " dict ops" << endl;
    if (! send_dict_ops_cmd(ctx, NF_ops ? 1 : 2))
      break;
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    {
      for (int i = 0; i < nodeIdCnt; i++) {
        int nodeId = nodeIdList[i];

        bool nostart = true;
        bool abort = NF_type == 0 ? myRandom48(2) : (NF_type == 2);
        bool initial = myRandom48(2);

        char flags[40];
        strcpy(flags, "flags: nostart");
        if (abort)
          strcat(flags, ",abort");
        if (initial)
          strcat(flags, ",initial");

        g_info << "1: restart " << nodeId << " " << flags << endl;
        CHECK(restarter.restartOneDbNode(nodeId, initial, nostart, abort) == 0);
      }
    }

    g_info << "1: wait for nostart" << endl;
    CHECK(restarter.waitNodesNoStart(nodeIdList, nodeIdCnt, timeout) == 0);
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    int err_master = 0;
    int err_node[2] = { 0, 0 };

    if (NR_error) {
      err_master = errlst_master[l % errcnt_master];

      // limitation: cannot have 2 node restarts and crash_insert
      // one node may die for real (NF during startup)

      for (int i = 0; i < nodeIdCnt && nodeIdCnt == 1; i++) {
        err_node[i] = errlst_node[l % errcnt_node];

        // 7176 - no DICT lock protection

        if (err_node[i] == 7176) {
          g_info << "1: no dict ops due to error insert "
                 << err_node[i] << endl;
          NR_ops = false;
        }
      }
    }

    g_info << "1: " << (NR_ops ? "run" : "pause") << " dict ops" << endl;
    if (! send_dict_ops_cmd(ctx, NR_ops ? 1 : 2))
      break;
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "1: start nodes" << endl;
    CHECK(restarter.startNodes(nodeIdList, nodeIdCnt) == 0);

    if (NR_error) {
      {
        int err = err_master;
        if (err != 0) {
          g_info << "1: insert master error " << err << endl;
          CHECK(restarter.insertErrorInNode(masterNodeId, err) == 0);
        }
      }

      for (int i = 0; i < nodeIdCnt; i++) {
        int nodeId = nodeIdList[i];

        int err = err_node[i];
        if (err != 0) {
          g_info << "1: insert node " << nodeId << " error " << err << endl;
          CHECK(restarter.insertErrorInNode(nodeId, err) == 0);
        }
      }
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "1: wait cluster started" << endl;
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "1: restart done" << endl;
  }

  g_info << "1: stop dict ops" << endl;
  send_dict_ops_cmd(ctx, 3);

  return result;
}

int
runDictOps(NDBT_Context* ctx, NDBT_Step* step)
{
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  int result = NDBT_OK;

  for (int l = 0; result == NDBT_OK; l++) {
    if (! recv_dict_ops_run(ctx))
      break;
    
    g_info << "2: === loop " << l << " ===" << endl;

    Ndb* pNdb = GETNDB(step);
    NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
    const NdbDictionary::Table* pTab = ctx->getTab();
    //const char* tabName = pTab->getName(); //XXX what goes on?
    char tabName[40];
    strcpy(tabName, pTab->getName());

    const unsigned long maxsleep = 100; //ms

    g_info << "2: create table" << endl;
    {
      uint count = 0;
    try_create:
      count++;
      if (pDic->createTable(*pTab) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << tabName << ": create failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_create;
      }
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "2: verify create" << endl;
    const NdbDictionary::Table* pTab2 = pDic->getTable(tabName);
    if (pTab2 == NULL) {
      const NdbError err = pDic->getNdbError();
      g_err << "2: " << tabName << ": verify create: " << err << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    // replace by the Retrieved table
    pTab = pTab2;

    // create indexes
    const char** indlist = NDBT_Tables::getIndexes(tabName);
    uint indnum = 0;
    while (indlist != 0 && *indlist != 0) {
      uint count = 0;
    try_create_index:
      count++;
      if (count == 1)
        g_info << "2: create index " << indnum << " " << *indlist << endl;
      NdbDictionary::Index ind;
      char indName[200];
      sprintf(indName, "%s_X%u", tabName, indnum);
      ind.setName(indName);
      ind.setTable(tabName);
      if (strcmp(*indlist, "UNIQUE") == 0) {
        ind.setType(NdbDictionary::Index::UniqueHashIndex);
        ind.setLogging(pTab->getLogging());
      } else if (strcmp(*indlist, "ORDERED") == 0) {
        ind.setType(NdbDictionary::Index::OrderedIndex);
        ind.setLogging(false);
      } else {
        require(false);
      }
      const char** indtemp = indlist;
      while (*++indtemp != 0) {
        ind.addColumn(*indtemp);
      }
      if (pDic->createIndex(ind) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << indName << ": create failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_create_index;
      }
      indlist = ++indtemp;
      indnum++;
    }
    if (result == NDBT_FAILED)
      break;

    uint indcount = indnum;

    int records = myRandom48(ctx->getNumRecords());
    g_info << "2: load " << records << " records" << endl;
    HugoTransactions hugoTrans(*pTab);
    if (hugoTrans.loadTable(pNdb, records) != 0) {
      // XXX get error code from hugo
      g_err << "2: " << tabName << ": load failed" << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    // drop indexes
    indnum = 0;
    while (indnum < indcount) {
      uint count = 0;
    try_drop_index:
      count++;
      if (count == 1)
        g_info << "2: drop index " << indnum << endl;
      char indName[200];
      sprintf(indName, "%s_X%u", tabName, indnum);
      if (pDic->dropIndex(indName, tabName) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << indName << ": drop failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_drop_index;
      }
      indnum++;
    }
    if (result == NDBT_FAILED)
      break;

    g_info << "2: drop" << endl;
    {
      uint count = 0;
    try_drop:
      count++;
      if (pDic->dropTable(tabName) != 0) {
        const NdbError err = pDic->getNdbError();
        if (count == 1)
          g_err << "2: " << tabName << ": drop failed: " << err << endl;
        if (err.code != 711) {
          result = NDBT_FAILED;
          break;
        }
        NdbSleep_MilliSleep(myRandom48(maxsleep));
        goto try_drop;
      }
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));

    g_info << "2: verify drop" << endl;
    const NdbDictionary::Table* pTab3 = pDic->getTable(tabName);
    if (pTab3 != NULL) {
      g_err << "2: " << tabName << ": verify drop: table exists" << endl;
      result = NDBT_FAILED;
      break;
    }
    if (pDic->getNdbError().code != 709 &&
        pDic->getNdbError().code != 723) {
      const NdbError err = pDic->getNdbError();
      g_err << "2: " << tabName << ": verify drop: " << err << endl;
      result = NDBT_FAILED;
      break;
    }
    NdbSleep_MilliSleep(myRandom48(maxsleep));
  }

  return result;
}

int
runBug21755(NDBT_Context* ctx, NDBT_Step* step)
{
  char buf[256];
  NdbRestarter res;
  NdbDictionary::Table pTab0 = * ctx->getTab();
  NdbDictionary::Table pTab1 = pTab0;

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  
  if (pDic->createTable(pTab0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Index idx0;
  BaseString::snprintf(buf, sizeof(buf), "%s-idx", pTab0.getName());  
  idx0.setName(buf);
  idx0.setType(NdbDictionary::Index::OrderedIndex);
  idx0.setTable(pTab0.getName());
  idx0.setStoredIndex(false);
  for (Uint32 i = 0; i<(Uint32)pTab0.getNoOfColumns(); i++)
  {
    const NdbDictionary::Column * col = pTab0.getColumn(i);
    if(col->getPrimaryKey()){
      idx0.addIndexColumn(col->getName());
    }
  }
  
  if (pDic->createIndex(idx0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  BaseString::snprintf(buf, sizeof(buf), "%s-2", pTab1.getName());
  pTab1.setName(buf);

  if (pDic->createTable(pTab1))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  {
    const NdbDictionary::Table* pTab = pDic->getTable(pTab0.getName());
    if (pTab == NULL) {
      g_err << "Table 'pTab0': " << pTab0.getName()
            << ", not found on line " << __LINE__
            <<", error: " << pDic->getNdbError()
            << endl;
      return NDBT_FAILED;
    }
    HugoTransactions t0 (*pTab);
    t0.loadTable(pNdb, 1000);
  }

  {
    const NdbDictionary::Table* pTab = pDic->getTable(pTab1.getName());
    if (pTab == NULL) {
      g_err << "Table 'pTab1': " << pTab1.getName()
            << ", not found on line " << __LINE__
            <<", error: " << pDic->getNdbError()
            << endl;
      return NDBT_FAILED;
    }
    HugoTransactions t1 (*pTab);
    t1.loadTable(pNdb, 1000);
  }
  
  int node = res.getRandomNotMasterNodeId(rand());
  res.restartOneDbNode(node, false, true, true);
  
  if (pDic->dropTable(pTab1.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  BaseString::snprintf(buf, sizeof(buf), "%s-idx2", pTab0.getName());    
  idx0.setName(buf);
  if (pDic->createIndex(idx0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  res.waitNodesNoStart(&node, 1);
  res.startNodes(&node, 1);
  
  if (res.waitClusterStarted())
  {
    return NDBT_FAILED;
  }
  
  if (pDic->dropTable(pTab0.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

static
int
create_tablespace(NdbDictionary::Dictionary* pDict, 
                  const char * lgname, 
                  const char * tsname, 
                  const char * dfname)
{
  NdbDictionary::Tablespace ts;
  ts.setName(tsname);
  ts.setExtentSize(1024*1024);
  ts.setDefaultLogfileGroup(lgname);
  
  if(pDict->createTablespace(ts) != 0)
  {
    g_err << "Failed to create tablespace:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  NdbDictionary::Datafile df;
  df.setPath(dfname);
  df.setSize(1*1024*1024);
  df.setTablespace(tsname);
  
  if(pDict->createDatafile(df) != 0)
  {
    g_err << "Failed to create datafile:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  return 0;
}

int
runBug24631(NDBT_Context* ctx, NDBT_Step* step)
{
  char tsname[256];
  char dfname[256];
  char lgname[256];
  char ufname[256];
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  
  NdbDictionary::Dictionary::List list;
  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;
  
  const char * lgfound = 0;
  
  for (Uint32 i = 0; i<list.count; i++)
  {
    switch(list.elements[i].type){
    case NdbDictionary::Object::LogfileGroup:
      lgfound = list.elements[i].name;
      break;
    default:
      break;
    }
    if (lgfound)
      break;
  }

  if (lgfound == 0)
  {
    BaseString::snprintf(lgname, sizeof(lgname), "LG-%u", rand());
    NdbDictionary::LogfileGroup lg;
    
    lg.setName(lgname);
    lg.setUndoBufferSize(8*1024*1024);
    if(pDict->createLogfileGroup(lg) != 0)
    {
      g_err << "Failed to create logfilegroup:"
	    << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }

    NdbDictionary::Undofile uf;
    BaseString::snprintf(ufname, sizeof(ufname), "%s-%u", lgname, rand());
    uf.setPath(ufname);
    uf.setSize(2*1024*1024);
    uf.setLogfileGroup(lgname);
    
    if(pDict->createUndofile(uf) != 0)
    {
      g_err << "Failed to create undofile:"
            << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    BaseString::snprintf(lgname, sizeof(lgname), "%s", lgfound);
  }

  BaseString::snprintf(tsname, sizeof(tsname), "TS-%u", rand());
  BaseString::snprintf(dfname, sizeof(dfname), "%s-%u.dat", tsname, rand());

  if (create_tablespace(pDict, lgname, tsname, dfname))
    return NDBT_FAILED;

  
  int node = res.getRandomNotMasterNodeId(rand());
  res.restartOneDbNode(node, false, true, true);
  NdbSleep_SecSleep(3);

  if (pDict->dropDatafile(pDict->getDatafile(0, dfname)) != 0)
  {
    g_err << "Failed to drop datafile: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDict->dropTablespace(pDict->getTablespace(tsname)) != 0)
  {
    g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (res.waitNodesNoStart(&node, 1))
    return NDBT_FAILED;
  
  res.startNodes(&node, 1);
  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  if (create_tablespace(pDict, lgname, tsname, dfname))
    return NDBT_FAILED;

  if (pDict->dropDatafile(pDict->getDatafile(0, dfname)) != 0)
  {
    g_err << "Failed to drop datafile: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDict->dropTablespace(pDict->getTablespace(tsname)) != 0)
  {
    g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  if (lgfound == 0)
  {
    if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lgname)) != 0)
      return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runBug29186(NDBT_Context* ctx, NDBT_Step* step)
{
  int lgError = 15000;
  int tsError = 16000;
  char lgname[256];
  char ufname[256];
  char tsname[256];
  char dfname[256];

  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  NdbDictionary::Dictionary::List list;

  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;

  // 1.create logfile group
  const char * lgfound = 0;

  for (Uint32 i = 0; i<list.count; i++)
  {
    switch(list.elements[i].type){
    case NdbDictionary::Object::LogfileGroup:
      lgfound = list.elements[i].name;
      break;
    default:
      break;
    }
    if (lgfound)
      break;
  }

  if (lgfound == 0)
  {
    BaseString::snprintf(lgname, sizeof(lgname), "LG-%u", rand());
    NdbDictionary::LogfileGroup lg;

    lg.setName(lgname);
    lg.setUndoBufferSize(8*1024*1024);
    if(pDict->createLogfileGroup(lg) != 0)
    {
      g_err << "Failed to create logfilegroup:"
            << endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    BaseString::snprintf(lgname, sizeof(lgname), "%s", lgfound);
  }

  if(restarter.waitClusterStarted(60)){
    g_err << "waitClusterStarted failed"<< endl;
    return NDBT_FAILED;
  }
 
  if(restarter.insertErrorInAllNodes(lgError) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }

  g_info << "error inserted"  << endl;
  g_info << "waiting some before add log file"  << endl;
  g_info << "starting create log file group"  << endl;

  NdbDictionary::Undofile uf;
  BaseString::snprintf(ufname, sizeof(ufname), "%s-%u", lgname, rand());
  uf.setPath(ufname);
  uf.setSize(2*1024*1024);
  uf.setLogfileGroup(lgname);

  if(pDict->createUndofile(uf) == 0)
  {
    g_err << "Create log file group should fail on error_insertion " << lgError << endl;
    return NDBT_FAILED;
  }

  //clear lg error
  if(restarter.insertErrorInAllNodes(15099) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(5);

  //lg error has been cleared, so we can add undo file
  if(pDict->createUndofile(uf) != 0)
  {
    g_err << "Failed to create undofile:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if(restarter.waitClusterStarted(60)){
    g_err << "waitClusterStarted failed"<< endl;
    return NDBT_FAILED;
  }

  if(restarter.insertErrorInAllNodes(tsError) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  g_info << "error inserted"  << endl;
  g_info << "waiting some before create table space"  << endl;
  g_info << "starting create table space"  << endl;

  //r = runCreateTablespace(ctx, step);
  BaseString::snprintf(tsname,  sizeof(tsname), "TS-%u", rand());
  BaseString::snprintf(dfname, sizeof(dfname), "%s-%u-1.dat", tsname, rand());

  NdbDictionary::Tablespace ts;
  ts.setName(tsname);
  ts.setExtentSize(1024*1024);
  ts.setDefaultLogfileGroup(lgname);

  if(pDict->createTablespace(ts) != 0)
  {
    g_err << "Failed to create tablespace:"
          << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Datafile df;
  df.setPath(dfname);
  df.setSize(1*1024*1024);
  df.setTablespace(tsname);

  if(pDict->createDatafile(df) == 0)
  {
    g_err << "Create table space should fail on error_insertion " << tsError << endl;
    return NDBT_FAILED;
  }
  //Clear the inserted error
  if(restarter.insertErrorInAllNodes(16099) != 0){
    g_err << "failed to set error insert"<< endl;
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(5);

  if (pDict->dropTablespace(pDict->getTablespace(tsname)) != 0)
  {
    g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (lgfound == 0)
  {
    if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lgname)) != 0)
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

struct RandSchemaOp
{
  RandSchemaOp(unsigned * randseed = 0) {
    if (randseed == 0)
    {
      ownseed = (unsigned)NdbTick_CurrentMillisecond();
      seed = &ownseed;
    }
    else
    {
      seed = randseed;
    }
  }
  struct Obj 
  { 
    BaseString m_name;
    Uint32 m_type;
    struct Obj* m_parent;
    Vector<Obj*> m_dependant;
  };

  Vector<Obj*> m_objects;

  int schema_op(Ndb*);
  int validate(Ndb*);
  int cleanup(Ndb*);

  Obj* get_obj(Uint32 mask);
  int create_table(Ndb*);
  int create_index(Ndb*, Obj*);
  int alter_table(Ndb*, Obj*);
  int drop_obj(Ndb*, Obj*);

  void remove_obj(Obj*);
private:
  unsigned * seed;
  unsigned ownseed;
};

template class Vector<RandSchemaOp::Obj*>;

int
RandSchemaOp::schema_op(Ndb* ndb)
{
  struct Obj* obj = 0;
  Uint32 type = 0;
loop:
  switch(ndb_rand_r(seed) % 5){
  case 0:
    return create_table(ndb);
  case 1:
    if ((obj = get_obj(1 << NdbDictionary::Object::UserTable)) == 0)
      goto loop;
    return create_index(ndb, obj);
  case 2:
    type = (1 << NdbDictionary::Object::UserTable);
    goto drop_object;
  case 3:
    type = 
      (1 << NdbDictionary::Object::UniqueHashIndex) |
      (1 << NdbDictionary::Object::OrderedIndex);    
    goto drop_object;
  case 4:
    if ((obj = get_obj(1 << NdbDictionary::Object::UserTable)) == 0)
      goto loop;
    return alter_table(ndb, obj);
  default:
    goto loop;
  }

drop_object:
  if ((obj = get_obj(type)) == 0)
    goto loop;
  return drop_obj(ndb, obj);
}

RandSchemaOp::Obj*
RandSchemaOp::get_obj(Uint32 mask)
{
  Vector<Obj*> tmp;
  for (Uint32 i = 0; i<m_objects.size(); i++)
  {
    if ((1 << m_objects[i]->m_type) & mask)
      tmp.push_back(m_objects[i]);
  }

  if (tmp.size())
  {
    return tmp[ndb_rand_r(seed)%tmp.size()];
  }
  return 0;
}

int
RandSchemaOp::create_table(Ndb* ndb)
{
  int numTables = NDBT_Tables::getNumTables();
  int num = ndb_rand_r(seed) % numTables;
  NdbDictionary::Table pTab = * NDBT_Tables::getTable(num);
  
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  pTab.setForceVarPart(true);

  if (pDict->getTable(pTab.getName()))
  {
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "%s-%d", 
                         pTab.getName(), ndb_rand_r(seed));
    pTab.setName(buf);
    if (pDict->createTable(pTab))
      return NDBT_FAILED;
  }
  else
  {
    if (NDBT_Tables::createTable(ndb, pTab.getName()))
    {
      return NDBT_FAILED;
    }
  }

  ndbout_c("create table %s",  pTab.getName());
  const NdbDictionary::Table* tab2 = pDict->getTable(pTab.getName());
  if (tab2 == NULL) {
    g_err << "Table : " << pTab.getName()
          << ", not found on line " << __LINE__
          <<", error: " << pDict->getNdbError()
          << endl;
    return NDBT_FAILED;
  }
  HugoTransactions trans(*tab2);
  trans.loadTable(ndb, 1000);

  Obj *obj = new Obj;
  obj->m_name.assign(pTab.getName());
  obj->m_type = NdbDictionary::Object::UserTable;
  obj->m_parent = 0;
  m_objects.push_back(obj);
  
  return NDBT_OK;
}

int
RandSchemaOp::create_index(Ndb* ndb, Obj* tab)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  const NdbDictionary::Table * pTab = pDict->getTable(tab->m_name.c_str());

  if (pTab == 0)
  {
    return NDBT_FAILED;
  }

  bool ordered = ndb_rand_r(seed) & 1;
  bool stored = ndb_rand_r(seed) & 1;

  Uint32 type = ordered ? 
    NdbDictionary::Index::OrderedIndex :
    NdbDictionary::Index::UniqueHashIndex;
  
  char buf[255];
  BaseString::snprintf(buf, sizeof(buf), "%s-%s", 
                       pTab->getName(),
                       ordered ? "OI" : "UI");
  
  if (pDict->getIndex(buf, pTab->getName()))
  {
    // Index exists...let it be ok
    return NDBT_OK;
  }
  
  ndbout_c("create index %s", buf);
  NdbDictionary::Index idx0;
  idx0.setName(buf);
  idx0.setType((NdbDictionary::Index::Type)type);
  idx0.setTable(pTab->getName());
  idx0.setStoredIndex(ordered ? false : stored);

  for (Uint32 i = 0; i<(Uint32)pTab->getNoOfColumns(); i++)
  {
    if (pTab->getColumn(i)->getPrimaryKey())
      idx0.addColumn(pTab->getColumn(i)->getName());
  }
  if (pDict->createIndex(idx0))
  {
    ndbout << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  Obj *obj = new Obj;
  obj->m_name.assign(buf);
  obj->m_type = type;
  obj->m_parent = tab;
  m_objects.push_back(obj);
  
  tab->m_dependant.push_back(obj);
  return NDBT_OK;
}

int
RandSchemaOp::drop_obj(Ndb* ndb, Obj* obj)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  
  if (obj->m_type == NdbDictionary::Object::UserTable)
  {
    ndbout_c("drop table %s", obj->m_name.c_str());
    /**
     * Drop of table automatically drops all indexes
     */
    if (pDict->dropTable(obj->m_name.c_str()))
    {
      return NDBT_FAILED;
    }
    while(obj->m_dependant.size())
    {
      remove_obj(obj->m_dependant[0]);
    }
    remove_obj(obj);
  }
  else if (obj->m_type == NdbDictionary::Object::UniqueHashIndex ||
           obj->m_type == NdbDictionary::Object::OrderedIndex)
  {
    ndbout_c("drop index %s", obj->m_name.c_str());
    if (pDict->dropIndex(obj->m_name.c_str(),
                         obj->m_parent->m_name.c_str()))
    {
      return NDBT_FAILED;
    }
    remove_obj(obj);
  }
  return NDBT_OK;
}

void
RandSchemaOp::remove_obj(Obj* obj)
{
  Uint32 i;
  if (obj->m_parent)
  {
    bool found = false;
    for (i = 0; i<obj->m_parent->m_dependant.size(); i++)
    {
      if (obj->m_parent->m_dependant[i] == obj)
      {
        found = true;
        obj->m_parent->m_dependant.erase(i);
        break;
      }
    }
    require(found);
  }

  {
    bool found = false;
    for (i = 0; i<m_objects.size(); i++)
    {
      if (m_objects[i] == obj)
      {
        found = true;
        m_objects.erase(i);
        break;
      }
    }
    require(found);
  }
  delete obj;
}

int
RandSchemaOp::alter_table(Ndb* ndb, Obj* obj)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  const NdbDictionary::Table * pOld = pDict->getTable(obj->m_name.c_str());
  NdbDictionary::Table tNew = * pOld;

  BaseString ops;
  unsigned mask = 3;

  unsigned type;
  while (ops.length() == 0 && (mask != 0))
  {
    switch((type = (ndb_rand_r(seed) & 1))){
    default:
    case 0:{
      if ((mask & (1 << type)) == 0)
        break;
      BaseString name;
      name.assfmt("newcol_%d", tNew.getNoOfColumns());
      NdbDictionary::Column col(name.c_str());
      col.setType(NdbDictionary::Column::Unsigned);
      col.setDynamic(true);
      col.setPrimaryKey(false);
      col.setNullable(true);
      NdbDictionary::Table save = tNew;
      tNew.addColumn(col);
      if (!pDict->supportedAlterTable(* pOld, tNew))
      {
        ndbout_c("not supported...");
        mask &= ~(1 << type);
        tNew = save;
        break;
      }
      ops.append(" addcol");
      break;
    }
    case 1:{
      BaseString name;
      do
      {
        unsigned no = ndb_rand_r(seed);
        name.assfmt("%s_%u", pOld->getName(), no);
      } while (pDict->getTable(name.c_str()));
      tNew.setName(name.c_str());
      ops.appfmt(" rename: %s", name.c_str());
      break;
    }

    }
  }

  if (ops.length())
  {
    ndbout_c("altering %s ops: %s", pOld->getName(), ops.c_str());
    if (pDict->alterTable(*pOld, tNew) != 0)
    {
      g_err << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    pDict->invalidateTable(pOld->getName());
    if (strcmp(pOld->getName(), tNew.getName()))
    {
      obj->m_name.assign(tNew.getName());
    }
  }

  return NDBT_OK;
}


int
RandSchemaOp::validate(Ndb* ndb)
{
  NdbDictionary::Dictionary* pDict = ndb->getDictionary();
  for (Uint32 i = 0; i<m_objects.size(); i++)
  {
    if (m_objects[i]->m_type == NdbDictionary::Object::UserTable)
    {
      const NdbDictionary::Table* tab2 = 
        pDict->getTable(m_objects[i]->m_name.c_str());

      if (tab2 == NULL) {
        g_err << "Table: " << m_objects[i]->m_name.c_str()
              << ", not found on line " << __LINE__
              <<", error: " << pDict->getNdbError()
              << endl;
        return NDBT_FAILED;
      }
      HugoTransactions trans(*tab2);
      trans.scanUpdateRecords(ndb, 1000);
      trans.clearTable(ndb);
      trans.loadTable(ndb, 1000);
    }
  }
  
  return NDBT_OK;
}

/*
      SystemTable = 1,        ///< System table
      UserTable = 2,          ///< User table (may be temporary)
      UniqueHashIndex = 3,    ///< Unique un-ordered hash index
      OrderedIndex = 6,       ///< Non-unique ordered index
      HashIndexTrigger = 7,   ///< Index maintenance, internal
      IndexTrigger = 8,       ///< Index maintenance, internal
      SubscriptionTrigger = 9,///< Backup or replication, internal
      ReadOnlyConstraint = 10,///< Trigger, internal
      Tablespace = 20,        ///< Tablespace
      LogfileGroup = 21,      ///< Logfile group
      Datafile = 22,          ///< Datafile
      Undofile = 23           ///< Undofile
*/

int
RandSchemaOp::cleanup(Ndb* ndb)
{
  Int32 i;
  for (i = m_objects.size() - 1; i >= 0; i--)
  {
    switch(m_objects[i]->m_type){
    case NdbDictionary::Object::UniqueHashIndex:
    case NdbDictionary::Object::OrderedIndex:        
      if (drop_obj(ndb, m_objects[i]))
        return NDBT_FAILED;
      
      break;
    default:
      break;
    }
  }

  for (i = m_objects.size() - 1; i >= 0; i--)
  {
    switch(m_objects[i]->m_type){
    case NdbDictionary::Object::UserTable:
      if (drop_obj(ndb, m_objects[i]))
        return NDBT_FAILED;
      break;
    default:
      break;
    }
  }
  
  require(m_objects.size() == 0);
  return NDBT_OK;
}

extern unsigned opt_seed;

int
runDictRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int loops = ctx->getNumLoops();

  unsigned seed = opt_seed;
  NdbMixRestarter res(&seed);
  RandSchemaOp dict(&seed);
  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  if (res.init(ctx, step))
    return NDBT_FAILED;
  
  for (int i = 0; i<loops; i++)
  {
    for (Uint32 j = 0; j<10; j++)
      if (dict.schema_op(pNdb))
        return NDBT_FAILED;
    
    if (res.dostep(ctx, step))
      return NDBT_FAILED;

    if (dict.validate(pNdb))
      return NDBT_FAILED;
  }

  if (res.finish(ctx, step))
    return NDBT_FAILED;

  if (dict.validate(pNdb))
    return NDBT_FAILED;
  
  if (dict.cleanup(pNdb))
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int
runBug29501(NDBT_Context* ctx, NDBT_Step* step) {
  NdbRestarter res;
  NdbDictionary::LogfileGroup lg;
  lg.setName("DEFAULT-LG");
  lg.setUndoBufferSize(8*1024*1024);

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  int node = res.getRandomNotMasterNodeId(rand());
  res.restartOneDbNode(node, true, true, false);

  if(pDict->createLogfileGroup(lg) != 0){
    g_err << "Failed to create logfilegroup:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Undofile uf;
  uf.setPath("undofile01.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");

  if(pDict->createUndofile(uf) != 0){
    g_err << "Failed to create undofile:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  res.waitNodesNoStart(&node, 1);
  res.startNodes(&node, 1);

  if (res.waitClusterStarted()){
  	g_err << "Node restart failed"
  	<< endl << pDict->getNdbError() << endl;
      return NDBT_FAILED;
  }

  if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lg.getName())) != 0){
  	g_err << "Drop of LFG Failed"
  	<< endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runDropDDObjects(NDBT_Context* ctx, NDBT_Step* step){
  //Purpose is to drop all tables, data files, Table spaces and LFG's
  Uint32 i = 0;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  
  NdbDictionary::Dictionary::List list;
  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;
  
  //Search the list and drop all tables found
  const char * tableFound = 0;
  for (i = 0; i < list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::UserTable:
        tableFound = list.elements[i].name;
        if(tableFound != 0){
          if(strcmp(list.elements[i].database, "TEST_DB") == 0 &&
             !is_prefix(tableFound, "NDB$BLOB"))
          { 
      	    if(pDict->dropTable(tableFound) != 0){
              g_err << "Failed to drop table: " << tableFound << pDict->getNdbError() << endl;
              return NDBT_FAILED;
            }
          }
        }
        tableFound = 0;
        break;
      default:
        break;
    }
  }
 
  //Search the list and drop all data file found
  const char * dfFound = 0;
  for (i = 0; i < list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::Datafile:
        dfFound = list.elements[i].name;
        if(dfFound != 0){
      	  if(pDict->dropDatafile(pDict->getDatafile(0, dfFound)) != 0){
            g_err << "Failed to drop datafile: " << pDict->getNdbError() << endl;
            return NDBT_FAILED;
          }
        }
        dfFound = 0;
        break;
      default:
        break;
    }
  }

  //Search the list and drop all Table Spaces Found 
  const char * tsFound  = 0;
  for (i = 0; i <list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::Tablespace:
        tsFound = list.elements[i].name;
        if(tsFound != 0){
          if(pDict->dropTablespace(pDict->getTablespace(tsFound)) != 0){
            g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
            return NDBT_FAILED;
          }
        }
        tsFound = 0;
        break;
      default:
        break;
    }
  }

  //Search the list and drop all LFG Found
  //Currently only 1 LGF is supported, but written for future 
  //when more then one is supported. 
  const char * lgFound  = 0;
  for (i = 0; i < list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::LogfileGroup:
        lgFound = list.elements[i].name;
        if(lgFound != 0){
          if (pDict->dropLogfileGroup(pDict->getLogfileGroup(lgFound)) != 0){
            g_err << "Failed to drop tablespace: " << pDict->getNdbError() << endl;
            return NDBT_FAILED;
          }
       }   
        lgFound = 0;
        break;
      default:
        break;
    }
  }

  return NDBT_OK;
}

int
runWaitStarted(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  restarter.waitClusterStarted(300);

  NdbSleep_SecSleep(3);
  return NDBT_OK;
}

int
testDropDDObjectsSetup(NDBT_Context* ctx, NDBT_Step* step){
  //Purpose is to setup to test DropDDObjects
  char tsname[256];
  char dfname[256];

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  NdbDictionary::LogfileGroup lg;
  lg.setName("DEFAULT-LG");
  lg.setUndoBufferSize(8*1024*1024);


  if(pDict->createLogfileGroup(lg) != 0){
    g_err << "Failed to create logfilegroup:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Undofile uf;
  uf.setPath("undofile01.dat");
  uf.setSize(5*1024*1024);
  uf.setLogfileGroup("DEFAULT-LG");

  if(pDict->createUndofile(uf) != 0){
    g_err << "Failed to create undofile:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  BaseString::snprintf(tsname, sizeof(tsname), "TS-%u", rand());
  BaseString::snprintf(dfname, sizeof(dfname), "%s-%u.dat", tsname, rand());

  if (create_tablespace(pDict, lg.getName(), tsname, dfname)){
  	g_err << "Failed to create undofile:"
        << endl << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runBug36072(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  NdbRestarter res;

  int err[] = { 6016, 
#if BUG_46856
                6017, 
#endif
                0 };
  for (Uint32 i = 0; err[i] != 0; i++)
  {
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    if (res.dumpStateAllNodes(val2, 2))
      return NDBT_FAILED;

    if (res.insertErrorInAllNodes(932)) // arbit
      return NDBT_FAILED;

    int code = err[i];

    if (code == 6016)
    {
      if (res.insertErrorInAllNodes(code))
        return NDBT_FAILED;
    }

    NdbDictionary::LogfileGroup lg;
    lg.setName("DEFAULT-LG");
    lg.setUndoBufferSize(8*1024*1024);

    NdbDictionary::Undofile uf;
    uf.setPath("undofile01.dat");
    uf.setSize(5*1024*1024);
    uf.setLogfileGroup("DEFAULT-LG");

    int r = pDict->createLogfileGroup(lg);
    if (code == 6017)
    {
      if (r)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }

      if (res.insertErrorInAllNodes(err[i]))
        return NDBT_FAILED;

      pDict->createUndofile(uf);
    }

    if (res.waitClusterNoStart())
      return NDBT_FAILED;

    res.startAll();
    if (res.waitClusterStarted())
      return NDBT_FAILED;

    if (code == 6016)
    {
      NdbDictionary::LogfileGroup lg2 = pDict->getLogfileGroup("DEFAULT-LG");
      NdbError err= pDict->getNdbError();
      if( (int) err.classification == (int) ndberror_cl_none)
      {
        ndbout << __LINE__ << endl;
        return NDBT_FAILED;
      }

      if (pDict->createLogfileGroup(lg) != 0)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }
    else
    {
      NdbDictionary::Undofile uf2 = pDict->getUndofile(0, "undofile01.dat");
      NdbError err= pDict->getNdbError();
      if( (int) err.classification == (int) ndberror_cl_none)
      {
        ndbout << __LINE__ << endl;
        return NDBT_FAILED;
      }

      if (pDict->createUndofile(uf) != 0)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }

    {
      NdbDictionary::LogfileGroup lg2 = pDict->getLogfileGroup("DEFAULT-LG");
      NdbError err= pDict->getNdbError();
      if( (int) err.classification != (int) ndberror_cl_none)
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }

      if (pDict->dropLogfileGroup(lg2))
      {
        ndbout << __LINE__ << " : " << pDict->getNdbError() << endl;
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}

int
restartClusterInitial(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  
  res.restartAll2(NdbRestarter::NRRF_INITIAL |
                  NdbRestarter::NRRF_NOSTART |
                  NdbRestarter::NRRF_ABORT);
  if (res.waitClusterNoStart())
    return NDBT_FAILED;

  res.startAll();
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  return NDBT_OK;
}


int
DropDDObjectsVerify(NDBT_Context* ctx, NDBT_Step* step){
  //Purpose is to verify test DropDDObjects worked
  Uint32 i = 0;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  NdbDictionary::Dictionary::List list;
  if (pDict->listObjects(list) == -1)
    return NDBT_FAILED;

    bool ddFound  = false;
  for (i = 0; i <list.count; i++){
    switch(list.elements[i].type){
      case NdbDictionary::Object::Tablespace:
        ddFound = true;
        break;
      case NdbDictionary::Object::LogfileGroup:
        ddFound = true;
        break;
      default:
        break;
    }
    if(ddFound == true){
      g_err << "DropDDObjects Failed: DD found:"
        << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

// Bug48604

// string messages between local/remote steps identified by stepNo-1
// each Msg<loc><rem> waits for Ack<loc><rem>

static const uint MaxMsg = 100;

static bool
send_msg(NDBT_Context* ctx, int loc, int rem, const char* msg)
{
  char msgName[20], ackName[20];
  sprintf(msgName, "Msg%d%d", loc, rem);
  sprintf(ackName, "Ack%d%d", loc, rem);
  g_info << loc << ": send to:" << rem << " msg:" << msg << endl;
  ctx->setProperty(msgName, msg);
  int cnt = 0;
  while (1)
  {
    if (ctx->isTestStopped())
      return false;
    int ret;
    if ((ret = ctx->getProperty(ackName, (Uint32)0)) != 0)
      break;
    if (++cnt % 100 == 0)
      g_info << loc << ": send to:" << rem << " wait for ack" << endl;
    NdbSleep_MilliSleep(10);
  }
  ctx->setProperty(ackName, (Uint32)0);
  return true;
}

static bool
poll_msg(NDBT_Context* ctx, int loc, int rem, char* msg)
{
  char msgName[20], ackName[20];
  sprintf(msgName, "Msg%d%d", rem, loc);
  sprintf(ackName, "Ack%d%d", rem, loc);
  const char* ptr;
  if ((ptr = ctx->getProperty(msgName, (char*)0)) != 0 && ptr[0] != 0)
  {
    require(strlen(ptr) < MaxMsg);
    memset(msg, 0, MaxMsg);
    strcpy(msg, ptr);
    g_info << loc << ": recv from:" << rem << " msg:" << msg << endl;
    ctx->setProperty(msgName, "");
    ctx->setProperty(ackName, (Uint32)1);
    return true;
  }
  return false;
}

static int
recv_msg(NDBT_Context* ctx, int loc, int rem, char* msg)
{
  uint cnt = 0;
  while (1)
  {
    if (ctx->isTestStopped())
      return false;
    if (poll_msg(ctx, loc, rem, msg))
      break;
    if (++cnt % 100 == 0)
      g_info << loc << ": recv from:" << rem << " wait for msg" << endl;
    NdbSleep_MilliSleep(10);
  }
  return true;
}

const char* tabName_Bug48604 = "TBug48604";
const char* indName_Bug48604 = "TBug48604X1";

static const NdbDictionary::Table*
runBug48604createtable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Table* pTab = 0;
  int result = NDBT_OK;
  do
  {
    NdbDictionary::Table tab(tabName_Bug48604);
    {
      NdbDictionary::Column col("a");
      col.setType(NdbDictionary::Column::Unsigned);
      col.setPrimaryKey(true);
      tab.addColumn(col);
    }
    {
      NdbDictionary::Column col("b");
      col.setType(NdbDictionary::Column::Unsigned);
      col.setNullable(false);
      tab.addColumn(col);
    }
    CHECK(pDic->createTable(tab) == 0);
    CHECK((pTab = pDic->getTable(tabName_Bug48604)) != 0);
  }
  while (0);
  return pTab;
}

static const NdbDictionary::Index*
runBug48604createindex(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Index* pInd = 0;
  int result = NDBT_OK;
  do {
    NdbDictionary::Index ind(indName_Bug48604);
    ind.setTable(tabName_Bug48604);
    ind.setType(NdbDictionary::Index::OrderedIndex);
    ind.setLogging(false);
    ind.addColumn("b");
    g_info << "index create.." << endl;
    CHECK(pDic->createIndex(ind) == 0);
    CHECK((pInd = pDic->getIndex(indName_Bug48604, tabName_Bug48604)) != 0);
    g_info << "index created" << endl;
    return pInd;
  }
  while (0);
  return pInd;
}

int
runBug48604(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Table* pTab = 0;
  const NdbDictionary::Index* pInd = 0;
  (void)pDic->dropTable(tabName_Bug48604);
  int loc = step->getStepNo() - 1;
  require(loc == 0);
  g_err << "main" << endl;
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  char msg[MaxMsg];

  do
  {
    CHECK((pTab = runBug48604createtable(ctx, step)) != 0);
    CHECK(send_msg(ctx, 0, 1, "s"));

    int loop = 0;
    while (result == NDBT_OK && loop++ < loops)
    {
      g_err << "loop:" << loop << endl;
      {
        // create index fully while uncommitted ops wait
        const char* ops[][3] =
        {
          { "ozin", "oc", "oa" },       // 0: before 1-2: after
          { "oziun", "oc", "oa" },
          { "ozidn", "oc", "oa" },
          { "ozicun", "oc", "oa" },
          { "ozicuuun", "oc", "oa" },
          { "ozicdn", "oc", "oa" },
          { "ozicdin", "oc", "oa" },
          { "ozicdidiuuudidn", "oc", "oa" },
          { "ozicdidiuuudidin", "oc", "oa" }
        };
        const int cnt = sizeof(ops)/sizeof(ops[0]);
        int i;
        for (i = 0; result == NDBT_OK && i < cnt; i++)
        {
          int j;
          for (j = 1; result == NDBT_OK && j <= 2; j++)
          {
            if (ops[i][j] == 0)
              continue;
            CHECK(send_msg(ctx, 0, 1, ops[i][0]));
            CHECK(recv_msg(ctx, 0, 1, msg) && msg[0] == 'o');
            CHECK((pInd = runBug48604createindex(ctx, step)) != 0);
            CHECK(send_msg(ctx, 0, 1, ops[i][j]));
            CHECK(recv_msg(ctx, 0, 1, msg) && msg[0] == 'o');

            CHECK(pDic->dropIndex(indName_Bug48604, tabName_Bug48604) == 0);
            g_info << "index dropped" << endl;
          }
        }
      }
    }
  }
  while (0);

  (void)send_msg(ctx, 0, 1, "x");
  ctx->stopTest();
  g_err << "main: exit:" << result << endl;
  return result;
}

int
runBug48604ops(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const NdbDictionary::Table* pTab = 0;
  //const NdbDictionary::Index* pInd = 0;
  int loc = step->getStepNo() - 1;
  require(loc > 0);
  g_err << "ops: loc:" << loc << endl;
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  char msg[MaxMsg];

  do
  {
    CHECK(recv_msg(ctx, loc, 0, msg));
    require(msg[0] == 's');
    CHECK((pTab = pDic->getTable(tabName_Bug48604)) != 0);
    HugoOperations ops(*pTab);
    bool have_trans = false;
    int opseq = 0;

    while (result == NDBT_OK && !ctx->isTestStopped())
    {
      CHECK(recv_msg(ctx, loc, 0, msg));
      if (msg[0] == 'x')
        break;
      if (msg[0] == 'o')
      {
        char* p = &msg[1];
        int c;
        while (result == NDBT_OK && (c = *p++) != 0)
        {
          if (c == 'n')
          {
            require(have_trans);
            CHECK(ops.execute_NoCommit(pNdb) == 0);
            g_info << loc << ": not committed" << endl;
            continue;
          }
          if (c == 'c')
          {
            require(have_trans);
            CHECK(ops.execute_Commit(pNdb) == 0);
            ops.closeTransaction(pNdb);
            have_trans = false;
            g_info << loc << ": committed" << endl;
            continue;
          }
          if (c == 'a')
          {
            require(have_trans);
            CHECK(ops.execute_Rollback(pNdb) == 0);
            ops.closeTransaction(pNdb);
            have_trans = false;
            g_info << loc << ": aborted" << endl;
            continue;
          }
          if (c == 'i' || c == 'u' || c == 'd')
          {
            if (!have_trans)
            {
              CHECK(ops.startTransaction(pNdb) == 0);
              have_trans = true;
              g_info << loc << ": trans started" << endl;
            }
            int i;
            for (i = 0; result == NDBT_OK && i < records; i++)
            {
              if (c == 'i')
                  CHECK(ops.pkInsertRecord(pNdb, i, 1, opseq) == 0);
              if (c == 'u')
                CHECK(ops.pkUpdateRecord(pNdb, i, 1, opseq) == 0);
              if (c == 'd')
                CHECK(ops.pkDeleteRecord(pNdb, i, 1) == 0);
            }
            char op_str[2];
            sprintf(op_str, "%c", c);
            g_info << loc << ": op:" << op_str << " records:" << records << endl;
            opseq++;
            continue;
          }
          if (c == 'z')
          {
            CHECK(ops.clearTable(pNdb) == 0);
            continue;
          }
          require(false);
        }
        CHECK(send_msg(ctx, loc, 0, "o"));
        continue;
      }
      require(false);
    }
  } while (0);

  g_err << "ops: loc:" << loc << " exit:" << result << endl;
  if (result != NDBT_OK)
    ctx->stopTest();
  return result;
}

int
runBug54651(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  for (Uint32 j = 0; j< 2; j++)
  {
    pDic->createTable(* ctx->getTab());
    
    const NdbDictionary::Table * pTab =pDic->getTable(ctx->getTab()->getName());
    NdbDictionary::Table copy = * pTab;
    BaseString name;
    name.assfmt("%s_1", pTab->getName());
    copy.setName(name.c_str());
    
    if (pDic->createTable(copy))
    {
      ndbout_c("Failed to create table...");
      ndbout << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    
    NdbDictionary::Table alter = * pTab;
    alter.setName(name.c_str());
    for (Uint32 i = 0; i<2; i++)
    {
      // now rename org table to same name...
      if (pDic->alterTable(* pTab, alter) == 0)
      {
        ndbout << "Alter with duplicate name succeeded!!" << endl;
        return NDBT_FAILED;
      }
      
      ndbout << "Alter with duplicate name failed...good" << endl
             << pDic->getNdbError() << endl;
    }
    
    pDic->dropTable(copy.getName());
    pDic->dropTable(ctx->getTab()->getName());
  }
  return NDBT_OK;
}

/** telco-6.4 **/

// begin schema trans

#undef chk1
#undef chk2

static bool st_core_on_err = false;

#define chk1(x) \
  do { \
    if (x) break; \
    g_err << "FAIL " << __LINE__ << " " << #x << endl; \
    if (st_core_on_err) abort(); \
    goto err; \
  } while (0)

#define chk2(x, e) \
  do { \
    if (x) break; \
    g_err << "FAIL " << __LINE__ << " " << #x << ": " << e << endl; \
    if (st_core_on_err) abort(); \
    goto err; \
  } while (0)

static uint
urandom(uint m)
{
  require(m != 0);
  uint n = (uint)ndb_rand();
  return n % m;
}

static bool
randomly(uint k, uint m)
{
  uint n = urandom(m);
  return n < k;
}

// structs

struct ST_Obj;
template class Vector<ST_Obj*>;
typedef Vector<ST_Obj*> ST_Objlist;

static ST_Objlist st_objlist;
#ifndef NDEBUG
static const ST_Obj* st_find_obj(const char* db, const char* name);
#endif

#define ST_MAX_NAME_SIZE  (MAX_TAB_NAME_SIZE + 100)

struct ST_Obj {
  NdbDictionary::Object::Type type;
  char dbname[ST_MAX_NAME_SIZE];
  char name[ST_MAX_NAME_SIZE];
  int id;
  enum { Skip = 0xFFFF }; // mark ignored objects in List
  bool create; // true/false = create/drop prepared or committed
  bool commit;
  bool exists() const { // visible to trans
    return !(!create && commit);
  }
  virtual bool is_trigger() const {
    return false;
  }
  virtual bool is_index() const {
    return false;
  }
  virtual bool is_table() const {
    return false;
  }
  virtual const char* realname() const {
    return name;
  }
  ST_Obj(const char* a_dbname, const char* a_name) {
    type = NdbDictionary::Object::TypeUndefined;
    strcpy(dbname, a_dbname);
    strcpy(name, a_name);
    id = -1;
    create = false; // init as dropped
    commit = true;
    assert(st_find_obj(dbname, name) == 0);
    st_objlist.push_back(this);
  }
  virtual ~ST_Obj() {}
};

static NdbOut&
operator<<(NdbOut& out, const ST_Obj& obj)
{
  out << obj.name << "[" << obj.id << "]";
  return out;
}

struct ST_Trg : public ST_Obj {
  struct ST_Ind* ind;
  TriggerEvent::Value event;
  mutable char realname_buf[ST_MAX_NAME_SIZE];
  virtual bool is_trigger() const {
    return true;
  }
  virtual const char* realname() const;
  ST_Trg(const char* a_db, const char* a_name) :
    ST_Obj(a_db, a_name) {
    ind = 0;
  }
  virtual ~ST_Trg() {};
};

template class Vector<ST_Trg*>;
typedef Vector<ST_Trg*> ST_Trglist;

struct ST_Ind : public ST_Obj {
  struct ST_Tab* tab;
  const NdbDictionary::Index* ind;
  const NdbDictionary::Index* ind_r; // retrieved
  BaseString colnames;
  ST_Trglist* trglist;
  int trgcount;
  virtual bool is_index() const {
    return true;
  }
  bool is_unique() const {
    return type == NdbDictionary::Object::UniqueHashIndex;
  }
  const ST_Trg& trg(int k) const {
    return *((*trglist)[k]);
  }
  ST_Trg& trg(int k) {
    return *((*trglist)[k]);
  }
  ST_Ind(const char* a_db, const char* a_name) :
    ST_Obj(a_db, a_name) {
    tab = 0;
    ind = 0;
    ind_r = 0;
    trglist = new ST_Trglist;
    trgcount = 0;
  };
  virtual ~ST_Ind() {
    delete ind;
    delete trglist;
    ind = 0;
    trglist = 0;
  }
};

const char*
ST_Trg::realname() const
{
  if (!exists())
    return name;
  const char* p = name;
  const char* q = strchr(p, '<');
  const char* r = strchr(p, '>');
  require(q != 0 && r != 0 && q < r);
  require(ind->id != -1);
  sprintf(realname_buf, "%.*s%d%s", (int)(q - p), p, ind->id, r + 1);
  return realname_buf;
}

template class Vector<ST_Ind*>;
typedef Vector<ST_Ind*> ST_Indlist;

struct ST_Tab : public ST_Obj {
  const NdbDictionary::Table* tab;
  const NdbDictionary::Table* tab_r; // retrieved
  ST_Indlist* indlist;
  int indcount;
  int induniquecount;
  int indorderedcount;
  virtual bool is_table() const {
    return true;
  }
  const ST_Ind& ind(int j) const {
    return *((*indlist)[j]);
  }
  ST_Ind& ind(int j) {
    return *((*indlist)[j]);
  }
  ST_Tab(const char* a_db, const char* a_name) :
    ST_Obj(a_db, a_name) {
    tab = 0;
    tab_r = 0;
    indlist = new ST_Indlist;
    indcount = 0;
    induniquecount = 0;
    indorderedcount = 0;
  }
  virtual ~ST_Tab() {
    delete tab;
    delete indlist;
    tab = 0;
    indlist = 0;
  }
};

template class Vector<ST_Tab*>;
typedef Vector<ST_Tab*> ST_Tablist;

struct ST_Restarter : public NdbRestarter {
  int get_status();
  const ndb_mgm_node_state& get_state(int node_id);
  ST_Restarter() {
    int i;
    for (i = 0; i < MAX_NODES; i++)
      state[i].node_type = NDB_MGM_NODE_TYPE_UNKNOWN;
    first_time = true;
  }
protected:
  void set_state(const ndb_mgm_node_state& state);
  ndb_mgm_node_state state[MAX_NODES];
  bool first_time;
};

const ndb_mgm_node_state&
ST_Restarter::get_state(int node_id) {
  require(node_id > 0 && node_id < MAX_NODES);
  require(!first_time);
  return state[node_id];
}

void
ST_Restarter::set_state(const ndb_mgm_node_state& new_state)
{
  int node_id = new_state.node_id;
  require(1 <= node_id && node_id < MAX_NODES);

  require(new_state.node_type == NDB_MGM_NODE_TYPE_MGM ||
          new_state.node_type == NDB_MGM_NODE_TYPE_NDB ||
          new_state.node_type == NDB_MGM_NODE_TYPE_API);

  ndb_mgm_node_state& old_state = state[node_id];
  if (!first_time)
    require(old_state.node_type == new_state.node_type);
  old_state = new_state;
}

int
ST_Restarter::get_status()
{
  if (getStatus() == -1)
    return -1;
  int i;
  for (i = 0; i < (int)mgmNodes.size(); i++)
    set_state(mgmNodes[i]);
  for (i = 0; i < (int)ndbNodes.size(); i++)
    set_state(ndbNodes[i]);
  for (i = 0; i < (int)apiNodes.size(); i++)
    set_state(apiNodes[i]);
  first_time = false;
  return 0;
}

struct ST_Con {
  Ndb_cluster_connection* ncc;
  Ndb* ndb;
  NdbDictionary::Dictionary* dic;
  ST_Restarter* restarter;
  int numdbnodes;
  char dbname[ST_MAX_NAME_SIZE];
  ST_Tablist* tablist;
  int tabcount;
  bool tx_on;
  bool tx_commit;
  bool is_xcon;
  ST_Con* xcon;
  int node_id;
  int loop;
  const ST_Tab& tab(int i) const {
    return *((*tablist)[i]);
  }
  ST_Tab& tab(int i) {
    return *((*tablist)[i]);
  }
  ST_Con(Ndb_cluster_connection* a_ncc,
         Ndb* a_ndb,
         ST_Restarter* a_restarter) {
    ncc = a_ncc;
    ndb = a_ndb;
    dic = a_ndb->getDictionary();
    restarter = a_restarter;
    numdbnodes = restarter->getNumDbNodes();
    require(numdbnodes >= 1);
    sprintf(dbname, "%s", ndb->getDatabaseName());
    tablist = new ST_Tablist;
    tabcount = 0;
    tx_on = false;
    tx_commit = false;
    is_xcon = false;
    xcon = 0;
    node_id = ncc->node_id();
    {
      require(restarter->get_status() == 0);
      const ndb_mgm_node_state& state = restarter->get_state(node_id);
      require(state.node_type == NDB_MGM_NODE_TYPE_API);
      require(state.version != 0); // means "connected"
      g_info << "node_id:" << node_id << endl;
    }
    loop = -1;
  }
  ~ST_Con() {
    if (!is_xcon) {
      delete tablist;
    } else {
      delete ndb;
      delete ncc;
    }
    tablist = 0;
    ndb = 0;
    ncc = 0;
  }
};

// initialization

static int
st_drop_all_tables(ST_Con& c)
{
  g_info << "st_drop_all_tables" << endl;
  NdbDictionary::Dictionary::List list;
  chk2(c.dic->listObjects(list) == 0, c.dic->getNdbError());
  int n;
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (element.type == NdbDictionary::Object::UserTable &&
        strcmp(element.database, "TEST_DB") == 0) {
      chk2(c.dic->dropTable(element.name) == 0, c.dic->getNdbError());
    }
  }
  return 0;
err:
  return -1;
}

static void
st_init_objects(ST_Con& c, NDBT_Context* ctx)
{
  int numTables = ctx->getNumTables();
  c.tabcount = 0;
  int i;
  for (i = 0; i < numTables; i++) {
    const NdbDictionary::Table* pTab = 0;
#if ndb_test_ALL_TABLES_is_fixed
    const NdbDictionary::Table** tables = ctx->getTables();
    pTab = tables[i];
#else
    const Vector<BaseString>& tables = ctx->getSuite()->m_tables_in_test;
    pTab = NDBT_Tables::getTable(tables[i].c_str());
#endif
    require(pTab != 0 && pTab->getName() != 0);

    {
      bool ok = true;
      int n;
      for (n = 0; n < pTab->getNoOfColumns(); n++) {
        const NdbDictionary::Column* pCol = pTab->getColumn(n);
        require(pCol != 0);
        if (pCol->getStorageType() !=
            NdbDictionary::Column::StorageTypeMemory) {
          g_err << pTab->getName() << ": skip non-mem table for now" << endl;
          ok = false;
          break;
        }
      }
      if (!ok)
        continue;
    }

    c.tablist->push_back(new ST_Tab(c.dbname, pTab->getName()));
    c.tabcount++;
    ST_Tab& tab = *c.tablist->back();
    tab.type = NdbDictionary::Object::UserTable;
    tab.tab = new NdbDictionary::Table(*pTab);

    const char** indspec = NDBT_Tables::getIndexes(tab.name);

    while (indspec != 0 && *indspec != 0) {
      char ind_name[ST_MAX_NAME_SIZE];
      sprintf(ind_name, "%sX%d", tab.name, tab.indcount);
      tab.indlist->push_back(new ST_Ind("sys", ind_name));
      ST_Ind& ind = *tab.indlist->back();
      ind.tab = &tab;

      NdbDictionary::Index* pInd = new NdbDictionary::Index(ind.name);
      pInd->setTable(tab.name);
      pInd->setLogging(false);

      const char* type = *indspec++;
      if (strcmp(type, "UNIQUE") == 0) {
        ind.type = NdbDictionary::Object::UniqueHashIndex;
        pInd->setType((NdbDictionary::Index::Type)ind.type);
        tab.induniquecount++;

        { char trg_name[ST_MAX_NAME_SIZE];
          sprintf(trg_name, "NDB$INDEX_<%s>_UI", ind.name);
          ind.trglist->push_back(new ST_Trg("", trg_name));
          ST_Trg& trg = *ind.trglist->back();
          trg.ind = &ind;
          trg.type = NdbDictionary::Object::HashIndexTrigger;
          trg.event = TriggerEvent::TE_INSERT;
        }
        ind.trgcount = 1;
      }
      else if (strcmp(type, "ORDERED") == 0) {
        ind.type = NdbDictionary::Object::OrderedIndex;
        pInd->setType((NdbDictionary::Index::Type)ind.type);
        tab.indorderedcount++;

        { char trg_name[ST_MAX_NAME_SIZE];
          sprintf(trg_name, "NDB$INDEX_<%s>_CUSTOM", ind.name);
          ind.trglist->push_back(new ST_Trg("", trg_name));
          ST_Trg& trg = *ind.trglist->back();
          trg.ind = &ind;
          trg.type = NdbDictionary::Object::IndexTrigger;
          trg.event = TriggerEvent::TE_CUSTOM;
        }
        ind.trgcount = 1;
      }
      else
      {
        require(false);
      }

      const char* sep = "";
      const char* colname;
      while ((colname = *indspec++) != 0) {
        const NdbDictionary::Column* col = tab.tab->getColumn(colname);
        require(col != 0);
        pInd->addColumn(*col);

        ind.colnames.appfmt("%s%s", sep, colname);
        sep = ",";
      }

      ind.ind = pInd;
      tab.indcount++;
    }
  }
}

// node states

static int
st_report_db_nodes(ST_Con& c, NdbOut& out)
{
  chk1(c.restarter->get_status() == 0);
  char r1[100]; // up
  char r2[100]; // down
  char r3[100]; // unknown
  r1[0] =r2[0] = r3[0] = 0;
  int i;
  for (i = 1; i < MAX_NODES; i++) {
    const ndb_mgm_node_state& state = c.restarter->get_state(i);
    if (state.node_type == NDB_MGM_NODE_TYPE_NDB) {
      char* r = 0;
      if (state.node_status == NDB_MGM_NODE_STATUS_STARTED)
        r = r1;
      else if (state.node_status == NDB_MGM_NODE_STATUS_NO_CONTACT)
        r = r2;
      else
        r = r3;
      sprintf(r + strlen(r), "%s%d", r[0] == 0 ? "" : ",", i);
    }
  }
  if (r2[0] != 0 || r3[0] != 0) {
    out << "nodes up:" << r1 << " down:" << r2 << " unknown:" << r3 << endl;
    goto err;
  }
  out << "nodes up:" << r1 << " (all)" << endl;
  return 0;
err:
  return -1;
}

static int
st_check_db_nodes(ST_Con& c, int ignore_node_id = -1)
{
  chk1(c.restarter->get_status() == 0);
  int i;
  for (i = 1; i < MAX_NODES; i++) {
    const ndb_mgm_node_state& state = c.restarter->get_state(i);
    if (state.node_type == NDB_MGM_NODE_TYPE_NDB &&
        i != ignore_node_id) {
      chk2(state.node_status == NDB_MGM_NODE_STATUS_STARTED, " node:" << i);
    }
  }
  return 0;
err:
  return -1;
}

#if 0
static int
st_wait_db_node_up(ST_Con& c, int node_id)
{
  int count = 0;
  int max_count = 30;
  int milli_sleep = 2000;
  while (count++ < max_count) {
    // get status and check that other db nodes have not crashed
    chk1(st_check_db_nodes(c, node_id) == 0);

    const ndb_mgm_node_state& state = c.restarter->get_state(node_id);
    require(state.node_type == NDB_MGM_NODE_TYPE_NDB);
    if (state.node_status == NDB_MGM_NODE_STATUS_STARTED)
      break;
    g_info << "waiting count:" << count << "/" << max_count << endl;
    NdbSleep_MilliSleep(milli_sleep);
  }
  return 0;
err:
  return -1;
}
#endif

// extra connection (separate API node)

static int
st_start_xcon(ST_Con& c)
{
  require(c.xcon == 0);
  g_info << "start extra connection" << endl;

  do {
    int ret;
    Ndb_cluster_connection* xncc = new Ndb_cluster_connection;
    chk2((ret = xncc->connect(30, 1, 0)) == 0, "ret:" << ret);
    chk2((ret = xncc->wait_until_ready(30, 10)) == 0, "ret:" << ret);
    Ndb* xndb = new Ndb(xncc, c.dbname);
    chk1(xndb->init() == 0);
    chk1(xndb->waitUntilReady(30) == 0);
    // share restarter
    c.xcon = new ST_Con(xncc, xndb, c.restarter);
    // share objects
    c.xcon->tablist = c.tablist;
    c.xcon->tabcount = c.tabcount;
    c.xcon->is_xcon = true;
  } while (0);
  return 0;
err:
  return -1;
}

static int
st_stop_xcon(ST_Con& c)
{
  require(c.xcon != 0);
  int node_id = c.xcon->node_id;
  g_info << "stop extra connection node_id:" << node_id << endl;

  c.xcon->restarter = 0;
  c.xcon->tablist = 0;
  c.xcon->tabcount = 0;
  delete c.xcon;
  c.xcon = 0;
  int count = 0;
  while (1) {
    chk1(c.restarter->get_status() == 0);
    const ndb_mgm_node_state& state = c.restarter->get_state(node_id);
    require(state.node_type == NDB_MGM_NODE_TYPE_API);
    if (state.version == 0) // means "disconnected"
      break;
    g_info << "waiting count:" << ++count << endl;
    NdbSleep_MilliSleep(10 * count);
  }
  return 0;
err:
  return -1;
}

// error insert

struct ST_Errins {
  int value;              // error value to insert
  int code;               // ndb error code to expect
  int master;             // insert on master / non-master (-1 = random)
  int node;               // insert on node id
  const ST_Errins* list;  // include another list
  bool ends;              // end list
  ST_Errins() :
    value(0), code(0), master(-1), node(0), list(0), ends(true)
  {}
  ST_Errins(const ST_Errins* l) :
    value(0), code(0), master(-1), node(0), list(l), ends(false)
  {}
  ST_Errins(int v, int c, int m = -1) :
    value(v), code(c), master(m), node(0), list(0), ends(false)
  {}
};

static NdbOut&
operator<<(NdbOut& out, const ST_Errins& errins)
{
  out << "value:" << errins.value;
  out << " code:" << errins.code;
  out << " master:" << errins.master;
  out << " node:" << errins.node;
  return out;
}

static ST_Errins
st_get_errins(ST_Con& c, const ST_Errins* list)
{
  uint size = 0;
  while (!list[size++].ends)
    ;
  require(size > 1);
  uint n = urandom(size - 1);
  const ST_Errins& errins = list[n];
  if (errins.list == 0) {
    require(errins.value != 0);
    return errins;
  }
  return st_get_errins(c, errins.list);
}

static int
st_do_errins(ST_Con& c, ST_Errins& errins)
{
  require(errins.value != 0);
  if (c.numdbnodes < 2)
    errins.master = 1;
  else if (errins.master == -1)
    errins.master = randomly(1, 2);
  if (errins.master) {
    errins.node = c.restarter->getMasterNodeId();
  } else {
    uint rand = urandom(c.numdbnodes);
    errins.node = c.restarter->getRandomNotMasterNodeId(rand);
  }
  g_info << "errins: " << errins << endl;
  chk2(c.restarter->insertErrorInNode(errins.node, errins.value) == 0, errins);
  c.restarter->get_status(); // do sync call to ensure error has been inserted
  return 0;
err:
  return -1;
}

// debug aid
#ifndef NDEBUG
static const ST_Obj*
st_find_obj(const char* dbname, const char* name)
{
  const ST_Obj* ret_objp = 0;
  int i;
  for (i = 0; i < (int)st_objlist.size(); i++) {
    const ST_Obj* objp = st_objlist[i];
    if (strcmp(objp->dbname, dbname) == 0 &&
        strcmp(objp->name, name) == 0) {
      require(ret_objp == 0);
      ret_objp = objp;
    }
  }
  return ret_objp;
}
#endif

#if 0
static void
st_print_obj(const char* dbname, const char* name, int line = 0)
{
  const ST_Obj* objp = st_find_obj(dbname, name);
  g_info << name << ": by name:";
  if (objp != 0)
    g_info << " create:" << objp->create
           << " commit:" << objp->commit
           << " exists:" << objp->exists();
  else
    g_info << " not found";
  if (line != 0)
    g_info << " line:" << line;
  g_info << endl;
}
#endif

// set object state

static void
st_set_commit_obj(ST_Con& c, ST_Obj& obj)
{
  bool create_old = obj.create;
  bool commit_old = obj.commit;
  if (!c.tx_commit && !obj.commit)
    obj.create = !obj.create;
  obj.commit = true;
  if (create_old != obj.create || commit_old != obj.commit) {
    g_info << obj.name << ": set commit:"
           << " create:" << create_old << "->" << obj.create
           << " commit:" << commit_old << "->" << obj.commit << endl;
  }
}

#if 0
static void
st_set_commit_trg(ST_Con& c, ST_Trg& trg)
{
  st_set_commit_obj(c, trg);
}
#endif

static void
st_set_commit_ind(ST_Con& c, ST_Ind& ind)
{
  st_set_commit_obj(c, ind);
  int k;
  for (k = 0; k < ind.trgcount; k++) {
    ST_Trg& trg = ind.trg(k);
    st_set_commit_obj(c, trg);
  }
}

static void
st_set_commit_tab(ST_Con& c, ST_Tab& tab)
{
  st_set_commit_obj(c, tab);
  int j;
  for (j = 0; j < tab.indcount; j++) {
    ST_Ind& ind = tab.ind(j);
    st_set_commit_ind(c, ind);
  }
}

static void
st_set_commit_all(ST_Con& c)
{
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    st_set_commit_tab(c, tab);
  }
}

static void
st_set_create_obj(ST_Con& c, ST_Obj& obj, bool create)
{
  bool create_old = obj.create;
  bool commit_old = obj.commit;
  obj.create = create;
  obj.commit = !c.tx_on;
  if (create_old != obj.create || commit_old != obj.commit) {
    g_info << obj.name << ": set create:"
           << " create:" << create_old << "->" << obj.create
           << " commit:" << commit_old << "->" << obj.commit << endl;
  }
}

static void
st_set_create_trg(ST_Con& c, ST_Trg& trg, bool create)
{
  st_set_create_obj(c, trg, create);
}

static void
st_set_create_ind(ST_Con& c, ST_Ind& ind, bool create)
{
  st_set_create_obj(c, ind, create);
  int k;
  for (k = 0; k < ind.trgcount; k++) {
    ST_Trg& trg = ind.trg(k);
    st_set_create_trg(c, trg, create);
  }
}

static void
st_set_create_tab(ST_Con& c, ST_Tab& tab, bool create)
{
  st_set_create_obj(c, tab, create);
  int j;
  for (j = 0; j < tab.indcount; j++) {
    ST_Ind& ind = tab.ind(j);
    if (create == true)
      require(!ind.exists());
    else {
      if (ind.exists())
        st_set_create_ind(c, ind, false);
    }
  }
}

// verify against database listing

static bool
st_known_type(const NdbDictionary::Dictionary::List::Element& element)
{
  return element.id != ST_Obj::Skip;
}

static int
st_find_object(const NdbDictionary::Dictionary::List& list,
               NdbDictionary::Object::Type type, int id)
{
  int n;
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (element.type == type && (int)element.id == id)
      return n;
  }
  return -1;
}

// filter out irrelevant by whatever means (we need listObjects2)
static int
st_list_objects(ST_Con& c, NdbDictionary::Dictionary::List& list)
{
  g_info << "st_list_objects" << endl;
  int keep[256];
  memset(keep, 0, sizeof(keep));
  chk2(c.dic->listObjects(list) == 0, c.dic->getNdbError());
  int n;
  // tables
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (element.type == NdbDictionary::Object::UserTable) {
      int i;
      for (i = 0; i < c.tabcount; i++) {
        const ST_Tab& tab = c.tab(i);
        if (strcmp(element.name, tab.name) == 0)
          keep[n]++;
      }
    }
    require(keep[n] <= 1);
  }
  // indexes
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (element.type == NdbDictionary::Object::UniqueHashIndex ||
        element.type == NdbDictionary::Object::OrderedIndex) {
      int i, j;
      for (i = 0; i < c.tabcount; i++) {
        const ST_Tab& tab = c.tab(i);
        for (j = 0; j < tab.indcount; j++) {
          const ST_Ind& ind = tab.ind(j);
          if (strcmp(element.name, ind.name) == 0)
            keep[n]++;
        }
      }
    }
    require(keep[n] <= 1);
  }
  // triggers
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (element.type == NdbDictionary::Object::HashIndexTrigger) {
      int id, n2;
      chk2(sscanf(element.name, "NDB$INDEX_%d_UI", &id) == 1,
           element.name);
      n2 = st_find_object(list, NdbDictionary::Object::UniqueHashIndex, id);
      chk2(n2 >= 0, element.name);
      if (keep[n2])
        keep[n]++;
    }
    if (element.type == NdbDictionary::Object::IndexTrigger) {
      int id, n2;
      chk2(sscanf(element.name, "NDB$INDEX_%d_CUSTOM", &id) == 1,
           element.name);
      n2 = st_find_object(list, NdbDictionary::Object::OrderedIndex, id);
      chk2(n2 >= 0, element.name);
      if (keep[n2])
        keep[n]++;
    }
    require(keep[n] <= 1);
  }
  // mark ignored
  for (n = 0; n < (int)list.count; n++) {
    NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    g_info << "id=" << element.id << " type=" << element.type
           << " name=" << element.name << " keep=" << keep[n] << endl;
    if (!keep[n]) {
      require(element.id != ST_Obj::Skip);
      element.id = ST_Obj::Skip;
    }
  }
  return 0;
err:
  return -1;
}

static bool
st_match_obj(const ST_Obj& obj,
             const NdbDictionary::Dictionary::List::Element& element)
{
  int veryverbose = 0;
  if (veryverbose) {
    g_info
      << "match:"
      << " " << obj.type << "-" << element.type
      << " " << obj.dbname << "-" << element.database
      << " " << obj.realname() << "-" << element.name << endl;
  }
  return
    obj.type == element.type &&
    strcmp(obj.dbname, element.database) == 0 &&
    strcmp(obj.realname(), element.name) == 0;
}

static int // check state
st_verify_obj(const ST_Obj& obj,
              const NdbDictionary::Dictionary::List::Element& element)
{
  chk2(obj.exists(), obj.name);

  if (obj.commit)
    chk2(element.state == NdbDictionary::Object::StateOnline, obj.name);

  // other states are inconsistent

  else if (obj.create) {
    if (obj.is_table() || obj.is_index())
      chk2(element.state == NdbDictionary::Object::StateBuilding, obj.name);
    if (obj.is_trigger())
      chk2(element.state == NdbDictionary::Object::StateBuilding, obj.name);
  }
  else {
    if (obj.is_trigger())
      chk2(element.state == NdbDictionary::Object::StateOnline, obj.name);
    if (obj.is_table() || obj.is_index())
      chk2(element.state == NdbDictionary::Object::StateDropping, obj.name);
  }
  return 0;
err:
  return -1;
}

static int // find on list
st_verify_obj(const ST_Obj& obj,
              const NdbDictionary::Dictionary::List& list)
{
  int found = 0;
  int n;
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (!st_known_type(element))
      continue;
    if (st_match_obj(obj, element)) {
      chk1(st_verify_obj(obj, element) == 0);
      found += 1;
    }
  }
  if (obj.exists())
    chk2(found == 1, obj.name);
  else
    chk2(found == 0, obj.name);
  return 0;
err:
  return -1;
}

static int // possible match
st_verify_obj(const ST_Obj& obj,
             const NdbDictionary::Dictionary::List::Element& element,
             int& found)
{
  if (obj.exists()) {
    if (st_match_obj(obj, element)) {
      chk1(st_verify_obj(obj, element) == 0);
      found += 1;
    }
  }
  else {
    chk2(st_match_obj(obj, element) == false, obj.name);
  }
  return 0;
err:
  return -1;
}

static int
st_verify_list(ST_Con& c)
{
  NdbDictionary::Dictionary::List list;
  chk1(st_list_objects(c, list) == 0);
  int i, j, k, n;
  // us vs list
  for (i = 0; i < c.tabcount; i++) {
    const ST_Tab& tab = c.tab(i);
    chk1(st_verify_obj(tab, list) == 0);
    for (j = 0; j < tab.indcount; j++) {
      const ST_Ind& ind = tab.ind(j);
      chk1(st_verify_obj(ind, list) == 0);
      for (k = 0; k < ind.trgcount; k++) {
        const ST_Trg& trg = ind.trg(k);
        chk1(st_verify_obj(trg, list) == 0);
      }
    }
  }
  // list vs us
  for (n = 0; n < (int)list.count; n++) {
    const NdbDictionary::Dictionary::List::Element& element =
      list.elements[n];
    if (!st_known_type(element))
      continue;
    int found = 0;
    for (i = 0; i < c.tabcount; i++) {
      const ST_Tab& tab = c.tab(i);
      chk1(st_verify_obj(tab, element, found) == 0);
      for (j = 0; j < tab.indcount; j++) {
        const ST_Ind& ind = tab.ind(j);
        chk1(st_verify_obj(ind, element, found) == 0);
        for (k = 0; k < ind.trgcount; k++) {
          const ST_Trg& trg = ind.trg(k);
          chk1(st_verify_obj(trg, element, found) == 0);
        }
      }
    }
    const char* dot = element.database[0] != 0 ? "." : "";
    chk2(found == 1, element.database << dot << element.name);
  }
  return 0;
err:
  return -1;
}

// wait for DICT to finish current trans

static int
st_wait_idle(ST_Con& c)
{
  // todo: use try-lock when available
  g_info << "st_wait_idle" << endl;
  int count = 0;
  int max_count = 60;
  int milli_sleep = 1000;
  while (count++ < max_count) {
    NdbDictionary::Dictionary::List list;
    chk1(st_list_objects(c, list) == 0);
    bool ok = true;
    int n;
    for (n = 0; n < (int)list.count; n++) {
      const NdbDictionary::Dictionary::List::Element& element =
        list.elements[n];
      if (!st_known_type(element))
        continue;
      if (element.state != NdbDictionary::Object::StateOnline) {
        ok = false;
        break;
      }
    }
    if (ok)
      return 0;
    g_info << "waiting count:" << count << "/" << max_count << endl;
    NdbSleep_MilliSleep(milli_sleep);
  }
  g_err << "st_wait_idle: objects did not become Online" << endl;
err:
  return -1;
}

// ndb dict comparisons (non-retrieved vs retrieved)

static int
st_equal_column(const NdbDictionary::Column& c1,
                const NdbDictionary::Column& c2,
                NdbDictionary::Object::Type type)
{
  chk1(strcmp(c1.getName(), c2.getName()) == 0);
  chk1(c1.getNullable() == c2.getNullable());
  if (type == NdbDictionary::Object::UserTable) {
    chk1(c1.getPrimaryKey() == c2.getPrimaryKey());
  }
  if (0) { // should fix
    chk1(c1.getColumnNo() == c2.getColumnNo());
  }
  chk1(c1.getType() == c2.getType());
  if (c1.getType() == NdbDictionary::Column::Decimal ||
      c1.getType() == NdbDictionary::Column::Decimalunsigned) {
    chk1(c1.getPrecision() == c2.getPrecision());
    chk1(c1.getScale() == c2.getScale());
  }
  if (c1.getType() != NdbDictionary::Column::Blob &&
      c1.getType() != NdbDictionary::Column::Text) {
    chk1(c1.getLength() == c2.getLength());
  } else {
    chk1(c1.getInlineSize() == c2.getInlineSize());
    chk1(c1.getPartSize() == c2.getPartSize());
    chk1(c1.getStripeSize() == c2.getStripeSize());
  }
  chk1(c1.getCharset() == c2.getCharset());
  if (type == NdbDictionary::Object::UserTable) {
    chk1(c1.getPartitionKey() == c2.getPartitionKey());
  }
  chk1(c1.getArrayType() == c2.getArrayType());
  chk1(c1.getStorageType() == c2.getStorageType());
  chk1(c1.getDynamic() == c2.getDynamic());
  chk1(c1.getAutoIncrement() == c2.getAutoIncrement());
  return 0;
err:
  return -1;
}

static int
st_equal_table(const NdbDictionary::Table& t1, const NdbDictionary::Table& t2)
{
  chk1(strcmp(t1.getName(), t2.getName()) == 0);
  chk1(t1.getLogging() == t2.getLogging());
  chk1(t1.getFragmentType() == t2.getFragmentType());
  chk1(t1.getKValue() == t2.getKValue());
  chk1(t1.getMinLoadFactor() == t2.getMinLoadFactor());
  chk1(t1.getMaxLoadFactor() == t2.getMaxLoadFactor());
  chk1(t1.getNoOfColumns() == t2.getNoOfColumns());
  /*
   * There is no method to get type of table...
   * On the other hand SystemTable/UserTable should be just Table
   * and "System" should be an independent property.
   */
  NdbDictionary::Object::Type type;
  type = NdbDictionary::Object::UserTable;
  int n;
  for (n = 0; n < t1.getNoOfColumns(); n++) {
    const NdbDictionary::Column* c1 = t1.getColumn(n);
    const NdbDictionary::Column* c2 = t2.getColumn(n);
    require(c1 != 0 && c2 != 0);
    chk2(st_equal_column(*c1, *c2, type) == 0, "col:" << n);
  }
  chk1(t1.getNoOfPrimaryKeys() == t2.getNoOfPrimaryKeys());
  chk1(t1.getTemporary() == t2.getTemporary());
  chk1(t1.getForceVarPart() == t2.getForceVarPart());
  return 0;
err:
  return -1;
}

static int
st_equal_index(const NdbDictionary::Index& i1, const NdbDictionary::Index& i2)
{
  chk1(strcmp(i1.getName(), i2.getName()) == 0);
  require(i1.getTable() != 0 && i2.getTable() != 0);
  chk1(strcmp(i1.getTable(), i2.getTable()) == 0);
  chk1(i1.getNoOfColumns() == i2.getNoOfColumns());
  chk1(i1.getType() == i2.getType());
  NdbDictionary::Object::Type type;
  type = (NdbDictionary::Object::Type)i1.getType();
  int n;
  for (n = 0; n < (int)i1.getNoOfColumns(); n++) {
    const NdbDictionary::Column* c1 = i1.getColumn(n);
    const NdbDictionary::Column* c2 = i2.getColumn(n);
    require(c1 != 0 && c2 != 0);
    chk2(st_equal_column(*c1, *c2, type) == 0, "col:" << n);
  }
  chk1(i1.getLogging() == i2.getLogging());
  chk1(i1.getTemporary() == i2.getTemporary());
  return 0;
err:
  return -1;
}

// verify against database objects (hits all nodes randomly)

static int
st_verify_table(ST_Con& c, ST_Tab& tab)
{
  c.dic->invalidateTable(tab.name);
  const NdbDictionary::Table* pTab = c.dic->getTable(tab.name);
  tab.tab_r = pTab;
  if (tab.exists()) {
    chk2(pTab != 0, c.dic->getNdbError());
    chk1(st_equal_table(*tab.tab, *pTab) == 0);
    tab.id = pTab->getObjectId();
    g_info << tab << ": verified exists tx_on:" << c.tx_on << endl;
  } else {
    chk2(pTab == 0, tab);
    chk2(c.dic->getNdbError().code == 723, c.dic->getNdbError());
    g_info << tab << ": verified not exists tx_on:" << c.tx_on << endl;
    tab.id = -1;
  }
  return 0;
err:
  return -1;
}

static int
st_verify_index(ST_Con& c, ST_Ind& ind)
{
  ST_Tab& tab = *ind.tab;
  c.dic->invalidateIndex(ind.name, tab.name);
  const NdbDictionary::Index* pInd = c.dic->getIndex(ind.name, tab.name);
  ind.ind_r = pInd;
  if (ind.exists()) {
    chk2(pInd != 0, c.dic->getNdbError());
    chk1(st_equal_index(*ind.ind, *pInd) == 0);
    ind.id = pInd->getObjectId();
    g_info << ind << ": verified exists tx_on:" << c.tx_on << endl;
  } else {
    chk2(pInd == 0, ind);
    chk2(c.dic->getNdbError().code == 4243, c.dic->getNdbError());
    g_info << ind << ": verified not exists tx_on:" << c.tx_on << endl;
    ind.id = -1;
  }
  return 0;
err:
  return -1;
}

static int
st_verify_all(ST_Con& c)
{
  chk1(st_verify_list(c) == 0);
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_verify_table(c, tab) == 0);
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      chk1(st_verify_index(c, ind) == 0);
    }
  }
  return 0;
err:
  return -1;
}

// subroutines

static const uint
ST_CommitFlag = 0;

static const uint
ST_AbortFlag = NdbDictionary::Dictionary::SchemaTransAbort;

static const uint
ST_BackgroundFlag = NdbDictionary::Dictionary::SchemaTransBackground;

struct ST_Retry {
  int max_tries;
  int sleep_ms;
};

static int
st_begin_trans(ST_Con& c, int code = 0)
{
  g_info << "begin trans";
  if (code == 0) {
    g_info << endl;
    chk2(c.dic->beginSchemaTrans() == 0, c.dic->getNdbError());
    chk1(c.dic->hasSchemaTrans() == true);
    c.tx_on = true;
  } else {
    g_info << " - expect error " << code << endl;
    chk1(c.dic->beginSchemaTrans() == -1);
    const NdbError& error = c.dic->getNdbError();
    chk2(error.code == code, error << " wanted: " << code);
  }
  return 0;
err:
  return -1;
}

static int
st_begin_trans(ST_Con& c, ST_Errins errins)
{
  require(errins.code != 0);
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_begin_trans(c, errins.code) == 0);
  return 0;
err:
  return -1;
}

static int
st_begin_trans(ST_Con& c, ST_Retry retry)
{
  int tries = 0;
  while (++tries <= retry.max_tries) {
    int code = 0;
    if (c.dic->beginSchemaTrans() == -1) {
      code = c.dic->getNdbError().code;
      require(code != 0);
    }
    chk2(code == 0 || code == 780 || code == 701, c.dic->getNdbError());
    if (code == 0) {
      chk1(c.dic->hasSchemaTrans() == true);
      g_info << "begin trans at try " << tries << endl;
      break;
    }
    NdbSleep_MilliSleep(retry.sleep_ms);
  }
  return 0;
err:
  return -1;
}

static int
st_end_trans(ST_Con& c, uint flags)
{
  g_info << "end trans flags:" << hex << flags << endl;
  int res= c.dic->endSchemaTrans(flags);
  g_info << "end trans result:" << res << endl;
  chk2(res == 0, c.dic->getNdbError());
  c.tx_on = false;
  c.tx_commit = !(flags & ST_AbortFlag);
  st_set_commit_all(c);
  return 0;
err:
  return -1;
}

static int
st_end_trans_aborted(ST_Con& c, uint flags)
{
  g_info << "end trans flags:" << hex << flags << endl;
  int res= c.dic->endSchemaTrans(flags);
  g_info << "end trans result:" << res << endl;
  if (flags & ST_AbortFlag)
    chk1(res == 0);
  else
    chk1(res != 0);
  c.tx_on = false;
  c.tx_commit = (flags & ST_AbortFlag);
  return 0;
err:
  return -1;
}

static int
st_end_trans(ST_Con& c, ST_Errins errins, uint flags)
{
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_end_trans(c, flags) == 0);
  return 0;
err:
  return -1;
}

static int
st_end_trans_aborted(ST_Con& c, ST_Errins errins, uint flags)
{
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_end_trans_aborted(c, flags) == 0);
  return 0;
err:
  return -1;
}

static int
st_load_table(ST_Con& c, ST_Tab& tab, int rows = 1000)
{
  g_info << tab.name << ": load data rows:" << rows << endl;
  chk1(tab.tab_r != NULL);
  {
    HugoTransactions ht(*tab.tab_r);
    chk1(ht.loadTable(c.ndb, rows) == 0);
  }
  return 0;
err:
  return -1;
}

static int
st_create_table(ST_Con& c, ST_Tab& tab, int code = 0)
{
  g_info << tab.name << ": create table";
  if (code == 0) {
    g_info << endl;
    require(!tab.exists());
    chk2(c.dic->createTable(*tab.tab) == 0, c.dic->getNdbError());
    g_info << tab.name << ": created" << endl;
    st_set_create_tab(c, tab, true);
  }
  else {
    g_info << " - expect error " << code << endl;
    chk1(c.dic->createTable(*tab.tab) == -1);
    const NdbError& error = c.dic->getNdbError();
    chk2(error.code == code, error << " wanted: " << code);
  }
  chk1(st_verify_table(c, tab) == 0);
  return 0;
err:
  return -1;
}

static int
st_create_table(ST_Con& c, ST_Tab& tab, ST_Errins errins)
{
  require(errins.code != 0);
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_create_table(c, tab, errins.code) == 0);
  return 0;
err:
  return -1;
}

static int
st_drop_table(ST_Con& c, ST_Tab& tab, int code = 0)
{
  g_info << tab.name << ": drop table";
  if (code == 0) {
    g_info << endl;
    require(tab.exists());
    c.dic->invalidateTable(tab.name);
    chk2(c.dic->dropTable(tab.name) == 0, c.dic->getNdbError());
    g_info << tab.name << ": dropped" << endl;
    st_set_create_tab(c, tab, false);
  } else {
    g_info << " - expect error " << code << endl;
    c.dic->invalidateTable(tab.name);
    chk1(c.dic->dropTable(tab.name) == -1);
    const NdbError& error = c.dic->getNdbError();
    chk2(error.code == code, error << " wanted: " << code);
  }
  chk1(st_verify_table(c, tab) == 0);
  return 0;
err:
  return -1;
}

static int
st_drop_table(ST_Con& c, ST_Tab& tab, ST_Errins errins)
{
  require(errins.code != 0);
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_drop_table(c, tab, errins.code) == 0);
  return 0;
err:
  return -1;
}

static int
st_create_index(ST_Con& c, ST_Ind& ind, int code = 0)
{
  ST_Tab& tab = *ind.tab;
  g_info << ind.name << ": create index on "
         << tab.name << "(" << ind.colnames.c_str() << ")";
  if (code == 0) {
    g_info << endl;
    require(!ind.exists());
    chk2(c.dic->createIndex(*ind.ind, *tab.tab_r) == 0, c.dic->getNdbError());
    st_set_create_ind(c, ind, true);
    g_info << ind.name << ": created" << endl;
  } else {
    g_info << " - expect error " << code << endl;
    chk1(c.dic->createIndex(*ind.ind, *tab.tab_r) == -1);
    const NdbError& error = c.dic->getNdbError();
    chk2(error.code == code, error << " wanted: " << code);
  }
  chk1(st_verify_index(c, ind) == 0);
  return 0;
err:
  return -1;
}

static int
st_create_index(ST_Con& c, ST_Ind& ind, ST_Errins errins)
{
  require(errins.code != 0);
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_create_index(c, ind, errins.code) == 0);
  return 0;
err:
  return -1;
}

static int
st_drop_index(ST_Con& c, ST_Ind& ind, int code = 0)
{
  ST_Tab& tab = *ind.tab;
  g_info << ind.name << ": drop index";
  if (code == 0) {
    g_info << endl;
    require(ind.exists());
    c.dic->invalidateIndex(ind.name, tab.name);
    chk2(c.dic->dropIndex(ind.name, tab.name) == 0, c.dic->getNdbError());
    g_info << ind.name << ": dropped" << endl;
    st_set_create_ind(c, ind, false);
  } else {
    g_info << " expect error " << code << endl;
    c.dic->invalidateIndex(ind.name, tab.name);
    chk1(c.dic->dropIndex(ind.name, tab.name) == -1);
    const NdbError& error = c.dic->getNdbError();
    chk2(error.code == code, error << " wanted: " << code);
  }
  chk1(st_verify_index(c, ind) == 0);
  return 0;
err:
  return -1;
}

static int
st_drop_index(ST_Con& c, ST_Ind& ind, ST_Errins errins)
{
  require(errins.code != 0);
  chk1(st_do_errins(c, errins) == 0);
  chk1(st_drop_index(c, ind, errins.code) == 0);
  return 0;
err:
  return -1;
}

static int
st_create_table_index(ST_Con& c, ST_Tab& tab)
{
  chk1(st_create_table(c, tab) == 0);
  int j;
  for (j = 0; j < tab.indcount; j++) {
    ST_Ind& ind = tab.ind(j);
    chk1(st_create_index(c, ind) == 0);
  }
  return 0;
err:
  return -1;
}

// drop all

static int
st_drop_test_tables(ST_Con& c)
{
  g_info << "st_drop_test_tables" << endl;
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.exists())
      chk1(st_drop_table(c, tab) == 0);
  }
  return 0;
err:
  return -1;
}

// error insert values

static const ST_Errins
st_errins_begin_trans[] = {
  ST_Errins(6101, 780),
  ST_Errins()
};

static const ST_Errins
st_errins_end_trans1[] = {
  ST_Errins(ERR_INSERT_MASTER_FAILURE1, 0, 1),
  ST_Errins()
};

static const ST_Errins
st_errins_end_trans2[] = {
  ST_Errins(ERR_INSERT_MASTER_FAILURE2, 0, 1),
  ST_Errins()
};

static const ST_Errins
st_errins_end_trans3[] = {
  ST_Errins(ERR_INSERT_MASTER_FAILURE3, 0, 1),
  ST_Errins()
};

static const ST_Errins
st_errins_table[] = {
  ST_Errins(6111, 783),
  ST_Errins(6121, 9121),
  //ST_Errins(6131, 9131),
  ST_Errins()
};

static ST_Errins
st_errins_index[] = {
  ST_Errins(st_errins_table),
  ST_Errins(6112, 783),
  ST_Errins(6113, 783),
  ST_Errins(6114, 783),
  ST_Errins(6122, 9122),
  ST_Errins(6123, 9123),
  ST_Errins(6124, 9124),
  //ST_Errins(6132, 9131),
  //ST_Errins(6133, 9131),
  //ST_Errins(6134, 9131),
  //ST_Errins(6135, 9131),
  ST_Errins()
};

static ST_Errins
st_errins_index_create[] = {
  ST_Errins(st_errins_index),
  ST_Errins(6116, 783),
  ST_Errins(6126, 9126),
  //ST_Errins(6136, 9136),
  ST_Errins()
};

static ST_Errins
st_errins_index_drop[] = {
  ST_Errins(st_errins_index),
  ST_Errins()
};

// specific test cases

static int
st_test_create(ST_Con& c, int arg = -1)
{
  int do_abort = (arg == 1);
  int i;
  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  chk1(st_verify_list(c) == 0);
  if (!do_abort)
    chk1(st_end_trans(c, 0) == 0);
  else
    chk1(st_end_trans(c, ST_AbortFlag) == 0);
  chk1(st_verify_list(c) == 0);
  if (!do_abort)
    chk1(st_drop_test_tables(c) == 0);
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_drop(ST_Con& c, int arg = -1)
{
  int do_abort = (arg == 1);
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_drop_table(c, tab) == 0);
  }
  chk1(st_verify_list(c) == 0);
  if (!do_abort)
    chk1(st_end_trans(c, 0) == 0);
  else
    chk1(st_end_trans(c, ST_AbortFlag) == 0);
  chk1(st_verify_list(c) == 0);
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_rollback_create_table(ST_Con& c, int arg = -1)
{
  int i;
  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (i % 2 == 0) {
      ST_Errins errins(6111, 783, 0); // fail CTa seize op
      chk1(st_create_table(c, tab, errins) == 0);
    } else {
      chk1(st_create_table(c, tab) == 0);
    }
  }
  chk1(st_end_trans(c, 0) == 0);
  chk1(st_verify_list(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (i % 2 == 0)
      require(!tab.exists());
    else {
      require(tab.exists());
      chk1(st_drop_table(c, tab) == 0);
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_rollback_drop_table(ST_Con& c, int arg = -1)
{
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table(c, tab) == 0);
  }
  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (i % 2 == 0) {
      ST_Errins errins(6111, 783, 0); // fail DTa seize op
      chk1(st_drop_table(c, tab, errins) == 0);
    } else {
      chk1(st_drop_table(c, tab) == 0);
    }
  }
  chk1(st_end_trans(c, 0) == 0);
  chk1(st_verify_list(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (i % 2 == 0) {
      require(tab.exists());
      chk1(st_drop_table(c, tab) == 0);
    } else {
      require(!tab.exists());
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_rollback_create_index(ST_Con& c, int arg = -1)
{
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.indcount < 1)
      continue;
    chk1(st_create_table(c, tab) == 0);
    chk1(st_begin_trans(c) == 0);
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (j % 2 == 0) {
        ST_Errins errins(6116, 783, 0); // fail BIn seize op
        chk1(st_create_index(c, ind, errins) == 0);
      } else {
        chk1(st_create_index(c, ind) == 0);
      }
    }
    chk1(st_end_trans(c, 0) == 0);
    chk1(st_verify_list(c) == 0);
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (j % 2 == 0)
        require(!ind.exists());
      else {
        require(ind.exists());
        chk1(st_drop_index(c, ind) == 0);
      }
    }
    chk1(st_drop_table(c, tab) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_rollback_drop_index(ST_Con& c, int arg = -1)
{
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.indcount < 1)
      continue;
    chk1(st_create_table_index(c, tab) == 0);
  }
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.indcount < 1)
      continue;
    chk1(st_begin_trans(c) == 0);
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (j % 2 == 0) {
        ST_Errins errins(6114, 783, 0); // fail ATr seize op
        chk1(st_drop_index(c, ind, errins) == 0);
      } else {
        chk1(st_drop_index(c, ind) == 0);
      }
    }
    chk1(st_end_trans(c, 0) == 0);
    chk1(st_verify_list(c) == 0);
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (j % 2 == 0) {
        require(ind.exists());
        chk1(st_drop_index(c, ind) == 0);
      } else {
        require(!ind.exists());
      }
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_dup_create_table(ST_Con& c, int arg = -1)
{
  int do_trans;
  int do_abort;
  int i;
  for (do_trans = 0; do_trans <= 1; do_trans++) {
    for (do_abort = 0; do_abort <= do_trans; do_abort++) {
      g_info << "trans:" << do_trans
             << " abort:" << do_abort << endl;
      for (i = 0; i < c.tabcount; i++) {
        ST_Tab& tab = c.tab(i);
        if (do_trans)
          chk1(st_begin_trans(c) == 0);
        chk1(st_create_table(c, tab) == 0);
        chk1(st_create_table(c, tab, 721) == 0);
        if (do_trans) {
          if (!do_abort)
            chk1(st_end_trans(c, 0) == 0);
          else
            chk1(st_end_trans(c, ST_AbortFlag) == 0);
        }
        chk1(st_verify_list(c) == 0);
        if (tab.exists()) {
          chk1(st_drop_table(c, tab) == 0);
        }
      }
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_dup_drop_table(ST_Con& c, int arg = -1)
{
  int do_trans;
  int do_abort;
  int i;
  for (do_trans = 0; do_trans <= 1; do_trans++) {
    for (do_abort = 0; do_abort <= do_trans; do_abort++) {
      g_info << "trans:" << do_trans
             << " abort:" << do_abort << endl;
      for (i = 0; i < c.tabcount; i++) {
        ST_Tab& tab = c.tab(i);
        chk1(st_create_table(c, tab) == 0);
        if (do_trans)
          chk1(st_begin_trans(c) == 0);
        chk1(st_drop_table(c, tab) == 0);
        if (!do_trans)
          chk1(st_drop_table(c, tab, 723) == 0);
        else
          chk1(st_drop_table(c, tab, 785) == 0);
        if (do_trans) {
          if (!do_abort)
            chk1(st_end_trans(c, 0) == 0);
          else
            chk1(st_end_trans(c, ST_AbortFlag) == 0);
        }
        chk1(st_verify_list(c) == 0);
        if (tab.exists()) {
          chk1(st_drop_table(c, tab) == 0);
        }
      }
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_dup_create_index(ST_Con& c, int arg = -1)
{
  int do_trans;
  int do_abort;
  int i, j;
  for (do_trans = 0; do_trans <= 1; do_trans++) {
    for (do_abort = 0; do_abort <= do_trans; do_abort++) {
      g_info << "trans:" << do_trans
             << " abort:" << do_abort << endl;
      for (i = 0; i < c.tabcount; i++) {
        ST_Tab& tab = c.tab(i);
        if (tab.indcount < 1)
          continue;
        chk1(st_create_table(c, tab) == 0);
        for (j = 0; j < tab.indcount; j++) {
          ST_Ind& ind = tab.ind(j);
          if (do_trans)
            chk1(st_begin_trans(c) == 0);
          chk1(st_create_index(c, ind) == 0);
          chk1(st_create_index(c, ind, 721) == 0);
          if (do_trans) {
            if (!do_abort)
              chk1(st_end_trans(c, 0) == 0);
            else
              chk1(st_end_trans(c, ST_AbortFlag) == 0);
          }
          chk1(st_verify_list(c) == 0);
        }
        chk1(st_drop_table(c, tab) == 0);
      }
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_dup_drop_index(ST_Con& c, int arg = -1)
{
  int do_trans;
  int do_abort;
  int i, j;
  for (do_trans = 0; do_trans <= 1; do_trans++) {
    for (do_abort = 0; do_abort <= do_trans; do_abort++) {
      g_info << "trans:" << do_trans
             << " abort:" << do_abort << endl;
      for (i = 0; i < c.tabcount; i++) {
        ST_Tab& tab = c.tab(i);
        if (tab.indcount < 1)
          continue;
        chk1(st_create_table(c, tab) == 0);
        for (j = 0; j < tab.indcount; j++) {
          ST_Ind& ind = tab.ind(j);
          chk1(st_create_index(c, ind) == 0);
          if (do_trans)
            chk1(st_begin_trans(c) == 0);
          chk1(st_drop_index(c, ind) == 0);
          if (!do_trans)
            chk1(st_drop_index(c, ind, 4243) == 0);
          else
            chk1(st_drop_index(c, ind, 785) == 0);
          if (do_trans) {
            if (!do_abort)
              chk1(st_end_trans(c, 0) == 0);
            else
              chk1(st_end_trans(c, ST_AbortFlag) == 0);
          }
          chk1(st_verify_list(c) == 0);
        }
        chk1(st_drop_table(c, tab) == 0);
      }
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_build_index(ST_Con& c, int arg = -1)
{
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.indcount < 1)
      continue;
    chk1(st_create_table(c, tab) == 0);
    chk1(st_load_table(c, tab) == 0);
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      chk1(st_create_index(c, ind) == 0);
      chk1(st_verify_list(c) == 0);
    }
    chk1(st_drop_table(c, tab) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static ST_Errins
st_test_local_create_list[] = {
  ST_Errins(8033, 293, 1),    // TC trigger
  ST_Errins(8033, 293, 0),
  ST_Errins(4003, 4237, 1),   // TUP trigger
  ST_Errins(4003, 4237, 0),
  ST_Errins(8034, 292, 1),    // TC index
  ST_Errins(8034, 292, 0)
};

static int
st_test_local_create(ST_Con& c, int arg = -1)
{
  const int n = arg;
  ST_Errins *list = st_test_local_create_list;
  const int listlen = 
    sizeof(st_test_local_create_list)/sizeof(st_test_local_create_list[0]);
  require(0 <= n && n < listlen);
  const bool only_unique = (n == 0 || n == 1 || n == 4 || n == 5);
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    bool tabdone = false;
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (only_unique && !ind.is_unique())
        continue;
      if (!tabdone) {
        chk1(st_create_table(c, tab) == 0);
        chk1(st_load_table(c, tab) == 0);
        tabdone = true;
      }
      ST_Errins errins = list[n];
      chk1(st_create_index(c, ind, errins) == 0);
      chk1(st_verify_list(c) == 0);
    }
    if (tabdone)
      chk1(st_drop_table(c, tab) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

// random test cases

static const uint ST_AllowAbort = 1;
static const uint ST_AllowErrins = 2;

static int
st_test_trans(ST_Con& c, int arg = -1)
{
  if ((arg & ST_AllowErrins) && randomly(2, 3)) {
    ST_Errins errins = st_get_errins(c, st_errins_begin_trans);
    chk1(st_begin_trans(c, errins) == 0);
  } else {
    chk1(st_begin_trans(c) == 0);
    if (randomly(1, 5)) {
      g_info << "try duplicate begin trans" << endl;
      chk1(st_begin_trans(c, 4410) == 0);
      chk1(c.dic->hasSchemaTrans() == true);
    }
    if ((arg & ST_AllowAbort) && randomly(1, 3)) {
      chk1(st_end_trans(c, ST_AbortFlag) == 0);
    } else {
      chk1(st_end_trans(c, 0) == 0);
    }
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_create_table(ST_Con& c, int arg = -1)
{
  bool trans = randomly(3, 4);
  bool simpletrans = !trans && randomly(1, 2);
  g_info << "trans:" << trans << " simpletrans:" << simpletrans << endl;
  if (trans) {
    chk1(st_begin_trans(c) == 0);
  }
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.exists()) {
      g_info << tab.name << ": skip existing" << endl;
      continue;
    }
    g_info << tab.name << ": to create" << endl;
    if (simpletrans) {
      chk1(st_begin_trans(c) == 0);
    }
    if ((arg & ST_AllowErrins) && randomly(1, 3)) {
      ST_Errins errins = st_get_errins(c, st_errins_table);
      chk1(st_create_table(c, tab, errins) == 0);
      if (simpletrans) {
        if (randomly(1, 2))
          chk1(st_end_trans(c, 0) == 0);
        else
          chk1(st_end_trans(c, ST_AbortFlag) == 0);
      }
    } else {
      chk1(st_create_table(c, tab) == 0);
      if (simpletrans) {
        uint flags = 0;
        if ((arg & ST_AllowAbort) && randomly(4, 5))
          flags |= ST_AbortFlag;
        chk1(st_end_trans(c, flags) == 0);
      }
    }
    if (tab.exists() && randomly(1, 3)) {
      g_info << tab.name << ": try duplicate create" << endl;
      chk1(st_create_table(c, tab, 721) == 0);
    }
  }
  if (trans) {
    uint flags = 0;
    if ((arg & ST_AllowAbort) && randomly(4, 5))
      flags |= ST_AbortFlag;
    chk1(st_end_trans(c, flags) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_drop_table(ST_Con& c, int arg = -1)
{
  bool trans = randomly(3, 4);
  bool simpletrans = !trans && randomly(1, 2);
  g_info << "trans:" << trans << " simpletrans:" << simpletrans << endl;
  if (trans) {
    chk1(st_begin_trans(c) == 0);
  }
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (!tab.exists()) {
      g_info << tab.name << ": skip not existing" << endl;
      continue;
    }
    g_info << tab.name << ": to drop" << endl;
    if (simpletrans) {
      chk1(st_begin_trans(c) == 0);
    }
    if ((arg & ST_AllowErrins) && randomly(1, 3)) {
      ST_Errins errins = st_get_errins(c, st_errins_table);
      chk1(st_drop_table(c, tab, errins) == 0);
      if (simpletrans) {
        if (randomly(1, 2))
          chk1(st_end_trans(c, 0) == 0);
        else
          chk1(st_end_trans(c, ST_AbortFlag) == 0);
      }
    } else {
      chk1(st_drop_table(c, tab) == 0);
      if (simpletrans) {
        uint flags = 0;
        if ((arg & ST_AllowAbort) && randomly(4, 5))
          flags |= ST_AbortFlag;
        chk1(st_end_trans(c, flags) == 0);
      }
    }
    if (!tab.exists() && randomly(1, 3)) {
      g_info << tab.name << ": try duplicate drop" << endl;
      chk1(st_drop_table(c, tab, 723) == 0);
    }
  }
  if (trans) {
    uint flags = 0;
    if ((arg & ST_AllowAbort) && randomly(4, 5))
      flags |= ST_AbortFlag;
    chk1(st_end_trans(c, flags) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_table(ST_Con& c, int arg = -1)
{
  chk1(st_test_create_table(c) == NDBT_OK);
  chk1(st_test_drop_table(c) == NDBT_OK);
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_create_index(ST_Con& c, int arg = -1)
{
  bool trans = randomly(3, 4);
  bool simpletrans = !trans && randomly(1, 2);
  g_info << "trans:" << trans << " simpletrans:" << simpletrans << endl;
  if (trans) {
    chk1(st_begin_trans(c) == 0);
  }
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.indcount == 0)
      continue;
    if (!tab.exists()) {
      g_info << tab.name << ": to create" << endl;
      chk1(st_create_table(c, tab) == 0);
    }
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (ind.exists()) {
        g_info << ind.name << ": skip existing" << endl;
        continue;
      }
      g_info << ind.name << ": to create" << endl;
      if (simpletrans) {
        chk1(st_begin_trans(c) == 0);
      }
      if ((arg & ST_AllowErrins) && randomly(1, 3)) {
        const ST_Errins* list = st_errins_index_create;
        ST_Errins errins = st_get_errins(c, list);
        chk1(st_create_index(c, ind, errins) == 0);
        if (simpletrans) {
          if (randomly(1, 2))
            chk1(st_end_trans(c, 0) == 0);
          else
            chk1(st_end_trans(c, ST_AbortFlag) == 0);
        }
      } else {
        chk1(st_create_index(c, ind) == 0);
        if (simpletrans) {
          uint flags = 0;
          if ((arg & ST_AllowAbort) && randomly(4, 5))
            flags |= ST_AbortFlag;
          chk1(st_end_trans(c, flags) == 0);
        }
      }
      if (ind.exists() && randomly(1, 3)) {
        g_info << ind.name << ": try duplicate create" << endl;
        chk1(st_create_index(c, ind, 721) == 0);
      }
    }
  }
  if (trans) {
    uint flags = 0;
    if ((arg & ST_AllowAbort) && randomly(4, 5))
      flags |= ST_AbortFlag;
    chk1(st_end_trans(c, flags) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_drop_index(ST_Con& c, int arg = -1)
{
  bool trans = randomly(3, 4);
  bool simpletrans = !trans && randomly(1, 2);
  g_info << "trans:" << trans << " simpletrans:" << simpletrans << endl;
  if (trans) {
    chk1(st_begin_trans(c) == 0);
  }
  int i, j;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (tab.indcount == 0)
      continue;
    if (!tab.exists()) {
      g_info << tab.name << ": skip not existing" << endl;
      continue;
    }
    for (j = 0; j < tab.indcount; j++) {
      ST_Ind& ind = tab.ind(j);
      if (!ind.exists()) {
        g_info << ind.name << ": skip not existing" << endl;
        continue;
      }
      g_info << ind.name << ": to drop" << endl;
      if (simpletrans) {
        chk1(st_begin_trans(c) == 0);
      }
      if ((arg & ST_AllowErrins) && randomly(1, 3)) {
        const ST_Errins* list = st_errins_index_drop;
        ST_Errins errins = st_get_errins(c, list);
        chk1(st_drop_index(c, ind, errins) == 0);
        if (simpletrans) {
          if (randomly(1, 2))
            chk1(st_end_trans(c, 0) == 0);
          else
            chk1(st_end_trans(c, ST_AbortFlag) == 0);
        }
      } else {
        chk1(st_drop_index(c, ind) == 0);
        if (simpletrans) {
          uint flags = 0;
          if ((arg & ST_AllowAbort) && randomly(4, 5))
            flags |= ST_AbortFlag;
          chk1(st_end_trans(c, flags) == 0);
        }
      }
      if (!ind.exists() && randomly(1, 3)) {
        g_info << ind.name << ": try duplicate drop" << endl;
        chk1(st_drop_index(c, ind, 4243) == 0);
      }
    }
  }
  if (trans) {
    uint flags = 0;
    if ((arg & ST_AllowAbort) && randomly(4, 5))
      flags |= ST_AbortFlag;
    chk1(st_end_trans(c, flags) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_index(ST_Con& c, int arg = -1)
{
  chk1(st_test_create_index(c) == NDBT_OK);
  chk1(st_test_drop_index(c) == NDBT_OK);
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

// node failure and system restart

static int
st_test_anf_parse(ST_Con& c, int arg = -1)
{
  int i;
  chk1(st_start_xcon(c) == 0);
  {
    ST_Con& xc = *c.xcon;
    chk1(st_begin_trans(xc) == 0);
    for (i = 0; i < c.tabcount; i++) {
      ST_Tab& tab = c.tab(i);
      chk1(st_create_table_index(xc, tab) == 0);
    }
    // DICT aborts the trans
    xc.tx_on = false;
    xc.tx_commit = false;
    st_set_commit_all(xc);
    chk1(st_stop_xcon(c) == 0);
    chk1(st_wait_idle(c) == 0);
    chk1(st_verify_list(c) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_anf_background(ST_Con& c, int arg = -1)
{
  int i;
  chk1(st_start_xcon(c) == 0);
  {
    ST_Con& xc = *c.xcon;
    chk1(st_begin_trans(xc) == 0);
    for (i = 0; i < c.tabcount; i++) {
      ST_Tab& tab = c.tab(i);
      chk1(st_create_table(xc, tab) == 0);
    }
    // DICT takes over and completes the trans
    st_end_trans(xc, ST_BackgroundFlag);
    chk1(st_stop_xcon(c) == 0);
    chk1(st_wait_idle(c) == 0);
    chk1(st_verify_list(c) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_anf_fail_begin(ST_Con& c, int arg = -1)
{
  chk1(st_start_xcon(c) == 0);
  {
    ST_Con& xc = *c.xcon;

    ST_Errins errins1(6102, -1, 1); // master kills us at begin
    ST_Errins errins2(6103, -1, 0); // slave delays conf
    chk1(st_do_errins(xc, errins1) == 0);
    chk1(st_do_errins(xc, errins2) == 0);

    chk1(st_begin_trans(xc, 4009) == 0);

    // DICT aborts the trans
    xc.tx_on = false;
    xc.tx_commit = false;
    st_set_commit_all(xc);
    chk1(st_stop_xcon(c) == 0);

    // xc may get 4009 before takeover is ready (5000 ms delay)
    ST_Retry retry = { 100, 100 }; // 100 * 100ms = 10000ms
    chk1(st_begin_trans(c, retry) == 0);
    chk1(st_wait_idle(c) == 0);
    chk1(st_verify_list(c) == 0);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_snf_parse(ST_Con& c, int arg = -1)
{
  bool do_abort = (arg == 1);
  chk1(st_begin_trans(c) == 0);
  int node_id;
  node_id = -1;
  int i;
  int midcount;
  midcount = c.tabcount / 2;

  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (i == midcount) {
      require(c.numdbnodes > 1);
      uint rand = urandom(c.numdbnodes);
      node_id = c.restarter->getRandomNotMasterNodeId(rand);
      g_info << "restart node " << node_id << " (async)" << endl;
      const int flags = NdbRestarter::NRRF_NOSTART;
      chk1(c.restarter->restartOneDbNode2(node_id, flags) == 0);
      chk1(c.restarter->waitNodesNoStart(&node_id, 1) == 0);
      chk1(c.restarter->startNodes(&node_id, 1) == 0);
    }
    chk1(st_create_table_index(c, tab) == 0);
  }
  if (!do_abort)
    chk1(st_end_trans(c, 0) == 0);
  else
    chk1(st_end_trans(c, ST_AbortFlag) == 0);

  g_info << "wait for node " << node_id << " to come up" << endl;
  chk1(c.restarter->waitClusterStarted() == 0);
  g_info << "verify all" << endl;
  chk1(st_verify_all(c) == 0);
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_parse(ST_Con& c, int arg = -1)
{
  const NdbDictionary::Table* pTab;
  bool do_abort = (arg == 1);
  chk1(st_begin_trans(c) == 0);
  int node_id;
  node_id = -1;
  int i;
  int midcount;
  midcount = c.tabcount / 2;

  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
    if (i == midcount) {
      require(c.numdbnodes > 1);
      node_id = c.restarter->getMasterNodeId();
      g_info << "restart node " << node_id << " (async)" << endl;
      const int flags = NdbRestarter::NRRF_NOSTART;
      chk1(c.restarter->restartOneDbNode2(node_id, flags) == 0);
      chk1(c.restarter->waitNodesNoStart(&node_id, 1) == 0);
      chk1(c.restarter->startNodes(&node_id, 1) == 0);
      break;
    }
  }
  if (!do_abort)
    chk1(st_end_trans_aborted(c, ST_CommitFlag) == 0);
  else
    chk1(st_end_trans_aborted(c, ST_AbortFlag) == 0);

  g_info << "wait for node " << node_id << " to come up" << endl;
  chk1(c.restarter->waitClusterStarted() == 0);
  g_info << "verify all" << endl;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    // Verify that table is not in db
    c.dic->invalidateTable(tab.name);
    pTab =
      NDBT_Table::discoverTableFromDb(c.ndb, tab.name);
    chk1(pTab == NULL);
  }
/*
  chk1(st_verify_all(c) == 0);
*/
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_prepare(ST_Con& c, int arg = -1)
{
  NdbRestarter restarter;
  //int master = restarter.getMasterNodeId();
  ST_Errins errins = st_get_errins(c, st_errins_end_trans1);
  int i;

  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  if (arg == 1)
  {
    chk1(st_end_trans_aborted(c, errins, ST_BackgroundFlag) == 0);
    chk1(st_wait_idle(c) == 0);
  }
  else
    chk1(st_end_trans_aborted(c, errins, ST_CommitFlag) == 0);
  chk1(c.restarter->waitClusterStarted() == 0);
  //st_wait_db_node_up(c, master);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    // Verify that table is not in db
    c.dic->invalidateTable(tab.name);
    const NdbDictionary::Table* pTab =
      NDBT_Table::discoverTableFromDb(c.ndb, tab.name);
    chk1(pTab == NULL);
  }
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_commit1(ST_Con& c, int arg = -1)
{
  NdbRestarter restarter;
  //int master = restarter.getMasterNodeId();
  ST_Errins errins = st_get_errins(c, st_errins_end_trans2);
  int i;

  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  if (arg == 1)
  {
    chk1(st_end_trans(c, errins, ST_BackgroundFlag) == 0);
    chk1(st_wait_idle(c) == 0);
  }
  else
    chk1(st_end_trans(c, errins, ST_CommitFlag) == 0);
  chk1(c.restarter->waitClusterStarted() == 0);
  //st_wait_db_node_up(c, master);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_verify_table(c, tab) == 0);
  }
  chk1(st_drop_test_tables(c) == 0);  
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_commit2(ST_Con& c, int arg = -1)
{
  NdbRestarter restarter;
  //int master = restarter.getMasterNodeId();
  ST_Errins errins = st_get_errins(c, st_errins_end_trans3);
  int i;

  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  if (arg == 1)
  {
    chk1(st_end_trans(c, errins, ST_BackgroundFlag) == 0);
    chk1(st_wait_idle(c) == 0);
  }
  else
    chk1(st_end_trans(c, errins, ST_CommitFlag) == 0);
  chk1(c.restarter->waitClusterStarted() == 0);
  //st_wait_db_node_up(c, master);
  chk1(st_verify_all(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_load_table(c, tab) == 0);
  }
  chk1(st_drop_test_tables(c) == 0);  
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_run_commit(ST_Con& c, int arg = -1)
{
  const NdbDictionary::Table* pTab;
  NdbRestarter restarter;
  //int master = restarter.getMasterNodeId();
  int i;

  if (arg == FAIL_BEGIN)
  {
    // No transaction to be found if only one node left
    if (restarter.getNumDbNodes() < 3)
      return NDBT_OK;
    chk1(st_begin_trans(c) == -1);
    goto verify;
  }
  else
    chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    if (arg == FAIL_CREATE)
    {
      chk1(st_create_table_index(c, tab) == -1);
      goto verify;
    }
    else
      chk1(st_create_table_index(c, tab) == 0);
  }
  if (arg == FAIL_END)
  {
    chk1(st_end_trans(c, ST_CommitFlag) == -1);
  }
  else // if (arg == SUCCEED_COMMIT)
    chk1(st_end_trans(c, ST_CommitFlag) == 0);

verify:
  g_info << "wait for master node to come up" << endl;
  chk1(c.restarter->waitClusterStarted() == 0);
  //st_wait_db_node_up(c, master);
  g_info << "verify all" << endl;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    switch (arg) {
    case FAIL_BEGIN:
    case FAIL_CREATE:
    case FAIL_END:
    {
      // Verify that table is not in db
      c.dic->invalidateTable(tab.name);
      pTab =
        NDBT_Table::discoverTableFromDb(c.ndb, tab.name);
      chk1(pTab == NULL);
      break;
    }
    default:
      chk1(st_verify_table(c, tab) == 0);
    }
  }

  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_run_abort(ST_Con& c, int arg = -1)
{
  NdbRestarter restarter;
  //int master = restarter.getMasterNodeId();
  const NdbDictionary::Table* pTab;
  bool do_abort = (arg == SUCCEED_ABORT);
  int i;

  chk1(st_begin_trans(c) == 0);
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  if (!do_abort)
    chk1(st_end_trans(c, ST_CommitFlag) == -1);
  else
    chk1(st_end_trans_aborted(c, ST_AbortFlag) == 0);

  g_info << "wait for master node to come up" << endl;
  chk1(c.restarter->waitClusterStarted() == 0);
  //st_wait_db_node_up(c, master);
  g_info << "verify all" << endl;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    // Verify that table is not in db
    c.dic->invalidateTable(tab.name);
    pTab =
      NDBT_Table::discoverTableFromDb(c.ndb, tab.name);
    chk1(pTab == NULL);
  }

  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int
st_test_mnf_start_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_START_FAIL, 0, 1); // slave skips start
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_parse_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_PARSE_FAIL, 0, 1); // slave skips parse
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_flush_prepare_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_FLUSH_PREPARE_FAIL, 0, 1); // slave skips flush prepare
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_prepare_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_PREPARE_FAIL, 0, 1); // slave skips prepare
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_abort_parse_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_ABORT_PARSE_FAIL, 0, 1); // slave skips abort parse
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_abort(c, arg);
err:
  return -1;
}

static int
st_test_mnf_abort_prepare_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_ABORT_PREPARE_FAIL, 0, 1); // slave skips abort prepare
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_abort(c, arg);
err:
  return -1;
}

static int
st_test_mnf_flush_commit_partial(ST_Con& c, int arg = -1)
{
  NdbRestarter restarter;
  ST_Errins errins(ERR_INSERT_PARTIAL_FLUSH_COMMIT_FAIL, 0, 1); // slave skips flush commit
  chk1(st_do_errins(c, errins) == 0);
  if (restarter.getNumDbNodes() < 3)
    // If new master is only node and it hasn't flush commit, we abort
    return st_test_mnf_run_commit(c, FAIL_END);
  else
    return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_commit_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_COMMIT_FAIL, 0, 1); // slave skips commit
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_flush_complete_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_FLUSH_COMPLETE_FAIL, 0, 1); // slave skips flush complete
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_complete_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_COMPLETE_FAIL, 0, 1); // slave skips complete
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_mnf_end_partial(ST_Con& c, int arg = -1)
{
  ST_Errins errins(ERR_INSERT_PARTIAL_END_FAIL, 0, 1); // slave skips end
  chk1(st_do_errins(c, errins) == 0);
  return st_test_mnf_run_commit(c, arg);
err:
  return -1;
}

static int
st_test_sr_parse(ST_Con& c, int arg = -1)
{
  bool do_abort = (arg == 1);
  chk1(st_begin_trans(c) == 0);
  int i;
  for (i = 0; i < c.tabcount; i++) {
    ST_Tab& tab = c.tab(i);
    chk1(st_create_table_index(c, tab) == 0);
  }
  if (!do_abort)
    chk1(st_end_trans(c, 0) == 0);
  else
    chk1(st_end_trans(c, ST_AbortFlag) == 0);

  g_info << "restart all" << endl;
  int flags;
  flags = NdbRestarter::NRRF_NOSTART;
  chk1(c.restarter->restartAll2(flags) == 0);
  g_info << "wait for cluster started" << endl;
  chk1(c.restarter->waitClusterNoStart() == 0);
  chk1(c.restarter->startAll() == 0);
  chk1(c.restarter->waitClusterStarted() == 0);
  g_info << "verify all" << endl;
  chk1(st_verify_all(c) == 0);
  return NDBT_OK;
err:
  return NDBT_FAILED;
}

#if 0
static int
st_test_sr_commit(ST_Con& c, int arg = -1)
{
  g_info << "not yet" << endl;
  return NDBT_OK;
}
#endif

// run test cases

struct ST_Test {
  const char* key;
  int mindbnodes;
  int arg;
  int (*func)(ST_Con& c, int arg);
  const char* name;
  const char* desc;
};

static NdbOut&
operator<<(NdbOut& out, const ST_Test& test)
{
  out << "CASE " << test.key;
  out << " " << test.name;
  if (test.arg != -1)
    out << "+" << test.arg;
  out << " - " << test.desc;
  return out;
}

static const ST_Test
st_test_list[] = {
#define func(f) f, #f
  // specific ops
  { "a1", 1, 0,
     func(st_test_create),
     "create all within trans, commit" },
  { "a2", 1, 1,
     func(st_test_create),
     "create all within trans, abort" },
  { "a3", 1, 0,
     func(st_test_drop),
     "drop all within trans, commit" },
  { "a4", 1, 1,
     func(st_test_drop),
     "drop all within trans, abort" },
  { "b1", 1, -1,
    func(st_test_rollback_create_table),
    "partial rollback of create table ops" },
  { "b2", 1, -1,
    func(st_test_rollback_drop_table),
    "partial rollback of drop table ops" },
  { "b3", 1, -1,
    func(st_test_rollback_create_index),
    "partial rollback of create index ops" },
  { "b4", 1, -1,
    func(st_test_rollback_drop_index),
    "partial rollback of drop index ops" },
  { "c1", 1, -1,
    func(st_test_dup_create_table),
    "try to create same table twice" },
  { "c2", 1, -1,
    func(st_test_dup_drop_table),
    "try to drop same table twice" },
  { "c3", 1, -1,
    func(st_test_dup_create_index),
    "try to create same index twice" },
  { "c4", 1, -1,
    func(st_test_dup_drop_index),
    "try to drop same index twice" },
  { "d1", 1, -1,
    func(st_test_build_index),
    "build index on non-empty table" },
  { "e1", 1, 0,
    func(st_test_local_create),
    "fail trigger create in TC, master errins 8033" },
  { "e2", 2, 1,
    func(st_test_local_create),
    "fail trigger create in TC, slave errins 8033" },
  { "e3", 1, 2,
    func(st_test_local_create),
    "fail trigger create in TUP, master errins 4003" },
  { "e4", 2, 3,
    func(st_test_local_create),
    "fail trigger create in TUP, slave errins 4003" },
  { "e5", 1, 4,
    func(st_test_local_create),
    "fail index create in TC, master errins 8034" },
  { "e6", 2, 5,
    func(st_test_local_create),
    "fail index create in TC, slave errins 8034" },
  // random ops
  { "o1", 1, 0,
    func(st_test_trans),
    "start and stop schema trans" },
  { "o2", 1, ST_AllowAbort,
    func(st_test_trans),
    "start and stop schema trans, allow abort" },
  { "o3", 1, ST_AllowAbort | ST_AllowErrins,
    func(st_test_trans),
    "start and stop schema trans, allow abort errins" },
  //
  { "p1", 1, 0,
    func(st_test_create_table),
    "create tables at random" },
  { "p2", 1, ST_AllowAbort,
    func(st_test_create_table),
    "create tables at random, allow abort" },
  { "p3", 1, ST_AllowAbort | ST_AllowErrins,
    func(st_test_create_table),
    "create tables at random, allow abort errins" },
  //
  { "p4", 1, 0,
    func(st_test_table),
    "create and drop tables at random" },
  { "p5", 1, ST_AllowAbort,
    func(st_test_table),
    "create and drop tables at random, allow abort" },
  { "p6", 1, ST_AllowAbort | ST_AllowErrins,
    func(st_test_table),
    "create and drop tables at random, allow abort errins" },
  //
  { "q1", 1, 0,
    func(st_test_create_index),
    "create indexes at random" },
  { "q2", 1, ST_AllowAbort,
    func(st_test_create_index),
    "create indexes at random, allow abort" },
  { "q3", 1, ST_AllowAbort | ST_AllowErrins,
    func(st_test_create_index),
    "create indexes at random, allow abort errins" },
  //
  { "q4", 1, 0,
    func(st_test_index),
    "create and drop indexes at random" },
  { "q5", 1, ST_AllowAbort,
    func(st_test_index),
    "create and drop indexes at random, allow abort" },
  { "q6", 1, ST_AllowAbort | ST_AllowErrins,
    func(st_test_index),
    "create and drop indexes at random, allow abort errins" },
  // node failure and system restart
  { "u1", 1, -1,
    func(st_test_anf_parse),
    "api node fail in parse phase" },
  { "u2", 1, -1,
    func(st_test_anf_background),
    "api node fail after background trans" },
  { "u3", 2, -1,
    func(st_test_anf_fail_begin),
    "api node fail in middle of kernel begin trans" },
  //
  { "v1", 2, 0,
    func(st_test_snf_parse),
    "slave node fail in parse phase, commit" },
  { "v2", 2, 1,
    func(st_test_snf_parse),
    "slave node fail in parse phase, abort" },
  { "w1", 1, 0,
    func(st_test_sr_parse),
    "system restart in parse phase, commit" },
  { "w2", 1, 1,
    func(st_test_sr_parse),
    "system restart in parse phase, abort" },
#ifdef ndb_master_failure
  { "x1", 2, 0,
    func(st_test_mnf_parse),
    "master node fail in parse phase, commit" },
  { "x2", 2, 1,
    func(st_test_mnf_parse),
    "master node fail in parse phase, abort" },
  { "x3", 2, 0,
    func(st_test_mnf_prepare),
    "master node fail in prepare phase" },
  { "x4", 2, 0,
    func(st_test_mnf_commit1),
    "master node fail in start of commit phase" },
  { "x5", 2, 0,
    func(st_test_mnf_commit2),
    "master node fail in end of commit phase" },
  { "y1", 2, SUCCEED_COMMIT,
    func(st_test_mnf_start_partial),
    "master node fail in start phase, retry will succeed" },
  { "y2", 2, FAIL_CREATE,
    func(st_test_mnf_parse_partial),
    "master node fail in parse phase, partial rollback" },
  { "y3", 2, FAIL_END,
    func(st_test_mnf_flush_prepare_partial),
    "master node fail in flush prepare phase, partial rollback" },
  { "y4", 2, FAIL_END,
    func(st_test_mnf_prepare_partial),
    "master node fail in prepare phase, partial rollback" },
  { "y5", 2, SUCCEED_COMMIT,
    func(st_test_mnf_flush_commit_partial),
    "master node fail in flush commit phase, partial rollback" },
  { "y6", 2, SUCCEED_COMMIT,
    func(st_test_mnf_commit_partial),
    "master node fail in commit phase, commit, partial rollforward" },
  { "y7", 2, SUCCEED_COMMIT,
    func(st_test_mnf_flush_complete_partial),
    "master node fail in flush complete phase, commit, partial rollforward" },
  { "y8", 2, SUCCEED_COMMIT,
    func(st_test_mnf_complete_partial),
    "master node fail in complete phase, commit, partial rollforward" },
  { "y9", 2, SUCCEED_COMMIT,
    func(st_test_mnf_end_partial),
    "master node fail in end phase, commit, partial rollforward" },
  { "z1", 2, SUCCEED_ABORT,
    func(st_test_mnf_abort_parse_partial),
    "master node fail in abort parse phase, partial rollback" },
  { "z2", 2, FAIL_END,
    func(st_test_mnf_abort_prepare_partial),
    "master node fail in abort prepare phase, partial rollback" },
  { "z3", 2, 1,
    func(st_test_mnf_prepare),
    "master node fail in prepare phase in background" },
  { "z4", 2, 1,
    func(st_test_mnf_commit1),
    "master node fail in start of commit phase in background" },
  { "z5", 2, 1,
    func(st_test_mnf_commit2),
    "master node fail in end of commit phase in background" },

#endif
#undef func
};

static const int
st_test_count = sizeof(st_test_list)/sizeof(st_test_list[0]);

static const char* st_test_case = 0;
static const char* st_test_skip = 0;

static bool
st_test_match(const ST_Test& test)
{
  const char* p = 0;
  if (st_test_case == 0)
    goto skip;
  if (strstr(st_test_case, test.key) != 0)
    goto skip;
  p = strchr(st_test_case, test.key[0]);
  if (p != 0 && (p[1] < '0' || p[1] > '9'))
    goto skip;
  return false;
skip:
  if (st_test_skip == 0)
    return true;
  if (strstr(st_test_skip, test.key) != 0)
    return false;
  p = strchr(st_test_skip, test.key[0]);
  if (p != 0 && (p[1] < '0' || p[1] > '9'))
    return false;
  return true;
}

static int
st_test(ST_Con& c, const ST_Test& test)
{
  chk1(st_end_trans(c, ST_AbortFlag) == 0);
  chk1(st_drop_test_tables(c) == 0);
  chk1(st_check_db_nodes(c) == 0);

  g_err << test << endl;
  if (c.numdbnodes < test.mindbnodes) {
    g_err << "skip, too few db nodes" << endl;
    return NDBT_OK;
  }

  chk1((*test.func)(c, test.arg) == NDBT_OK);
  chk1(st_check_db_nodes(c) == 0);
  //chk1(st_verify_list(c) == 0);

  return NDBT_OK;
err:
  return NDBT_FAILED;
}

static int st_random_seed = -1;

int
runSchemaTrans(NDBT_Context* ctx, NDBT_Step* step)
{
#ifdef NDB_USE_GET_ENV
  { const char* env = NdbEnv_GetEnv("NDB_TEST_DBUG", 0, 0);
    if (env != 0 && env[0] != 0) // e.g. d:t:L:F:o,ndb_test.log
      DBUG_PUSH(env);
  }
  { const char* env = NdbEnv_GetEnv("NDB_TEST_CORE", 0, 0);
    if (env != 0 && env[0] != 0 && env[0] != '0' && env[0] != 'N')
      st_core_on_err = true;
  }
  { const char* env = NdbEnv_GetEnv("NDB_TEST_CASE", 0, 0);
    st_test_case = env;
  }
  { const char* env = NdbEnv_GetEnv("NDB_TEST_SKIP", 0, 0);
    st_test_skip = env;
  }
  { const char* env = NdbEnv_GetEnv("NDB_TEST_SEED", 0, 0);
    if (env != 0)
      st_random_seed = atoi(env);
  }
#endif
  if (st_test_case != 0 && strcmp(st_test_case, "?") == 0) {
    int i;
    ndbout << "case func+arg desc" << endl;
    for (i = 0; i < st_test_count; i++) {
      const ST_Test& test = st_test_list[i];
      ndbout << test << endl;
    }
    return NDBT_WRONGARGS;
  }

  if (st_random_seed == -1)
    st_random_seed = (short)getpid();
  if (st_random_seed != 0) {
    g_err << "random seed: " << st_random_seed << endl;
    ndb_srand(st_random_seed);
  } else {
    g_err << "random seed: loop number" << endl;
  }

  Ndb_cluster_connection* ncc = &ctx->m_cluster_connection;
  Ndb* ndb = GETNDB(step);
  ST_Restarter* restarter = new ST_Restarter;
  ST_Con c(ncc, ndb, restarter);

  chk1(st_drop_all_tables(c) == 0);
  st_init_objects(c, ctx);

  int numloops;
  numloops = ctx->getNumLoops();

  for (c.loop = 0; numloops == 0 || c.loop < numloops; c.loop++) {
    g_err << "LOOP " << c.loop << endl;
    if (st_random_seed == 0)
      ndb_srand(c.loop);
    int i;
    for (i = 0; i < st_test_count; i++) {
      const ST_Test& test = st_test_list[i];
      if (st_test_match(test)) {
        chk1(st_test(c, test) == NDBT_OK);
      }
    }
  }

  st_report_db_nodes(c, g_err);
  return NDBT_OK;
err:
  st_report_db_nodes(c, g_err);
  return NDBT_FAILED;
}

// end schema trans

int 
runFailCreateHashmap(NDBT_Context* ctx, NDBT_Step* step)
{
  static int lst[] = { 6204, 6205, 6206, 6207, 6208, 6209, 6210, 6211, 0 };
  
  NdbRestarter restarter;
  int nodeId = restarter.getMasterNodeId();
  Ndb* pNdb = GETNDB(step);  
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  int errNo = 0;
#ifdef NDB_USE_GET_ENV
  char buf[100];
  if (NdbEnv_GetEnv("ERRNO", buf, sizeof(buf)))
  {
    errNo = atoi(buf);
    ndbout_c("Using errno: %u", errNo);
  }
#endif 
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;

  int dump1 = DumpStateOrd::SchemaResourceSnapshot;
  int dump2 = DumpStateOrd::SchemaResourceCheckLeak;

  NdbDictionary::HashMap hm;
  pDic->initDefaultHashMap(hm, 1);

loop:
  if (pDic->getHashMap(hm, hm.getName()) != -1)
  {
    pDic->initDefaultHashMap(hm, rand() % 64);
    goto loop;
  }

  for (int l = 0; l < loops; l++) 
  {
    for (unsigned i0 = 0; lst[i0]; i0++) 
    {
      unsigned j = (l == 0 ? i0 : myRandom48(i0 + l));
      int errval = lst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_info << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      
      int res = pDic->createHashMap(hm);
      CHECK2(res != 0, "create hashmap failed to fail");

      NdbDictionary::HashMap check;
      CHECK2(res != 0, "create hashmap existed");
      
      CHECK2(restarter.insertErrorInNode(nodeId, 0) == 0,
             "failed to clear error insert");
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
    }
  }
end:
  return result;
}
// end FAIL create hashmap

int
runCreateHashmaps(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  // int nodeId = restarter.getMasterNodeId();
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;

  NdbDictionary::HashMap hm;

  int created = 0;
  for (int i = 1; i <= NDB_DEFAULT_HASHMAP_BUCKETS && created < loops ; i++)
  {
    pDic->initDefaultHashMap(hm, i);
    int res = pDic->getHashMap(hm, hm.getName());
    if (res == -1)
    {
      const NdbError err = pDic->getNdbError();
      if (err.code != 723)
      {
        g_err << "getHashMap: " << hm.getName() << ": " << err << endl;
        result = NDBT_FAILED;
        break;
      }
      int res = pDic->createHashMap(hm);
      if (res == -1)
      {
        const NdbError err = pDic->getNdbError();
        if (err.code != 707 && err.code != 712)
        {
          g_err << "createHashMap: " << hm.getName() << ": " << err << endl;
          result = NDBT_FAILED;
        }
        break;
      }
      created++;
    }
  }

  // Drop all hashmaps (and everything else) with initial restart
  ndbout << "Restarting cluster" << endl;
  restarter.restartAll(/* initial */ true);
  restarter.waitClusterStarted();

  return result;
}
// end FAIL create hashmap

int
runFailAddPartition(NDBT_Context* ctx, NDBT_Step* step)
{
  static int lst[] = { 7211, 7212, 4050, 12008, 6212, 6124, 6213, 6214, 0 };

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbDictionary::Table tab(*ctx->getTab());
  NdbRestarter restarter;
  int nodeId = restarter.getMasterNodeId();

  int errNo = 0;
#ifdef NDB_USE_GET_ENV
  char buf[100];
  if (NdbEnv_GetEnv("ERRNO", buf, sizeof(buf)))
  {
    errNo = atoi(buf);
    ndbout_c("Using errno: %u", errNo);
  }
#endif
  // ordered index on first few columns
  NdbDictionary::Index idx("X");
  idx.setTable(tab.getName());
  idx.setType(NdbDictionary::Index::OrderedIndex);
  idx.setLogging(false);
  for (int cnt = 0, i_hate_broken_compilers = 0;
       cnt < 3 &&
       i_hate_broken_compilers < tab.getNoOfColumns();
       i_hate_broken_compilers++) {
    if (NdbSqlUtil::check_column_for_ordered_index
        (tab.getColumn(i_hate_broken_compilers)->getType(), 0) == 0 &&
        tab.getColumn(i_hate_broken_compilers)->getStorageType() !=
        NdbDictionary::Column::StorageTypeDisk)
    {
      idx.addColumn(*tab.getColumn(i_hate_broken_compilers));
      cnt++;
    }
  }

  for (int i = 0; i<tab.getNoOfColumns(); i++)
  {
    if (tab.getColumn(i)->getStorageType() ==
        NdbDictionary::Column::StorageTypeDisk)
    {
      NDBT_Tables::create_default_tablespace(pNdb);
      break;
    }
  }

  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  (void)pDic->dropTable(tab.getName());
  if (pDic->createTable(tab) != 0)
  {
    ndbout << "FAIL: " << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->createIndex(idx) != 0)
  {
    ndbout << "FAIL: " << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  const NdbDictionary::Table * org = pDic->getTable(tab.getName());
  NdbDictionary::Table altered = * org;
  altered.setFragmentCount(org->getFragmentCount() +
                           restarter.getNumDbNodes());

  if (pDic->beginSchemaTrans())
  {
    ndbout << "Failed to beginSchemaTrans()" << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->prepareHashMap(*org, altered) == -1)
  {
    ndbout << "Failed to create hashmap: " << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->endSchemaTrans())
  {
    ndbout << "Failed to endSchemaTrans()" << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }
  
  int dump1 = DumpStateOrd::SchemaResourceSnapshot;
  int dump2 = DumpStateOrd::SchemaResourceCheckLeak;

  for (int l = 0; l < loops; l++)
  {
    for (unsigned i0 = 0; lst[i0]; i0++)
    {
      unsigned j = (l == 0 ? i0 : myRandom48(sizeof(lst)/sizeof(lst[0]) - 1));
      int errval = lst[j];
      if (errNo != 0 && errNo != errval)
        continue;
      g_err << "insert error node=" << nodeId << " value=" << errval << endl;
      CHECK(restarter.dumpStateAllNodes(&dump1, 1) == 0);
      CHECK2(restarter.insertErrorInNode(nodeId, errval) == 0,
             "failed to set error insert");

      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived

      int res = pDic->alterTable(*org, altered);
      if (res)
      {
        ndbout << pDic->getNdbError() << endl;
      }
      CHECK2(res != 0,
             "failed to fail after error insert " << errval);
      CHECK2(restarter.insertErrorInNode(nodeId, 0) == 0,
             "failed to clear error insert");
      CHECK(restarter.dumpStateAllNodes(&dump2, 1) == 0);
      NdbSleep_MilliSleep(SAFTY); // Hope that snapshot has arrived

      int dump3[] = {DumpStateOrd::DihAddFragFailCleanedUp, org->getTableId()};
      CHECK(restarter.dumpStateAllNodes(dump3, 2) == 0);

      const NdbDictionary::Table* check = pDic->getTable(tab.getName());

      CHECK2((check->getObjectId() == org->getObjectId() &&
              check->getObjectVersion() == org->getObjectVersion()),
             "table has been altered!");
    }
  }

end:
  (void)pDic->dropTable(tab.getName());
  return result;
}
// fail add partition

int
runTableAddPartition(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  const int loops = ctx->getNumLoops();

  ndbout << "|- " << ctx->getTab()->getName() << endl;

  NdbDictionary::Table myTab= *(ctx->getTab());
  myTab.setFragmentType(NdbDictionary::Object::HashMapPartition);

  for (int l = 0; l < loops && result == NDBT_OK ; l++)
  {
    // Try to create table in db
    if (NDBT_Tables::createTable(pNdb, myTab.getName()) != 0){
      return NDBT_FAILED;
    }

    // Verify that table is in db
    const NdbDictionary::Table* pTab2 =
      NDBT_Table::discoverTableFromDb(pNdb, myTab.getName());
    if (pTab2 == NULL){
      ndbout << myTab.getName() << " was not found in DB"<< endl;
      return NDBT_FAILED;
    }
    ctx->setTab(pTab2);

#if 1
    // Load table
    const NdbDictionary::Table* pTab;
    CHECK((pTab = ctx->getTab()) != NULL);
    HugoTransactions beforeTrans(*pTab);
    if (beforeTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }
#endif

    // Add attributes to table.
    BaseString pTabName(pTab2->getName());
    const NdbDictionary::Table * oldTable = dict->getTable(pTabName.c_str());

    NdbDictionary::Table newTable= *oldTable;

    newTable.setFragmentCount(2 * oldTable->getFragmentCount());
    CHECK2(dict->alterTable(*oldTable, newTable) == 0,
           "TableAddAttrs failed");

    /* Need to purge old version and reload new version after alter table. */
    dict->invalidateTable(pTabName.c_str());

#if 0
    {
      HugoTransactions afterTrans(* dict->getTable(pTabName.c_str()));

      ndbout << "delete...";
      if (afterTrans.clearTable(pNdb) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "insert...";
      if (afterTrans.loadTable(pNdb, records) != 0){
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "update...";
      if (afterTrans.scanUpdateRecords(pNdb, records) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;

      ndbout << "delete...";
      if (afterTrans.clearTable(pNdb) != 0)
      {
        return NDBT_FAILED;
      }
      ndbout << endl;
    }
#endif
    abort();
    // Drop table.
    dict->dropTable(pTabName.c_str());
  }
end:

  return result;
}

int
runBug41905(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* pTab = ctx->getTab();
  BaseString tabName(pTab->getName());
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  NdbDictionary::Table creTab = *pTab;
  creTab.setForceVarPart(true);
  int ret = NDBT_OK;

  (void)pDic->dropTable(tabName.c_str());
  if (pDic->createTable(creTab)) {
    g_err << __LINE__ << ": " << pDic->getNdbError() << endl;
    ret = NDBT_FAILED;
  }

  Uint32 cols = creTab.getNoOfColumns();
  Uint32 vers = 0;
  while (ret == NDBT_OK) {
    const NdbDictionary::Table* pOldTab = pDic->getTableGlobal(tabName.c_str());
    require(pOldTab != 0);

    const Uint32 old_st = pOldTab->getObjectStatus();
    const Uint32 old_cols = pOldTab->getNoOfColumns();
    const Uint32 old_vers = pOldTab->getObjectVersion() >> 24;

    if (old_st != NdbDictionary::Object::Retrieved) {
      g_err << __LINE__ << ": " << "got status " << old_st << endl;
      ret = NDBT_FAILED;
      break;
    }
    // bug#41905 or related: other thread causes us to get old version
    if (old_cols != cols || old_vers != vers) {
      g_err << __LINE__ << ": "
            << "got cols,vers " << old_cols << "," << old_vers
            << " expected " << cols << "," << vers << endl;
      ret = NDBT_FAILED;
      break;
    }
    if (old_cols >= 100)
      break;
    const NdbDictionary::Table& oldTab = *pOldTab;

    NdbDictionary::Table newTab = oldTab;
    char colName[100];
    sprintf(colName, "COL41905_%02d", cols);
    g_info << "add " << colName << endl;
    NDBT_Attribute newCol(colName, NdbDictionary::Column::Unsigned, 1,
                          false, true, (CHARSET_INFO*)0,
                          NdbDictionary::Column::StorageTypeMemory, true);
    newTab.addColumn(newCol);

    ctx->setProperty("Bug41905", 1);
    NdbSleep_MilliSleep(10);

    const bool removeEarly = (uint)rand() % 2;
    g_info << "removeEarly = " << removeEarly << endl;

    if (pDic->beginSchemaTrans() != 0) {
      g_err << __LINE__ << ": " << pDic->getNdbError() << endl;
      ret = NDBT_FAILED;
      break;
    }
    if (pDic->alterTable(oldTab, newTab) != 0) {
      g_err << __LINE__ << ": " << pDic->getNdbError() << endl;
      ret = NDBT_FAILED;
      break;
    }

    if (removeEarly)
      pDic->removeTableGlobal(*pOldTab, 0);

    if (pDic->endSchemaTrans() != 0) {
      g_err << __LINE__ << ": " << pDic->getNdbError() << endl;
      ret = NDBT_FAILED;
      break;
    }

    cols++;
    vers++;
    if (!removeEarly)
      pDic->removeTableGlobal(*pOldTab, 0);
    ctx->setProperty("Bug41905", 2);
    NdbSleep_MilliSleep(10);
  }

  ctx->setProperty("Bug41905", 3);
  return ret;
}

int
runBug41905getTable(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* pTab = ctx->getTab();
  BaseString tabName(pTab->getName());
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  while (1) {
    while (1) {
      if (ctx->getProperty("Bug41905") == 1)
        break;
      if (ctx->getProperty("Bug41905") == 3)
        goto out;
      NdbSleep_MilliSleep(10);
    }

    uint ms = (uint)rand() % 1000;
    NdbSleep_MilliSleep(ms);
    g_info << "get begin ms=" << ms << endl;

    Uint32 count = 0;
    Uint32 oldstatus = 0;
    while (1) {
      count++;
      const NdbDictionary::Table* pTmp = pDic->getTableGlobal(tabName.c_str());
      require(pTmp != 0);
      Uint32 code = pDic->getNdbError().code;
      Uint32 status = pTmp->getObjectStatus();
      if (oldstatus == 2 && status == 3)
        g_info << "code=" << code << " status=" << status << endl;
      oldstatus = status;
      pDic->removeTableGlobal(*pTmp, 0);
      if (ctx->getProperty("Bug41905") != 1)
        break;
      NdbSleep_MilliSleep(10);
    }
    g_info << "get end count=" << count << endl;
  }

out:
  (void)pDic->dropTable(tabName.c_str());
  return NDBT_OK;
}

static
int
createIndexes(NdbDictionary::Dictionary* pDic,
              const NdbDictionary::Table & tab, int cnt)
{
  for (int i = 0; i<cnt && i < tab.getNoOfColumns(); i++)
  {
    char buf[256];
    NdbDictionary::Index idx0;
    BaseString::snprintf(buf, sizeof(buf), "%s-idx-%u", tab.getName(), i);
    idx0.setName(buf);
    idx0.setType(NdbDictionary::Index::OrderedIndex);
    idx0.setTable(tab.getName());
    idx0.setStoredIndex(false);
    idx0.addIndexColumn(tab.getColumn(i)->getName());

    if (pDic->createIndex(idx0))
    {
      ndbout << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }

  return 0;
}

int
runBug46552(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();

  NdbRestarter res;
  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  NdbDictionary::Table tab0 = *pTab;
  NdbDictionary::Table tab1 = *pTab;

  BaseString name;
  name.assfmt("%s_0", tab0.getName());
  tab0.setName(name.c_str());
  name.assfmt("%s_1", tab1.getName());
  tab1.setName(name.c_str());

  pDic->dropTable(tab0.getName());
  pDic->dropTable(tab1.getName());

  if (pDic->createTable(tab0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->createTable(tab1))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (createIndexes(pDic, tab1, 4))
    return NDBT_FAILED;

  Vector<int> group1;
  Vector<int> group2;
  Bitmask<256/32> nodeGroupMap;
  for (int j = 0; j<res.getNumDbNodes(); j++)
  {
    int node = res.getDbNodeId(j);
    int ng = res.getNodeGroup(node);
    if (nodeGroupMap.get(ng))
    {
      group2.push_back(node);
    }
    else
    {
      group1.push_back(node);
      nodeGroupMap.set(ng);
    }
  }

  res.restartNodes(group1.getBase(), (int)group1.size(),
                   NdbRestarter::NRRF_NOSTART |
                   NdbRestarter::NRRF_ABORT);

  res.waitNodesNoStart(group1.getBase(), (int)group1.size());
  res.startNodes(group1.getBase(), (int)group1.size());
  res.waitClusterStarted();

  res.restartNodes(group2.getBase(), (int)group2.size(),
                   NdbRestarter::NRRF_NOSTART |
                   NdbRestarter::NRRF_ABORT);
  res.waitNodesNoStart(group2.getBase(), (int)group2.size());
  res.startNodes(group2.getBase(), (int)group2.size());
  res.waitClusterStarted();

  if (pDic->dropTable(tab0.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->createTable(tab0))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (createIndexes(pDic, tab0, 4))
    return NDBT_FAILED;

  res.restartAll2(NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
  res.waitClusterNoStart();
  res.startAll();
  res.waitClusterStarted();

  if (pDic->dropTable(tab0.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->dropTable(tab1.getName()))
  {
    ndbout << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runBug46585(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbDictionary::Table tab(*ctx->getTab());
  NdbRestarter res;
  int records = ctx->getNumRecords();

  // ordered index on first few columns
  NdbDictionary::Index idx("X");
  idx.setTable(tab.getName());
  idx.setType(NdbDictionary::Index::OrderedIndex);
  idx.setLogging(false);
  for (int cnt = 0, i_hate_broken_compilers = 0;
       cnt < 3 &&
       i_hate_broken_compilers < tab.getNoOfColumns();
       i_hate_broken_compilers++) {
    if (NdbSqlUtil::check_column_for_ordered_index
        (tab.getColumn(i_hate_broken_compilers)->getType(), 0) == 0 &&
        tab.getColumn(i_hate_broken_compilers)->getStorageType() !=
        NdbDictionary::Column::StorageTypeDisk)
    {
      idx.addColumn(*tab.getColumn(i_hate_broken_compilers));
      cnt++;
    }
  }

  for (int i = 0; i<tab.getNoOfColumns(); i++)
  {
    if (tab.getColumn(i)->getStorageType() ==
        NdbDictionary::Column::StorageTypeDisk)
    {
      NDBT_Tables::create_default_tablespace(pNdb);
      break;
    }
  }

  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  (void)pDic->dropTable(tab.getName());
  if (pDic->createTable(tab) != 0)
  {
    ndbout << "FAIL: " << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if (pDic->createIndex(idx) != 0)
  {
    ndbout << "FAIL: " << pDic->getNdbError() << endl;
    return NDBT_FAILED;
  }

  for (int i = 0; i<loops; i++)
  {
    const NdbDictionary::Table * org = pDic->getTable(tab.getName());
    {
      CHECK(org != NULL);
      HugoTransactions trans(* org);
      CHECK2(trans.loadTable(pNdb, records) == 0,
           "load table failed");
    }

    NdbDictionary::Table altered = * org;
    altered.setFragmentCount(org->getFragmentCount() + 1);
    ndbout_c("alter from %u to %u partitions",
             org->getFragmentCount(),
             altered.getFragmentCount());

    if (pDic->beginSchemaTrans())
    {
      ndbout << "Failed to beginSchemaTrans()" << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }

    if (pDic->prepareHashMap(*org, altered) == -1)
    {
      ndbout << "Failed to create hashmap: " << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }

    if (pDic->endSchemaTrans())
    {
      ndbout << "Failed to endSchemaTrans()" << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }

    result = pDic->alterTable(*org, altered);
    if (result)
    {
      ndbout << pDic->getNdbError() << endl;
    }
    if (pDic->getNdbError().code == 1224)
    {
      /**
       * To many fragments is an acceptable error
       *   depending on configuration used for test-case
       */
      result = NDBT_OK;
      goto end;
    }
    CHECK2(result == 0,
           "failed to alter");

    pDic->invalidateTable(tab.getName());
    {
      const NdbDictionary::Table * alteredP = pDic->getTable(tab.getName());
      CHECK2(alteredP->getFragmentCount() == altered.getFragmentCount(),
             "altered table does not have correct frag count");

      HugoTransactions trans(* alteredP);

      CHECK2(trans.scanUpdateRecords(pNdb, records) == 0,
             "scan update failed");
      trans.startTransaction(pNdb);
      trans.pkUpdateRecord(pNdb, 0);
      trans.execute_Commit(pNdb);
      ndbout_c("before restart, gci: %d", trans.getRecordGci(0));
      trans.closeTransaction(pNdb);
    }

    switch(i % 2){
    case 0:
      if (res.getNumDbNodes() > 1)
      {
        int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
        ndbout_c("performing node-restart of node %d", nodeId);
        CHECK2(res.restartOneDbNode(nodeId,
                                    false,
                                    true,
                                    true) == 0,
               "restart one node failed");
        CHECK2(res.waitNodesNoStart(&nodeId, 1) == 0,
               "wait node started failed");
        CHECK2(res.startNodes(&nodeId, 1) == 0,
               "start node failed");
        break;
      }
    case 1:
    {
      ndbout_c("performing system restart");
      CHECK2(res.restartAll(false, true, false) == 0,
             "restart all failed");
      CHECK2(res.waitClusterNoStart() == 0,
             "waitClusterNoStart failed");
      CHECK2(res.startAll() == 0,
             "startAll failed");
      break;
    }
    }
    CHECK2(res.waitClusterStarted() == 0,
           "wait cluster started failed");

    Uint32 restartGCI = 0;
    CHECK2(pDic->getRestartGCI(&restartGCI) == 0,
           "getRestartGCI failed");
    ndbout_c("restartGCI: %u", restartGCI);

    pDic->invalidateTable(tab.getName());
    {
      const NdbDictionary::Table * alteredP = pDic->getTable(tab.getName());
      CHECK(alteredP != NULL);
      HugoTransactions trans(* alteredP);

      int cnt;
      CHECK2(trans.selectCount(pNdb, 0, &cnt) == 0,
             "select count failed");

      CHECK2(cnt == records,
             "table does not have correct record count: "
             << cnt << " != " << records);

      CHECK2(alteredP->getFragmentCount() == altered.getFragmentCount(),
             "altered table does not have correct frag count");

      CHECK2(trans.scanUpdateRecords(pNdb, records) == 0,
             "scan update failed");
      CHECK2(trans.pkUpdateRecords(pNdb, records) == 0,
             "pkUpdateRecords failed");
      CHECK2(trans.clearTable(pNdb) == 0,
             "clear table failed");
    }
  }

end:
  (void)pDic->dropTable(tab.getName());
  return result;
}

int
runBug53944(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbDictionary::Table tab(*ctx->getTab());
  NdbRestarter res;

  Vector<int> ids;
  for (unsigned i = 0; i< 25; i++)
  {
    NdbDictionary::Table copy = tab;
    BaseString name;
    name.appfmt("%s_%u", copy.getName(), i);
    copy.setName(name.c_str());
    int res = pDic->createTable(copy);
    if (res)
    {
      g_err << "Failed to create table" << copy.getName() << "\n"
            << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    const NdbDictionary::Table* tab = pDic->getTable(copy.getName());
    if (tab == 0)
    {
      g_err << "Failed to retreive table" << copy.getName() << endl;
      return NDBT_FAILED;
      
    }
    ids.push_back(tab->getObjectId());
  }

  res.restartAll2(NdbRestarter::NRRF_ABORT | NdbRestarter::NRRF_NOSTART);
  res.waitClusterNoStart();
  res.startAll();
  res.waitClusterStarted();

  for (unsigned i = 0; i< 25; i++)
  {
    NdbDictionary::Table copy = tab;
    BaseString name;
    name.appfmt("%s_%u", copy.getName(), i);
    copy.setName(name.c_str());
    const NdbDictionary::Table* tab = pDic->getTable(copy.getName());
    if (tab == 0)
    {
      g_err << "Failed to retreive table" << copy.getName() << endl;
      return NDBT_FAILED;
      
    }
    int res = pDic->dropTable(copy.getName());
    if (res)
    {
      g_err << "Failed to drop table" << copy.getName() << "\n"
            << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  
  Vector<int> ids2;
  for (unsigned i = 0; i< 25; i++)
  {
    NdbDictionary::Table copy = tab;
    BaseString name;
    name.appfmt("%s_%u", copy.getName(), i);
    copy.setName(name.c_str());
    int res = pDic->createTable(copy);
    if (res)
    {
      g_err << "Failed to create table" << copy.getName() << "\n"
            << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
    const NdbDictionary::Table* tab = pDic->getTable(copy.getName());
    if (tab == 0)
    {
      g_err << "Failed to retreive table" << copy.getName() << endl;
      return NDBT_FAILED;
      
    }
    ids2.push_back(tab->getObjectId());
  }

  for (unsigned i = 0; i< 25; i++)
  {
    NdbDictionary::Table copy = tab;
    BaseString name;
    name.appfmt("%s_%u", copy.getName(), i);
    copy.setName(name.c_str());
    const NdbDictionary::Table* tab = pDic->getTable(copy.getName());
    if (tab == 0)
    {
      g_err << "Failed to retreive table" << copy.getName() << endl;
      return NDBT_FAILED;
      
    }
    int res = pDic->dropTable(copy.getName());
    if (res)
    {
      g_err << "Failed to drop table" << copy.getName() << "\n"
            << pDic->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }

  /**
   * With Bug53944 - none of the table-id have been reused in this scenario
   *   check that atleast 15 of the 25 have been to return OK
   */
  unsigned reused = 0;
  for (unsigned i = 0; i<ids.size(); i++)
  {
    int id = ids[i];
    for (unsigned j = 0; j<ids2.size(); j++)
    {
      if (ids2[j] == id)
      {
        reused++;
        break;
      }
    }
  }

  ndbout_c("reused %u table-ids out of %u", 
           (unsigned)reused, (unsigned)ids.size());

  if (reused >= (ids.size() >> 2))
  {
    return NDBT_OK;
  }
  else
  {
    return NDBT_FAILED;
  }
}

// Bug58277 + Bug57057

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    result = NDBT_FAILED; \
    break; \
  }

// allow list of expected error codes which do not cause NDBT_FAILED
#define CHK3(b, e, x) \
  if (!(b)) { \
    int n = sizeof(x)/sizeof(x[0]); \
    int i; \
    for (i = 0; i < n; i++) { \
      int s = (x[i] >= 0 ? +1 : -1); \
      if (e.code == s * x[i]) { \
        if (s == +1) \
          g_info << "OK: " << #b << " failed at line " << __LINE__ \
                << ": " << e << endl; \
        break; \
      } \
    } \
    if (i == n) { \
      g_err << "ERR: " << #b << " failed at line " << __LINE__ \
            << ": " << e << endl; \
      result = NDBT_FAILED; \
    } \
    break; \
  }

const char* tabName_Bug58277 = "TBug58277";
const char* indName_Bug58277 = "TBug58277X1";

static void
sync_main_step(NDBT_Context* ctx, NDBT_Step* step, const char* state)
{
  // total sub-steps
  Uint32 sub_steps = ctx->getProperty("SubSteps", (Uint32)0);
  require(sub_steps != 0);
  // count has been reset before
  require(ctx->getProperty("SubCount", (Uint32)0) == 0);
  // set the state
  g_info << "step main: set " << state << endl;
  require(ctx->getProperty(state, (Uint32)0) == 0);
  ctx->setProperty(state, (Uint32)1);
  // wait for sub-steps
  ctx->getPropertyWait("SubCount", sub_steps);
  if (ctx->isTestStopped())
    return;
  g_info << "step main: sub-steps got " << state << endl;
  // reset count and state
  ctx->setProperty("SubCount", (Uint32)0);
  ctx->setProperty(state, (Uint32)0);
}

static void
sync_sub_step(NDBT_Context* ctx, NDBT_Step* step, const char* state)
{
  // wait for main step to set state
  g_info << "step " << step->getStepNo() << ": wait for " << state << endl;
  ctx->getPropertyWait(state, (Uint32)1);
  if (ctx->isTestStopped())
    return;
  // add to sub-step counter
  ctx->incProperty("SubCount");
  g_info << "step " << step->getStepNo() << ": got " << state << endl;
  // continue to run until next sync
}

static int
runBug58277createtable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;

  do
  {
    CHK2(rows > 0, "cannot use --records=0"); // others require this
    g_info << "create table " << tabname << endl;
    NdbDictionary::Table tab(tabname);
    const char* name[] = { "a", "b" };
    for (int i = 0; i <= 1; i++)
    {
      NdbDictionary::Column c(name[i]);
      c.setType(NdbDictionary::Column::Unsigned);
      c.setPrimaryKey(i == 0);
      c.setNullable(false);
      tab.addColumn(c);
    }
    if (rand() % 3 != 0)
    {
      g_info << "set FragAllLarge" << endl;
      tab.setFragmentType(NdbDictionary::Object::FragAllLarge);
    }
    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
  }
  while (0);
  return result;
}

static int
runBug58277loadtable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;

  do
  {
    g_info << "load table" << endl;
    const NdbDictionary::Table* pTab = 0;
    CHK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());

    int cnt = 0;
    for (int i = 0; i < rows; i++)
    {
      int retries = 10;
  retry:
      NdbTransaction* pTx = 0;
      CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());

      NdbOperation* pOp = 0;
      CHK2((pOp = pTx->getNdbOperation(pTab)) != 0, pTx->getNdbError());
      CHK2(pOp->insertTuple() == 0, pOp->getNdbError());
      Uint32 aVal = i;
      Uint32 bVal = rand() % rows;
      CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
      CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());

      do
      {
        int x[] = {
         -630
        };
        int res = pTx->execute(Commit);
        if (res != 0 &&
            pTx->getNdbError().status == NdbError::TemporaryError)
        {
          retries--;
          if (retries >= 0)
          {
            pTx->close();
            NdbSleep_MilliSleep(10);
            goto retry;
          }
        }
        CHK3(res == 0, pTx->getNdbError(), x);
        cnt++;
      }
      while (0);
      CHK2(result == NDBT_OK, "load failed");
      pNdb->closeTransaction(pTx);
    }
    CHK2(result == NDBT_OK, "load failed");
    g_info << "load " << cnt << " rows" << endl;
  }
  while (0);
  return result;
}

static int
runBug58277createindex(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;

  do
  {
    g_info << "create index " << indname << endl;
    NdbDictionary::Index ind(indname);
    ind.setTable(tabname);
    ind.setType(NdbDictionary::Index::OrderedIndex);
    ind.setLogging(false);
    ind.addColumn("b");
    CHK2(pDic->createIndex(ind) == 0, pDic->getNdbError());

    const NdbDictionary::Index* pInd = 0;
    CHK2((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError());
  }
  while (0);
  return result;
}

// separate error handling test
int
runBug58277errtest(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  //const int rows = ctx->getNumRecords();
  NdbRestarter restarter;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  (void)pDic->dropTable(tabname);

  const int errloops = loops < 5 ? loops : 5;
  int errloop = 0;
  while (!ctx->isTestStopped() && errloop < errloops)
  {
    g_info << "===== errloop " << errloop << " =====" << endl;

    if (errloop == 0)
    {
      CHK2(runBug58277createtable(ctx, step) == NDBT_OK, "create table failed");
      CHK2(runBug58277loadtable(ctx, step) == NDBT_OK, "load table failed");
      CHK2(runBug58277createindex(ctx, step) == NDBT_OK, "create index failed");
    }
    const NdbDictionary::Index* pInd = 0;
    CHK2((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError());

    int errins[] = {
      12008, 909,  // TuxNoFreeScanOp
      12009, 4259  // InvalidBounds
    };
    const int errcnt = (int)(sizeof(errins)/sizeof(errins[0]));
    for (int i = 0; i < errcnt; i += 2)
    {
      const int ei = errins[i + 0];
      const int ec = errins[i + 1];
      CHK2(restarter.insertErrorInAllNodes(ei) == 0, "value " << ei);

      NdbTransaction* pSTx = 0;
      CHK2((pSTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
      NdbIndexScanOperation* pSOp = 0;
      CHK2((pSOp = pSTx->getNdbIndexScanOperation(pInd)) != 0, pSTx->getNdbError());

      NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
      Uint32 flags = 0;
      CHK2(pSOp->readTuples(lm, flags) == 0, pSOp->getNdbError());

      Uint32 aVal = 0;
      CHK2(pSOp->getValue("a", (char*)&aVal) != 0, pSOp->getNdbError());
      CHK2(pSTx->execute(NoCommit) == 0, pSTx->getNdbError());
      // before fixes 12009 failed to fail at once here
      CHK2(pSOp->nextResult(true) == -1, "failed to fail on " << ei);
      CHK2(pSOp->getNdbError().code == ec, "expect " << ec << " got " << pSOp->getNdbError());
      pNdb->closeTransaction(pSTx);

      g_info << "error " << ei << " " << ec << " ok" << endl;
      CHK2(restarter.insertErrorInAllNodes(0) == 0, "value " << 0);
    }
    CHK2(result == NDBT_OK, "test error handling failed");

    errloop++;
    if (errloop == errloops)
    {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_info << "table " << tabname << " dropped" << endl;
    }
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

int
runBug58277drop(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  int dropms = 0;

  while (!ctx->isTestStopped())
  {
    sync_sub_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;
    dropms = ctx->getProperty("DropMs", (Uint32)0);
    NdbSleep_MilliSleep(dropms);

    g_info << "drop index " << indname << endl;
    CHK2(pDic->dropIndex(indname, tabname) == 0, pDic->getNdbError());
    pDic->invalidateIndex(indname, tabname);
    CHK2(pDic->getIndex(indname, tabname) == 0, "failed");
    g_info << "drop index done" << endl;

    sync_sub_step(ctx, step, "Stop");
    if (ctx->isTestStopped())
      break;
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

static int
runBug58277scanop(NDBT_Context* ctx, NDBT_Step* step, int cnt[1+3])
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  const int range_max = ctx->getProperty("RANGE_MAX", (Uint32)0);
  require(range_max > 0);
  const bool scan_delete = ctx->getProperty("SCAN_DELETE", (Uint32)0);

  do
  {
    const NdbDictionary::Index* pInd = 0;
    {
      int x[] = {
        4243  // Index not found
      };
      pDic->invalidateIndex(indname, tabname);
      CHK3((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError(), x);
    }

    NdbTransaction* pSTx = 0;
    CHK2((pSTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
    NdbIndexScanOperation* pSOp = 0;
    CHK2((pSOp = pSTx->getNdbIndexScanOperation(pInd)) != 0, pSTx->getNdbError());
    NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
    Uint32 flags = 0;
    int range_cnt = rand() % range_max;
    if (range_cnt > 1 || rand() % 5 == 0)
      flags |= NdbIndexScanOperation::SF_MultiRange;
    CHK2(pSOp->readTuples(lm, flags) == 0, pSOp->getNdbError());
    g_info << "range cnt " << range_cnt << endl;
    for (int i = 0; i < range_cnt; )
    {
      int tlo = -1;
      int thi = -1;
      if (rand() % 5 == 0)
      {
        if (rand() % 5 != 0)
          tlo = 0 + rand() % 2;
        if (rand() % 5 != 0)
          thi = 2 + rand() % 2;
      }
      else
        tlo = 4;
      // apparently no bounds is not allowed (see also bug#57396)
      if (tlo == -1 && thi == -1)
        continue;
      Uint32 blo = 0;
      Uint32 bhi = 0;
      if (tlo != -1)
      {
        blo = rand() % rows;
        CHK2(pSOp->setBound("b", tlo, &blo) == 0, pSOp->getNdbError());
      }
      if (thi != -1)
      {
        bhi = rand() % (rows + 1);
        if (bhi < blo)
          bhi = rand() % (rows + 1);
        CHK2(pSOp->setBound("b", thi, &bhi) == 0, pSOp->getNdbError());
      }
      CHK2(pSOp->end_of_bound() == 0, pSOp->getNdbError());
      i++;
    }
    CHK2(result == NDBT_OK, "set bound ranges failed");

    Uint32 aVal = 0;
    CHK2(pSOp->getValue("a", (char*)&aVal) != 0, pSOp->getNdbError());
    CHK2(pSTx->execute(NoCommit) == 0, pSTx->getNdbError());

    while (1)
    {
      int ret;
      {
        int x[] = {
          241,  // Invalid schema object version
          274,  // Time-out in NDB, probably caused by deadlock
          283,  // Table is being dropped
          284,  // Table not defined in transaction coordinator
          910,  // Index is being dropped
          1226  // Table is being dropped
        };
        CHK3((ret = pSOp->nextResult(true)) != -1, pSOp->getNdbError(), x);
      }
      require(ret == 0 || ret == 1);
      if (ret == 1)
        break;

      NdbTransaction* pTx = 0;
      CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());

      while (1)
      {
        int type = 1 + rand() % 3;
        if (type == 2) // insert->update
          type = 1;
        if (scan_delete)
          type = 3;
        do
        {
          if (type == 1)
          {
            NdbOperation* pOp = 0;
            CHK2((pOp = pSOp->updateCurrentTuple(pTx)) != 0, pSOp->getNdbError());
            Uint32 bVal = (Uint32)(rand() % rows);
            CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());
            break;
          }
          if (type == 3)
          {
            CHK2(pSOp->deleteCurrentTuple(pTx) == 0, pSOp->getNdbError());
            break;
          }
          require(false);
        }
        while (0);
        CHK2(result == NDBT_OK, "scan takeover error");
        cnt[type]++;
        {
          int x[] = {
            266,  // Time-out in NDB, probably caused by deadlock
            499,  // Scan take over error
            631,  // 631
            4350  // Transaction already aborted
          };
          CHK3(pTx->execute(NoCommit) == 0, pTx->getNdbError(), x);
        }

        CHK2((ret = pSOp->nextResult(false)) != -1, pSOp->getNdbError());
        require(ret == 0 || ret == 2);
        if (ret == 2)
          break;
      }
      CHK2(result == NDBT_OK, "batch failed");

      {
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
          4350  // Transaction already aborted
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
      }
      pNdb->closeTransaction(pTx);
    }
    CHK2(result == NDBT_OK, "batch failed");
    pNdb->closeTransaction(pSTx);
  }
  while (0);
  return result;
}

int
runBug58277scan(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  while (!ctx->isTestStopped())
  {
    sync_sub_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;
    g_info << "start scan loop" << endl;
    while (!ctx->isTestStopped())
    {
      g_info << "start scan" << endl;
      int cnt[1+3] = { 0, 0, 0, 0 };
      CHK2(runBug58277scanop(ctx, step, cnt) == NDBT_OK, "scan failed");
      g_info << "scan ops " << cnt[1] << "/-/" << cnt[3] << endl;

      if (ctx->getProperty("Stop", (Uint32)0) == 1)
      {
        sync_sub_step(ctx, step, "Stop");
        break;
      }
    }
    CHK2(result == NDBT_OK, "scan loop failed");
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

static int
runBug58277pkop(NDBT_Context* ctx, NDBT_Step* step, int cnt[1+3])
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int rows = ctx->getNumRecords();
  const char* tabname = tabName_Bug58277;

  do
  {
    const NdbDictionary::Table* pTab = 0;
    CHK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());

    NdbTransaction* pTx = 0;
    CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
    NdbOperation* pOp = 0;
    CHK2((pOp = pTx->getNdbOperation(pTab)) != 0, pTx->getNdbError());
    int type = 1 + rand() % 3;
    Uint32 aVal = rand() % rows;
    Uint32 bVal = rand() % rows;

    do
    {
      if (type == 1)
      {
        CHK2(pOp->updateTuple() == 0, pOp->getNdbError());
        CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
        CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
         -626   // Tuple did not exist
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        break;
      }
      if (type == 2)
      {
        CHK2(pOp->insertTuple() == 0, pOp->getNdbError());
        CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
        CHK2(pOp->setValue("b", bVal) == 0, pOp->getNdbError());
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
         -630   // Tuple already existed when attempting to insert
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        break;
      }
      if (type == 3)
      {
        CHK2(pOp->deleteTuple() == 0, pOp->getNdbError());
        CHK2(pOp->equal("a", (char*)&aVal) == 0, pOp->getNdbError());
        int x[] = {
          266,  // Time-out in NDB, probably caused by deadlock
         -626   // Tuple did not exist
        };
        CHK3(pTx->execute(Commit) == 0, pTx->getNdbError(), x);
        break;
      }
      require(false);
    }
    while (0);
    CHK2(result == NDBT_OK, "pk op failed");

    pNdb->closeTransaction(pTx);
    cnt[type]++;
  }
  while (0);
  return result;
}

int
runBug58277pk(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  while (!ctx->isTestStopped())
  {
    sync_sub_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;

    g_info << "start pk loop" << endl;
    int cnt[1+3] = { 0, 0, 0, 0 };
    while (!ctx->isTestStopped())
    {
      CHK2(runBug58277pkop(ctx, step, cnt) == NDBT_OK, "pk op failed");

      if (ctx->getProperty("Stop", (Uint32)0) == 1)
      {
        sync_sub_step(ctx, step, "Stop");
        break;
      }
    }
    CHK2(result == NDBT_OK, "pk loop failed");
    g_info << "pk ops " << cnt[1] << "/" << cnt[2] << "/" << cnt[3] << endl;
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  return result;
}

int
runBug58277rand(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbRestarter restarter;

  while (!ctx->isTestStopped())
  {
    int sleepms = rand() % 5000;
    g_info << "rand sleep " << sleepms << " ms" << endl;
    NdbSleep_MilliSleep(sleepms);
    if (rand() % 5 == 0)
    {
      g_info << "rand force LCP" << endl;
      int dump1[] = { DumpStateOrd::DihStartLcpImmediately };
      CHK2(restarter.dumpStateAllNodes(dump1, 1) == 0, "failed");
    }
  }
  if (result != NDBT_OK)
  {
    g_info << "stop test at line " << __LINE__ << endl;
    ctx->stopTest();
  }
  g_info << "rand exit" << endl;
  return result;
}

int
runBug58277(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  const bool rss_check = ctx->getProperty("RSS_CHECK", (Uint32)0);
  NdbRestarter restarter;
  const char* tabname = tabName_Bug58277;
  const char* indname = indName_Bug58277;
  (void)pDic->dropTable(tabname);

  int loop = 0;
  while (!ctx->isTestStopped())
  {
    g_info << "===== loop " << loop << " =====" << endl;

    if (loop == 0)
    {
      CHK2(runBug58277createtable(ctx, step) == NDBT_OK, "create table failed");
      CHK2(runBug58277loadtable(ctx, step) == NDBT_OK, "load table failed");
    }

    if (rss_check)
    {
      g_info << "save all resource usage" << endl;
      int dump1[] = { DumpStateOrd::SchemaResourceSnapshot };
      CHK2(restarter.dumpStateAllNodes(dump1, 1) == 0, "failed");
    }

    CHK2(runBug58277createindex(ctx, step) == NDBT_OK, "create index failed");

    int dropmin = 1000;
    int dropmax = 9000;
    int dropms = dropmin + rand() % (dropmax - dropmin + 1);
    g_info << "drop in " << dropms << " ms" << endl;
    ctx->setProperty("DropMs", dropms);

    sync_main_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;

    // vary Stop time a bit in either direction
    int stopvar = rand() % 100;
    int stopsgn = (rand() % 2 == 0 ? +1 : -1);
    int stopms = dropms + stopsgn * stopvar;
    NdbSleep_MilliSleep(stopms);

    sync_main_step(ctx, step, "Stop");
    if (ctx->isTestStopped())
      break;

    // index must have been dropped
    pDic->invalidateIndex(indname, tabname);
    CHK2(pDic->getIndex(indname, tabname) == 0, "failed");

    if (rss_check)
    {
      g_info << "check all resource usage" << endl;
      int dump2[] = { DumpStateOrd::SchemaResourceCheckLeak };
      CHK2(restarter.dumpStateAllNodes(dump2, 1) == 0, "failed");

      g_info << "check cluster is up" << endl;
      CHK2(restarter.waitClusterStarted() == 0, "failed");
    }

    if (++loop == loops)
    {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_info << "table " << tabname << " dropped" << endl;
      break;
    }
  }

  g_info << "stop test at line " << __LINE__ << endl;
  ctx->stopTest();
  return result;
}

int
runBug57057(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  const bool rss_check = ctx->getProperty("RSS_CHECK", (Uint32)0);
  NdbRestarter restarter;
  const char* tabname = tabName_Bug58277;
  //const char* indname = indName_Bug58277;
  (void)pDic->dropTable(tabname);

  int loop = 0;
  while (!ctx->isTestStopped())
  {
    g_info << "===== loop " << loop << " =====" << endl;

    if (loop == 0)
    {
      CHK2(runBug58277createtable(ctx, step) == NDBT_OK, "create table failed");
      CHK2(runBug58277createindex(ctx, step) == NDBT_OK, "create index failed");
    }

    CHK2(runBug58277loadtable(ctx, step) == NDBT_OK, "load table failed");

    if (rss_check)
    {
      g_info << "save all resource usage" << endl;
      int dump1[] = { DumpStateOrd::SchemaResourceSnapshot };
      CHK2(restarter.dumpStateAllNodes(dump1, 1) == 0, "failed");
    }

    int dropmin = 1000;
    int dropmax = 2000;
    int dropms = dropmin + rand() % (dropmax - dropmin + 1);
    int stopms = dropms;

    sync_main_step(ctx, step, "Start");
    if (ctx->isTestStopped())
      break;

    g_info << "stop in " << stopms << " ms" << endl;
    NdbSleep_MilliSleep(stopms);

    sync_main_step(ctx, step, "Stop");
    if (ctx->isTestStopped())
      break;

    if (rss_check)
    {
      g_info << "check all resource usage" << endl;
      int dump2[] = { DumpStateOrd::SchemaResourceCheckLeak };
      CHK2(restarter.dumpStateAllNodes(dump2, 1) == 0, "failed");

      g_info << "check cluster is up" << endl;
      CHK2(restarter.waitClusterStarted() == 0, "failed");
    }

    if (++loop == loops)
    {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_info << "table " << tabname << " dropped" << endl;
      break;
    }
  }

  g_info << "stop test at line " << __LINE__ << endl;
  ctx->stopTest();
  return result;
}

/**
 * This is a regression test for Bug #14647210 "CAN CRASH ALL NODES EASILY 
 * WHEN RESTARTING MORE THAN 6 NODES SIMULTANEOUSLY". The cause of this bug
 * was that DICT did not handle GET_TABINFOREF signals.
 */
static int
runGetTabInfoRef(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() == 1)
  {
    g_info << "Cannot do this test with just one datanode." << endl;
    return NDBT_OK;
  }

  /**
   * This error insert makes DICT respond with GET_TABINFOREF where
   * error==busy when receiving the next GET_TABINFOREQ signal.
   */
  restarter.insertErrorInAllNodes(6026);

  /* Find a node in each nodegroup to restart. */
  Vector<int> nodeSet;
  Bitmask<MAX_NDB_NODES/32> nodeGroupMap;
  for (int i = 0; i < restarter.getNumDbNodes(); i++)
  {
    const int node = restarter.getDbNodeId(i);
    const int ng = restarter.getNodeGroup(node);
    if (!nodeGroupMap.get(ng))
    {
      g_info << "Node " << node << " will be stopped." << endl;
      nodeSet.push_back(node);
      nodeGroupMap.set(ng);
    }
  }

  if (restarter.restartNodes(nodeSet.getBase(), (int)nodeSet.size(),
                             NdbRestarter::NRRF_NOSTART |
                             NdbRestarter::NRRF_ABORT))
  {
    g_err << "Failed to stop nodes" << endl;
    restarter.insertErrorInAllNodes(0);
    return NDBT_FAILED;
  }

  g_info << "Waiting for nodes to stop." << endl;
  if (restarter.waitNodesNoStart(nodeSet.getBase(), (int)nodeSet.size()))
  {
    g_err << "Failed to wait for nodes to stop" << endl;
    restarter.insertErrorInAllNodes(0);
    return NDBT_FAILED;
  }

  if (restarter.startNodes(nodeSet.getBase(), (int)nodeSet.size()))
  {
    g_err << "Failed to restart nodes" << endl;
    restarter.insertErrorInAllNodes(0);
    return NDBT_FAILED;
  }

  g_info << "Waiting for nodes to start again." << endl;
  if (restarter.waitClusterStarted() != 0)
  {
    g_err << "Failed to restart cluster " << endl;
    restarter.insertErrorInAllNodes(0);
    return NDBT_FAILED;
  }

  restarter.insertErrorInAllNodes(0);
  return NDBT_OK;
} // runGetTabInfoRef()

int
runBug13416603(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbIndexStat is;
  NdbRestarter res;

  int elist[] = { 18026, 0 };
  const NdbDictionary::Table *pTab = pDic->getTable(ctx->getTab()->getName());
  const NdbDictionary::Index *pIdx = 0;
  NdbDictionary::Dictionary::List indexes;
  pDic->listIndexes(indexes, * pTab);
  for (unsigned i = 0; i < indexes.count; i++)
  {
    if ((pIdx = pDic->getIndex(indexes.elements[i].name, pTab->getName())) != 0)
      break;
  }

  if (pIdx == 0)
  {
    return NDBT_OK;
  }

  bool has_created_stat_tables = false;
  bool has_created_stat_events = false;
  pNdb->setDatabaseName("mysql");
  if (is.create_systables(pNdb) == 0)
  {
    has_created_stat_tables = true;
  }

  if (is.create_sysevents(pNdb) == 0)
  {
    has_created_stat_events = true;
  }

  chk2(is.create_listener(pNdb) == 0, is.getNdbError());
  chk2(is.execute_listener(pNdb) == 0, is.getNdbError());

  is.set_index(* pIdx, * pTab);

  {
    ndbout_c("%u - update_stat", __LINE__);
    chk2(is.update_stat(pNdb) == 0, is.getNdbError());
    int ret;
    ndbout_c("%u - poll_listener", __LINE__);
    chk2((ret = is.poll_listener(pNdb, 10000)) != -1, is.getNdbError());
    chk1(ret == 1);
    // one event is expected
    ndbout_c("%u - next_listener", __LINE__);
    chk2((ret = is.next_listener(pNdb)) != -1, is.getNdbError());
    chk1(ret == 1);
    ndbout_c("%u - next_listener", __LINE__);
    chk2((ret = is.next_listener(pNdb)) != -1, is.getNdbError());
    chk1(ret == 0);
  }

  {
    Vector<Vector<int> > partitions = res.splitNodes();
    if (partitions.size() == 1)
      goto cleanup;

    for (unsigned i = 0; i < partitions.size(); i++)
    {
      printf("stopping: ");
      for (unsigned j = 0; j < partitions[i].size(); j++)
        printf("%d ", partitions[i][j]);
      printf("\n");

      res.restartNodes(partitions[i].getBase(),
                       partitions[i].size(),
                       NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
      res.waitNodesNoStart(partitions[i].getBase(),
                           partitions[i].size());

      {
        ndbout_c("%u - update_stat", __LINE__);
        chk2(is.update_stat(pNdb) == 0, is.getNdbError());
        int ret;
        ndbout_c("%u - poll_listener", __LINE__);
        chk2((ret = is.poll_listener(pNdb, 10000)) != -1, is.getNdbError());
        chk1(ret == 1);
        // one event is expected
        ndbout_c("%u - next_listener", __LINE__);
        chk2((ret = is.next_listener(pNdb)) != -1, is.getNdbError());
        chk1(ret == 1);
        ndbout_c("%u - next_listener", __LINE__);
        chk2((ret = is.next_listener(pNdb)) != -1, is.getNdbError());
        chk1(ret == 0);
      }

      res.startNodes(partitions[i].getBase(),
                     partitions[i].size());
      res.waitClusterStarted();
    }
  }

  for (int i = 0; elist[i] != 0; i++)
  {
    ndbout_c("testing errno: %u", elist[i]);
    res.insertErrorInAllNodes(elist[i]);
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    res.dumpStateAllNodes(val2, 2);

    {
      ndbout_c("%u - update_stat", __LINE__);
      int ret = is.update_stat(pNdb);
      ndbout_c("%u - update_stat => %d", __LINE__, ret);
      chk1(ret == -1);
      ndbout << is.getNdbError() << endl;
      ndbout_c("%u - poll_listener", __LINE__);
      chk2((ret = is.poll_listener(pNdb, 10000)) != -1, is.getNdbError());
      if (ret == 1)
      {
        /* After the new api is introduced, pollEvents() (old api version)
         * returns 1 when empty epoch is at the head of the event queue.
         * pollEvents2() (new api version) returns 1 when exceptional
         * epoch is at the head of the event queue.
         * So next_listener() must be called to handle them.
         */
        chk2((ret = is.next_listener(pNdb)) != -1, is.getNdbError());
      }
      // Check that the event queue is empty
      chk1(ret == 0);
    }

    /**
     * Wait for one of the nodes to have died...
     */
    int count_started = 0;
    int count_not_started = 0;
    int count_nok = 0;
    int down = 0;
    do
    {
      NdbSleep_MilliSleep(100);
      count_started = count_not_started = count_nok = 0;
      for (int i = 0; i < res.getNumDbNodes(); i++)
      {
        int n = res.getDbNodeId(i);
        if (res.getNodeStatus(n) == NDB_MGM_NODE_STATUS_NOT_STARTED)
        {
          count_not_started++;
          down = n;
        }
        else if (res.getNodeStatus(n) == NDB_MGM_NODE_STATUS_STARTED)
          count_started++;
        else
          count_nok ++;
      }
    } while (count_not_started != 1);

    res.startNodes(&down, 1);
    res.waitClusterStarted();
    res.insertErrorInAllNodes(0);
  }

cleanup:
  // cleanup
  is.drop_listener(pNdb);
  if (has_created_stat_events)
  {
    is.drop_sysevents(pNdb);
  }
  if (has_created_stat_tables)
  {
    is.drop_systables(pNdb);
  }

  // Ensure that nodes will start after error inserts again.
  {
    const int restartState[] = 
      { DumpStateOrd::CmvmiSetRestartOnErrorInsert, NRT_DoStart_Restart };
    
    require(res.dumpStateAllNodes(restartState,
                                  sizeof restartState/sizeof restartState[0])
            == 0);
  }

  return NDBT_OK;

err:
  return NDBT_FAILED;
}

int
runIndexStatCreate(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbIndexStat is;

  const int loops = ctx->getNumLoops();

  pNdb->setDatabaseName("mysql");

  Uint64 end = NdbTick_CurrentMillisecond() + 1000 * loops;
  do
  {
    if (is.create_systables(pNdb) == 0)
    {
      /**
       * OK
       */
    }
    else if (! (is.getNdbError().code == 701  || // timeout
                is.getNdbError().code == 721  || // already exists
                is.getNdbError().code == 4244 || // already exists
                is.getNdbError().code == 4009))  // no connection
    {
      ndbout << is.getNdbError() << endl;
      return NDBT_FAILED;
    }

    is.drop_systables(pNdb);
  } while (!ctx->isTestStopped() && NdbTick_CurrentMillisecond() < end);

  return NDBT_OK;
}

int
runWL946(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  bool keep_table = false; // keep table and data
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  {
    const char* p = NdbEnv_GetEnv("KEEP_TABLE_WL946", (char*)0, 0);
    if (p != 0 && strchr("1Y", p[0]) != 0)
      keep_table = true;
  }
#endif
#endif
  int result = NDBT_OK;

  const char* tabname = "T_WL946";
  (void)pDic->dropTable(tabname);

  for (int loop = 0; loop < loops; loop++)
  {
    g_info << "loop " << loop << "(" << loops << ")" << endl;

    NdbDictionary::Table tab;
    tab.setName(tabname);

    struct Coldef {
      const char* name;
      NdbDictionary::Column::Type type;
      int prec; // fractional precision 0-6
      int flag; // 1-pk 2-nullable 4-fractional 8-create index
      const char* indname;
    } coldef[] = {
      // primary key
      { "pk", NdbDictionary::Column::Unsigned, 0, 1, 0 },
      // deprecated
      { "a0", NdbDictionary::Column::Time, 0, 2|8, "x0" },
      { "a1", NdbDictionary::Column::Datetime, 0, 2|8, "x1" },
      { "a2", NdbDictionary::Column::Timestamp, 0, 2|8, "x2" },
      // fractional
      { "b0", NdbDictionary::Column::Time2, 0, 2|4|8, "y0" },
      { "b1", NdbDictionary::Column::Datetime2, 0, 2|4|8, "y1" },
      { "b2", NdbDictionary::Column::Timestamp2, 0, 2|4|8, "y2" },
      // update key
      { "uk", NdbDictionary::Column::Unsigned, 0, 0, 0 }
    };
    const int Colcnt = sizeof(coldef)/sizeof(coldef[0]);

    NdbDictionary::Column col[Colcnt];
    for (int i = 0; i < Colcnt; i++)
    {
      Coldef& d = coldef[i];
      NdbDictionary::Column& c = col[i];
      c.setName(d.name);
      c.setType(d.type);
      if (d.flag & 4)
      {
        d.prec = myRandom48(7);
        require(d.prec >= 0 && d.prec <= 6);
        c.setPrecision(d.prec);
      }
      c.setPrimaryKey(d.flag & 1);
      c.setNullable(d.flag & 2);
      tab.addColumn(c);
    }

    g_info << "create table " << tabname << endl;
    const NdbDictionary::Table* pTab = 0;
    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
    CHK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());

    const NdbDictionary::Column* pCol[Colcnt];
    for (int i = 0; i < Colcnt; i++)
    {
      const Coldef& d = coldef[i];
      const NdbDictionary::Column* pc = 0;
      CHK2((pc = tab.getColumn(i)) != 0, pDic->getNdbError());
      CHK2(strcmp(pc->getName(), d.name) == 0, "name");
      CHK2(pc->getType() == d.type, "type");
      CHK2(pc->getPrecision() == d.prec, "prec");
      pCol[i] = pc;
    }
    CHK2(result == NDBT_OK, "verify columns");

    g_info << "create indexes" << endl;
    NdbDictionary::Index ind[Colcnt];
    const NdbDictionary::Index* pInd[Colcnt];
    for (int i = 0; i < Colcnt; i++)
    {
      Coldef& d = coldef[i];
      pInd[i] = 0;
      if (d.flag & 8)
      {
        NdbDictionary::Index& x = ind[i];
        x.setName(d.indname);
        x.setTable(tabname);
        x.setType(NdbDictionary::Index::OrderedIndex);
        x.setLogging(false);
        x.addColumn(d.name);
        const NdbDictionary::Index* px = 0;
        CHK2(pDic->createIndex(x) == 0, pDic->getNdbError());
        CHK2((px = pDic->getIndex(d.indname, tabname)) != 0, pDic->getNdbError());
        pInd[i] = px;
      }
    }
    CHK2(result == NDBT_OK, "create indexes");

    HugoTransactions trans(*pTab);

    g_info << "load records" << endl;
    CHK2(trans.loadTable(pNdb, records) == 0, trans.getNdbError());

    const int scanloops = 5;
    for (int j = 0; j < scanloops; j++)
    {
      g_info << "scan table " << j << "(" << scanloops << ")" << endl;
      CHK2(trans.scanReadRecords(pNdb, records) == 0, trans.getNdbError());

      for (int i = 0; i < Colcnt; i++)
      {
        Coldef& d = coldef[i];
        if (d.flag & 8)
        {
          g_info << "scan index " << d.indname << endl;
          const NdbDictionary::Index* px = pInd[i];
          CHK2(trans.scanReadRecords(pNdb, px, records) == 0, trans.getNdbError());
        }
      }
      CHK2(result == NDBT_OK, "index scan");

      g_info << "update records" << endl;
      CHK2(trans.scanUpdateRecords(pNdb, records) == 0, trans.getNdbError());
    }
    CHK2(result == NDBT_OK, "scans");

    if (loop + 1 < loops || !keep_table)
    {
      g_info << "delete records" << endl;
      CHK2(trans.clearTable(pNdb) == 0, trans.getNdbError());

      g_info << "drop table" << endl;
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
    }
  }

  if (result != NDBT_OK && !keep_table)
  {
    g_info << "drop table after error" << endl;
    (void)pDic->dropTable(tabname);
  }
  return result;
}

int
getOrCreateDefaultHashMap(NdbDictionary::Dictionary& dict, NdbDictionary::HashMap& hm, Uint32 buckets, Uint32 fragments)
{
  if (dict.getDefaultHashMap(hm, buckets, fragments) == 0)
  {
    return 0;
  }

  dict.initDefaultHashMap(hm, buckets, fragments);
  if (dict.createHashMap(hm, NULL) == -1)
  {
    return -1;
  }

  if (dict.getDefaultHashMap(hm, buckets, fragments) == 0)
  {
    return 0;
  }

  return -1;
}

struct Bug14645319_createTable_args
{
  char const* template_name;
  char const* name;
  Uint32 buckets;
  Uint32 fragments;
};

int Bug14645319_createTable(Ndb* pNdb, NdbDictionary::Table& tab, int when,
                                    void* arg)
{
  Bug14645319_createTable_args& args = *static_cast<Bug14645319_createTable_args*>(arg);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  if (when == 0)
  {
    tab.setName(args.name);
    tab.setFragmentCount(args.fragments);
    if (args.fragments == 0)
    {
      tab.setFragmentData(0, 0);
    }
    NdbDictionary::HashMap hm;
    getOrCreateDefaultHashMap(*pDic, hm, args.buckets, args.fragments);
    tab.setHashMap(hm);
  }
  return 0;
}

int
runBug14645319(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int failures = 0;

  struct test_case {
    char const* description;
    int old_fragments;
    int old_buckets;
    int new_fragments;
    int new_buckets;
    int expected_buckets;
  };

  STATIC_ASSERT(NDB_DEFAULT_HASHMAP_BUCKETS % 240 == 0);
  STATIC_ASSERT(NDB_DEFAULT_HASHMAP_BUCKETS % 260 != 0);
  test_case test_cases[] = {
    { "Simulate online reorg, may or may not change hashmap depending on default fragment count",
      3, 120, 0, NDB_DEFAULT_HASHMAP_BUCKETS, 0 },
    { "Keep old hashmap since no new fragments",
      3, 120, 3, NDB_DEFAULT_HASHMAP_BUCKETS, 120 },
    { "Keep old hashmap size since old size a multiple of new fragment count",
      3, 120, 6, NDB_DEFAULT_HASHMAP_BUCKETS, 120 },
    { "Keep old hashmap size since new size not a multiple of old",
      3, 130, 6, NDB_DEFAULT_HASHMAP_BUCKETS, 130 },
    { "Extend hashmap",
      3, 120, 7, NDB_DEFAULT_HASHMAP_BUCKETS, NDB_DEFAULT_HASHMAP_BUCKETS },
    { "Keep old hashmap size since old size not multiple of old fragment count",
      5, 84, 7, 42, 84 },
    { "Shrink hashmap",
      3, 120, 6, 60, 60 },
  };

  Bug14645319_createTable_args args;
  args.template_name = ctx->getTab()->getName();
  args.name = "Bug14645319";

  for (size_t testi = 0; testi < NDB_ARRAY_SIZE(test_cases); testi++)
  {
    test_case const& test = test_cases[testi];
    int result = NDBT_FAILED;

    int old_fragments = 0;
    int old_buckets = 0;
    int new_fragments = 0;
    int new_buckets = 0;

    do {
      /* setup old table */
      args.buckets = test.old_buckets;
      args.fragments = test.old_fragments;
      result = NDBT_Tables::createTable(pNdb, args.template_name, false, false, Bug14645319_createTable, &args);
      if (result != 0) break;

      NdbDictionary::Table const& old_tab = *pDic->getTable(args.name);

      /* check old table properties */
      NdbDictionary::HashMap old_hm;
      result = pDic->getHashMap(old_hm, &old_tab);
      if (result != 0) break;

      old_fragments = old_tab.getFragmentCount();
      old_buckets = old_hm.getMapLen();
      if (old_fragments != test.old_fragments)
      {
        result = NDBT_FAILED;
        break;
      }
      if (old_buckets != test.old_buckets)
      {
        result = NDBT_FAILED;
        break;
      }

      /* alter table */
      NdbDictionary::Table new_tab = old_tab;
      new_tab.setFragmentCount(test.new_fragments);
      if (test.new_fragments == 0)
        new_tab.setFragmentData(0, 0);

      result = pDic->beginSchemaTrans();
      if (result != 0) break;

      result = pDic->prepareHashMap(old_tab, new_tab, test.new_buckets);

      result |= pDic->endSchemaTrans();
      if (result != 0) break;

      result = pDic->alterTable(old_tab, new_tab);
      if (result != 0) break;

      /* check */
      NdbDictionary::HashMap new_hm;
      result = pDic->getHashMap(new_hm, &new_tab);
      if (result != 0) break;

      new_fragments = new_tab.getFragmentCount();
      new_buckets = new_hm.getMapLen();

      if (test.expected_buckets > 0 &&
          new_buckets != test.expected_buckets)
      {
        result = NDBT_FAILED;
        break;
      }
      result = 0;
    } while (false);

    result |= pDic->dropTable(args.name);

    if (result == 0)
    {
      ndbout << "Test#" << (testi + 1) << " '" << test_cases[testi].description << "' passed" <<
        " (" << old_buckets << " => " << test_cases[testi].new_buckets << " => " << test_cases[testi].expected_buckets << ")" << endl;
    }
    else
    {
      ndbout << "Test#" << (testi + 1) << " '" << test_cases[testi].description << "' failed" <<
        " (" << old_buckets << " => " << test_cases[testi].new_buckets << " => " << new_buckets << " expected: " << test_cases[testi].expected_buckets << ")" << endl;
      failures++;
    }
  }

  return failures > 0 ? NDBT_FAILED : NDBT_OK;
}

// FK SR/NR

#define CHK1(b) CHK2(b, "-");

// myRandom48 seems too non-random
#define myRandom48(x) (unsigned(ndb_rand()) % (x))
#define myRandom48Init(x) (ndb_srand(x))

// used for create and verify
struct Fkdef {
  static const int tabmax = 5;
  static const int colmax = 5;
  static const int indmax = 5;
  static const int keymax = tabmax * 5;
  static const int strmax = 10;
  struct Ob {
    bool retrieved;
    int id;
    int version;
  };
  struct Col {
    char colname[strmax];
    bool pk;
    bool nullable; // false
    int icol; // pos in table columns
  };
  struct Ind : Ob {
    char indname[strmax];
    Col col[colmax];
    int ncol;
    bool pk;
    bool unique;
    const NdbDictionary::Index* pInd;
  };
  struct Tab : Ob {
    char tabname[strmax];
    Col col[colmax];
    int ncol;
    Ind ind[indmax]; // first "index" is primary key
    int nind;
    const NdbDictionary::Table* pTab;
  };
  struct Key : Ob {
    char keyname[strmax];
    char fullname[20 + strmax]; // bug#19122346
    // 0-parent 1-child
    const Tab* tab0;
    const Tab* tab1;
    const Ind* ind0;
    const Ind* ind1;
    NdbDictionary::ForeignKey::FkAction updateAction;
    NdbDictionary::ForeignKey::FkAction deleteAction;
  };
  struct List {
    NdbDictionary::Dictionary::List* list;
    int keystart; // FK stuff sorted to end of list starts here
    List() { list = 0; }
    ~List() { delete list; }
  };
  Tab tab[tabmax];
  int ntab;
  Key key[keymax];
  int nkey;
  List list;
  bool nokeys;
  bool nodrop;
  int testcase;
};

static int
fk_compare_icol(const void* p1, const void* p2)
{
  const Fkdef::Col& col1 = *(const Fkdef::Col*)p1;
  const Fkdef::Col& col2 = *(const Fkdef::Col*)p2;
  return col1.icol - col2.icol;
}

static int
fk_type(int t)
{
  if (
    t ==  NdbDictionary::Object::ForeignKey ||
    t ==  NdbDictionary::Object::FKParentTrigger ||
    t ==  NdbDictionary::Object::FKChildTrigger
  )
    return 1;
  return 0;
}

static int
fk_compare_element(const void* p1, const void* p2)
{
  const NdbDictionary::Dictionary::List::Element& e1 =
    *(const NdbDictionary::Dictionary::List::Element*)p1;
  const NdbDictionary::Dictionary::List::Element& e2 =
    *(const NdbDictionary::Dictionary::List::Element*)p2;
  int k = 0;
  if ((k = fk_type(e1.type) - fk_type(e2.type)) != 0)
    return k;
  if ((k = e1.type - e2.type) != 0)
    return k;
  if ((k = (int)e1.id - (int)e2.id) != 0)
    return k;
  return 0;
}

static bool
fk_find_element(const Fkdef::List& list, int type,
                const char* database, const char* name)
{
  int found = 0;
  for (int i = 0; i < (int)list.list->count; i++)
  {
    const NdbDictionary::Dictionary::List::Element& e =
      list.list->elements[i];
    if (e.type == type &&
        strcmp(e.database, database) == 0 &&
        strcmp(e.name, name) == 0)
    {
      found++;
    }
  }
  require(found == 0 || found == 1);
  return found;
}

// testcase 1: t0 (a0 pk, b0 key), t1 (a1 pk, b1 key), fk b1->a0

static void
fk_define_tables1(Fkdef& d)
{
  d.ntab = 2;
  for (int i = 0; i < d.ntab; i++)
  {
    Fkdef::Tab& dt = d.tab[i];
    sprintf(dt.tabname, "t%d", i);
    dt.ncol = 2;
    for (int j = 0; j < dt.ncol; j++)
    {
      Fkdef::Col& dc = dt.col[j];
      sprintf(dc.colname, "%c%d", 'a' + j, i);
      dc.pk = (j == 0);
      dc.nullable = false;
      dc.icol = j;
    }
    dt.nind = 2;
    dt.pTab = 0;
    dt.retrieved = false;
    {
      Fkdef::Ind& di = dt.ind[0];
      sprintf(di.indname, "%s", "pk");
      di.ncol = 1;
      di.col[0] = dt.col[0];
      di.pk = true;
      di.unique = true;
      di.pInd = 0;
      di.retrieved = false;
    }
    {
      Fkdef::Ind& di = dt.ind[1];
      sprintf(di.indname, "t%dx%d", i, 1);
      di.ncol = 1;
      di.col[0] = dt.col[1];
      di.pk = false;
      di.unique = false;
      di.pInd = 0;
      di.retrieved = false;
    }
  }
  g_info << "defined " << d.ntab << " tables" << endl;
}

static void
fk_define_keys1(Fkdef& d)
{
  d.nkey = 1;
  Fkdef::Key& dk = d.key[0];
  sprintf(dk.keyname, "fk%d", 0);
  dk.tab0 = &d.tab[0];
  dk.tab1 = &d.tab[1];
  dk.ind0 = &dk.tab0->ind[0];
  dk.ind1 = &dk.tab1->ind[1];
  dk.updateAction = NdbDictionary::ForeignKey::NoAction;
  dk.deleteAction = NdbDictionary::ForeignKey::NoAction;
  dk.retrieved = false;
  g_info << "defined " << d.nkey << " keys" << endl;
}

// testcase 2: random

static void
fk_define_tables2(Fkdef& d)
{
  d.ntab = 1 + myRandom48(d.tabmax);
  for (int i = 0; i < d.ntab; i++)
  {
    Fkdef::Tab& dt = d.tab[i];
    sprintf(dt.tabname, "t%d", i);
    dt.ncol = 2 + myRandom48(d.colmax - 1);
    for (int j = 0; j < dt.ncol; j++)
    {
      Fkdef::Col& dc = dt.col[j];
      sprintf(dc.colname, "%c%d", 'a' + j, i);
      dc.pk = (j == 0 || myRandom48(d.colmax) == 0);
      dc.nullable = false;
      dc.icol = j;
    }
    dt.nind = 1 + myRandom48(d.indmax);
    dt.pTab = 0;
    dt.retrieved = false;
    for (int k = 0; k < dt.nind; k++)
    {
      Fkdef::Ind& di = dt.ind[k];
      if (k == 0)
      {
        sprintf(di.indname, "%s", "pk");
        di.ncol = 0;
        for (int j = 0; j < dt.ncol; j++)
        {
          Fkdef::Col& dc = dt.col[j];
          if (dc.pk)
            di.col[di.ncol++] = dc;
        }
        di.pk = true;
        di.unique = true;
      }
      else
      {
        di.unique = (myRandom48(3) != 0);
        sprintf(di.indname, "t%dx%d", i, k);
        di.ncol = 1 + myRandom48(dt.ncol);
        uint mask = 0;
        int n = 0;
        while (n < di.ncol)
        {
          int j = myRandom48(dt.ncol);
          Fkdef::Col& dc = dt.col[j];
          if ((mask & (1 << j)) == 0)
          {
            di.col[n++] = dc;
            mask |= (1 << j);
          }
        }
        if (di.unique)
          qsort(&di.col, di.ncol, sizeof(di.col[0]), fk_compare_icol);
      }
      di.pInd = 0;
      di.retrieved = false;
    }
  }
  g_info << "defined " << d.ntab << " tables" << endl;
}

static void
fk_define_keys2(Fkdef& d)
{
  int nkey = 1 + myRandom48(d.ntab * 5);
  int k = 0;
  int ntrymax = nkey * 100;
  int ntry = 0;
  while (k < nkey && ntry++ < ntrymax)
  {
    Fkdef::Key& dk = d.key[k];
    new (&dk) Fkdef::Key;
    int i0 = myRandom48(d.ntab);
    int i1 = myRandom48(d.ntab);
    Fkdef::Tab& dt0 = d.tab[i0];
    Fkdef::Tab& dt1 = d.tab[i1];
    int k0 = myRandom48(dt0.nind);
    int k1 = myRandom48(dt1.nind);
    Fkdef::Ind& di0 = dt0.ind[k0];
    Fkdef::Ind& di1 = dt1.ind[k1];
    if (!di0.unique || di0.ncol != di1.ncol)
      continue;
    if (i0 == i1 && k0 == k1)
      if (myRandom48(10) != 0) // allowed but try to avoid
        continue;
    sprintf(dk.keyname, "fk%d", k);
    dk.tab0 = &dt0;
    dk.tab1 = &dt1;
    dk.ind0 = &di0;
    dk.ind1 = &di1;
    dk.updateAction = NdbDictionary::ForeignKey::NoAction;
    dk.deleteAction = NdbDictionary::ForeignKey::NoAction;
    dk.retrieved = false;
    k++;
  }
  d.nkey = k;
  g_info << "defined " << d.nkey << " keys tries:" << ntry << endl;
}

static void
fk_define_tables(Fkdef& d)
{
  if (d.testcase == 1)
    fk_define_tables1(d);
  else if (d.testcase == 2)
    fk_define_tables2(d);
  else
    require(false);
}

static void
fk_define_keys(Fkdef& d)
{
  if (d.nokeys)
  {
    d.nkey = 0;
    return;
  }
  if (d.testcase == 1)
    fk_define_keys1(d);
  else if (d.testcase == 2)
    fk_define_keys2(d);
  else
    require(false);
}

static void
fk_undefine_keys(Fkdef& d)
{
  d.nkey = 0;
}

static void
fk_define_all(Fkdef& d)
{
  fk_define_tables(d);
  fk_define_keys(d);
}

static int
fk_create_table(Fkdef& d, Ndb* pNdb, int i)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    Fkdef::Tab& dt = d.tab[i];
    NdbDictionary::Table tab;
    tab.setName(dt.tabname);
    for (int j = 0; j < dt.ncol; j++)
    {
      Fkdef::Col& dc = dt.col[j];
      NdbDictionary::Column col;
      col.setName(dc.colname);
      col.setType(NdbDictionary::Column::Unsigned);
      col.setPrimaryKey(dc.pk);
      col.setNullable(dc.nullable);
      tab.addColumn(col);
    }
    g_info << "create table " << dt.tabname << endl;
    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
    const NdbDictionary::Table* pTab = 0;
    CHK2((pTab = pDic->getTable(dt.tabname)) != 0, pDic->getNdbError());
    require(!dt.retrieved);
    dt.retrieved = true;
    dt.id = pTab->getObjectId();
    dt.version = pTab->getObjectVersion();
    dt.pTab = pTab;
    for (int k = 1; k < dt.nind; k++) // skip pk
    {
      Fkdef::Ind& di = dt.ind[k];
      NdbDictionary::Index ind;
      ind.setName(di.indname);
      ind.setTable(dt.tabname);
      if (di.unique)
      {
        ind.setType(NdbDictionary::Index::UniqueHashIndex);
        ind.setLogging(true);
      }
      else
      {
        ind.setType(NdbDictionary::Index::OrderedIndex);
        ind.setLogging(false);
      }
      for (int j = 0; j < di.ncol; j++)
      {
        const Fkdef::Col& dc = di.col[j];
        ind.addColumn(dc.colname);
      }
      g_info << "create index " << di.indname << endl;
      CHK2(pDic->createIndex(ind) == 0, pDic->getNdbError());
      const NdbDictionary::Index* pInd = 0;
      CHK2((pInd = pDic->getIndex(di.indname, dt.tabname)) != 0, pDic->getNdbError());
      require(!di.retrieved);
      di.retrieved = true;
      di.id = pInd->getObjectId();
      di.version = pInd->getObjectVersion();
      di.pInd = pInd;
    }
  }
  while (0);
  return result;
}

static int
fk_create_tables(Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  for (int i = 0; i < d.ntab; i++)
  {
    CHK1(fk_create_table(d, pNdb, i) == NDBT_OK);
  }
  return result;
}

static int
fk_create_key(Fkdef& d, Ndb* pNdb, int k)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    Fkdef::Key& dk = d.key[k];
    NdbDictionary::ForeignKey key;
    key.setName(dk.keyname);
    const Fkdef::Tab& dt0 = *dk.tab0;
    const Fkdef::Tab& dt1 = *dk.tab1;
    const Fkdef::Ind& di0 = *dk.ind0;
    const Fkdef::Ind& di1 = *dk.ind1;
    const NdbDictionary::Table* pTab0 = dt0.pTab;
    const NdbDictionary::Table* pTab1 = dt1.pTab;
    const NdbDictionary::Index* pInd0 = di0.pInd;
    const NdbDictionary::Index* pInd1 = di1.pInd;
    key.setParent(*pTab0, pInd0);
    key.setChild(*pTab1, pInd1);
    g_info << "create key " << dk.keyname << endl;
    CHK2(pDic->createForeignKey(key) == 0, pDic->getNdbError());
    {
      NdbDictionary::ForeignKey key;
      sprintf(dk.fullname, "%d/%d/%s", dt0.id, dt1.id, dk.keyname);
      CHK2(pDic->getForeignKey(key, dk.fullname) == 0, pDic->getNdbError());
      require(!dk.retrieved);
      dk.retrieved = true;
      dk.id = key.getObjectId();
      dk.version = key.getObjectVersion();
    }
  }
  while (0);
  return result;
}

static int
fk_create_keys(Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  for (int k = 0; k < d.nkey; k++)
  {
    CHK1(fk_create_key(d, pNdb, k) == NDBT_OK);
  }
  return result;
}

static int
fk_alter_table(Fkdef& d, Ndb* pNdb, int i)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    Fkdef::Tab& dt = d.tab[i];
    const NdbDictionary::Table* pTab1 = 0;
    CHK2((pTab1 = pDic->getTable(dt.tabname)) != 0, pDic->getNdbError());
    g_info << "alter table " << dt.tabname << endl;
    int id1 = pTab1->getObjectId();
    int version1 = pTab1->getObjectVersion();
    g_info << "old: id=" << id1 << " version=" << hex << version1 << endl;
    CHK2(pDic->alterTable(*pTab1, *pTab1) == 0, pDic->getNdbError());
    pDic->invalidateTable(dt.tabname);
    const NdbDictionary::Table* pTab2 = 0;
    CHK2((pTab2 = pDic->getTable(dt.tabname)) != 0, pDic->getNdbError());
    int id2 = pTab2->getObjectId();
    int version2 = pTab2->getObjectVersion();
    g_info << "old: id=" << id2 << " version=" << hex << version2 << endl;
    CHK2(id1 == id2, id1 << " != " << id2);
    CHK2(version1 != version2, version1 << " == " << version2);
    dt.id = id2;
    dt.version = version2;
  }
  while (0);
  return result;
}

static int
fk_alter_tables(Fkdef& d, Ndb* pNdb, bool atrandom)
{
  int result = NDBT_OK;
  for (int i = 0; i < d.ntab; i++)
  {
    if (!atrandom || myRandom48(2) == 0)
    {
      CHK1(fk_alter_table(d, pNdb, i) == NDBT_OK);
    }
  }
  return result;
}

static int
fk_create_all(Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  do
  {
    CHK1(fk_create_tables(d, pNdb) == 0);
    CHK1(fk_create_keys(d, pNdb) == NDBT_OK);
    // imitate mysqld by doing an alter table afterwards
    CHK1(fk_alter_tables(d, pNdb, true) == NDBT_OK);
  }
  while (0);
  return result;
}

static int
fk_verify_table(const Fkdef& d, Ndb* pNdb, int i)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    const Fkdef::Tab& dt = d.tab[i];
    g_info << "verify table " << dt.tabname << endl;
    const NdbDictionary::Table* pTab = 0;
    CHK2((pTab = pDic->getTable(dt.tabname)) != 0, pDic->getNdbError());
    int id = pTab->getObjectId();
    int version = pTab->getObjectVersion();
    require(dt.retrieved);
    CHK2(dt.id == id, dt.id << " != " << id);
    CHK2(dt.version == version, dt.version << " != " << version);
    for (int k = 1; k < dt.nind; k++) // skip pk
    {
      const Fkdef::Ind& di = dt.ind[k];
      g_info << "verify index " << di.indname << endl;
      const NdbDictionary::Index* pInd = 0;
      CHK2((pInd = pDic->getIndex(di.indname, dt.tabname)) != 0, pDic->getNdbError());
      int id = pInd->getObjectId();
      int version = pInd->getObjectVersion();
      require(di.retrieved);
      CHK2(di.id == id, di.id << " != " << id);
      CHK2(di.version == version, di.version << " != " << version);
    }
    CHK1(result == NDBT_OK);
  }
  while (0);
  return result;
}

static int
fk_verify_tables(const Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  for (int i = 0; i < d.ntab; i++)
  {
    CHK1(fk_verify_table(d, pNdb, i) == 0);
  }
  return result;
}

static int
fk_verify_key(const Fkdef& d, Ndb* pNdb, int k)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    const Fkdef::Key& dk = d.key[k];
    g_info << "verify key " << dk.fullname << endl;
    NdbDictionary::ForeignKey key;
    CHK2(pDic->getForeignKey(key, dk.fullname) == 0, pDic->getNdbError());
    int id = key.getObjectId();
    int version = key.getObjectVersion();
    require(dk.retrieved);
    CHK2(dk.id == id, dk.id << " != " << id);
    CHK2(dk.version == version, dk.version << " != " << version);
    CHK2(strcmp(dk.fullname, key.getName()) == 0, dk.fullname << " != " << key.getName());
#if 0 // can add more checks
    const Fkdef::Tab& dt0 = *dk.tab0;
    const Fkdef::Tab& dt1 = *dk.tab1;
    const Fkdef::Ind& di0 = *dk.ind0;
    const Fkdef::Ind& di1 = *dk.ind1;
#endif
  }
  while (0);
  return result;
}

static int
fk_verify_keys(const Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  for (int k = 0; k < d.nkey; k++)
  {
    CHK1(fk_verify_key(d, pNdb, k) == 0);
  }
  return result;
}

static int
fk_verify_ddl(const Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  do
  {
    g_info << "verify ddl" << endl;
    CHK1(fk_verify_tables(d, pNdb) == 0);
    CHK1(fk_verify_keys(d, pNdb) == 0);
  }
  while (0);
  return result;
}

static int
fk_verify_dml(const Fkdef& d, Ndb* pNdb, int records)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  const int batch = 1;
  const bool allowCV = false;
  const int errNoParent = 255;
  const int errHasChild = 256;
  do
  {
    if (!(d.testcase == 1 && records > 0))
      break;
    g_info << "verify dml" << endl;
    const Fkdef::Tab& dt0 = d.tab[0];
    const Fkdef::Tab& dt1 = d.tab[1];
    const NdbDictionary::Table* pTab0 = 0;
    const NdbDictionary::Table* pTab1 = 0;
    CHK2((pTab0 = pDic->getTable(dt0.tabname)) != 0, pDic->getNdbError());
    CHK2((pTab1 = pDic->getTable(dt1.tabname)) != 0, pDic->getNdbError());
    HugoTransactions tx0(*pTab0);
    HugoTransactions tx1(*pTab1);
    // insert into child t1 - not ok
    g_err << "expect error " << errNoParent << endl;
    CHK1(tx1.loadTable(pNdb, records, batch, allowCV) != 0);
    CHK2(tx1.getNdbError().code == errNoParent, tx1.getNdbError());
    // insert into parent t0 - ok
    CHK2(tx0.loadTable(pNdb, records, batch, allowCV) == 0,
         tx0.getNdbError());
    // insert into child t1 - ok (b1 is 0, a0 is 0,1,2,..)
    CHK2(tx1.loadTable(pNdb, records, batch, allowCV) == 0,
         tx1.getNdbError());
    // delete from parent - not ok
    g_err << "expect error " << errHasChild << endl;
    CHK1(tx0.pkDelRecords(pNdb, records, batch, allowCV) != 0);
    CHK2(tx0.getNdbError().code == errHasChild, tx0.getNdbError());
    // delete from child t1 - ok
    CHK2(tx1.pkDelRecords(pNdb, records, batch, allowCV) == 0,
         tx1.getNdbError());
    // delete from parent to - ok
    CHK2(tx0.pkDelRecords(pNdb, records, batch, allowCV) == 0,
         tx0.getNdbError());
  }
  while (0);
  return result;
}

static int
fk_retrieve_list(Fkdef& d, Ndb* pNdb, Fkdef::List& list)
{
  (void)d;
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    g_info << "list objects" << endl;
    require(list.list == 0);
    list.list = new NdbDictionary::Dictionary::List;
    CHK2(pDic->listObjects(*list.list) == 0, pDic->getNdbError());
    qsort(list.list->elements, list.list->count, sizeof(list.list->elements[0]),
          fk_compare_element);
    list.keystart = 0;
    for (int i = 0; i < (int)list.list->count; i++)
    {
      NdbDictionary::Dictionary::List::Element& e =
        list.list->elements[i];
      if (e.database == 0)
      {
        e.database = new char [1];
        e.database[0] = 0;
      }
      if (!fk_type(e.type))
        list.keystart++;
      g_info << "ob " << i << ":"
             << " type=" << e.type << " id=" << e.id
             << " db=" << e.database << " name=" << e.name << endl;
      if (i > 0)
      {
        const NdbDictionary::Dictionary::List::Element& e2 =
          list.list->elements[i - 1];
        CHK1(e.type != e2.type || e.id != e2.id);
      }
    }
    g_info << "list count=" << list.list->count
           << " keystart=" << list.keystart << endl;
  }
  while (0);
  return result;
}

static int
fk_verify_list(Fkdef& d, Ndb* pNdb, bool ignore_keys)
{
  int result = NDBT_OK;
  do
  {
    Fkdef::List& list1 = d.list;
    if (list1.list == 0)
    {
      g_info << "retrieve first object list" << endl;
      CHK1(fk_retrieve_list(d, pNdb, list1) == 0);
    }
    else
    {
      g_info << "verify object list old vs new"
                " ignore_keys=" << ignore_keys << endl;
      Fkdef::List list2;
      CHK1(fk_retrieve_list(d, pNdb, list2) == NDBT_OK);
      // optionally ignore FK stuff in either list
      int count1 = !ignore_keys ? list1.list->count : list1.keystart;
      int count2 = !ignore_keys ? list2.list->count : list2.keystart;
      CHK1(count1 == count2);
      for (int i = 0; i < count1; i++)
      {
        const NdbDictionary::Dictionary::List::Element& e1 =
          list1.list->elements[i];
        const NdbDictionary::Dictionary::List::Element& e2 =
          list2.list->elements[i];
        CHK2(e1.type == e2.type,
             i << ": " << e1.type << " != " << e2.type);
        CHK2(e1.id == e2.id,
             i << ": " << e1.id << " != " << e2.id);
        CHK2(strcmp(e1.database, e2.database) == 0,
             i << ": " << e1.database << " != " << e2.database);
        CHK2(strcmp(e1.name, e2.name) == 0,
             i << ": " << e1.name << " != " << e2.name);
      }
      CHK1(result == NDBT_OK);
      // replace old by new
      delete list1.list;
      list1.list = list2.list;
      list1.keystart = list2.keystart;
      list2.list = 0;
    }
    // verify objects vs list
    for (int i = 0; i < d.ntab; i++)
    {
      const Fkdef::Tab& dt = d.tab[i];
      CHK2(fk_find_element(list1, NdbDictionary::Object::UserTable,
           "TEST_DB", dt.tabname), dt.tabname);
      for (int k = 1; k < dt.nind; k++)
      {
        const Fkdef::Ind& di = dt.ind[k];
        if (di.unique)
        {
          CHK2(fk_find_element(list1, NdbDictionary::Object::UniqueHashIndex,
               "sys", di.indname), di.indname);
        }
        else
        {
          CHK2(fk_find_element(list1, NdbDictionary::Object::OrderedIndex,
               "sys", di.indname), di.indname);
        }
      }
      CHK1(result == NDBT_OK);
    }
    for (int k = 0; k < d.nkey; k++) {
      const Fkdef::Key& dk = d.key[k];
      CHK2(fk_find_element(list1, NdbDictionary::Object::ForeignKey,
           "", dk.fullname), dk.fullname);
      // could also check FK triggers..
    }
    CHK1(result == NDBT_OK);
  }
  while (0);
  return result;
}

static int
fk_drop_table(Fkdef& d, Ndb* pNdb, int i, bool force)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    Fkdef::Tab& dt = d.tab[i];
    g_info << "drop table " << dt.tabname
           << (force ? " (force)" : "") << endl;
    if (pDic->dropTable(dt.tabname) != 0)
    {
      const NdbError& err = pDic->getNdbError();
      CHK2(force, err);
      CHK2(err.code == 709 || err.code == 723, err);
      break;
    }
    // all indexes are dropped by ndb api
    // all related FKs child/parent are dropped by ndb api
  }
  while (0);
  return result;
}

static int
fk_drop_tables(Fkdef& d, Ndb* pNdb, bool force)
{
  int result = NDBT_OK;
  for (int i = 0; i < d.ntab; i++)
  {
    CHK1(fk_drop_table(d, pNdb, i, force) == NDBT_OK);
  }
  return result;
}

static int
fk_drop_key(Fkdef& d, Ndb* pNdb, int k, bool force)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    Fkdef::Key& dk = d.key[k];
    g_info << "drop key " << dk.fullname
           << (force ? " (force)" : "") << endl;
    NdbDictionary::ForeignKey key;
    if (pDic->getForeignKey(key, dk.fullname) != 0)
    {
      const NdbError& err = pDic->getNdbError();
      CHK2(force, err);
      CHK2(err.code == 709 || err.code == 723 || err.code == 21040, err);
      break;
    }
    CHK2(pDic->dropForeignKey(key) == 0, pDic->getNdbError());
  }
  while (0);
  return result;
}

static int
fk_drop_keys(Fkdef& d, Ndb* pNdb, bool force)
{
  int result = NDBT_OK;
  for (int k = 0; k < d.nkey; k++)
  {
    CHK1(fk_drop_key(d, pNdb, k, force) == NDBT_OK);
  }
  return result;
}

static int
fk_drop_all(Fkdef& d, Ndb* pNdb, bool force)
{
  int result = NDBT_OK;
  do
  {
    CHK1(fk_drop_keys(d, pNdb, force) == NDBT_OK);
    CHK1(fk_drop_tables(d, pNdb, force) == NDBT_OK);
  }
  while (0);
  return result;
}

// commit drop

// just reset all retrieved
static void
fk_dropped_all(Fkdef& d)
{
  for (int i = 0; i < d.ntab; i++)
  {
    Fkdef::Tab& dt = d.tab[i];
    dt.retrieved = false;
    for (int k = 0; k < dt.nind; k++)
    {
      Fkdef::Ind& di = dt.ind[k];
      di.retrieved = false;
    }
  }
  for (int k = 0; k < d.nkey; k++)
  {
    Fkdef::Key& dk = d.key[k];
    dk.retrieved = false;
  }
}

// for FK_Bug18069680

static int
fk_create_all_random(Fkdef& d, Ndb* pNdb)
{
  int result = NDBT_OK;
  int ntab = 0;
  int nkey = 0;
  do
  {
    for (int i = 0; i < d.ntab; i++)
    {
      Fkdef::Tab& dt = d.tab[i];
      if (!dt.retrieved && myRandom48(3) == 0)
      {
        CHK1(fk_create_table(d, pNdb, i) == 0);
        require(dt.retrieved);
        ntab++;
      }
    }
    CHK1(result == NDBT_OK);
    for (int k = 0; k < d.nkey; k++)
    {
      Fkdef::Key& dk = d.key[k];
      if (!dk.retrieved && myRandom48(3) == 0 &&
          dk.tab0->retrieved && dk.tab1->retrieved)
      {
        CHK1(fk_create_key(d, pNdb, k) == 0);
        require(dk.retrieved);
        nkey++;
      }
    }
    CHK1(result == NDBT_OK);
    require(ntab <= d.ntab && nkey <= d.nkey);
  }
  while (ntab < d.ntab || nkey < d.nkey);
  return result;
}

static int
fk_drop_indexes_under(const Fkdef& d, Ndb* pNdb)
{
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  do
  {
    for (int i = 0; i < d.ntab; i++)
    {
      const Fkdef::Tab& dt = d.tab[i];
      for (int k = 1; k < dt.nind; k++) // skip pk
      {
        const Fkdef::Ind& di = dt.ind[k];
        int parent = 0;
        int child = 0;
        for (int m = 0; m < d.nkey; m++)
        {
          const Fkdef::Key& dk = d.key[m];
          if (dk.ind0 == &di)
            parent++;
          if (dk.ind1 == &di)
            child++;
        }
        if (parent != 0 || child != 0)
        {
          // drop must fail
          g_info << "try to drop index under " << di.indname
                 << " parent:" << parent << " child:" << child << endl;
          int ret = pDic->dropIndex(di.indname, dt.tabname);
          CHK2(ret != 0, "no error on drop underlying index");
          const NdbError& err = pDic->getNdbError();
          // could be either error code depending on check order
          CHK2(err.code == 21081 || err.code == 21082, pDic->getNdbError());
        }
      }
      CHK1(result == NDBT_OK);
    }
    CHK1(result == NDBT_OK);
  }
  while (0);
  return result;
}

// for manual testing
static void
fk_env_options(Fkdef& d)
{
  // random seed
  int seed = (int)getpid();
#ifdef NDB_USE_GET_ENV
  {
    const char* p = NdbEnv_GetEnv("RANDOM_SEED", (char*)0, 0);
    if (p != 0)
      seed = atoi(p);
  }
#endif
  myRandom48Init(seed);
  g_err << "random seed: " << seed << endl;
  // create no FKs at all
  d.nokeys = false;
#ifdef NDB_USE_GET_ENV
  {
    const char* p = NdbEnv_GetEnv("FK_NOKEYS", (char*)0, 0);
    if (p != 0 && strchr("1Y", p[0]) != 0)
      d.nokeys = true;
  }
#endif
  // do not drop objects at end
  d.nodrop = false;
#ifdef NDB_USE_GET_ENV
  {
    const char* p = NdbEnv_GetEnv("FK_NODROP", (char*)0, 0);
    if (p != 0 && strchr("1Y", p[0]) != 0)
      d.nodrop = true;
  }
#endif
}

int
runFK_SRNR(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  NdbRestarter restarter;
  const int numdbnodes = restarter.getNumDbNodes();

  Fkdef d;
  d.testcase = ctx->getProperty("testcase", (Uint32)0);
  fk_env_options(d);
  fk_define_all(d);

  do
  {
    (void)fk_drop_all(d, pNdb, true);
    CHK1(fk_create_all(d, pNdb) == NDBT_OK);
    CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
    CHK1(fk_verify_dml(d, pNdb, records) == NDBT_OK);
    CHK1(fk_verify_list(d, pNdb, false) == NDBT_OK);

    for (int loop = 0; loop < loops; loop++)
    {
      g_info << "loop " << loop << "<" << loops << endl;

      bool rs = (numdbnodes == 1 || myRandom48(2) == 0);
      if (rs)
      {
        g_info << "restart all" << endl;
        CHK1(restarter.restartAll() == 0);
      }
      else
      {
        int i = myRandom48(numdbnodes);
        int nodeid = restarter.getDbNodeId(i);
        bool initial = (bool)myRandom48(2);
        bool nostart = true;
        g_info << "restart node " << nodeid << " initial=" << initial << endl;

        CHK1(restarter.restartOneDbNode(nodeid, initial, nostart) == 0);
        CHK1(restarter.waitNodesNoStart(&nodeid, 1) == 0);
        g_info << "nostart node " << nodeid << endl;

        CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
        CHK1(fk_verify_dml(d, pNdb, records) == NDBT_OK);
        CHK1(fk_verify_list(d, pNdb, false) == NDBT_OK);

        g_info << "start node " << nodeid << endl;
        CHK1(restarter.startNodes(&nodeid, 1) == 0);
      }

      CHK1(restarter.waitClusterStarted() == 0);
      g_info << "cluster is started" << endl;

      CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
      CHK1(fk_verify_dml(d, pNdb, records) == NDBT_OK);
      CHK1(fk_verify_list(d, pNdb, false) == NDBT_OK);
    }
    CHK1(result == NDBT_OK);

    if (!d.nodrop)
    {
      CHK1(fk_drop_all(d, pNdb, false) == NDBT_OK);
    }
  }
  while (0);

  if (result != NDBT_OK)
  {
    if (!d.nodrop)
      (void)fk_drop_all(d, pNdb, true);
  }
  return result;
}

int
runFK_TRANS(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;
  const int abort_flag = NdbDictionary::Dictionary::SchemaTransAbort;

  Fkdef d;
  d.testcase = ctx->getProperty("testcase", (Uint32)0);
  fk_env_options(d);
  fk_define_tables(d);
  fk_undefine_keys(d);

  do
  {
    (void)fk_drop_all(d, pNdb, true);
    CHK1(fk_create_tables(d, pNdb) == NDBT_OK);
    CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
    CHK1(fk_verify_list(d, pNdb, false) == NDBT_OK);

    // what to do on loop % 3
    const int abort_loop[3][2] = { { 1, -1 }, { 0, 1 }, { 0, 0 } };

    for (int loop = 0; loop < loops; loop++)
    {
      g_info << "loop " << loop << "<" << loops << endl;

      int abort_create = abort_loop[loop % 3][0];
      require(abort_create == 0 || abort_create == 1);
      g_info << "abort create: " << abort_create << endl;

      fk_define_keys(d);
      CHK2(pDic->beginSchemaTrans() == 0, pDic->getNdbError());
      CHK1(fk_create_keys(d, pNdb) == 0);
      if (!abort_create)
      {
        g_info << "commit schema trans" << endl;
        CHK2(pDic->endSchemaTrans(0) == 0, pDic->getNdbError());
        CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
        CHK1(fk_verify_dml(d, pNdb, records) == NDBT_OK);
        CHK1(fk_verify_list(d, pNdb, true) == NDBT_OK);
      }
      else
      {
        g_info << "abort schema trans" << endl;
        CHK2(pDic->endSchemaTrans(abort_flag) == 0, pDic->getNdbError());
        fk_undefine_keys(d);
        CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
        CHK1(fk_verify_list(d, pNdb, false) == NDBT_OK);
        continue; // nothing to drop
      }

      int abort_drop = abort_loop[loop % 3][1];
      require(abort_drop == 0 || abort_drop == 1);
      g_info << "abort drop: " << abort_drop << endl;

      CHK2(pDic->beginSchemaTrans() == 0, pDic->getNdbError());
      CHK1(fk_drop_keys(d, pNdb, false) == 0);
      if (!abort_drop)
      {
        g_info << "commit schema trans" << endl;
        CHK2(pDic->endSchemaTrans(0) == 0, pDic->getNdbError());
        fk_undefine_keys(d);
        CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
        CHK1(fk_verify_list(d, pNdb, true) == NDBT_OK);
      }
      else
      {
        g_info << "abort schema trans" << endl;
        CHK2(pDic->endSchemaTrans(abort_flag) == 0, pDic->getNdbError());
        CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
        CHK1(fk_verify_dml(d, pNdb, records) == NDBT_OK);
        CHK1(fk_verify_list(d, pNdb, false) == NDBT_OK);
        // prepare for next round
        CHK1(fk_drop_keys(d, pNdb, false) == NDBT_OK);
        fk_undefine_keys(d);
      }
    }
    CHK1(result == NDBT_OK);

    if (!d.nodrop)
    {
      CHK1(fk_drop_all(d, pNdb, false) == NDBT_OK);
    }
  }
  while (0);

  if (result != NDBT_OK)
  {
    (void)pDic->endSchemaTrans(abort_flag);
    if (!d.nodrop)
      (void)fk_drop_all(d, pNdb, true);
  }
  return result;
}

int
runFK_Bug18069680(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  Fkdef d;
  d.testcase = ctx->getProperty("testcase", (Uint32)0);
  fk_env_options(d);
  fk_define_all(d);

  do
  {
    (void)fk_drop_all(d, pNdb, true);

    for (int loop = 0; loop < loops; loop++)
    {
      g_info << "loop " << loop << "<" << loops << endl;

      CHK1(fk_create_all_random(d, pNdb) == NDBT_OK);
      CHK1(fk_verify_ddl(d, pNdb) == NDBT_OK);
      CHK1(fk_verify_dml(d, pNdb, records) == NDBT_OK);

      CHK1(fk_drop_indexes_under(d, pNdb) == NDBT_OK);
      CHK1(fk_drop_tables(d, pNdb, false) == NDBT_OK);

      fk_dropped_all(d);
    }
    CHK1(result == NDBT_OK);
  }
  while (0);

  if (result != NDBT_OK)
  {
    if (!d.nodrop)
      (void)fk_drop_all(d, pNdb, true);
  }
  return result;
}

#undef myRandom48
#undef myRandom48Init

int
runDictTO_1(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 3)
    return NDBT_OK;

  for (int i = 0; i < ctx->getNumLoops(); i++)
  {
    int master = restarter.getMasterNodeId();
    int next = restarter.getNextMasterNodeId(master);
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    restarter.dumpStateOneNode(master, val2, 2);
    restarter.insertError2InNode(master, 6050, next);

    ndbout_c("master: %d next: %d", master, next);
    {
      g_info << "save all resource usage" << endl;
      int dump1[] = { DumpStateOrd::SchemaResourceSnapshot };
      restarter.dumpStateAllNodes(dump1, 1);
    }


    {
      if (pDic->beginSchemaTrans() != 0)
      {
        ndbout << "ERROR: line: " << __LINE__ << endl;
        ndbout << pDic->getNdbError();
        return NDBT_FAILED;
      }
      for (int j = 0; j < (i + 1); j++)
      {
        NdbDictionary::Table pTab(* ctx->getTab());
        pTab.setName(BaseString(pTab.getName()).appfmt("_EXTRA_%u", j).c_str());

        if (pDic->createTable(pTab) != 0)
        {
          ndbout << "ERROR: line: " << __LINE__ << endl;
          ndbout << pDic->getNdbError();
          return NDBT_FAILED;
        }
      }

      // this should give master failuer...but trans should rollforward
      if (pDic->endSchemaTrans() != 0)
      {
        ndbout << "ERROR: line: " << __LINE__ << endl;
        ndbout << pDic->getNdbError();
        return NDBT_FAILED;
      }
    }

    for (int j = 0; j < (i + 1); j++)
    {
      pDic->dropTable(BaseString(ctx->getTab()->getName()).appfmt("_EXTRA_%u", j).c_str());
    }

    {
      g_info << "check all resource usage" << endl;
      for (int j = 0; j < restarter.getNumDbNodes(); j++)
      {
        if (restarter.getDbNodeId(j) == master)
          continue;

        int dump1[] = { DumpStateOrd::SchemaResourceCheckLeak };
        restarter.dumpStateOneNode(restarter.getDbNodeId(j), dump1, 1);
      }
    }

    restarter.waitNodesNoStart(&master, 1);
    restarter.startNodes(&master, 1);
    restarter.waitClusterStarted();
  }

  return NDBT_OK;
}

NDBT_TESTSUITE(testDict);
TESTCASE("testDropDDObjects",
         "* 1. start cluster\n"
         "* 2. Create LFG\n"
         "* 3. create TS\n"
         "* 4. run DropDDObjects\n"
         "* 5. Verify DropDDObjectsRestart worked\n"){
INITIALIZER(runWaitStarted);
INITIALIZER(runDropDDObjects);
INITIALIZER(testDropDDObjectsSetup);
STEP(runDropDDObjects);
FINALIZER(DropDDObjectsVerify);
}

TESTCASE("Bug29501",
         "* 1. start cluster\n"
         "* 2. Restart 1 node -abort -nostart\n"
         "* 3. create LFG\n"
         "* 4. Restart data node\n"
         "* 5. Restart 1 node -nostart\n"
         "* 6. Drop LFG\n"){
INITIALIZER(runWaitStarted);
INITIALIZER(runDropDDObjects);
STEP(runBug29501);
FINALIZER(runDropDDObjects);
}
TESTCASE("CreateAndDrop", 
	 "Try to create and drop the table loop number of times\n"){
  INITIALIZER(runCreateAndDrop);
}
TESTCASE("CreateAndDropAtRandom",
	 "Try to create and drop table at random loop number of times\n"
         "Uses all available tables\n"
         "Uses error insert 4013 to make TUP verify table descriptor"){
  INITIALIZER(runCreateAndDropAtRandom);
}
TESTCASE("CreateAndDropIndexes",
	 "Like CreateAndDropAtRandom but also creates random ordered\n"
         "indexes and loads data as a simple check of index operation"){
  TC_PROPERTY("CreateIndexes", 1);
  TC_PROPERTY("LoadData", 1);
  INITIALIZER(runCreateAndDropAtRandom);
}
TESTCASE("CreateAndDropWithData", 
	 "Try to create and drop the table when it's filled with data\n"
	 "do this loop number of times\n"){
  INITIALIZER(runCreateAndDropWithData);
}
TESTCASE("CreateAndDropDuring", 
	 "Try to create and drop the table when other thread is using it\n"
	 "do this loop number of times\n"){
  STEP(runCreateAndDropDuring);
  STEP(runUseTableUntilStopped);
}
TESTCASE("DropWithTakeover","bug 14190114"){
  INITIALIZER(runDropTakeoverTest);
}
TESTCASE("CreateInvalidTables", 
	 "Try to create the invalid tables we have defined\n"){ 
  INITIALIZER(runCreateInvalidTables);
}
TESTCASE("DropTableConcurrentLCP",
         "Drop a table while LCP is ongoing\n")
{
  INITIALIZER(runCreateTheTable);
  INITIALIZER(runFillTable);
  INITIALIZER(runSetMinTimeBetweenLCP);
  INITIALIZER(runSetDropTableConcurrentLCP);
  INITIALIZER(runDropTheTable);
  FINALIZER(runResetMinTimeBetweenLCP);
}
TESTCASE("DropTableConcurrentLCP2",
         "Drop a table while LCP is ongoing\n")
{
  INITIALIZER(runCreateTheTable);
  INITIALIZER(runFillTable);
  INITIALIZER(runSetMinTimeBetweenLCP);
  INITIALIZER(runSetDropTableConcurrentLCP2);
  INITIALIZER(runDropTheTable);
  FINALIZER(runResetMinTimeBetweenLCP);
}
TESTCASE("CreateTableWhenDbIsFull", 
	 "Try to create a new table when db already is full\n"){ 
  INITIALIZER(runCreateTheTable);
  INITIALIZER(runFillTable);
  INITIALIZER(runCreateTableWhenDbIsFull);
  INITIALIZER(runDropTableWhenDbIsFull);
  FINALIZER(runDropTheTable);
}
TESTCASE("FragmentTypeSingle", 
	 "Create the table with fragment type Single\n"){
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragSingle);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("FragmentTypeAllSmall", 
	 "Create the table with fragment type AllSmall\n"){ 
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragAllSmall);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("FragmentTypeAllMedium", 
	 "Create the table with fragment type AllMedium\n"){ 
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragAllMedium);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("FragmentTypeAllLarge", 
	 "Create the table with fragment type AllLarge\n"){ 
  TC_PROPERTY("FragmentType", NdbDictionary::Table::FragAllLarge);
  INITIALIZER(runTestFragmentTypes);
}
TESTCASE("TemporaryTables", 
	 "Create the table as temporary and make sure it doesn't\n"
	 "contain any data when system is restarted\n"){ 
  INITIALIZER(runTestTemporaryTables);
}
TESTCASE("CreateMaxTables", 
	 "Create tables until db says that it can't create any more\n"){
  TC_PROPERTY("tables", 1000);
  INITIALIZER(runCreateMaxTables);
  INITIALIZER(runDropMaxTables);
}
TESTCASE("PkSizes", 
	 "Create tables with all different primary key sizes.\n"\
	 "Test all data operations insert, update, delete etc.\n"\
	 "Drop table."){
  INITIALIZER(runPkSizes);
}
TESTCASE("StoreFrm", 
	 "Test that a frm file can be properly stored as part of the\n"
	 "data in Dict."){
  INITIALIZER(runStoreFrm);
}
TESTCASE("GetPrimaryKey", 
	 "Test the function NdbDictionary::Column::getPrimaryKey\n"
	 "It should return true only if the column is part of \n"
	 "the primary key in the table"){
  INITIALIZER(runGetPrimaryKey);
}
TESTCASE("StoreFrmError", 
	 "Test that a frm file with too long length can't be stored."){
  INITIALIZER(runStoreFrmError);
}
TESTCASE("TableRename",
	 "Test basic table rename"){
  INITIALIZER(runTableRename);
}
TESTCASE("TableRenameSR",
	 "Test that table rename can handle system restart"){
  INITIALIZER(runTableRenameSR);
}
TESTCASE("DictionaryPerf",
	 ""){
  INITIALIZER(runTestDictionaryPerf);
}
TESTCASE("CreateLogfileGroup", ""){
  INITIALIZER(runCreateLogfileGroup);
}
TESTCASE("CreateTablespace", ""){
  INITIALIZER(runCreateTablespace);
}
TESTCASE("CreateDiskTable", ""){
  INITIALIZER(runCreateDiskTable);
}
TESTCASE("FailAddFragment",
         "Fail add fragment or attribute in ACC or TUP or TUX\n"){
  INITIALIZER(runFailAddFragment);
}
TESTCASE("Restart_NF1",
         "DICT ops during node graceful shutdown (not master)"){
  TC_PROPERTY("Restart_NF_ops", 1);
  TC_PROPERTY("Restart_NF_type", 1);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("Restart_NF2",
         "DICT ops during node shutdown abort (not master)"){
  TC_PROPERTY("Restart_NF_ops", 1);
  TC_PROPERTY("Restart_NF_type", 2);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("Restart_NR1",
         "DICT ops during node startup (not master)"){
  TC_PROPERTY("Restart_NR_ops", 1);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("Restart_NR2",
         "DICT ops during node startup with crash inserts (not master)"){
  TC_PROPERTY("Restart_NR_ops", 1);
  TC_PROPERTY("Restart_NR_error", 1);
  STEP(runRestarts);
  STEP(runDictOps);
}
TESTCASE("TableAddAttrs",
	 "Add attributes to an existing table using alterTable()"){
  INITIALIZER(runTableAddAttrs);
}
TESTCASE("TableAddAttrsDuring",
	 "Try to add attributes to the table when other thread is using it\n"
	 "do this loop number of times\n"){
  INITIALIZER(runCreateTheTable);
  STEP(runTableAddAttrsDuring);
  STEP(runUseTableUntilStopped2);
  STEP(runUseTableUntilStopped3);
  FINALIZER(runDropTheTable);
}
TESTCASE("TableAddAttrsDuringError",
	 "Try to add attributes to the table when other thread is using it\n"
	 "do this loop number of times\n"){
  TC_PROPERTY("AbortAlter", 1);
  INITIALIZER(runCreateTheTable);
  STEP(runTableAddAttrsDuring);
  STEP(runUseTableUntilStopped2);
  STEP(runUseTableUntilStopped3);
  FINALIZER(runDropTheTable);
}
TESTCASE("Bug21755",
         ""){
  INITIALIZER(runBug21755);
}
TESTCASE("DictRestart",
         ""){
  INITIALIZER(runDictRestart);
}
TESTCASE("Bug24631",
         ""){
  INITIALIZER(runBug24631);
}
TESTCASE("Bug36702", "")
{
  INITIALIZER(runDropDDObjects);
  INITIALIZER(runBug36072);
  FINALIZER(restartClusterInitial);
}
TESTCASE("Bug29186",
         ""){
  INITIALIZER(runBug29186);
}
TESTCASE("Bug48604",
         "Online ordered index build.\n"
         "Complements testOIBasic -case f"){
  STEP(runBug48604);
  STEP(runBug48604ops);
#if 0 // for future MT test
  STEP(runBug48604ops);
  STEP(runBug48604ops);
  STEP(runBug48604ops);
#endif
}
TESTCASE("Bug54651", ""){
  INITIALIZER(runBug54651);
}
/** telco-6.4 **/
TESTCASE("SchemaTrans",
         "Schema transactions"){
  ALL_TABLES();
  STEP(runSchemaTrans);
}
TESTCASE("FailCreateHashmap",
         "Fail create hashmap")
{
  INITIALIZER(runFailCreateHashmap);
}
TESTCASE("FailAddPartition",
         "Fail add partition")
{
  INITIALIZER(runFailAddPartition);
}
TESTCASE("TableAddPartitions",
	 "Add partitions to an existing table using alterTable()"){
  INITIALIZER(runTableAddPartition);
}
TESTCASE("Bug41905",
	 ""){
  STEP(runBug41905);
  STEP(runBug41905getTable);
}
TESTCASE("Bug46552", "")
{
  INITIALIZER(runBug46552);
}
TESTCASE("Bug46585", "")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(runBug46585);
}
TESTCASE("Bug53944", "")
{
  INITIALIZER(runBug53944);
}
TESTCASE("Bug58277",
         "Dropping busy ordered index can crash data node.\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("RSS_CHECK", (Uint32)true);
  TC_PROPERTY("RANGE_MAX", (Uint32)5);
  INITIALIZER(runBug58277errtest);
  STEP(runBug58277);
  // sub-steps 2-8 synced with main step
  TC_PROPERTY("SubSteps", 7);
  STEP(runBug58277drop);
  /*
   * A single scan update can show the bug but this is not likely.
   * Add more scan updates.  Also add PK ops for other asserts.
   */
  STEP(runBug58277scan);
  STEP(runBug58277scan);
  STEP(runBug58277scan);
  STEP(runBug58277scan);
  STEP(runBug58277pk);
  STEP(runBug58277pk);
  // kernel side scans (eg. LCP) for resource usage check
  STEP(runBug58277rand);
}
TESTCASE("Bug57057",
         "MRR + delete leaks stored procs (fixed under Bug58277).\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("RSS_CHECK", (Uint32)true);
  TC_PROPERTY("RANGE_MAX", (Uint32)100);
  TC_PROPERTY("SCAN_DELETE", (Uint32)1);
  STEP(runBug57057);
  TC_PROPERTY("SubSteps", 1);
  STEP(runBug58277scan);
}
TESTCASE("GetTabInfoRef", "Regression test for bug #14647210 'CAN CRASH ALL "
         "NODES EASILY WHEN RESTARTING MORE THAN 6 NODES SIMULTANEOUSLY'"
         " (missing handling of GET_TABINFOREF signal).")
{
  INITIALIZER(runGetTabInfoRef);
}
TESTCASE("Bug13416603", "")
{
  INITIALIZER(runCreateTheTable);
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug13416603);
  FINALIZER(runDropTheTable);
}
TESTCASE("IndexStatCreate", "")
{
  STEPS(runIndexStatCreate, 10);
}
TESTCASE("WL946",
         "Time types with fractional seconds.\n"
         "Give any tablename as argument (T1)"){
  INITIALIZER(runWL946);
}
TESTCASE("Bug14645319", "")
{
  STEP(runBug14645319);
}
TESTCASE("FK_SRNR1",
         "Foreign keys SR/NR, simple case with DDL and DML checks.\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("testcase", 1);
  INITIALIZER(runFK_SRNR);
}
TESTCASE("FK_SRNR2",
         "Foreign keys SR/NR, complex case with DDL checks .\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("testcase", 2);
  INITIALIZER(runFK_SRNR);
}
TESTCASE("FK_TRANS1",
         "Foreign keys schema trans, simple case with DDL and DML checks.\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("testcase", 1);
  INITIALIZER(runFK_TRANS);
}
TESTCASE("FK_TRANS2",
         "Foreign keys schema trans, complex case with DDL checks.\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("testcase", 2);
  INITIALIZER(runFK_TRANS);
}
TESTCASE("FK_Bug18069680",
         "NDB API drop table with foreign keys.\n"
         "Give any tablename as argument (T1)"){
  TC_PROPERTY("testcase", 2);
  INITIALIZER(runFK_Bug18069680);
}
TESTCASE("CreateHashmaps",
         "Create (default) hashmaps")
{
  INITIALIZER(runCreateHashmaps);
}
TESTCASE("DictTakeOver_1", "")
{
  INITIALIZER(runDictTO_1);
}
NDBT_TESTSUITE_END(testDict);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testDict);
  // Tables should not be auto created
  testDict.setCreateTable(false);
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  return testDict.execute(argc, argv);
}
