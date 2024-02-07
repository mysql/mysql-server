/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#include <NdbEnv.h>
#include <NdbMutex.h>
#include <ndb_version.h>
#include <random.h>
#include <AtrtClient.hpp>
#include <Bitmask.hpp>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbBackup.hpp>
#include <NdbRestarter.hpp>
#include <UtilTransactions.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <string>
#include "../../src/ndbapi/NdbInfo.hpp"
#include "util/require.h"

static Vector<BaseString> table_list;

static Uint32 preVersion = 0;
static Uint32 postVersion = 0;

struct NodeInfo {
  int nodeId;
  int processId;
  int nodeGroup;
};

static const char *getBaseDir() {
  /* Atrt basedir exposed via env var MYSQL_HOME */
  const char *envHome = NdbEnv_GetEnv("MYSQL_HOME", NULL, 0);

  if (envHome == NULL) {
    ndbout_c("testUpgrade getBaseDir() MYSQL_HOME not set");
    return "./";
  } else {
    return envHome;
  }
}

int CMT_createTableHook(Ndb *ndb, NdbDictionary::Table &table, int when,
                        void *arg) {
  if (when == 0) {
    Uint32 num = ((Uint32 *)arg)[0];
    Uint32 fragCount = ((Uint32 *)arg)[1];

    /* Substitute a unique name */
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "%s_%u", table.getName(), num);
    table.setName(buf);
    if (fragCount > 0) {
      table.setFragmentCount(fragCount);
      table.setPartitionBalance(
          NdbDictionary::Object::PartitionBalance_Specific);
    }

    ndbout << "Creating " << buf << " with fragment count " << fragCount
           << endl;
  }
  return 0;
}

Uint32 determineMaxFragCount(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();

  /* Find max # frags we can create... */
  ndbout << "Determining max fragment count on this cluster" << endl;
  Uint32 fc = (ctx->getTab()->getFragmentCount() * 2);
  ndbout << "Start point " << fc << endl;
  bool up = true;
  do {
    ndbout << "Trying " << fc << " ...";

    NdbDictionary::HashMap hm;
    bool ok = (dict->getDefaultHashMap(hm, fc) == 0);

    ndbout << "a" << endl;

    if (!ok) {
      if (dict->initDefaultHashMap(hm, fc) == 0) {
        ndbout << "b" << endl;
        ok = (dict->createHashMap(hm) == 0);
      }
      ndbout << "c" << endl;
    }

    if (ok) {
      Uint32 args[2];
      args[0] = 0;
      args[1] = fc;

      if (NDBT_Tables::createTable(pNdb, ctx->getTab()->getName(), false, false,
                                   CMT_createTableHook, &args) != 0) {
        ok = false;
      } else {
        /* Worked, drop it... */
        char buf[100];
        BaseString::snprintf(buf, sizeof(buf), "%s_%u",
                             ctx->getTab()->getName(), 0);
        ndbout << "Dropping " << buf << endl;
        pNdb->getDictionary()->dropTable(buf);
      }
    }

    if (ok) {
      ndbout << "ok" << endl;
      if (up) {
        fc *= 2;
      } else {
        break;
      }
    } else {
      ndbout << "failed" << endl;

      if (up) {
        up = false;
      }

      fc--;
    }
  } while (true);

  ndbout << "Max frag count : " << fc << endl;

  return fc;
}

static const Uint32 defaultManyTableCount = 35;

