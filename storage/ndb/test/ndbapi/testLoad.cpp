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
#include <ndb_rand.h>
#include <Bitmask.hpp>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbConfig.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <RefConvert.hpp>
#include <UtilTransactions.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>

#define PK_READ_LOCK 0
#define PK_INSERT 1
#define PK_UPDATE 2
#define PK_DELETE 3
#define PK_WRITE 4

#define LARGE_COMMIT 0
#define LARGE_ABORT 1

int runLoadTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }
  g_err << "Latest GCI = " << hugoTrans.get_high_latest_gci() << endl;
  return NDBT_OK;
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

int runLargeTransactions(NDBT_Context *ctx, NDBT_Step *step) {
  int multiop = 200;
  HugoOperations *hugo_op;
  int num_steps = ctx->getProperty("NumSteps", Uint32(1));
  int operation_type = ctx->getProperty("OperationType", Uint32(PK_INSERT));
  int commit_type = ctx->getProperty("CommitType", Uint32(LARGE_COMMIT));
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  int divisor = multiop * num_steps;
  records = ((records + (divisor - 1)) / divisor) * divisor;
  int our_records = records / num_steps;
  int our_step = step->getStepNo();
  assert(our_step > 0);
  int first_record = (our_step - 1) * our_records;
  int num_loops = our_records / multiop;
  int result = NDBT_FAILED;

  hugo_op = new HugoOperations(*ctx->getTab());
  if (hugo_op == NULL) {
    ndbout << "Failed to allocate HugoOperations instance, step = " << our_step
           << endl;
    return NDBT_FAILED;
  }
  if (hugo_op->startTransaction(pNdb) != 0) {
    ndbout << "Failed to start Transaction, step = " << our_step << endl;
    delete hugo_op;
    return NDBT_FAILED;
  }
  for (int i = 0; i < num_loops; i++) {
    for (int j = 0; j < multiop; j++) {
      int record_no = first_record + (i * multiop) + j;
      int res;
      switch (operation_type) {
        case PK_READ_LOCK: {
          res = hugo_op->pkReadRecord(pNdb, record_no);
          break;
        }
        case PK_INSERT: {
          res = hugo_op->pkInsertRecord(pNdb, record_no);
          break;
        }
        case PK_UPDATE: {
          res = hugo_op->pkUpdateRecord(pNdb, record_no, 1, 1);
          break;
        }
        case PK_DELETE: {
          res = hugo_op->pkDeleteRecord(pNdb, record_no);
          break;
        }
        case PK_WRITE: {
          res = hugo_op->pkWriteRecord(pNdb, record_no);
          break;
        }
        default: {
          abort();
        }
      }
      if (res) {
        ndbout << "Failed to insert record number = " << record_no
               << " step = " << our_step << endl;
        goto end;
      }
    }
    if (hugo_op->execute_NoCommit(pNdb) != 0) {
      ndbout << "Failed to execute no commit, step = " << our_step << endl;
      goto end;
    }
  }
  if (commit_type == LARGE_COMMIT) {
    ndbout << "Start large commit" << endl;
    if (hugo_op->execute_Commit(pNdb) != 0) {
      ndbout << "Failed to execute commit, step = " << our_step << endl;
      goto end;
    }
  } else if (commit_type == LARGE_ABORT) {
    ndbout << "Start large abort" << endl;
    if (hugo_op->execute_Rollback(pNdb) != 0) {
      ndbout << "Failed to execute rollback, step: " << our_step << endl;
      goto end;
    }
  } else {
    abort();
  }
  result = NDBT_OK;
end:
  hugo_op->closeTransaction(pNdb);
  return result;
}

NDBT_TESTSUITE(testLoad);
TESTCASE("LargeTransactionInsertCommitP1",
         "Large Transaction in one thread that commits") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_INSERT));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionInsertCommitP10",
         "Large Transaction in ten threads that commits") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_INSERT));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionInsertAbortP1",
         "Large Transaction in one thread that aborts") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_INSERT));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  STEP(runLargeTransactions);
}
TESTCASE("LargeTransactionInsertAbortP10",
         "Large Transaction in ten threads that aborts") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_INSERT));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  STEPS(runLargeTransactions, 10);
}
TESTCASE("LargeTransactionWriteCommitP1",
         "Large Transaction in one thread that commits") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_WRITE));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionWriteCommitP10",
         "Large Transaction in ten threads that commits") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_WRITE));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionWriteAbortP1",
         "Large Transaction in one thread that aborts") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_WRITE));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  STEP(runLargeTransactions);
}
TESTCASE("LargeTransactionWriteAbortP10",
         "Large Transaction in ten threads that aborts") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_WRITE));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  STEPS(runLargeTransactions, 10);
}
TESTCASE("LargeTransactionUpdateCommitP1",
         "Large Transaction in one thread that commits") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_UPDATE));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  INITIALIZER(runLoadTable);
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionUpdateCommitP10",
         "Large Transaction in ten threads that commits") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_UPDATE));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  INITIALIZER(runLoadTable);
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionUpdateAbortP1",
         "Large Transaction in one thread that aborts") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_UPDATE));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  INITIALIZER(runLoadTable);
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionUpdateAbortP10",
         "Large Transaction in ten threads that aborts") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_UPDATE));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  INITIALIZER(runLoadTable);
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionReadCommitP1",
         "Large Transaction in one thread that commits") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_READ_LOCK));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  INITIALIZER(runLoadTable);
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionReadCommitP10",
         "Large Transaction in ten threads that commits") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_READ_LOCK));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  INITIALIZER(runLoadTable);
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionReadAbortP1",
         "Large Transaction in one thread that aborts") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_READ_LOCK));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  INITIALIZER(runLoadTable);
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionReadAbortP10",
         "Large Transaction in ten threads that aborts") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_READ_LOCK));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  INITIALIZER(runLoadTable);
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionDeleteCommitP1",
         "Large Transaction in one thread that commits") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_DELETE));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  INITIALIZER(runLoadTable);
  STEP(runLargeTransactions);
}
TESTCASE("LargeTransactionDeleteCommitP10",
         "Large Transaction in ten threads that commits") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_DELETE));
  TC_PROPERTY("CommitType", Uint32(LARGE_COMMIT));
  INITIALIZER(runLoadTable);
  STEPS(runLargeTransactions, 10);
}
TESTCASE("LargeTransactionDeleteAbortP1",
         "Large Transaction in one thread that aborts") {
  TC_PROPERTY("NumSteps", 1);
  TC_PROPERTY("OperationType", Uint32(PK_DELETE));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  INITIALIZER(runLoadTable);
  STEP(runLargeTransactions);
  FINALIZER(runClearTable);
}
TESTCASE("LargeTransactionDeleteAbortP10",
         "Large Transaction in ten threads that aborts") {
  TC_PROPERTY("NumSteps", 10);
  TC_PROPERTY("OperationType", Uint32(PK_DELETE));
  TC_PROPERTY("CommitType", Uint32(LARGE_ABORT));
  INITIALIZER(runLoadTable);
  STEPS(runLargeTransactions, 10);
  FINALIZER(runClearTable);
}

NDBT_TESTSUITE_END(testLoad)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testLoad);
  return testLoad.execute(argc, argv);
}
