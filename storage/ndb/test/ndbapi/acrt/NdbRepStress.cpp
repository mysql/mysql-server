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

#define GETNDB(ps) ((NDBT_NdbApiStep*)ps)->getNdb()
/*
*/

int 
syncSlaveWithMaster()
{
  /* 
     We need to look at the MAX epoch of the
     mysql.ndb_binlog_index table so we will
     know when the slave has caught up
  */

  MYSQL_RES * result;
  MYSQL_ROW   row;
  unsigned int masterEpoch = 0;
  unsigned int slaveEpoch = 0;
  unsigned int slaveEpochOld = 0;
  int maxLoops = 100;
  int loopCnt = 0;

  //Create a DbUtil object for the master
  DbUtil master("mysql","");

  //Login to Master
  if (!master.connect())
  {
    return NDBT_FAILED;
  } 

  //Get max epoch from master
  if(master.doQuery("SELECT MAX(epoch) FROM mysql.ndb_binlog_index"))
  {
    return NDBT_FAILED;
  }
  result = mysql_use_result(master.getMysql());
  row    = mysql_fetch_row(result);
  masterEpoch = atoi(row[0]);
  mysql_free_result(result);

  /*
     Now we will pull current epoch from slave. If not the
     same as master, we will continue to retrieve the epoch
     and compare until it matches or we reach the max loops
     allowed.
  */

  //Create a dbutil object for the slave
  DbUtil slave("mysql",".slave");

  //Login to slave
  if (!slave.connect())
  {
    return NDBT_FAILED;
  }

  while(slaveEpoch != masterEpoch && loopCnt < maxLoops)
  {
    if(slave.doQuery("SELECT epoch FROM mysql.ndb_apply_status"))
    {
      return NDBT_FAILED;
    }
    result = mysql_use_result(slave.getMysql());
    row    = mysql_fetch_row(result);
    slaveEpoch = atoi(row[0]);
    mysql_free_result(result);

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
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
verifySlaveLoad(BaseString *table)
{
  BaseString  sqlStm;
  BaseString  db;
  MYSQL_RES * result;
  MYSQL_ROW   row;
  unsigned int masterCount = 0;
  unsigned int slaveCount  = 0;
 
  db.assign("TEST_DB");
  sqlStm.assfmt("SELECT COUNT(*) FROM %s", table);

  //First thing to do is sync slave
  if(syncSlaveWithMaster())
  {
    g_err << "Verify Load -> Syncing with slave failed" << endl;
    return NDBT_FAILED;
  }

  //Now that slave is sync we can verify load
  DbUtil master(db.c_str()," ");

  //Login to Master
  if (!master.connect())
  {
    return NDBT_FAILED;
  }

  if(master.doQuery(sqlStm.c_str()))
  {
    return NDBT_FAILED;
  }
  result = mysql_use_result(master.getMysql());
  row    = mysql_fetch_row(result);
  masterCount = atoi(row[0]);
  mysql_free_result(result);

  //Create a DB Object for slave
  DbUtil slave(db.c_str(),".slave");

  //Login to slave
  if (!slave.connect())
  {
    return NDBT_FAILED;
  }

  if(slave.doQuery(sqlStm.c_str()))
  {
    return NDBT_FAILED;
  }
  result = mysql_use_result(slave.getMysql());
  row    = mysql_fetch_row(result);
  slaveCount = atoi(row[0]);
  mysql_free_result(result);

  if(slaveCount != masterCount)
  {
    g_err << "Verify Load -> Slave Count != Master Count "
          << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
createTEST_DB(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString cdb;
  cdb.assign("TEST_DB");

  //Create a dbutil object
  DbUtil master("mysql","");

  if (master.connect())
  {
    if (master.createDb(cdb) == NDBT_OK)
    {
      return NDBT_OK;
    }
  }
  return NDBT_FAILED;
}

int
dropTEST_DB(NDBT_Context* ctx, NDBT_Step* step)
{
  //Create an SQL Object
  DbUtil master("mysql","");

  //Login to Master
  if (!master.connect())
  {
    return NDBT_FAILED;
  }

  if(master.doQuery("DROP DATABASE TEST_DB") != NDBT_OK)
  {
    return NDBT_FAILED;
  }

  if(syncSlaveWithMaster() != NDBT_OK)
  {
    g_err << "Drop DB -> Syncing with slave failed"
          << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
verifySlave(BaseString& sqlStm, BaseString& db)
{
  MYSQL_RES*  resource;
  MYSQL_ROW   row;
  float       masterSum;
  float       slaveSum;

  //Create SQL Objects
  DbUtil     master(db.c_str(),"");
  DbUtil     slave(db.c_str(),".slave");

  if(syncSlaveWithMaster() != NDBT_OK)
  {
    g_err << "Verify Slave rep1 -> Syncing with slave failed"
          << endl;
    return NDBT_FAILED;
  }

  //Login to Master
  if (!master.connect())
  {
    return NDBT_FAILED;
  }

  if(master.doQuery(sqlStm.c_str()) != NDBT_OK)
  {
    return NDBT_FAILED;
  }
  resource = mysql_use_result(master.getMysql());
  row = mysql_fetch_row(resource);
  masterSum = atoi(row[0]);
  mysql_free_result(resource);

  //Login to slave
  if (!slave.connect())
  {
    return NDBT_FAILED;
  }

  if(slave.doQuery(sqlStm.c_str()) != NDBT_OK)
  {
     return NDBT_FAILED;
  }
  resource = mysql_use_result(slave.getMysql());
  row = mysql_fetch_row(resource);
  slaveSum = atoi(row[0]);
  mysql_free_result(resource);

  if(masterSum != slaveSum)
  {
    g_err << "VerifySlave -> masterSum != slaveSum..." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


/**** Test Section ****/

int 
createDB(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString cdb;
  cdb.assign("TEST_DB");

  //Create a dbutil object
  DbUtil master("mysql","");

  if (master.connect())
  {
    if (master.createDb(cdb) == NDBT_OK)
    {
      return NDBT_OK;
    }
  }
  return NDBT_FAILED;
}

int
createTable_rep1(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString table;
  BaseString db;

  table.assign("rep1");
  db.assign("TEST_DB");

  //Ensure slave is up and ready
  if(syncSlaveWithMaster() != NDBT_OK)
  {
    g_err << "Create Table -> Syncing with slave failed"
          << endl;
    return NDBT_FAILED;
  }

  //Create an SQL Object
  DbUtil master(db.c_str(),"");

  //Login to Master
  if (!master.connect())
  {
    return NDBT_FAILED;
  }

  if (master.doQuery("CREATE TABLE rep1 (c1 MEDIUMINT NOT NULL AUTO_INCREMENT,"
                     " c2 FLOAT, c3 CHAR(5), c4 bit(8), c5 FLOAT, c6 INT,"
                     " c7 INT, PRIMARY KEY (c1))ENGINE=NDB"))
  {
    return NDBT_FAILED;
  }
  ctx->setProperty("TABLES",table.c_str());
  HugoTransactions hugoTrans(*ctx->getTab());

  if (hugoTrans.loadTable(GETNDB(step), ctx->getNumRecords(), 1, true, 0) != NDBT_OK)
  {
    g_err << "Create Table -> Load failed!" << endl;
    return NDBT_FAILED;
  }

  if(verifySlaveLoad(&table)!= NDBT_OK)
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
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  while(!ctx->isTestStopped())
  {
    if (hugoTrans.pkUpdateRecords(GETNDB(step), ctx->getNumRecords(), 1, 30) != 0)
    {
      g_err << "pkUpdate Failed!" << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans.scanUpdateRecords(GETNDB(step), ctx->getNumRecords(), 1, 30) != 0)
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

  DbUtil master("TEST_DB","");
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
    if(master.doQuery(sqlStm.c_str()))
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

  sql.assign("SELECT SUM(c3) FROM rep1");
  db.assign("TEST_DB");

  if (verifySlave(sql,db) != NDBT_OK)
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

 verifySlave(BaseString& sql, BaseSting& db) 
 {The SQL statement must sum a column and will verify
  that the sum of the column is equal on master & slave}
*/
                     

NDBT_TESTSUITE(NdbRepStress);
TESTCASE("PHASE_I_Stress","Basic Replication Stressing") 
{
  INITIALIZER(createDB);
  INITIALIZER(createTable_rep1);
  STEP(stressNDB_rep1);
  STEP(stressSQL_rep1);
  FINALIZER(verifySlave_rep1);
  FINALIZER(dropTEST_DB);
}
NDBT_TESTSUITE_END(NdbRepStress);

int main(int argc, const char** argv){
  ndb_init();
  NdbRepStress.setCreateAllTables(true);
  return NdbRepStress.execute(argc, argv);
}