int createManyTables(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  Uint32 tableCount = ctx->getProperty("ManyTableCount", defaultManyTableCount);
  Uint32 fragmentCount = ctx->getProperty("FragmentCount", Uint32(0));

  /* fragmentCount
   * 0 = default
   * 1..n = as requested
   * ~Uint32(0) = max possible
   */
  if (fragmentCount == ~Uint32(0)) {
    fragmentCount = determineMaxFragCount(ctx, step);
  }

  for (Uint32 tn = 1; tn < tableCount; tn++) {
    Uint32 args[2];
    args[0] = tn;
    args[1] = fragmentCount;

    if (NDBT_Tables::createTable(pNdb, ctx->getTab()->getName(), false, false,
                                 CMT_createTableHook, &args) != 0) {
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int dropManyTables(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  Uint32 tableCount = ctx->getProperty("ManyTableCount", defaultManyTableCount);
  char buf[100];

  for (Uint32 tn = 0; tn < tableCount; tn++) {
    BaseString::snprintf(buf, sizeof(buf), "%s_%u", ctx->getTab()->getName(),
                         tn);
    ndbout << "Dropping " << buf << endl;
    pNdb->getDictionary()->dropTable(buf);
  }

  return NDBT_OK;
}

static int createEvent(Ndb *pNdb, const NdbDictionary::Table &tab,
                       bool merge_events = true, bool report = true) {
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

static NdbMutex *createDropEvent_mutex = 0;

static int createDropEvent(NDBT_Context *ctx, NDBT_Step *step,
                           bool wait = true) {
  if (!wait) {
    if (NdbMutex_Trylock(createDropEvent_mutex) != 0) {
      g_err << "Skipping createDropEvent since already running in other process"
            << endl;
      return NDBT_OK;
    }
  } else if (NdbMutex_Lock(createDropEvent_mutex) != 0) {
    g_err << "Error while locking createDropEvent_mutex" << endl;
    return NDBT_FAILED;
  }

  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  int res = NDBT_OK;
  if (ctx->getProperty("NoDDL", Uint32(0)) == 0) {
    for (unsigned i = 0; i < table_list.size(); i++) {
      const NdbDictionary::Table *tab = myDict->getTable(table_list[i].c_str());
      if (tab == 0) {
        continue;
      }

      if ((res = createEvent(pNdb, *tab) != NDBT_OK)) {
        goto done;
      }

      if ((res = dropEvent(pNdb, *tab)) != NDBT_OK) {
        goto done;
      }
    }
  }

done:
  if (NdbMutex_Unlock(createDropEvent_mutex) != 0) {
    g_err << "Error while unlocking createDropEvent_mutex" << endl;
    return NDBT_FAILED;
  }

  return res;
}

/* An enum for expressing how many of the multiple nodes
 * of a given type an action should be applied to
 */
enum NodeSet {
  All = 0,
  NotAll = 1, /* less than All, or None if there's only 1 */
  None = 2
};

uint getNodeCount(NodeSet set, uint numNodes) {
  switch (set) {
    case All:
      return numNodes;
    case NotAll: {
      if (numNodes < 2) return 0;

      if (numNodes == 2) return 1;

      uint range = numNodes - 2;

      /* At least 1, at most numNodes - 1 */
      return (1 + (rand() % (range + 1)));
    }
    case None: {
      return 0;
    }
    default:
      g_err << "Unknown set type : " << set << endl;
      abort();
      return 0;
  }
}

static bool check_arbitration_setup(Ndb_cluster_connection *connection) {
  NdbInfo ndbinfo(connection, "ndbinfo/");
  if (!ndbinfo.init()) {
    g_err << "ndbinfo.init failed" << endl;
    return false;
  }

  const NdbInfo::Table *table;
  if (ndbinfo.openTable("ndbinfo/membership", &table) != 0) {
    g_err << "Failed to openTable(membership)" << endl;
    return false;
  }

  NdbInfoScanOperation *scanOp = nullptr;
  if (ndbinfo.createScanOperation(table, &scanOp)) {
    g_err << "No NdbInfoScanOperation" << endl;
    ndbinfo.closeTable(table);
    return false;
  }

  if (scanOp->readTuples() != 0) {
    g_err << "scanOp->readTuples failed" << endl;
    ndbinfo.releaseScanOperation(scanOp);
    ndbinfo.closeTable(table);
    return false;
  }

  const NdbInfoRecAttr *arbitrator_nodeid_colval =
      scanOp->getValue("arbitrator");
  const NdbInfoRecAttr *arbitration_connection_status_colval =
      scanOp->getValue("arb_connected");

  if (scanOp->execute() != 0) {
    g_err << "scanOp->execute failed" << endl;
    ndbinfo.releaseScanOperation(scanOp);
    ndbinfo.closeTable(table);
    return false;
  }

  // Iterate through the result to check if arbitration has been set up
  bool arbitration_setup = true;
  do {
    const int scan_next_result = scanOp->nextResult();
    if (scan_next_result == -1) {
      g_err << "Failure to process ndbinfo records" << endl;
      ndbinfo.releaseScanOperation(scanOp);
      ndbinfo.closeTable(table);
      return false;
    } else if (scan_next_result == 0) {
      // All ndbinfo records processed
      break;
    } else {
      // Check the arbitration status
      const bool known_arbitrator =
          (arbitrator_nodeid_colval->u_32_value() != 0);
      const bool connected =
          static_cast<bool>(arbitration_connection_status_colval->u_32_value());
      arbitration_setup = known_arbitrator && connected;
    }
  } while (arbitration_setup);

  if (!arbitration_setup) {
    ndbout << "Waiting for arbitration to be set up" << endl;
  }

  ndbinfo.releaseScanOperation(scanOp);
  ndbinfo.closeTable(table);
  return arbitration_setup;
}

/**
 * Perform up/downgrade of MGMDs
 */
int runChangeMgmds(NDBT_Context *ctx, NDBT_Step *step, NdbRestarter &restarter,
                   AtrtClient &atrt, const uint clusterId,
                   const NodeSet mgmdNodeSet) {
  SqlResultSet mgmds;
  if (!atrt.getMgmds(clusterId, mgmds)) return NDBT_FAILED;

  const uint mgmdCount = mgmds.numRows();
  uint restartCount = getNodeCount(mgmdNodeSet, mgmdCount);

  if (ctx->getProperty("InitialMgmdRestart", Uint32(0)) == 1) {
    /**
     * MGMD initial restart requires that all MGMDs are stopped,
     * up/downgraded and started together
     */
    const char *mgmd_args = "--initial=1";

    if (mgmdNodeSet != All && mgmdNodeSet != None) {
      ndbout << "Error cannot restart MGMDs --initial without restarting all."
             << endl;
      return NDBT_FAILED;
    }

    ndbout << "Restarting " << restartCount << " of " << mgmdCount
           << " mgmds with --initial" << endl;

    uint startCount = restartCount;
    while (mgmds.next() && restartCount--) {
      ndbout << "Stop mgmd " << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.stopProcess(mgmds.columnAsInt("id")) ||
          !atrt.switchConfig(mgmds.columnAsInt("id"), mgmd_args))
        return NDBT_FAILED;
    }
    mgmds.reset();
    while (mgmds.next() && startCount--) {
      ndbout << "Start mgmd " << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.startProcess(mgmds.columnAsInt("id"))) return NDBT_FAILED;
    }
  } else {
    /**
     * MGMD rolling restart without initial, do one MGMD at a time
     */
    const char *mgmd_args = "--initial=0";

    ndbout << "Restarting " << restartCount << " of " << mgmdCount << " mgmds"
           << endl;

    while (mgmds.next() && restartCount--) {
      ndbout << "Restart mgmd " << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.changeVersion(mgmds.columnAsInt("id"), mgmd_args))
        return NDBT_FAILED;

      if (restarter.waitConnected()) return NDBT_FAILED;
    }
  }

  bool arbitration_complete = false;
  const int attempts = 10;
  const int wait_time_ms = 500;
  for (int a = 0; !arbitration_complete && a < attempts; a++) {
    arbitration_complete = check_arbitration_setup(&ctx->m_cluster_connection);
    if (!arbitration_complete) {
      NdbSleep_MilliSleep(wait_time_ms);
    }
  }

  if (!arbitration_complete) {
    ndberr << "Failed to complete arbitration after "
           << (attempts * wait_time_ms) / 1000 << " seconds" << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

/**
  Test that one node at a time can be upgraded
*/

int runUpgrade_NR1(NDBT_Context *ctx, NDBT_Step *step) {
  AtrtClient atrt;

  NodeSet mgmdNodeSet = (NodeSet)ctx->getProperty("MgmdNodeSet", Uint32(0));
  NodeSet ndbdNodeSet = (NodeSet)ctx->getProperty("NdbdNodeSet", Uint32(0));

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters)) return NDBT_FAILED;

  while (clusters.next()) {
    uint clusterId = clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result)) return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true);  // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name") << "@"
          << tmp_result.column("connectstring") << "'" << endl;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;

    // Restart ndb_mgmd(s)
    if (runChangeMgmds(ctx, step, restarter, atrt, clusterId, mgmdNodeSet) !=
        NDBT_OK) {
      return NDBT_FAILED;
    }

    // Restart ndbd(s)
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds)) return NDBT_FAILED;

    uint ndbdCount = ndbds.numRows();
    uint restartCount = getNodeCount(ndbdNodeSet, ndbdCount);

    ndbout << "Restarting " << restartCount << " of " << ndbdCount << " ndbds"
           << endl;

    while (ndbds.next() && restartCount--) {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");
      ndbout << "Restart node " << nodeId << endl;

      if (!atrt.changeVersion(processId, "")) return NDBT_FAILED;

      if (restarter.waitNodesNoStart(&nodeId, 1)) return NDBT_FAILED;

      if (restarter.startNodes(&nodeId, 1)) return NDBT_FAILED;

      if (restarter.waitNodesStarted(&nodeId, 1)) return NDBT_FAILED;

      // Return value is ignored until WL#4101 is implemented.
      createDropEvent(ctx, step);
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

static int runBug48416(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  return NDBT_Tables::createTable(pNdb, "I1");
}

static int runUpgrade_Half(NDBT_Context *ctx, NDBT_Step *step) {
  // Assuming 2 replicas

  AtrtClient atrt;

  const bool waitNode = ctx->getProperty("WaitNode", Uint32(0)) != 0;
  const bool event = ctx->getProperty("CreateDropEvent", Uint32(0)) != 0;
  const char *ndbd_args = "";

  if (ctx->getProperty("KeepFS", Uint32(0)) != 0) {
    ndbd_args = "--initial=0";
  }

  NodeSet mgmdNodeSet = (NodeSet)ctx->getProperty("MgmdNodeSet", Uint32(0));
  NodeSet ndbdNodeSet = (NodeSet)ctx->getProperty("NdbdNodeSet", Uint32(0));

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters)) return NDBT_FAILED;

  while (clusters.next()) {
    uint clusterId = clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result)) return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true);  // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name") << "@"
          << tmp_result.column("connectstring") << "'" << endl;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;

    // Restart ndb_mgmd(s)
    if (runChangeMgmds(ctx, step, restarter, atrt, clusterId, mgmdNodeSet) !=
        NDBT_OK) {
      return NDBT_FAILED;
    }

    // Restart one ndbd in each node group
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds)) return NDBT_FAILED;

    Vector<NodeInfo> nodes;
    while (ndbds.next()) {
      struct NodeInfo n;
      n.nodeId = ndbds.columnAsInt("node_id");
      n.processId = ndbds.columnAsInt("id");
      n.nodeGroup = restarter.getNodeGroup(n.nodeId);
      nodes.push_back(n);
    }

    uint ndbdCount = ndbds.numRows();
    uint restartCount = getNodeCount(ndbdNodeSet, ndbdCount);

    ndbout << "Restarting " << restartCount << " of " << ndbdCount << " ndbds"
           << endl;

    int nodesarray[256];
    int cnt = 0;

    Bitmask<4> seen_groups;
    Bitmask<4> restarted_nodes;
    for (Uint32 i = 0; (i < nodes.size() && restartCount); i++) {
      int nodeId = nodes[i].nodeId;
      int processId = nodes[i].processId;
      int nodeGroup = nodes[i].nodeGroup;

      if (nodeGroup != 0) {
        ndberr << "Expected nodeGroup 0, but got " << nodeGroup << endl;
      }
      if (seen_groups.get(nodeGroup)) {
        // One node in this node group already down
        continue;
      }
      seen_groups.set(nodeGroup);
      restarted_nodes.set(nodeId);

      ndbout << "Restart node " << nodeId << endl;

      if (!atrt.changeVersion(processId, ndbd_args)) return NDBT_FAILED;

      if (waitNode) {
        restarter.waitNodesNoStart(&nodeId, 1);
      }

      nodesarray[cnt++] = nodeId;
      restartCount--;
    }

    if (!waitNode) {
      if (restarter.waitNodesNoStart(nodesarray, cnt)) return NDBT_FAILED;
    }

    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll()) return NDBT_FAILED;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;

    CHK_NDB_READY(GETNDB(step));

    if (event) {
      // Return value is ignored until WL#4101 is implemented.
      createDropEvent(ctx, step);
    }

    ndbout << "Half started" << endl;

    if (ctx->getProperty("HalfStartedHold", (Uint32)0) != 0) {
      while (ctx->getProperty("HalfStartedHold", (Uint32)0) != 0) {
        ndbout << "Half started holding..." << endl;
        ctx->setProperty("HalfStartedDone", (Uint32)1);
        NdbSleep_SecSleep(30);
      }
      ndbout << "Got half started continue..." << endl;
    }

    // Restart the remaining nodes
    cnt = 0;
    for (Uint32 i = 0; (i < nodes.size() && restartCount); i++) {
      int nodeId = nodes[i].nodeId;
      int processId = nodes[i].processId;

      if (restarted_nodes.get(nodeId)) continue;

      ndbout << "Restart node " << nodeId << endl;
      if (!atrt.changeVersion(processId, ndbd_args)) return NDBT_FAILED;

      if (waitNode) {
        restarter.waitNodesNoStart(&nodeId, 1);
      }

      nodesarray[cnt++] = nodeId;
      restartCount--;
    }

    if (!waitNode) {
      if (restarter.waitNodesNoStart(nodesarray, cnt)) return NDBT_FAILED;
    }

    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll()) return NDBT_FAILED;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;

    CHK_NDB_READY(GETNDB(step));

    if (event) {
      // Return value is ignored until WL#4101 is implemented.
      createDropEvent(ctx, step);
    }
  }

  return NDBT_OK;
}

/**
   Test that one node in each nodegroup can be upgraded simultaneously
    - using method1
*/

int runUpgrade_NR2(NDBT_Context *ctx, NDBT_Step *step) {
  // Assuming 2 replicas

  ctx->setProperty("WaitNode", 1);
  ctx->setProperty("CreateDropEvent", 1);
  int res = runUpgrade_Half(ctx, step);
  ctx->stopTest();
  return res;
}

/**
   Test that one node in each nodegroup can be upgrade simultaneously
    - using method2, ie. don't wait for "nostart" before stopping
      next node
*/

