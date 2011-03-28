/*
   Copyright (C) 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <DbUtil.hpp>
#include <mysql.h>

/*
Will include restart testing in future phases
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
*/

/**** TOOL SECTION ****/

static uint
urandom()
{
  uint r = (uint)random();
  return r;
}

static uint
urandom(uint m)
{
  if (m == 0)
    return NDBT_OK;
  uint r = urandom();
  r = r % m;
  return r;
}

bool
syncSlaveWithMaster()
{
  /* 
     We need to look at the MAX epoch of the
     mysql.ndb_binlog_index table so we will
     know when the slave has caught up
  */

  SqlResultSet result;
  unsigned long long masterEpoch = 0;
  unsigned long long slaveEpoch = 0;
  unsigned long long  slaveEpochOld = 0;
  int maxLoops = 100;
  int loopCnt = 0;
  
  //Create a DbUtil object for the master
  DbUtil master("mysql");

  //Login to Master
  if (!master.connect())
  {
    g_err << "sync connect to master failed" << endl;
    return false;
  } 

    //Get max epoch from master
  if(!master.doQuery("SELECT MAX(epoch) FROM mysql.ndb_binlog_index", result))
  {
    g_err << "Select max(epoch) SQL failed" << endl;
    return false;
  }
  masterEpoch = result.columnAsLong("epoch");
  
  /*
     Now we will pull current epoch from slave. If not the
     same as master, we will continue to retrieve the epoch
     and compare until it matches or we reach the max loops
     allowed.
  */

  //Create a dbutil object for the slave
  DbUtil slave("mysql", ".1.slave");

  //Login to slave
  if (!slave.connect())
  {
    g_err << "sync connect to slave failed" << endl;
    return false;
  }

  while(slaveEpoch != masterEpoch && loopCnt < maxLoops)
  {
    if(!slave.doQuery("SELECT epoch FROM mysql.ndb_apply_status",result))
    {
      g_err << "Select epoch SQL on slave failed" << endl;
      return false;
    }
    result.print();
    if (result.numRows() > 0)
      slaveEpoch = result.columnAsLong("epoch");
   
    if(slaveEpoch != slaveEpochOld)
    {
      slaveEpochOld = slaveEpoch;
      if(loopCnt > 0)
        loopCnt--;
      sleep(3);
    }
    else
    {
      sleep(1);
      loopCnt++;
    }
  }

  if(slaveEpoch != masterEpoch)
  {
    g_err << "Slave not in sync with master!" << endl;
    return false;
  }
  return true;
}

bool
verifySlaveLoad(BaseString &table)
{
  //BaseString  sqlStm;
  BaseString  db;
  unsigned int masterCount = 0;
  unsigned int slaveCount  = 0;
 
  db.assign("TEST_DB");
  //sqlStm.assfmt("SELECT COUNT(*) FROM %s", table);

  //First thing to do is sync slave
  printf("Calling syncSlave\n");
  if(!syncSlaveWithMaster())
  {
    g_err << "Verify Load -> Syncing with slave failed" << endl;
    return false;
  }

  //Now that slave is sync we can verify load
  DbUtil master(db.c_str());

  //Login to Master
  if (!master.connect())
  {
    g_err << "Verify Load -> connect to master failed" << endl;
    return false;
  }

  if((masterCount = master.selectCountTable(table.c_str())) == 0 )
  {
    g_err << "Verify Load -> masterCount == ZERO!" << endl;
    return false;
  }
  
  //Create a DB Object for slave
  DbUtil slave(db.c_str(), ".1.slave");

  //Login to slave
  if (!slave.connect())
  {
    g_err << "Verify Load -> connect to master failed" << endl;
    return false;
  }

  if((slaveCount = slave.selectCountTable(table.c_str())) == 0 )
  {
    g_err << "Verify Load -> slaveCount == ZERO" << endl;
    return false;
  }
  
  if(slaveCount != masterCount)
  {
    g_err << "Verify Load -> Slave Count != Master Count "
          << endl;
    return false;
  }
  return true;
}

