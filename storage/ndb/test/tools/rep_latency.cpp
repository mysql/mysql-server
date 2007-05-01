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

/* 
 * Update on master wait for update on slave
 *
 */

#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <sys/time.h>
#include <NdbOut.hpp>
#include <NDBT.hpp>

struct Xxx
{
  Ndb *ndb;
  const NdbDictionary::Table *table;
  Uint32 pk_col;
  Uint32 col;
};

struct XxxR
{
  Uint32 pk_val;
  Uint32 val;
  struct timeval start_time;
  Uint32 latency;
};

static int
prepare_master_or_slave(Ndb &myNdb,
                        const char* table,
                        const char* pk,
                        Uint32 pk_val,
                        const char* col,
                        struct Xxx &xxx,
                        struct XxxR &xxxr);
static void
run_master_update(struct Xxx &xxx, struct XxxR &xxxr);
static void
run_slave_wait(struct Xxx &xxx, struct XxxR &xxxr);

#define PRINT_ERROR(code,msg) \
  g_err << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << ".\n"
#define APIERROR(error) { \
  PRINT_ERROR((error).code, (error).message); \
  exit(-1); }

int main(int argc, char** argv)
{
  if (argc != 8)
  {
    ndbout << "Arguments are <connect_string cluster 1> <connect_string cluster 2> <database> <table name> <primary key> <value of primary key> <attribute to update>.\n";
    exit(-1);
  }
  // ndb_init must be called first
  ndb_init();
  {
    const char *opt_connectstring1 = argv[1];
    const char *opt_connectstring2 = argv[2];
    const char *opt_db             = argv[3];
    const char *opt_table          = argv[4];
    const char *opt_pk             = argv[5];
    const Uint32 opt_pk_val        = atoi(argv[6]);
    const char *opt_col            = argv[7];
    
    // Object representing the cluster 1
    Ndb_cluster_connection cluster1_connection(opt_connectstring1);
    // Object representing the cluster 2
    Ndb_cluster_connection cluster2_connection(opt_connectstring2);
    
    // connect cluster 1 and run application
    // Connect to cluster 1  management server (ndb_mgmd)
    if (cluster1_connection.connect(4 /* retries               */,
				    5 /* delay between retries */,
				    1 /* verbose               */))
    {
      g_err << "Cluster 1 management server was not ready within 30 secs.\n";
      exit(-1);
    }
    // Optionally connect and wait for the storage nodes (ndbd's)
    if (cluster1_connection.wait_until_ready(30,0) < 0)
    {
      g_err << "Cluster 1 was not ready within 30 secs.\n";
      exit(-1);
    }
    // connect cluster 2 and run application
    // Connect to cluster management server (ndb_mgmd)
    if (cluster2_connection.connect(4 /* retries               */,
				    5 /* delay between retries */,
				    1 /* verbose               */))
    {
      g_err << "Cluster 2 management server was not ready within 30 secs.\n";
      exit(-1);
    }
    // Optionally connect and wait for the storage nodes (ndbd's)
    if (cluster2_connection.wait_until_ready(30,0) < 0)
    {
      g_err << "Cluster 2 was not ready within 30 secs.\n";
      exit(-1);
    }
    // Object representing the database
    Ndb myNdb1(&cluster1_connection, opt_db);
    Ndb myNdb2(&cluster2_connection, opt_db);
    //
    struct Xxx xxx1;
    struct Xxx xxx2;
    struct XxxR xxxr;
    prepare_master_or_slave(myNdb1, opt_table, opt_pk, opt_pk_val, opt_col,
                            xxx1, xxxr);
    prepare_master_or_slave(myNdb2, opt_table, opt_pk, opt_pk_val, opt_col,
                            xxx2, xxxr);
    while (1)
    {
      // run the application code
      run_master_update(xxx1, xxxr);
      run_slave_wait(xxx2, xxxr);
      ndbout << "latency: " << xxxr.latency << endl;
    }
  }
  // Note: all connections must have been destroyed before calling ndb_end()
  ndb_end(0);

  return 0;
}

static int
prepare_master_or_slave(Ndb &myNdb,
                        const char* table,
                        const char* pk,
                        Uint32 pk_val,
                        const char* col,
                        struct Xxx &xxx,
                        struct XxxR &xxxr)
{
  if (myNdb.init())
    APIERROR(myNdb.getNdbError());
  const NdbDictionary::Dictionary* myDict = myNdb.getDictionary();
  const NdbDictionary::Table *myTable = myDict->getTable(table);
  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());
  const NdbDictionary::Column *myPkCol = myTable->getColumn(pk);
  if (myPkCol == NULL)
    APIERROR(myDict->getNdbError());
  if (myPkCol->getType() != NdbDictionary::Column::Unsigned)
  {
    PRINT_ERROR(0, "Primary key column not of type unsigned");
    exit(-1);
  }
  const NdbDictionary::Column *myCol = myTable->getColumn(col);
  if (myCol == NULL)
    APIERROR(myDict->getNdbError());
  if (myCol->getType() != NdbDictionary::Column::Unsigned)
  {
    PRINT_ERROR(0, "Update column not of type unsigned");
    exit(-1);
  }

  xxx.ndb = &myNdb;
  xxx.table = myTable;
  xxx.pk_col = myPkCol->getColumnNo();
  xxx.col = myCol->getColumnNo();

  xxxr.pk_val = pk_val;

  return 0;
}