int runUpgrade_NR3(NDBT_Context *ctx, NDBT_Step *step) {
  // Assuming 2 replicas

  ctx->setProperty("CreateDropEvent", 1);
  int res = runUpgrade_Half(ctx, step);
  ctx->stopTest();
  return res;
}

/**
   Test that we can upgrade the Ndbds on their own
*/
int runUpgrade_NdbdOnly(NDBT_Context *ctx, NDBT_Step *step) {
  ctx->setProperty("MgmdNodeSet", (Uint32)NodeSet(None));
  int res = runUpgrade_Half(ctx, step);
  ctx->stopTest();
  return res;
}

/**
   Test that we can upgrade the Ndbds first, then
   the MGMDs
*/
int runUpgrade_NdbdFirst(NDBT_Context *ctx, NDBT_Step *step) {
  ctx->setProperty("MgmdNodeSet", (Uint32)NodeSet(None));
  int res = runUpgrade_Half(ctx, step);
  if (res == NDBT_OK) {
    ctx->setProperty("MgmdNodeSet", (Uint32)NodeSet(All));
    ctx->setProperty("NdbdNodeSet", (Uint32)NodeSet(None));
    res = runUpgrade_Half(ctx, step);
  }
  ctx->stopTest();
  return res;
}

/**
   Upgrade some of the MGMDs
*/
int runUpgrade_NotAllMGMD(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  int minMgmVer = 0;
  int maxMgmVer = 0;
  int myVer = NDB_VERSION;

  if (restarter.getNodeTypeVersionRange(NDB_MGM_NODE_TYPE_MGM, minMgmVer,
                                        maxMgmVer) == -1) {
    g_err << "runUpgrade_NotAllMGMD: getNodeTypeVersionRange call failed"
          << endl;
  }

  g_err << "runUpgrade_NotAllMGMD: My version " << myVer << " Min mgm version "
        << minMgmVer << " Max mgm version " << maxMgmVer << endl;

  ctx->setProperty("MgmdNodeSet", (Uint32)NodeSet(NotAll));
  ctx->setProperty("NdbdNodeSet", (Uint32)NodeSet(None));
  int res = runUpgrade_Half(ctx, step);
  ctx->stopTest();
  return res;
}

