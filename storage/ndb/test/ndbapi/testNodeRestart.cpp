/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <BlockNumbers.h>
#include <NdbEnv.h>
#include <NdbHost.h>
#include <NdbSleep.h>
#include <ndb_rand.h>
#include <Bitmask.hpp>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbConfig.hpp>
#include <NdbMgmd.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <RefConvert.hpp>
#include <UtilTransactions.hpp>
#include <Vector.hpp>
#include <cstring>
#include <signaldata/DumpStateOrd.hpp>
#include "../../src/ndbapi/NdbInfo.hpp"
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "util/require.h"

static int changeStartPartitionedTimeout(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_FAILED;
  Config conf;
  NdbRestarter restarter;
  Uint32 startPartitionedTimeout =
      ctx->getProperty("STARTPARTITIONTIMEOUT", (Uint32)60000);
  Uint32 defaultValue = Uint32(~0);

  do {
    NdbMgmd mgmd;
    mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
    if (!mgmd.connect()) {
      g_err << "Failed to connect to ndb_mgmd." << endl;
      break;
    }
    if (!mgmd.get_config(conf)) {
      g_err << "Failed to get config from ndb_mgmd." << endl;
      break;
    }
    g_err << "Setting StartPartitionedTimeout to " << startPartitionedTimeout
          << endl;
    ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
    for (int idx = 0; iter.openSection(CFG_SECTION_NODE, idx); idx++) {
      Uint32 oldValue = 0;
      if (iter.get(CFG_DB_START_PARTITION_TIMEOUT, oldValue)) {
        if (defaultValue == Uint32(~0)) {
          defaultValue = oldValue;
        } else if (oldValue != defaultValue) {
          g_err << "StartPartitionedTimeout is not consistent across data node"
                   "sections"
                << endl;
          break;
        }
      }
      iter.set(CFG_DB_START_PARTITION_TIMEOUT, startPartitionedTimeout);
      iter.closeSection();
    }
    /* Save old config value */
    ctx->setProperty("STARTPARTITIONTIMEOUT", Uint32(defaultValue));

    if (!mgmd.set_config(conf)) {
      g_err << "Failed to set config in ndb_mgmd." << endl;
      break;
    }
    g_err << "Restarting nodes to apply config change" << endl;
    NdbSleep_SecSleep(3);  // Give MGM server time to restart
    if (restarter.restartAll()) {
      g_err << "Failed to restart nodes." << endl;
      break;
    }
    if (restarter.waitClusterStarted(120) != 0) {
      g_err << "Failed waiting for nodes to start." << endl;
      break;
    }
    g_err << "Nodes restarted with StartPartitionedTimeout = "
          << startPartitionedTimeout << endl;
    result = NDBT_OK;
  } while (0);
  return result;
}

#define CHECK(b, m)                                                   \
  {                                                                   \
    int _xx = b;                                                      \
    if (!(_xx)) {                                                     \
      ndbout << "ERR: " << m << "   "                                 \
             << "File: " << __FILE__ << " (Line: " << __LINE__ << ")" \
             << "- " << _xx << endl;                                  \
      return NDBT_FAILED;                                             \
    }                                                                 \
  }

#define CHECK2(b)                                                         \
  if (!(b)) {                                                             \
    g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    break;                                                                \
  }

int runLoadTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }
  g_err << "Latest GCI = " << hugoTrans.get_high_latest_gci() << endl;
  return NDBT_OK;
}

int runFillTable(NDBT_Context *ctx, NDBT_Step *step) {
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.fillTable(GETNDB(step)) != 0) {
    return NDBT_FAILED;
  }
  g_err << "Latest GCI = " << hugoTrans.get_high_latest_gci() << endl;
  return NDBT_OK;
}

int runInsertUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.loadTable(GETNDB(step), records) != 0) {
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runClearTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();

  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }
  g_err << "Latest GCI = " << utilTrans.get_high_latest_gci() << endl;
  return NDBT_OK;
}

int runClearTableUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  int i = 0;

  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (utilTrans.clearTable(GETNDB(step), records) != 0) {
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records) != 0) {
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runPkReadUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  NdbOperation::LockMode lm = (NdbOperation::LockMode)ctx->getProperty(
      "ReadLockMode", (Uint32)NdbOperation::LM_Read);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    int rows = (rand() % records) + 1;
    int batch = (rand() % rows) + 1;
    if (hugoTrans.pkReadRecords(GETNDB(step), rows, batch, lm) != 0) {
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

static int start_transaction_on_specific_place(
    Vector<HugoOperations *> op_array, Uint32 index, Ndb *pNdb, NodeId node_id,
    Uint32 instance_id) {
  if (op_array[index]->startTransaction(pNdb, node_id, instance_id) !=
      NDBT_OK) {
    return NDBT_FAILED;
  }
  NdbConnection *pCon = op_array[index]->getTransaction();
  Uint32 transNode = pCon->getConnectedNodeId();
  if (transNode == node_id) {
    return NDBT_OK;
  }
  op_array[index]->closeTransaction(pNdb);
  return NDBT_FAILED;
}

static void cleanup_op_array(Vector<HugoOperations *> &op_array, Ndb *pNdb,
                             int num_instances) {
  for (int instance_id = 0; instance_id < num_instances; instance_id++) {
    op_array[instance_id]->closeTransaction(pNdb);
  }
}

/**
 * This test case is about stress testing our TC failover code.
 * We always run this with a special config with 4 data nodes
 * where node 2 has more transaction records than node 1 and
 * node 3. Node 4 has 4 TC instances and has more operation
 * records than node 1 and node 3.
 *
 * So in order to test we fill up all transaction records with
 * small transactions in node 2 and instance 1. This is done
 * by runManyTransactions.
 *
 * We also fill up all operation records in instance 1 through
 * 4. This is done by runLargeTransactions since we execute
 * this by fairly large transactions, few transactions enough to
 * be able to handle all transactions, but too many operations to
 * handle. This will ensure that each TC failover step will make
 * progress.
 *
 * We don't commit the transactions, instead we crash the node
 * 2 and 4 (we do this by a special error insert that crashes
 * node 4 when node 2 fails. This ensures that both the nodes
 * have to handle TC failover in the same failover batch. This
 * is important to ensure that we also test the failed node
 * queue handling in DBTC.
 */
int run_multiTCtakeover(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records, 12) != 0) {
    ndbout << "Failed to load table for multiTC takeover test" << endl;
    return NDBT_FAILED;
  }
  ctx->setProperty("runLargeDone", (Uint32)0);
  ctx->setProperty("restartsDone", (Uint32)0);
  return NDBT_OK;
}

int runLargeTransactions(NDBT_Context *ctx, NDBT_Step *step) {
  int multiop = 50;
  int trans_per_instance = 10;
  int num_instances = 4;
  int op_instances = num_instances * trans_per_instance;
  Vector<HugoOperations *> op_array;
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);

  for (int i = 0; i < op_instances; i++) {
    op_array.push_back(new HugoOperations(*ctx->getTab()));
    if (op_array[i] == NULL) {
      ndbout << "Failed to allocate HugoOperations instance " << i << endl;
      cleanup_op_array(op_array, pNdb, i);
      return NDBT_FAILED;
    }
  }

  for (int instance_id = 1; instance_id <= num_instances; instance_id++) {
    for (int i = 0; i < trans_per_instance; i++) {
      Uint32 index = (instance_id - 1) * trans_per_instance + i;
      if (start_transaction_on_specific_place(op_array, index, pNdb,
                                              4, /* node id */
                                              instance_id) != NDBT_OK) {
        ndbout << "Failed to start transaction, index = " << index << endl;
        cleanup_op_array(op_array, pNdb, op_instances);
        return NDBT_FAILED;
      }
      for (int j = 0; j < multiop; j++) {
        int record_no = records + (index * multiop) + j;
        if (op_array[index]->pkInsertRecord(pNdb, record_no, 1, rand())) {
          ndbout << "Failed to insert record number = " << record_no << endl;
          cleanup_op_array(op_array, pNdb, op_instances);
          return NDBT_FAILED;
        }
      }
      if (op_array[index]->execute_NoCommit(pNdb) != 0) {
        ndbout << "Failed to execute no commit, index = " << index << endl;
        cleanup_op_array(op_array, pNdb, op_instances);
        return NDBT_FAILED;
      }
    }
  }
  /**
   * Wait until all preparations are complete until we restart node 4 that
   * holds those transactions.
   */
  ndbout << "runLargeTransactions prepare done" << endl;
  ctx->setProperty("runLargeDone", (Uint32)1);
  while (ctx->getProperty("restartsDone", (Uint32)0) != 1) {
    ndbout << "Waiting for restarts to complete" << endl;
    NdbSleep_SecSleep(10);
  }
  cleanup_op_array(op_array, pNdb, op_instances);
  return NDBT_OK;
}

int runManyTransactions(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int multi_trans = 400;
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  Vector<HugoOperations *> op_array;

  if (restarter.getNumDbNodes() != 4) {
    ndbout << "Need to have exactly 4 DB nodes for this test" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }

  for (int i = 0; i < multi_trans; i++) {
    op_array.push_back(new HugoOperations(*ctx->getTab()));
    if (op_array[i] == NULL) {
      ndbout << "Failed to allocate HugoOperations instance " << i << endl;
      cleanup_op_array(op_array, pNdb, i);
      return NDBT_FAILED;
    }
  }
  for (int i = 0; i < multi_trans; i++) {
    if (start_transaction_on_specific_place(op_array, i, pNdb, 2, /* node id */
                                            1) != NDBT_OK) {
      ndbout << "Failed to start transaction, i = " << i << endl;
      cleanup_op_array(op_array, pNdb, multi_trans);
      return NDBT_FAILED;
    }
    int record_no = records + (50 * 4 * 10) + i;
    if (op_array[i]->pkInsertRecord(pNdb, record_no, 1, rand())) {
      ndbout << "Failed to insert record no = " << record_no << endl;
      cleanup_op_array(op_array, pNdb, multi_trans);
      return NDBT_FAILED;
    }
    if (op_array[i]->execute_NoCommit(pNdb) != 0) {
      ndbout << "Failed to execute transaction " << i << endl;
      cleanup_op_array(op_array, pNdb, multi_trans);
      return NDBT_FAILED;
    }
  }

  /**
   * Wait until all preparations are complete until we restart node 2 that
   * holds those transactions.
   */
  ndbout << "Run many transactions done" << endl;
  while (ctx->getProperty("runLargeDone", (Uint32)0) != 1) {
    NdbSleep_SecSleep(1);
  }
  /**
   * We ensure that node 2 and 4 fail together by inserting
   * error number 941 that fails in PREP_FAILREQ handling
   */
  if (restarter.insertErrorInNode(4, 941)) {
    ndbout << "Failed to insert error 941" << endl;
    result = NDBT_FAILED;
    goto end;
  }
  ndbout << "Restart node "
         << "2" << endl;
  if (restarter.restartOneDbNode(2, false, false, true) != 0) {
    g_err << "Failed to restart Node 2" << endl;
    result = NDBT_FAILED;
    goto end;
  }
  ndbout << "Wait for node 2 and 4 to restart" << endl;
  if (restarter.waitClusterStarted() != 0) {
    g_err << "Cluster failed to start" << endl;
    result = NDBT_FAILED;
    goto end;
  }
  CHK_NDB_READY(pNdb);
  ndbout << "Cluster restarted" << endl;
end:
  ctx->setProperty("restartsDone", (Uint32)1);
  cleanup_op_array(op_array, pNdb, multi_trans);
  return result;
}

int runPkUpdateUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int multiop = ctx->getProperty("MULTI_OP", 1);
  Ndb *pNdb = GETNDB(step);
  int i = 0;

  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    int batch = (rand() % records) + 1;
    int row = rand() % records;

    if (batch > 25) batch = 25;

    if (row + batch > records) batch = records - row;

    if (hugoOps.startTransaction(pNdb) != 0) goto err;

    if (hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0) goto err;

    for (int j = 1; j < multiop; j++) {
      if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

      if (hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0) goto err;
    }

    if (hugoOps.execute_Commit(pNdb) != 0) goto err;

    hugoOps.closeTransaction(pNdb);

    continue;

  err:
    NdbConnection *pCon = hugoOps.getTransaction();
    if (pCon == 0) continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError) {
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;

    i++;
  }
  return result;
}

int runPkReadPkUpdateUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i++ << ": ";
    int rows = (rand() % records) + 1;
    int batch = (rand() % rows) + 1;
    int row = (records - rows) ? rand() % (records - rows) : 0;

    int j, k;
    for (j = 0; j < rows; j += batch) {
      k = batch;
      if (j + k > rows) k = rows - j;

      if (hugoOps.startTransaction(pNdb) != 0) goto err;

      if (hugoOps.pkReadRecord(pNdb, row + j, k, NdbOperation::LM_Exclusive) !=
          0)
        goto err;

      if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

      if (hugoOps.pkUpdateRecord(pNdb, row + j, k, rand()) != 0) goto err;

      if (hugoOps.execute_Commit(pNdb) != 0) goto err;

      if (hugoOps.closeTransaction(pNdb) != 0) return NDBT_FAILED;
    }

    continue;
  err:
    NdbConnection *pCon = hugoOps.getTransaction();
    if (pCon == 0) continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError) {
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runPkReadPkUpdatePkUnlockUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i++ << ": ";
    int rows = (rand() % records) + 1;
    int batch = (rand() % rows) + 1;
    int row = (records - rows) ? rand() % (records - rows) : 0;

    int j, k;
    for (j = 0; j < rows; j += batch) {
      k = batch;
      if (j + k > rows) k = rows - j;

      Vector<const NdbLockHandle *> lockHandles;

      if (hugoOps.startTransaction(pNdb) != 0) goto err;

      if (hugoOps.pkReadRecordLockHandle(pNdb, lockHandles, row + j, k,
                                         NdbOperation::LM_Exclusive) != 0)
        goto err;

      if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

      if (hugoOps.pkUpdateRecord(pNdb, row + j, k, rand()) != 0) goto err;

      if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

      if (hugoOps.pkUnlockRecord(pNdb, lockHandles) != 0) goto err;

      if (hugoOps.execute_Commit(pNdb) != 0) goto err;

      if (hugoOps.closeTransaction(pNdb) != 0) return NDBT_FAILED;
    }

    continue;
  err:
    NdbConnection *pCon = hugoOps.getTransaction();
    if (pCon == 0) continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError) {
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runDeleteInsertUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (utilTrans.clearTable(GETNDB(step), records) != 0) {
      result = NDBT_FAILED;
      break;
    }
    if (hugoTrans.loadTable(GETNDB(step), records, 50000) != 0) {
      result = NDBT_FAILED;
      break;
    }
    i++;
  }

  return result;
}

int runScanUpdateUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  int abort = ctx->getProperty("AbortProb", (Uint32)0);
  int check = ctx->getProperty("ScanUpdateNoRowCountCheck", (Uint32)0);
  int retry_max = ctx->getProperty("RetryMax", Uint32(100));

  if (check) records = 0;

  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  ndbout_c("Set RetryMax to %u", retry_max);
  hugoTrans.setRetryMax(retry_max);
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanUpdateRecords(GETNDB(step), records, abort,
                                    parallelism) == NDBT_FAILED) {
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runScanReadVerify(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, 64) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runRestarter(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int sync_threads = ctx->getProperty("SyncThreads", (unsigned)0);
  int sleep0 = ctx->getProperty("Sleep0", (unsigned)0);
  int sleep1 = ctx->getProperty("Sleep1", (unsigned)0);
  int randnode = ctx->getProperty("RandNode", (unsigned)0);
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2) {
    ctx->stopTest();
    return NDBT_OK;
  }

  if (restarter.waitClusterStarted() != 0) {
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  loops *= (restarter.getNumDbNodes() > 2 ? 2 : restarter.getNumDbNodes());
  if (loops < restarter.getNumDbNodes()) loops = restarter.getNumDbNodes();

  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    int id = lastId % restarter.getNumDbNodes();
    if (randnode == 1) {
      id = rand() % restarter.getNumDbNodes();
    }
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl;
    if (restarter.restartOneDbNode(nodeId, false, true, true) != 0) {
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitNodesNoStart(&nodeId, 1)) {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (sleep1) NdbSleep_MilliSleep(sleep1);

    if (restarter.startNodes(&nodeId, 1)) {
      g_err << "Failed to start node" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitClusterStarted() != 0) {
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (sleep0) NdbSleep_MilliSleep(sleep0);

    ctx->sync_up_and_wait("PauseThreads", sync_threads);

    lastId++;
    i++;
  }

  ctx->stopTest();

  return result;
}

int runCheckAllNodesStarted(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  if (restarter.waitClusterStarted(1) != 0) {
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

/* runNamedRestartTest()
   This will call into a test-specific function in NdbRestarts.cpp based on
   the name of the test case.
*/
int runNamedRestartTest(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NDBT_TestCase *pCase = ctx->getCase();
  NdbRestarts restarts;
  int i = 0;
  int timeout = 240;

  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    int safety = 0;
    if (i > 0) safety = 15;

    if (ctx->closeToTimeout(safety)) break;

    if (restarts.executeRestart(ctx, pCase->getName(), timeout, safety) != 0) {
      g_err << "Failed to executeRestart(" << pCase->getName() << ")" << endl;
      result = NDBT_FAILED;
      break;
    }
    i++;
  }
  ctx->stopTest();
  return result;
}

int runDirtyRead(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  int i = 0;
  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    g_info << i << ": ";

    int id = i % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl;
    restarter.insertErrorInNode(nodeId, 5041);
    restarter.insertErrorInAllNodes(8048 + (i & 1));

    for (int j = 0; j < records; j++) {
      if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

      if (hugoOps.pkReadRecord(pNdb, j, 1, NdbOperation::LM_CommittedRead) != 0)
        goto err;

      int res;
      if ((res = hugoOps.execute_Commit(pNdb)) == 4119) goto done;

      if (res != 0) goto err;

      if (hugoOps.closeTransaction(pNdb) != 0) return NDBT_FAILED;
    }
  done:
    if (hugoOps.closeTransaction(pNdb) != 0) return NDBT_FAILED;

    i++;
    restarter.waitClusterStarted(60);
    CHK_NDB_READY(pNdb);
  }
  CHECK(restarter.insertErrorInAllNodes(0) == 0, "Failed to clear insertError");
  return result;
err:
  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

int runLateCommit(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  int i = 0;
  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    g_info << i << ": ";

    if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

    if (hugoOps.pkUpdateRecord(pNdb, 1, 128) != 0) return NDBT_FAILED;

    if (hugoOps.execute_NoCommit(pNdb) != 0) return NDBT_FAILED;

    Uint32 transNode = hugoOps.getTransaction()->getConnectedNodeId();
    int id = i % restarter.getNumDbNodes();
    int nodeId;
    while ((nodeId = restarter.getDbNodeId(id)) == (int)transNode)
      id = (id + 1) % restarter.getNumDbNodes();

    ndbout << "Restart node " << nodeId << endl;

    restarter.restartOneDbNode(nodeId,
                               /** initial */ false,
                               /** nostart */ true,
                               /** abort   */ true);

    restarter.waitNodesNoStart(&nodeId, 1);

    int res;
    if (i & 1)
      res = hugoOps.execute_Commit(pNdb);
    else
      res = hugoOps.execute_Rollback(pNdb);

    ndbout_c("res= %d", res);

    hugoOps.closeTransaction(pNdb);

    restarter.startNodes(&nodeId, 1);
    restarter.waitNodesStarted(&nodeId, 1);

    if (i & 1) {
      if (res != 286) return NDBT_FAILED;
    } else {
      if (res != 0) return NDBT_FAILED;
    }
    i++;
  }

  return NDBT_OK;
}

int runBug15587(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  Uint32 tableId = ctx->getTab()->getTableId();
  int dump[2] = {DumpStateOrd::LqhErrorInsert5042, 0};
  dump[1] = tableId;

  int nodeId = restarter.getDbNodeId(1);

  ndbout << "Restart node " << nodeId << endl;

  if (restarter.restartOneDbNode(nodeId,
                                 /** initial */ false,
                                 /** nostart */ true,
                                 /** abort   */ true))
    return NDBT_FAILED;

  if (restarter.waitNodesNoStart(&nodeId, 1)) return NDBT_FAILED;

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

  if (restarter.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

  if (restarter.dumpStateOneNode(nodeId, dump, 2)) return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1)) return NDBT_FAILED;

  restarter.waitNodesStartPhase(&nodeId, 1, 3);

  if (restarter.waitNodesNoStart(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.dumpStateOneNode(nodeId, val2, 1)) return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.waitNodesStarted(&nodeId, 1)) return NDBT_FAILED;

  ctx->stopTest();
  return NDBT_OK;
}

#define NO_NODE_GROUP int(-1)

static int numNodeGroups;
static int numNoNodeGroups;
static int nodeGroup[MAX_NDB_NODE_GROUPS];
static int nodeGroupIds[MAX_NDB_NODE_GROUPS];

void getNodeGroups(NdbRestarter &restarter) {
  Uint32 nextFreeNodeGroup = 0;

  numNoNodeGroups = 0;
  for (Uint32 i = 0; i < MAX_NDB_NODE_GROUPS; i++) {
    nodeGroup[i] = NO_NODE_GROUP;
    nodeGroupIds[i] = NO_NODE_GROUP;
  }

  int numDbNodes = restarter.getNumDbNodes();
  for (int i = 0; i < numDbNodes; i++) {
    int nodeId = restarter.getDbNodeId(i);
    int nodeGroupId = restarter.getNodeGroup(nodeId);
    ndbout_c("nodeId: %d", nodeId);
    require(nodeId != -1);
    ndbout_c("nodeGroupId: %d", nodeGroupId);
    require(nodeGroupId != -1);
    nodeGroup[nodeId] = nodeGroupId;
    if (nodeGroupId == NDBT_NO_NODE_GROUP_ID) {
      numNoNodeGroups++;
    } else {
      bool found = false;
      for (Uint32 i = 0; i < nextFreeNodeGroup; i++) {
        if (nodeGroupIds[i] == nodeGroupId) {
          found = true;
          break;
        }
      }
      if (!found) {
        nodeGroupIds[nextFreeNodeGroup++] = nodeGroupId;
      }
    }
  }
  numNodeGroups = nextFreeNodeGroup;
}

void crash_nodes_together(NdbRestarter &restarter, int *dead_nodes,
                          int num_dead_nodes) {
  /**
   * This method ensures that all nodes sent in the dead_nodes
   * array will die at the same time. We accomplish this by
   * first inserting ERROR_INSERT code 1006. This code will
   * perform a CRASH_INSERTION if NODE_FAILREP is received
   * when this error insert is set.
   *
   * Next we fail all nodes with a forced graceful shutdown.
   * As soon as one node fails the other nodes will also fail
   * at the same time due to the error insert.
   */
  for (int i = 0; i < num_dead_nodes; i++) {
    int nodeId = dead_nodes[i];
    ndbout_c("Kill node %d", nodeId);
    restarter.insertErrorInNode(nodeId, 1006);
  }
  restarter.restartNodes(dead_nodes, num_dead_nodes,
                         NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_FORCE);
  restarter.waitNodesNoStart(dead_nodes, num_dead_nodes);
}

int getFirstNodeInNodeGroup(NdbRestarter &restarter, int nodeGroupRequested) {
  require(nodeGroupRequested < MAX_NDB_NODE_GROUPS ||
          nodeGroupRequested == NO_NODE_GROUP);
  int numDbNodes = restarter.getNumDbNodes();
  for (int i = 0; i < numDbNodes; i++) {
    int nodeId = restarter.getDbNodeId(i);
    require(nodeId != -1);
    int nodeGroupId = nodeGroup[nodeId];
    require(nodeGroupId != NO_NODE_GROUP);
    if (nodeGroupRequested != NO_NODE_GROUP &&
        nodeGroupId != nodeGroupRequested)
      continue;
    if (nodeGroupId != NDBT_NO_NODE_GROUP_ID) {
      return nodeId;
    }
  }
  require(false);
  return 0;
}

int getNextNodeInNodeGroup(NdbRestarter &restarter, int prev_node_id,
                           int nodeGroupRequested) {
  require(nodeGroupRequested < MAX_NDB_NODE_GROUPS);
  int numDbNodes = restarter.getNumDbNodes();
  require(prev_node_id != 0);
  bool found = false;
  for (int i = 0; i < numDbNodes; i++) {
    int nodeId = restarter.getDbNodeId(i);
    require(nodeId != -1);
    int nodeGroupId = nodeGroup[nodeId];
    require(nodeGroupId != NO_NODE_GROUP);
    if (nodeGroupId == NDBT_NO_NODE_GROUP_ID) continue;
    if (nodeGroupRequested != NO_NODE_GROUP &&
        nodeGroupId != nodeGroupRequested)
      continue;
    if (found) return nodeId;
    if (nodeId == prev_node_id) found = true;
  }
  return 0;
}

void crash_first_node_group(NdbRestarter &restarter, int *dead_nodes,
                            int &num_dead_nodes) {
  num_dead_nodes = 0;
  int node_id = getFirstNodeInNodeGroup(restarter, NO_NODE_GROUP);
  int first_node_group = restarter.getNodeGroup(node_id);
  dead_nodes[num_dead_nodes] = node_id;
  num_dead_nodes++;
  while ((node_id = getNextNodeInNodeGroup(restarter, node_id,
                                           first_node_group)) != 0) {
    dead_nodes[num_dead_nodes] = node_id;
    num_dead_nodes++;
  }
  crash_nodes_together(restarter, dead_nodes, num_dead_nodes);
}

/**
 * Crash one node per node group, index specifies which one
 * to crash in each node. This makes it possible to call this
 * multiple times with different index to ensure that we kill
 * one node per node group at a time until we're out of nodes
 * in the node group(s).
 */
void crash_one_node_per_node_group(NdbRestarter &restarter, int *dead_nodes,
                                   int &num_dead_nodes, int index) {
  int local_dead_nodes[MAX_NDB_NODES];
  int num_local_dead_nodes = 0;

  for (int i = 0; i < numNodeGroups; i++) {
    int node_id = getFirstNodeInNodeGroup(restarter, nodeGroupIds[i]);
    int loop_count = 0;
    do {
      if (index == loop_count) {
        dead_nodes[num_dead_nodes++] = node_id;
        local_dead_nodes[num_local_dead_nodes++] = node_id;
        break;
      }
      node_id = getNextNodeInNodeGroup(restarter, node_id, nodeGroupIds[i]);
      loop_count++;
    } while (1);
  }
  crash_nodes_together(restarter, &local_dead_nodes[0], num_local_dead_nodes);
}

void crash_x_nodes_per_node_group(NdbRestarter &restarter, int *dead_nodes,
                                  int &num_dead_nodes,
                                  int crash_node_count_per_ng) {
  num_dead_nodes = 0;
  for (int i = 0; i < numNodeGroups; i++) {
    int node_id = getFirstNodeInNodeGroup(restarter, nodeGroupIds[i]);
    for (int j = 0; j < crash_node_count_per_ng; j++) {
      dead_nodes[num_dead_nodes++] = node_id;
      node_id = getNextNodeInNodeGroup(restarter, node_id, nodeGroupIds[i]);
    }
  }
  crash_nodes_together(restarter, dead_nodes, num_dead_nodes);
}

void crash_all_except_one_plus_one_nodegroup_untouched(NdbRestarter &restarter,
                                                       int *dead_nodes,
                                                       int &num_dead_nodes,
                                                       int num_replicas) {
  num_dead_nodes = 0;
  int node_group_to_not_crash = 0;
  for (int i = 0; i < numNodeGroups; i++) {
    if (i == node_group_to_not_crash) continue;  // Skip first node group
    int j = 0;
    int node_id = getFirstNodeInNodeGroup(restarter, nodeGroupIds[i]);
    do {
      j++;
      dead_nodes[num_dead_nodes++] = node_id;
      node_id = getNextNodeInNodeGroup(restarter, node_id, nodeGroupIds[i]);
    } while (j < (num_replicas - 1));
  }
  crash_nodes_together(restarter, dead_nodes, num_dead_nodes);
}

void prepare_all_nodes_for_death(NdbRestarter &restarter) {
  int numDbNodes = restarter.getNumDbNodes();
  for (int i = 0; i < numDbNodes; i++) {
    int nodeId = restarter.getDbNodeId(i);
    restarter.insertErrorInNode(nodeId, 944);
  }
}

void set_all_dead(NdbRestarter &restarter, int *dead_nodes,
                  int &num_dead_nodes) {
  int numDbNodes = restarter.getNumDbNodes();
  for (int i = 0; i < numDbNodes; i++) {
    int nodeId = restarter.getDbNodeId(i);
    dead_nodes[i] = nodeId;
  }
  num_dead_nodes = numDbNodes;
}

int runMultiCrashTest(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int numDbNodes = restarter.getNumDbNodes();
  getNodeGroups(restarter);
  int num_replicas = (numDbNodes - numNoNodeGroups) / numNodeGroups;
  int dead_nodes[MAX_NDB_NODES];
  int num_dead_nodes = 0;

  ndbout_c(
      "numDbNodes: %d, numNoNodeGroups: %d, numNodeGroups: %d, num_replicas: "
      "%d",
      numDbNodes, numNoNodeGroups, numNodeGroups, num_replicas);

  Uint32 expect_0 = (numDbNodes - numNoNodeGroups) % numNodeGroups;
  require(expect_0 == 0);
  require(num_replicas > 0);
  require(num_replicas <= 4);

  /**
   * We start by verifying that we never survive a complete node
   * group failure.
   */
  ndbout_c("Crash first node group");
  prepare_all_nodes_for_death(restarter);
  crash_first_node_group(restarter, dead_nodes, num_dead_nodes);
  set_all_dead(restarter, dead_nodes, num_dead_nodes);
  if (!restarter.checkClusterState(dead_nodes, num_dead_nodes)) {
    return NDBT_FAILED;
  }
  if (restarter.startAll() != 0) return NDBT_FAILED;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  num_dead_nodes = 0;
  if (num_replicas == 1) return NDBT_OK;
  /**
   * With 2 replicas we expect to survive all types of crashes
   * that don't crash two nodes in the same node group.
   *
   * We test the obvious case of surviving one node failure in
   * each node group.
   *
   * Next we verify that crashing one more node per node group
   * crashes the entire cluster.
   *
   * With 3 replicas we expect to survive all crashes with
   * at most 1 crash per node group. We also expect to
   * survive all crashes of at most 2 crashes per node
   * group AND one node group with no crashes. We also
   * expect to survive crashes where half of the nodes
   * crash.
   *
   * With 4 nodes we also expect to survive 2 crashes
   * in a node group.
   *
   * We start by verifying that we survive one node at
   * at a time per node group to crash until we shut
   * down the 3rd replica in each node group when we
   * expect a complete failure.
   *
   * Next we verify that we don't survive a failure of
   * 2 replicas in each node group if there are 3
   * replicas, for 4 replicas we expect to survive.
   *
   * Finally we verify that we can survive a failure of
   * all replicas except one when the first node group
   * survives completely.
   */

  num_dead_nodes = 0;
  for (int i = 1; i <= num_replicas; i++) {
    ndbout_c("Crash one node per group, index: %d", i - 1);
    if (i == num_replicas) {
      prepare_all_nodes_for_death(restarter);
    }
    crash_one_node_per_node_group(restarter, dead_nodes, num_dead_nodes, i - 1);
    if (i == num_replicas) {
      set_all_dead(restarter, dead_nodes, num_dead_nodes);
    }
    if (!restarter.checkClusterState(dead_nodes, num_dead_nodes)) {
      return NDBT_FAILED;
    }
    NdbSleep_SecSleep(2);
  }
  if (restarter.startNodes(dead_nodes, num_dead_nodes) != 0) return NDBT_FAILED;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  if (num_replicas == 2) return NDBT_OK;

  ndbout_c("Crash two nodes per node group");
  if (num_replicas == 3) {
    prepare_all_nodes_for_death(restarter);
  }
  crash_x_nodes_per_node_group(restarter, dead_nodes, num_dead_nodes, 2);
  if (num_replicas == 3) {
    set_all_dead(restarter, dead_nodes, num_dead_nodes);
  }
  if (!restarter.checkClusterState(dead_nodes, num_dead_nodes)) {
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(3);
  if (restarter.startNodes(dead_nodes, num_dead_nodes) != 0) return NDBT_FAILED;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  if (num_replicas == 4) {
    ndbout_c("Crash three nodes per node group");
    prepare_all_nodes_for_death(restarter);
    crash_x_nodes_per_node_group(restarter, dead_nodes, num_dead_nodes, 3);
    set_all_dead(restarter, dead_nodes, num_dead_nodes);
    if (!restarter.checkClusterState(dead_nodes, num_dead_nodes)) {
      return NDBT_FAILED;
    }
    if (restarter.startNodes(dead_nodes, num_dead_nodes) != 0)
      return NDBT_FAILED;
    if (restarter.waitClusterStarted()) return NDBT_FAILED;
  }

  if (numNodeGroups == 1) return NDBT_OK;

  ndbout_c(
      "Crash all except one per node group except one node group untouched");
  crash_all_except_one_plus_one_nodegroup_untouched(
      restarter, dead_nodes, num_dead_nodes, num_replicas);

  if (!restarter.checkClusterState(dead_nodes, num_dead_nodes)) {
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(3);
  if (restarter.startNodes(dead_nodes, num_dead_nodes) != 0) return NDBT_FAILED;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;
  return NDBT_OK;
}

int run_suma_handover_test(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int numDbNodes = restarter.getNumDbNodes();
  getNodeGroups(restarter);
  int num_replicas = (numDbNodes - numNoNodeGroups) / numNodeGroups;
  if (num_replicas < 3) {
    return NDBT_OK;
  }
  int restart_node_id = getFirstNodeInNodeGroup(restarter, 0);
  int delay_node_id = getNextNodeInNodeGroup(restarter, restart_node_id, 0);
  if (restarter.insertErrorInNode(delay_node_id, 13054)) return NDBT_FAILED;
  if (restarter.restartOneDbNode(restart_node_id,
                                 /** initial */ false,
                                 /** nostart */ false,
                                 /** abort   */ false))
    return NDBT_FAILED;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;
  if (restarter.insertErrorInNode(delay_node_id, 0)) return NDBT_FAILED;
  return NDBT_OK;
}

int run_suma_handover_with_node_failure(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int numDbNodes = restarter.getNumDbNodes();
  getNodeGroups(restarter);
  int num_replicas = (numDbNodes - numNoNodeGroups) / numNodeGroups;
  if (num_replicas < 3) {
    return NDBT_OK;
  }
  int restart_node = getFirstNodeInNodeGroup(restarter, 0);
  int takeover_node = getNextNodeInNodeGroup(restarter, restart_node, 0);

  // restart_node is shutdown and starts handing over buckets to takeover_node
  // crash another node after starting takeover to interleave node-failure
  // handling with shutdown takeover
  if (restarter.insertErrorInNode(takeover_node, 13056)) return NDBT_FAILED;

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (restarter.dumpStateAllNodes(val2, 2)) return NDBT_FAILED;

  if (restarter.restartOneDbNode(restart_node,
                                 /** initial */ false,
                                 /** nostart */ true,
                                 /** abort   */ false))
    return NDBT_FAILED;

  if (restarter.startAll()) return NDBT_FAILED;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;
  if (restarter.insertErrorInNode(takeover_node, 0)) return NDBT_FAILED;
  return NDBT_OK;
}

int runBug15632(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  int nodeId = restarter.getDbNodeId(1);

  ndbout << "Restart node " << nodeId << endl;

  if (restarter.restartOneDbNode(nodeId,
                                 /** initial */ false,
                                 /** nostart */ true,
                                 /** abort   */ true))
    return NDBT_FAILED;

  if (restarter.waitNodesNoStart(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.insertErrorInNode(nodeId, 7165)) return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.waitNodesStarted(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.restartOneDbNode(nodeId,
                                 /** initial */ false,
                                 /** nostart */ true,
                                 /** abort   */ true))
    return NDBT_FAILED;

  if (restarter.waitNodesNoStart(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.insertErrorInNode(nodeId, 7171)) return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1)) return NDBT_FAILED;

  if (restarter.waitNodesStarted(&nodeId, 1)) return NDBT_FAILED;

  ctx->stopTest();
  return NDBT_OK;
}

int runBug15685(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  NdbRestarter restarter;

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), 10) != 0) {
    return NDBT_FAILED;
  }

  if (hugoOps.startTransaction(pNdb) != 0) goto err;

  if (hugoOps.pkUpdateRecord(pNdb, 0, 1, rand()) != 0) goto err;

  if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

  if (restarter.insertErrorInAllNodes(5100)) return NDBT_FAILED;

  hugoOps.execute_Rollback(pNdb);

  if (restarter.waitClusterStarted() != 0) goto err;

  if (restarter.insertErrorInAllNodes(0)) return NDBT_FAILED;

  ctx->stopTest();
  return NDBT_OK;

err:
  ctx->stopTest();
  return NDBT_FAILED;
}

int runBug16772(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int aliveNodeId = restarter.getRandomNotMasterNodeId(rand());
  int deadNodeId = aliveNodeId;
  while (deadNodeId == aliveNodeId)
    deadNodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());

  // Suppress NDB_FAILCONF; simulates that it arrives late,
  // or out of order, relative to node restart.
  if (restarter.insertErrorInNode(aliveNodeId, 930)) return NDBT_FAILED;

  ndbout << "Restart node " << deadNodeId << endl;

  if (restarter.restartOneDbNode(deadNodeId,
                                 /** initial       */ false,
                                 /** nostart       */ true,
                                 /** abort         */ true,
                                 /** force         */ false,
                                 /** capture error */ true) == 0) {
    g_err << "Restart of node " << deadNodeId << " succeeded when it should "
          << "have failed";
    return NDBT_FAILED;
  }

  // It should now be hanging since we throw away NDB_FAILCONF
  const int ret = restarter.waitNodesNoStart(&deadNodeId, 1);

  // So this should fail...i.e node should not restart (yet)
  if (ret) {
    // Now send a NDB_FAILCONF for deadNo
    int dump[] = {7020, 323, 252, 0};
    dump[3] = deadNodeId;
    if (restarter.dumpStateOneNode(aliveNodeId, dump, 4)) return NDBT_FAILED;

    // Got (the delayed) NDB_NODECONF, and should now start.
    if (restarter.waitNodesNoStart(&deadNodeId, 1)) return NDBT_FAILED;
  }

  if (restarter.startNodes(&deadNodeId, 1)) return NDBT_FAILED;

  if (restarter.waitNodesStarted(&deadNodeId, 1)) return NDBT_FAILED;

  return ret ? NDBT_OK : NDBT_FAILED;
}

