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

#include <NdbRestarter.hpp>
#include "HugoTransactions.hpp"
#include "NDBT_ReturnCodes.h"
#include "NDBT_Test.hpp"
#include "UtilTransactions.hpp"

template class Vector<int>;

struct OperationTestCase {
  const char *name;
  bool preCond;  // start transaction | insert | commit

  // start transaction
  const char *op1;
  const int res1;
  const int val1;

  // no commit

  const char *op2;
  const int res2;
  const int val2;
  // Commit

  // start transaction
  // op3 = READ
  const int res3;
  const int val3;
  // commit transaction
};

OperationTestCase matrix[] = {
    {"ReadRead", true, "READ", 0, 0, "READ", 0, 0, 0, 0},
    {"ReadReadEx", true, "READ", 0, 0, "READ-EX", 0, 0, 0, 0},
    {"ReadSimpleRead", true, "READ", 0, 0, "S-READ", 0, 0, 0, 0},
    {"ReadDirtyRead", true, "READ", 0, 0, "D-READ", 0, 0, 0, 0},
    {"ReadInsert", true, "READ", 0, 0, "INSERT", 630, 1, 0, 0},
    {"ReadUpdate", true, "READ", 0, 0, "UPDATE", 0, 1, 0, 1},
    {"ReadDelete", true, "READ", 0, 0, "DELETE", 0, 0, 626, 0},

    {"FReadRead", false, "READ", 626, 0, "READ", 626, 0, 626, 0},
    {"FReadReadEx", false, "READ", 626, 0, "READ-EX", 626, 0, 626, 0},
    {"FReadSimpleRead", false, "READ", 626, 0, "S-READ", 626, 0, 626, 0},
    {"FReadDirtyRead", false, "READ", 626, 0, "D-READ", 626, 0, 626, 0},
    {"FReadInsert", false, "READ", 626, 0, "INSERT", 0, 1, 0, 1},
    {"FReadUpdate", false, "READ", 626, 0, "UPDATE", 626, 0, 626, 0},
    {"FReadDelete", false, "READ", 626, 0, "DELETE", 626, 0, 626, 0},

    {"FSimpleReadRead", false, "S-READ", 626, 0, "READ", 626, 0, 626, 0},
    {"FSimpleReadReadEx", false, "S-READ", 626, 0, "READ-EX", 626, 0, 626, 0},
    {"FSimpleReadSimpleRead", false, "S-READ", 626, 0, "S-READ", 626, 0, 626,
     0},
    {"FSimpleReadDirtyRead", false, "S-READ", 626, 0, "D-READ", 626, 0, 626, 0},
    {"FSimpleReadInsert", false, "S-READ", 626, 0, "INSERT", 0, 1, 0, 1},
    {"FSimpleReadUpdate", false, "S-READ", 626, 0, "UPDATE", 626, 0, 626, 0},
    {"FSimpleReadDelete", false, "S-READ", 626, 0, "DELETE", 626, 0, 626, 0},

    {"ReadExRead", true, "READ-EX", 0, 0, "READ", 0, 0, 0, 0},
    {"ReadExReadEx", true, "READ-EX", 0, 0, "READ-EX", 0, 0, 0, 0},
    {"ReadExSimpleRead", true, "READ-EX", 0, 0, "S-READ", 0, 0, 0, 0},
    {"ReadExDirtyRead", true, "READ-EX", 0, 0, "D-READ", 0, 0, 0, 0},
    {"ReadExInsert", true, "READ-EX", 0, 0, "INSERT", 630, 1, 0, 0},
    {"ReadExUpdate", true, "READ-EX", 0, 0, "UPDATE", 0, 1, 0, 1},
    {"ReadExDelete", true, "READ-EX", 0, 0, "DELETE", 0, 0, 626, 0},

    {"InsertRead", false, "INSERT", 0, 0, "READ", 0, 0, 0, 0},
    {"InsertReadEx", false, "INSERT", 0, 0, "READ-EX", 0, 0, 0, 0},
    {"InsertSimpleRead", false, "INSERT", 0, 0, "S-READ", 0, 0, 0, 0},
    {"InsertDirtyRead", false, "INSERT", 0, 0, "D-READ", 0, 0, 0, 0},
    {"InsertInsert", false, "INSERT", 0, 0, "INSERT", 630, 0, 626, 0},
    {"InsertUpdate", false, "INSERT", 0, 0, "UPDATE", 0, 1, 0, 1},
    {"InsertDelete", false, "INSERT", 0, 0, "DELETE", 0, 0, 626, 0},

    {"UpdateRead", true, "UPDATE", 0, 1, "READ", 0, 1, 0, 1},
    {"UpdateReadEx", true, "UPDATE", 0, 1, "READ-EX", 0, 1, 0, 1},
    {"UpdateSimpleRead", true, "UPDATE", 0, 1, "S-READ", 0, 1, 0, 1},
    {"UpdateDirtyRead", true, "UPDATE", 0, 1, "D-READ", 0, 1, 0, 1},
    {"UpdateInsert", true, "UPDATE", 0, 1, "INSERT", 630, 0, 0, 0},
    {"UpdateUpdate", true, "UPDATE", 0, 1, "UPDATE", 0, 2, 0, 2},
    {"UpdateDelete", true, "UPDATE", 0, 1, "DELETE", 0, 0, 626, 0},

    {"DeleteRead", true, "DELETE", 0, 0, "READ", 626, 0, 0, 0},
    {"DeleteReadEx", true, "DELETE", 0, 0, "READ-EX", 626, 0, 0, 0},
    {"DeleteSimpleRead", true, "DELETE", 0, 0, "S-READ", 626, 0, 0, 0},
    {"DeleteDirtyRead", true, "DELETE", 0, 0, "D-READ", 626, 0, 626, 0},
    {"DeleteInsert", true, "DELETE", 0, 0, "INSERT", 0, 1, 0, 1},
    {"DeleteUpdate", true, "DELETE", 0, 0, "UPDATE", 626, 1, 0, 0},
    {"DeleteDelete", true, "DELETE", 0, 0, "DELETE", 626, 0, 0, 0}};