int runCheckStarted(NDBT_Context *ctx, NDBT_Step *step) {
  // Check cluster is started
  NdbRestarter restarter;
  if (restarter.waitClusterStarted() != 0) {
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }

  // Check atrtclient is started
  AtrtClient atrt;
  if (!atrt.waitConnected()) {
    g_err << "atrt server was not started " << endl;
    return NDBT_FAILED;
  }

  // Make sure atrt assigns nodeid != -1
  SqlResultSet procs;
  if (!atrt.doQuery("SELECT * FROM process where type <> \'mysql\'", procs))
    return NDBT_FAILED;

  while (procs.next()) {
    if (procs.columnAsInt("node_id") == (unsigned)-1) {
      ndbout << "Found one process with node_id -1, "
             << "use --fix-nodeid=1 to atrt to fix this" << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runCreateIndexT1(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  const NdbDictionary::Table *pTab = pDict->getTable("T1");
  if (pTab == 0) {
    g_err << "getTable(T1) error: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  NdbDictionary::Index ind;
  ind.setName("T1X1");
  ind.setTable("T1");
  ind.setType(NdbDictionary::Index::OrderedIndex);
  ind.setLogging(false);
  ind.addColumn("KOL2");
  ind.addColumn("KOL3");
  ind.addColumn("KOL4");
  if (pDict->createIndex(ind, *pTab) != 0) {
    g_err << "createIndex(T1X1) error: " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateAllTables(NDBT_Context *ctx, NDBT_Step *step) {
  Uint32 useRangeScanT1 = ctx->getProperty("UseRangeScanT1", (Uint32)0);

  ndbout_c("createAllTables");
  if (NDBT_Tables::createAllTables(GETNDB(step), false, true))
    return NDBT_FAILED;

  for (int i = 0; i < NDBT_Tables::getNumTables(); i++)
    table_list.push_back(BaseString(NDBT_Tables::getTable(i)->getName()));

  if (useRangeScanT1)
    if (runCreateIndexT1(ctx, step) != NDBT_OK) return NDBT_FAILED;

  return NDBT_OK;
}

int runCreateOneTable(NDBT_Context *ctx, NDBT_Step *step) {
  // Table is already created...
  // so we just add it to table_list
  table_list.push_back(BaseString(ctx->getTab()->getName()));

  return NDBT_OK;
}

int runGetTableList(NDBT_Context *ctx, NDBT_Step *step) {
  table_list.clear();
  ndbout << "Looking for tables ... ";
  for (int i = 0; i < NDBT_Tables::getNumTables(); i++) {
    const NdbDictionary::Table *tab = GETNDB(step)->getDictionary()->getTable(
        NDBT_Tables::getTable(i)->getName());
    if (tab != NULL) {
      ndbout << tab->getName() << " ";
      table_list.push_back(BaseString(tab->getName()));
    }
  }
  ndbout << endl;

  return NDBT_OK;
}

int runLoadAll(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  int result = NDBT_OK;

  for (unsigned i = 0; i < table_list.size(); i++) {
    const NdbDictionary::Table *tab = pDict->getTable(table_list[i].c_str());
    HugoTransactions trans(*tab);
    trans.loadTable(pNdb, records);
    trans.scanUpdateRecords(pNdb, records);
  }

  return result;
}

int runClearAll(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  int result = NDBT_OK;

  for (unsigned i = 0; i < table_list.size(); i++) {
    const NdbDictionary::Table *tab = pDict->getTable(table_list[i].c_str());
    if (tab) {
      HugoTransactions trans(*tab);
      trans.clearTable(pNdb, records);
    }
  }

  return result;
}

int runBasic(NDBT_Context *ctx, NDBT_Step *step) {
  Uint32 useRangeScanT1 = ctx->getProperty("UseRangeScanT1", (uint32)0);

  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  int result = NDBT_OK;

  int l = 0;
  while (!ctx->isTestStopped()) {
    for (unsigned i = 0; i < table_list.size(); i++) {
      const NdbDictionary::Table *tab = pDict->getTable(table_list[i].c_str());
      HugoTransactions trans(*tab);
      switch (l % 4) {
        case 0:
          trans.loadTable(pNdb, records);
          trans.scanUpdateRecords(pNdb, records);
          trans.pkUpdateRecords(pNdb, records);
          trans.pkReadUnlockRecords(pNdb, records);
          break;
        case 1:
          trans.scanUpdateRecords(pNdb, records);
          // TODO make pkInterpretedUpdateRecords work on any table
          // (or check if it does)
          if (strcmp(tab->getName(), "T1") == 0)
            trans.pkInterpretedUpdateRecords(pNdb, records);
          if (strcmp(tab->getName(), "T1") == 0 && useRangeScanT1) {
            const NdbDictionary::Index *pInd = pDict->getIndex("T1X1", "T1");
            if (pInd == 0) {
              g_err << "getIndex(T1X1) error: " << pDict->getNdbError() << endl;
              return NDBT_FAILED;
            }
            // bug#13834481 - bound values do not matter
            const Uint32 lo = 0x11110000;
            const Uint32 hi = 0xaaaa0000;
            HugoTransactions::HugoBound bound_arr[6];
            int bound_cnt = 0;
            for (int j = 0; j <= 1; j++) {
              int n = rand() % 4;
              for (int i = 0; i < n; i++) {
                HugoTransactions::HugoBound &b = bound_arr[bound_cnt++];
                b.attr = i;
                b.type = (j == 0 ? 0 : 2);  // LE/GE
                b.value = (j == 0 ? &lo : &hi);
              }
            }
            g_info << "range scan T1 with " << bound_cnt << " bounds" << endl;
            if (trans.scanReadRecords(pNdb, pInd, records, 0, 0,
                                      NdbOperation::LM_Read, 0, bound_cnt,
                                      bound_arr) != 0) {
              const NdbError &err = trans.getNdbError();
              /*
               * bug#13834481 symptoms include timeouts and error 1231.
               * Check for any non-temporary error.
               */
              if (err.status == NdbError::TemporaryError) {
                g_info << "range scan T1 temporary error: " << err << endl;
              }
              if (err.status != NdbError::TemporaryError) {
                g_err << "range scan T1 permanent error: " << err << endl;
                return NDBT_FAILED;
              }
            }
          }
          trans.clearTable(pNdb, records / 2);
          trans.loadTable(pNdb, records / 2);
          break;
        case 2:
          trans.clearTable(pNdb, records / 2);
          trans.loadTable(pNdb, records / 2);
          trans.clearTable(pNdb, records / 2);
          break;
        case 3:
          // Return value is ignored until WL#4101 is implemented.
          createDropEvent(ctx, step, false);
          break;
      }
    }
    l++;
  }

  return result;
}

#define CHK2(b, e)                                                        \
  if (!(b)) {                                                             \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ << ": " << e \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    break;                                                                \
  }

static int runBug14702377(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  int records = ctx->getNumRecords();
  int result = NDBT_OK;

  while (ctx->getProperty("HalfStartedDone", (Uint32)0) == 0) {
    ndbout << "Wait for half started..." << endl;
    NdbSleep_SecSleep(15);
  }
  ndbout << "Got half started" << endl;

  while (1) {
    require(table_list.size() == 1);
    const char *tabname = table_list[0].c_str();
    const NdbDictionary::Table *tab = 0;
    CHK2((tab = pDict->getTable(tabname)) != 0,
         tabname << ": " << pDict->getNdbError());
    const int ncol = tab->getNoOfColumns();

    {
      HugoTransactions trans(*tab);
      CHK2(trans.loadTable(pNdb, records) == 0, trans.getNdbError());
    }

    for (int r = 0; r < records; r++) {
      // with 1000 records will surely hit bug case
      const int lm = myRandom48(4);           // 2
      const int nval = myRandom48(ncol + 1);  // most
      const bool exist = myRandom48(2);       // false

      NdbTransaction *pTx = 0;
      NdbOperation *pOp = 0;
      CHK2((pTx = pNdb->startTransaction()) != 0, pNdb->getNdbError());
      CHK2((pOp = pTx->getNdbOperation(tab)) != 0, pTx->getNdbError());
      CHK2((pOp->readTuple((NdbOperation::LockMode)lm)) == 0,
           pOp->getNdbError());

      for (int id = 0; id <= 0; id++) {
        const NdbDictionary::Column *c = tab->getColumn(id);
        require(c != 0 && c->getPrimaryKey() &&
                c->getType() == NdbDictionary::Column::Unsigned);
        Uint32 val = myRandom48(records);
        if (!exist) val = 0xaaaa0000 + myRandom48(0xffff + 1);
        const char *valp = (const char *)&val;
        CHK2(pOp->equal(id, valp) == 0, pOp->getNdbError());
      }
      CHK2(result == NDBT_OK, "failed");

      for (int id = 0; id < nval; id++) {
        const NdbDictionary::Column *c = tab->getColumn(id);
        require(c != 0 && (id == 0 || !c->getPrimaryKey()));
        CHK2(pOp->getValue(id) != 0, pOp->getNdbError());
      }
      CHK2(result == NDBT_OK, "failed");

      char info1[200];
      sprintf(info1, "lm=%d nval=%d exist=%d", lm, nval, exist);
      g_info << "PK read T1 exec: " << info1 << endl;
      Uint64 t1 = NdbTick_CurrentMillisecond();
      int ret = pTx->execute(NdbTransaction::NoCommit);
      Uint64 t2 = NdbTick_CurrentMillisecond();
      int msec = (int)(t2 - t1);
      const NdbError &txerr = pTx->getNdbError();
      const NdbError &operr = pOp->getNdbError();
      char info2[300];
      sprintf(info2, "%s msec=%d ret=%d txerr=%d operr=%d", info1, msec, ret,
              txerr.code, operr.code);
      g_info << "PK read T1 done: " << info2 << endl;

      if (ret == 0 && txerr.code == 0 && operr.code == 0) {
        CHK2(exist, "row should not be found: " << info2);
      } else if (ret == 0 && txerr.code == 626 && operr.code == 626) {
        CHK2(!exist, "row should be found: " << info2);
      } else if (txerr.status == NdbError::TemporaryError) {
        g_err << "PK read T1 temporary error (tx): " << info2 << endl;
        NdbSleep_MilliSleep(50);
      } else if (operr.status == NdbError::TemporaryError) {
        g_err << "PK read T1 temporary error (op): " << info2 << endl;
        NdbSleep_MilliSleep(50);
      } else {
        // gets 4012 before bugfix
        CHK2(false, "unexpected error: " << info2);
      }
      pNdb->closeTransaction(pTx);
      pTx = 0;
    }

    break;
  }

  g_err << "Clear half started hold..." << endl;
  ctx->setProperty("HalfStartedHold", (Uint32)0);
  return result;
}

int rollingRestart(NDBT_Context *ctx, NDBT_Step *step) {
  // Assuming 2 replicas

  AtrtClient atrt;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters)) return NDBT_FAILED;

  while (clusters.next()) {
    uint clusterId = clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result)) return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    if (restarter.rollingRestart()) return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runUpgrade_Traffic(NDBT_Context *ctx, NDBT_Step *step) {
  // Assuming 2 replicas

  ndbout_c("upgrading");
  int res = runUpgrade_Half(ctx, step);
  if (res == NDBT_OK) {
    ndbout_c("rolling restarting");
    res = rollingRestart(ctx, step);
  }
  ctx->stopTest();
  return res;
}

/**
 * Return true for versions that have tests named Downgrade*
 * for downgrade tests. (>= 7.2.40, 7.3.28, 7.4.27, 7.5.17, 7.6.13, 8.0.19)
 */
static bool haveNamedDowngradeTests(Uint32 version) {
  const Uint32 major = (version >> 16) & 0xFF;
  const Uint32 minor = (version >> 8) & 0xFF;
  const Uint32 build = (version >> 0) & 0xFF;

  if (major == 7) {
    switch (minor) {
      case 0:
        return false;
      case 1:
        return false;
      case 2:
        return build >= 40;
      case 3:
        return build >= 28;
      case 4:
        return build >= 27;
      case 5:
        return build >= 17;
      case 6:
        return build >= 13;
      default:
        return true;
    }
  }

  return version >= NDB_MAKE_VERSION(8, 0, 19);
}

int startPostUpgradeChecks(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * This will restart *self* in new version
   */

  BaseString extraArgs;
  if (ctx->getProperty("RestartNoDDL", Uint32(0))) {
    /* Ask post-upgrade steps not to perform DDL
     * (e.g. for 6.3->7.0 upgrade)
     */
    extraArgs.append(" --noddl ");
  }

  /**
   * mysql-getopt works so that passing "-n X -n Y" is ok
   *   and is interpreted as "-n Y"
   *
   * so we restart ourselves with testcase-name and "--post-upgrade" appended
   * e.g if testcase is "testUpgrade -n X"
   *     this will restart it as "testUpgrade -n X -n X--post-upgrade"
   */
  BaseString tc;
  std::string tc_name = ctx->getCase()->getName();

  /**
   * In older versions, there are no test cases with names of the form
   * Downgrade* .
   * Hence, when we downgrade from a version that has a test case like
   * Downgrade* to a version that doesn't, the post-upgrade test fails
   * since it is of the form Upgrade* in the lower versions.
   * Hence, we change the names of the post upgrade test cases of the older
   * versions to Upgrade* here.
   */
  if ((tc_name.find("Downgrade") == 0) &&
      !haveNamedDowngradeTests(postVersion)) {
    // Remove _WithMGMDInitialStart and _WithMGMDStart tags
    size_t with_mgmd_initial_start_tag = tc_name.find("_WithMGMDInitialStart");
    size_t with_mgmd_start_tag = tc_name.find("_WithMGMDStart");

    if (with_mgmd_initial_start_tag != std::string::npos) {
      tc_name = tc_name.substr(0, with_mgmd_initial_start_tag);
    } else if (with_mgmd_start_tag != std::string::npos) {
      tc_name = tc_name.substr(0, with_mgmd_start_tag);
    }
    // Change tc name from Downgrade* to Upgrade*
    tc_name = tc_name.substr(strlen("Downgrade"), tc_name.length());
    tc_name = std::string("Upgrade").append(tc_name);
  }
  tc.assfmt("-n %s--post-upgrade %s", tc_name.c_str(), extraArgs.c_str());

  ndbout << "About to restart self with extra arg: " << tc.c_str() << endl;

  AtrtClient atrt;
  int process_id = atrt.getOwnProcessId();
  if (process_id == -1) {
    g_err << "Failed to find own process id" << endl;
    return NDBT_FAILED;
  }

  if (!atrt.changeVersion(process_id, tc.c_str())) return NDBT_FAILED;

  // Will not be reached...

  return NDBT_OK;
}

int startPostUpgradeChecksApiFirst(NDBT_Context *ctx, NDBT_Step *step) {
  /* If Api is upgraded before all NDBDs then it may not
   * be possible to use DDL from the upgraded API
   * The upgraded Api will decide, but we pass NoDDL
   * in
   */
  ctx->setProperty("RestartNoDDL", 1);
  return startPostUpgradeChecks(ctx, step);
}

int runPostUpgradeChecks(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Table will be dropped/recreated
   *   automatically by NDBT...
   *   so when we enter here, this is already tested
   */
  NdbBackup backup;
  backup.set_default_encryption_password(
      ctx->getProperty("BACKUP_PASSWORD", (char *)NULL), -1);

  ndbout << "Starting backup..." << flush;
  if (backup.start() != 0) {
    ndbout << "Failed" << endl;
    return NDBT_FAILED;
  }
  ndbout << "done" << endl;

  if ((ctx->getProperty("NoDDL", Uint32(0)) == 0) &&
      (ctx->getProperty("KeepFS", Uint32(0)) != 0)) {
    /**
     * Bug48227
     * Upgrade with FS 6.3->7.0, followed by table
     * create, followed by Sys restart resulted in
     * table loss.
     */
    Ndb *pNdb = GETNDB(step);
    NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
    {
      NdbDictionary::Dictionary::List l;
      pDict->listObjects(l);
      for (Uint32 i = 0; i < l.count; i++)
        ndbout_c("found %u : %s", l.elements[i].id, l.elements[i].name);
    }

    pDict->dropTable("I3");
    if (NDBT_Tables::createTable(pNdb, "I3")) {
      ndbout_c("Failed to create table!");
      ndbout << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }

    {
      NdbDictionary::Dictionary::List l;
      pDict->listObjects(l);
      for (Uint32 i = 0; i < l.count; i++)
        ndbout_c("found %u : %s", l.elements[i].id, l.elements[i].name);
    }

    NdbRestarter res;
    if (res.restartAll() != 0) {
      ndbout_c("restartAll() failed");
      return NDBT_FAILED;
    }

    if (res.waitClusterStarted() != 0) {
      ndbout_c("waitClusterStarted() failed");
      return NDBT_FAILED;
    }

    CHK_NDB_READY(pNdb);

    if (pDict->getTable("I3") == 0) {
      ndbout_c("Table disappered");
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runWait(NDBT_Context *ctx, NDBT_Step *step) {
  Uint32 waitSeconds = ctx->getProperty("WaitSeconds", Uint32(30));
  while (waitSeconds && !ctx->isTestStopped()) {
    NdbSleep_MilliSleep(1000);
    waitSeconds--;
  }
  ctx->stopTest();
  return NDBT_OK;
}

bool versionsSpanBoundary(int verA, int verB, int incBoundaryVer) {
  int minPeerVer = MIN(verA, verB);
  int maxPeerVer = MAX(verA, verB);

  return ((minPeerVer < incBoundaryVer) && (maxPeerVer >= incBoundaryVer));
}

bool versionsProtocolCompatible(int verA, int verB) {
  /**
   * Version 8.0 introduced some incompatibilities, check
   * whether they make a test unusable
   */
  if (versionsSpanBoundary(verA, verB, NDB_MAKE_VERSION(8, 0, 0))) {
    /* Versions span 8.0 boundary, check compatibility */
    if (!ndbd_protocol_accepted_by_8_0(verA) ||
        !ndbd_protocol_accepted_by_8_0(verB)) {
      ndbout_c(
          "Versions spanning 8.0 boundary not protocol compatible : "
          "%x -> %x",
          verA, verB);
      return false;
    }
  }

  return true;
}

static int checkForDowngrade(NDBT_Context *ctx, NDBT_Step *step) {
  if (preVersion < postVersion) {
    ndbout_c(
        "Error: Not doing downgrade as expected : "
        "%x -> %x",
        preVersion, postVersion);
    return NDBT_FAILED;
  }

  if (!versionsProtocolCompatible(preVersion, postVersion)) {
    ndbout_c("Skipping test due to protocol incompatibility");
    return NDBT_SKIPPED;
  }

  return NDBT_OK;
}

static int checkForUpgrade(NDBT_Context *ctx, NDBT_Step *step) {
  if (preVersion > postVersion) {
    ndbout_c(
        "Error: Not doing upgrade as expected : "
        "%x -> %x",
        preVersion, postVersion);
    return NDBT_FAILED;
  }

  if (!versionsProtocolCompatible(preVersion, postVersion)) {
    ndbout_c("Skipping test due to protocol incompatibility");
    return NDBT_SKIPPED;
  }

  return NDBT_OK;
}

/**
 * This function skips the test case if it is configured to do a non-initial
 * restart but actually requires an initial restart.
 */
static int checkDowngradeCompatibleConfigFileformatVersion(NDBT_Context *ctx,
                                                           NDBT_Step *step) {
  const Uint32 problemBoundary = NDB_USE_CONFIG_VERSION_V2_80;

  const bool need_initial_mgmd_restart =
      versionsSpanBoundary(preVersion, postVersion, problemBoundary);

  const Uint32 initial_mgmd_restart =
      ctx->getProperty("InitialMgmdRestart", Uint32(0));
  if (initial_mgmd_restart == 1) {
    ndbout << "InitialMgmdRestart is set to 1" << endl;
    return NDBT_OK;
  }
  if (need_initial_mgmd_restart) {
    /**
     * Test case is configured for non initial mgmd restart but
     * upgrade/downgrade needs initial restart
     */
    ndbout << "Skipping test due to incompatible config versions "
           << "with non-initial mgmd restart" << endl;
    return NDBT_SKIPPED;
  }
  ndbout << "Config versions are compatible" << endl;
  return NDBT_OK;
}

#define SchemaTransVersion NDB_MAKE_VERSION(6, 4, 0)

int runPostUpgradeDecideDDL(NDBT_Context *ctx, NDBT_Step *step) {
  /* We are running post-upgrade, now examine the versions
   * of connected nodes and update the 'NoDDL' variable
   * accordingly
   */
  /* DDL should be ok as long as
   *  1) All data nodes have the same version
   *  2) There is not some version specific exception
   */
  bool useDDL = true;

  Ndb *pNdb = GETNDB(step);
  NdbRestarter restarter;
  int minNdbVer = 0;
  int maxNdbVer = 0;
  int myVer = NDB_VERSION;

  if (restarter.getNodeTypeVersionRange(NDB_MGM_NODE_TYPE_NDB, minNdbVer,
                                        maxNdbVer) == -1) {
    g_err << "getNodeTypeVersionRange call failed" << endl;
    return NDBT_FAILED;
  }

  if (minNdbVer != maxNdbVer) {
    useDDL = false;
    ndbout << "Ndbd nodes have mixed versions, DDL not supported" << endl;
  }
  if (versionsSpanBoundary(myVer, minNdbVer, SchemaTransVersion)) {
    useDDL = false;
    ndbout
        << "Api and Ndbd versions span schema-trans boundary, DDL not supported"
        << endl;
  }

  ctx->setProperty("NoDDL", useDDL ? 0 : 1);

  if (useDDL) {
    ndbout << "Dropping and recreating tables..." << endl;

    for (int i = 0; i < NDBT_Tables::getNumTables(); i++) {
      /* Drop table (ignoring rc if it doesn't exist etc...) */
      pNdb->getDictionary()->dropTable(NDBT_Tables::getTable(i)->getName());
      int ret =
          NDBT_Tables::createTable(pNdb, NDBT_Tables::getTable(i)->getName(),
                                   false,   // temp
                                   false);  // exists ok
      if (ret) {
        NdbError err = pNdb->getDictionary()->getNdbError();

        g_err << "Failed to create table "
              << NDBT_Tables::getTable(i)->getName() << " error : " << err
              << endl;

        /* Check for allowed exceptions during upgrade */
        if (err.code == 794) {
          /* Schema feature requires data node upgrade */
          if (minNdbVer >= myVer) {
            g_err << "Error 794 received, but data nodes are upgraded" << endl;
            // TODO : Dump versions here
            return NDBT_FAILED;
          }
          g_err << "Create table failure due to old version NDBDs, continuing"
                << endl;
        }
      }
    }
    ndbout << "Done" << endl;
  }

  return NDBT_OK;
}

static int runUpgrade_SR(NDBT_Context *ctx, NDBT_Step *step) {
  /* System restart upgrade.
   * Stop all data nodes
   * Change versions
   * Restart em together.
   */
  AtrtClient atrt;
  NodeSet mgmdNodeSet = All;

  const char *ndbd_args = "";

  bool skipMgmds = (ctx->getProperty("SkipMgmds", Uint32(0)) != 0);

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters)) return NDBT_FAILED;

  while (clusters.next()) {
    uint clusterId = clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result)) return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true);  // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name") << "@"
          << tmp_result.column("connectstring") << "'" << endl;

    if (restarter.waitClusterStarted()) return NDBT_FAILED;

    /* Now restart to nostart state, prior to SR */
    g_err << "Restarting all data nodes-nostart" << endl;
    if (restarter.restartAll2(NdbRestarter::NRRF_NOSTART) != 0) {
      g_err << "Failed to restart all" << endl;
      return NDBT_FAILED;
    }

    ndbout << "Waiting for no-start state" << endl;
    if (restarter.waitClusterNoStart() != 0) {
      g_err << "Failed waiting for NoStart state" << endl;
      return NDBT_FAILED;
    }

    // Restart ndb_mgmd(s)
    if (!skipMgmds) {
      if (runChangeMgmds(ctx, step, restarter, atrt, clusterId, mgmdNodeSet) !=
          NDBT_OK) {
        return NDBT_FAILED;
      }
    } else {
      ndbout << "Skipping MGMD upgrade" << endl;
    }

    // Restart all ndbds
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds)) return NDBT_FAILED;

    uint ndbdCount = ndbds.numRows();
    uint restartCount = ndbdCount;

    ndbout << "Upgrading " << restartCount << " of " << ndbdCount << " ndbds"
           << endl;

    while (ndbds.next()) {
      uint nodeId = ndbds.columnAsInt("node_id");
      uint processId = ndbds.columnAsInt("id");

      ndbout << "Upgrading node " << nodeId << endl;

      if (!atrt.changeVersion(processId, ndbd_args)) return NDBT_FAILED;
    }

    ndbout << "Waiting for no-start state" << endl;
    if (restarter.waitClusterNoStart() != 0) {
      g_err << "Failed waiting for NoStart state" << endl;
      return NDBT_FAILED;
    }

    ndbout << "Starting cluster (SR)" << endl;

    if (restarter.restartAll2(0) != 0) {
      g_err << "Error restarting all nodes" << endl;
      return NDBT_FAILED;
    }

    ndbout << "Waiting for cluster to start" << endl;
    if (restarter.waitClusterStarted() != 0) {
      g_err << "Failed waiting for Cluster start" << endl;
      return NDBT_FAILED;
    }

    ndbout << "Cluster started." << endl;
  }

  return NDBT_OK;
}