int runBug18414(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  int loop = 0;
  do {
    if (hugoOps.startTransaction(pNdb) != 0) goto err;

    if (hugoOps.pkUpdateRecord(pNdb, 0, 128, rand()) != 0) goto err;

    if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

    int node1 = hugoOps.getTransaction()->getConnectedNodeId();
    int node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());

    if (node1 == -1 || node2 == -1) break;

    if (loop & 1) {
      if (restarter.insertErrorInNode(node1, 8080)) goto err;
    }

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (restarter.dumpStateOneNode(node2, val2, 2)) goto err;

    if (restarter.insertErrorInNode(node2, 5003)) goto err;

    /** int res= */ hugoOps.execute_Rollback(pNdb);

    if (restarter.waitNodesNoStart(&node2, 1) != 0) goto err;

    if (restarter.insertErrorInAllNodes(0)) goto err;

    if (restarter.startNodes(&node2, 1) != 0) goto err;

    if (restarter.waitClusterStarted() != 0) goto err;
    CHK_NDB_READY(pNdb);
    if (hugoTrans.scanUpdateRecords(pNdb, 128) != 0) goto err;

    hugoOps.closeTransaction(pNdb);

  } while (++loop < 5);

  return NDBT_OK;

err:
  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

int runBug18612(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  if (restarter.getMaxConcurrentNodeFailures() < 1) {
    g_err << "[SKIPPED] Configuration cannot handle 1 node failure." << endl;
    return NDBT_SKIPPED;
  }

  Uint32 cnt = restarter.getNumDbNodes();

  for (int loop = 0; loop < ctx->getNumLoops(); loop++) {
    int partition0[256];
    int partition1[256];
    std::memset(partition0, 0, sizeof(partition0));
    std::memset(partition1, 0, sizeof(partition1));
    Bitmask<4> nodesmask;

    Uint32 node1 = restarter.getDbNodeId(rand() % cnt);
    for (Uint32 i = 0; i < cnt / 2; i++) {
      do {
        node1 = restarter.getRandomNodePreferOtherNodeGroup(node1, rand());
      } while (nodesmask.get(node1));

      partition0[i] = node1;
      partition1[i] = restarter.getRandomNodeSameNodeGroup(node1, rand());

      ndbout_c("nodes %d %d", node1, partition1[i]);

      require(!nodesmask.get(node1));
      require(!nodesmask.get(partition1[i]));
      nodesmask.set(node1);
      nodesmask.set(partition1[i]);
    }

    ndbout_c("done");

    int dump[255];
    dump[0] = DumpStateOrd::NdbcntrStopNodes;
    memcpy(dump + 1, partition0, sizeof(int) * cnt / 2);

    Uint32 master = restarter.getMasterNodeId();

    if (restarter.dumpStateOneNode(master, dump, 1 + cnt / 2))
      return NDBT_FAILED;

    if (restarter.waitNodesNoStart(partition0, cnt / 2)) return NDBT_FAILED;

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    if (restarter.dumpStateAllNodes(val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInAllNodes(932)) return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition0, sizeof(int) * cnt / 2);
    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.dumpStateOneNode(partition1[i], dump, 1 + cnt / 2))
        return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition1, sizeof(int) * cnt / 2);
    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.dumpStateOneNode(partition0[i], dump, 1 + cnt / 2))
        return NDBT_FAILED;

    if (restarter.startNodes(partition0, cnt / 2)) return NDBT_FAILED;

    if (restarter.waitNodesStartPhase(partition0, cnt / 2, 2))
      return NDBT_FAILED;

    dump[0] = 9001;
    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.dumpStateAllNodes(dump, 2)) return NDBT_FAILED;

    if (restarter.waitNodesNoStart(partition0, cnt / 2)) return NDBT_FAILED;

    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.restartOneDbNode(partition0[i], true, true, true))
        return NDBT_FAILED;

    if (restarter.waitNodesNoStart(partition0, cnt / 2)) return NDBT_FAILED;

    if (restarter.startAll()) return NDBT_FAILED;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runBug18612SR(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  return NDBT_SKIPPED; /* Until we fix handling of partitioned clusters */

  if (restarter.getNumReplicas() < 2) {
    g_err << "[SKIPPED] Test requires 2 or more replicas." << endl;
    return NDBT_SKIPPED;
  }
  if (restarter.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }
  Uint32 cnt = restarter.getNumDbNodes();

  for (int loop = 0; loop < ctx->getNumLoops(); loop++) {
    int partition0[256];
    int partition1[256];
    std::memset(partition0, 0, sizeof(partition0));
    std::memset(partition1, 0, sizeof(partition1));
    Bitmask<4> nodesmask;

    Uint32 node1 = restarter.getDbNodeId(rand() % cnt);
    for (Uint32 i = 0; i < cnt / 2; i++) {
      do {
        int tmp = restarter.getRandomNodeOtherNodeGroup(node1, rand());
        if (tmp == -1) break;
        node1 = tmp;
      } while (nodesmask.get(node1));

      partition0[i] = node1;
      partition1[i] = restarter.getRandomNodeSameNodeGroup(node1, rand());

      ndbout_c("nodes %d %d", node1, partition1[i]);

      require(!nodesmask.get(node1));
      require(!nodesmask.get(partition1[i]));
      nodesmask.set(node1);
      nodesmask.set(partition1[i]);
    }

    ndbout_c("done");

    g_err << "Restarting all" << endl;
    if (restarter.restartAll(false, true, false)) return NDBT_FAILED;

    int dump[255];
    dump[0] = 9000;
    memcpy(dump + 1, partition0, sizeof(int) * cnt / 2);
    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.dumpStateOneNode(partition1[i], dump, 1 + cnt / 2))
        return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition1, sizeof(int) * cnt / 2);
    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.dumpStateOneNode(partition0[i], dump, 1 + cnt / 2))
        return NDBT_FAILED;

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    g_err << "DumpState all nodes" << endl;
    if (restarter.dumpStateAllNodes(val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInAllNodes(932)) return NDBT_FAILED;

    g_err << "Starting all" << endl;
    if (restarter.startAll()) return NDBT_FAILED;

    g_err << "Waiting for phase 2" << endl;
    if (restarter.waitClusterStartPhase(2, 300)) return NDBT_FAILED;

    g_err << "DumpState all nodes" << endl;
    dump[0] = 9001;
    for (Uint32 i = 0; i < cnt / 2; i++)
      if (restarter.dumpStateAllNodes(dump, 2)) return NDBT_FAILED;

    g_err << "Waiting cluster/nodes no-start" << endl;
    if (restarter.waitClusterNoStart(30) == 0) {
      g_err << "Starting all" << endl;
      if (restarter.startAll()) return NDBT_FAILED;
    } else if (restarter.waitNodesNoStart(partition0, cnt / 2, 10) == 0) {
      g_err << "Clear errors in surviving partition1" << endl;
      if (restarter.insertErrorInNodes(partition1, cnt / 2, 0))
        return NDBT_FAILED;

      g_err << "Starting partition0" << endl;
      if (restarter.startNodes(partition0, cnt / 2)) return NDBT_FAILED;
    } else if (restarter.waitNodesNoStart(partition1, cnt / 2, 10) == 0) {
      g_err << "Clear errors in surviving partition0" << endl;
      if (restarter.insertErrorInNodes(partition0, cnt / 2, 0))
        return NDBT_FAILED;

      g_err << "Starting partition1" << endl;
      if (restarter.startNodes(partition1, cnt / 2)) return NDBT_FAILED;
    } else {
      return NDBT_FAILED;
    }

    g_err << "Waiting for the cluster to start" << endl;
    if (restarter.waitClusterStarted()) return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runBug20185(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  const int masterNode = restarter.getMasterNodeId();

  int dump[] = {7090, 20};
  if (restarter.dumpStateAllNodes(dump, 2)) return NDBT_FAILED;

  NdbSleep_MilliSleep(3000);
  Vector<int> nodes;
  for (int i = 0; i < restarter.getNumDbNodes(); i++)
    nodes.push_back(restarter.getDbNodeId(i));

  if (hugoOps.startTransaction(pNdb, masterNode, 0) != 0) {
    g_err << "ERR: Failed to start transaction at master node " << masterNode
          << endl;
    return NDBT_FAILED;
  }

  if (hugoOps.pkUpdateRecord(pNdb, 1, 1) != 0) return NDBT_FAILED;

  if (hugoOps.execute_NoCommit(pNdb) != 0) return NDBT_FAILED;

  const int node = hugoOps.getTransaction()->getConnectedNodeId();
  if (node != masterNode) {
    g_err << "ERR: Transaction did not end up at master node " << masterNode
          << " but at node " << node << endl;
    return NDBT_FAILED;
  }

  const int nodeId = restarter.getRandomNotMasterNodeId(rand());
  if (nodeId == -1) {
    g_err << "ERR: Could not find any node but master node " << masterNode
          << endl;
    return NDBT_FAILED;
  }

  ndbout_c("7031 to %d", nodeId);
  if (restarter.insertErrorInNode(nodeId, 7031)) return NDBT_FAILED;

  for (Uint32 i = 0; i < nodes.size(); i++) {
    if (nodes[i] != nodeId)
      if (restarter.insertErrorInNode(nodes[i], 7030)) return NDBT_FAILED;
  }

  NdbSleep_MilliSleep(500);

  if (hugoOps.execute_Commit(pNdb) == 0) return NDBT_FAILED;

  NdbSleep_MilliSleep(3000);

  restarter.waitClusterStarted();

  if (restarter.dumpStateAllNodes(dump, 1)) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug24717(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);

  HugoTransactions hugoTrans(*ctx->getTab());

  int dump[] = {9002, 0};
  Uint32 ownNode = refToNode(pNdb->getReference());
  dump[1] = ownNode;

  for (; loops; loops--) {
    int nodeId = restarter.getRandomNotMasterNodeId(rand());
    restarter.restartOneDbNode(nodeId, false, true, true);
    restarter.waitNodesNoStart(&nodeId, 1);

    if (restarter.dumpStateOneNode(nodeId, dump, 2)) return NDBT_FAILED;

    restarter.startNodes(&nodeId, 1);

    do {
      CHK_NDB_READY(pNdb);
      for (Uint32 i = 0; i < 100; i++) {
        hugoTrans.pkReadRecords(pNdb, 100, 1, NdbOperation::LM_CommittedRead);
      }
    } while (restarter.waitClusterStarted(5) != 0);
  }

  return NDBT_OK;
}

int runBug29364(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);

  HugoTransactions hugoTrans(*ctx->getTab());

  if (restarter.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  int dump0[] = {9000, 0};
  int dump1[] = {9001, 0};
  Uint32 ownNode = refToNode(pNdb->getReference());
  dump0[1] = ownNode;

  for (; loops; loops--) {
    int node0 = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    int node1 = restarter.getRandomNodePreferOtherNodeGroup(node0, rand());

    restarter.restartOneDbNode(node0, false, true, true);
    restarter.waitNodesNoStart(&node0, 1);
    restarter.startNodes(&node0, 1);
    restarter.waitClusterStarted();

    restarter.restartOneDbNode(node1, false, true, true);
    restarter.waitNodesNoStart(&node1, 1);
    if (restarter.dumpStateOneNode(node1, dump0, 2)) return NDBT_FAILED;

    restarter.startNodes(&node1, 1);

    do {
      CHK_NDB_READY(pNdb);
      for (Uint32 i = 0; i < 100; i++) {
        hugoTrans.pkReadRecords(pNdb, 100, 1, NdbOperation::LM_CommittedRead);
      }
    } while (restarter.waitClusterStarted(5) != 0);

    if (restarter.dumpStateOneNode(node1, dump1, 1)) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug25364(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int loops = ctx->getNumLoops();

  if (restarter.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

  for (; loops; loops--) {
    int master = restarter.getMasterNodeId();
    int victim = restarter.getRandomNodePreferOtherNodeGroup(master, rand());
    int second = restarter.getRandomNodeSameNodeGroup(victim, rand());

    int dump[] = {935, victim};
    if (restarter.dumpStateOneNode(master, dump, 2)) return NDBT_FAILED;

    if (restarter.dumpStateOneNode(master, val2, 2)) return NDBT_FAILED;

    if (restarter.restartOneDbNode(second, false, true, true))
      return NDBT_FAILED;

    int nodes[2] = {master, second};
    if (restarter.waitNodesNoStart(nodes, 2)) return NDBT_FAILED;

    restarter.startNodes(nodes, 2);

    if (restarter.waitNodesStarted(nodes, 2)) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug21271(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());

  const int masterNode = restarter.getMasterNodeId();
  const int nodeId = restarter.getRandomNodeSameNodeGroup(masterNode, rand());

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (restarter.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

  Uint32 tableId = ctx->getTab()->getTableId();
  int dump[] = {DumpStateOrd::LqhErrorInsert5042, 0, 5044};
  dump[1] = tableId;

  if (restarter.dumpStateOneNode(nodeId, dump, 3)) return NDBT_FAILED;

  restarter.waitNodesNoStart(&nodeId, 1);
  ctx->stopTest();

  restarter.startNodes(&nodeId, 1);

  if (restarter.waitClusterStarted() != 0) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug24543(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (restarter.dumpStateAllNodes(val2, 2)) return NDBT_FAILED;

  int nodes[2];
  nodes[0] = restarter.getMasterNodeId();
  restarter.insertErrorInNode(nodes[0], 934);

  nodes[1] = restarter.getRandomNodeOtherNodeGroup(nodes[0], rand());
  if (nodes[1] == -1) {
    nodes[1] = restarter.getRandomNodeSameNodeGroup(nodes[0], rand());
  }

  restarter.restartOneDbNode(nodes[1], false, true, true);
  if (restarter.waitNodesNoStart(nodes, 2)) return NDBT_FAILED;

  restarter.startNodes(nodes, 2);
  if (restarter.waitNodesStarted(nodes, 2)) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runBug25468(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;

  for (int i = 0; i < loops; i++) {
    int master = restarter.getMasterNodeId();
    int node1 = 0;
    int node2 = 0;
    switch (i % 5) {
      case 0:
        node1 = master;
        node2 = restarter.getRandomNodeSameNodeGroup(master, rand());
        break;
      case 1:
        node1 = restarter.getRandomNodeSameNodeGroup(master, rand());
        node2 = master;
        break;
      case 2:
      case 3:
      case 4:
        node1 = restarter.getRandomNodeOtherNodeGroup(master, rand());
        if (node1 == -1) {
          // only one node group in cluster
          node1 = master;
        }
        node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());
        break;
    }

    ndbout_c("node1: %d node2: %d master: %d", node1, node2, master);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    if (restarter.dumpStateOneNode(node2, val2, 2)) return NDBT_FAILED;

    if (restarter.insertError2InNode(node1, 7178, node2)) return NDBT_FAILED;

    int val1 = 7099;
    if (restarter.dumpStateOneNode(master, &val1, 1)) return NDBT_FAILED;

    if (restarter.waitNodesNoStart(&node2, 1)) return NDBT_FAILED;

    if (restarter.startAll()) return NDBT_FAILED;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug25554(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;

  if (restarter.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  for (int i = 0; i < loops; i++) {
    int master = restarter.getMasterNodeId();
    int node1 = restarter.getRandomNodePreferOtherNodeGroup(master, rand());
    restarter.restartOneDbNode(node1, false, true, true);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    if (restarter.dumpStateOneNode(master, val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInNode(master, 7141)) return NDBT_FAILED;

    if (restarter.waitNodesNoStart(&node1, 1)) return NDBT_FAILED;

    if (restarter.dumpStateOneNode(node1, val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInNode(node1, 932)) return NDBT_FAILED;

    if (restarter.startNodes(&node1, 1)) return NDBT_FAILED;

    int nodes[] = {master, node1};
    if (restarter.waitNodesNoStart(nodes, 2)) return NDBT_FAILED;

    if (restarter.startNodes(nodes, 2)) return NDBT_FAILED;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug25984(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);

  NdbDictionary::Table tab = *ctx->getTab();
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();

  if (restarter.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    return NDBT_SKIPPED;
  }

  pDict->dropTable(tab.getName());

  if (restarter.restartAll(true, true, true)) return NDBT_FAILED;

  if (restarter.waitClusterNoStart()) return NDBT_FAILED;

  if (restarter.startAll()) return NDBT_FAILED;

  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  CHK_NDB_READY(pNdb);

  int res = pDict->createTable(tab);
  if (res) {
    return NDBT_FAILED;
  }
  HugoTransactions trans(*pDict->getTable(tab.getName()));
  trans.loadTable(pNdb, ctx->getNumRecords());

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  int master = restarter.getMasterNodeId();
  int victim = restarter.getRandomNodeOtherNodeGroup(master, rand());
  if (victim == -1)
    victim = restarter.getRandomNodeSameNodeGroup(master, rand());

  restarter.restartOneDbNode(victim, false, true, true);

  for (Uint32 i = 0; i < 10; i++) {
    ndbout_c("Loop: %d", i);
    if (restarter.waitNodesNoStart(&victim, 1)) return NDBT_FAILED;

    if (restarter.dumpStateOneNode(victim, val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInNode(victim, 7191)) return NDBT_FAILED;

    trans.scanUpdateRecords(pNdb, ctx->getNumRecords());

    if (restarter.startNodes(&victim, 1)) return NDBT_FAILED;

    NdbSleep_SecSleep(3);
  }

  if (restarter.waitNodesNoStart(&victim, 1)) return NDBT_FAILED;

  if (restarter.restartAll(false, false, true)) return NDBT_FAILED;

  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  CHK_NDB_READY(pNdb);

  trans.scanUpdateRecords(pNdb, ctx->getNumRecords());

  restarter.restartOneDbNode(victim, false, true, true);
  for (Uint32 i = 0; i < 1; i++) {
    ndbout_c("Loop: %d", i);
    if (restarter.waitNodesNoStart(&victim, 1)) return NDBT_FAILED;

    if (restarter.dumpStateOneNode(victim, val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInNode(victim, 7016)) return NDBT_FAILED;

    trans.scanUpdateRecords(pNdb, ctx->getNumRecords());

    if (restarter.startNodes(&victim, 1)) return NDBT_FAILED;

    NdbSleep_SecSleep(3);
  }

  if (restarter.waitNodesNoStart(&victim, 1)) return NDBT_FAILED;

  if (restarter.startNodes(&victim, 1)) return NDBT_FAILED;

  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug26457(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }
  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();
  while (loops--) {
  retry:
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);

    ndbout_c("master: %d next: %d", master, next);

    if (res.getNodeGroup(master) == res.getNodeGroup(next)) {
      res.restartOneDbNode(next, false, false, true);
      if (res.waitClusterStarted()) return NDBT_FAILED;
      goto retry;
    }

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 2};

    if (res.dumpStateOneNode(next, val2, 2)) return NDBT_FAILED;

    if (res.insertErrorInNode(next, 7180)) return NDBT_FAILED;

    res.restartOneDbNode(master, false, false, true);
    if (res.waitClusterStarted()) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runInitialNodeRestartTest(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (runLoadTable(ctx, step) != NDBT_OK) return NDBT_FAILED;

  {
    int lcpdump = DumpStateOrd::DihMinTimeBetweenLCP;
    res.dumpStateAllNodes(&lcpdump, 1);
  }
  NdbSleep_SecSleep(10);
  int node = res.getRandomNotMasterNodeId(rand());
  ndbout_c("node: %d", node);

  if (res.restartOneDbNode(node, true, true, true)) return NDBT_FAILED;

  if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

  if (res.insertErrorInNode(node, 5091)) return NDBT_FAILED;

  res.startNodes(&node, 1);

  res.waitNodesStartPhase(&node, 1, 3);

  if (res.waitClusterStarted()) return NDBT_FAILED;
  return NDBT_OK;
}

int runBug26481(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  int node = res.getRandomNotMasterNodeId(rand());
  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, true, true, true)) return NDBT_FAILED;

  if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (res.dumpStateOneNode(node, val2, 2)) return NDBT_FAILED;

  if (res.insertErrorInNode(node, 7018)) return NDBT_FAILED;

  if (res.startNodes(&node, 1)) return NDBT_FAILED;

  res.waitNodesStartPhase(&node, 1, 3);

  if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

  res.startNodes(&node, 1);

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug26450(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  Ndb *pNdb = GETNDB(step);

  int node = res.getRandomNotMasterNodeId(rand());
  Vector<int> nodes;
  for (int i = 0; i < res.getNumDbNodes(); i++) {
    if (res.getDbNodeId(i) != node) nodes.push_back(res.getDbNodeId(i));
  }

  if (res.restartAll()) return NDBT_FAILED;

  if (res.waitClusterStarted()) return NDBT_FAILED;

  CHK_NDB_READY(GETNDB(step));

  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, false, true, true)) return NDBT_FAILED;

  if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

  if (runClearTable(ctx, step)) return NDBT_FAILED;

  for (int i = 0; i < 2; i++) {
    if (res.restartAll(false, true, i > 0)) return NDBT_FAILED;

    if (res.waitClusterNoStart()) return NDBT_FAILED;

    if (res.startNodes(nodes.getBase(), nodes.size())) return NDBT_FAILED;

    if (res.waitNodesStarted(nodes.getBase(), nodes.size())) return NDBT_FAILED;
  }

  if (res.startNodes(&node, 1)) return NDBT_FAILED;

  if (res.waitNodesStarted(&node, 1)) return NDBT_FAILED;

  HugoTransactions trans(*ctx->getTab());
  if (trans.selectCount(pNdb) != 0) return NDBT_FAILED;

  return NDBT_OK;
}

int run_test_multi_socket(NDBT_Context *ctx, NDBT_Step *step) {
  static const int errnos[] = {951, 952, 953, 954, 955, 956,
                               957, 958, 959, 960, 0};
  static const int delay_nos[] = {970, 971, 972, 973, 974, 975, 976, 977, 978,
                                  979, 980, 981, 982, 983, 984, 985, 0};
  int nodegroup_nodes[MAX_NDB_NODES];
  NdbRestarter res;
  getNodeGroups(res);
  int node_id = getFirstNodeInNodeGroup(res, NO_NODE_GROUP);
  int first_node_group = res.getNodeGroup(node_id);
  Uint32 index = 0;
  nodegroup_nodes[index++] = node_id;
  ndbout_c("Node group %u used", first_node_group);
  ndbout_c("Node[%u] = %u", index - 1, node_id);
  while ((node_id = getNextNodeInNodeGroup(res, node_id, first_node_group)) !=
         0) {
    nodegroup_nodes[index++] = node_id;
    ndbout_c("Node[%u] = %u", index - 1, node_id);
  }
  if (index < 2) {
    /* Test requires at least 2 replicas */
    return NDBT_SKIPPED;
  }
  int pos = 0;
  int start_index = 1;
  while (errnos[pos] != 0) {
    for (Uint32 i = start_index; i < index; i++) {
      int restart_node = nodegroup_nodes[i];
      ndbout_c("Restart node %u", restart_node);
      if (res.restartOneDbNode(restart_node, true, true, true))
        return NDBT_FAILED;
      ndbout_c("Wait node %u no start", restart_node);
      if (res.waitNodesNoStart(&restart_node, 1)) return NDBT_FAILED;
      ndbout_c("Insert error %u into node %u", errnos[pos], restart_node);
      if (res.insertErrorInNode(restart_node, errnos[pos])) return NDBT_FAILED;
      if (res.insertErrorInNode(restart_node, 1006)) return NDBT_FAILED;
    }
    g_err << "Start nodes, expect crash" << endl;

    res.startNodes(&nodegroup_nodes[start_index], index - 1);
    if (res.waitClusterStarted()) return NDBT_FAILED;
    if (res.insertErrorInAllNodes(0)) return NDBT_FAILED;
    pos++;
  }
  pos = 0;
  while (delay_nos[pos] != 0) {
    for (Uint32 i = start_index; i < index; i++) {
      int restart_node = nodegroup_nodes[i];
      ndbout_c("Restart node %u", restart_node);
      if (res.restartOneDbNode(restart_node, true, true, true))
        return NDBT_FAILED;
      ndbout_c("Wait node %u no start", restart_node);
      if (res.waitNodesNoStart(&restart_node, 1)) return NDBT_FAILED;
      ndbout_c("Insert error %u into node %u", delay_nos[pos], restart_node);
      if (res.insertErrorInNode(restart_node, delay_nos[pos]))
        return NDBT_FAILED;
    }
    g_err << "Start nodes" << endl;

    res.startNodes(&nodegroup_nodes[start_index],
                   index - 1);  // Expect crash
    if (res.waitClusterStarted()) return NDBT_FAILED;
    if (res.insertErrorInAllNodes(0)) return NDBT_FAILED;
    pos++;
  }
  return NDBT_OK;
}

int runBug27003(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  static const int errnos[] = {4025, 4026, 4027, 4028, 0};

  int node = res.getRandomNotMasterNodeId(rand());
  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, true, true, true)) return NDBT_FAILED;

  Uint32 pos = 0;
  for (int i = 0; i < loops; i++) {
    while (errnos[pos] != 0) {
      ndbout_c("Testing err: %d", errnos[pos]);

      if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

      if (res.insertErrorInNode(node, 1000)) return NDBT_FAILED;

      if (res.insertErrorInNode(node, errnos[pos])) return NDBT_FAILED;

      int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 3};
      if (res.dumpStateOneNode(node, val2, 2)) return NDBT_FAILED;

      res.startNodes(&node, 1);
      NdbSleep_SecSleep(3);
      pos++;
    }
    pos = 0;
  }

  if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

  res.startNodes(&node, 1);
  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug27283(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  static const int errnos[] = {7181, 7182, 0};

  Uint32 pos = 0;
  for (Uint32 i = 0; i < (Uint32)loops; i++) {
    while (errnos[pos] != 0) {
      int master = res.getMasterNodeId();
      int next = res.getNextMasterNodeId(master);
      // int next2 = res.getNextMasterNodeId(next);

      // int node = (i & 1) ? next : next2;
      ndbout_c("Testing err: %d", errnos[pos]);
      if (res.insertErrorInNode(next, errnos[pos])) return NDBT_FAILED;

      NdbSleep_SecSleep(3);

      if (res.waitClusterStarted()) return NDBT_FAILED;

      pos++;
    }
    pos = 0;
  }

  return NDBT_OK;
}

int runBug27466(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  for (Uint32 i = 0; i < (Uint32)loops; i++) {
    int node1 = res.getDbNodeId(rand() % res.getNumDbNodes());
    int node2 = node1;
    while (node1 == node2) {
      node2 = res.getDbNodeId(rand() % res.getNumDbNodes());
    }

    ndbout_c("nodes %u %u", node1, node2);

    if (res.restartOneDbNode(node1, false, true, true)) return NDBT_FAILED;

    if (res.waitNodesNoStart(&node1, 1)) return NDBT_FAILED;

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(node1, val2, 2)) return NDBT_FAILED;

    if (res.insertErrorInNode(node2, 8039)) return NDBT_FAILED;

    res.startNodes(&node1, 1);
    NdbSleep_SecSleep(3);
    if (res.waitNodesNoStart(&node1, 1)) return NDBT_FAILED;
    NdbSleep_SecSleep(5);  // Wait for delayed INCL_NODECONF to arrive

    res.startNodes(&node1, 1);
    if (res.waitClusterStarted()) return NDBT_FAILED;
    // Error is consumed only in one DBTC block.
    // Force error to be cleared in all DBTC instances.
    CHECK(res.insertErrorInNode(node2, 0) == 0, "Failed to clear insertError");
  }

  return NDBT_OK;
}

int runBug28023(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(pNdb, records) != 0) {
    return NDBT_FAILED;
  }

  if (hugoTrans.clearTable(pNdb, records) != 0) {
    return NDBT_FAILED;
  }

  for (Uint32 i = 0; i < (Uint32)loops; i++) {
    int node1 = res.getDbNodeId(rand() % res.getNumDbNodes());

    if (res.restartOneDbNode2(
            node1, NdbRestarter::NRRF_ABORT | NdbRestarter::NRRF_NOSTART))
      return NDBT_FAILED;

    if (res.waitNodesNoStart(&node1, 1)) return NDBT_FAILED;

    if (hugoTrans.loadTable(pNdb, records) != 0) {
      return NDBT_FAILED;
    }

    if (hugoTrans.clearTable(pNdb, records) != 0) {
      return NDBT_FAILED;
    }

    res.startNodes(&node1, 1);
    if (res.waitClusterStarted()) return NDBT_FAILED;

    CHK_NDB_READY(pNdb);

    if (hugoTrans.loadTable(pNdb, records) != 0) {
      return NDBT_FAILED;
    }

    if (hugoTrans.scanUpdateRecords(pNdb, records) != 0) return NDBT_FAILED;

    if (hugoTrans.clearTable(pNdb, records) != 0) {
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runBug28717(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    return NDBT_SKIPPED;
  }

  int master = res.getMasterNodeId();
  int node0 = res.getRandomNodePreferOtherNodeGroup(master, rand());
  int node1 = res.getRandomNodeSameNodeGroup(node0, rand());

  ndbout_c("master: %d node0: %d node1: %d", master, node0, node1);

  if (res.restartOneDbNode(node0, false, true, true)) {
    return NDBT_FAILED;
  }

  {
    int filter[] = {15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0};
    NdbLogEventHandle handle =
        ndb_mgm_create_logevent_handle(res.handle, filter);

    int dump[] = {DumpStateOrd::DihStartLcpImmediately};
    struct ndb_logevent event;

    for (Uint32 i = 0; i < 3; i++) {
      res.dumpStateOneNode(master, dump, 1);
      while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
             event.type != NDB_LE_LocalCheckpointStarted)
        ;
      while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
             event.type != NDB_LE_LocalCheckpointCompleted)
        ;
    }
  }

  if (res.waitNodesNoStart(&node0, 1)) return NDBT_FAILED;

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

  if (res.dumpStateOneNode(node0, val2, 2)) return NDBT_FAILED;

  if (res.insertErrorInNode(node0, 5010)) return NDBT_FAILED;

  if (res.insertErrorInNode(node1, 1001)) return NDBT_FAILED;

  if (res.startNodes(&node0, 1)) return NDBT_FAILED;

  NdbSleep_SecSleep(3);

  if (res.insertErrorInNode(node1, 0)) return NDBT_FAILED;

  if (res.waitNodesNoStart(&node0, 1)) return NDBT_FAILED;

  if (res.startNodes(&node0, 1)) return NDBT_FAILED;

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

static int f_master_failure[] = {7000, 7001, 7002, 7003, 7004, 7186,
                                 7187, 7188, 7189, 7190, 0};

static int f_participant_failure[] = {7005, 7006, 7007, 7008, 5000, 7228, 0};

int runerrors(NdbRestarter &res, NdbRestarter::NodeSelector sel,
              const int *errors) {
  for (Uint32 i = 0; errors[i]; i++) {
    int node = res.getNode(sel);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(node, val2, 2)) return NDBT_FAILED;

    ndbout << "node " << node << " err: " << errors[i] << endl;
    if (res.insertErrorInNode(node, errors[i])) return NDBT_FAILED;

    if (res.waitNodesNoStart(&node, 1) != 0) return NDBT_FAILED;

    res.startNodes(&node, 1);

    if (res.waitClusterStarted() != 0) return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runGCP(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  int loops = ctx->getNumLoops();

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  if (res.getNumDbNodes() < 4) {
    /**
     * 7186++ is only usable for 4 nodes and above
     */
    Uint32 i;
    for (i = 0; f_master_failure[i] && f_master_failure[i] != 7186; i++)
      ;
    f_master_failure[i] = 0;
  }

  while (loops >= 0 && !ctx->isTestStopped()) {
    loops--;

#if 0
    if (runerrors(res, NdbRestarter::NS_NON_MASTER, f_participant_failure))
    {
      return NDBT_FAILED;
    }

    if (runerrors(res, NdbRestarter::NS_MASTER, f_participant_failure))
    {
      return NDBT_FAILED;
    }
#endif

    if (runerrors(res, NdbRestarter::NS_RANDOM, f_participant_failure)) {
      return NDBT_FAILED;
    }

    if (runerrors(res, NdbRestarter::NS_MASTER, f_master_failure)) {
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

int runCommitAck(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);

  if (records < 2) return NDBT_SKIPPED;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  int trans_type = -1;
  NdbConnection *pCon;
  int node;
  while (loops--) {
    trans_type++;
    if (trans_type > 2) trans_type = 0;
    HugoTransactions hugoTrans(*ctx->getTab());
    switch (trans_type) {
      case 0:
        /*
          - load records less 1
        */
        g_info << "case 0\n";
        if (hugoTrans.loadTable(GETNDB(step), records - 1)) {
          return NDBT_FAILED;
        }
        break;
      case 1:
        /*
          - load 1 record
        */
        g_info << "case 1\n";
        if (hugoTrans.loadTable(GETNDB(step), 1)) {
          return NDBT_FAILED;
        }
        break;
      case 2:
        /*
          - load 1 record in the end
        */
        g_info << "case 2\n";
        {
          HugoOperations hugoOps(*ctx->getTab());
          if (hugoOps.startTransaction(pNdb)) abort();
          if (hugoOps.pkInsertRecord(pNdb, records - 1)) abort();
          if (hugoOps.execute_Commit(pNdb)) abort();
          if (hugoOps.closeTransaction(pNdb)) abort();
        }
        break;
      default:
        abort();
    }

    /* run transaction that should be tested */
    HugoOperations hugoOps(*ctx->getTab());
    if (hugoOps.startTransaction(pNdb)) return NDBT_FAILED;
    pCon = hugoOps.getTransaction();
    node = pCon->getConnectedNodeId();
    switch (trans_type) {
      case 0:
      case 1:
        /*
          insert records with ignore error
          - insert rows, some exist already
        */
        for (int i = 0; i < records; i++) {
          if (hugoOps.pkInsertRecord(pNdb, i)) goto err;
        }
        break;
      case 2:
        /*
          insert records with ignore error
          - insert rows, some exist already
        */
        for (int i = 0; i < records; i++) {
          if (hugoOps.pkInsertRecord(pNdb, i)) goto err;
        }
        break;
      default:
        abort();
    }

    /*
      insert error in ndb kernel (TC) that throws away acknowledge of commit
      and then die 5 seconds later
    */
    {
      if (restarter.insertErrorInNode(node, 8054)) goto err;
    }
    {
      int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
      if (restarter.dumpStateOneNode(node, val2, 2)) goto err;
    }

    /* execute transaction and verify return code */
    g_info << "  execute... hangs for 5 seconds\n";
    {
      const NdbOperation *first = pCon->getFirstDefinedOperation();
      int check = pCon->execute(Commit, AO_IgnoreError);
      const NdbError err = pCon->getNdbError();

      while (first) {
        const NdbError &err = first->getNdbError();
        g_info << "         error " << err.code << endl;
        first = pCon->getNextCompletedOperation(first);
      }

      int expected_commit_res[3] = {630, 630, 630};
      if (check == -1 || err.code != expected_commit_res[trans_type]) {
        g_err << "check == " << check << endl;
        g_err << "got error: " << err.code
              << " expected: " << expected_commit_res[trans_type] << endl;
        goto err;
      }
    }

    g_info << "  wait node nostart\n";
    if (restarter.waitNodesNoStart(&node, 1)) {
      g_err << "  wait node nostart failed\n";
      goto err;
    }

    /* close transaction */
    if (hugoOps.closeTransaction(pNdb)) return NDBT_FAILED;

    /* commit ack marker pools should be empty */
    g_info << "  dump pool status\n";
    {
      int dump[255];
      dump[0] = 2552;
      if (restarter.dumpStateAllNodes(dump, 1)) return NDBT_FAILED;
    }

    /* wait for cluster to come up again */
    g_info << "  wait cluster started\n";
    if (restarter.startNodes(&node, 1) ||
        restarter.waitNodesStarted(&node, 1)) {
      g_err << "Cluster failed to start\n";
      return NDBT_FAILED;
    }

    /* verify data */
    g_info << "  verifying\n";
    switch (trans_type) {
      case 0:
      case 1:
      case 2:
        /*
          insert records with ignore error
          - should have all records
        */
        if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, 64) != 0) {
          return NDBT_FAILED;
        }
        break;
      default:
        abort();
    }

    /* cleanup for next round in loop */
    g_info << "  cleaning\n";
    if (hugoTrans.clearTable(GETNDB(step), records)) {
      return NDBT_FAILED;
    }
    continue;
  err:
    hugoOps.closeTransaction(pNdb);
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int max_cnt(int arr[], int cnt) {
  int res = 0;

  for (int i = 0; i < cnt; i++) {
    if (arr[i] > res) {
      res = arr[i];
    }
  }
  return res;
}

int runPnr(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter res(0, &ctx->m_cluster_connection);
  bool lcp = ctx->getProperty("LCP", (unsigned)0);

  int nodegroups[MAX_NDB_NODES];
  std::memset(nodegroups, 0, sizeof(nodegroups));

  for (int i = 0; i < res.getNumDbNodes(); i++) {
    int node = res.getDbNodeId(i);
    int ng = res.getNodeGroup(node);
    if (ng != NDBT_NO_NODE_GROUP_ID) {
      nodegroups[ng]++;
    }
  }

  for (int i = 0; i < MAX_NDB_NODES; i++) {
    if (nodegroups[i] && nodegroups[i] == 1) {
      /**
       * nodegroup with only 1 member, can't run test
       */
      ctx->stopTest();
      return NDBT_SKIPPED;
    }
  }

  for (int i = 0; i < loops && ctx->isTestStopped() == false; i++) {
    if (lcp) {
      int lcpdump = DumpStateOrd::DihMinTimeBetweenLCP;
      res.dumpStateAllNodes(&lcpdump, 1);
    }

    int ng_copy[MAX_NDB_NODES];
    memcpy(ng_copy, nodegroups, sizeof(ng_copy));

    Vector<int> nodes;
    printf("restarting ");
    while (max_cnt(ng_copy, MAX_NDB_NODES) > 1) {
      int node = res.getNode(NdbRestarter::NS_RANDOM);
      if (res.getNodeGroup(node) == NDBT_NO_NODE_GROUP_ID) continue;
      bool found = false;
      for (Uint32 i = 0; i < nodes.size(); i++) {
        if (nodes[i] == node) {
          found = true;
          break;
        }
      }
      if (found) continue;
      int ng = res.getNodeGroup(node);
      if (ng_copy[ng] > 1) {
        printf("%u ", node);
        nodes.push_back(node);
        ng_copy[ng]--;
      }
    }
    printf("\n");

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    for (Uint32 j = 0; j < nodes.size(); j++) {
      res.dumpStateOneNode(nodes[j], val2, 2);
    }

    int kill[] = {9999, 0};
    for (Uint32 j = 0; j < nodes.size(); j++) {
      res.dumpStateOneNode(nodes[j], kill, 2);
      if (res.waitNodesNoStart(nodes.getBase(), j + 1)) {
        printf("Failed wait nodes no start\n");
        return NDBT_FAILED;
      }
    }

    if (res.startNodes(nodes.getBase(), nodes.size())) {
      printf("Failed start nodes\n");
      return NDBT_FAILED;
    }

    if (res.waitClusterStarted()) {
      printf("Failed start cluster\n");
      return NDBT_FAILED;
    }
    printf("Success one loop\n");
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runCreateBigTable(NDBT_Context *ctx, NDBT_Step *step) {
  const char *prefix = ctx->getProperty("PREFIX", "");
  NdbDictionary::Table tab = *ctx->getTab();
  BaseString tmp;
  tmp.assfmt("%s_%s", prefix, tab.getName());
  tab.setName(tmp.c_str());

  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();
  int res = pDict->createTable(tab);
  if (res) {
    return NDBT_FAILED;
  }

  const NdbDictionary::Table *pTab = pDict->getTable(tmp.c_str());
  if (pTab == 0) {
    return NDBT_FAILED;
  }

  int bytes = tab.getRowSizeInBytes();
  int size = 50 * 1024 * 1024;  // 50Mb
  int rows = size / bytes;

  if (rows > 1000000) rows = 1000000;

  ndbout_c("Loading %u rows into %s", rows, tmp.c_str());
  Uint64 now = NdbTick_CurrentMillisecond();
  HugoTransactions hugoTrans(*pTab);
  int cnt = 0;
  do {
    hugoTrans.loadTableStartFrom(GETNDB(step), cnt, 10000);
    cnt += 10000;
  } while (cnt < rows &&
           (NdbTick_CurrentMillisecond() - now) < 180000);  // 180s
  ndbout_c("Loaded %u rows in %llums", cnt, NdbTick_CurrentMillisecond() - now);

  return NDBT_OK;
}

int runDropBigTable(NDBT_Context *ctx, NDBT_Step *step) {
  const char *prefix = ctx->getProperty("PREFIX", "");
  NdbDictionary::Table tab = *ctx->getTab();
  BaseString tmp;
  tmp.assfmt("%s_%s", prefix, tab.getName());
  GETNDB(step)->getDictionary()->dropTable(tmp.c_str());
  return NDBT_OK;
}

int runBug31525(NDBT_Context *ctx, NDBT_Step *step) {
  // int result = NDBT_OK;
  // int loops = ctx->getNumLoops();
  // int records = ctx->getNumRecords();
  // Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  int nodes[2];
  nodes[0] = res.getMasterNodeId();
  nodes[1] = res.getNextMasterNodeId(nodes[0]);

  while (res.getNodeGroup(nodes[0]) != res.getNodeGroup(nodes[1])) {
    ndbout_c("Restarting %u as it not in same node group as %u", nodes[1],
             nodes[0]);
    if (res.restartOneDbNode(nodes[1], false, true, true)) return NDBT_FAILED;

    if (res.waitNodesNoStart(nodes + 1, 1)) return NDBT_FAILED;

    if (res.startNodes(nodes + 1, 1)) return NDBT_FAILED;

    if (res.waitClusterStarted()) return NDBT_FAILED;

    nodes[1] = res.getNextMasterNodeId(nodes[0]);
  }

  ndbout_c("nodes[0]: %u nodes[1]: %u", nodes[0], nodes[1]);

  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if (res.dumpStateAllNodes(&val, 1)) return NDBT_FAILED;

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (res.dumpStateAllNodes(val2, 2)) return NDBT_FAILED;

  if (res.insertErrorInAllNodes(932)) return NDBT_FAILED;

  if (res.insertErrorInNode(nodes[1], 7192)) return NDBT_FAILED;

  if (res.insertErrorInNode(nodes[0], 7191)) return NDBT_FAILED;

  if (res.waitClusterNoStart()) return NDBT_FAILED;

  if (res.startAll()) return NDBT_FAILED;

  if (res.waitClusterStarted()) return NDBT_FAILED;

  if (res.restartOneDbNode(nodes[1], false, false, true)) return NDBT_FAILED;

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug31980(NDBT_Context *ctx, NDBT_Step *step) {
  // int result = NDBT_OK;
  // int loops = ctx->getNumLoops();
  // int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  HugoOperations hugoOps(*ctx->getTab());
  if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

  if (hugoOps.pkInsertRecord(pNdb, 1) != 0) return NDBT_FAILED;

  if (hugoOps.execute_NoCommit(pNdb) != 0) return NDBT_FAILED;

  int transNode = hugoOps.getTransaction()->getConnectedNodeId();
  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

  if (res.dumpStateOneNode(transNode, val2, 2)) {
    return NDBT_FAILED;
  }

  if (res.insertErrorInNode(transNode, 8055)) {
    return NDBT_FAILED;
  }

  hugoOps.execute_Commit(pNdb);  // This should hang/fail

  if (res.waitNodesNoStart(&transNode, 1)) return NDBT_FAILED;

  if (res.startNodes(&transNode, 1)) return NDBT_FAILED;

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug32160(NDBT_Context *ctx, NDBT_Step *step) {
  // int result = NDBT_OK;
  // int loops = ctx->getNumLoops();
  // int records = ctx->getNumRecords();
  // Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  int master = res.getMasterNodeId();
  int next = res.getNextMasterNodeId(master);

  if (res.insertErrorInNode(next, 7194)) {
    return NDBT_FAILED;
  }

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (res.dumpStateOneNode(master, val2, 2)) return NDBT_FAILED;

  if (res.insertErrorInNode(master, 7193)) return NDBT_FAILED;

  int val3[] = {7099};
  if (res.dumpStateOneNode(master, val3, 1)) return NDBT_FAILED;

  if (res.waitNodesNoStart(&master, 1)) return NDBT_FAILED;

  if (res.startNodes(&master, 1)) return NDBT_FAILED;

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug32922(NDBT_Context *ctx, NDBT_Step *step) {
  // int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  // int records = ctx->getNumRecords();
  // Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  while (loops--) {
    int master = res.getMasterNodeId();

    int victim = 32768;
    for (Uint32 i = 0; i < (Uint32)res.getNumDbNodes(); i++) {
      int node = res.getDbNodeId(i);
      if (node != master && node < victim) victim = node;
    }

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(victim, val2, 2)) return NDBT_FAILED;

    if (res.insertErrorInNode(master, 7200)) return NDBT_FAILED;

    if (res.waitNodesNoStart(&victim, 1)) return NDBT_FAILED;

    if (res.startNodes(&victim, 1)) return NDBT_FAILED;

    if (res.waitClusterStarted()) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug34216(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;
  HugoOperations hugoOps(*ctx->getTab());
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);

  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  if (restarter.waitClusterStarted() != 0) {
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

#ifdef NDB_USE_GET_ENV
  char buf[100];
  const char *off = NdbEnv_GetEnv("NDB_ERR_OFFSET", buf, sizeof(buf));
#else
  const char *off = NULL;
#endif
  int offset = off ? atoi(off) : 0;
  int place = 0;
  int ret_code = 0;

  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    if (i > 0 && ctx->closeToTimeout(100 / loops)) break;

    CHK_NDB_READY(pNdb);

    int id = lastId % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    int err = 5048 + ((i + offset) % 2);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    if ((ret_code = hugoOps.startTransaction(pNdb)) != 0) {
      place = 1;
      goto err;
    }

    nodeId = hugoOps.getTransaction()->getConnectedNodeId();
    ndbout << "Restart node " << nodeId << " " << err << endl;

    if (restarter.dumpStateOneNode(nodeId, val2, 2)) {
      g_err << "Failed to dumpStateOneNode" << endl;
      return NDBT_FAILED;
    }

    const Uint32 tableId = ctx->getTab()->getTableId();
    if (restarter.insertError2InNode(nodeId, err, tableId) != 0) {
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.insertErrorInNode(nodeId, 8057) != 0) {
      g_err << "Failed to insert error 8057" << endl;
      result = NDBT_FAILED;
      break;
    }

    int rows = 25;
    if (rows > records) rows = records;

    int batch = 1;
    int row = (records - rows) ? rand() % (records - rows) : 0;
    if (row + rows > records) row = records - row;

    /**
     * We should really somehow check that one of the 25 rows
     *   resides in the node we're targeting
     */
    for (int r = row; r < row + rows; r++) {
      if ((ret_code = hugoOps.pkUpdateRecord(pNdb, r, batch, rand())) != 0) {
        place = 2;
        goto err;
      }

      for (int l = 1; l < 5; l++) {
        if ((ret_code = hugoOps.execute_NoCommit(pNdb)) != 0) {
          place = 3;
          goto err;
        }

        if ((ret_code = hugoOps.pkUpdateRecord(pNdb, r, batch, rand())) != 0) {
          place = 4;
          goto err;
        }
      }
    }

    hugoOps.execute_Commit(pNdb);
    hugoOps.closeTransaction(pNdb);

    if (restarter.waitNodesNoStart(&nodeId, 1)) {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.startNodes(&nodeId, 1)) {
      g_err << "Failed to startNodes" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitClusterStarted() != 0) {
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    lastId++;
    i++;
  }

  ctx->stopTest();

  return result;
err:
  g_err << "Failed with error = " << ret_code << " in place " << place << endl;
  return NDBT_FAILED;
}

int runNF_commit(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter(0, &ctx->m_cluster_connection);
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  if (restarter.waitClusterStarted() != 0) {
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  int i = 0;
  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    int err = 5048;

    ndbout << "Restart node " << nodeId << " " << err << endl;

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (restarter.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

    if (restarter.insertErrorInNode(nodeId, err) != 0) {
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitNodesNoStart(&nodeId, 1)) {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.startNodes(&nodeId, 1)) {
      g_err << "Failed to startNodes" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitClusterStarted() != 0) {
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    i++;
  }

  ctx->stopTest();

  return result;
}

int runBug34702(NDBT_Context *ctx, NDBT_Step *step) {
  // int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  // int records = ctx->getNumRecords();
  // Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  /* Account for 3 tests per loop */
  loops = (loops + 2) / 3;

  while (loops > 0) {
    loops--;
    for (Uint32 i = 0; i <= 2; i++) {
      int victim = res.getDbNodeId(rand() % res.getNumDbNodes());
      res.restartOneDbNode(victim,
                           /** initial */ true,
                           /** nostart */ true,
                           /** abort   */ true);

      if (res.waitNodesNoStart(&victim, 1)) return NDBT_FAILED;

      if (i == 0) {
        res.insertErrorInAllNodes(7204);
      } else if (i == 1) {
        res.insertErrorInAllNodes(7245);
      } else if (i == 2) {
        res.insertErrorInAllNodes(7246);
      }

      res.insertErrorInNode(victim, 7203);

      res.startNodes(&victim, 1);

      if (res.waitClusterStarted()) return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runMNF(NDBT_Context *ctx, NDBT_Step *step) {
  // int result = NDBT_OK;
  NdbRestarter res;
  int numDbNodes = res.getNumDbNodes();
  getNodeGroups(res);
  int num_replicas = (numDbNodes - numNoNodeGroups) / numNodeGroups;

  if (res.getNumDbNodes() < 2 || num_replicas < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes & replicas"
          << endl;
    return NDBT_SKIPPED;
  }

  Vector<int> part0;  // One node per ng
  Vector<int> part1;  // One node per ng
  Vector<int> part2;  // One node per ng
  Vector<int> part3;  // One node per ng
  Bitmask<255> part0mask;
  Bitmask<255> part1mask;
  Bitmask<255> part2mask;
  Bitmask<255> part3mask;
  Uint32 ng_count[MAX_NDB_NODE_GROUPS];
  std::memset(ng_count, 0, sizeof(ng_count));

  for (int i = 0; i < res.getNumDbNodes(); i++) {
    int nodeId = res.getDbNodeId(i);
    int ng = res.getNodeGroup(nodeId);
    if (ng == NDBT_NO_NODE_GROUP_ID) continue;
    if (ng_count[ng] == 0) {
      part0.push_back(nodeId);
      part0mask.set(nodeId);
    } else if (ng_count[ng] == 1) {
      part1.push_back(nodeId);
      part1mask.set(nodeId);
    } else if (ng_count[ng] == 2) {
      part2.push_back(nodeId);
      part2mask.set(nodeId);
    } else if (ng_count[ng] == 3) {
      part3.push_back(nodeId);
      part3mask.set(nodeId);
    } else {
      ndbout_c("Too many replicas");
      return NDBT_FAILED;
    }
    ng_count[ng]++;
  }

  printf("part0: ");
  for (unsigned i = 0; i < part0.size(); i++) printf("%u ", part0[i]);
  printf("\n");

  printf("part1: ");
  for (unsigned i = 0; i < part1.size(); i++) printf("%u ", part1[i]);
  printf("\n");

  printf("part2: ");
  for (unsigned i = 0; i < part2.size(); i++) printf("%u ", part2[i]);
  printf("\n");

  printf("part3: ");
  for (unsigned i = 0; i < part3.size(); i++) printf("%u ", part3[i]);
  printf("\n");

  int loops = ctx->getNumLoops();
  while (loops-- && !ctx->isTestStopped()) {
    int cnt, *nodes;
    int master = res.getMasterNodeId();
    int nextMaster = res.getNextMasterNodeId(master);
    bool obsolete_error = false;

    bool cmf = false;  // true if both master and nextMaster will crash
    if (part0mask.get(master) && part0mask.get(nextMaster)) {
      cmf = true;
      cnt = part0.size();
      nodes = part0.getBase();
      printf("restarting part0");
    } else if (part1mask.get(master) && part1mask.get(nextMaster)) {
      cmf = true;
      cnt = part1.size();
      nodes = part1.getBase();
      printf("restarting part1");
    } else if (part2mask.get(master) && part2mask.get(nextMaster)) {
      cmf = true;
      cnt = part2.size();
      nodes = part2.getBase();
      printf("restarting part2");
    } else if (part3mask.get(master) && part3mask.get(nextMaster)) {
      cmf = true;
      cnt = part3.size();
      nodes = part3.getBase();
      printf("restarting part3");
    } else {
      cmf = false;
      if (loops & 1) {
        cnt = part0.size();
        nodes = part0.getBase();
        printf("restarting part0");
      } else {
        cnt = part1.size();
        nodes = part1.getBase();
        printf("restarting part1");
      }
    }

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    for (int i = 0; i < cnt; i++)
      if (res.dumpStateOneNode(nodes[i], val2, 2)) return NDBT_FAILED;

    int type = loops;
#ifdef NDB_USE_GET_ENV
    char buf[100];
    if (NdbEnv_GetEnv("MNF", buf, sizeof(buf))) {
      type = atoi(buf);
    }
#endif
    if (cmf) {
      type = type % 7;
    } else {
      type = type % 4;
    }
    ndbout_c(" type: %u (cmf: %u)", type, cmf);
    switch (type) {
      case 0:
        for (int i = 0; i < cnt; i++) {
          if (res.restartOneDbNode(nodes[i],
                                   /** initial */ false,
                                   /** nostart */ true,
                                   /** abort   */ true))
            return NDBT_FAILED;

          NdbSleep_MilliSleep(10);
        }
        break;
      case 1:
        for (int i = 0; i < cnt; i++) {
          if (res.restartOneDbNode(nodes[i],
                                   /** initial */ false,
                                   /** nostart */ true,
                                   /** abort   */ true))
            return NDBT_FAILED;
        }
        break;
      case 2:
        for (int i = 0; i < cnt; i++) {
          res.insertErrorInNode(nodes[i], 8058);
        }
        res.restartOneDbNode(nodes[0],
                             /** initial */ false,
                             /** nostart */ true,
                             /** abort   */ true);
        break;
      case 3:
        for (int i = 0; i < cnt; i++) {
          res.insertErrorInNode(nodes[i], 8059);
        }
        res.restartOneDbNode(nodes[0],
                             /** initial */ false,
                             /** nostart */ true,
                             /** abort   */ true);
        break;
      case 4: {
        for (int i = 0; i < cnt; i++) {
          if (res.getNextMasterNodeId(master) == nodes[i])
            res.insertErrorInNode(nodes[i], 7180);
          else
            res.insertErrorInNode(nodes[i], 7205);
        }

        int lcp = 7099;
        res.insertErrorInNode(master, 7193);
        res.dumpStateOneNode(master, &lcp, 1);
        break;
      }
      case 5: {
        for (int i = 0; i < cnt; i++) {
          res.insertErrorInNode(nodes[i], 7206);
        }

        int lcp = 7099;
        res.insertErrorInNode(master, 7193);
        res.dumpStateOneNode(master, &lcp, 1);

        obsolete_error = true;
        break;
      }
      case 6: {
        for (int i = 0; i < cnt; i++) {
          res.insertErrorInNode(nodes[i], 5008);
        }

        int lcp = 7099;
        res.insertErrorInNode(master, 7193);
        res.dumpStateOneNode(master, &lcp, 1);

        obsolete_error = true;
        break;
      }
    }

    /**
     * Note: After version >= 7.4.3, the EMPTY_LCP protocol
     * tested by case 5 & 6 above has become obsolete.
     * Thus, the error insert 7206 / 5008 in all nodes
     * has no effect in case 5 & 6
     * (EMPTY_LCP code still kept for backward compat.)
     * -> Only master node is now killed by error 7193 insert,
     *    and test below now verify that EMPTY_LCP not
     *    being used.
     *
     * Test will fail if mixing versions with and
     * without EMPTY_LCP in use.
     */
    if (obsolete_error)  // Error no longer in use, only master will crash
    {
      if (res.waitNodesNoStart(&master, 1)) return NDBT_FAILED;

      if (res.startNodes(&master, 1)) return NDBT_FAILED;

    } else {
      if (res.waitNodesNoStart(nodes, cnt)) return NDBT_FAILED;

      if (res.startNodes(nodes, cnt)) return NDBT_FAILED;
    }

    if (res.waitClusterStarted()) return NDBT_FAILED;

    if (obsolete_error)  // Error never cleared nor node restarted
    {
      /*
       * For obsolete error inserts, error is never cleared nor node
       * restarted.  Clearing those here after test case succeeded.
       */
      for (int i = 0; i < cnt; i++) {
        if (nodes[i] == master) continue;
        res.insertErrorInNode(nodes[i], 0);
      }
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug36199(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }
  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);
  int victim = res.getRandomNodeSameNodeGroup(nextMaster, rand());
  if (victim == master) {
    victim = res.getRandomNodeOtherNodeGroup(nextMaster, rand());
  }
  require(victim != -1);

  ndbout_c("master: %u next master: %u victim: %u", master, nextMaster, victim);

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(victim, val2, 2);

  res.insertErrorInNode(victim, 7205);
  res.insertErrorInNode(master, 7014);
  int lcp = 7099;
  res.dumpStateOneNode(master, &lcp, 1);

  int nodes[2];
  nodes[0] = master;
  nodes[1] = victim;
  if (res.waitNodesNoStart(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.startNodes(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug36246(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  Ndb *pNdb = GETNDB(step);

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }
  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  HugoOperations hugoOps(*ctx->getTab());
restartloop:
  CHK_NDB_READY(pNdb);
  int tryloop = 0;
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);

loop:
  if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

  if (hugoOps.pkUpdateRecord(pNdb, 1, 1) != 0) return NDBT_FAILED;

  if (hugoOps.execute_NoCommit(pNdb) != 0) return NDBT_FAILED;

  int victim = hugoOps.getTransaction()->getConnectedNodeId();
  printf("master: %u nextMaster: %u victim: %u", master, nextMaster, victim);
  if (victim == master || victim == nextMaster ||
      res.getNodeGroup(victim) == res.getNodeGroup(master) ||
      res.getNodeGroup(victim) == res.getNodeGroup(nextMaster)) {
    hugoOps.execute_Rollback(pNdb);
    hugoOps.closeTransaction(pNdb);
    tryloop++;
    if (tryloop == 10) {
      ndbout_c(" -> restarting next master: %u", nextMaster);
      res.restartOneDbNode(nextMaster,
                           /** initial */ false,
                           /** nostart */ true,
                           /** abort   */ true);

      res.waitNodesNoStart(&nextMaster, 1);
      res.startNodes(&nextMaster, 1);
      if (res.waitClusterStarted()) return NDBT_FAILED;
      goto restartloop;
    } else {
      ndbout_c(" -> loop");
      goto loop;
    }
  }
  ndbout_c(" -> go go gadget skates");

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(victim, val2, 2);

  res.insertErrorInNode(master, 8060);
  res.insertErrorInNode(victim, 9999);

  int nodes[2];
  nodes[0] = master;
  nodes[1] = victim;
  if (res.waitNodesNoStart(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.startNodes(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.waitClusterStarted()) return NDBT_FAILED;

  CHK_NDB_READY(pNdb);

  hugoOps.execute_Rollback(pNdb);
  hugoOps.closeTransaction(pNdb);

  return NDBT_OK;
}

int runBug36247(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }
  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());

restartloop:
  CHK_NDB_READY(pNdb);
  int tryloop = 0;
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);

loop:
  if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

  if (hugoOps.pkUpdateRecord(pNdb, 1, 100) != 0) return NDBT_FAILED;

  if (hugoOps.execute_NoCommit(pNdb) != 0) return NDBT_FAILED;

  int victim = hugoOps.getTransaction()->getConnectedNodeId();
  printf("master: %u nextMaster: %u victim: %u", master, nextMaster, victim);
  if (victim == master || victim == nextMaster ||
      res.getNodeGroup(victim) == res.getNodeGroup(master) ||
      res.getNodeGroup(victim) == res.getNodeGroup(nextMaster)) {
    hugoOps.execute_Rollback(pNdb);
    hugoOps.closeTransaction(pNdb);
    tryloop++;
    if (tryloop == 10) {
      ndbout_c(" -> restarting next master: %u", nextMaster);
      res.restartOneDbNode(nextMaster,
                           /** initial */ false,
                           /** nostart */ true,
                           /** abort   */ true);

      res.waitNodesNoStart(&nextMaster, 1);
      res.startNodes(&nextMaster, 1);
      if (res.waitClusterStarted()) return NDBT_FAILED;
      goto restartloop;
    } else {
      ndbout_c(" -> loop");
      goto loop;
    }
  }
  ndbout_c(" -> go go gadget skates");

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(victim, val2, 2);

  int err5050[] = {5050};
  res.dumpStateAllNodes(err5050, 1);

  res.insertErrorInNode(victim, 9999);

  int nodes[2];
  nodes[0] = master;
  nodes[1] = victim;
  if (res.waitNodesNoStart(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.startNodes(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.waitClusterStarted()) return NDBT_FAILED;
  CHK_NDB_READY(pNdb);
  hugoOps.execute_Rollback(pNdb);
  hugoOps.closeTransaction(pNdb);

  return NDBT_OK;
}

int runBug36276(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * This test case was introduced to test the EMPTY_LCP protocol.
   * This protocol was removed in 7.4, so now this function simply
   * tests shooting down the master node at the end phases of an LCP.
   */
  // int result = NDBT_OK;
  // int loops = ctx->getNumLoops();
  NdbRestarter res;
  // Ndb* pNdb = GETNDB(step);

  if (res.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    return NDBT_SKIPPED;
  }
  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }

  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);
  int victim = res.getRandomNodeSameNodeGroup(nextMaster, rand());
  if (victim == master) {
    victim = res.getRandomNodeOtherNodeGroup(nextMaster, rand());
  }
  require(victim != -1);  // having tried both same group and other group

  ndbout_c("master: %u nextMaster: %u victim: %u", master, nextMaster, victim);

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  res.dumpStateOneNode(master, val2, 2);
  res.insertErrorInNode(victim, 7209);

  int lcp = 7099;
  res.dumpStateOneNode(master, &lcp, 1);

  if (res.waitNodesNoStart(&master, 1)) {
    return NDBT_FAILED;
  }

  if (res.startNodes(&master, 1)) {
    return NDBT_FAILED;
  }

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runBug36245(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  Ndb *pNdb = GETNDB(step);

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }
  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  /**
   * Make sure master and nextMaster is in different node groups
   */
loop1:
  CHK_NDB_READY(pNdb);
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);

  printf("master: %u nextMaster: %u", master, nextMaster);
  if (res.getNodeGroup(master) == res.getNodeGroup(nextMaster)) {
    ndbout_c(" -> restarting next master: %u", nextMaster);
    res.restartOneDbNode(nextMaster,
                         /** initial */ false,
                         /** nostart */ true,
                         /** abort   */ true);

    res.waitNodesNoStart(&nextMaster, 1);
    res.startNodes(&nextMaster, 1);
    if (res.waitClusterStarted()) {
      ndbout_c("cluster didnt restart!!");
      return NDBT_FAILED;
    }
    goto loop1;
  }
  ndbout_c(" -> go go gadget skates");

  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(nextMaster, val2, 2);

  res.insertErrorInNode(master, 8063);
  res.insertErrorInNode(nextMaster, 936);

  int err = 0;
  HugoOperations hugoOps(*ctx->getTab());

  if ((err = hugoOps.startTransaction(pNdb, master, 0)) != 0) {
    ndbout_c("failed to start transaction: %u", err);
    return NDBT_FAILED;
  }

  int victim = hugoOps.getTransaction()->getConnectedNodeId();
  if (victim != master) {
    ndbout_c("ERR: transnode: %u != master: %u -> loop", victim, master);
    hugoOps.closeTransaction(pNdb);
    return NDBT_FAILED;
  }

  if ((err = hugoOps.pkUpdateRecord(pNdb, 1)) != 0) {
    ndbout_c("failed to update: %u", err);
    return NDBT_FAILED;
  }

  if ((err = hugoOps.execute_Commit(pNdb)) != 4010) {
    ndbout_c("incorrect error code: %u", err);
    return NDBT_FAILED;
  }
  hugoOps.closeTransaction(pNdb);

  int nodes[2];
  nodes[0] = master;
  nodes[1] = nextMaster;
  if (res.waitNodesNoStart(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.startNodes(nodes, 2)) {
    return NDBT_FAILED;
  }

  if (res.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

int runHammer(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  while (!ctx->isTestStopped()) {
    int r = rand() % records;
    if (hugoOps.startTransaction(pNdb) != 0) continue;

    if ((rand() % 100) < 50) {
      if (hugoOps.pkUpdateRecord(pNdb, r, 1, rand()) != 0) goto err;
    } else {
      if (hugoOps.pkWriteRecord(pNdb, r, 1, rand()) != 0) goto err;
    }

    if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

    if (hugoOps.pkDeleteRecord(pNdb, r, 1) != 0) goto err;

    if (hugoOps.execute_NoCommit(pNdb) != 0) goto err;

    if ((rand() % 100) < 50) {
      if (hugoOps.pkInsertRecord(pNdb, r, 1, rand()) != 0) goto err;
    } else {
      if (hugoOps.pkWriteRecord(pNdb, r, 1, rand()) != 0) goto err;
    }

    if ((rand() % 100) < 90) {
      hugoOps.execute_Commit(pNdb);
    } else {
    err:
      hugoOps.execute_Rollback(pNdb);
    }

    hugoOps.closeTransaction(pNdb);
  }
  return NDBT_OK;
}

int runMixedLoad(NDBT_Context *ctx, NDBT_Step *step) {
  int res = 0;
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  unsigned id = (unsigned)rand();
  while (!ctx->isTestStopped()) {
    if (ctx->getProperty("Pause", (Uint32)0)) {
      ndbout_c("thread %u stopped", id);
      ctx->sync_down("WaitThreads");
      while (ctx->getProperty("Pause", (Uint32)0) && !ctx->isTestStopped())
        NdbSleep_MilliSleep(15);

      if (ctx->isTestStopped()) break;
      ndbout_c("thread %u continue", id);
    }

    if ((res = hugoOps.startTransaction(pNdb)) != 0) {
      if (res == 4009) return NDBT_FAILED;
      continue;
    }

    for (int i = 0; i < 10; i++) {
      int r = rand() % records;
      if ((rand() % 100) < 50) {
        if (hugoOps.pkUpdateRecord(pNdb, r, 1, rand()) != 0) goto err;
      } else {
        if (hugoOps.pkWriteRecord(pNdb, r, 1, rand()) != 0) goto err;
      }
    }

    if ((rand() % 100) < 90) {
      res = hugoOps.execute_Commit(pNdb);
    } else {
    err:
      res = hugoOps.execute_Rollback(pNdb);
    }

    hugoOps.closeTransaction(pNdb);

    if (res == 4009) {
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runBug41295(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int leak = 4002;
  const int cases = 1;
  int loops = ctx->getNumLoops();
  if (loops <= cases) loops = cases + 1;

  for (int i = 0; i < loops; i++) {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(next, val2, 2)) return NDBT_FAILED;

    ndbout_c("stopping %u, err 8073", next);
    res.insertErrorInNode(next, 8073);
    ndbout_c("waiting for %u", next);
    res.waitNodesNoStart(&next, 1);

    ndbout_c("pausing all threads");
    ctx->setProperty("Pause", 1);
    ctx->sync_up_and_wait("WaitThreads", ctx->getProperty("Threads", 1));
    ndbout_c("all threads paused");
    NdbSleep_MilliSleep(5000);
    res.dumpStateAllNodes(&leak, 1);
    NdbSleep_MilliSleep(1000);
    if (res.checkClusterAlive(&next, 1)) {
      return NDBT_FAILED;
    }
    ndbout_c("restarting threads");
    ctx->setProperty("Pause", (Uint32)0);

    ndbout_c("starting %u", next);
    res.startNodes(&next, 1);
    ndbout_c("waiting for cluster started");
    if (res.waitClusterStarted()) {
      return NDBT_FAILED;
    }

    ndbout_c("pausing all threads");
    ctx->setProperty("Pause", 1);
    ctx->sync_up_and_wait("WaitThreads", ctx->getProperty("Threads", 1));
    ndbout_c("all threads paused");
    NdbSleep_MilliSleep(5000);
    res.dumpStateAllNodes(&leak, 1);
    NdbSleep_MilliSleep(1000);
    ndbout_c("restarting threads");
    ctx->setProperty("Pause", (Uint32)0);
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug41469(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();

  int val0[] = {7216, 0};
  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  for (int i = 0; i < loops; i++) {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);

    if (res.dumpStateOneNode(master, val2, 2)) return NDBT_FAILED;

    ndbout_c("stopping %u, err 7216 (next: %u)", master, next);
    val0[1] = next;
    if (res.dumpStateOneNode(master, val0, 2)) return NDBT_FAILED;

    res.waitNodesNoStart(&master, 1);
    res.startNodes(&master, 1);
    ndbout_c("waiting for cluster started");
    if (res.waitClusterStarted()) {
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

int runBug42422(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Need at least 2 node groups to run the test" << endl;
    return NDBT_SKIPPED;
  }

  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();
  while (--loops >= 0) {
    int master = res.getMasterNodeId();
    ndbout_c("master: %u", master);
    int nodeId = res.getRandomNodeSameNodeGroup(master, rand());
    ndbout_c("target: %u", nodeId);
    int node2 = res.getRandomNodeOtherNodeGroup(nodeId, rand());
    ndbout_c("node 2: %u", node2);

    if (node2 == -1) {
      g_err << "Could not get node from other node group" << endl;
      return NDBT_FAILED;
    }

    res.restartOneDbNode(nodeId,
                         /** initial */ false,
                         /** nostart */ true,
                         /** abort   */ true);

    res.waitNodesNoStart(&nodeId, 1);

    int dump[] = {9000, 0};
    dump[1] = node2;

    if (res.dumpStateOneNode(nodeId, dump, 2)) return NDBT_FAILED;

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

    res.insertErrorInNode(nodeId, 937);
    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);

    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);

    ndbout_c("%u : waiting for cluster started", __LINE__);
    if (res.waitClusterStarted()) {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug43224(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();
  while (--loops >= 0) {
    int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
    res.restartOneDbNode(nodeId,
                         /** initial */ false,
                         /** nostart */ true,
                         /** abort   */ true);

    res.waitNodesNoStart(&nodeId, 1);

    NdbSleep_SecSleep(10);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

    res.insertErrorInNode(nodeId, 9994);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);

    if (res.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

    res.insertErrorInNode(nodeId, 9994);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);

    NdbSleep_SecSleep(20);  // Hardcoded in ndb_mgmd (alloc timeout)

    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);

    ndbout_c("%u : waiting for cluster started", __LINE__);
    if (res.waitClusterStarted()) {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug43888(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();
  while (--loops >= 0) {
    int master = res.getMasterNodeId();
    ndbout_c("master: %u", master);
    int nodeId = master;
    do {
      nodeId = res.getNode(NdbRestarter::NS_RANDOM);
    } while (nodeId == master);

    ndbout_c("target: %u", nodeId);

    res.restartOneDbNode(nodeId,
                         /** initial */ false,
                         /** nostart */ true,
                         /** abort   */ true);

    res.waitNodesNoStart(&nodeId, 1);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (res.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

    res.insertErrorInNode(master, 7217);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);

    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);

    ndbout_c("%u : waiting for cluster started", __LINE__);
    if (res.waitClusterStarted()) {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug44952(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();

  const int codes[] = {5051, 5052, 5053, 0};
  (void)codes;

  // int randomId = myRandom48(res.getNumDbNodes());
  // int nodeId = res.getDbNodeId(randomId);

  int loops = ctx->getNumLoops();
  const int val[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  for (int l = 0; l < loops; l++) {
    int randomId = myRandom48(res.getNumDbNodes());
    int nodeId = res.getDbNodeId(randomId);

    ndbout_c("killing node %u error 5051 loop %u/%u", nodeId, l + 1, loops);
    CHECK(res.dumpStateOneNode(nodeId, val, 2) == 0,
          "failed to set RestartOnErrorInsert");

    CHECK(res.insertErrorInNode(nodeId, 5051) == 0,
          "failed to insert error 5051");

    while (res.waitNodesNoStart(&nodeId, 1, 1 /* seconds */) != 0) {
      pDict->forceGCPWait();
    }

    ndbout_c("killing node %u during restart error 5052", nodeId);
    for (int j = 0; j < 3; j++) {
      ndbout_c("loop: %d - killing node %u during restart error 5052", j,
               nodeId);
      int val[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
      CHECK(res.dumpStateOneNode(nodeId, val, 2) == 0,
            "failed to set RestartOnErrorInsert");

      CHECK(res.insertErrorInNode(nodeId, 5052) == 0,
            "failed to set error insert");

      NdbSleep_SecSleep(3);  // ...

      CHECK(res.startNodes(&nodeId, 1) == 0, "failed to start node");

      NdbSleep_SecSleep(3);

      CHECK(res.waitNodesNoStart(&nodeId, 1) == 0, "waitNodesNoStart failed");
    }

    CHECK(res.startNodes(&nodeId, 1) == 0, "failed to start node");

    CHECK(res.waitNodesStarted(&nodeId, 1) == 0, "waitNodesStarted failed");
  }

  ctx->stopTest();
  return NDBT_OK;
}

static BaseString tab_48474;

int initBug48474(NDBT_Context *ctx, NDBT_Step *step) {
  NdbDictionary::Table tab = *ctx->getTab();
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();

  const NdbDictionary::Table *pTab = pDict->getTable(tab.getName());
  if (pTab == 0) return NDBT_FAILED;

  /**
   * Create a table with tableid > ctx->getTab()
   */
  Uint32 cnt = 0;
  Vector<BaseString> tables;
  do {
    BaseString tmp;
    tmp.assfmt("%s_%u", tab.getName(), cnt);
    tab.setName(tmp.c_str());

    pDict->dropTable(tab.getName());
    if (pDict->createTable(tab) != 0) return NDBT_FAILED;

    const NdbDictionary::Table *pTab2 = pDict->getTable(tab.getName());
    if (pTab2->getObjectId() < pTab->getObjectId()) {
      tables.push_back(tmp);
    } else {
      tab_48474 = tmp;
      HugoTransactions hugoTrans(*pTab2);
      if (hugoTrans.loadTable(GETNDB(step), 1000) != 0) {
        return NDBT_FAILED;
      }
      break;
    }
    cnt++;
  } while (true);

  // Now delete the extra one...
  for (Uint32 i = 0; i < tables.size(); i++) {
    pDict->dropTable(tables[i].c_str());
  }

  tables.clear();

  return NDBT_OK;
}

int runBug48474(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();
  const NdbDictionary::Table *pTab = pDict->getTable(tab_48474.c_str());
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*pTab);

  int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
  ndbout_c("restarting %d", nodeId);
  res.restartOneDbNode(nodeId, false, true, true);
  res.waitNodesNoStart(&nodeId, 1);

  int minlcp[] = {7017, 1};
  res.dumpStateAllNodes(minlcp, 1);  // Set min time between LCP

  ndbout_c("starting %d", nodeId);
  res.startNodes(&nodeId, 1);

  Uint32 minutes = 5;
  ndbout_c("starting uncommitted transaction %u minutes", minutes);
  for (Uint32 m = 0; m < minutes; m++) {
    int retry = 0;
    while (retry < 300) {
      if (hugoOps.startTransaction(pNdb) != 0) {
        ndbout_c("startTransaction failed");
        return NDBT_FAILED;
      }

      if (hugoOps.pkUpdateRecord(pNdb, 0, 50, rand()) != 0) {
        ndbout_c("pkUpdateRecord failed");
        return NDBT_FAILED;
      }
      int ret_code;
      if ((ret_code = hugoOps.execute_NoCommit(pNdb)) != 0) {
        if (ret_code == 410) {
          hugoOps.closeTransaction(pNdb);
          NdbSleep_MilliSleep(100);
          ndbout_c("410 on main node, wait a 100ms");
          retry++;
          continue;
        }
        ndbout_c("Prepare failed error: %u", ret_code);
        return NDBT_FAILED;
      }
      break;
    }
    if (retry >= 300) {
      ndbout_c("Test stopped due to problems with 410");
      break;
    }

    ndbout_c("sleeping 60s");
    for (Uint32 i = 0; i < 600 && !ctx->isTestStopped(); i++) {
      hugoOps.getTransaction()->refresh();
      NdbSleep_MilliSleep(100);
    }

    if (hugoOps.execute_Commit(pNdb) != 0) {
      ndbout_c("Transaction commit failed");
      return NDBT_FAILED;
    }

    hugoOps.closeTransaction(pNdb);

    if (ctx->isTestStopped()) break;
  }
  res.dumpStateAllNodes(minlcp, 2);  // reset min time between LCP
  if (res.waitClusterStarted() != 0) {
    ndbout_c("Failed to start cluster");
    return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}

int cleanupBug48474(NDBT_Context *ctx, NDBT_Step *step) {
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();
  pDict->dropTable(tab_48474.c_str());
  return NDBT_OK;
}

int runBug56044(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  for (int i = 0; i < loops; i++) {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);
    ndbout_c("master: %u next: %u", master, next);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    if (res.dumpStateOneNode(master, val2, 2)) return NDBT_FAILED;

    if (res.insertErrorInNode(next, 7224)) return NDBT_FAILED;

    if (res.waitNodesNoStart(&master, 1)) return NDBT_FAILED;
    if (res.startNodes(&master, 1)) return NDBT_FAILED;
    if (res.waitClusterStarted() != 0) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug57767(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  int node0 = res.getNode(NdbRestarter::NS_RANDOM);
  int node1 = res.getRandomNodeSameNodeGroup(node0, rand());
  ndbout_c("%u %u", node0, node1);

  res.restartOneDbNode(node0, false, true, true);
  res.waitNodesNoStart(&node0, 1);
  res.insertErrorInNode(node0, 1000);
  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  res.dumpStateOneNode(node0, val2, 2);

  HugoTransactions hugoTrans(*ctx->getTab());
  hugoTrans.scanUpdateRecords(GETNDB(step), 0);

  res.insertErrorInNode(node1, 5060);
  res.startNodes(&node0, 1);
  NdbSleep_SecSleep(3);
  res.waitNodesNoStart(&node0, 1);

  res.insertErrorInNode(node1, 0);
  res.startNodes(&node0, 1);
  res.waitClusterStarted();
  return NDBT_OK;
}

int runBug57522(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    return NDBT_SKIPPED;
  }

  for (int i = 0; i < loops; i++) {
    int master = res.getMasterNodeId();
    int next0 = res.getNextMasterNodeId(master);
    int next1 = res.getNextMasterNodeId(next0);
    ndbout_c("master: %d next0: %d next1: %d", master, next0, next1);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    if (res.dumpStateOneNode(master, val2, 2)) return NDBT_FAILED;

    int val3[] = {7999, 7226, next1};
    if (res.dumpStateOneNode(master, val3, 3)) return NDBT_FAILED;

    res.waitNodesNoStart(&master, 1);
    res.startNodes(&master, 1);
    if (res.waitClusterStarted() != 0) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runForceStopAndRestart(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  if (res.getNumDbNodes() != 2) {
    g_err << "[SKIPPED] Test skipped. Requires 2 nodes" << endl;
    return NDBT_SKIPPED;
  }

  Vector<int> group1;
  Vector<int> group2;
  Bitmask<256 / 32> nodeGroupMap;
  for (int j = 0; j < res.getNumDbNodes(); j++) {
    int node = res.getDbNodeId(j);
    int ng = res.getNodeGroup(node);
    if (ng == NDBT_NO_NODE_GROUP_ID) continue;
    if (nodeGroupMap.get(ng)) {
      group2.push_back(node);
    } else {
      group1.push_back(node);
      nodeGroupMap.set(ng);
    }
  }

  printf("group1: ");
  for (unsigned i = 0; i < group1.size(); i++) printf("%d ", group1[i]);
  printf("\n");

  printf("group2: ");
  for (unsigned i = 0; i < group2.size(); i++) printf("%d ", group2[i]);
  printf("\n");

  // Stop half of the cluster
  res.restartNodes(group1.getBase(), (int)group1.size(),
                   NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
  res.waitNodesNoStart(group1.getBase(), (int)group1.size());

  ndbout_c("%u", __LINE__);
  // Try to stop first node in second half without force, should return error
  if (res.restartOneDbNode(group2[0], false, /* initial */
                           true,             /* nostart  */
                           false,            /* abort */
                           false /* force */) != -1) {
    ndbout_c("%u", __LINE__);
    g_err << "Restart suceeded without force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  // Now stop with force
  if (res.restartOneDbNode(group2[0], false, /* initial */
                           true,             /* nostart  */
                           false,            /* abort */
                           true /* force */) != 0) {
    ndbout_c("%u", __LINE__);
    g_err << "Could not restart with force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  // All nodes should now be in nostart, the above stop force
  // caused the remaining nodes to be stopped(and restarted nostart)
  res.waitClusterNoStart();

  ndbout_c("%u", __LINE__);

  // Start second half back up again
  res.startNodes(group2.getBase(), (int)group2.size());
  res.waitNodesStarted(group2.getBase(), (int)group2.size());

  ndbout_c("%u", __LINE__);

  // Try to stop remaining half without force, should return error
  if (res.restartNodes(group2.getBase(), (int)group2.size(),
                       NdbRestarter::NRRF_NOSTART) != -1) {
    g_err << "Restart suceeded without force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  // Now stop with force
  if (res.restartNodes(group2.getBase(), (int)group2.size(),
                       NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_FORCE) !=
      0) {
    g_err << "Could not restart with force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  if (res.waitNodesNoStart(group2.getBase(), (int)group2.size())) {
    g_err << "Failed to waitNodesNoStart" << endl;
    return NDBT_FAILED;
  }

  // Start all nodes again
  res.startAll();
  res.waitClusterStarted();

  return NDBT_OK;
}

int runBug58453(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  if (res.getNumReplicas() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 Replicas" << endl;
    return NDBT_SKIPPED;
  }
  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 Node Groups" << endl;
    return NDBT_SKIPPED;
  }

  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());

  int loops = ctx->getNumLoops();
  while (loops--) {
    if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

    if (hugoOps.pkInsertRecord(pNdb, 0, 128 /* records */) != 0)
      return NDBT_FAILED;

    int err = 5062;
    switch (loops & 1) {
      case 0:
        err = 5062;
        break;
      case 1:
        err = 5063;
        break;
    }
    int node = (int)hugoOps.getTransaction()->getConnectedNodeId();
    int node0 = res.getRandomNodeOtherNodeGroup(node, rand());

    int node1 = res.getRandomNodeSameNodeGroup(node0, rand());

    ndbout_c("node %u err: %u, node: %u err: %u", node0, 5061, node1, err);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};

    res.dumpStateOneNode(node, val2, 2);
    res.insertErrorInNode(node0, 5061);
    res.insertErrorInNode(node1, err);

    hugoOps.execute_Commit(pNdb);
    hugoOps.closeTransaction(pNdb);

    res.waitNodesNoStart(&node, 1);
    res.startNodes(&node, 1);
    res.waitClusterStarted();
    CHK_NDB_READY(pNdb);
    hugoOps.clearTable(pNdb);
  }
  return NDBT_OK;
}

int runRestartToDynamicOrder(NDBT_Context *ctx, NDBT_Step *step) {
  /* Here we perform node restarts to get the various node's
   * dynamic ids in a particular order
   * This affects which nodes heartbeat which (low -> high)
   * and which is the president (lowest).
   * Each restarting node gets a higher dynamic id, so the
   * first node restarted will eventually become president
   * Note that we're assuming NoOfReplicas == 2 here.
   */
  /* TODO :
   * Refactor into
   *   1) Get current cluster dynorder info
   *   2) Choose a dynorder info
   *   3) Restart to given dynorder if necessary
   */
  Uint32 dynOrder = ctx->getProperty("DynamicOrder", Uint32(0));
  NdbRestarter restarter;
  Uint32 numNodes = restarter.getNumDbNodes();
  getNodeGroups(restarter);
  int num_replicas = (numNodes - numNoNodeGroups) / numNodeGroups;
  if (num_replicas != 2) {
    g_err << "[SKIPPED] Test skipped. Requires 2 replicas" << endl;
    return NDBT_SKIPPED;
  }

  Vector<Uint32> currOrder;
  Vector<Uint32> newOrder;
  Vector<Uint32> odds;
  Vector<Uint32> evens;

  if (numNodes == 2) {
    ndbout_c("[SKIPPED] No Dynamic reordering possible with 2 nodes");
    return NDBT_SKIPPED;
  }
  if (numNodes & 1) {
    ndbout_c("Non multiple-of-2 number of nodes.  Not supported");
    return NDBT_FAILED;
  }

  Uint32 master = restarter.getMasterNodeId();

  for (Uint32 n = 0; n < numNodes; n++) {
    currOrder.push_back(master);
    master = restarter.getNextMasterNodeId(master);
  }

  for (Uint32 n = 0; n < numNodes; n++) {
    Uint32 nodeId = restarter.getDbNodeId(n);
    if (nodeId & 1) {
      odds.push_back(nodeId);
    } else {
      evens.push_back(nodeId);
    }
  }

  if (odds.size() != evens.size()) {
    ndbout_c("Failed - odds.size() (%u) != evens.size() (%u)", odds.size(),
             evens.size());
    return NDBT_FAILED;
  }

  ndbout_c("Current dynamic ordering : ");
  for (Uint32 n = 0; n < numNodes; n++) {
    ndbout_c("  %u %s", currOrder[n], ((n == 0) ? "*" : ""));
  }

  if (dynOrder == 0) {
    ndbout_c("No change in dynamic order");
    return NDBT_OK;
  }

  Uint32 control = dynOrder - 1;

  bool oddPresident = control & 1;
  bool interleave = control & 2;
  bool reverseSideA = control & 4;
  bool reverseSideB = control & 8;

  /*     Odds first    Interleave O/E  Reverse A  Reverse B
   * 1       N              N              N         N
   * 2       Y              N              N         N
   * 3       N              Y              N         N
   * 4       Y              Y              N         N
   * 5       N              N              Y         N
   * 6       Y              N              Y         N
   * 7       N              Y              Y         N
   * 8       Y              Y              Y         N
   * 9       N              N              N         Y
   * 10      Y              N              N         Y
   * 11      N              Y              N         Y
   * 12      Y              Y              N         Y
   * 13      N              N              Y         Y
   * 14      Y              N              Y         Y
   * 15      N              Y              Y         Y
   * 16      Y              Y              Y         Y
   *
   * Interesting values
   *   1) Even first, no interleave, no reverse
   *      e.g. 2->4->6->3->5->7
   *   2) Odd first, no interleave, no reverse
   *      e.g. 3->5->7->2->4->6
   *   3) Even first, interleave, no reverse
   *      e.g. 2->3->4->5->6->7
   *   9) Even first, no interleave, reverse B
   *      e.g. 2->4->6->7->5->3
   *
   *  'First' node becomes president.
   *  Which node(s) monitor president affects when
   *  arbitration may be required
   */

  ndbout_c("Generating ordering with %s president, sides %sinterleaved",
           (oddPresident ? "odd" : "even"), (interleave ? "" : "not "));
  if (reverseSideA)
    ndbout_c("  %s reversed", (oddPresident ? "odds" : "evens"));

  if (reverseSideB)
    ndbout_c("  %s reversed", (oddPresident ? "evens" : "odds"));

  Vector<Uint32> *sideA;
  Vector<Uint32> *sideB;

  if (oddPresident) {
    sideA = &odds;
    sideB = &evens;
  } else {
    sideA = &evens;
    sideB = &odds;
  }

  if (interleave) {
    for (Uint32 n = 0; n < sideA->size(); n++) {
      Uint32 indexA = reverseSideA ? (sideA->size() - (n + 1)) : n;
      newOrder.push_back((*sideA)[indexA]);
      Uint32 indexB = reverseSideB ? (sideB->size() - (n + 1)) : n;
      newOrder.push_back((*sideB)[indexB]);
    }
  } else {
    for (Uint32 n = 0; n < sideA->size(); n++) {
      Uint32 indexA = reverseSideA ? (sideA->size() - (n + 1)) : n;
      newOrder.push_back((*sideA)[indexA]);
    }
    for (Uint32 n = 0; n < sideB->size(); n++) {
      Uint32 indexB = reverseSideB ? (sideB->size() - (n + 1)) : n;
      newOrder.push_back((*sideB)[indexB]);
    }
  }

  bool diff = false;
  for (Uint32 n = 0; n < newOrder.size(); n++) {
    ndbout_c("  %u %s", newOrder[n], ((n == 0) ? "*" : " "));

    diff |= (newOrder[n] != currOrder[n]);
  }

  if (!diff) {
    ndbout_c("Cluster already in correct configuration");
    return NDBT_OK;
  }

  for (Uint32 n = 0; n < newOrder.size(); n++) {
    ndbout_c("Now restarting node %u", newOrder[n]);
    if (restarter.restartOneDbNode(newOrder[n],
                                   false,  // initial
                                   true,   // nostart
                                   true)   // abort
        != NDBT_OK) {
      ndbout_c("Failed to restart node");
      return NDBT_FAILED;
    }
    if (restarter.waitNodesNoStart((const int *)&newOrder[n], 1) != NDBT_OK) {
      ndbout_c("Failed waiting for node to enter NOSTART state");
      return NDBT_FAILED;
    }
    if (restarter.startNodes((const int *)&newOrder[n], 1) != NDBT_OK) {
      ndbout_c("Failed to start node");
      return NDBT_FAILED;
    }
    if (restarter.waitNodesStarted((const int *)&newOrder[n], 1) != NDBT_OK) {
      ndbout_c("Failed waiting for node to start");
      return NDBT_FAILED;
    }
    ndbout_c("  Done.");
  }

  ndbout_c("All restarts completed.  NdbRestarter says master is %u",
           restarter.getMasterNodeId());
  if (restarter.getMasterNodeId() != (int)newOrder[0]) {
    ndbout_c("  Should be %u, failing", newOrder[0]);
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

struct NodeGroupMembers {
  Uint32 ngid;
  Uint32 membCount;
  Uint32 members[4];
};

template class Vector<NodeGroupMembers>;

int analyseDynamicOrder(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  Uint32 numNodes = restarter.getNumDbNodes();
  Uint32 master = restarter.getMasterNodeId();
  Vector<Uint32> dynamicOrder;
  Vector<Uint32> nodeGroup;
  Vector<Uint32> monitorsNode;
  Vector<Uint32> monitoredByNode;
  Vector<Uint32> monitorsRemote;
  Vector<Uint32> remoteMonitored;
  Vector<Uint32> sameNGMonitored;
  Vector<Uint32> distanceToRemote;
  Vector<Uint32> nodeIdToDynamicIndex;
  Uint32 maxDistanceToRemoteLink = 0;
  getNodeGroups(restarter);
  int num_replicas = (numNodes - numNoNodeGroups) / numNodeGroups;
  if (num_replicas != 2) {
    g_err << "[SKIPPED] Test skipped. Requires 2 replicas" << endl;
    return NDBT_SKIPPED;
  }

  /* TODO :
   * Refactor into :
   *   1) Determine dynorder from running cluster
   *   2) Analyse dynorder in general
   *   3) Analyse dynorder from point of view of latency split
   *
   *   4) Support splits other than odd/even total
   *      - Partial split
   *      - Some link failures
   */

  /* Determine dynamic order from running cluster */
  for (Uint32 n = 0; n < numNodes; n++) {
    dynamicOrder.push_back(master);
    nodeGroup.push_back(restarter.getNodeGroup(master));
    Uint32 zero = 0;
    nodeIdToDynamicIndex.set(n, master, zero);
    master = restarter.getNextMasterNodeId(master);
  }

  /* Look at implied HB links */
  for (Uint32 n = 0; n < numNodes; n++) {
    Uint32 nodeId = dynamicOrder[n];
    Uint32 monitoredByIndex = (n + 1) % numNodes;
    Uint32 monitorsIndex = (n + numNodes - 1) % numNodes;
    monitoredByNode.push_back(dynamicOrder[monitoredByIndex]);
    monitorsNode.push_back(dynamicOrder[monitorsIndex]);
    remoteMonitored.push_back((nodeId & 1) != (monitoredByNode[n] & 1));
    monitorsRemote.push_back((nodeId & 1) != (monitorsNode[n] & 1));
    sameNGMonitored.push_back(nodeGroup[n] == nodeGroup[monitoredByIndex]);
  }

  /* Look at split implications */
  for (Uint32 n = 0; n < numNodes; n++) {
    Uint32 distanceToRemoteHBLink = 0;
    for (Uint32 m = 0; m < numNodes; m++) {
      if (remoteMonitored[n + m]) break;
      distanceToRemoteHBLink++;
    }

    distanceToRemote.push_back(distanceToRemoteHBLink);
    maxDistanceToRemoteLink =
        MAX(maxDistanceToRemoteLink, distanceToRemoteHBLink);
  }

  ndbout_c("Dynamic order analysis");

  for (Uint32 n = 0; n < numNodes; n++) {
    ndbout_c(
        "  %u %s %u%s%u%s%u \t Monitored by %s nodegroup, Dist to remote link "
        ": %u",
        dynamicOrder[n], ((n == 0) ? "*" : " "), monitorsNode[n],
        ((monitorsRemote[n]) ? "  >" : "-->"), dynamicOrder[n],
        ((remoteMonitored[n]) ? "  >" : "-->"), monitoredByNode[n],
        ((sameNGMonitored[n]) ? "same" : "other"), distanceToRemote[n]);
  }

  ndbout_c("\n");

  Vector<NodeGroupMembers> nodeGroupMembers;

  for (Uint32 n = 0; n < numNodes; n++) {
    Uint32 ng = nodeGroup[n];

    bool ngfound = false;
    for (Uint32 m = 0; m < nodeGroupMembers.size(); m++) {
      if (nodeGroupMembers[m].ngid == ng) {
        NodeGroupMembers &ngInfo = nodeGroupMembers[m];
        ngInfo.members[ngInfo.membCount++] = dynamicOrder[n];
        ngfound = true;
        break;
      }
    }

    if (!ngfound) {
      NodeGroupMembers newGroupInfo;
      newGroupInfo.ngid = ng;
      newGroupInfo.membCount = 1;
      newGroupInfo.members[0] = dynamicOrder[n];
      nodeGroupMembers.push_back(newGroupInfo);
    }
  }

  ndbout_c("Nodegroups");

  for (Uint32 n = 0; n < nodeGroupMembers.size(); n++) {
    ndbout << "  " << nodeGroupMembers[n].ngid << " (";
    bool allRemoteMonitored = true;
    for (Uint32 m = 0; m < nodeGroupMembers[n].membCount; m++) {
      Uint32 nodeId = nodeGroupMembers[n].members[m];
      ndbout << nodeId;
      if ((m + 1) < nodeGroupMembers[n].membCount) ndbout << ",";
      Uint32 dynamicIndex = nodeIdToDynamicIndex[nodeId];
      allRemoteMonitored &= remoteMonitored[dynamicIndex];
    }
    ndbout << ") Entirely remote monitored NGs risk : "
           << (allRemoteMonitored ? "Y" : "N") << "\n";
  }
  ndbout_c("\n");

  ndbout_c("Cluster-split latency behaviour");

  Uint32 oddPresident = dynamicOrder[0];
  Uint32 evenPresident = dynamicOrder[0];

  for (Uint32 n = 0; n <= maxDistanceToRemoteLink; n++) {
    Vector<Uint32> failedNodeGroups;
    ndbout << "  " << n << " HB latency period(s), nodes (";
    bool useComma = false;
    bool presidentFailed = false;
    for (Uint32 m = 0; m < numNodes; m++) {
      if (distanceToRemote[m] == n) {
        Uint32 failingNodeId = dynamicOrder[m];
        if (useComma) ndbout << ",";

        useComma = true;
        ndbout << failingNodeId;

        if ((failingNodeId == evenPresident) ||
            (failingNodeId == oddPresident)) {
          ndbout << "*";
          presidentFailed = true;
        }

        {
          Uint32 ng = nodeGroup[m];
          for (Uint32 i = 0; i < nodeGroupMembers.size(); i++) {
            if (nodeGroupMembers[i].ngid == ng) {
              if ((--nodeGroupMembers[i].membCount) == 0) {
                failedNodeGroups.push_back(ng);
              }
            }
          }
        }
      }
    }
    ndbout << ") will be declared failed." << endl;
    if (failedNodeGroups.size() != 0) {
      ndbout << "    NG failure risk on reconnect for nodegroups : ";
      for (Uint32 i = 0; i < failedNodeGroups.size(); i++) {
        if (i > 0) ndbout << ",";
        ndbout << failedNodeGroups[i];
      }
      ndbout << endl;
    }
    if (presidentFailed) {
      /* A president (even/odd/both) has failed, we should
       * calculate the new president(s) from the p.o.v.
       * of both sides
       */
      Uint32 newOdd = 0;
      Uint32 newEven = 0;
      for (Uint32 i = 0; i < numNodes; i++) {
        /* Each side finds either the first node on their
         * side, or the first node on the other side which
         * is still 'alive' from their point of view
         */
        bool candidateIsOdd = dynamicOrder[i] & 1;

        if (!newOdd) {
          if (candidateIsOdd || (distanceToRemote[i] > n)) {
            newOdd = dynamicOrder[i];
          }
        }
        if (!newEven) {
          if ((!candidateIsOdd) || (distanceToRemote[i] > n)) {
            newEven = dynamicOrder[i];
          }
        }
      }

      bool oddPresidentFailed = (oddPresident != newOdd);
      bool evenPresidentFailed = (evenPresident != newEven);

      if (oddPresidentFailed) {
        ndbout_c("    Odd president (%u) failed, new odd president : %u",
                 oddPresident, newOdd);
        oddPresident = newOdd;
      }
      if (evenPresidentFailed) {
        ndbout_c("    Even president (%u) failed, new even president : %u",
                 evenPresident, newEven);
        evenPresident = newEven;
      }

      if (oddPresident != evenPresident) {
        ndbout_c("    President role duplicated, Odd (%u), Even (%u)",
                 oddPresident, evenPresident);
      }
    }
  }

  ndbout << endl << endl;

  return NDBT_OK;
}

int runSplitLatency25PctFail(NDBT_Context *ctx, NDBT_Step *step) {
  /* Use dump commands to inject artificial inter-node latency
   * Use an error insert to cause latency to disappear when
   * a node observes > 25% of nodes failed.
   * This should trigger a race of FAIL_REQs from both sides
   * of the cluster, and can result in cluster failure
   */
  NdbRestarter restarter;
  Uint32 numNodes = restarter.getNumDbNodes();
  getNodeGroups(restarter);
  int num_replicas = (numNodes - numNoNodeGroups) / numNodeGroups;
  if (num_replicas != 2) {
    g_err << "[SKIPPED] Test skipped. Requires 2 replicas" << endl;
    return NDBT_SKIPPED;
  }

  /*
   * First set the ConnectCheckIntervalDelay to 1500
   */
  {
    int dump[] = {9994, 1500};
    restarter.dumpStateAllNodes(dump, 2);
  }

  {
    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    restarter.dumpStateAllNodes(val2, 2);
  }

  /* First the error insert which will drop latency (QMGR) */
  restarter.insertErrorInAllNodes(938);

  /* Now the dump code which causes the system to experience
   * latency along odd/even lines (CMVMI)
   *
   */
  int dumpStateArgs[] = {9990, 1};
  restarter.dumpStateAllNodes(dumpStateArgs, 2);

  /**
   * Now wait for half of cluster to die...
   */
  const int node_count = restarter.getNumDbNodes();
  ndbout_c("Waiting for half of cluster (%u/%u) to die", node_count / 2,
           node_count);
  int not_started = 0;
  do {
    not_started = 0;
    for (int i = 0; i < node_count; i++) {
      int nodeId = restarter.getDbNodeId(i);
      int status = restarter.getNodeStatus(nodeId);
      ndbout_c("Node %u status %u", nodeId, status);
      if (status == NDB_MGM_NODE_STATUS_NOT_STARTED) not_started++;
    }
    NdbSleep_MilliSleep(2000);
    ndbout_c("%u / %u in state NDB_MGM_NODE_STATUS_NOT_STARTED(%u)",
             not_started, node_count, NDB_MGM_NODE_STATUS_NOT_STARTED);
  } while (2 * not_started != node_count);

  ndbout_c("Restarting cluster");
  if (restarter.restartAll(false, true, true)) return NDBT_FAILED;

  ndbout_c("Waiting cluster not started");
  if (restarter.waitClusterNoStart()) return NDBT_FAILED;

  ndbout_c("Starting");
  if (restarter.startAll()) return NDBT_FAILED;

  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  return NDBT_OK;
}

/*
  The purpose of this test is to check that a node failure is not
  misdiagnosed as a GCP stop. In other words, the timeout set to detect
  GCP stop must not be set so low that they are triggered before a
  cascading node failure has been detected.
  The test isolates the master node. This causes the master node to
  wait for the heartbeat from each of the other nodes to time
  out. Note that this happens sequentially for each node. Finally, the
  master is forced to run an arbitration (by using an error
  insert). The total time needed to detect the node failures is thus:

  (no_of_nodes - 1) * heartbeat_failure_time + arbitration_time

  The test then verifies that the node failed due to detcting that is was
  isolated and not due to GCP stop.
*/
int runIsolateMaster(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  const unsigned nodeCount = restarter.getNumDbNodes();

  if (nodeCount < 4) {
    /*
      With just two nodes, the isolated master wins the arbitration and
      the test would behave very differently. This case is not covered.
     */
    g_err << "At least four data nodes required to run test." << endl;
    return NDBT_SKIPPED;
  }

  const int masterId = restarter.getMasterNodeId();

  g_err << "Inserting errors 943 and 7145 in node " << masterId << endl;
  /*
     There is a corresponding CRASH_INSERTION(943), so the node will
     be restarted if it crashes due to being isolated from other
     nodes. If it crashes due to GCP stop, however, it will remain
     down.  In addition, the 943 error insert forces the master to
     run an arbitration that times out, even if it is isolated.
  */
  restarter.insertErrorInNode(masterId, 943);

  /*
    This error inserts sets the GCP stop and micro GCP timeouts to
    their minimal value, i.e. only the maximal time needed to detect
    node failure. That way, the test verifies the latter value is not
    set to low.
   */
  restarter.insertErrorInNode(masterId, 7145);

  /*
    Block signals between the master node and all other nodes. The
    master will wait for heartbeats from other nodes to time out,
    sequentially for each node. Finally, the master should decide that
    it cannot form a viable cluster and stop itself.
   */
  for (unsigned i = 0; i < nodeCount; i++) {
    if (restarter.getDbNodeId(i) != masterId) {
      // Block signals from master node.
      g_err << "Blocking node " << restarter.getDbNodeId(i)
            << " for signals from node " << masterId << endl;
      const int dumpStateArgs[] = {9992, masterId};
      int res = restarter.dumpStateOneNode(restarter.getDbNodeId(i),
                                           dumpStateArgs, 2);
      (void)res;  // Prevent compiler warning.
      assert(res == 0);

      // Block signals to master node.
      g_err << "Blocking node " << masterId << " for signals from node "
            << restarter.getDbNodeId(i) << endl;
      const int dumpStateArgs2[] = {9992, restarter.getDbNodeId(i)};
      res = restarter.dumpStateOneNode(masterId, dumpStateArgs2, 2);
      (void)res;  // Prevent compiler warning.
      assert(res == 0);
    }
  }

  g_err << "Waiting for node " << masterId << " to restart " << endl;

  g_info << "Subscribing to MGMD events..." << endl;

  NdbMgmd mgmd;
  mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
  if (!mgmd.connect()) {
    g_err << "Failed to connect to MGMD" << endl;
    return NDBT_FAILED;
  }

  if (!mgmd.subscribe_to_events()) {
    g_err << "Failed to subscribe to events" << endl;
    return NDBT_FAILED;
  }

  char restartEventMsg[200];
  // This is the message we expect to see when the master restarts.
  sprintf(restartEventMsg, "Node %d: Node shutdown completed, restarting.",
          masterId);

  const NDB_TICKS start = NdbTick_getCurrentTicks();

  while (true) {
    char buff[1000];

    if (mgmd.get_next_event_line(buff, sizeof(buff), 5 * 1000) &&
        strstr(buff, restartEventMsg) != NULL) {
      g_err << "Node " << masterId << " restarting." << endl;
      break;
    }

    g_info << "Mgmd event: " << buff << endl;

    /**
     * Assume default heartbeatIntervalDbDb (= 5 seconds).
     * After missing four heartbeat intervals in a row, a node is declared dead.
     * Thus, the maximum time for discovering a failure through the heartbeat
     * mechanism is five times the heartbeat interval = 25 seconds.
     */
    if (NdbTick_Elapsed(start, NdbTick_getCurrentTicks()).seconds() >
        (25 * nodeCount)) {
      g_err << "Waited " << (25 * nodeCount)
            << " seconds for master to restart." << endl;
      return NDBT_FAILED;
    }
  }

  /*
    Now unblock outgoing signals from the master. Signals to the master will be
    unblocked automatically as it restarts.
  */

  for (unsigned i = 0; i < nodeCount; i++) {
    if (restarter.getDbNodeId(i) != masterId) {
      g_err << "Unblocking node " << restarter.getDbNodeId(i)
            << " for signals from node " << masterId << endl;
      const int dumpStateArgs[] = {9993, masterId};
      int res = restarter.dumpStateOneNode(restarter.getDbNodeId(i),
                                           dumpStateArgs, 2);
      (void)res;  // Prevent compiler warning.
      assert(res == 0);
    }
  }

  g_err << "Waiting for node " << masterId << " to come back up again." << endl;
  if (restarter.waitClusterStarted() == 0) {
    // All nodes are up.
    return NDBT_OK;
  } else {
    g_err << "Failed to restart master node!" << endl;
    return NDBT_FAILED;
  }
}

int runMasterFailSlowLCP(NDBT_Context *ctx, NDBT_Step *step) {
  /* Motivated by bug# 13323589 */
  NdbRestarter res;

  if (res.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    return NDBT_SKIPPED;
  }

  int master = res.getMasterNodeId();
  int otherVictim = res.getRandomNodePreferOtherNodeGroup(master, rand());
  int nextMaster = res.getNextMasterNodeId(master);
  nextMaster = (nextMaster == otherVictim)
                   ? res.getNextMasterNodeId(otherVictim)
                   : nextMaster;
  require(nextMaster != master);
  require(nextMaster != otherVictim);

  /* Get a node which is not current or next master */
  int slowNode = nextMaster;
  while ((slowNode == nextMaster) || (slowNode == otherVictim) ||
         (slowNode == master)) {
    slowNode = res.getRandomNotMasterNodeId(rand());
  }

  ndbout_c("master: %d otherVictim : %d nextMaster: %d slowNode: %d", master,
           otherVictim, nextMaster, slowNode);

  /* Steps :
   * 1. Insert slow LCP frag error in slowNode
   * 2. Start LCP
   * 3. Wait for LCP to start
   * 4. Kill at least two nodes including Master
   * 5. Wait for killed nodes to attempt to rejoin
   * 6. Remove slow LCP error
   * 7. Allow system to stabilise + check no errors
   */
  // 5073 = Delay on handling BACKUP_FRAGMENT_CONF in LQH
  if (res.insertErrorInNode(slowNode, 5073)) {
    return NDBT_FAILED;
  }

  {
    int req[1] = {DumpStateOrd::DihStartLcpImmediately};
    if (res.dumpStateOneNode(master, req, 1)) {
      return NDBT_FAILED;
    }
  }

  ndbout_c("Giving LCP time to start...");

  NdbSleep_SecSleep(10);

  ndbout_c("Killing other victim node (%u)...", otherVictim);

  if (res.restartOneDbNode(otherVictim, false, false, true)) {
    return NDBT_FAILED;
  }

  ndbout_c("Killing Master node (%u)...", master);

  if (res.restartOneDbNode(master, false, false, true)) {
    return NDBT_FAILED;
  }

  /*
     ndbout_c("Waiting for old Master node to enter NoStart state...");
     if (res.waitNodesNoStart(&master, 1, 10))
     return NDBT_FAILED;

     ndbout_c("Starting old Master...");
     if (res.startNodes(&master, 1))
     return NDBT_FAILED;

  */
  ndbout_c("Waiting for some progress on old Master and other victim restart");
  NdbSleep_SecSleep(15);

  ndbout_c("Now removing error insert on slow node (%u)", slowNode);

  if (res.insertErrorInNode(slowNode, 0)) {
    return NDBT_FAILED;
  }

  ndbout_c("Now wait a while to check stability...");
  NdbSleep_SecSleep(30);

  if (res.getNodeStatus(master) == NDB_MGM_NODE_STATUS_NOT_STARTED) {
    ndbout_c("Old Master needs kick to restart");
    if (res.startNodes(&master, 1)) {
      return NDBT_FAILED;
    }
  }

  ndbout_c("Wait for cluster recovery...");
  if (res.waitClusterStarted()) {
    return NDBT_FAILED;
  }

  ndbout_c("Done");
  return NDBT_OK;
}

/*
 Check that create big table and delete rows followed by node
 restart does not leak memory.

 See bugs,
 Bug #18683398 MEMORY LEAK DURING ROLLING RESTART
 Bug #18731008 NDB : AVOID MAPPING EMPTY PAGES DUE TO DELETES DURING NR
 */
int runDeleteRestart(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();

  if (runCreateBigTable(ctx, step) != NDBT_OK) {
    return NDBT_FAILED;
  }

  res.getNumDbNodes();  // will force it to connect...

  /**
   * Get memory usage
   */
  struct ndb_mgm_events *time0 =
      ndb_mgm_dump_events(res.handle, NDB_LE_MemoryUsage, 0, 0);
  if (!time0) {
    ndbout_c("ERROR: failed to fetch report!");
    return NDBT_FAILED;
    ;
  }

  printf("memory usage:\n");
  Uint32 t0_minpages = ~Uint32(0);
  Uint32 t0_maxpages = 0;
  for (int i = 0; i < time0->no_of_events; i++) {
    if (time0->events[i].MemoryUsage.block != DBTUP) continue;

    printf("node %u pages: %u\n", time0->events[i].source_nodeid,
           time0->events[i].MemoryUsage.pages_used);

    if (time0->events[i].MemoryUsage.pages_used < t0_minpages)
      t0_minpages = time0->events[i].MemoryUsage.pages_used;
    if (time0->events[i].MemoryUsage.pages_used > t0_maxpages)
      t0_maxpages = time0->events[i].MemoryUsage.pages_used;
  }

  /**
   * Stop one node
   */
  int node = res.getNode(NdbRestarter::NS_RANDOM);
  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node,
                           /** initial */ false,
                           /** nostart */ true,
                           /** abort   */ true))
    return NDBT_FAILED;

  if (res.waitNodesNoStart(&node, 1)) return NDBT_FAILED;

  /**
   * Then clear table it...
   */
  {
    BaseString name;
    name.assfmt("_%s", ctx->getTab()->getName());
    const NdbDictionary::Table *pTab = pDict->getTable(name.c_str());
    UtilTransactions trans(*pTab);
    trans.clearTable(GETNDB(step));
  }

  /**
   * Create a new big table...
   */
  ctx->setProperty("PREFIX", "2");
  if (runCreateBigTable(ctx, step) != NDBT_OK) return NDBT_FAILED;

  /**
   * Then start node
   */
  res.startNodes(&node, 1);
  res.waitClusterStarted();
  CHK_NDB_READY(GETNDB(step));

  /**
   * Get memory usage
   */
  struct ndb_mgm_events *time1 =
      ndb_mgm_dump_events(res.handle, NDB_LE_MemoryUsage, 0, 0);
  if (!time1) {
    ndbout_c("ERROR: failed to fetch report!");
    return NDBT_FAILED;
    ;
  }

  printf("memory usage:\n");
  Uint32 t1_minpages = ~Uint32(0);
  Uint32 t1_maxpages = 0;
  for (int i = 0; i < time1->no_of_events; i++) {
    if (time1->events[i].MemoryUsage.block != DBTUP) continue;

    printf("node %u pages: %u\n", time1->events[i].source_nodeid,
           time1->events[i].MemoryUsage.pages_used);

    if (time1->events[i].MemoryUsage.pages_used < t1_minpages)
      t1_minpages = time1->events[i].MemoryUsage.pages_used;
    if (time1->events[i].MemoryUsage.pages_used > t1_maxpages)
      t1_maxpages = time1->events[i].MemoryUsage.pages_used;
  }

  {  // Drop table 1
    BaseString name;
    name.assfmt("_%s", ctx->getTab()->getName());
    pDict->dropTable(name.c_str());
  }

  {  // Drop table 2
    BaseString name;
    name.assfmt("2_%s", ctx->getTab()->getName());
    pDict->dropTable(name.c_str());
  }

  /**
   * Verification...
   *   each node should have roughly the same now as before
   */
  bool ok = true;
  int maxpctdiff = 10;
  for (int i = 0; i < time0->no_of_events; i++) {
    if (time0->events[i].MemoryUsage.block != DBTUP) continue;

    unsigned node = time0->events[i].source_nodeid;
    for (int j = 0; j < time1->no_of_events; j++) {
      if (time1->events[j].MemoryUsage.block != DBTUP) continue;

      if (time1->events[j].source_nodeid != node) continue;

      int diff = time0->events[i].MemoryUsage.pages_used -
                 time1->events[j].MemoryUsage.pages_used;

      if (diff < 0) diff = -diff;

      int diffpct = 0;
      if (time0->events[i].MemoryUsage.pages_used > 0)
        diffpct = (100 * diff) / time0->events[i].MemoryUsage.pages_used;
      ndbout_c("node %u pages %u - %u => diff pct: %u%% (max: %u) => %s", node,
               time0->events[i].MemoryUsage.pages_used,
               time1->events[j].MemoryUsage.pages_used, diffpct, maxpctdiff,
               diffpct <= maxpctdiff ? "OK" : "FAIL");

      if (diffpct > maxpctdiff) ok = false;
      break;
    }
  }

  free(time0);
  free(time1);

  return ok ? NDBT_OK : NDBT_FAILED;
}

int master_err[] = {7025,  // LCP_FRG_REP in DIH
                    5056,  // LCP complete rep from LQH
                    7191,  // execLCP_COMPLETE_REP in DIH
                    7015,  // execSTART_LCP_CONF in DIH
                    0};

static struct {
  int errnum;
  bool obsolete;
} other_err[] = {
    {7205, false},  // execMASTER_LCPREQ
    {7206, true},   // execEMPTY_LCP_CONF (not in use since 7.4.3)
    {7230, false},  // sendMASTER_LCPCONF and die
    {7232, false},  // Die after sending MASTER_LCPCONF
    {0, false},
};

int runLCPTakeOver(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  if (res.getNumDbNodes() < 4) {
    g_err << "[SKIPPED] Test skipped. Requires at least 4 nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  for (int i = 0; master_err[i] != 0; i++) {
    int errno1 = master_err[i];
    for (int j = 0; other_err[j].errnum != 0; j++) {
      int errno2 = other_err[j].errnum;
      bool only_master_crash = other_err[j].obsolete;

      /**
       * we want to kill master,
       *   and kill another node during LCP take-ove (not new master)
       */
      NdbRestarter res;
      int master = res.getMasterNodeId();
      int next = res.getNextMasterNodeId(master);
    loop:
      int victim = res.getRandomNodePreferOtherNodeGroup(master, rand());
      while (next == victim) goto loop;

      ndbout_c("master: %u next: %u victim: %u master-err: %u victim-err: %u",
               master, next, victim, errno1, errno2);

      int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
      res.dumpStateOneNode(master, val2, 2);
      res.dumpStateOneNode(victim, val2, 2);
      res.insertErrorInNode(next, 7233);
      res.insertErrorInNode(victim, errno2);
      res.insertErrorInNode(master, errno1);

      int val1[] = {7099};
      res.dumpStateOneNode(master, val1, 1);
      int list[] = {master, victim};
      int cnt = NDB_ARRAY_SIZE(list);
      if (only_master_crash) {
        cnt = 1;
      }
      if (res.waitNodesNoStart(list, cnt)) {
        return NDBT_FAILED;
      }
      if (res.startNodes(list, cnt)) {
        return NDBT_FAILED;
      }
      if (res.waitClusterStarted()) {
        return NDBT_FAILED;
      }
      if (only_master_crash) {
        /*
         * Error set in victim should never be reached, so it will not
         * be cleared, nor node restarted.  Clearing error here after
         * test case succeeded.
         */
        res.insertErrorInNode(victim, 0);
      }
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug16007980(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  if (res.getNumNodeGroups() < 2) {
    g_err << "[SKIPPED] Test requires at least 2 node groups." << endl;
    return NDBT_SKIPPED;
  }
  if (res.getMaxConcurrentNodeFailures() < 2) {
    g_err << "[SKIPPED] Configuration cannot handle 2 node failures." << endl;
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();
  for (int i = 0; i < loops; i++) {
    int master = res.getMasterNodeId();
    int node1 = res.getRandomNodeSameNodeGroup(master, rand());
    int node2 = res.getRandomNodeOtherNodeGroup(master, rand());

    ndbout_c("master: %u node1: %u node2: %u", master, node1, node2);

    ndbout_c("restart node %u nostart", node2);
    res.restartNodes(&node2, 1,
                     NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
    CHECK(res.waitNodesNoStart(&node2, 1) == 0, "");

    ndbout_c("prepare node %u to crash while node %u is starting", node1,
             node2);
    ndbout_c("dump/error insert 939 into node %u", node1);
    int dump[] = {939, node2};
    res.dumpStateOneNode(node1, dump, NDB_ARRAY_SIZE(dump));

    ndbout_c("error insert 940 into node %u", node1);
    res.insertErrorInNode(node1, 940);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    res.dumpStateOneNode(node1, val2, 2);

    res.insertErrorInNode(node2, 932);  // Expect node 2 to crash with error 932
    res.dumpStateOneNode(node2, val2, 2);

    ndbout_c("starting node %u", node2);
    res.startNodes(&node2, 1);

    /**
     * Now both should have failed!
     */
    int list[] = {node1, node2};
    ndbout_c("waiting for node %u and %u nostart", node1, node2);
    CHECK(res.waitNodesNoStart(list, NDB_ARRAY_SIZE(list)) == 0, "");

    ndbout_c("starting %u and %u", node1, node2);
    res.startNodes(list, NDB_ARRAY_SIZE(list));

    ndbout_c("wait cluster started");
    CHECK(res.waitClusterStarted() == 0, "");
  }

  return NDBT_OK;
}

int runTestScanFragWatchdog(NDBT_Context *ctx, NDBT_Step *step) {
  /* Setup an error insert, then start a checkpoint */
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  do {
    g_err << "Injecting fault to suspend LCP frag scan..." << endl;
    Uint32 victim = restarter.getNode(NdbRestarter::NS_RANDOM);
    Uint32 otherNode = 0;
    do {
      otherNode = restarter.getNode(NdbRestarter::NS_RANDOM);
    } while (otherNode == victim);

    // Setting 'RestartOnErrorInsert = 2' will auto restart 'victim'
    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 2};
    if (restarter.dumpStateOneNode(victim, val2, 2) != 0) {
      g_err << "Failed setting dump state 'RestartOnErrorInsert'" << endl;
      break;
    }

    if (restarter.insertErrorInNode(victim, 10055) !=
        0) /* Cause LCP frag scan to halt */
    {
      g_err << "Error insert failed." << endl;
      break;
    }
    if (ctx->getProperty("WatchdogKillFail", Uint32(0))) {
      if (restarter.insertErrorInNode(victim, 5086) !=
          0) /* Disable watchdog kill */
      {
        g_err << "Error insert failed." << endl;
        break;
      }
      if (restarter.insertErrorInNode(victim, 942) !=
          0) /* Disable self-kill via Isolation */
      {
        g_err << "Error insert failed." << endl;
        break;
      }
      /* Can only be killed by others disconnecting me */
    } else {
      if (restarter.insertErrorInNode(victim, 5075) !=
          0) /* Treat watchdog fail as test success */
      {
        g_err << "Error insert failed." << endl;
        break;
      }
    }

    g_err << "Triggering LCP..." << endl;
    /* Now trigger LCP, in case the concurrent updates don't */
    {
      int startLcpDumpCode = 7099;
      if (restarter.dumpStateOneNode(victim, &startLcpDumpCode, 1)) {
        g_err << "Dump state failed." << endl;
        break;
      }
    }

    g_err << "Subscribing to MGMD events..." << endl;

    NdbMgmd mgmd;
    mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
    if (!mgmd.connect()) {
      g_err << "Failed to connect to MGMD" << endl;
      break;
    }

    if (!mgmd.subscribe_to_events()) {
      g_err << "Failed to subscribe to events" << endl;
      break;
    }

    g_err << "Waiting to hear of LCP completion..." << endl;
    Uint32 completedLcps = 0;
    Uint64 maxWaitSeconds = 240;
    Uint64 endTime = NdbTick_CurrentMillisecond() + (maxWaitSeconds * 1000);

    while (NdbTick_CurrentMillisecond() < endTime) {
      char buff[512];

      if (!mgmd.get_next_event_line(buff, sizeof(buff), 10 * 1000)) {
        g_err << "Failed to get event line " << endl;
        break;
      }

      // g_err << "Event : " << buff;

      if (strstr(buff, "Local checkpoint") && strstr(buff, "completed")) {
        completedLcps++;
        g_err << "LCP " << completedLcps << " completed." << endl;

        if (completedLcps == 2) break;

        /* Request + wait for another... */
        {
          int startLcpDumpCode = 7099;
          if (restarter.dumpStateOneNode(otherNode, &startLcpDumpCode, 1)) {
            g_err << "Dump state failed." << endl;
            break;
          }
        }
      }
    }

    if (completedLcps != 2) {
      g_err << "Some problem while waiting for LCP completion" << endl;
      break;
    }

    /* Now wait for the node to recover */
    if (restarter.waitNodesStarted((const int *)&victim, 1, 120) != 0) {
      g_err << "Failed waiting for node " << victim << "to start" << endl;
      break;
    }

    ctx->stopTest();
    return NDBT_OK;
  } while (0);

  ctx->stopTest();
  return NDBT_FAILED;
}

/**
 * The function remembers the old values such that they can be restored.
 * If the configuration doesn't contain any value then it will be restored
 * to 0 (which isn't generally correct, but correct for all current use
 * cases).
 */
static Uint32 setConfigValueAndRestartNode(NdbMgmd *mgmd, Uint32 *keys,
                                           Uint32 *values, Uint32 num_values,
                                           int nodeId, bool all_nodes,
                                           NdbRestarter *restarter,
                                           bool initial_nr) {
  g_err << "nodeId = " << nodeId << endl;
  // Get the binary config
  Config conf;
  if (!mgmd->get_config(conf)) {
    g_err << "Failed to get config from ndb_mgmd." << endl;
    return NDBT_FAILED;
  }
  // Set the key
  ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
  Uint32 oldValue[4];
  for (Uint32 i = 0; i < 4; i++) {
    oldValue[i] = 0;
  }
  require(num_values <= 4);
  bool first = true;
  for (int i = 0; i < MAX_NODES; i++) {
    if (!iter.openSection(CFG_SECTION_NODE, i)) continue;
    Uint32 nodeid;
    Uint32 node_type;
    iter.get(CFG_TYPE_OF_SECTION, &node_type);
    if (node_type != NODE_TYPE_DB) continue;
    iter.get(CFG_NODE_ID, &nodeid);
    if (all_nodes) {
      for (Uint32 i = 0; i < num_values; i++) {
        Uint32 prev_old_value = oldValue[i];
        if (iter.get(keys[i], &oldValue[i])) {
          iter.set(keys[i], values[i]);
        }
        if (!first && prev_old_value != oldValue[i]) {
          iter.closeSection();
          g_err << "Failed since node configs not equal" << endl;
          return NDBT_FAILED;
        }
        if (!first) {
          values[i] = oldValue[i];
        }
      }
      first = false;
      iter.closeSection();
    } else if ((int)nodeid == nodeId) {
      for (Uint32 i = 0; i < num_values; i++) {
        if (!iter.get(keys[i], &oldValue[i])) {
          oldValue[i] = 0;
        }
        g_info << "Set key " << keys[i] << " to " << values[i] << endl;
        g_info << "Node is " << nodeid << endl;
        require(iter.set(keys[i], values[i]));
        values[i] = oldValue[i];
      }
    }
    iter.closeSection();
  }
  // Set the modified config
  if (!mgmd->set_config(conf)) {
    g_err << "Failed to set config in ndb_mgmd." << endl;
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(5);  // Give MGM server time to restart
  g_err << "Restarting node " << nodeId << " to apply config change.." << endl;
  if (restarter->restartOneDbNode(nodeId, initial_nr, false, true)) {
    g_err << "Failed to restart node." << endl;
    return NDBT_FAILED;
  }
  if (restarter->waitNodesStarted(&nodeId, 1) != 0) {
    g_err << "Failed waiting for node started." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runChangeNumLogPartsINR(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }
  int node_1 = restarter.getDbNodeId(0);
  if (node_1 == -1) {
    g_err << "Failed to find node id of data node" << endl;
    return NDBT_FAILED;
  }
  NdbMgmd mgmd;
  Uint32 key;
  Uint32 value;
  key = CFG_DB_NO_REDOLOG_PARTS;

  mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
  if (!mgmd.connect()) {
    g_err << "Failed to connect to ndb_mgmd." << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  value = 8;
  if (setConfigValueAndRestartNode(&mgmd, &key, &value, 1, node_1, false,
                                   &restarter, true) == NDBT_FAILED) {
    g_err << "Failed to change first node to 8 log parts" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  Uint32 save_value = value;

  value = 6;
  if (setConfigValueAndRestartNode(&mgmd, &key, &value, 1, node_1, false,
                                   &restarter, true) == NDBT_FAILED) {
    g_err << "Failed to change first node to 6 log parts" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  if (setConfigValueAndRestartNode(&mgmd, &key, &save_value, 1, node_1, false,
                                   &restarter, true) == NDBT_FAILED) {
    g_err << "Failed to change first node to original log parts" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  ctx->stopTest();
  return NDBT_OK;
}

static int get_num_exec_threads(Ndb_cluster_connection *connection,
                                Uint32 nodeId) {
  NdbInfo ndbinfo(connection, "ndbinfo/");
  if (!ndbinfo.init()) {
    g_err << "ndbinfo.init failed" << endl;
    return -1;
  }

  const NdbInfo::Table *table;
  if (ndbinfo.openTable("ndbinfo/threads", &table) != 0) {
    g_err << "Failed to openTable(threads)" << endl;
    return -1;
  }

  NdbInfoScanOperation *scanOp = nullptr;
  if (ndbinfo.createScanOperation(table, &scanOp)) {
    g_err << "No NdbInfoScanOperation" << endl;
    ndbinfo.closeTable(table);
    return -1;
  }

  if (scanOp->readTuples() != 0) {
    g_err << "scanOp->readTuples failed" << endl;
    ndbinfo.releaseScanOperation(scanOp);
    ndbinfo.closeTable(table);
    return -1;
  }

  const NdbInfoRecAttr *node_id_col = scanOp->getValue("node_id");
  const NdbInfoRecAttr *thr_no_col = scanOp->getValue("thr_no");

  if (scanOp->execute() != 0) {
    g_err << "scanOp->execute failed" << endl;
    ndbinfo.releaseScanOperation(scanOp);
    ndbinfo.closeTable(table);
    return -1;
  }

  bool found_node_id = false;
  Uint32 thread_no = 0;
  // Iterate through the result list
  do {
    const int scan_next_result = scanOp->nextResult();
    if (scan_next_result == -1) {
      g_err << "Failure to process ndbinfo records" << endl;
      ndbinfo.releaseScanOperation(scanOp);
      ndbinfo.closeTable(table);
      return -1;
    } else if (scan_next_result == 0) {
      // All ndbinfo records processed
      ndbinfo.releaseScanOperation(scanOp);
      ndbinfo.closeTable(table);
      if (!found_node_id) return 0;
      if (thread_no == 0)
        g_err << "Single threaded data node" << endl;
      else
        g_err << "Multi threaded data node" << endl;
      return thread_no + 1;

    } else {
      // Check thread_no of records from given nodeId
      const Uint32 node_id_record = node_id_col->u_32_value();
      if (node_id_record != nodeId) continue;
      found_node_id = true;
      thread_no = thr_no_col->u_32_value();
    }
  } while (true);
}

int runChangeNumLDMsNR(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }
  int node_1 = restarter.getDbNodeId(0);
  int node_2 = restarter.getDbNodeId(1);
  if (node_1 == -1 || node_2 == -1) {
    g_err << "Failed to find node ids of data nodes" << endl;
    return NDBT_FAILED;
  }

  int node1_no_threads =
      get_num_exec_threads(&ctx->m_cluster_connection, node_1);
  int node2_no_threads =
      get_num_exec_threads(&ctx->m_cluster_connection, node_2);
  g_err << node_1 << " " << node1_no_threads << endl;
  g_err << node_2 << " " << node2_no_threads << endl;

  if (node1_no_threads < 2 || node2_no_threads < 2) {
    g_err << "[SKIPPED] Test is useful only for clusters running multi threaded"
             "data node (ndbmtd)"
          << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }
  NdbMgmd mgmd;
  Uint32 keys[2];
  Uint32 values[2];
  Uint32 save_values_first[2];
  Uint32 save_values_second[2];
  keys[0] = CFG_DB_AUTO_THREAD_CONFIG;
  keys[1] = CFG_DB_NUM_CPUS;

  mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
  if (!mgmd.connect()) {
    g_err << "Failed to connect to ndb_mgmd." << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  values[0] = 1;
  values[1] = 16;
  if (setConfigValueAndRestartNode(&mgmd, &keys[0], &values[0], 2, node_1,
                                   false, &restarter, false) == NDBT_FAILED) {
    g_err << "Failed to change first node" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  save_values_first[0] = values[0];
  save_values_first[1] = values[1];
  values[0] = 1;
  values[1] = 16;
  if (setConfigValueAndRestartNode(&mgmd, &keys[0], &values[0], 2, node_2,
                                   false, &restarter, false) == NDBT_FAILED) {
    g_err << "Failed to change second node" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  save_values_second[0] = values[0];
  save_values_second[1] = values[1];
  for (Uint32 test_index = 0; test_index < 8; test_index++) {
    switch (test_index) {
      case 0: {
        values[0] = 1;
        values[1] = 2;
        break;
      }
      case 1: {
        values[0] = 1;
        values[1] = 4;
        break;
      }
      case 2: {
        values[0] = 1;
        values[1] = 8;
        break;
      }
      case 3: {
        values[0] = 1;
        values[1] = 16;
        break;
      }
      case 4: {
        values[0] = 1;
        values[1] = 24;
        break;
      }
      case 5: {
        values[0] = 1;
        values[1] = 30;
        break;
      }
      case 6: {
        values[0] = 1;
        values[1] = 20;
        break;
      }
      case 7: {
        values[0] = 1;
        values[1] = 10;
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    if (setConfigValueAndRestartNode(&mgmd, &keys[0], &values[0], 2, node_2,
                                     false, &restarter, false) == NDBT_FAILED) {
      g_err << "Failed to change second node, step " << test_index << endl;
      ctx->stopTest();
      return NDBT_FAILED;
    }
  }
  int ret_code =
      setConfigValueAndRestartNode(&mgmd, &keys[0], &save_values_first[0], 2,
                                   node_1, false, &restarter, false);
  if (ret_code == NDBT_FAILED) {
    g_err << "Failed to change back first node" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  ret_code =
      setConfigValueAndRestartNode(&mgmd, &keys[0], &save_values_second[0], 2,
                                   node_2, false, &restarter, false);
  if (ret_code == NDBT_FAILED) {
    g_err << "Failed to change back second node" << endl;
    ctx->stopTest();
    return NDBT_FAILED;
  }
  ctx->stopTest();
  return NDBT_OK;
}

int runTestScanFragWatchdogDisable(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }
  Uint32 lcp_watchdog_limit = 0;
  int victim = restarter.getNode(NdbRestarter::NS_RANDOM);
  do {
    NdbMgmd mgmd;
    mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
    if (!mgmd.connect()) {
      g_err << "Failed to connect to ndb_mgmd." << endl;
      break;
    }
    g_err << "Disabling LCP frag scan watchdog..." << endl;

    // to disable the LCP frag scan watchdog, set
    // CFG_DB_LCP_SCAN_WATCHDOG_LIMIT = 0
    lcp_watchdog_limit = 0;
    Uint32 key = CFG_DB_LCP_SCAN_WATCHDOG_LIMIT;
    if (setConfigValueAndRestartNode(&mgmd, &key, &lcp_watchdog_limit, 1,
                                     victim, true, &restarter,
                                     false) == NDBT_FAILED)
      break;

    g_err << "Injecting fault in node " << victim;
    g_err << " to suspend LCP frag scan..." << endl;
    if (restarter.insertErrorInNode(victim, 10055) != 0) {
      g_err << "Error insert failed." << endl;
      break;
    }

    g_err << "Creating table for LCP frag scan..." << endl;
    runLoadTable(ctx, step);

    g_err << "Triggering LCP..." << endl;
    {
      int startLcpDumpCode = 7099;
      if (restarter.dumpStateAllNodes(&startLcpDumpCode, 1)) {
        g_err << "Dump state failed." << endl;
        break;
      }
    }

    if (!mgmd.subscribe_to_events()) {
      g_err << "Failed to subscribe to mgmd events." << endl;
      break;
    }

    g_err << "Waiting for activity from LCP Frag watchdog..." << endl;
    Uint64 maxWaitSeconds = 240;
    Uint64 endTime = NdbTick_CurrentMillisecond() + (maxWaitSeconds * 1000);
    int result = NDBT_OK;
    while (NdbTick_CurrentMillisecond() < endTime) {
      char buff[512];

      if (!mgmd.get_next_event_line(buff, sizeof(buff), 10 * 1000)) {
        g_err << "Failed to get event line." << endl;
        result = NDBT_FAILED;
        break;
      }
      if (strstr(buff, "Local checkpoint") && strstr(buff, "completed")) {
        g_err << "Failed to disable LCP Frag watchdog." << endl;
        result = NDBT_FAILED;
        break;
      }
    }
    if (result == NDBT_FAILED) break;

    g_err << "No LCP activity: LCP Frag watchdog successfully disabled..."
          << endl;
    g_err << "Restoring default LCP Frag watchdog config..." << endl;
    if (setConfigValueAndRestartNode(&mgmd, &key, &lcp_watchdog_limit, 1,
                                     victim, true, &restarter,
                                     false) == NDBT_FAILED)
      break;

    ctx->stopTest();
    return NDBT_OK;
  } while (0);

  // Insert error code to resume LCP in case node halted
  if (restarter.insertErrorInNode(victim, 0) != 0) {
    g_err << "Test cleanup failed: failed to resume LCP." << endl;
  }
  ctx->stopTest();
  return NDBT_FAILED;
}

int runBug16834416(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int loops = ctx->getNumLoops();
  for (int i = 0; i < loops; i++) {
    ndbout_c("running big trans");
    HugoOperations ops(*ctx->getTab());
    ops.startTransaction(pNdb);
    ops.pkInsertRecord(0, 1024);  // 1024 rows
    ops.execute_NoCommit(pNdb, AO_IgnoreError);

    // TC node id
    Uint32 nodeId = ops.getTransaction()->getConnectedNodeId();

    int errcode = 8054;
    ndbout_c("TC: %u => kill kill kill (error: %u)", nodeId, errcode);

    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    restarter.dumpStateOneNode(nodeId, val2, 2);
    restarter.insertErrorInNode(nodeId, errcode);

    ops.execute_Commit(pNdb, AO_IgnoreError);

    int victim = (int)nodeId;
    restarter.waitNodesNoStart(&victim, 1);
    restarter.startAll();
    restarter.waitClusterStarted();
    CHK_NDB_READY(pNdb);

    ops.closeTransaction(pNdb);
    ops.clearTable(pNdb);

    int val3[] = {4003};  // Check TC/LQH CommitAckMarker leak
    restarter.dumpStateAllNodes(val3, 1);
  }

  restarter.insertErrorInAllNodes(0);
  return NDBT_OK;
}

enum LCPFSStopCases { NdbFsError1, NdbFsError2, NUM_CASES };

int runTestLcpFsErr(NDBT_Context *ctx, NDBT_Step *step) {
  /* Setup an error insert, then start a checkpoint */
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  g_err << "Subscribing to MGMD events..." << endl;

  int filter[] = {15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0};
  NdbLogEventHandle handle =
      ndb_mgm_create_logevent_handle(restarter.handle, filter);

  int scenario = NdbFsError1;
  bool failed = false;

  do {
    g_err << "Injecting fault " << scenario << " to suspend LCP frag scan..."
          << endl;
    Uint32 victim = restarter.getNode(NdbRestarter::NS_RANDOM);
    Uint32 otherNode = 0;
    do {
      otherNode = restarter.getNode(NdbRestarter::NS_RANDOM);
    } while (otherNode == victim);

    // Setting 'RestartOnErrorInsert = 2' will auto restart 'victim'
    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 2};
    if (restarter.dumpStateOneNode(victim, val2, 2) != 0) {
      g_err << "Failed setting dump state 'RestartOnErrorInsert'" << endl;
      break;
    }

    bool failed = false;
    Uint32 lcpsRequired = 2;
    switch (scenario) {
      case NdbFsError1: {
        if (restarter.insertErrorInNode(victim, 10044) != 0) {
          g_err << "Error insert 10044 failed." << endl;
          failed = true;
        }
        lcpsRequired = 6;
        break;
      }
      case NdbFsError2: {
        if (restarter.insertErrorInNode(victim, 10045) != 0) {
          g_err << "Error insert 10045 failed." << endl;
          failed = true;
        }
        lcpsRequired = 6;
        break;
      }
    }
    if (failed) break;

    g_err << "Triggering LCP..." << endl;
    /* Now trigger LCP, in case the concurrent updates don't */
    {
      int startLcpDumpCode = 7099;
      if (restarter.dumpStateOneNode(victim, &startLcpDumpCode, 1)) {
        g_err << "Dump state failed." << endl;
        break;
      }
    }

    g_err << "Waiting to hear of LCP completion..." << endl;
    Uint32 completedLcps = 0;
    Uint64 maxWaitSeconds = (120 * lcpsRequired);
    Uint64 endTime = NdbTick_CurrentMillisecond() + (maxWaitSeconds * 1000);
    struct ndb_logevent event;

    do {
      while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
             event.type != NDB_LE_LocalCheckpointStarted &&
             NdbTick_CurrentMillisecond() < endTime)
        ;
      while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
             event.type != NDB_LE_LocalCheckpointCompleted &&
             NdbTick_CurrentMillisecond() < endTime)
        ;

      if (NdbTick_CurrentMillisecond() >= endTime) break;

      completedLcps++;
      g_err << "LCP " << completedLcps << " completed." << endl;

      if (completedLcps == lcpsRequired) break;

      /* Request + wait for another... */
      {
        int startLcpDumpCode = 7099;
        if (restarter.dumpStateOneNode(otherNode, &startLcpDumpCode, 1)) {
          g_err << "Dump state failed." << endl;
          break;
        }
      }
    } while (1);

    if (completedLcps != lcpsRequired) {
      g_err << "Some problem while waiting for LCP completion" << endl;
      break;
    }

    /* Now wait for the node to recover */
    g_err << "Waiting for all nodes to be started..." << endl;
    if (restarter.waitNodesStarted((const int *)&victim, 1, 120) != 0) {
      g_err << "Failed waiting for node " << victim << "to start" << endl;
      break;
    }

    restarter.insertErrorInAllNodes(0);

    {
      Uint32 count = 0;
      g_err << "Consuming intervening mgmapi events..." << endl;
      while (ndb_logevent_get_next(handle, &event, 10) != 0) count++;

      g_err << count << " events consumed." << endl;
    }
  } while (!failed && ++scenario < NUM_CASES);

  ctx->stopTest();

  if (failed)
    return NDBT_FAILED;
  else
    return NDBT_OK;
}

int runDelayedNodeFail(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int i = 0;
  int victim = restarter.getNode(NdbRestarter::NS_RANDOM);
  while (i < 2 && !ctx->isTestStopped()) {
    /* Wait a moment or two */
    ndbout_c("Waiting 20 seconds...");
    NdbSleep_SecSleep(20);
    ndbout_c("Restart node: %d", victim);
    if (restarter.insertErrorInNode(victim, 7008) != 0) {
      g_err << "Error insert 7008 failed." << endl;
      ctx->stopTest();
      return NDBT_FAILED;
    }
    g_err << "Waiting for node " << victim << " to die" << endl;
    restarter.waitNodesNoStart(&victim, 1);
    ndbout_c("  start node");
    if (restarter.startNodes(&victim, 1) != 0) {
      g_err << "startNodes failed" << endl;
      ctx->stopTest();
      return NDBT_FAILED;
    }
    ndbout_c("Wait for cluster to start up again");
    if (restarter.waitClusterStarted() != 0) {
      g_err << "waitClusterStarted failed" << endl;
      ctx->stopTest();
      return NDBT_FAILED;
    }
    ndbout_c("Cluster up again");
    i++;
  }
  ndbout_c("Stop test");
  ctx->stopTest();
  return NDBT_OK;
}

int runNodeFailGCPOpen(NDBT_Context *ctx, NDBT_Step *step) {
  /* Use an error insert to cause node failures,
   * then bring the cluster back up
   */
  NdbRestarter restarter;
  int numDbNodes = restarter.getNumDbNodes();
  getNodeGroups(restarter);
  int num_replicas = (numDbNodes - numNoNodeGroups) / numNodeGroups;
  if (num_replicas != 2) {
    g_err << "[SKIPPED] Test skipped. Requires 2 replicas" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  int i = 0;
  while (i < 10 && !ctx->isTestStopped()) {
    /* Wait a moment or two */
    ndbout_c("Waiting...");
    NdbSleep_SecSleep(10);
    /* Insert error in all nodes */
    ndbout_c("Inserting error...");
    restarter.insertErrorInAllNodes(8098);

    /* Wait for failure... */
    ndbout_c("Waiting to hear of node failure %u...", i);
    int timeout = 120;
    while ((restarter.waitClusterStarted(1) == 0) && timeout--)
      ;

    if (timeout == 0) {
      g_err << "Timed out waiting for node failure" << endl;
    }

    ndbout_c("Clearing error...");
    restarter.insertErrorInAllNodes(0);

    ndbout_c("Waiting for node recovery...");
    timeout = 120;
    while ((restarter.waitClusterStarted(1) != 0) &&
           (restarter.startAll() == 0) && timeout--)
      ;

    ndbout_c("Done.");

    if (timeout == 0) {
      g_err << "Timed out waiting for recovery" << endl;
      return NDBT_FAILED;
    }

    if (restarter.waitClusterStarted(1) != 0) {
      g_err << "Failed waiting for cluster to start." << endl;
      return NDBT_FAILED;
    }
    i++;
  }

  ctx->stopTest();

  return NDBT_OK;
}

static void callback(int retCode, NdbTransaction *trans, void *ptr) {}

int runBug16944817(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2) {
    g_err << "[SKIPPED] Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

#ifndef NDEBUG
  /**
   * This program doesn't work with debug compiled due
   * due various asserts...which are correct...
   */
  {
    ctx->stopTest();
    return NDBT_OK;
  }
#endif

  const int loops = ctx->getNumLoops();
  for (int i = 0; i < loops; i++) {
    ndbout_c("loop %u/%u", (i + 1), loops);
    Ndb *pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
    if (pNdb->init() != 0 || pNdb->waitUntilReady(30)) {
      delete pNdb;
      return NDBT_FAILED;
    }

    ndbout_c("  start trans");
    HugoOperations hugoOps(*ctx->getTab());
    if (hugoOps.startTransaction(pNdb) != 0) return NDBT_FAILED;

    if (hugoOps.pkInsertRecord(pNdb, i, 1, rand()) != 0) return NDBT_FAILED;

    if (hugoOps.execute_NoCommit(pNdb) != 0) return NDBT_FAILED;

    NdbTransaction *pTrans = hugoOps.getTransaction();
    hugoOps.setTransaction(0, true);

    ndbout_c("  executeAsynchPrepare");
    pTrans->executeAsynchPrepare(Commit, callback, 0, AbortOnError);

    int nodeId = pTrans->getConnectedNodeId();
    ndbout_c("  insert error 8054 into %d", nodeId);
    restarter.insertErrorInNode(nodeId, 8054);
    int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    if (restarter.dumpStateOneNode(nodeId, val2, 2)) return NDBT_FAILED;

    ndbout_c("  sendPreparedTransactions");
    const int forceSend = 1;
    pNdb->sendPreparedTransactions(forceSend);

    /**
     * Now delete ndb-object with having heard reply from commit
     */
    ndbout_c("  delete pNdb");
    delete pNdb;

    /**
     * nodeId will die due to errorInsert 8054 above
     */
    ndbout_c("  wait nodes no start");
    restarter.waitNodesNoStart(&nodeId, 1);
    ndbout_c("  start nodes");
    restarter.startNodes(&nodeId, 1);
    ndbout_c("  wait nodes started");
    restarter.waitNodesStarted(&nodeId, 1);

    /**
     * restart it again...will cause duplicate marker (before bug fix)
     */
    ndbout_c("  restart (again)");
    restarter.restartNodes(
        &nodeId, 1, NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
    ndbout_c("  wait nodes no start");
    restarter.waitNodesNoStart(&nodeId, 1);
    ndbout_c("  start nodes");
    restarter.startNodes(&nodeId, 1);
    ndbout_c("  wait nodes started");
    restarter.waitClusterStarted();
  }

  bool checkMarkers = true;

  if (checkMarkers) {
    ndbout_c("and finally...check markers");
    int check = 2552;  // check that no markers are leaked
    restarter.dumpStateAllNodes(&check, 1);
  }

  return NDBT_OK;
}

#define CHK2(b, e)                                                        \
  if (!(b)) {                                                             \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ << ": " << e \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    break;                                                                \
  }

int runBug16766493(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  const int records = ctx->getNumRecords();
  char *tabname = strdup(ctx->getTab()->getName());
  int result = NDBT_OK;
  ndb_srand(NdbHost_GetProcessId());
  NdbRestarter restarter;
  (void)pDic->dropTable(tabname);  // replace table

  do {
    NdbDictionary::Table tab;
    tab.setName(tabname);
    tab.setTablespaceName("DEFAULT-TS");
    {
      NdbDictionary::Column c;
      c.setName("A");
      c.setType(NdbDictionary::Column::Unsigned);
      c.setPrimaryKey(true);
      tab.addColumn(c);
    }
    /*
     * Want big DD column which does not fit evenly into 32k UNDO
     * buffer i.e. produces big NOOP entries.  The bug was reported
     * in 7.2 for longblob where part size is 13948.  This will do.
     */
    {
      NdbDictionary::Column c;
      c.setName("B");
      c.setType(NdbDictionary::Column::Char);
      c.setLength(13948);
      c.setNullable(false);
      c.setStorageType(NdbDictionary::Column::StorageTypeDisk);
      tab.addColumn(c);
    }
    {
      NdbDictionary::Column c;  // for hugo
      c.setName("C");
      c.setType(NdbDictionary::Column::Unsigned);
      c.setNullable(false);
      tab.addColumn(c);
    }

    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
    const NdbDictionary::Table *pTab;
    CHK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());
    HugoTransactions trans(*pTab);

    if (loops <= 1)
      g_err << "note: test is not useful for loops=" << loops << endl;
    for (int loop = 0; loop < loops; loop++) {
      g_info << "loop: " << loop << endl;
      CHK2(trans.loadTable(pNdb, records) == 0, trans.getNdbError());
      if (loop + 1 == loops) break;  // leave rows for verify
      while (1) {
        g_info << "clear table" << endl;
#if 0
        if (trans.clearTable(pNdb, records) == 0)
          break;
#else  // nicer for debugging
        if (trans.pkDelRecords(pNdb, records, records) == 0) break;
#endif
        const NdbError &err = trans.getNdbError();
        // hugo does not return error code on max tries
        CHK2(err.code == 0, err);
#if 0  // can cause ndbrequire in exec_lcp_frag_ord
        int lcp = 7099;
        CHK2(restarter.dumpStateAllNodes(&lcp, 1) == 0, "-");
#endif
        const int timeout = 5;
        CHK2(restarter.waitClusterStarted(timeout) == 0, "-");
        CHK_NDB_READY(pNdb);
        g_info << "assume UNDO overloaded..." << endl;
        NdbSleep_MilliSleep(1000);
      }
      CHK2(result == NDBT_OK, "-");
    }
    CHK2(result == NDBT_OK, "-");

    g_info << "verify records" << endl;
    CHK2(trans.scanReadRecords(pNdb, records) == 0, trans.getNdbError());

    // test that restart works
    g_info << "restart" << endl;
    const bool initial = false;
    const bool nostart = true;
    CHK2(restarter.restartAll(initial, nostart) == 0, "-");
    CHK2(restarter.waitClusterNoStart() == 0, "-");
    g_info << "nostart done" << endl;
    CHK2(restarter.startAll() == 0, "-");
    CHK2(restarter.waitClusterStarted() == 0, "-");
    CHK_NDB_READY(pNdb);
    g_info << "restart done" << endl;

    g_info << "verify records" << endl;
    CHK2(trans.scanReadRecords(pNdb, records) == 0, trans.getNdbError());
  } while (0);

  if (result != NDBT_OK) abort();
  free(tabname);
  return result;
}

/* Bug16895311 */

struct Bug16895311 {
  struct Row {
    int bytelen;
    int chrlen;
    uchar *data;
    bool exist;
    Row() {
      bytelen = -1;
      chrlen = -1;
      data = 0;
      exist = false;
    }
  };
  const char *tabname;
  int maxbytelen;
  CHARSET_INFO *cs;
  const NdbDictionary::Table *pTab;
  int records;
  Row *rows;
  Bug16895311() {
    tabname = "tBug16895311";
    maxbytelen = 0;
    cs = 0;
    pTab = 0;
    records = 0;
    rows = 0;
  }
};

static Bug16895311 bug16895311;

int runBug16895311_create(NDBT_Context *ctx, NDBT_Step *step) {
  Bug16895311 &bug = bug16895311;
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDic = pNdb->getDictionary();
  int result = 0;
  ndb_srand(NdbHost_GetProcessId());
  do {
    (void)pDic->dropTable(bug.tabname);
    NdbDictionary::Table tab;
    tab.setName(bug.tabname);
    const char *csname = "utf8mb3_unicode_ci";
    bug.cs = get_charset_by_name(csname, MYF(0));
    require(bug.cs != 0);
    // can hit too small xfrm buffer in 2 ways
    // ndbrequire line numbers are from 7.1 revno: 4997
    if (ndb_rand() % 100 < 50)
      bug.maxbytelen = 255 * 3;  // line 732
    else
      bug.maxbytelen = MAX_KEY_SIZE_IN_WORDS * 4 - 2;  // line 1862
    g_err << "char key: maxbytelen=" << bug.maxbytelen << endl;
    {
      NdbDictionary::Column c;
      c.setName("a");
      c.setType(NdbDictionary::Column::Longvarchar);
      c.setCharset(bug.cs);
      c.setLength(bug.maxbytelen);
      c.setNullable(false);
      c.setPrimaryKey(true);
      tab.addColumn(c);
    }
    CHK2(pDic->createTable(tab) == 0, pDic->getNdbError());
    CHK2((bug.pTab = pDic->getTable(bug.tabname)) != 0, pDic->getNdbError());
    // allocate rows
    bug.records = ctx->getNumRecords();
    bug.rows = new Bug16895311::Row[bug.records];
  } while (0);
  return result;
}

void doBug16895311_data(int i) {
  Bug16895311 &bug = bug16895311;
  require(0 <= i && i < bug.records);
  Bug16895311::Row &row = bug.rows[i];
  const uchar chr[][3] = {
      {0xE2, 0x82, 0xAC},  // U+20AC
      {0xE2, 0x84, 0xB5},  // U+2135
      {0xE2, 0x88, 0xAB}   // U+222B
  };
  const int chrcnt = sizeof(chr) / sizeof(chr[0]);
  while (1) {
    if (row.data != 0) delete[] row.data;
    int len;
    if (ndb_rand() % 100 < 50)
      len = bug.maxbytelen;
    else
      len = ndb_rand() % (bug.maxbytelen + 1);
    row.chrlen = len / 3;
    row.bytelen = row.chrlen * 3;
    row.data = new uchar[2 + row.bytelen];
    row.data[0] = uint(row.bytelen) & 0xFF;
    row.data[1] = uint(row.bytelen) >> 8;
    for (int j = 0; j < row.chrlen; j++) {
      int k = ndb_rand() % chrcnt;
      memcpy(&row.data[2 + j * 3], chr[k], 3);
    }
    int not_used;
    int wflen = (int)(*bug.cs->cset->well_formed_len)(
        bug.cs, (const char *)&row.data[2],
        (const char *)&row.data[2] + row.bytelen, row.chrlen, &not_used);
    require(wflen == row.bytelen);
    bool dups = false;
    for (int i2 = 0; i2 < bug.records; i2++) {
      if (i2 != i) {
        Bug16895311::Row &row2 = bug.rows[i2];
        if (row2.exist && row2.bytelen == row.bytelen &&
            memcmp(row2.data, row.data, 2 + row.bytelen) == 0) {
          dups = true;
          break;
        }
      }
    }
    if (dups) continue;
    break;
  }
  require(row.data != 0);
}

int doBug16895311_op(Ndb *pNdb, const char *op, int i) {
  Bug16895311 &bug = bug16895311;
  int result = NDBT_OK;
  require(strcmp(op, "I") == 0 || strcmp(op, "D") == 0);
  Bug16895311::Row &row = bug.rows[i];
  int tries = 0;
  while (1) {
    tries++;
    Uint32 acol = 0;
    const char *aval = (const char *)row.data;
    require(aval != 0);
    NdbTransaction *pTx = 0;
    CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
    NdbOperation *pOp = 0;
    CHK2((pOp = pTx->getNdbOperation(bug.pTab)) != 0, pTx->getNdbError());
    if (*op == 'I') {
      CHK2(pOp->insertTuple() == 0, pOp->getNdbError());
    }
    if (*op == 'D') {
      CHK2(pOp->deleteTuple() == 0, pOp->getNdbError());
    }
    CHK2(pOp->equal(acol, aval) == 0, pOp->getNdbError());
    int ret = pTx->execute(NdbTransaction::Commit);
    if (ret != 0) {
      const NdbError &error = pTx->getNdbError();
      g_info << "i=" << i << " op=" << op << ": " << error << endl;
      CHK2(error.status == NdbError::TemporaryError, error);
      CHK2(tries < 100, error << ": tries=" << tries);
      NdbSleep_MilliSleep(100);
      pNdb->closeTransaction(pTx);
      continue;
    }
    pNdb->closeTransaction(pTx);
    if (*op == 'I') {
      require(!row.exist);
      row.exist = true;
    }
    if (*op == 'D') {
      require(row.exist);
      row.exist = false;
    }
    break;
  }
  return result;
}

int runBug16895311_load(NDBT_Context *ctx, NDBT_Step *step) {
  Bug16895311 &bug = bug16895311;
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  for (int i = 0; i < bug.records; i++) {
    doBug16895311_data(i);
    CHK2(doBug16895311_op(pNdb, "I", i) == 0, "-");
  }
  return result;
}

int runBug16895311_update(NDBT_Context *ctx, NDBT_Step *step) {
  Bug16895311 &bug = bug16895311;
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  int i = 0;
  while (!ctx->isTestStopped()) {
    // the delete/insert can turn into update on recovering node
    // TODO: investigate what goes on
    CHK2(doBug16895311_op(pNdb, "D", i) == 0, "-");
    CHK2(doBug16895311_op(pNdb, "I", i) == 0, "-");
    i++;
    if (i >= bug.records) i = 0;
  }
  return result;
}

int runBug16895311_drop(NDBT_Context *ctx, NDBT_Step *step) {
  Bug16895311 &bug = bug16895311;
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDic = pNdb->getDictionary();
  int result = 0;
  do {
    CHK2(pDic->dropTable(bug.tabname) == 0, pDic->getNdbError());
    // free rows
    delete[] bug.rows;
    bug.rows = 0;
  } while (0);
  return result;
}

int runBug18044717(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  NdbRestarter restarter;
  int master = restarter.getMasterNodeId();

  do {
    ndbout_c("slow down LCP so that global c_lcpStatus = LCP_INIT_TABLES");
    ndbout_c("and all tables have tabLcpStatus = TLS_ACTIVE");
    if (restarter.insertErrorInAllNodes(7236)) {
      result = NDBT_FAILED;
      break;
    }

    ndbout_c("start LCP");
    int startLcpDumpCode = 7099;
    if (restarter.dumpStateAllNodes(&startLcpDumpCode, 1)) {
      result = NDBT_FAILED;
      break;
    }

    ndbout_c("restart master node so that NODE_FAILREP changes");
    ndbout_c("c_lcpState from LCP_INIT_TABLES to LCP_STATUS_IDLE");
    if (restarter.restartOneDbNode(master, false, false, true, true) != 0) {
      result = NDBT_FAILED;
      break;
    }
  } while (0);
  ndbout_c("restore original state of cluster and verify that there");
  ndbout_c("is no core due to inconsistent c_lcpStatus/tabLcpStatus");

  if (restarter.waitNodesStarted(&master, 1)) {
    ndbout_c("master node failed to start");
    return NDBT_FAILED;
  }

  if (restarter.insertErrorInAllNodes(0)) {
    result = NDBT_FAILED;
  }
  return result;
}

int runRestartAllNodes(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  CHECK(restarter.restartAll() == 0, "-");
  CHECK(restarter.waitClusterNoStart() == 0, "-");
  CHECK(restarter.startAll() == 0, "-");
  CHECK(restarter.waitClusterStarted() == 0, "-");
  CHK_NDB_READY(GETNDB(step));
  return NDBT_OK;
}

static int createEvent(Ndb *pNdb, const NdbDictionary::Table &tab,
                       bool merge_events, bool report) {
  char eventName[1024];
  sprintf(eventName, "%s_EVENT", tab.getName());

  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  if (!myDict) {
    g_err << "Dictionary not found " << pNdb->getNdbError().code << " "
          << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  myDict->dropEvent(eventName);

  NdbDictionary::Event myEvent(eventName);
  myEvent.setTable(tab.getName());
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL);
  for (int a = 0; a < tab.getNoOfColumns(); a++) {
    myEvent.addEventColumn(a);
  }
  myEvent.mergeEvents(merge_events);

  if (report) myEvent.setReport(NdbDictionary::Event::ER_SUBSCRIBE);

  int res = myDict->createEvent(myEvent);  // Add event to database

  if (res == 0)
    myEvent.print();
  else if (myDict->getNdbError().classification ==
           NdbError::SchemaObjectExists) {
    g_info << "Event creation failed event exists\n";
    res = myDict->dropEvent(eventName);
    if (res) {
      g_err << "Failed to drop event: " << myDict->getNdbError().code << " : "
            << myDict->getNdbError().message << endl;
      return NDBT_FAILED;
    }
    // try again
    res = myDict->createEvent(myEvent);  // Add event to database
    if (res) {
      g_err << "Failed to create event (1): " << myDict->getNdbError().code
            << " : " << myDict->getNdbError().message << endl;
      return NDBT_FAILED;
    }
  } else {
    g_err << "Failed to create event (2): " << myDict->getNdbError().code
          << " : " << myDict->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

static int createEvent(Ndb *pNdb, const NdbDictionary::Table &tab,
                       NDBT_Context *ctx) {
  bool merge_events = ctx->getProperty("MergeEvents");
  bool report = ctx->getProperty("ReportSubscribe");

  return createEvent(pNdb, tab, merge_events, report);
}

static int dropEvent(Ndb *pNdb, const NdbDictionary::Table &tab) {
  char eventName[1024];
  sprintf(eventName, "%s_EVENT", tab.getName());
  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();
  if (!myDict) {
    g_err << "Dictionary not found " << pNdb->getNdbError().code << " "
          << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }
  if (myDict->dropEvent(eventName)) {
    g_err << "Failed to drop event: " << myDict->getNdbError().code << " : "
          << myDict->getNdbError().message << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

static NdbEventOperation *createEventOperation(Ndb *ndb,
                                               const NdbDictionary::Table &tab,
                                               int do_report_error = 1) {
  char buf[1024];
  sprintf(buf, "%s_EVENT", tab.getName());
  NdbEventOperation *pOp = ndb->createEventOperation(buf);
  if (pOp == 0) {
    if (do_report_error)
      g_err << "createEventOperation: " << ndb->getNdbError().code << " "
            << ndb->getNdbError().message << endl;
    return 0;
  }
  int n_columns = tab.getNoOfColumns();
  for (int j = 0; j < n_columns; j++) {
    pOp->getValue(tab.getColumn(j)->getName());
    pOp->getPreValue(tab.getColumn(j)->getName());
  }
  if (pOp->execute()) {
    if (do_report_error)
      g_err << "pOp->execute(): " << pOp->getNdbError().code << " "
            << pOp->getNdbError().message << endl;
    ndb->dropEventOperation(pOp);
    return 0;
  }
  return pOp;
}

static int runCreateEvent(NDBT_Context *ctx, NDBT_Step *step) {
  if (createEvent(GETNDB(step), *ctx->getTab(), ctx) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runDropEvent(NDBT_Context *ctx, NDBT_Step *step) {
  return dropEvent(GETNDB(step), *ctx->getTab());
}

struct GcpStopVariant {
  int errorCode;
  const char *description;
  bool masterOnly;
  bool gcpSaveOnly;
};

GcpStopVariant gcpStopVariants[] = {
    {7238, "GCP_PREPARE @ participant", false, false},
    {7239, "GCP_COMMIT @ participant", false, false},
    {7244, "SUB_GCP_COMPLETE_REP @ participant", false, false},
    {7237, "GCP_SAVEREQ @ participant", false, true},
    {7241, "COPY_GCIREQ @ participant", false, true},
    {7242, "GCP COMMIT IDLE @ master", true, false},
    {7243, "GCP SAVE IDLE @ master", true, true},
    {0, "", false, false}};

int setupTestVariant(NdbRestarter &res, const GcpStopVariant &variant,
                     Uint32 victimNode, bool requireIsolation) {
  /**
   * First use dump code to lower thresholds to something
   * reasonable
   * This is run on all nodes to include the master.
   */
  {
    /* GCP Commit watchdog threshold */
    int dumpCommand[3] = {DumpStateOrd::DihSetGcpStopVals, 0, 10000};
    if (res.dumpStateAllNodes(&dumpCommand[0], 3) != 0) {
      g_err << "Error setting dump state 'GcpStopVals'" << endl;
      return NDBT_FAILED;
    }
  }
  {
    /* GCP Save watchdog threshold */
    int dumpCommand[3] = {DumpStateOrd::DihSetGcpStopVals, 1, 15000};
    if (res.dumpStateAllNodes(&dumpCommand[0], 3) != 0) {
      g_err << "Error setting dump state 'GcpStopVals'" << endl;
      return NDBT_FAILED;
    }
  }

  // Setting 'RestartOnErrorInsert = 2' will auto restart 'victim'
  int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 2};
  if (res.dumpStateAllNodes(val2, 2)) {
    g_err << "Error setting dump state 'RestartOnErrorInsert'" << endl;
    return NDBT_FAILED;
  }

  if (res.insertErrorInAllNodes(0) != 0) {
    g_err << "Failed clearing errors" << endl;
    return NDBT_FAILED;
  }

  /**
   * Cause GCP to stall in some way
   */
  if (requireIsolation) {
    /* Error insert flagging that we are testing the
     * 'isolation required' scenario
     */
    g_err << "Causing GCP stall using error code " << variant.errorCode << " 1"
          << endl;
    if (res.insertError2InNode(victimNode, variant.errorCode, 1) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }
  } else {
    g_err << "Causing GCP stall using error code " << variant.errorCode << endl;
    if (res.insertErrorInNode(victimNode, variant.errorCode) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }
  }

  if (requireIsolation) {
    /**
     * Now error inserts to stop the normal GCP stop
     * mechanisms working so that we rely on
     * isolation
     */
    g_err << "Causing GCP self-stop to fail on node " << victimNode << endl;
    /* NDBCNTR : Ignore GCP Stop in SYSTEM_ERROR */
    if (res.insertErrorInNode(victimNode, 1004) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }

    /* LQH : Ignore GCP Stop Kill in DUMP */
    if (res.insertErrorInNode(victimNode, 5085) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }

    /**
     * QMGR : Node will not disconnect itself,
     * due to ISOLATE_REQ, others must do it.
     * BUT DISCONNECT_REP is an ok way to die.
     */
    if (res.insertErrorInNode(victimNode, 942) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }
  } else {
    /* Testing normal GCP stop kill method */

    /* LQH : GCP Stop Kill is ok way to die */
    if (res.insertErrorInNode(victimNode, 5087) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }

    /**
     * NDBCNTR 'Normal' GCP stop kill in SYSTEM_ERROR
     * is ok way to die
     */
    if (res.insertErrorInNode(victimNode, 1005) != 0) {
      g_err << "Error inserting error" << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runGcpStop(NDBT_Context *ctx, NDBT_Step *step) {
  /* Intention here is to :
   *   a) Use DUMP code to lower GCP stop detection threshold
   *   b) Use ERROR INSERT to trigger GCP stop
   *   c) (Optional : Use ERROR INSERT to cause 'kill-self'
   *       handling of GCP Stop to fail, so that isolation
   *       is required)
   *   d) Check that GCP is resumed
   */
  /* TODO : Survivable multiple participant failure */
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  Ndb *pNdb = GETNDB(step);

  /**
   * We use an event here just so that we get live 'cluster epoch'
   * info in the API.
   * There's no actual row events used or read.
   */
  NdbEventOperation *myEvent = createEventOperation(pNdb, *ctx->getTab());

  if (myEvent == NULL) {
    g_err << "Failed to create Event operation" << endl;
    return NDBT_FAILED;
  }

  /**
   * requireIsolation == the normal GCP stop 'kill self'
   * mechanism is disabled via ERROR_INSERT, so that
   * isolation of the node by other nodes is required
   * to get it 'cut off' from the cluster
   */
  bool requireIsolation =
      (ctx->getProperty("GcpStopIsolation", Uint32(0)) != 0);

  int result = NDBT_FAILED;
  while (loops--) {
    int variantIndex = 0;
    bool done = false;
    do {
      GcpStopVariant &variant = gcpStopVariants[variantIndex++];
      g_err << "Testcase " << variant.description << "  Save only? "
            << variant.gcpSaveOnly << "  Isolation : " << requireIsolation
            << endl;

      int victimNode = res.getNode(NdbRestarter::NS_RANDOM);

      if (variant.masterOnly) {
        victimNode = res.getNode(NdbRestarter::NS_MASTER);
      }

      bool isMaster = (victimNode == res.getNode(NdbRestarter::NS_MASTER));

      g_err << "Victim will be " << victimNode << " " << (isMaster ? "*" : "")
            << endl;

      if (setupTestVariant(res, variant, victimNode, requireIsolation) !=
          NDBT_OK) {
        break;
      }

      /**
       * Epoch / GCP should not be stopped
       * Let's wait for it to start again
       */

      /* GCP Commit stall visible within 2 s
       * GCP Save stall requires longer
       */
      Uint32 minStallSeconds = (variant.gcpSaveOnly ? 10 : 2);

      g_err << "Waiting for " << minStallSeconds << " seconds of epoch stall"
            << endl;

      pNdb->pollEvents(1, 0);
      Uint64 startEpoch = pNdb->getLatestGCI();

      Uint32 stallSeconds = 0;
      do {
        NdbSleep_MilliSleep(1000);
        pNdb->pollEvents(1, 0);

        Uint64 currEpoch = pNdb->getLatestGCI();
        bool same = false;
        if (variant.gcpSaveOnly) {
          same = ((currEpoch >> 32) == (startEpoch >> 32));
        } else {
          same = (currEpoch == startEpoch);
        }

        if (same) {
          g_err << "Epoch stalled @ " << (currEpoch >> 32) << "/"
                << (currEpoch & 0xffffffff) << endl;
          stallSeconds++;
        } else {
          g_err << "Epoch not stalled yet" << endl;
          /* Diff */
          startEpoch = currEpoch;
          stallSeconds = 0;
        }
      } while (stallSeconds < minStallSeconds);

      g_err << "Epoch definitely stalled" << endl;

      /* GCP Commit stall stops any increase
       * GCP Save stall stops only msw increase
       */
      Uint64 minNewEpoch = (variant.gcpSaveOnly ? ((startEpoch >> 32) + 1) << 32
                                                : (startEpoch + 1));

      Uint64 currEpoch = pNdb->getLatestGCI();
      while (currEpoch < minNewEpoch) {
        g_err << "Waiting for epoch to advance from " << (currEpoch >> 32)
              << "/" << (currEpoch & 0xffffffff) << " to at least "
              << (minNewEpoch >> 32) << "/" << (minNewEpoch & 0xffffffff)
              << endl;
        NdbSleep_MilliSleep(1000);
        currEpoch = pNdb->getLatestGCI();
      }

      g_err << "Epoch is now " << (currEpoch >> 32) << "/"
            << (currEpoch & 0xffffffff) << endl;
      g_err << "Cluster recovered from GCP stop" << endl;

      g_err << "Now waiting for victim node to recover" << endl;
      /**
       * Now wait until all nodes are available
       */
      if (res.waitClusterStarted() != 0) {
        g_err << "Timed out waiting for cluster to fully start" << endl;
        break;
      }
      CHK_NDB_READY(pNdb);

      g_err << "Cluster recovered..." << endl;

      done = (gcpStopVariants[variantIndex].errorCode == 0);
    } while (!done);

    if (!done) {
      /* Error exit from inner loop */
      break;
    }

    if (loops == 0) {
      /* All loops done */
      result = NDBT_OK;
    }
  }

  pNdb->dropEventOperation(myEvent);

  return result;
}

int cleanupGcpStopTest(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  restarter.insertErrorInAllNodes(0);

  /* Reset GCP stop timeouts */
  int code = DumpStateOrd::DihSetGcpStopVals;
  restarter.dumpStateAllNodes(&code, 1);

  /* Reset StopOnError behaviour */
  code = DumpStateOrd::CmvmiSetRestartOnErrorInsert;
  restarter.dumpStateAllNodes(&code, 1);

  return NDBT_OK;
}

int CMT_createTableHook(Ndb *ndb, NdbDictionary::Table &table, int when,
                        void *arg) {
  if (when == 0) {
    Uint32 num = ((Uint32 *)arg)[0];

    /* Substitute a unique name */
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "%s_%u", table.getName(), num);
    table.setName(buf);

    ndbout << "Creating " << buf << endl;
  }
  return 0;
}

int createManyTables(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  const Uint32 numTables = ctx->getProperty("NumTables", Uint32(20));

  for (Uint32 tn = 0; tn < numTables; tn++) {
    Uint32 args[1];
    args[0] = tn;

    if (NDBT_Tables::createTable(pNdb, ctx->getTab()->getName(), false, false,
                                 CMT_createTableHook, &args) != 0) {
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int dropManyTables(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  char buf[100];

  const Uint32 numTables = ctx->getProperty("NumTables", Uint32(20));

  for (Uint32 tn = 0; tn < numTables; tn++) {
    BaseString::snprintf(buf, sizeof(buf), "%s_%u", ctx->getTab()->getName(),
                         tn);
    ndbout << "Dropping " << buf << endl;
    pNdb->getDictionary()->dropTable(buf);
  }

  return NDBT_OK;
}

int runGetTabInfo(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();

  Uint32 stepNum = step->getStepNo();

  char buf[100];
  BaseString::snprintf(buf, sizeof(buf), "%s_%u", ctx->getTab()->getName(),
                       stepNum - 1);

  ndbout << "runGetTabInfo() Step num " << stepNum << " accessing table " << buf
         << endl;

  Uint32 success = 0;
  Uint32 failure = 0;
  NDB_TICKS periodStart = NdbTick_getCurrentTicks();
  Uint32 periodSnap = 0;
  while (!ctx->isTestStopped()) {
    dict->invalidateTable(buf);
    const NdbDictionary::Table *pTab = dict->getTable(buf);

    if (pTab == NULL) {
      ndbout << "Step num " << stepNum << " got error "
             << dict->getNdbError().code << " " << dict->getNdbError().message
             << " when getting table " << buf << endl;
      failure++;
    } else {
      success++;
    }

    Uint64 millisPassed =
        NdbTick_Elapsed(periodStart, NdbTick_getCurrentTicks()).milliSec();

    if (millisPassed > 10000) {
      ndbout << "Step num " << stepNum << " completed "
             << (success - periodSnap) << " lookups "
             << " in " << millisPassed << " millis.  "
             << "Rate is " << (success - periodSnap) * 1000 / millisPassed
             << " lookups/s" << endl;
      periodSnap = success;
      periodStart = NdbTick_getCurrentTicks();
    }
  }

  ndbout << "Step num " << stepNum << " ok : " << success
         << " failed : " << failure << endl;

  return NDBT_OK;
}

int runLCPandRestart(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  NdbSleep_MilliSleep(6000);

  for (int i = 0; i < 4; i++) {
    ndbout << "Triggering LCP..." << endl;
    int lcpDumpCode = 7099;
    restarter.dumpStateAllNodes(&lcpDumpCode, 1);

    /* TODO : Proper 'wait for LCP completion' here */
    NdbSleep_MilliSleep(20000);
  }

  int node = restarter.getNode(NdbRestarter::NS_RANDOM);
  ndbout << "Triggering node restart " << node << endl;
  restarter.restartOneDbNode2(node, 0);

  ndbout << "Wait for node recovery..." << endl;
  if (restarter.waitNodesStarted(&node, 1) != 0) {
    ndbout << "Failed waiting for node to restart" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Done." << endl;

  ctx->stopTest();

  return NDBT_OK;
}

int runLCP(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  NdbSleep_MilliSleep(6000);

  while (ctx->isTestStopped() == false) {
    ndbout << "Triggering LCP..." << endl;
    int lcpDumpCode = 7099;
    restarter.dumpStateAllNodes(&lcpDumpCode, 1);

    /* TODO : Proper 'wait for LCP completion' here */
    NdbSleep_MilliSleep(2000);
  }

  return NDBT_OK;
}

int snapshotLMBUsage(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  int code = DumpStateOrd::CmvmiLongSignalMemorySnapshotStart;
  restarter.dumpStateAllNodes(&code, 1);
  code = DumpStateOrd::CmvmiLongSignalMemorySnapshot;
  restarter.dumpStateAllNodes(&code, 1);

  return NDBT_OK;
}

int waitAndCheckLMBUsage(NDBT_Context *ctx, NDBT_Step *step) {
  ndbout_c("Waiting for some time (and LCPs) to pass...");
  NdbSleep_MilliSleep(120000);

  NdbRestarter restarter;

  ndbout_c("Checking growth not excessive...");
  int code = DumpStateOrd::CmvmiLongSignalMemorySnapshotCheck2;
  restarter.dumpStateAllNodes(&code, 1);
  NdbSleep_MilliSleep(5000);

  ctx->stopTest();
  return NDBT_OK;
}

int runArbitrationWithApiNodeFailure(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Check that arbitration do not fail with non arbitrator api node
   * failure.
   */

  NdbRestarter restarter;

  /**
   * Bug#23006431 UNRELATED API FAILURE DURING ARBITRATION CAUSES
   *              ARBITRATION FAILURE
   *
   * If a data node that have won the arbitration get a api failure it
   * could trample the arbitration state and result in arbitration failure
   * before the win was effectuated.
   *
   * 1. connect api node
   * 2. error insert in next master to delay win after api node failure
   * 3. kill master
   * 4. disconnect api node
   * 5. next master should survive
   *
   */

  /**
   * This test case has been designed to work with only 1 nodegroup.
   * With multiple nodegroups, a single node failure is not enough to
   * force arbitration. Since the single node which failed does not
   * form a viable community by itself, arbitration (and thus the error
   * insert) is skipped. Thus, this test case should be skipped for
   * clusters with more than 1 nodegroup.
   */
  if (restarter.getNumDbNodes() != 2) {
    g_err << "[SKIPPED] Test skipped.  Needs 1 nodegroup" << endl;
    return NDBT_SKIPPED;
  }

  /**
   * 1. connect new api node
   */
  Ndb_cluster_connection *cluster_connection = new Ndb_cluster_connection();
  cluster_connection->configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (cluster_connection->connect() != 0) {
    g_err << "ERROR: connect failure." << endl;
    return NDBT_FAILED;
  }
  Ndb *ndb = new Ndb(cluster_connection, "TEST_DB");
  if (ndb->init() != 0 || ndb->waitUntilReady(30) != 0) {
    g_err << "ERROR: Ndb::init failure." << endl;
    return NDBT_FAILED;
  }

  /**
   * 2. error insert in next master to delay arbitration win after api
   *    node failure
   */
  const int master = restarter.getMasterNodeId();
  const int nextMaster = restarter.getNextMasterNodeId(master);
  if (restarter.insertErrorInNode(nextMaster, 945) != 0) {
    g_err << "ERROR: inserting error 945 into next master " << nextMaster
          << endl;
    return NDBT_FAILED;
  }

  /**
   * 3. kill master
   */
  if (restarter.restartOneDbNode2(
          master, NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT,
          true) == 0) {
    g_err << "ERROR: Old master " << master << " reached not started state "
          << "before arbitration win" << endl;
    return NDBT_FAILED;
  }

  /**
   * 4. disconnect api node
   */
  delete ndb;
  delete cluster_connection;

  /**
   * 5. next master should survive
   *
   * Verify cluster up with correct master.
   */

  if (restarter.waitNodesNoStart(&master, 1) != 0) {
    g_err << "ERROR: old master " << master << " not stopped" << endl;
    return NDBT_FAILED;
  }

  if (restarter.startNodes(&master, 1) != 0) {
    g_err << "ERROR: restarting old master " << master << " failed" << endl;
    return NDBT_FAILED;
  }

  if (restarter.waitClusterStarted() != 0) {
    g_err << "ERROR: wait cluster start failed" << endl;
    return NDBT_FAILED;
  }

  const int newMaster = restarter.getMasterNodeId();
  if (newMaster != nextMaster) {
    g_err << "ERROR: wrong master, got " << newMaster << " expected "
          << nextMaster << endl;
    return NDBT_FAILED;
  }

  /**
   * Clear error insert in next master.
   */
  restarter.insertErrorInNode(nextMaster, 0);

  return NDBT_OK;
}

int runLCPandRecordId(NDBT_Context *ctx, NDBT_Step *step) {
  /* Bug #23602217:  MISSES TO USE OLDER LCP WHEN LATEST LCP
   * IS NOT RECOVERABLE. This function is called twice so that
   * 2 consecutive LCPs are triggered and the id of the first
   * LCP is recorded in order to compare it to the id of LCP
   * restored in the restart in the next step.
   */
  NdbRestarter restarter;
  struct ndb_logevent event;
  int filter[] = {15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0};
  int arg1[] = {DumpStateOrd::DihMaxTimeBetweenLCP};
  int arg2[] = {DumpStateOrd::DihStartLcpImmediately};
  if (restarter.dumpStateAllNodes(arg1, 1) != 0) {
    g_err << "ERROR: Dump MaxTimeBetweenLCP failed" << endl;
    return NDBT_FAILED;
  }
  NdbLogEventHandle handle =
      ndb_mgm_create_logevent_handle(restarter.handle, filter);
  ndbout << "Triggering LCP..." << endl;
  if (restarter.dumpStateAllNodes(arg2, 1) != 0) {
    g_err << "ERROR: Dump StartLcpImmediately failed" << endl;
    ndb_mgm_destroy_logevent_handle(&handle);
    return NDBT_FAILED;
  }
  while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
         event.type != NDB_LE_LocalCheckpointCompleted)
    ;
  Uint32 LCPid = event.LocalCheckpointCompleted.lci;
  ndbout << "LCP: " << LCPid << endl;
  if (ctx->getProperty("LCP", (Uint32)0) == 0) {
    ndbout << "Recording id of first LCP" << endl;
    ctx->setProperty("LCP", LCPid);
  }
  ndb_mgm_destroy_logevent_handle(&handle);
  return NDBT_OK;
}

int runRestartandCheckLCPRestored(NDBT_Context *ctx, NDBT_Step *step) {
  /* Bug #23602217:  MISSES TO USE OLDER LCP WHEN LATEST LCP
   * IS NOT RECOVERABLE. The steps followed are as follows:
   * - Restart node in nostart state
   * - Insert error 7248 so first LCP is considered non-restorable
   * - Start node
   * - Wait for LCPRestored log event
   * - Check if restored LCP is same as first LCP id
   *   recorded in INITIALIZER
   */
  NdbRestarter restarter;
  struct ndb_logevent event;
  int filter[] = {15, NDB_MGM_EVENT_CATEGORY_STARTUP, 0};
  int node = restarter.getNode(NdbRestarter::NS_RANDOM);
  ndbout << "Triggering node restart " << node << endl;
  if (restarter.restartOneDbNode(node, false, true, true) != 0) {
    g_err << "ERROR: Restarting node " << node << " failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Wait for NoStart state" << endl;
  if (restarter.waitNodesNoStart(&node, 1) != 0) {
    g_err << "ERROR: Node " << node << " stop failed" << endl;
    return NDBT_FAILED;
  }
  NdbLogEventHandle handle =
      ndb_mgm_create_logevent_handle(restarter.handle, filter);
  ndbout << "Insert error 7248 so most recent LCP is non-restorable" << endl;
  if (restarter.insertErrorInNode(node, 7248) != 0) {
    g_err << "ERROR: Error insert 7248 failed" << endl;
    ndb_mgm_destroy_logevent_handle(&handle);
    return NDBT_FAILED;
  }
  ndbout << "Start node" << endl;
  if (restarter.startNodes(&node, 1) != 0) {
    g_err << "ERROR: Node " << node << " start failed" << endl;
    if (restarter.insertErrorInNode(node, 0) != 0) {
      g_err << "ERROR: Error insert clear failed" << endl;
    }
    ndb_mgm_destroy_logevent_handle(&handle);
    return NDBT_FAILED;
  }
  if (restarter.waitNodesStarted(&node, 1) != 0) {
    g_err << "ERROR: Wait node " << node << " start failed" << endl;
    if (restarter.insertErrorInNode(node, 0) != 0) {
      g_err << "ERROR: Error insert clear failed" << endl;
    }
    ndb_mgm_destroy_logevent_handle(&handle);
    return NDBT_FAILED;
  }
  while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
         event.type != NDB_LE_LCPRestored)
    ;
  Uint32 lcp_restored = event.LCPRestored.restored_lcp_id;
  ndbout << "LCP Restored: " << lcp_restored << endl;
  Uint32 first_lcp = ctx->getProperty("LCP", (Uint32)0);
  if (lcp_restored != first_lcp && lcp_restored != (first_lcp + 1)) {
    g_err << "ERROR: LCP " << lcp_restored << " restored, "
          << "expected restore of LCP " << first_lcp << " or "
          << (first_lcp + 1) << endl;
    if (restarter.insertErrorInNode(node, 0) != 0) {
      g_err << "ERROR: Error insert clear failed" << endl;
    }
    ndb_mgm_destroy_logevent_handle(&handle);
    return NDBT_FAILED;
  }
  if (restarter.insertErrorInNode(node, 0) != 0) {
    g_err << "ERROR: Error insert clear failed" << endl;
    return NDBT_FAILED;
  }
  ndb_mgm_destroy_logevent_handle(&handle);
  return NDBT_OK;
}

int runTestStartNode(NDBT_Context *ctx, NDBT_Step *step) {
  /*
   * Bug #11757421:  SEND START OF NODE START COMMAND IGNORED IN RESTART
   *
   * This test checks the following scenarios:
   * - Restart of a single data node
   *   - When the shutdown process fails to begin
   *   - When the shutdown process fails to complete
   * - Restart of multiple data nodes
   *   - When the shutdown process fails to begin
   *   - When the shutdown process fails to complete
   *
   * The steps in each sub-scenario are as follows:
   * - Insert error code in management node
   * - Trigger restart which should fail to start node(s)
   * - Remove the error insert
   */
  NdbRestarter restarter;
  int cnt = restarter.getNumDbNodes();

  if (restarter.waitClusterStarted() != 0) {
    g_err << "ERROR: Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  int nodeId = restarter.getDbNodeId(rand() % cnt);
  int mgmdNodeId = ndb_mgm_get_mgmd_nodeid(restarter.handle);

  ndbout << "Case 1: Restart of a single data node where the"
         << " shutdown process fails to begin" << endl;
  ndbout << "Insert error 10006 in mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 10006) != 0) {
    g_err << "ERROR: Error insert in mgmd failed" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Trigger restart of node " << nodeId << " which should fail"
         << endl;
  if (restarter.restartOneDbNode(nodeId, false, true, true, false, true) == 0) {
    g_err << "ERROR: Restart of node " << nodeId
          << " succeeded instead of failing" << endl;
    return NDBT_FAILED;
  }

  // Check if the restart failed with correct error
  BaseString error_code(ndb_mgm_get_latest_error_desc(restarter.handle), 4);
  if (error_code != "5024") {
    g_err << "ERROR: Restart of node " << nodeId << " failed with "
          << "error " << error_code.c_str() << " instead of error "
          << "5024" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Remove the error code from mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 0) != 0) {
    g_err << "ERROR: Error insert clear failed" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Case 2: Restart of a single data node where the"
         << " shutdown process fails to complete" << endl;
  ndbout << "Insert error 10007 in mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 10007) != 0) {
    g_err << "ERROR: Error insert in mgmd failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Trigger restart of node " << nodeId << " which should fail"
         << endl;
  if (restarter.restartOneDbNode(nodeId, false, true, true, false, true) == 0) {
    g_err << "ERROR: Restart of node " << nodeId
          << " succeeded instead of failing" << endl;
    return NDBT_FAILED;
  }

  // Check if the restart failed with correct error
  error_code.assign(ndb_mgm_get_latest_error_desc(restarter.handle), 4);
  if (error_code != "5025") {
    g_err << "ERROR: Restart of node " << nodeId << " failed with "
          << "error " << error_code.c_str() << " instead of error "
          << "5025" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Remove the error code from mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 0) != 0) {
    g_err << "ERROR: Error insert clear failed" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Case 3: Restart of all data nodes where the"
         << " shutdown process fails to begin" << endl;
  ndbout << "Insert error 10006 in mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 10006) != 0) {
    g_err << "ERROR: Error insert in mgmd failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Trigger restart of all nodes which should fail" << endl;
  if (restarter.restartAll3(false, true, true, false) == 0) {
    g_err << "ERROR: Restart of nodes succeeded "
          << "instead of failing" << endl;
    return NDBT_FAILED;
  }

  // Check if the restart failed with correct error
  error_code.assign(ndb_mgm_get_latest_error_desc(restarter.handle), 4);
  if (error_code != "5024") {
    g_err << "ERROR: Restart of nodes failed with error " << error_code.c_str()
          << " instead of error "
          << "5024" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Remove the error code from mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 0) != 0) {
    g_err << "ERROR: Error insert clear failed" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Case 4: Restart of all data nodes where the"
         << " shutdown process fails to complete" << endl;
  ndbout << "Insert error 10007 in mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 10007) != 0) {
    g_err << "ERROR: Error insert in mgmd failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Trigger restart of all nodes which should fail" << endl;
  if (restarter.restartAll3(false, true, true, false) == 0) {
    g_err << "ERROR: Restart of nodes succeeded instead of failing" << endl;
    return NDBT_FAILED;
  }

  // Check if the restart failed with correct error
  error_code.assign(ndb_mgm_get_latest_error_desc(restarter.handle), 4);
  if (error_code != "5025") {
    g_err << "ERROR: Restart of nodes failed with error " << error_code.c_str()
          << " instead of error "
          << "5025" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Remove the error code from mgmd" << endl;
  if (restarter.insertErrorInNode(mgmdNodeId, 0) != 0) {
    g_err << "ERROR: Error insert clear failed" << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

/**
 * In Partial LCP we need many LCPs to restore a checkpoint. The
 * maximum number of LCPs we need in order to restore a checkpoint
 * is 2048. This test uses error insert 10048 to ensure that each
 * LCP only stores 1 part completely. This means that this test
 * can generate checkpoints that have to write LCP control files
 * consisting of close to 2048 parts and similarly to restore those.
 *
 * The test loops for more than 2048 times to ensure that we come
 * to a situation with a large number of parts in each LCP and in
 * particular for the last one that we are to restore. The number
 * 2058 is somewhat arbitrarily chosen to ensure this.
 *
 * Between each LCP we perform a random amount of updates to ensure
 * that each part of this table will create a non-empty LCP. We
 * insert a number of random LCPs that are empty as well to ensure
 * that we generate empty LCPs correctly as well even if there are
 * many parts in the LCP.
 */
int run_PLCP_many_parts(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  Config conf;
  NdbMgmd mgmd;

  int node_1 = restarter.getDbNodeId(0);
  int node_2 = restarter.getDbNodeId(1);
  if (node_1 == -1 || node_2 == -1) {
    g_err << "Failed to find node ids of data nodes" << endl;
    return NDBT_FAILED;
  }

  mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
  if (!mgmd.connect()) {
    g_err << "Failed to connect to ndb_mgmd." << endl;
    return NDBT_FAILED;
  }
  if (!mgmd.get_config(conf)) {
    g_err << "Failed to get config from ndb_mgmd." << endl;
    return NDBT_FAILED;
  }
  ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
  Uint32 enabledPartialLCP = 1;
  for (int idx = 0; iter.openSection(CFG_SECTION_NODE, idx); idx++) {
    Uint32 nodeId = 0;
    if (iter.get(CFG_NODE_ID, &nodeId)) {
      if (nodeId == (Uint32)node_1) {
        iter.get(CFG_DB_ENABLE_PARTIAL_LCP, &enabledPartialLCP);
        iter.closeSection();
        break;
      }
    }
    iter.closeSection();
  }

  if (enabledPartialLCP == 0) {
    g_err << "[SKIPPED] Test skipped. Needs EnablePartialLcp=1" << endl;
    iter.closeSection();
    return NDBT_SKIPPED;
  }

  Ndb *pNdb = GETNDB(step);
  int loops = 2200;
  int records = ctx->getNumRecords();
  bool drop_table = (bool)ctx->getProperty("DropTable", 1);
  HugoTransactions hugoTrans(*ctx->getTab());
  const Uint32 nodeCount = restarter.getNumDbNodes();
  NdbDictionary::Dictionary *pDict = GETNDB(step)->getDictionary();
  NdbDictionary::Table tab = *ctx->getTab();
  HugoOperations hugoOps(tab);
  if (nodeCount != 2) {
    g_err << "[SKIPPED] Test skipped.  Needs 2 nodes" << endl;
    return NDBT_SKIPPED; /* Requires exact 2 nodes to run */
  }

  Uint32 gcp_interval = 200;
  Uint32 key = CFG_DB_GCP_INTERVAL;
  if (setConfigValueAndRestartNode(&mgmd, &key, &gcp_interval, 1, node_1, true,
                                   &restarter, false) == NDBT_FAILED) {
    g_err << "Failed to set TimeBetweenGlobalCheckpoints to 200" << endl;
    return NDBT_FAILED;
  }
  g_err << "Restarting node " << node_2 << " to apply config change.." << endl;
  if (restarter.restartOneDbNode(node_2, false, false, true)) {
    g_err << "Failed to restart node." << endl;
    return NDBT_FAILED;
  }
  if (restarter.waitNodesStarted(&node_2, 1) != 0) {
    g_err << "Failed waiting for node started." << endl;
    return NDBT_FAILED;
  }
  if (hugoTrans.loadTable(pNdb, records) != NDBT_OK) {
    g_err << "Failed to load table" << endl;
    return NDBT_FAILED;
  }

  g_err << "Executing " << loops << " loops" << endl;
  if (restarter.insertErrorInNode(node_1, 10048) != 0) {
    g_err << "ERROR: Error insert 10048 failed" << endl;
    return NDBT_FAILED;
  }
  int i = 0;
  int result = NDBT_OK;
  while (++i <= loops && result != NDBT_FAILED) {
    g_err << "Start loop " << i << endl;
    ndbout << "Start an LCP" << endl;
    {
      int val = DumpStateOrd::DihStartLcpImmediately;
      if (restarter.dumpStateAllNodes(&val, 1) != 0) {
        g_err << "ERR: " << step->getName() << " failed on line " << __LINE__
              << endl;
        return NDBT_FAILED;
      }
    }
    Uint32 batch = 8;
    Uint32 row;
    row = rand() % records;
    if (row + batch > (Uint32)records) row = records - batch;

    if ((hugoOps.startTransaction(pNdb) != 0) ||
        (hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0) ||
        (hugoOps.execute_Commit(pNdb)) || (hugoOps.closeTransaction(pNdb))) {
      g_err << "Update failed" << endl;
      // return NDBT_FAILED;
    }
    NdbSleep_SecSleep(1);
    row = rand() % records;
    if (row + batch > (Uint32)records) row = records - batch;
    if ((hugoOps.startTransaction(pNdb) != 0) ||
        (hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0) ||
        (hugoOps.execute_Commit(pNdb)) || (hugoOps.closeTransaction(pNdb))) {
      g_err << "Update failed" << endl;
      // return NDBT_FAILED;
    }
  }
  if (drop_table) {
    /**
     * In this case we will drop this table, this will verify that
     * BUG#92955 is fixed. After this we create a new table and
     * perform a scan against the new table.
     * This will cause a crash if the bug isn't fixed.
     */
    pDict->dropTable(tab.getName());
    int res = pDict->createTable(tab);
    if (res) {
      ndbout_c("Failed to create table again");
      return NDBT_FAILED;
    }
    HugoTransactions trans(*pDict->getTable(tab.getName()));
    trans.loadTable(pNdb, ctx->getNumRecords());
    trans.scanUpdateRecords(pNdb, ctx->getNumRecords());
    CHECK(restarter.insertErrorInNode(node_1, 0) == 0,
          "Failed to clear insertError");
    return NDBT_OK;
  }
  /**
   * Finally after creating a complex restore situation we test this
   * by restarting node 2 to ensure that we can also recover the
   * complex LCP setup.
   */
  ndbout << "Restart node_1" << endl;
  if (restarter.restartOneDbNode(node_1, false, /* initial */
                                 true,          /* nostart  */
                                 false,         /* abort */
                                 false /* force */) != 0) {
    g_err << "Restart failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Wait for NoStart state" << endl;
  restarter.waitNodesNoStart(&node_1, 1);
  ndbout << "Start node" << endl;
  if (restarter.startNodes(&node_1, 1) != 0) {
    g_err << "Start failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Waiting for node to start" << endl;
  if (restarter.waitNodesStarted(&node_1, 1) != 0) {
    g_err << "Wait node start failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Reset TimeBetweenGlobalCheckpoints to " << gcp_interval << endl;

  if (setConfigValueAndRestartNode(&mgmd, &key, &gcp_interval, 1, node_1, true,
                                   &restarter, false) == NDBT_FAILED) {
    g_err << "Failed to reset TimeBetweenGlobalCheckpoints" << endl;
    return NDBT_FAILED;
  }
  g_err << "Restarting node " << node_2 << " to apply config change.." << endl;
  if (restarter.restartOneDbNode(node_2, false, false, true)) {
    g_err << "Failed to restart node." << endl;
    return NDBT_FAILED;
  }
  if (restarter.waitNodesStarted(&node_2, 1) != 0) {
    g_err << "Failed waiting for node started." << endl;
    return NDBT_FAILED;
  }
  ndbout << "Test complete" << endl;
  return NDBT_OK;
}

int run_PLCP_I1(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  int i = 0;
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  bool initial = (bool)ctx->getProperty("Initial", 1);
  bool wait_start = (bool)ctx->getProperty("WaitStart", 1);
  NdbRestarter restarter;
  const Uint32 nodeCount = restarter.getNumDbNodes();
  int nodeId = restarter.getRandomNotMasterNodeId(rand());
  HugoTransactions hugoTrans(*ctx->getTab());
  g_err << "Will restart node " << nodeId << endl;

  if (nodeCount < 2) {
    g_err << "[SKIPPED] Test skipped. Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED; /* Requires at least 2 nodes to run */
  }
  g_err << "Executing " << loops << " loops" << endl;
  while (++i <= loops && result != NDBT_FAILED) {
    g_err << "Start loop " << i << endl;
    g_err << "Loading " << records << " records..." << endl;
    if (hugoTrans.loadTable(pNdb, records) != NDBT_OK) {
      g_err << "Failed to load table" << endl;
      return NDBT_FAILED;
    }
    if (restarter.restartOneDbNode(nodeId, initial, /* initial */
                                   true,            /* nostart  */
                                   false,           /* abort */
                                   false /* force */) != 0) {
      g_err << "Restart failed" << endl;
      return NDBT_FAILED;
    }
    ndbout << "Wait for NoStart state" << endl;
    restarter.waitNodesNoStart(&nodeId, 1);
    if (restarter.insertErrorInNode(nodeId, 1011)) {
      g_err << "Failed to insert error 1011" << endl;
      return NDBT_FAILED;
    }
    if (!wait_start) {
      ndbout << "Start node" << endl;
      if (restarter.startNodes(&nodeId, 1) != 0) {
        g_err << "Start failed" << endl;
        return NDBT_FAILED;
      }
    }
    ndbout << "Delete records" << endl;

    Uint32 row_step = 10;
    Uint32 num_deleted_records = records / 10;
    Uint32 batch = 10;

    for (Uint32 start = 0; start < 10; start++) {
      CHECK((hugoTrans.pkDelRecords(pNdb, num_deleted_records, batch, true, 0,
                                    start, row_step) == 0),
            "");
      if (result == NDBT_FAILED) return result;
      NdbSleep_SecSleep(1);
      ndbout << "Completed Delete records (" << (start + 1) << ")" << endl;
    }
    if (wait_start) {
      ndbout << "Start node" << endl;
      if (restarter.startNodes(&nodeId, 1) != 0) {
        g_err << "Start failed" << endl;
        return NDBT_FAILED;
      }
    }
    ndbout << "Delete records" << endl;
    ndbout << "Wait for node restart to complete" << endl;
    if (restarter.waitNodesStarted(&nodeId, 1) != 0) {
      g_err << "Wait node restart failed" << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int run_PLCP_I2(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  int i = 0;
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  const Uint32 nodeCount = restarter.getNumDbNodes();
  int nodeId = restarter.getRandomNotMasterNodeId(rand());
  HugoTransactions hugoTrans(*ctx->getTab());

  if (nodeCount < 2) {
    g_info << "[SKIPPED] Requires at least 2 nodes" << endl;
    return NDBT_SKIPPED; /* Requires at least 2 nodes to run */
  }
  g_err << "Executing " << loops << " loops" << endl;
  while (++i <= loops && result != NDBT_FAILED) {
    g_err << "Start loop " << i << endl;
    g_err << "Loading " << records << " records..." << endl;
    if (hugoTrans.loadTable(pNdb, records) != NDBT_OK) {
      g_err << "Failed to load table" << endl;
      return NDBT_FAILED;
    }
    if (restarter.restartOneDbNode(nodeId, true, /* initial */
                                   true,         /* nostart  */
                                   false,        /* abort */
                                   false /* force */) != 0) {
      g_err << "Restart failed" << endl;
      return NDBT_FAILED;
    }
    ndbout << "Wait for NoStart state" << endl;
    restarter.waitNodesNoStart(&nodeId, 1);
    ndbout << "Start node" << endl;
    if (restarter.startNodes(&nodeId, 1) != 0) {
      g_err << "Start failed" << endl;
      return NDBT_FAILED;
    }
    ndbout << "Delete 10% of records" << endl;

    Uint32 row_step = 1;
    Uint32 start = 0;
    Uint32 num_deleted_records = records / 10;
    Uint32 batch = 1;

    CHECK((hugoTrans.pkDelRecords(pNdb, num_deleted_records, batch, true, 0,
                                  start, row_step) == 0),
          "");
    if (result == NDBT_FAILED) return result;
    ndbout << "Start an LCP" << endl;
    {
      int val = DumpStateOrd::DihStartLcpImmediately;
      if (restarter.dumpStateAllNodes(&val, 1) != 0) {
        g_err << "ERR: " << step->getName() << " failed on line " << __LINE__
              << endl;
        return NDBT_FAILED;
      }
    }
    ndbout << "Delete 80% of the records" << endl;
    for (Uint32 i = 2; i < 10; i++) {
      start += num_deleted_records;
      CHECK((hugoTrans.pkDelRecords(pNdb, num_deleted_records, batch, true, 0,
                                    start, row_step) == 0),
            "");
      if (result == NDBT_FAILED) return result;
    }
    ndbout << "Wait for initial node restart to complete" << endl;
    if (restarter.waitNodesStarted(&nodeId, 1) != 0) {
      g_err << "Wait node start failed" << endl;
      return NDBT_FAILED;
    }
    ndbout << "Delete remaining records" << endl;
    start += num_deleted_records;
    CHECK((hugoTrans.pkDelRecords(pNdb, num_deleted_records, batch, true, 0,
                                  start, row_step) == 0),
          "");
    if (result == NDBT_FAILED) return result;
  }
  return NDBT_OK;
}

int runNodeFailLcpStall(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int master = restarter.getMasterNodeId();
  int other = restarter.getRandomNodeSameNodeGroup(master, rand());

  ndbout_c("Master %u  Other %u", master, other);

  ndbout_c("Stalling lcp in node %u", other);
  restarter.insertErrorInNode(other, 5073);

  int dump[] = {7099};
  ndbout_c("Triggering LCP");
  restarter.dumpStateOneNode(master, dump, 1);

  ndbout_c("Giving time for things to stall");
  NdbSleep_MilliSleep(10000);

  ndbout_c("Getting Master to kill other when Master LCP complete %u", master);
  restarter.insertError2InNode(master, 7178, other);

  ndbout_c("Releasing scans in node %u", other);
  restarter.insertErrorInNode(other, 0);

  ndbout_c("Expect other node failure");
  Uint32 retries = 100;
  while (restarter.getNodeStatus(other) == NDB_MGM_NODE_STATUS_STARTED) {
    if ((--retries) == 0) {
      ndbout_c("Timeout waiting for other node to restart");
      return NDBT_FAILED;
    }
    NdbSleep_MilliSleep(500);
  }

  ndbout_c("Other node failed, now wait for it to restart");
  restarter.insertErrorInNode(master, 0);

  if (restarter.waitNodesStarted(&other, 1) != 0) {
    ndbout_c("Timed out waiting for restart");
    return NDBT_FAILED;
  }

  ndbout_c("Restart succeeded");

  return NDBT_OK;
}

/* Check whether the deceased node died within max_timeout_sec*/
int checkOneNodeDead(int deceased, int max_timeout_sec) {
  int timeout = 0;
  NdbRestarter restarter;

  int victim_status = NDB_MGM_NODE_STATUS_UNKNOWN;
  while (timeout++ < max_timeout_sec) {
    victim_status = restarter.getNodeStatus(deceased);
    if (victim_status == NDB_MGM_NODE_STATUS_STARTED) {
      NdbSleep_SecSleep(1);
    } else {
      g_info << "Node " << deceased << " died after " << timeout << "secs, "
             << " node's status " << victim_status << endl;
      return 0;
    }
  }
  g_err << "Node " << deceased << " has not died after " << timeout << "secs, "
        << endl;
  return 1;
}

/**
 * Reads a config variable id and value from the test context
 * change the config and restarts the data nodes.
 */

int runChangeDataNodeConfig(NDBT_Context *ctx, NDBT_Step *step) {
  int num_config_vars = ctx->getProperty("NumConfigVars", Uint32(1));

  ctx->setProperty("NumConfigVars", Uint32(0));

  for (int c = 1; c <= num_config_vars; c++) {
    BaseString varId;
    BaseString varVal;
    varId.assfmt("ConfigVarId%u", c);
    varVal.assfmt("ConfigValue%u", c);

    Uint32 new_value_read = 0;
    int config_var_id = ctx->getProperty(varId.c_str(), (Uint32)new_value_read);

    Uint32 new_config_value =
        ctx->getProperty(varVal.c_str(), (Uint32)new_value_read);

    g_err << "Setting config " << config_var_id << " val " << new_config_value
          << endl;

    // Override the config
    NdbMgmd mgmd;
    mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
    Uint32 old_config_value = 0;
    CHECK(mgmd.change_config32(new_config_value, &old_config_value,
                               CFG_SECTION_NODE, config_var_id),
          "Change config failed");

    g_err << "  Success, old val : " << old_config_value << endl;

    // Save the old_value in the test property 'config_var%u'.
    ctx->setProperty(varVal.c_str(), old_config_value);
    ctx->setProperty("NumConfigVars", Uint32(c));
  }

  g_err << "Restarting nodes with new config." << endl;

  // Restart cluster to get the new config value
  NdbRestarter restarter;
  CHECK(restarter.restartAll() == 0, "Restart all failed");

  CHECK(restarter.waitClusterStarted() == 0, "Cluster has not started");
  g_err << "Nodes restarted with new config." << endl;
  return NDBT_OK;
}

int runPauseGcpCommitUntilNodeFailure(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() > 4) {
    g_err << endl
          << "ERROR: This test was not run since #data nodes exceeded 4" << endl
          << endl;

    ctx->stopTest();
    return result;
  }

  int master = restarter.getMasterNodeId();
  int victim = restarter.getRandomNotMasterNodeId(rand());

  /* Save current gcp commit lag */
  int dump = DumpStateOrd::DihSaveGcpCommitLag;

  restarter.dumpStateOneNode(master, &dump, 1);

  while (true) {
    // Delay gcp commit conf at victim participant,
    // causing master to kill it eventually
    CHECK2(restarter.insertErrorInNode(victim, 7239) == 0);

    // Error insert to hit CRASH INSERTION on failure so
    // that test framework does not report failure
    CHECK2(restarter.insertErrorInNode(victim, 1005) == 0);

    // Error insert on master to stall takeover when it comes
    CHECK2(restarter.insertErrorInNode(master, 8118) == 0);

    g_err << "Waiting for node " << victim << " to fail." << endl;

    CHECK2(checkOneNodeDead(victim, 400) == 0);
    g_err << "Victim died" << endl;

    // Now master is stalled on takeover

    g_err << "Checking commit lag is unchanged" << endl;

    int dump = DumpStateOrd::DihCheckGcpCommitLag;

    restarter.dumpStateOneNode(master, &dump, 1);

    g_err << "OK : GCP timeout not changed" << endl;

    g_err << "Cleaning up" << endl;
    /**
     * Release master
     */
    CHECK2(restarter.insertErrorInNode(master, 0) == 0);

    g_err << "Waiting victim node " << victim << " to start" << endl;
    CHECK2(restarter.waitNodesStarted(&victim, 1) == 0);
    break;
  }

  ctx->stopTest();
  return result;
}

static const char *NbTabName = "NBTAB";

int runCreateCharKeyTable(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();

  {
    NdbDictionary::Table nbTab;

    nbTab.setName(NbTabName);

    const char *charsetName;
    if (ctx->getProperty("CSCharset", Uint32(0)) == 0) {
      ndbout_c("Using non case-sensitive charset");
      charsetName = "latin1_swedish_ci";
      //    charsetName = "utf8mb3_unicode_ci";
    } else {
      ndbout_c("Using case-sensitive charset");
      charsetName = "latin1_general_cs";
    };

    const Uint32 numDataCols = ctx->getProperty("NumDataColumns", Uint32(1));
    ndbout_c("Using %u data columns", numDataCols);

    {
      NdbDictionary::Column c;
      c.setName("Key");
      c.setType(NdbDictionary::Column::Varchar);
      c.setLength(40);
      c.setCharset(get_charset_by_name(charsetName, MYF(0)));
      /* Charset, length */
      c.setPrimaryKey(true);
      nbTab.addColumn(c);
    }

    for (Uint32 i = 0; i < numDataCols; i++) {
      NdbDictionary::Column c;
      BaseString name;
      name.assfmt("Data_%u", i);
      c.setName(name.c_str());
      c.setType(NdbDictionary::Column::Unsigned);
      nbTab.addColumn(c);
    }

    CHECK(pDict->createTable(nbTab) == 0, pDict->getNdbError());
  }

  CHECK(pDict->getTable(NbTabName) != NULL, pDict->getNdbError());

  return NDBT_OK;
}

int runDropCharKeyTable(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();

  CHECK(pDict->dropTable(NbTabName) == 0, pDict->getNdbError());

  return NDBT_OK;
}

const Uint32 DataSetRows = 26;
const Uint32 NumDataSets = 5;

int runLoadCharKeyTable(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();

  const NdbDictionary::Table *nbTab = pDict->getTable(NbTabName);

  const Uint32 numDataCols = ctx->getProperty("NumDataColumns", Uint32(1));

  /* Load table with rows keyed lower case a to z 0|1|2... */
  for (Uint32 p = 0; p < NumDataSets;
       p++)  // a0, b0, ..z0, a1, .. z1, a2, ... z2, ...
  {
    for (Uint32 i = 0; i < DataSetRows; i++) {
      NdbTransaction *trans = pNdb->startTransaction();
      CHECK(trans != NULL, pNdb->getNdbError());

      NdbOperation *op = trans->getNdbOperation(nbTab);
      CHECK(op != NULL, trans->getNdbError());

      CHECK((op->insertTuple() == 0), op->getNdbError());

      char keyBuf[3];
      keyBuf[0] = 2;
      keyBuf[1] = 'a' + i;
      keyBuf[2] = '0' + p;

      CHECK(op->equal("Key", &keyBuf[0]) == 0, op->getNdbError());
      for (Uint32 c = 0; c < numDataCols; c++) {
        BaseString name;
        name.assfmt("Data_%u", c);
        CHECK(op->setValue(name.c_str(), i) == 0, op->getNdbError());
      }

      CHECK(trans->execute(Commit) == 0, trans->getNdbError());

      trans->close();
    }
  }

  return NDBT_OK;
}

int runCheckCharKeyTable(NDBT_Context *ctx, NDBT_Step *step) {
  /* Check that table has all the expected datasets, and nothing more */
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();

  const NdbDictionary::Table *nbTab = pDict->getTable(NbTabName);
  const Uint32 totalRows = NumDataSets * DataSetRows;
  Uint32 rows[totalRows];

  bool unexpectedValue;
  const Uint32 numDataCols = ctx->getProperty("NumDataColumns", Uint32(1));
  NdbRecAttr *ras[512];
  Uint32 scanRetries = 20;

  do {
    for (Uint32 i = 0; i < totalRows; i++) {
      rows[i] = 0;
    }

    unexpectedValue = false;
    NdbTransaction *trans = pNdb->startTransaction();
    CHECK(trans != NULL, pNdb->getNdbError());

    NdbScanOperation *sop = trans->getNdbScanOperation(nbTab);
    CHECK(sop != NULL, trans->getNdbError());

    CHECK((sop->readTuples(NdbOperation::LM_CommittedRead) == 0),
          sop->getNdbError());

    NdbRecAttr *key;

    CHECK(((key = sop->getValue("Key")) != NULL), sop->getNdbError());
    for (Uint32 c = 0; c < numDataCols; c++) {
      BaseString name;
      name.assfmt("Data_%u", c);
      CHECK(((ras[c] = sop->getValue(name.c_str())) != NULL),
            sop->getNdbError());
    }

    CHECK(trans->execute(NoCommit) == 0, trans->getNdbError());
    /* TODO : Temporary error handling */

    int scanRc;
    while ((scanRc = sop->nextResult()) == 0) {
      /* For each result, we check that the key is as
       * expected, and that the data columns are as
       * expected
       */

      /* Expect key of form xy
       * x = a..z
       * y = 0..NumDataSets-1
       */
      const char *keyData = (const char *)key->aRef();
      const Uint32 keyLen = keyData[0];
      const Uint32 keyChar = keyData[1];
      const Uint32 keySetSym = keyData[2];
      if (keyLen == 2 && (keyChar >= 'a' && keyChar <= 'z') &&
          (keySetSym >= '0' && keySetSym <= ((char)('0' + NumDataSets)))) {
        /* Value in range, count */
        const Uint32 dataSetNum = keySetSym - '0';
        const Uint32 rowNum = keyChar - 'a';
        const Uint32 index = (dataSetNum * DataSetRows) + rowNum;
        rows[index]++;
      } else {
        ndbout_c("Found unexpected key value in table : ");
        unexpectedValue = true;

        for (int i = 0; i < keyData[0]; i++) {
          ndbout_c(" %u : %u %c", i, keyData[1 + i], keyData[1 + i]);
        }
      }

      /* Check data */
      /* Require that each data col key is at most 1 less than first
       * and updates are in sequence
       * e.g.
       *  ok
       *   33 ... 33
       *   33 ... 32
       *  not ok
       *   33 ... 31
       *   33 ... 32 ... 33
       *   32 ... 33
       */
      Uint32 firstValue = 0;
      Uint32 prevValue = 0;
      for (Uint32 c = 0; c < numDataCols; c++) {
        Uint32 val = ras[c]->u_32_value();

        if (c == 0) {
          firstValue = prevValue = val;
        } else {
          if (val != prevValue) {
            if (val == (prevValue - 1) && prevValue == firstValue) {
              /* Step down, ok */
              prevValue = val;
            } else {
              /* Something wrong */
              ndbout_c("Row has incorrect sequences :");
              ndbout_c("Key length %u : %c%c", keyLen, keyChar, keySetSym);

              for (Uint32 k = 0; k < numDataCols; k++) {
                ndbout_c(" %u : %u", k, ras[k]->u_32_value());
              }
              unexpectedValue = true;
              break;
            }
          }
        }
      }
    }  // while nextResult()

    if (scanRc != 1) {
      const bool retry =
          (sop->getNdbError().status == NdbError::TemporaryError);
      ndbout_c("Scan problem : %u : %s ", sop->getNdbError().code,
               sop->getNdbError().message);
      trans->close();

      if (retry && scanRetries--) {
        ndbout_c("Retrying scan, %u retries remain", scanRetries);
        continue;
      } else {
        return NDBT_FAILED;
      }
    }

    trans->close();

    break;
  } while (true);

  /* Check results */
  for (Uint32 i = 0; i < totalRows; i++) {
    if (rows[i] != 1) {
      const char key = 'a' + (i % DataSetRows);
      const Uint32 dataSet = i / DataSetRows;

      unexpectedValue = true;

      if (rows[i] < 1) {
        ndbout_c("Missing row %c%u", key, dataSet);
      } else {
        ndbout_c("Extra row %c%u", key, dataSet);
      }
    }
  }

  if (!unexpectedValue) {
    g_info << "Table content ok" << endl;
    return NDBT_OK;
  }

  return NDBT_FAILED;
}

static int defineDeleteOp(NdbTransaction *trans,
                          const NdbDictionary::Table *nbTab, char keyLen,
                          char byte0, char byte1, char byte2) {
  const char key[4] = {keyLen, byte0, byte1, byte2};

  NdbOperation *delOp = trans->getNdbOperation(nbTab);
  CHECK(delOp != NULL, trans->getNdbError());

  CHECK(delOp->deleteTuple() == 0, delOp->getNdbError());
  CHECK(delOp->equal("Key", &key[0]) == 0, delOp->getNdbError());

  return NDBT_OK;
}

static int defineInsertOp(NdbTransaction *trans,
                          const NdbDictionary::Table *nbTab, Uint32 numDataCols,
                          char keyLen, char byte0, char byte1, char byte2,
                          Uint32 i) {
  const char key[4] = {keyLen, byte0, byte1, byte2};

  NdbOperation *insOp = trans->getNdbOperation(nbTab);
  CHECK(insOp != NULL, trans->getNdbError());

  CHECK(insOp->insertTuple() == 0, insOp->getNdbError());
  CHECK(insOp->equal("Key", &key[0]) == 0, insOp->getNdbError());

  for (Uint32 c = 0; c < numDataCols; c++) {
    BaseString name;
    name.assfmt("Data_%u", c);
    CHECK(insOp->setValue(name.c_str(), i) == 0, insOp->getNdbError());
  }
  return NDBT_OK;
}

static int defineUpdateOp(NdbTransaction *trans,
                          const NdbDictionary::Table *nbTab, Uint32 numDataCols,
                          char keyLen, char byte0, char byte1, char byte2,
                          Uint32 i, Uint32 iterations, Uint32 offset) {
  const char key[4] = {keyLen, byte0, byte1, byte2};

  NdbOperation *insOp = trans->getNdbOperation(nbTab);
  CHECK(insOp != NULL, trans->getNdbError());

  CHECK(insOp->updateTuple() == 0, insOp->getNdbError());
  CHECK(insOp->equal("Key", &key[0]) == 0, insOp->getNdbError());

  /* We just update one column */
  /* Updates retain invariant that col(n+1) = col(0) | col(0)+1 */

  const Uint32 colnum = (iterations - 1) % numDataCols; /* 0...numDataCols */
  BaseString name;
  name.assfmt("Data_%u", colnum);
  CHECK(insOp->setValue(name.c_str(), (offset + i)) == 0, insOp->getNdbError());

  return NDBT_OK;
}

int runChangePkCharKeyTable(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();

  const NdbDictionary::Table *nbTab = pDict->getTable(NbTabName);

  const Uint32 numDataCols = ctx->getProperty("NumDataColumns", Uint32(1));
  const bool case_sensitive_collation =
      (ctx->getProperty("CSCharset", Uint32(0)) != 0);

  bool cycle = false;
  Uint32 iterations = 0;
  Uint32 offset;

  /**
   * Run transactions until stopped which contain
   *
   *   BEGIN
   *     # Same logical key, different actual key
   *       Delete row where pk = 'a0'|'A0'
   *       Insert row setting pk = 'A0' | 'a0'
   *
   *     # Different logical key, different actual key
   *       Delete row where pk = 'a1'|'AQ'
   *       Insert row setting pk = 'AQ'|'a1'
   *
   *     # Delete or Insert just to mix rowids a little
   *       Delete row where pk = 'a2'
   *       or
   *       Insert row where pk = 'A2'
   *
   *     # Same logical key, different actual key
   *        via trailing spaces
   *       Delete row where pk = 'a3' |'A3 '
   *       Insert row setting pk = 'A3 ' | 'a3'
   *
   *     # Same logical key, updating data in a pattern
   *        over time
   *       Update row where pk = 'a4' set col X = y
   *
   *   COMMIT
   *
   * As the table has a case-insensitive (non binary)
   * collation, we need proper collation aware comparisons
   * to be used as appropriate.
   * We can describe the key of a row being looked up (for
   * read, update, delete) using any case and trailing spaces,
   * and expect it to be found.
   * When we insert a row we expect :
   *  - Trailing spaces and case are preserved
   *
   * Mix of different variants to help surface bugs.
   */
  while (!ctx->isTestStopped()) {
    cycle = !cycle;
    offset = 1 + (iterations / numDataCols);
    iterations++;

    /**
     * Periodically check the table content on the
     * true cycle, when data should be in its original
     * state
     */
    if (cycle && (iterations % 33) == 0) {
      if (runCheckCharKeyTable(ctx, step) != NDBT_OK) {
        return NDBT_FAILED;
      }
    }

    for (Uint32 i = 0; i < DataSetRows; i++) {
      while (true)  // Temp retry loop
      {
        /**
         * For case-sensitive collations, we must use correct case
         * when specifying keys.
         * For case-insensitive collations, we do not need to, so use
         * the 'to' case for the key, and the 'to' value.
         */
        const char toCaseKey = ((cycle ? 'A' : 'a') + i);
        const char fromCaseKey =
            case_sensitive_collation ? ((cycle ? 'a' : 'A') + i) : toCaseKey;

        NdbTransaction *trans = pNdb->startTransaction();
        CHECK(trans != NULL, pNdb->getNdbError());

        {
          /* Case 1 */
          /* Single transaction, Key changes only case */
          /* a0..z0 */
          /* a0 -> A0, A0 -> a0 */
          CHECK(defineDeleteOp(trans, nbTab, 2, fromCaseKey, '0', 0) == NDBT_OK,
                "Failed to define delete op 1");
          CHECK(defineInsertOp(trans, nbTab, numDataCols, 2, toCaseKey, '0', 0,
                               i) == NDBT_OK,
                "Failed to define insert op 1");
        }

        {
          /* Case 2 */
          /* Single transaction, Key changes case and other value */
          /* a1..z1 */
          /* a1 -> AQ, AQ -> a1 */
          const char fromKey = cycle ? '1' : 'Q';
          const char toKey = cycle ? 'Q' : '1';

          CHECK(defineDeleteOp(trans, nbTab, 2, fromCaseKey, fromKey, 0) ==
                    NDBT_OK,
                "Failed to define delete op 2");
          CHECK(defineInsertOp(trans, nbTab, numDataCols, 2, toCaseKey, toKey,
                               0, i) == NDBT_OK,
                "Failed to define insert op 2");
        }

        {
          /* Case 3 */
          /* Separate transactions, Delete or Insert (of every second row) */
          /* a2..z2 */
          /* b2 -> -, - -> B2 */

          if (i % 2 == 1) {
            if (cycle) {
              CHECK(defineDeleteOp(trans, nbTab, 2, fromCaseKey, '2', 0) ==
                        NDBT_OK,
                    "Failed to define delete op 3");
            } else {
              CHECK(defineInsertOp(trans, nbTab, numDataCols, 2, toCaseKey, '2',
                                   0, i) == NDBT_OK,
                    "Failed to define insert op 3");
            }
          }
        }

        {
          /* Case 4 */
          /* Single transaction Same key, different data due to trailing space
           */
          /* a3..z3 */
          /* 'a3' -> 'a3 ', 'a3 ' -> 'a3' */

          /* Length of key, without + with trailing data */
          const char keyLen = (cycle ? 3 : 2);
          const char lowerCaseKey = 'a' + i;
          const char caseKey =
              (case_sensitive_collation ? lowerCaseKey
                                        : /* Stick with lower case */
                   toCaseKey);            /* Also flip case */

          CHECK(defineDeleteOp(trans, nbTab, keyLen, caseKey, '3', ' ') ==
                    NDBT_OK,
                "Failed to define delete op 1");
          CHECK(defineInsertOp(trans, nbTab, numDataCols, keyLen, caseKey, '3',
                               ' ', i) == NDBT_OK,
                "Failed to define insert op 1");
        }

        {
          /* Case 5 */
          /* Update column values inplace, using diff key */
          /* a4..z4 */
          /* UPDATE A4 set data_2 = <next> */
          /* UPDATE A4 set data_3 = <next> */
          /* e.g. :
           *  Key  Col0  Col1  Col2  Col3 .. Coln
           *  'a4'    0     0     0     0       0
           *          1     0     0     0       0
           *          1     1     0     0       0
           *          ...
           *          1     1     1     1       1
           *          2     1     1     1       1
           *          ...
           *
           * Intention is that missing updates on replicas
           * become visible (as the pattern above is broken)
           * This is checked in runCheckCharKeyTable()
           */
          const char lowerCaseKey = 'a' + i;
          const char upperCaseKey = 'A' + i;
          const char caseKey =
              (case_sensitive_collation ? lowerCaseKey
                                        : /* Stick with lower case */
                   upperCaseKey);         /* Always use the 'wrong' case */

          CHECK(defineUpdateOp(trans, nbTab, numDataCols, 2, caseKey, '4', 0, i,
                               iterations, offset) == NDBT_OK,
                "Failed to define update op");
        }

        if (trans->execute(Commit) != 0) {
          char buf[2] = {toCaseKey, 0};
          g_err << "Failed to execute transaction " << trans->getNdbError()
                << endl;
          g_err << "Cycle " << cycle << " i " << i << " toCaseKey " << buf
                << endl;
          if (trans->getNdbError().status == NdbError::TemporaryError) {
            /* Ignore temporary errors due to restarts etc */
            trans->close();
            continue;
          }
          return NDBT_FAILED;
        }
        trans->close();

        break;
      }
    }
  }

  return NDBT_OK;
}

int runErrorInsertSlowCopyFrag(NDBT_Context *ctx, NDBT_Step *step) {
  /* Slow down CopyFrag, to give more time to find errors */
  NdbRestarter restarter;

  return restarter.insertErrorInAllNodes(5106);
}

int runClearErrorInsert(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  return restarter.insertErrorInAllNodes(0);
}

int runWatchdogSlowShutdown(NDBT_Context *ctx, NDBT_Step *step) {
  /* Steps
   * 1 Set low watchdog threshold
   * 2 Get error reporter to be slow during shutdown
   * 3 Trigger shutdown
   *
   * Expectation
   * - Shutdown triggered, but slow
   * - Watchdog detects and also attempts shutdown
   * - No crash results, shutdown completes eventually
   */

  NdbRestarter restarter;

  /* 1 Set low watchdog threshold */
  {
    const int dumpVals[] = {DumpStateOrd::CmvmiSetWatchdogInterval, 2000};
    CHECK((restarter.dumpStateAllNodes(dumpVals, 2) == NDBT_OK),
          "Failed to set watchdog thresh");
  }

  /* 2 Use error insert to get error reporter to be slow
   *   during shutdown
   */
  {
    const int dumpVals[] = {DumpStateOrd::CmvmiSetErrorHandlingError, 1};
    CHECK((restarter.dumpStateAllNodes(dumpVals, 2) == NDBT_OK),
          "Failed to set error handling mode");
  }

  /* 3 Trigger shutdown */
  const int nodeId = restarter.getNode(NdbRestarter::NS_RANDOM);
  g_err << "Injecting crash in node " << nodeId << endl;
  /* First request a 'NOSTART' restart on error insert */
  {
    const int dumpVals[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
    CHECK((restarter.dumpStateOneNode(nodeId, dumpVals, 2) == NDBT_OK),
          "Failed to request error insert restart");
  }

  /* Next cause an error insert failure */
  CHECK((restarter.insertErrorInNode(nodeId, 9999) == NDBT_OK),
        "Failed to request node crash");

  /* Expect shutdown to be stalled, and shortly after, watchdog
   * to detect this and act
   */
  g_err << "Waiting for node " << nodeId << " to stop." << endl;
  CHECK((restarter.waitNodesNoStart(&nodeId, 1) == NDBT_OK),
        "Timeout waiting for node to stop");

  g_err << "Waiting for node " << nodeId << " to start." << endl;
  CHECK((restarter.startNodes(&nodeId, 1) == NDBT_OK),
        "Timeout waiting for node to start");

  CHECK((restarter.waitClusterStarted() == NDBT_OK),
        "Timeout waiting for cluster to start");

  g_err << "Success" << endl;
  return NDBT_OK;
}

int runWatchdogSlowShutdownCleanup(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  g_err << "Cleaning up" << endl;

  /* Cleanup special measures */
  {
    const int dumpVals[] = {DumpStateOrd::CmvmiSetWatchdogInterval};
    if (restarter.dumpStateAllNodes(dumpVals, 1) != NDBT_OK) {
      g_err << "Failed to clear interval" << endl;
      return NDBT_FAILED;
    }
  }
  {
    const int dumpVals[] = {DumpStateOrd::CmvmiSetErrorHandlingError};
    if (restarter.dumpStateAllNodes(dumpVals, 1) != NDBT_OK) {
      g_err << "Failed to clear error handlng" << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runApiDetectNoFirstHeartbeat(NDBT_Context *ctx, NDBT_Step *step) {
  /* Steps
   * 1 Stop a random data node from witch link to API node will be blocked
   * 2 Connect new API Node
   * 3 Block data node for sending signals to API node
   * 4 Start data node
   *
   * Expectation
   * - API node disconnected after 60 secs timeout
   */

  NdbRestarter restarter;

  const int nodeId = restarter.getNode(NdbRestarter::NS_RANDOM);
  g_err << "Stop target Data Node." << endl;
  if (restarter.restartOneDbNode(nodeId,
                                 /** initial */ false,
                                 /** nostart */ true,
                                 /** abort   */ true)) {
    return NDBT_FAILED;
  }

  if (restarter.waitNodesNoStart(&nodeId, 1)) return NDBT_FAILED;

  g_err << "Connect new API Node." << endl;
  Ndb_cluster_connection *cluster_connection = new Ndb_cluster_connection();
  cluster_connection->configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (cluster_connection->connect() != 0) {
    g_err << "ERROR: connect failure." << endl;
    return NDBT_FAILED;
  }

  Ndb *ndb = new Ndb(cluster_connection, "TEST_DB");
  if (ndb->init() != 0) {
    g_err << "ERROR: Ndb::init failure." << endl;
    return NDBT_FAILED;
  }
  if (ndb->waitUntilReady(30) != 0) {
    g_err << "ERROR: Ndb::waitUntilReady timeout." << endl;
    return NDBT_FAILED;
  }

  const int apiNodeId = ndb->getNodeId();
  g_err << "Blocking node " << nodeId << " for sending signals to API node "
        << apiNodeId << "." << endl;
  const int dumpCodeBlockSend[] = {9988, apiNodeId};
  const int dumpCodeUnblockSend[] = {9989, apiNodeId};
  if (restarter.dumpStateOneNode(nodeId, dumpCodeBlockSend, 2)) {
    g_err << "Dump state failed." << endl;
    return NDBT_FAILED;
  }

  g_err << "Start target Data Node." << endl;
  if (restarter.startNodes(&nodeId, 1) != 0) {
    g_err << "Wait node start failed" << endl;
    CHECK(
        (restarter.dumpStateOneNode(nodeId, dumpCodeUnblockSend, 2)) == NDBT_OK,
        "Dump state failed.")
    return NDBT_FAILED;
  }

  if (restarter.waitClusterStarted() != 0) {
    g_err << "ERROR: Cluster failed to start" << endl;
    CHECK(
        (restarter.dumpStateOneNode(nodeId, dumpCodeUnblockSend, 2)) == NDBT_OK,
        "Dump state failed.")
    return NDBT_FAILED;
  }

  struct ndb_logevent event;
  int filter[] = {15, NDB_MGM_EVENT_CATEGORY_CONNECTION, 0};
  NdbLogEventHandle handle =
      ndb_mgm_create_logevent_handle(restarter.handle, filter);

  Uint32 timeout = 65000;
  while (ndb_logevent_get_next(handle, &event, 1) >= 0 &&
         event.type != NDB_LE_Disconnected && timeout > 0) {
    timeout--;
  }
  ndb_mgm_destroy_logevent_handle(&handle);

  g_err << "Cleaning up" << endl;
  // disconnect api node
  delete ndb;
  delete cluster_connection;

  CHECK((restarter.dumpStateOneNode(nodeId, dumpCodeUnblockSend, 2)) == NDBT_OK,
        "Dump state failed.")

  if (timeout == 0) {
    g_err << "Timeout waiting for node " << nodeId << " to disconnect." << endl;
    return NDBT_FAILED;
  }
  if (event.NODE_FAILREP.failed_node != static_cast<Uint32>(apiNodeId)) {
    g_err << "Node " << event.NODE_FAILREP.failed_node << " disconnect "
          << "Expected node to disconnect is " << apiNodeId << "." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

/**
 * LCPFragWatchdog (LCPFSW) monitors LCP progress and stops the DB node if no
 * progress is made for a max lag, initialized by the config
 * variable DB_LCP_SCAN_WATCHDOG_LIMIT = 60000 ms.  However this is
 * relaxed in one phase of the LCP (LCP_WAIT_END_LCP) to allow for the
 * worst case GCP completion, as the LCP requires a GCP to complete,
 * and that may take more time than the configured LCP 'stall'
 * limit. Max time a GCP is allowed to complete (gcp_stop_timer)
 * depending primarily on the number of nodes in the cluster at any
 * time, and is recalculated when nodes leave or join.
 *
 * The test case tests whether the lcp watchdog limit reflects the
 * newly calculated values in the following cases :
 * - 1) after all configured nodes joined initially
 * - 2) one node leves while the system is running
 * - 3) the node left in 2) rejoins.

 * The test case runs the following steps after the above 3 scenarios :
 *
 * - delays GCP_SAVEREQ by error insertion (EI) to stall GCP.  This
 *   tests the behaviour of the LCPFSWs when GCP is stalled for longer
 *   than the configured LCPFSW limit.  One sub-case of that is where
 *   the GCP is stalled for an LCP which was running prior to a node
 *   completing its start.
 *
 * - waits for 3*lcp_max_lag, which is a little longer than the expected
 *   LCP max lag.
 *
 * - clears EI and sleeps for 'clear_error_insert_seconds' to allow
 *   the delayed GCP and LCP to complete.
 *
 * The test will fail if the calculated values is not applied to newer LCPs.
 * Checked manually whether the newly calculated values in all 3 scenarios
 * are applied.
 */
int runDelayGCP_SAVEREQ(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  Uint32 db_node_count = restarter.getNumDbNodes();

  if (db_node_count == 2) {
    /**
     * With just 2 nodes, in the node stopped case the survivor
     * is Master and so the non-Master timer-change code is
     * not exercised.
     */
    g_err << "Number of db nodes found " << db_node_count
          << ".  The test gives better coverage with 3 or more nodes." << endl;
  }

  int result = NDBT_OK;
  unsigned int timeout = 240;  // To suite many node/replica tests
  const int victim = restarter.getNode(NdbRestarter::NS_RANDOM);
  const Uint32 lcp_max_lag = ctx->getProperty("MaxLcpLag", Uint32(60));
  const Uint32 clear_error_insert_seconds = 60;

  for (int scenario = 1; scenario < 4; scenario++) {
    switch (scenario) {
      case 1:
        g_err << "Scenario 1 : block GCP, check no LCP stall" << endl;
        break;
      case 2:
        g_err << "Scenario 2 : Stop node, block GCP, check no LCP stall"
              << endl;
        g_err << "Stopping node : " << victim << endl;
        CHECK2(restarter.restartOneDbNode(victim, true, /* initial */
                                          true,         /* nostart  */
                                          false,        /* abort */
                                          false /* force */) == 0);
        g_err << "Waiting until node " << victim << " stops" << endl;
        restarter.waitNodesNoStart(&victim, 1, timeout);
        break;
      case 3:
        g_err << "Scenario 3 : Start node, block GCP, check no LCP stall"
              << endl;
        g_err << "Starting node " << victim << endl;
        CHECK2(restarter.startNodes(&victim, 1) == 0);
        CHECK2(restarter.waitClusterStarted(timeout) == 0);
        break;
      default:
        abort();
    }

    g_err << "Inserting err delaying GCP_SAVEREQ" << endl;
    CHECK2(restarter.insertErrorInAllNodes(7237) == 0);

    g_err << "Sleeping for 3 * MaxLcpLag = " << 3 * lcp_max_lag << " seconds."
          << endl;
    NdbSleep_SecSleep(3 * lcp_max_lag);

    // Remove the error insertion and let the GCP and LCP to finish
    CHECK2(restarter.insertErrorInAllNodes(0) == 0);

    g_err << "Sleeping for " << clear_error_insert_seconds
          << "s to allow GCP and LCP to resume." << endl;
    NdbSleep_SecSleep(clear_error_insert_seconds);
  }

  ctx->stopTest();
  return result;
}

/* Basic callback data + function */
struct CallbackData {
  int ready;
  int result;
};

void asyncCallbackFn(int res, NdbTransaction *pCon, void *data) {
  CallbackData *cbd = (CallbackData *)data;

  if (res) {
    cbd->result = pCon->getNdbError().code;
  } else {
    cbd->result = 0;
  }

  /* todo : sync */
  cbd->ready = 1;
}

int runTestStallTimeout(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Testing for fix of bug#22602898
   *   NDB : CURIOUS STATE OF TC COMMIT_SENT / COMPLETE_SENT TIMEOUT HANDLING
   *
   * This fix removed the 'switch to serial commit/complete
   * protocol due to transaction timeout behaviour.
   * This is done as the serial commit/complete protocol further
   * slows the system when a timeout is detected.
   *
   * This means that if we stall the normal parallel commit/complete
   * signal handlers then commit/complete is stalled indefinitely,
   * whereas before it would switch protocol and complete.
   *
   * This behavioural change is tested here.
   */
  Ndb *pNdb = GETNDB(step);
  NdbRestarter restarter;

  struct TestCase {
    const char *type;
    NdbTransaction::ExecType execType;
    int errorCode;
    bool execOk;
  };

  TestCase testcases[] = {
      {
          "Stall in commit",                   /* LQH execCOMMIT()    */
          NdbTransaction::Commit, 5110, false, /* Commit stall blocks API ack */
      },
      {
          "Stall in complete",               /* LQH execCOMPLETE()  */
          NdbTransaction::Commit, 5111, true /* Complete stall does not block
                                              * API ack (ReadPrimary) */
      }};

  for (int stallPoint = 0; stallPoint < 2; stallPoint++) {
    HugoOperations hugoOps(*ctx->getTab());
    TestCase &test = testcases[stallPoint];

    ndbout_c("- *** Case : %s ***", test.type);

    /* Prepare some update operations on a number of rows */
    const Uint32 numUpdates = 10;
    CHECK(hugoOps.startTransaction(pNdb) == 0, "Start transaction failed");
    CHECK(hugoOps.pkUpdateRecord(pNdb, 1, numUpdates) == 0,
          "Define Updates failed");
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0, "Execute NoCommit failed");

    int errorCode = test.errorCode;
    ndbout_c("  - Inserting error %u on all data nodes", errorCode);
    CHECK(restarter.insertErrorInAllNodes(errorCode) == 0,
          "Error insert failed");

    ndbout_c("  - Sending commit with async api");

    NdbTransaction *trans = hugoOps.getTransaction();
    CallbackData cbd;
    cbd.ready = 0;
    cbd.result = 0;

    trans->executeAsynchPrepare(test.execType, asyncCallbackFn, &cbd);
    pNdb->sendPreparedTransactions();

    CHECK(trans->getNdbError().code == 0, "Async send failed");

    const int waitTime = 5;
    ndbout_c("  - Waiting for up to %u seconds for result", waitTime);

    for (int i = 0; i < waitTime; i++) {
      pNdb->pollNdb(1000);

      if (cbd.ready) {
        break;
      }
    }

    if (cbd.ready != test.execOk) {
      /* Mismatch with expectations */
      ndbout_c("cbd.ready : %u  test.execOk : %u, failed.", cbd.ready,
               test.execOk);
      /* Clear error insert and wait for cleanup */
      restarter.insertErrorInAllNodes(0);
      pNdb->pollNdb(20000);
      return NDBT_FAILED;
    }

    if (cbd.ready) {
      ndbout_c("  - Got a result : OK");
    } else {
      ndbout_c("  - No result after %u seconds : OK", waitTime);
    }

    ndbout_c(
        "  - Check that we cannot perform a further update on the same rows");
    {
      HugoOperations hugoOps2(*ctx->getTab());

      /* Prepare an update on the same rows from a different transaction */
      CHECK(hugoOps2.startTransaction(pNdb) == 0, "Start transaction failed");
      CHECK(hugoOps2.pkUpdateRecord(pNdb, 1, 10) == 0, "Define updates failed");

      NdbTransaction *trans2 = hugoOps2.getTransaction();
      CallbackData cbd2;
      cbd2.ready = 0;
      cbd2.result = 0;

      /* This will block as the first transaction has not
       * managed to commit/complete, and row locks
       * are still held
       */
      trans2->executeAsynchPrepare(NdbTransaction::Commit, asyncCallbackFn,
                                   &cbd2);
      pNdb->sendPreparedTransactions();

      CHECK(trans2->getNdbError().code == 0, "Async send2 failed");

      ndbout_c("    - Waiting for up to %u seconds for result", waitTime);

      /* For commit + complete blocking, update will fail
       * after TDDT.
       */
      for (int i = 0; i < waitTime; i++) {
        pNdb->pollNdb(1000);

        if (cbd2.ready) {
          /* Commit + Complete stalls */
          break;
        }
      }

      ndbout_c("    - Removing error insert");
      restarter.insertErrorInAllNodes(0);

      if (!cbd2.ready) {
        const int FurtherDelay = 5;
        ndbout_c("    - Waited for %us with no result on second update",
                 waitTime);
        ndbout_c("    - Waiting for a further %us with no stall", FurtherDelay);
        for (int i = 0; i < FurtherDelay; i++) {
          pNdb->pollNdb(1000);

          if (cbd2.ready) break;
        }

        if (!cbd2.ready) {
          ndbout_c("No result at all - failed.");
          pNdb->pollNdb(20000);
          return NDBT_FAILED;
        }
      }
      ndbout_c("    - Received response on second update");

      ndbout_c("    - Checking that second update received timeout");

      if (trans2->getNdbError().code != 266) {
        ndbout_c("Error, expected 266, but got %u %s",
                 trans2->getNdbError().code, trans2->getNdbError().message);
        return NDBT_FAILED;
      }

      CHECK(hugoOps2.closeTransaction(pNdb) == 0,
            "Failed to close transaction");
    }

    ndbout_c("  - Waiting for result of first request");
    const int FurtherDelay = 2;
    pNdb->pollNdb(FurtherDelay * 1000);

    if (!cbd.ready) {
      ndbout_c("No result on first request after %u seconds, failed",
               waitTime + FurtherDelay);
      pNdb->pollNdb(20000);
      return NDBT_FAILED;
    }

    ndbout_c("  - Original request result : %u", cbd.result);
    CHECK(cbd.result == 0, "Transaction failed");
  }

  return NDBT_OK;
}

int runTestStallTimeoutAndNF(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Testing for fix of bug#22602898
   *   NDB : CURIOUS STATE OF TC COMMIT_SENT / COMPLETE_SENT TIMEOUT HANDLING
   *
   * This fix removed the 'switch to serial commit/complete
   * protocol due to transaction timeout behaviour.
   * This is done as the serial commit/complete protocol further
   * slows the system when a timeout is detected.
   *
   * However we still need the serial commit/complete protocol to
   * handle node failures :
   *  - Failure of participant
   *    Surviving TC will switch protocol to commit/complete
   *    the transaction remains
   *  - Failure of TC
   *    Master TC will gather transaction state, then commit/complete
   *    the remains using a different (non stalled) protocol
   */
  Ndb *pNdb = GETNDB(step);
  NdbRestarter restarter;

  struct TestCase {
    const char *type;
    NdbTransaction::ExecType execType;
    int errorCode;
  };

  TestCase testcases[] = {{
                              "Stall in commit",
                              NdbTransaction::Commit,
                              5110,
                          },
                          {
                              "Stall in complete",
                              NdbTransaction::Commit,
                              5111,
                          }};

  const char *failTypes[] = {"Participant failure", "TC failure"};

  for (int failType = 0; failType < 2; failType++) {
    ndbout_c("Scenario : %s", failTypes[failType]);

    for (int stallPoint = 0; stallPoint < 2; stallPoint++) {
      TestCase &test = testcases[stallPoint];

      ndbout_c("  Stall case : %s", test.type);

      HugoOperations hugoOps(*ctx->getTab());

      /* Prepare a single update operation on a row,
       * in a single transaction hinted for the row
       */
      int rowNum = ndb_rand() % ctx->getNumRecords();

      ndbout_c("   - Preparing update on row %u", rowNum);

      CHECK(hugoOps.startTransaction(pNdb, rowNum) == 0,
            "Start transaction failed");
      CHECK(hugoOps.pkUpdateRecord(pNdb, rowNum, 1) == 0,
            "Define Update failed");
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0, "Execute NoCommit failed");

      NdbTransaction *trans = hugoOps.getTransaction();
      int primaryNodeId = trans->getConnectedNodeId();
      int participantNodeId =
          restarter.getRandomNodeSameNodeGroup(primaryNodeId, ndb_rand());

      ndbout_c("   - Performing error insert on primary node %u",
               primaryNodeId);
      CHECK(restarter.insertErrorInNode(primaryNodeId, test.errorCode) == 0,
            "Failed to insertError");

      ndbout_c("   - Executing commit/abort");
      CallbackData cbd;
      cbd.ready = 0;
      cbd.result = 0;

      trans->executeAsynchPrepare(test.execType, asyncCallbackFn, &cbd);
      pNdb->sendPreparedTransactions();

      CHECK(trans->getNdbError().code == 0, "Async send failed");

      const int waitTime = 5;
      for (int i = 0; i < waitTime; i++) {
        pNdb->pollNdb(1000);

        if (cbd.ready) {
          ndbout_c("     Result ready : %u", trans->getNdbError().code);
          break;
        }
      }
      if (!cbd.ready) {
        ndbout_c("     No result yet");
      }

      /* Transaction stalled now */

      /* Next, restart a node */
      /* For participant failure, restart non-TC node which should be
       * backup -> TC knows trans outcome so will handle.
       * For TC failure, restart TC node which will be taken over
       * by master.  As backup was not stalled, it knows outcome
       */
      int nodeToRestart = (failType == 0) ? participantNodeId : primaryNodeId;

      ndbout_c("   - Transaction stalled, now restarting node %u",
               nodeToRestart);

      CHECK(restarter.restartOneDbNode(nodeToRestart, false, false,
                                       true, /* abort */
                                       false) == 0,
            "Failed node restart");

      CHECK(restarter.waitNodesStarted(&nodeToRestart, 1) == 0,
            "Failed waiting for node to recover");

      ndbout_c("   - Restart complete, now checking trans result");

      // Now, wait for result to materialise and
      // check it

      for (int i = 0; i < waitTime; i++) {
        pNdb->pollNdb(1000);

        if (cbd.ready) {
          break;
        }
      }

      if (!cbd.ready) {
        ndbout_c("Failed to get any result");
        restarter.insertErrorInAllNodes(0);
        return NDBT_FAILED;
      }

      if (trans->getNdbError().code != 0) {
        ndbout_c("Got unexpected failure code : %u : %s",
                 trans->getNdbError().code, trans->getNdbError().message);
        return NDBT_FAILED;
      }

      NdbTransaction::CommitStatusType cst = trans->commitStatus();

      if (cst != NdbTransaction::Committed) {
        ndbout_c("ERROR : Bad commitstatus.  Expected %u, got %u",
                 NdbTransaction::Committed, cst);
        restarter.insertErrorInAllNodes(0);
        return NDBT_FAILED;
      }

      ndbout_c("   - Result ok, clearing error insert");

      restarter.insertErrorInAllNodes(0);
    } /* stallpoint */
  }   /* failType */

  return NDBT_OK;
}

int runLargeLockingReads(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int readsize = MIN(100, ctx->getNumRecords());
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.pkReadRecords(GETNDB(step), readsize, readsize,
                                NdbOperation::LM_Read) != 0) {
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runRestartsWithSlowCommitComplete(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  NdbRestarter restarter;
  const int numRestarts = 4;

  if (restarter.getNumDbNodes() < 2) {
    g_err << "Too few nodes" << endl;
    ctx->stopTest();
    return NDBT_SKIPPED;
  }

  for (int i = 0; i < numRestarts && !ctx->isTestStopped(); i++) {
    int errorCode = 8123;  // Slow commit and complete sending at TC
    ndbout << "Injecting error " << errorCode << " for slow commits + completes"
           << endl;
    restarter.insertErrorInAllNodes(errorCode);

    /* Give some time for things to get stuck in slowness */
    NdbSleep_MilliSleep(1000);

    const int id = restarter.getNode(NdbRestarter::NS_RANDOM);
    ndbout << "Restart node " << id << endl;

    if (restarter.restartOneDbNode(id, false, true, true) != 0) {
      g_err << "Failed to restart Db node" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitNodesNoStart(&id, 1)) {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    restarter.insertErrorInAllNodes(0);

    if (restarter.startNodes(&id, 1)) {
      g_err << "Failed to start node" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitClusterStarted() != 0) {
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    /* Ensure connected */
    if (GETNDB(step)->get_ndb_cluster_connection().wait_until_ready(30, 30) !=
        0) {
      g_err << "Timeout waiting for NdbApi reconnect" << endl;
      result = NDBT_FAILED;
      break;
    }
  }

  restarter.insertErrorInAllNodes(0);
  ctx->stopTest();

  return result;
}

NDBT_TESTSUITE(testNodeRestart);
TESTCASE("NoLoad",
         "Test that one node at a time can be stopped and then restarted "
         "when there are no load on the system. Do this loop number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("PkRead",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read while restarting. Do this loop number of times") {
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_Read);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadCommitted",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read while restarting. Do this loop number of times") {
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_CommittedRead);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedPkRead",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read while restarting. Do this loop number of times") {
  TC_PROPERTY("ReadLockMode", Uint32(-1));
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadPkUpdate",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read and pk update while restarting. Do this loop number "
         "of times") {
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_Read);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedPkReadPkUpdate",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read and pk update while restarting. Do this loop number "
         "of times") {
  TC_PROPERTY("ReadLockMode", Uint32(-1));
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("ReadUpdateScan",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read, pk update and scan reads while restarting. Do this "
         "loop number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedReadUpdateScan",
         "Test that one node at a time can be stopped and then restarted "
         "perform pk read, pk update and scan reads while restarting. Do this "
         "loop number of times") {
  TC_PROPERTY("ReadLockMode", Uint32(-1));
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Terror",
         "Test that one node at a time can be stopped and then restarted "
         "perform all kind of transactions while restarting. Do this loop "
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("FullDb",
         "Test that one node at a time can be stopped and then restarted "
         "when db is full. Do this loop number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runFillTable);
  STEP(runRestarter);
}
TESTCASE("RestartRandomNode",
         "Test that we can execute the restart RestartRandomNode loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNodeError",
         "Test that we can execute the restart RestartRandomNodeError loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNodeInitial",
         "Test that we can execute the restart RestartRandomNodeInitial loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartNFDuringNR",
         "Test that we can execute the restart RestartNFDuringNR loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartMasterNodeError",
         "Test that we can execute the restart RestartMasterNodeError loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("GetTabInfoOverload",
         "Test behaviour of GET_TABINFOREQ overload + LCP + restart") {
  TC_PROPERTY("NumTables", 20);
  INITIALIZER(createManyTables);
  STEPS(runGetTabInfo, 20);
  STEP(runLCPandRestart);
  FINALIZER(dropManyTables);
};

TESTCASE("TwoNodeFailure",
         "Test that we can execute the restart TwoNodeFailure\n"
         "(which is a multiple node failure restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("TwoMasterNodeFailure",
         "Test that we can execute the restart TwoMasterNodeFailure\n"
         "(which is a multiple node failure restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("FiftyPercentFail",
         "Test that we can execute the restart FiftyPercentFail\n"
         "(which is a multiple node failure restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodes",
         "Test that we can execute the restart RestartAllNodes\n"
         "(which is a system  restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodesAbort",
         "Test that we can execute the restart RestartAllNodesAbort\n"
         "(which is a system  restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodesError9999",
         "Test that we can execute the restart RestartAllNodesError9999\n"
         "(which is a system  restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("FiftyPercentStopAndWait",
         "Test that we can execute the restart FiftyPercentStopAndWait\n"
         "(which is a system  restart) loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartNodeDuringLCP",
         "Test that we can execute the restart RestartRandomNode loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("StopOnError",
         "Test StopOnError. A node that has StopOnError set to false "
         "should restart automatically when an error occurs") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNamedRestartTest);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("CommittedRead", "Test committed read") {
  INITIALIZER(runLoadTable);
  STEP(runDirtyRead);
  FINALIZER(runClearTable);
}
TESTCASE("LateCommit", "Test commit after node failure") {
  INITIALIZER(runLoadTable);
  STEP(runLateCommit);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15587", "Test bug with NF during NR") {
  INITIALIZER(runLoadTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runBug15587);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15632", "Test bug with NF during NR") {
  INITIALIZER(runLoadTable);
  STEP(runBug15632);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15685", "Test bug with NF during abort") {
  STEP(runBug15685);
  FINALIZER(runClearTable);
}
TESTCASE("Bug16772",
         "Test bug with restarting before NF handling is complete") {
  STEP(runBug16772);
}
TESTCASE("Bug18414", "Test bug with NF during NR") {
  INITIALIZER(runLoadTable);
  STEP(runBug18414);
  FINALIZER(runClearTable);
}
TESTCASE("Bug18612", "Test bug with partitioned clusters") {
  INITIALIZER(runLoadTable);
  STEP(runBug18612);
  FINALIZER(runClearTable);
}
TESTCASE("Bug18612SR", "Test bug with partitioned clusters") {
  INITIALIZER(runLoadTable);
  STEP(runBug18612SR);
  FINALIZER(runRestartAllNodes);
  FINALIZER(runClearTable);
}
TESTCASE("Bug20185", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug20185);
  FINALIZER(runClearTable);
}
TESTCASE("Bug24543", "") { INITIALIZER(runBug24543); }
TESTCASE("Bug21271", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug21271);
  STEP(runPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Bug24717", "") { INITIALIZER(runBug24717); }
TESTCASE("Bug25364", "") { INITIALIZER(runBug25364); }
TESTCASE("Bug25468", "") { INITIALIZER(runBug25468); }
TESTCASE("Bug25554", "") { INITIALIZER(runBug25554); }
TESTCASE("Bug25984", "") { INITIALIZER(runBug25984); }
TESTCASE("Bug26457", "") { INITIALIZER(runBug26457); }
TESTCASE("Bug26481", "") { INITIALIZER(runBug26481); }
TESTCASE("InitialNodeRestartTest", "") {
  INITIALIZER(runInitialNodeRestartTest);
}
TESTCASE("Bug26450", "") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug26450);
}
TESTCASE("Bug27003", "") { INITIALIZER(runBug27003); }
TESTCASE("Bug27283", "") { INITIALIZER(runBug27283); }
TESTCASE("Bug27466", "") { INITIALIZER(runBug27466); }
TESTCASE("Bug28023", "") { INITIALIZER(runBug28023); }
TESTCASE("Bug28717", "") { INITIALIZER(runBug28717); }
TESTCASE("Bug31980", "") { INITIALIZER(runBug31980); }
TESTCASE("Bug29364", "") {
  INITIALIZER(changeStartPartitionedTimeout);
  INITIALIZER(runBug29364);
  FINALIZER(changeStartPartitionedTimeout);
}
TESTCASE("GCP", "") {
  INITIALIZER(runLoadTable);
  STEP(runGCP);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("CommitAck", "") {
  INITIALIZER(runCommitAck);
  FINALIZER(runClearTable);
}
TESTCASE("Bug32160", "") { INITIALIZER(runBug32160); }
TESTCASE("pnr", "Parallel node restart") {
  TC_PROPERTY("ScanUpdateNoRowCountCheck", 1);
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateBigTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runDeleteInsertUntilStopped);
  STEP(runPnr);
  FINALIZER(runClearTable);
  FINALIZER(runDropBigTable);
}
TESTCASE("pnr_lcp", "Parallel node restart") {
  TC_PROPERTY("LCP", 1);
  TC_PROPERTY("ScanUpdateNoRowCountCheck", 1);
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateBigTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runDeleteInsertUntilStopped);
  STEP(runPnr);
  FINALIZER(runClearTable);
  FINALIZER(runDropBigTable);
}
TESTCASE("Bug32922", "") { INITIALIZER(runBug32922); }
TESTCASE("Bug34216", "") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runBug34216);
  FINALIZER(runClearTable);
}
TESTCASE("mixedmultiop", "") {
  TC_PROPERTY("MULTI_OP", 5);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNF_commit);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Bug34702", "") { INITIALIZER(runBug34702); }
TESTCASE("MNF", "") {
  INITIALIZER(runLoadTable);
  STEP(runMNF);
  STEP(runScanUpdateUntilStopped);
}
TESTCASE("Bug36199", "") { INITIALIZER(runBug36199); }
TESTCASE("Bug36246", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug36246);
  VERIFIER(runClearTable);
}
TESTCASE("Bug36247", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug36247);
  VERIFIER(runClearTable);
}
TESTCASE("Bug36276", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug36276);
  VERIFIER(runClearTable);
}
TESTCASE("Bug36245", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug36245);
  VERIFIER(runClearTable);
}
TESTCASE("NF_Hammer", "") {
  TC_PROPERTY("Sleep0", 9000);
  TC_PROPERTY("Sleep1", 3000);
  TC_PROPERTY("Rand", 1);
  INITIALIZER(runLoadTable);
  STEPS(runHammer, 25);
  STEP(runRestarter);
  VERIFIER(runClearTable);
}
TESTCASE("Bug41295", "") {
  TC_PROPERTY("Threads", 25);
  INITIALIZER(runLoadTable);
  STEPS(runMixedLoad, 25);
  STEP(runBug41295);
  FINALIZER(runClearTable);
}
TESTCASE("Bug41469", "") {
  INITIALIZER(runLoadTable);
  STEP(runBug41469);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Bug42422", "") { INITIALIZER(runBug42422); }
TESTCASE("Bug43224", "") { INITIALIZER(runBug43224); }
TESTCASE("Bug58453", "") { INITIALIZER(runBug58453); }
TESTCASE("Bug43888", "") { INITIALIZER(runBug43888); }
TESTCASE("Bug44952",
         "Test that we can execute the restart RestartNFDuringNR loop\n"
         "number of times") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runBug44952);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("Bug48474", "") {
  INITIALIZER(runLoadTable);
  INITIALIZER(initBug48474);
  STEP(runBug48474);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(cleanupBug48474);
}
TESTCASE("MixReadUnlockRestart",
         "Run mixed read+unlock and update transactions") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadPkUpdatePkUnlockUntilStopped);
  STEP(runPkReadPkUpdatePkUnlockUntilStopped);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("Bug56044", "") { INITIALIZER(runBug56044); }
TESTCASE("Bug57767", "") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug57767)
}
TESTCASE("Bug57522", "") { INITIALIZER(runBug57522); }
TESTCASE("Bug16944817", "") { INITIALIZER(runBug16944817); }
TESTCASE("MasterFailSlowLCP",
         "DIH Master failure during a slow LCP can cause a crash.") {
  INITIALIZER(runMasterFailSlowLCP);
}
TESTCASE("TestLCPFSErr", "Test LCP FS Error handling") {
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runTestLcpFsErr);
}
TESTCASE("ForceStopAndRestart", "Test restart and stop -with force flag") {
  STEP(runForceStopAndRestart);
}
TESTCASE("ClusterSplitLatency",
         "Test behaviour of 2-replica cluster with latency between halves") {
  TC_PROPERTY("DynamicOrder", Uint32(9));
  INITIALIZER(runRestartToDynamicOrder);
  INITIALIZER(analyseDynamicOrder);
  INITIALIZER(runSplitLatency25PctFail);
}
TESTCASE("GCPStopFalsePositive",
         "Test node failures is not misdiagnosed as GCP stop") {
  INITIALIZER(runIsolateMaster);
}
TESTCASE("LCPTakeOver", "") {
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runLCPTakeOver);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
}
TESTCASE("Bug16007980", "") { INITIALIZER(runBug16007980); }
TESTCASE("LCPScanFragWatchdog", "Test LCP scan watchdog") {
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runTestScanFragWatchdog);
}
TESTCASE("LCPScanFragWatchdogDisable", "Test disabling LCP scan watchdog") {
  STEP(runTestScanFragWatchdogDisable);
}
TESTCASE("LCPScanFragWatchdogIsolation",
         "Test LCP scan watchdog resulting in isolation") {
  TC_PROPERTY("WatchdogKillFail", Uint32(1));
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runTestScanFragWatchdog);
}
TESTCASE("Bug16834416", "") { INITIALIZER(runBug16834416); }
TESTCASE("NR_Disk_data_undo_log_local_lcp",
         "Test node restart when running out of UNDO log to perform"
         " local LCP") {
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runDelayedNodeFail);
}
TESTCASE("NodeFailGCPOpen",
         "Test behaviour of code to keep GCP open for node failure "
         " handling") {
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runNodeFailGCPOpen);
  FINALIZER(runClearTable);
}
TESTCASE("Bug16766493", "") { INITIALIZER(runBug16766493); }
TESTCASE("multiTCtakeover", "") {
  INITIALIZER(run_multiTCtakeover);
  STEP(runLargeTransactions);
  STEP(runManyTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("Bug16895311",
         "Test NR with long UTF8 PK.\n"
         "Give any tablename as argument (T1)") {
  INITIALIZER(runBug16895311_create);
  INITIALIZER(runBug16895311_load);
  STEP(runBug16895311_update);
  STEP(runRestarter);
  FINALIZER(runBug16895311_drop);
}
TESTCASE("Bug18044717",
         "Test LCP state change from LCP_INIT_TABLES "
         "to LCP_STATUS_IDLE during node restart") {
  INITIALIZER(runBug18044717);
}
TESTCASE("DeleteRestart",
         "Check that create big table and delete rows followed by "
         "node restart does not leak memory") {
  INITIALIZER(runDeleteRestart);
}
TESTCASE("GcpStop", "Check various Gcp stop scenarios") {
  INITIALIZER(runCreateEvent);
  STEP(runGcpStop);
  FINALIZER(cleanupGcpStopTest);
  FINALIZER(runDropEvent);
}
TESTCASE("GcpStopIsolation",
         "Check various Gcp stop scenarios where isolation is "
         "required to recover.") {
  TC_PROPERTY("GcpStopIsolation", Uint32(1));
  INITIALIZER(runCreateEvent);
  STEP(runGcpStop);
  FINALIZER(cleanupGcpStopTest);
  FINALIZER(runDropEvent);
}
TESTCASE("LCPLMBLeak", "Check for Long message buffer leaks during LCP");
{
  INITIALIZER(createManyTables);
  INITIALIZER(snapshotLMBUsage);
  STEP(runLCP);
  STEP(waitAndCheckLMBUsage);
  FINALIZER(dropManyTables);
}
TESTCASE("MultiCrashTest",
         "Check that we survive and die after node crashes as expected") {
  INITIALIZER(runLoadTable);
  STEP(runMultiCrashTest);
  FINALIZER(runClearTable);
}
TESTCASE("LCP_with_many_parts", "Ensure that LCP has many parts") {
  TC_PROPERTY("DropTable", (Uint32)0);
  INITIALIZER(run_PLCP_many_parts);
}
TESTCASE("LCP_with_many_parts_drop_table", "Ensure that LCP has many parts") {
  TC_PROPERTY("DropTable", (Uint32)1);
  INITIALIZER(run_PLCP_many_parts);
}
TESTCASE("PLCP_R1", "Node restart while deleting rows") {
  TC_PROPERTY("Initial", (Uint32)0);
  TC_PROPERTY("WaitStart", (Uint32)0);
  INITIALIZER(run_PLCP_I1);
}
TESTCASE("PLCP_RW1", "Node restart while deleting rows") {
  TC_PROPERTY("Initial", (Uint32)0);
  TC_PROPERTY("WaitStart", (Uint32)1);
  INITIALIZER(run_PLCP_I1);
}
TESTCASE("PLCP_IW1", "Node restart while deleting rows") {
  TC_PROPERTY("Initial", (Uint32)1);
  TC_PROPERTY("WaitStart", (Uint32)1);
  INITIALIZER(run_PLCP_I1);
}
TESTCASE("PLCP_I1", "Initial node restart while deleting rows") {
  TC_PROPERTY("Initial", (Uint32)1);
  TC_PROPERTY("WaitStart", (Uint32)0);
  INITIALIZER(run_PLCP_I1);
}
TESTCASE("PLCP_I2", "Initial node restart while deleting rows") {
  INITIALIZER(run_PLCP_I2);
}
TESTCASE("ArbitrationWithApiNodeFailure",
         "Check that arbitration do not fail with non arbitrator api node "
         "failure.");
{ STEP(runArbitrationWithApiNodeFailure); }
TESTCASE("RestoreOlderLCP",
         "Check if older LCP is restored when latest LCP is not recoverable");
{
  TC_PROPERTY("LCP", (Uint32)0);
  INITIALIZER(runLCPandRecordId);
  INITIALIZER(runLoadTable);
  INITIALIZER(runLCPandRecordId);
  STEP(runRestartandCheckLCPRestored);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("StartDuringNodeRestart",
         "Test Start of a node during a Restart when Stop is skipped/ "
         "not completed in time.");
{ STEP(runTestStartNode); }
TESTCASE("MultiSocketRestart",
         "Test failures in setup phase of multi sockets for multi failures");
{ STEP(run_test_multi_socket); }
TESTCASE("NodeFailLcpStall",
         "Check that node failure does not result in LCP stall") {
  TC_PROPERTY("NumTables", Uint32(100));
  INITIALIZER(createManyTables);
  STEP(runNodeFailLcpStall);
  FINALIZER(dropManyTables);
}
TESTCASE("PostponeRecalculateGCPCommitLag",
         "check that a slow TC takeover does not result in "
         "another GCP failure in a shorter period") {
  TC_PROPERTY("NumConfigVars", Uint32(3));
  TC_PROPERTY("ConfigVarId1", Uint32(CFG_DB_MICRO_GCP_TIMEOUT));
  TC_PROPERTY("ConfigValue1", Uint32(1000));
  TC_PROPERTY("ConfigVarId2", Uint32(CFG_DB_HEARTBEAT_INTERVAL));
  TC_PROPERTY("ConfigValue2", Uint32(5000));
  TC_PROPERTY("ConfigVarId3", Uint32(CFG_DB_LCP_SCAN_WATCHDOG_LIMIT));
  // 10000 sec - long enough not to expire before GCP max lags expire
  TC_PROPERTY("ConfigValue3", Uint32(10000));

  INITIALIZER(runChangeDataNodeConfig);
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runPauseGcpCommitUntilNodeFailure);
  FINALIZER(runChangeDataNodeConfig);
}
TESTCASE("SumaHandover3rpl",
         "Test Suma handover with multiple GCIs and more than 2 replicas") {
  INITIALIZER(run_suma_handover_test);
}
TESTCASE("SumaHandoverNF",
         "Test Suma handover with multiple GCIs and more than 2 replicas") {
  INITIALIZER(run_suma_handover_with_node_failure);
}
TESTCASE("InplaceCharPkChangeCS",
         "Check that pk changes which are binary different, but "
         "collation-compare the same, are ok during restarts") {
  TC_PROPERTY("CSCharset", Uint32(1));
  TC_PROPERTY("NumDataColumns", Uint32(10));
  INITIALIZER(runCreateCharKeyTable);
  INITIALIZER(runLoadCharKeyTable);
  INITIALIZER(runErrorInsertSlowCopyFrag);
  STEP(runChangePkCharKeyTable);
  STEP(runRestarter);
  FINALIZER(runClearErrorInsert);
  FINALIZER(runDropCharKeyTable);
}
TESTCASE("InplaceCharPkChangeCI",
         "Check that pk changes which are binary different, but "
         "collation-compare the same, are ok during restarts") {
  TC_PROPERTY("CSCharset", Uint32(0));
  TC_PROPERTY("NumDataColumns", Uint32(10));
  INITIALIZER(runCreateCharKeyTable);
  INITIALIZER(runLoadCharKeyTable);
  INITIALIZER(runErrorInsertSlowCopyFrag);
  STEP(runChangePkCharKeyTable);
  STEP(runRestarter);
  FINALIZER(runClearErrorInsert);
  FINALIZER(runDropCharKeyTable);
}
TESTCASE("ChangeNumLDMsNR", "Change the number of LDMs in a NR") {
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runChangeNumLDMsNR);
  FINALIZER(runClearTable);
}
TESTCASE("ChangeNumLogPartsINR", "Change the number of Log parts in an INR") {
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runChangeNumLogPartsINR);
  FINALIZER(runClearTable);
}
TESTCASE("WatchdogSlowShutdown",
         "Watchdog reacts to slow exec thread shutdown") {
  INITIALIZER(runWatchdogSlowShutdown);
  FINALIZER(runWatchdogSlowShutdownCleanup);
}
TESTCASE(
    "ApiDetectNoFirstHeartbeat",
    "Check that data nodes are notified of API node disconnection "
    "when communication is available one-way (from API node to data node)."
    "Includes the case where the link from data node to API node was broken"
    "before the first API_REGCONF arrived to API node");
{ STEP(runApiDetectNoFirstHeartbeat); }
TESTCASE("CheckGcpStopTimerDistributed",
         "Check that the lack of Gcp cordinator recalculating "
         "and distributing gcp_stop_timer does not result in "
         "an LCP failure in participants") {
  TC_PROPERTY("NumConfigVars", Uint32(2));
  TC_PROPERTY("ConfigVarId1", Uint32(CFG_DB_MICRO_GCP_TIMEOUT));
  // Set to a nonzero value to force GCP cordinator to recalculate
  // gcp_stop_timer.
  TC_PROPERTY("ConfigValue1", Uint32(120000));

  const Uint32 MaxLcpSeconds = 30;

  TC_PROPERTY("ConfigVarId2", Uint32(CFG_DB_LCP_SCAN_WATCHDOG_LIMIT));
  // Reduce default LCP watchdog max limit from 60 sec
  // to reduce the test run time.
  TC_PROPERTY("ConfigValue2", MaxLcpSeconds);

  TC_PROPERTY("MaxLcpLag", MaxLcpSeconds);

  INITIALIZER(runChangeDataNodeConfig);
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runDelayGCP_SAVEREQ);
  FINALIZER(runChangeDataNodeConfig);
}
TESTCASE("TransStallTimeout", "") {
  INITIALIZER(runLoadTable);
  STEP(runTestStallTimeout);
  FINALIZER(runClearTable);
}
TESTCASE("TransStallTimeoutNF", "") {
  INITIALIZER(runLoadTable);
  STEP(runTestStallTimeoutAndNF);
  FINALIZER(runClearTable);
}
TESTCASE("TransientStatesNF",
         "Test node failure handling with transactions in transient states") {
  INITIALIZER(runLoadTable);
  STEPS(runLargeLockingReads, 5);
  STEP(runRestartsWithSlowCommitComplete);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testNodeRestart)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testNodeRestart);
#if 0
  // It might be interesting to have longer defaults for num
  // loops in this test
  // Just performing 100 node restarts would not be enough?
  // We can have initialisers in the NDBT_Testcase class like
  // this...
  testNodeRestart.setDefaultLoops(1000);
#endif
  return testNodeRestart.execute(argc, argv);
}
template class Vector<HugoOperations *>;