#define CHECK(b)                                                          \
  if (!(b)) {                                                             \
    g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    break;                                                                \
  }

#define C3(b)                                            \
  if (!(b)) {                                            \
    g_err << "ERR: failed on line " << __LINE__ << endl; \
    return NDBT_FAILED;                                  \
  }

int runOp(HugoOperations &hugoOps, Ndb *pNdb, const char *op, int value) {
#define C2(x, y)                                                              \
  {                                                                           \
    int r = (x);                                                              \
    int s = (y);                                                              \
    if (r != s) {                                                             \
      g_err << "ERR: failed on line " << __LINE__ << ": " << r << " != " << s \
            << endl;                                                          \
      return NDBT_FAILED;                                                     \
    }                                                                         \
  }

  if (strcmp(op, "READ") == 0) {
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read), 0);
  } else if (strcmp(op, "READ-EX") == 0) {
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive), 0);
  } else if (strcmp(op, "S-READ") == 0) {
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_SimpleRead), 0);
  } else if (strcmp(op, "D-READ") == 0) {
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_CommittedRead), 0);
  } else if (strcmp(op, "INSERT") == 0) {
    C2(hugoOps.pkInsertRecord(pNdb, 1, 1, value), 0);
  } else if (strcmp(op, "UPDATE") == 0) {
    C2(hugoOps.pkUpdateRecord(pNdb, 1, 1, value), 0);
  } else if (strcmp(op, "DELETE") == 0) {
    C2(hugoOps.pkDeleteRecord(pNdb, 1, 1), 0);
  } else {
    g_err << __FILE__ << " - " << __LINE__ << ": Unknown operation" << op
          << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int checkVal(HugoOperations &hugoOps, const char *op, int value, int result) {
  if (result != 0) return NDBT_OK;

  if (strcmp(op, "READ") == 0) {
  } else if (strcmp(op, "READ-EX") == 0) {
  } else if (strcmp(op, "S-READ") == 0) {
  } else if (strcmp(op, "D-READ") == 0) {
  } else {
    return NDBT_OK;
  }

  return hugoOps.verifyUpdatesValue(value);
}

int runTwoOperations(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  const char *op1 = ctx->getProperty("op1", "NONE");
  const int val1 = ctx->getProperty("val1", ~0);
  const int res1 = ctx->getProperty("res1", ~0);
  const char *op2 = ctx->getProperty("op2", "NONE");
  const int res2 = ctx->getProperty("res2", ~0);
  const int val2 = ctx->getProperty("val2", ~0);

  const int res3 = ctx->getProperty("res3", ~0);
  const int val3 = ctx->getProperty("val3", ~0);

  do {
    // Insert, read
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(runOp(hugoOps, pNdb, op1, val1) == 0);
    AbortOption oa = (res1 == 0) ? AbortOnError : AO_IgnoreError;
    CHECK(hugoOps.execute_NoCommit(pNdb, oa) == res1);
    CHECK(checkVal(hugoOps, op1, val1, res1) == 0);

    ndbout_c("-- running op 2");

    CHECK(runOp(hugoOps, pNdb, op2, val2) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == res2);
    CHECK(checkVal(hugoOps, op2, val2, res2) == 0);

  } while (false);
  hugoOps.closeTransaction(pNdb);

  if (result != NDBT_OK) return result;

  do {
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(runOp(hugoOps, pNdb, "READ", 0) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == res3);
    CHECK(checkVal(hugoOps, "READ", val3, res3) == 0);
  } while (false);
  hugoOps.closeTransaction(pNdb);

  return result;
}

int runInsertRecord(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  do {
    // Insert, insert
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);

  } while (false);

  hugoOps.closeTransaction(pNdb);

  return result;
}

int runClearTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();

  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step), records, 240) != 0) {
    return NDBT_FAILED;
  }

  NdbRestarter r;
  int lcp = 7099;
  r.dumpStateAllNodes(&lcp, 1);

  return NDBT_OK;
}

enum OPS { o_DONE = 0, o_INS = 1, o_UPD = 2, o_DEL = 3 };
typedef Vector<OPS> Sequence;

static bool valid(const Sequence &s) {
  if (s.size() == 0) return false;

  for (unsigned i = 1; i < s.size(); i++) {
    switch (s[i]) {
      case o_INS:
        if (s[i - 1] != o_DEL) return false;
        break;
      case o_UPD:
      case o_DEL:
        if (s[i - 1] == o_DEL) return false;
        break;
      case o_DONE:
        return true;
    }
  }
  return true;
}

#if 0
static
NdbOut& operator<<(NdbOut& out, const Sequence& s)
{
  out << "[ ";
  for(unsigned = 0; i<s.size(); i++)
  {
   switch(s[i]){
    case o_INS:
      out << "INS ";
      break;
    case o_DEL:
      out << "DEL ";
      break;
    case o_UPD:
      out << "UPD ";
      break;
   case o_DONE:
     abort();
   }
  }
  out << "]";
  return out;
}
#endif

static void generate(Sequence &out, int no) {
  while (no & 3) {
    out.push_back((OPS)(no & 3));
    no >>= 2;
  }
}

static void generate(Vector<int> &out, size_t len) {
  int max = 1;
  while (len) {
    max <<= 2;
    len--;
  }

  len = 1;
  for (int i = 0; i < max; i++) {
    Sequence tmp;
    generate(tmp, i);

    if (tmp.size() >= len && valid(tmp)) {
      out.push_back(i);
      len = tmp.size();
    } else {
      // ndbout << "DISCARD: " << tmp << endl;
    }
  }
}

static const Uint32 DUMMY = 0;
static const Uint32 ROW = 1;