static int runStartBlockLcp(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  restarter.setReconnect(true);

  while ((ctx->getProperty("HalfStartedDone", (Uint32)0) == 0) &&
         !ctx->isTestStopped()) {
    ndbout << "runStartBlockLcp: waiting for half nodes to be restarted..."
           << endl;
    NdbSleep_MilliSleep(5000);
  }

  if (ctx->isTestStopped()) {
    return NDBT_FAILED;
  }

  ndbout << "Half of the nodes restarted, beginning slow LCPs for remainder..."
         << endl;

  /* Trigger LCPs which will be slow to complete,
   * testing more complex LCP takeover protocols
   * especially when the last 'old' data node
   * (likely to be DIH Master) fails.
   * */
  do {
    int dumpCode[] = {7099};
    while (restarter.dumpStateAllNodes(dumpCode, 1) != 0) {
    };

    /* Stall fragment completions */
    while (restarter.insertErrorInAllNodes(5073) != 0) {
    };

    /* Allow restarts to continue... */
    ctx->setProperty("HalfStartedHold", Uint32(0));

    /**
     * Only stall for 20s to avoid default LCP frag
     * watchdog timeouts
     */
    NdbSleep_MilliSleep(20000);

    ndbout << "Unblocking LCP..." << endl;
    while (restarter.insertErrorInAllNodes(0) != 0) {
    };

    NdbSleep_MilliSleep(5000);

  } while (!ctx->isTestStopped());

  return NDBT_OK;
}