int
createTEST_DB(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString cdb;
  cdb.assign("TEST_DB");

  //Create a dbutil object
  DbUtil master("mysql");

  if (!master.connect())
  {
    g_err << "Create DB -> Connect to master failed"
          << endl;
    return NDBT_FAILED;
  }

  if (!master.createDb(cdb))
  {
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
dropTEST_DB(NDBT_Context* ctx, NDBT_Step* step)
{
  //Create an SQL Object
  DbUtil master("mysql");

  //Login to Master
  if (!master.connect())
  {
    g_err << "Drop DB -> Connect to master failed"
          << endl;
    return NDBT_FAILED;
  }

  if(!master.doQuery("DROP DATABASE TEST_DB"))
  {
    g_err << "Drop DB -> SQL failed"
          << endl;
    return NDBT_FAILED;
  }

  if(!syncSlaveWithMaster())
  {
    g_err << "Drop DB -> Syncing with slave failed"
          << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
verifySlave(BaseString& sqlStm, BaseString& db, BaseString& column)
{
  SqlResultSet result;
  float       masterSum;
  float       slaveSum;

  //Create SQL Objects
  DbUtil     master(db.c_str());
  DbUtil     slave(db.c_str(), ".1.slave");

  if(!syncSlaveWithMaster())
  {
    g_err << "Verify Slave -> Syncing with slave failed"
          << endl;
    return NDBT_FAILED;
  }

  //Login to Master
  if (!master.connect())
  {
    g_err << "Verify Slave -> connect master failed"
          << endl;
    return NDBT_FAILED;
  }

  if(!master.doQuery(sqlStm.c_str(),result))
  {
    return NDBT_FAILED;
  }
  masterSum = result.columnAsInt(column.c_str());
  
  //Login to slave
  if (!slave.connect())
  {
    return NDBT_FAILED;
  }

  if(!slave.doQuery(sqlStm.c_str(),result))
  {
     return NDBT_FAILED;
  }
  slaveSum = result.columnAsInt(column.c_str());
  
  if(masterSum != slaveSum)
  {
    g_err << "VerifySlave -> masterSum != slaveSum..." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


/**** Test Section ****/

int
createTable_rep1(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString table;
  BaseString db;

  table.assign("rep1");
  db.assign("TEST_DB");

  //Ensure slave is up and ready
  if(!syncSlaveWithMaster())
  {
    g_err << "Create Table -> Syncing with slave failed"
          << endl;
    return NDBT_FAILED;
  }

  //Create an SQL Object
  DbUtil master(db.c_str());

  //Login to Master
  if (!master.connect())
  {
    g_err << "Create Table -> Connect to Master failed"
          << endl;
    return NDBT_FAILED;
  }

  if (!master.doQuery("CREATE TABLE rep1 (c1 MEDIUMINT NOT NULL AUTO_INCREMENT,"
                     " c2 FLOAT, c3 CHAR(5), c4 TEXT(8), c5 FLOAT, c6 INT,"
                     " c7 INT, PRIMARY KEY (c1))ENGINE=NDB"))
  {
    g_err << "Create Table -> Create table SQL failed"
          << endl;
    return NDBT_FAILED;
  }

  /* Not happy with the data hugo generated

  Ndb* ndb=GETNDB(step);
  NdbDictionary::Dictionary* myDict = ndb->getDictionary();
  const NdbDictionary::Table *ndbTab = myDict->getTable(table.c_str());
  HugoTransactions hugoTrans(*ndbTab);

  if (hugoTrans.loadTable(GETNDB(step), ctx->getNumRecords(), 1, true, 0)
      == NDBT_FAILED)
  {
    g_err << "Create Table -> Hudo Load failed!" << endl;
    return NDBT_FAILED;
  }

  */

  for(int i = 0; i < ctx->getNumRecords(); i++)
  {
    if (!master.doQuery("INSERT INTO rep1 VALUES(NULL, 0, 'TEXAS', 'works', 0, 2, 1)"))
    {
       g_err << "Create Table -> Insert SQL failed"
             << endl;
       return NDBT_FAILED;
    }
  }

  if(!verifySlaveLoad(table))
  {
    g_err << "Create Table -> Failed on verify slave load!" 
          << endl;
    return NDBT_FAILED;
  }
  //else everything is okay  
  return NDBT_OK;
}

int
stressNDB_rep1(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* ndb=GETNDB(step);
  NdbDictionary::Dictionary* myDict = ndb->getDictionary();
  const NdbDictionary::Table * table = myDict->getTable("rep1");
  HugoTransactions hugoTrans(* table);
  while(!ctx->isTestStopped())
  {
    if (hugoTrans.pkUpdateRecords(GETNDB(step), ctx->getNumRecords(), 1, 30)
        == NDBT_FAILED)
    {
      g_err << "pkUpdate Failed!" << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans.scanUpdateRecords(GETNDB(step), ctx->getNumRecords(), 1, 30)
        == NDBT_FAILED)
    {
      g_err << "scanUpdate Failed!" << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int
stressSQL_rep1(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString sqlStm;

  DbUtil master("TEST_DB");
  int loops = ctx->getNumLoops();
  uint record = 0;

  //Login to Master
  if (!master.connect())
  {
    ctx->stopTest();
    return NDBT_FAILED;
  }

  for (int j= 0; loops == 0 || j < loops; j++)
  {
    record = urandom(ctx->getNumRecords());
    sqlStm.assfmt("UPDATE TEST_DB.rep1 SET c2 = 33.3221 where c1 =  %u", record);
    if(!master.doQuery(sqlStm.c_str()))
    {
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

int
verifySlave_rep1(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString sql;
  BaseString db;
  BaseString column;

  sql.assign("SELECT SUM(c3) FROM rep1");
  db.assign("TEST_DB");
  column.assign("c3");

  if (!verifySlave(sql,db,column))
    return NDBT_FAILED;
  return NDBT_OK;
}

/* TOOLS LIST

 syncSlaveWithMaster() 
 {ensures slave is at same epoch as master}

 verifySlaveLoad(BaseString *table) 
 {ensures slave table has same record count as master}

 createTEST_DB() 
 {Creates TEST_DB database on master}

 dropTEST_DB() 
 {Drops TEST_DB database on master} 

 verifySlave(BaseString& sql, BaseSting& db, BaseSting& column) 
 {The SQL statement must sum a column and will verify
  that the sum of the column is equal on master & slave}
*/
                     

NDBT_TESTSUITE(NdbRepStress);
TESTCASE("PHASE_I_Stress","Basic Replication Stressing") 
{
  INITIALIZER(createTEST_DB);
  INITIALIZER(createTable_rep1);
  //STEP(stressNDB_rep1);
  STEP(stressSQL_rep1);
  FINALIZER(verifySlave_rep1);
  FINALIZER(dropTEST_DB);
}
NDBT_TESTSUITE_END(NdbRepStress);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(NdbRepStress);
  NdbRepStress.setCreateAllTables(true);
  return NdbRepStress.execute(argc, argv);
}