int verify_other(NDBT_Context *ctx, Ndb *pNdb, int seq, OPS latest,
                 bool initial_row, bool commit) {
  Uint32 no_wait =
      NdbOperation::LM_CommittedRead * ctx->getProperty("NoWait", (Uint32)1);

  for (size_t j = no_wait; j < 3; j++) {
    HugoOperations other(*ctx->getTab());
    C3(other.startTransaction(pNdb) == 0);
    C3(other.pkReadRecord(pNdb, ROW, 1, (NdbOperation::LockMode)j) == 0);
    int tmp = other.execute_Commit(pNdb);
    if (seq == 0) {
      if (j == NdbOperation::LM_CommittedRead) {
        C3(initial_row ? tmp == 0 && other.verifyUpdatesValue(0) == 0
                       : tmp == 626);
      } else {
        C3(tmp == 266);
      }
    } else if (commit) {
      switch (latest) {
        case o_INS:
        case o_UPD:
          C3(tmp == 0 && other.verifyUpdatesValue(seq) == 0);
          break;
        case o_DEL:
          C3(tmp == 626);
          break;
        case o_DONE:
          abort();
      }
    } else {
      // rollback
      C3(initial_row ? tmp == 0 && other.verifyUpdatesValue(0) == 0
                     : tmp == 626);
    }
  }

  return NDBT_OK;
}

int verify_savepoint(NDBT_Context *ctx, Ndb *pNdb, int seq, OPS latest,
                     Uint64 transactionId) {
  bool initial_row = (seq == 0) && latest == o_INS;

  for (size_t j = 0; j < 3; j++) {
    const NdbOperation::LockMode lm = (NdbOperation::LockMode)j;

    HugoOperations same(*ctx->getTab());
    C3(same.startTransaction(pNdb) == 0);
    same.setTransactionId(transactionId);  // Cheat

    /**
     * Increase savepoint to <em>k</em>
     */
    for (size_t l = 1; l <= (size_t)seq; l++) {
      C3(same.pkReadRecord(pNdb, DUMMY, 1, lm) == 0);  // Read dummy row
      C3(same.execute_NoCommit(pNdb) == 0);
      g_info << "savepoint: " << l << endl;
    }

    g_info << "op(" << seq << "): "
           << " lock mode " << lm << endl;

    C3(same.pkReadRecord(pNdb, ROW, 1, lm) == 0);  // Read real row
    int tmp = same.execute_Commit(pNdb);
    if (seq == 0) {
      if (initial_row) {
        C3(tmp == 0 && same.verifyUpdatesValue(0) == 0);
      } else {
        C3(tmp == 626);
      }
    } else {
      switch (latest) {
        case o_INS:
        case o_UPD:
          C3(tmp == 0 && same.verifyUpdatesValue(seq) == 0);
          break;
        case o_DEL:
          C3(tmp == 626);
          break;
        case o_DONE:
          abort();
      }
    }
  }
  return NDBT_OK;
}

int runOperations(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  Uint32 seqNo = ctx->getProperty("Sequence", (Uint32)0);
  Uint32 commit = ctx->getProperty("Commit", (Uint32)1);

  if (seqNo == 0) {
    return NDBT_FAILED;
  }

  Sequence seq;
  generate(seq, seqNo);

  {
    // Dummy row
    HugoOperations hugoOps(*ctx->getTab());
    C3(hugoOps.startTransaction(pNdb) == 0);
    C3(hugoOps.pkInsertRecord(pNdb, DUMMY, 1, 0) == 0);
    C3(hugoOps.execute_Commit(pNdb) == 0);
  }

  const bool initial_row = (seq[0] != o_INS);
  if (initial_row) {
    HugoOperations hugoOps(*ctx->getTab());
    C3(hugoOps.startTransaction(pNdb) == 0);
    C3(hugoOps.pkInsertRecord(pNdb, ROW, 1, 0) == 0);
    C3(hugoOps.execute_Commit(pNdb) == 0);
  }

  HugoOperations trans1(*ctx->getTab());
  C3(trans1.startTransaction(pNdb) == 0);
  for (unsigned i = 0; i < seq.size(); i++) {
    /**
     * Perform operation
     */
    switch (seq[i]) {
      case o_INS:
        C3(trans1.pkInsertRecord(pNdb, ROW, 1, i + 1) == 0);
        break;
      case o_UPD:
        C3(trans1.pkUpdateRecord(pNdb, ROW, 1, i + 1) == 0);
        break;
      case o_DEL:
        C3(trans1.pkDeleteRecord(pNdb, ROW, 1) == 0);
        break;
      case o_DONE:
        abort();
    }
    C3(trans1.execute_NoCommit(pNdb) == 0);

    /**
     * Verify other transaction
     */
    if (verify_other(ctx, pNdb, 0, seq[0], initial_row, commit) != NDBT_OK)
      return NDBT_FAILED;

    /**
     * Verify savepoint read
     */
    Uint64 transactionId = trans1.getTransaction()->getTransactionId();

    for (unsigned k = 0; k <= i + 1; k++) {
      if (verify_savepoint(ctx, pNdb, k,
                           k > 0 ? seq[k - 1] : initial_row ? o_INS : o_DONE,
                           transactionId) != NDBT_OK)
        return NDBT_FAILED;
    }
  }

  if (commit) {
    C3(trans1.execute_Commit(pNdb) == 0);
  } else {
    C3(trans1.execute_Rollback(pNdb) == 0);
  }

  if (verify_other(ctx, pNdb, seq.size(), seq.back(), initial_row, commit) !=
      NDBT_OK)
    return NDBT_FAILED;

  return NDBT_OK;
}