static int runUpgradeAndFail(NDBT_Context *ctx, NDBT_Step *step) {
  AtrtClient atrt;
  SqlResultSet clusters;

  if (!atrt.getClusters(clusters)) return NDBT_FAILED;

  // Get the first cluster
  clusters.next();

  uint clusterId = clusters.columnAsInt("id");
  SqlResultSet tmp_result;
  if (!atrt.getConnectString(clusterId, tmp_result)) return NDBT_FAILED;

  NdbRestarter restarter(tmp_result.column("connectstring"));
  restarter.setReconnect(true);  // Restarting mgmd
  ndbout << "Cluster '" << clusters.column("name") << "@"
         << tmp_result.column("connectstring") << "'" << endl;

  if (restarter.waitClusterStarted()) return NDBT_FAILED;

  // Restart ndb_mgmd(s)
  if (runChangeMgmds(ctx, step, restarter, atrt, clusterId, All) != NDBT_OK) {
    return NDBT_FAILED;
  }

  ndbout << "Waiting for started" << endl;
  if (restarter.waitClusterStarted()) return NDBT_FAILED;
  ndbout << "Started" << endl;

  // Restart one ndbd
  SqlResultSet ndbds;
  if (!atrt.getNdbds(clusterId, ndbds)) return NDBT_FAILED;

  // Get the node id of first node
  ndbds.next();
  int nodeId = ndbds.columnAsInt("node_id");
  int processId = ndbds.columnAsInt("id");

  ndbout << "Restart node " << nodeId << endl;
  if (!atrt.changeVersion(processId, "--initial=0")) {
    g_err << "Unable to change version of data node" << endl;
    return NDBT_FAILED;
  }

  if (restarter.waitNodesNoStart(&nodeId, 1)) {
    g_err << "The newer version of the node never came up" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Node changed version and in NOSTART state" << endl;
  ndbout << "Arranging for start failure to cause return to NOSTART" << endl;

  /* We need the node to go to NO START after crash.  */
  int restartDump[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
  if (restarter.dumpStateOneNode(nodeId, restartDump, 2)) return NDBT_FAILED;

  /* 1007 forces the node to crash instead of failing with
   * NDBD_EXIT_UPGRADE_INITIAL_REQUIRED */
  restarter.insertErrorInNode(nodeId, 1007);

  int origConnectCount = restarter.getNodeConnectCount(nodeId);

  ndbout << "Starting node" << endl;
  /* Now start it, should fail */
  restarter.startAll();

  ndbout << "Waiting for node to return to NOSTART state" << endl;

  int newConnectCount = origConnectCount;

  while (newConnectCount == origConnectCount) {
    /* Wait for the node to go to no start */
    if (restarter.waitNodesNoStart(&nodeId, 1)) {
      g_err << "Node never crashed" << nodeId << endl;
      return NDBT_FAILED;
    }

    newConnectCount = restarter.getNodeConnectCount(nodeId);
  }

  ndbout << "Node reconnected and returned to NOSTART state "
         << "due to start failure" << endl;

  return NDBT_OK;
}

static int runShowVersion(NDBT_Context *ctx, NDBT_Step *step) {
  const bool postUpgrade = (ctx->getProperty("PostUpgrade", Uint32(0))) != 0;

  ndbout_c("*******************************");
  ndbout_c("%s version", (postUpgrade ? "Post" : "Pre"));

  ndbout_c("  Mysql version : %u.%u.%u%s", MYSQL_VERSION_MAJOR,
           MYSQL_VERSION_MINOR, MYSQL_VERSION_PATCH, MYSQL_VERSION_EXTRA);
  ndbout_c("  Ndb version   : %u.%u.%u %s", NDB_VERSION_MAJOR,
           NDB_VERSION_MINOR, NDB_VERSION_BUILD, NDB_VERSION_STATUS);
  ndbout_c("*******************************");

  if (!postUpgrade) {
    ndbout_c(
        "  Note that this test may fail if Post version does"
        "  not implement ShowVersions, this is not a bug");
  }
  return NDBT_OK;
}

static const char *preVersionFileName = "preVersion.txt";
static const char *postVersionFileName = "postVersion.txt";

static BaseString getFileName(bool post) {
  /* Path is 1 level *above* basedir, treated as 'run' directory */
  BaseString nameBuff;
  nameBuff.assfmt("%s/../%s", getBaseDir(),
                  (post ? postVersionFileName : preVersionFileName));
  return nameBuff;
}

static int runRecordVersion(NDBT_Context *ctx, NDBT_Step *step) {
  const bool postUpgrade = (ctx->getProperty("PostUpgrade", Uint32(0))) != 0;
  BaseString fileNameBuf = getFileName(postUpgrade);
  const char *fileName = fileNameBuf.c_str();

  ndbout_c("Writing version info to %s", fileName);

  FILE *versionFile = fopen(fileName, "w");
  if (!versionFile) {
    ndbout_c("Error opening file %s", fileName);
    return NDBT_FAILED;
  }
  fprintf(versionFile, "%u.%u.%u\n", NDB_VERSION_MAJOR, NDB_VERSION_MINOR,
          NDB_VERSION_BUILD);
  fclose(versionFile);

  return NDBT_OK;
}

static bool readVersionFile(const char *fileName, Uint32 &version) {
  bool ok = true;
  version = 0;

  ndbout_c("Reading version info from %s", fileName);

  FILE *versionFile = fopen(fileName, "r");
  if (versionFile != NULL) {
    unsigned int major = 0;
    unsigned int minor = 0;
    unsigned int build = 0;
    int res = fscanf(versionFile, "%u.%u.%u", &major, &minor, &build);
    if (res == 3) {
      version = NDB_MAKE_VERSION(major, minor, build);
    } else {
      ndbout_c("Version file %s read failed res = %d", fileName, res);
      ok = false;
    }

    fclose(versionFile);
  }

  return ok;
}

/**
 * runReadVersions
 *
 * Read, print and store version info captured by previous test
 * run including runRecordVersion on both versions
 *
 * This can allow version specific conditions to be included
 * in tests.
 */
static int runReadVersions(NDBT_Context *ctx, NDBT_Step *step) {
  preVersion = postVersion = 0;

  const bool preVersionReadOk =
      readVersionFile(getFileName(false).c_str(), preVersion);
  const bool postVersionReadOk =
      readVersionFile(getFileName(true).c_str(), postVersion);

  ndbout_c("runReadVersions");
  ndbout_c("  PreVersion %s  : %u.%u.%u",
           (preVersionReadOk ? "read" : "not found"), ndbGetMajor(preVersion),
           ndbGetMinor(preVersion), ndbGetBuild(preVersion));
  ndbout_c("  PostVersion %s : %u.%u.%u",
           (postVersionReadOk ? "read" : "not found"), ndbGetMajor(postVersion),
           ndbGetMinor(postVersion), ndbGetBuild(postVersion));

  return NDBT_OK;
}

/**
 * runSkipIfCannotKeepFS
 *
 * Check whether we can perform a test while keeping the
 * FS with the versions being tested.
 * If not, skip
 */
static int runSkipIfCannotKeepFS(NDBT_Context *ctx, NDBT_Step *step) {
  if (preVersion == 0 || postVersion == 0) {
    ndbout_c("Missing version info to determine whether to skip, running.");
    return NDBT_OK;
  }

  /**
   * Crossing the boundary of 7.6 is problematic for
   * both upgrades and downgrades due to WL#8069 Partial LCP
   */
  {
    const Uint32 problemBoundary =
        NDB_MAKE_VERSION(7, 6, 3);  // NDBD_LOCAL_SYSFILE_VERSION

    if (versionsSpanBoundary(preVersion, postVersion, problemBoundary)) {
      ndbout_c(
          "Cannot run with these versions as they do not support "
          "non initial upgrades or downgrades (WL#8069).");
      return NDBT_SKIPPED;
    }
  }

  /**
   * Can upgrade across the boundary of 8.0.18, but cannot downgrade
   * due to WL#12876 SYSFILE format version 2
   */
  {
    const bool isDowngrade = postVersion < preVersion;
    const Uint32 problemBoundary = NDB_MAKE_VERSION(8, 0, 18);

    if (isDowngrade &&
        versionsSpanBoundary(preVersion, postVersion, problemBoundary)) {
      ndbout_c(
          "Cannot run with these versions as data nodes require "
          "initial restart for downgrades(WL#12876)");
      return NDBT_SKIPPED;
    }
  }
  return NDBT_OK;
}

/**
 * runSkipIfPostCanKeepFS
 *
 * Will skip unless the post version cannot handle a
 * 'keepFS' upgrade.
 */
static int runSkipIfPostCanKeepFS(NDBT_Context *ctx, NDBT_Step *step) {
  if (preVersion == 0 || postVersion == 0) {
    ndbout_c("Not running as specific version info missing");
    return NDBT_SKIPPED;
  }

  const bool upgrade = (postVersion > preVersion);

  if (!upgrade) {
    /* Only  support upgrade test, downgrade is not handled cleanly attm */
    ndbout_c("Not running as not an upgrade");
    return NDBT_SKIPPED;
  }

  const Uint32 problemBoundary =
      NDB_MAKE_VERSION(7, 6, 3);  // NDBD_LOCAL_SYSFILE_VERSION

  if (!versionsSpanBoundary(preVersion, postVersion, problemBoundary)) {
    ndbout_c("Not running as versions involved are not relevant for this test");
    return NDBT_SKIPPED;
  }
  return NDBT_OK;
}

static int restartDataNodesAtHalfWay(NDBT_Context *ctx, NDBT_Step *step) {
  assert(ctx->getProperty("HalfStartedHold", Uint32(0)) == 1);
  while (ctx->getProperty("HalfStartedDone", Uint32(0)) == 0 &&
         !ctx->isTestStopped()) {
    ndbout_c("Waiting for half changed");
    NdbSleep_SecSleep(5);
  }

  if (ctx->isTestStopped()) {
    return NDBT_OK;
  }

  ndbout_c("Got half changed, performing rolling restart (same version)");

  int rc = rollingRestart(ctx, step);

  ndbout_c("Rolling restart completed, resuming change process");
  ctx->setProperty("HalfStartedHold", Uint32(0));

  return rc;
}

NDBT_TESTSUITE(testUpgrade);
TESTCASE("ShowVersions", "Upgrade API, showing actual versions run") {
  /**
   * This test is used to check the versions involved, and
   * should do a minimal amount of other things, so that it
   * does not depend on anything that can break between
   * releases.
   */
  INITIALIZER(runCheckStarted);
  STEP(runShowVersion);
  STEP(runRecordVersion);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("ShowVersions") {
  /**
   * This test postupgrade is used to check the versions involved, and
   * should do a minimal amount of other things, so that it
   * does not depend on anything that can break between
   * releases.
   */
  TC_PROPERTY("PostUpgrade", Uint32(1));
  INITIALIZER(runCheckStarted);
  STEP(runShowVersion);
  STEP(runRecordVersion);
};
TESTCASE("Upgrade_NR1", "Test that one node at a time can be upgraded") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runBug48416);
  STEP(runUpgrade_NR1);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_NR1") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE(
    "Upgrade_NR2",
    "Test that one node in each nodegroup can be upgradde simultaneously") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  STEP(runUpgrade_NR2);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_NR2") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE(
    "Upgrade_NR3",
    "Test that one node in each nodegroup can be upgradde simultaneously") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  STEP(runUpgrade_NR3);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_NR3") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_FS",
         "Test that one node in each nodegroup can be upgrade simultaneously") {
  TC_PROPERTY("KeepFS", 1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(runCreateAllTables);
  INITIALIZER(runLoadAll);
  STEP(runUpgrade_Traffic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_FS") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_Traffic",
         "Test upgrade with traffic, all tables and restart --initial") {
  TC_PROPERTY("UseRangeScanT1", (Uint32)1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_Traffic") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_Traffic_FS",
         "Test upgrade with traffic, all tables and restart using FS") {
  TC_PROPERTY("UseRangeScanT1", (Uint32)1);
  TC_PROPERTY("KeepFS", 1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_Traffic_FS") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_Traffic_one",
         "Test upgrade with traffic, *one* table and restart --initial") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_Traffic_one") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_Traffic_FS_one",
         "Test upgrade with traffic, all tables and restart using FS") {
  TC_PROPERTY("KeepFS", 1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_Traffic_FS_one") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_Api_Only", "Test that upgrading the Api node only works") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Upgrade_Api_Only") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  TC_PROPERTY("WaitSeconds", 30);
  STEP(runBasic);
  STEP(runPostUpgradeChecks);
  STEP(runWait);
  FINALIZER(runClearAll);
}
TESTCASE("Upgrade_Api_Before_NR1",
         "Test that upgrading the Api node before the kernel works") {
  /* Api, then MGMD(s), then NDBDs */
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Upgrade_Api_Before_NR1") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  STEP(runBasic);
  STEP(runUpgrade_NR1); /* Upgrade kernel nodes using NR1 */
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}
TESTCASE("Upgrade_Api_NDBD_MGMD", "Test that updating in reverse order works") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Upgrade_Api_NDBD_MGMD") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  STEP(runBasic);
  STEP(runUpgrade_NdbdFirst);
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}
TESTCASE("Upgrade_Mixed_MGMD_API_NDBD",
         "Test that upgrading MGMD/API partially before data nodes works") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_NotAllMGMD); /* Upgrade an MGMD */
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecksApiFirst); /* Upgrade Api */
}
POSTUPGRADE("Upgrade_Mixed_MGMD_API_NDBD") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  INITIALIZER(runClearAll); /* Clear rows from old-ver basic run */
  STEP(runBasic);
  STEP(runUpgrade_NdbdFirst); /* Upgrade all Ndbds, then MGMDs finally */
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}
TESTCASE("Bug14702377", "Dirty PK read of non-existent tuple  6.3->7.x hangs") {
  TC_PROPERTY("HalfStartedHold", (Uint32)1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Half);
  STEP(runBug14702377);
}
POSTUPGRADE("Bug14702377") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}
TESTCASE("Upgrade_SR_ManyTablesMaxFrag",
         "Check that number of tables has no impact") {
  TC_PROPERTY("SkipMgmds", Uint32(1)); /* For 7.0.14... */
  TC_PROPERTY("FragmentCount", ~Uint32(0));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(createManyTables);
  STEP(runUpgrade_SR);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Upgrade_SR_ManyTablesMaxFrag") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
  INITIALIZER(dropManyTables);
}
TESTCASE("Upgrade_NR3_LCP_InProgress",
         "Check that half-cluster upgrade with LCP in progress is ok") {
  TC_PROPERTY("HalfStartedHold", Uint32(1)); /* Stop half way through */
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  STEP(runStartBlockLcp);
  STEP(runUpgrade_NR3);
  /* No need for postUpgrade, and cannot rely on it existing for
   * downgrades...
   * Better solution needed for downgrades where postUpgrade is
   * useful, e.g. RunIfPresentElseIgnore...
   */
  // VERIFIER(startPostUpgradeChecks);
}
// POSTUPGRADE("Upgrade_NR3_LCP_InProgress")
//{
//  INITIALIZER(runCheckStarted);
//  INITIALIZER(runPostUpgradeChecks);
//}
TESTCASE(
    "Upgrade_Newer_LCP_FS_Fail",
    "Try upgrading a data node from any lower version to 7.6.4 and fail."
    "7.6.4 has a newer LCP file system and requires a upgrade with initial."
    "(Bug#27308632)") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForUpgrade);
  INITIALIZER(runSkipIfPostCanKeepFS);
  STEP(runUpgradeAndFail);
  // No postupgradecheck required as the upgrade is expected to fail
}
TESTCASE("ChangeHalfRestartChangeHalf",
         "Try changing half datanodes, then rolling restart all "
         "then changing the others") {
  TC_PROPERTY("HalfStartedHold", Uint32(1));
  // Don't restart MGMDs
  TC_PROPERTY("MgmdNodeSet", Uint32(None));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  STEP(runUpgrade_Half);
  STEP(restartDataNodesAtHalfWay);
  // No postupgrade as we do not upgrade API
}