static void run_master_update(struct Xxx &xxx, struct XxxR &xxxr)
{
  Ndb *ndb = xxx.ndb;
  const NdbDictionary::Table *myTable = xxx.table;
  int retry_sleep= 10; /* 10 milliseconds */
  int retries= 100;
  while (1)
  {
    Uint32 val;
    NdbTransaction *trans = ndb->startTransaction();
    if (trans == NULL)
      goto err;
    {
      NdbOperation *op = trans->getNdbOperation(myTable);
      if (op == NULL)
        APIERROR(trans->getNdbError());
      op->readTupleExclusive();
      op->equal(xxx.pk_col, xxxr.pk_val);
      op->getValue(xxx.col, (char *)&val);
    }
    if (trans->execute(NdbTransaction::NoCommit))
      goto err;
    //fprintf(stderr, "read %u\n", val);
    xxxr.val = val + 1;
    {
      NdbOperation *op = trans->getNdbOperation(myTable);
      if (op == NULL)
        APIERROR(trans->getNdbError());
      op->updateTuple();
      op->equal(xxx.pk_col, xxxr.pk_val);
      op->setValue(xxx.col, xxxr.val);
    }
    if (trans->execute(NdbTransaction::Commit))
      goto err;
    ndb->closeTransaction(trans);
    //fprintf(stderr, "updated to %u\n", xxxr.val);
    break;
err:
    const NdbError this_error= trans ?
      trans->getNdbError() : ndb->getNdbError();
    if (this_error.status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        if (trans)
          ndb->closeTransaction(trans);
        NdbSleep_MilliSleep(retry_sleep);
        continue; // retry
      }
    }
    if (trans)
      ndb->closeTransaction(trans);
    APIERROR(this_error);
  }
  /* update done start timer */
  gettimeofday(&xxxr.start_time, 0);
}

static void run_slave_wait(struct Xxx &xxx, struct XxxR &xxxr)
{
  struct timeval old_end_time = xxxr.start_time, end_time;
  Ndb *ndb = xxx.ndb;
  const NdbDictionary::Table *myTable = xxx.table;
  int retry_sleep= 10; /* 10 milliseconds */
  int retries= 100;
  while (1)
  {
    Uint32 val;
    NdbTransaction *trans = ndb->startTransaction();
    if (trans == NULL)
      goto err;
    {
      NdbOperation *op = trans->getNdbOperation(myTable);
      if (op == NULL)
        APIERROR(trans->getNdbError());
      op->readTuple();
      op->equal(xxx.pk_col, xxxr.pk_val);
      op->getValue(xxx.col, (char *)&val);
      if (trans->execute(NdbTransaction::Commit))
        goto err;
    }
    /* read done, check time of read */
    gettimeofday(&end_time, 0);
    ndb->closeTransaction(trans);
    //fprintf(stderr, "read %u waiting for %u\n", val, xxxr.val);
    if (xxxr.val != val)
    {
      /* expected value not received yet */
      retries = 100;
      NdbSleep_MilliSleep(retry_sleep);
      old_end_time =  end_time;
      continue;
    }
    break;
err:
    const NdbError this_error= trans ?
      trans->getNdbError() : ndb->getNdbError();
    if (this_error.status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        if (trans)
          ndb->closeTransaction(trans);
        NdbSleep_MilliSleep(retry_sleep);
        continue; // retry
      }
    }
    if (trans)
      ndb->closeTransaction(trans);
    APIERROR(this_error);
  }

  Int64 elapsed_usec1 =
    ((Int64)end_time.tv_sec - (Int64)xxxr.start_time.tv_sec)*1000*1000 +
    ((Int64)end_time.tv_usec - (Int64)xxxr.start_time.tv_usec);
  Int64 elapsed_usec2 =
    ((Int64)end_time.tv_sec - (Int64)old_end_time.tv_sec)*1000*1000 +
    ((Int64)end_time.tv_usec - (Int64)old_end_time.tv_usec);
  xxxr.latency =
    ((elapsed_usec1 - elapsed_usec2/2)+999)/1000;
}