int runLockUpgrade1(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  if (hugoTrans.loadTable(pNdb, 1) != 0) {
    g_err << "Load table failed" << endl;
    return NDBT_FAILED;
  }

  int result = NDBT_OK;
  do {
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    if (ctx->getProperty("LOCK_UPGRADE", 1) == 1) {
      CHECK(hugoOps.pkReadRecord(pNdb, 0, 1, NdbOperation::LM_Read) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);

      ctx->setProperty("READ_DONE", 1);
      ctx->broadcast();
      ndbout_c("wait 2");
      ctx->getPropertyWait("READ_DONE", 2);
      ndbout_c("wait 2 - done");
    } else {
      ctx->setProperty("READ_DONE", 1);
      ctx->broadcast();
      ctx->getPropertyWait("READ_DONE", 2);
      ndbout_c("wait 2 - done");
      CHECK(hugoOps.pkReadRecord(pNdb, 0, 1, NdbOperation::LM_Read) == 0);
      CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    }
    if (ctx->getProperty("LU_OP", o_INS) == o_INS) {
      CHECK(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
      CHECK(hugoOps.pkInsertRecord(pNdb, 0, 1, 2) == 0);
    } else if (ctx->getProperty("LU_OP", o_UPD) == o_UPD) {
      CHECK(hugoOps.pkUpdateRecord(pNdb, 0, 1, 2) == 0);
    } else {
      CHECK(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
    }
    ctx->setProperty("READ_DONE", 3);
    ctx->broadcast();
    ndbout_c("before update");
    ndbout_c("wait update");
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    CHECK(hugoOps.closeTransaction(pNdb) == 0);

    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, 0, 1) == 0);
    int res = hugoOps.execute_Commit(pNdb);
    if (ctx->getProperty("LU_OP", o_INS) == o_INS) {
      CHECK(res == 0);
      CHECK(hugoOps.verifyUpdatesValue(2) == 0);
    } else if (ctx->getProperty("LU_OP", o_UPD) == o_UPD) {
      CHECK(res == 0);
      CHECK(hugoOps.verifyUpdatesValue(2) == 0);
    } else {
      CHECK(res == 626);
    }

  } while (0);

  return result;
}

int runLockUpgrade2(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  int result = NDBT_OK;
  do {
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    ndbout_c("wait 1");
    ctx->getPropertyWait("READ_DONE", 1);
    ndbout_c("wait 1 - done");
    CHECK(hugoOps.pkReadRecord(pNdb, 0, 1, NdbOperation::LM_Read) == 0);
    CHECK(hugoOps.execute_NoCommit(pNdb) == 0);
    ctx->setProperty("READ_DONE", 2);
    ctx->broadcast();
    ndbout_c("wait 3");
    ctx->getPropertyWait("READ_DONE", 3);
    ndbout_c("wait 3 - done");

    NdbSleep_MilliSleep(200);
    if (ctx->getProperty("LU_COMMIT", (Uint32)0) == 0) {
      CHECK(hugoOps.execute_Commit(pNdb) == 0);
    } else {
      CHECK(hugoOps.execute_Rollback(pNdb) == 0);
    }
  } while (0);

  return result;
}