TESTCASE("ChangeMGMDChangeHalfRestartChangeHalf",
         "Try changing MGMD then half datanodes, then rolling restart all "
         "then changing the others") {
  TC_PROPERTY("HalfStartedHold", Uint32(1));
  // Restart MGMDs
  TC_PROPERTY("MgmdNodeSet", Uint32(All));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  STEP(runUpgrade_Half);
  STEP(restartDataNodesAtHalfWay);
  // No postupgrade as we do not upgrade API
}

// Downgrade tests //

TESTCASE("Downgrade_NR1", "Test that one node at a time can be downgraded") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runBug48416);
  STEP(runUpgrade_NR1);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_NR1") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_NR1_WithMGMDStart",
         "Test that one node at a time can be downgraded") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(0));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runBug48416);
  STEP(runUpgrade_NR1);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_NR1_WithMGMDStart") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_NR2",
    "Test that one node in each nodegroup can be downgraded simultaneously") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runUpgrade_NR2);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_NR2") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_NR2_WithMGMDInitialStart",
    "Test that one node in each nodegroup can be downgraded simultaneously") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runUpgrade_NR2);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_NR2_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_NR3",
    "Test that one node in each nodegroup can be downgraded simultaneously") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runUpgrade_NR3);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_NR3") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_NR3_WithMGMDInitialStart",
    "Test that one node in each nodegroup can be downgraded simultaneously") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runUpgrade_NR3);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_NR3_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_FS",
    "Test that one node in each nodegroup can be downgraded simultaneously") {
  TC_PROPERTY("KeepFS", 1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  INITIALIZER(runLoadAll);
  STEP(runUpgrade_Traffic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_FS") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_FS_WithMGMDInitialStart",
    "Test that one node in each nodegroup can be downgraded simultaneously") {
  TC_PROPERTY("KeepFS", 1);
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  INITIALIZER(runLoadAll);
  STEP(runUpgrade_Traffic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_FS_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic",
         "Test downgrade with traffic, all tables and restart --initial") {
  TC_PROPERTY("UseRangeScanT1", (Uint32)1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_WithMGMDInitialStart",
         "Test downgrade with traffic, all tables and restart --initial") {
  TC_PROPERTY("UseRangeScanT1", (Uint32)1);
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_FS",
         "Test downgrade with traffic, all tables and restart using FS") {
  TC_PROPERTY("UseRangeScanT1", (Uint32)1);
  TC_PROPERTY("KeepFS", 1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_FS") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_FS_WithMGMDInitialStart",
         "Test downgrade with traffic, all tables and restart using FS") {
  TC_PROPERTY("UseRangeScanT1", (Uint32)1);
  TC_PROPERTY("KeepFS", 1);
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_FS_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_one",
         "Test downgrade with traffic, *one* table and restart --initial") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_one") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_one_WithMGMDInitialStart",
         "Test downgrade with traffic, *one* table and restart --initial") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_one_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_FS_one",
         "Test downgrade with traffic, all tables and restart using FS") {
  TC_PROPERTY("KeepFS", 1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_FS_one") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Traffic_FS_one_WithMGMDInitialStart",
         "Test downgrade with traffic, all tables and restart using FS") {
  TC_PROPERTY("KeepFS", 1);
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Traffic);
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_Traffic_FS_one_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Api_Only",
         "Test that downgrading the Api node only works") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Downgrade_Api_Only") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  TC_PROPERTY("WaitSeconds", 30);
  STEP(runBasic);
  STEP(runPostUpgradeChecks);
  STEP(runWait);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Api_Only_WithMGMDInitialStart",
         "Test that downgrading the Api node only works") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Downgrade_Api_Only_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  TC_PROPERTY("WaitSeconds", 30);
  STEP(runBasic);
  STEP(runPostUpgradeChecks);
  STEP(runWait);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Api_Before_NR1",
         "Test that downgrading the Api node before the kernel works") {
  /* Api, then MGMD(s), then NDBDs */
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Downgrade_Api_Before_NR1") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  STEP(runBasic);
  STEP(runUpgrade_NR1); /* Upgrade kernel nodes using NR1 */
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Api_Before_NR1_WithMGMDStart",
         "Test that downgrading the Api node before the kernel works") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(0));
  /* Api, then MGMD(s), then NDBDs */
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Downgrade_Api_Before_NR1_WithMGMDStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(0));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  STEP(runBasic);
  STEP(runUpgrade_NR1); /* Upgrade kernel nodes using NR1 */
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Api_NDBD_MGMD",
         "Test that updating in reverse order works") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Downgrade_Api_NDBD_MGMD") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  STEP(runBasic);
  STEP(runUpgrade_NdbdFirst);
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Api_NDBD_MGMD_WithMGMDInitialStart",
         "Test that updating in reverse order works") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  VERIFIER(startPostUpgradeChecksApiFirst);
}
POSTUPGRADE("Downgrade_Api_NDBD_MGMD_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  STEP(runBasic);
  STEP(runUpgrade_NdbdFirst);
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Mixed_MGMD_API_NDBD",
         "Test that downgrading MGMD/API partially before data nodes works") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateAllTables);
  STEP(runUpgrade_NotAllMGMD); /* Upgrade an MGMD */
  STEP(runBasic);
  VERIFIER(startPostUpgradeChecksApiFirst); /* Upgrade Api */
}
POSTUPGRADE("Downgrade_Mixed_MGMD_API_NDBD") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeDecideDDL);
  INITIALIZER(runGetTableList);
  INITIALIZER(runClearAll); /* Clear rows from old-ver basic run */
  STEP(runBasic);
  STEP(runUpgrade_NdbdFirst); /* Upgrade all Ndbds, then MGMDs finally */
  FINALIZER(runPostUpgradeChecks);
  FINALIZER(runClearAll);
}