int main(int argc, const char **argv) {
  ndb_init();

  Vector<int> tmp;
  generate(tmp, 5);

  NDBT_TestSuite ts("testOperations");

  ts.setTemporaryTables(true);

  for (Uint32 i = 0; i < 12; i++) {
    if (false && (i == 6 || i == 8 || i == 10)) continue;

    BaseString name("bug_9749");
    name.appfmt("_%d", i);
    NDBT_TestCaseImpl1 *pt = new NDBT_TestCaseImpl1(&ts, name.c_str(), "");

    pt->setProperty("LOCK_UPGRADE", 1 + (i & 1));
    pt->setProperty("LU_OP", 1 + ((i >> 1) % 3));
    pt->setProperty("LU_COMMIT", i / 6);

    pt->addInitializer(
        new NDBT_Initializer(pt, "runClearTable", runClearTable));

    pt->addStep(new NDBT_ParallelStep(pt, "thread1", runLockUpgrade1));

    pt->addStep(new NDBT_ParallelStep(pt, "thread2", runLockUpgrade2));

    pt->addFinalizer(new NDBT_Finalizer(pt, "runClearTable", runClearTable));
    ts.addTest(pt);
  }

  for (unsigned i = 0; i < tmp.size(); i++) {
    BaseString name;
    Sequence s;
    generate(s, tmp[i]);
    for (unsigned j = 0; j < s.size(); j++) {
      switch (s[j]) {
        case o_INS:
          name.append("_INS");
          break;
        case o_DEL:
          name.append("_DEL");
          break;
        case o_UPD:
          name.append("_UPD");
          break;
        case o_DONE:
          abort();
      }
    }

    BaseString n1;
    n1.append(name);
    n1.append("_COMMIT");

    NDBT_TestCaseImpl1 *pt = new NDBT_TestCaseImpl1(&ts, n1.c_str() + 1, "");

    pt->setProperty("Sequence", tmp[i]);
    pt->addInitializer(
        new NDBT_Initializer(pt, "runClearTable", runClearTable));

    pt->addStep(new NDBT_ParallelStep(pt, "run", runOperations));

    pt->addFinalizer(new NDBT_Finalizer(pt, "runClearTable", runClearTable));

    ts.addTest(pt);

    name.append("_ABORT");
    pt = new NDBT_TestCaseImpl1(&ts, name.c_str() + 1, "");
    pt->setProperty("Sequence", tmp[i]);
    pt->setProperty("Commit", (Uint32)0);
    pt->addInitializer(
        new NDBT_Initializer(pt, "runClearTable", runClearTable));

    pt->addStep(new NDBT_ParallelStep(pt, "run", runOperations));

    pt->addFinalizer(new NDBT_Finalizer(pt, "runClearTable", runClearTable));

    ts.addTest(pt);
  }

  for (Uint32 i = 0; i < sizeof(matrix) / sizeof(matrix[0]); i++) {
    NDBT_TestCaseImpl1 *pt = new NDBT_TestCaseImpl1(&ts, matrix[i].name, "");

    pt->addInitializer(
        new NDBT_Initializer(pt, "runClearTable", runClearTable));

    if (matrix[i].preCond) {
      pt->addInitializer(
          new NDBT_Initializer(pt, "runInsertRecord", runInsertRecord));
    }

    pt->setProperty("op1", matrix[i].op1);
    pt->setProperty("res1", matrix[i].res1);
    pt->setProperty("val1", matrix[i].val1);

    pt->setProperty("op2", matrix[i].op2);
    pt->setProperty("res2", matrix[i].res2);
    pt->setProperty("val2", matrix[i].val2);

    pt->setProperty("res3", matrix[i].res3);
    pt->setProperty("val3", matrix[i].val3);

    pt->addStep(new NDBT_ParallelStep(pt, matrix[i].name, runTwoOperations));
    pt->addFinalizer(new NDBT_Finalizer(pt, "runClearTable", runClearTable));

    ts.addTest(pt);
  }

  return ts.execute(argc, argv);
}

template class Vector<OPS>;
template class Vector<Sequence>;