TESTCASE("Downgrade_Bug14702377",
         "Dirty PK read of non-existent tuple  6.3->7.x hangs") {
  TC_PROPERTY("HalfStartedHold", (Uint32)1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Half);
  STEP(runBug14702377);
}
POSTUPGRADE("Downgrade_Bug14702377") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_Bug14702377_WithMGMDInitialStart",
         "Dirty PK read of non-existent tuple  6.3->7.x hangs") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  TC_PROPERTY("HalfStartedHold", (Uint32)1);
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(runCreateOneTable);
  STEP(runUpgrade_Half);
  STEP(runBug14702377);
}
POSTUPGRADE("Downgrade_Bug14702377_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
}

TESTCASE("Downgrade_SR_ManyTablesMaxFrag",
         "Check that number of tables has no impact") {
  TC_PROPERTY("SkipMgmds", Uint32(1)); /* For 7.0.14... */
  TC_PROPERTY("FragmentCount", ~Uint32(0));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(createManyTables);
  STEP(runUpgrade_SR);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_SR_ManyTablesMaxFrag") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
  INITIALIZER(dropManyTables);
}

TESTCASE("Downgrade_SR_ManyTablesMaxFrag_WithMGMDInitialStart",
         "Check that number of tables has no impact") {
  TC_PROPERTY("SkipMgmds", Uint32(1)); /* For 7.0.14... */
  TC_PROPERTY("FragmentCount", ~Uint32(0));
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfCannotKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  INITIALIZER(createManyTables);
  STEP(runUpgrade_SR);
  VERIFIER(startPostUpgradeChecks);
}
POSTUPGRADE("Downgrade_SR_ManyTablesMaxFrag_WithMGMDInitialStart") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runPostUpgradeChecks);
  INITIALIZER(dropManyTables);
}

TESTCASE("Downgrade_NR3_LCP_InProgress",
         "Check that half-cluster downgrade with LCP in progress is ok") {
  TC_PROPERTY("HalfStartedHold", Uint32(1)); /* Stop half way through */
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runStartBlockLcp);
  STEP(runUpgrade_NR3);
  /* No need for postUpgrade, and cannot rely on it existing for
   * downgrades...
   * Better solution needed for downgrades where postUpgrade is
   * useful, e.g. RunIfPresentElseIgnore...
   */
  // VERIFIER(startPostUpgradeChecks);
}
// POSTUPGRADE("Upgrade_NR3_LCP_InProgress")
//{
//  INITIALIZER(runCheckStarted);
//  INITIALIZER(runPostUpgradeChecks);
//}

TESTCASE("Downgrade_NR3_LCP_InProgress_WithMGMDInitialStart",
         "Check that half-cluster downgrade with LCP in progress is ok") {
  TC_PROPERTY("HalfStartedHold", Uint32(1)); /* Stop half way through */
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runStartBlockLcp);
  STEP(runUpgrade_NR3);
  /* No need for postUpgrade, and cannot rely on it existing for
   * downgrades...
   * Better solution needed for downgrades where postUpgrade is
   * useful, e.g. RunIfPresentElseIgnore...
   */
  // VERIFIER(startPostUpgradeChecks);
}

TESTCASE(
    "Downgrade_Newer_LCP_FS_Fail",
    "Try downgrading a data node from any lower version to 7.6.4 and fail."
    "7.6.4 has a newer LCP file system and requires a downgrade with initial."
    "(Bug#27308632)") {
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfPostCanKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runUpgradeAndFail);
  // No postupgradecheck required as the downgrade is expected to fail
}

TESTCASE(
    "Downgrade_Newer_LCP_FS_Fail_WithMGMDInitialStart",
    "Try downgrading a data node from any lower version to 7.6.4 and fail."
    "7.6.4 has a newer LCP file system and requires a downgrade with initial."
    "(Bug#27308632)") {
  TC_PROPERTY("InitialMgmdRestart", Uint32(1));
  INITIALIZER(runCheckStarted);
  INITIALIZER(runReadVersions);
  INITIALIZER(checkForDowngrade);
  INITIALIZER(runSkipIfPostCanKeepFS);
  INITIALIZER(checkDowngradeCompatibleConfigFileformatVersion);
  STEP(runUpgradeAndFail);
  // No postupgradecheck required as the downgrade is expected to fail
}

NDBT_TESTSUITE_END(testUpgrade)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testUpgrade);
  testUpgrade.setCreateAllTables(true);
  if (0) {
    static char env[100];
    strcpy(env, "API_SIGNAL_LOG=-");  // stdout
    putenv(env);
  }
  createDropEvent_mutex = NdbMutex_Create();
  int ret = testUpgrade.execute(argc, argv);
  NdbMutex_Destroy(createDropEvent_mutex);
  return ret;
}

template class Vector<NodeInfo>;
