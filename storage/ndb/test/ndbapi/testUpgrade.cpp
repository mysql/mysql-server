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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <AtrtClient.hpp>
#include <Bitmask.hpp>


/**
  Test that one node at a time can be upgraded
*/

int runUpgrade_NR1(NDBT_Context* ctx, NDBT_Step* step){
  AtrtClient atrt;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters))
    return NDBT_FAILED;

  while (clusters.next())
  {
    uint clusterId= clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result))
      return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true); // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name")
          << "@" << tmp_result.column("connectstring") << "'" << endl;

    if (restarter.waitClusterStarted(1))
      return NDBT_FAILED;

    // Restart ndb_mgmd(s)
    SqlResultSet mgmds;
    if (!atrt.getMgmds(clusterId, mgmds))
      return NDBT_FAILED;

    while (mgmds.next())
    {
      ndbout << "Restart mgmd " << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.changeVersion(mgmds.columnAsInt("id"), ""))
        return NDBT_FAILED;

      if (restarter.waitConnected())
        return NDBT_FAILED;
      ndbout << "Connected to mgmd"<< endl;
    }

    ndbout << "Waiting for started"<< endl;
    if (restarter.waitClusterStarted(1))
      return NDBT_FAILED;
    ndbout << "Started"<< endl;

    // Restart ndbd(s)
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds))
      return NDBT_FAILED;

    while(ndbds.next())
    {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");
      ndbout << "Restart node " << nodeId << endl;

      if (!atrt.changeVersion(processId, ""))
        return NDBT_FAILED;

      if (restarter.waitNodesNoStart(&nodeId, 1))
        return NDBT_FAILED;

      if (restarter.startNodes(&nodeId, 1))
        return NDBT_FAILED;

      if (restarter.waitNodesStarted(&nodeId, 1))
        return NDBT_FAILED;

    }
  }

  ctx->stopTest();
  return NDBT_OK;
}


/**
   Test that one node in each nodegroup can be upgraded simultaneously
    - using method1
*/

int runUpgrade_NR2(NDBT_Context* ctx, NDBT_Step* step){
  // Assuming 2 replicas

  AtrtClient atrt;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters))
    return NDBT_FAILED;

  while (clusters.next())
  {
    uint clusterId= clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result))
      return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true); // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name")
          << "@" << tmp_result.column("connectstring") << "'" << endl;

    if(restarter.waitClusterStarted(1))
      return NDBT_FAILED;

    // Restart ndb_mgmd(s)
    SqlResultSet mgmds;
    if (!atrt.getMgmds(clusterId, mgmds))
      return NDBT_FAILED;

    while (mgmds.next())
    {
      ndbout << "Restart mgmd" << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.changeVersion(mgmds.columnAsInt("id"), ""))
        return NDBT_FAILED;

      if(restarter.waitConnected())
        return NDBT_FAILED;
    }

    // Restart one ndbd in each node group
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds))
      return NDBT_FAILED;

    Bitmask<4> seen_groups;
    while(ndbds.next())
    {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");
      int nodeGroup= restarter.getNodeGroup(nodeId);

      if (seen_groups.get(nodeGroup)){
        // One node in this node group already down
        continue;
      }
      seen_groups.set(nodeGroup);

      // Remove row from resultset
      ndbds.remove();

      ndbout << "Restart node " << nodeId << endl;

      if (!atrt.changeVersion(processId, ""))
        return NDBT_FAILED;

      if (restarter.waitNodesNoStart(&nodeId, 1))
        return NDBT_FAILED;

    }

    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;

    // Restart the remaining nodes
    ndbds.reset();
    while(ndbds.next())
    {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");

      ndbout << "Restart node " << nodeId << endl;
      if (!atrt.changeVersion(processId, ""))
        return NDBT_FAILED;

      if (restarter.waitNodesNoStart(&nodeId, 1))
        return NDBT_FAILED;

    }

    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;

  }

  ctx->stopTest();
  return NDBT_OK;
}


/**
   Test that one node in each nodegroup can be upgrade simultaneously
    - using method2, ie. don't wait for "nostart" before stopping
      next node
*/

int runUpgrade_NR3(NDBT_Context* ctx, NDBT_Step* step){
  // Assuming 2 replicas

  AtrtClient atrt;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters))
    return NDBT_FAILED;

  while (clusters.next())
  {
    uint clusterId= clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result))
      return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true); // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name")
          << "@" << tmp_result.column("connectstring") << "'" << endl;

    if(restarter.waitClusterStarted(1))
      return NDBT_FAILED;

    // Restart ndb_mgmd(s)
    SqlResultSet mgmds;
    if (!atrt.getMgmds(clusterId, mgmds))
      return NDBT_FAILED;

    while (mgmds.next())
    {
      ndbout << "Restart mgmd" << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.changeVersion(mgmds.columnAsInt("id"), ""))
        return NDBT_FAILED;

      if(restarter.waitConnected())
        return NDBT_FAILED;
    }

    // Restart one ndbd in each node group
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds))
      return NDBT_FAILED;

    int nodes[256];
    int cnt= 0;

    Bitmask<4> seen_groups;
    while(ndbds.next())
    {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");
      int nodeGroup= restarter.getNodeGroup(nodeId);

      if (seen_groups.get(nodeGroup)){
        // One node in this node group already down
        continue;
      }
      seen_groups.set(nodeGroup);

      // Remove row from resultset
      ndbds.remove();

      ndbout << "Restart node " << nodeId << endl;

      if (!atrt.changeVersion(processId, ""))
        return NDBT_FAILED;

      nodes[cnt++]= nodeId;
    }

    if (restarter.waitNodesNoStart(nodes, cnt))
      return NDBT_FAILED;

    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;

    // Restart the remaining nodes
    cnt= 0;
    ndbds.reset();
    while(ndbds.next())
    {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");

      ndbout << "Restart node " << nodeId << endl;
      if (!atrt.changeVersion(processId, ""))
        return NDBT_FAILED;


      nodes[cnt++]= nodeId;

    }

    if (restarter.waitNodesNoStart(nodes, cnt))
      return NDBT_FAILED;


    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;

  }

  ctx->stopTest();
  return NDBT_OK;
}


int runCheckStarted(NDBT_Context* ctx, NDBT_Step* step){

  // Check cluster is started
  NdbRestarter restarter;
  if(restarter.waitClusterStarted(1) != 0){
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }

  // Check atrtclient is started
  AtrtClient atrt;
  if(!atrt.waitConnected(60)){
    g_err << "atrt server was not started " << endl;
    return NDBT_FAILED;
  }

  // Make sure atrt assigns nodeid != -1
  SqlResultSet procs;
  if (!atrt.doQuery("SELECT * FROM process", procs))
    return NDBT_FAILED;

  while (procs.next())
  {
    if (procs.columnAsInt("node_id") == -1){
      ndbout << "Found one process with node_id -1, "
             << "use --fix-nodeid=1 to atrt to fix this" << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}


int runRestoreProcs(NDBT_Context* ctx, NDBT_Step* step){
  AtrtClient atrt;
  g_err << "Starting to reset..." << endl;

  SqlResultSet clusters;
  if (!atrt.getClusters(clusters))
    return NDBT_FAILED;

  while (clusters.next())
  {
    uint clusterId= clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result))
      return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    restarter.setReconnect(true); // Restarting mgmd
    g_err << "Cluster '" << clusters.column("name")
          << "@" << tmp_result.column("connectstring") << "'" << endl;

    if(restarter.waitClusterStarted(1))
      return NDBT_FAILED;

    // Reset ndb_mgmd(s)
    SqlResultSet mgmds;
    if (!atrt.getMgmds(clusterId, mgmds))
      return NDBT_FAILED;

    while (mgmds.next())
    {
      ndbout << "Reset mgmd" << mgmds.columnAsInt("node_id") << endl;
      if (!atrt.resetProc(mgmds.columnAsInt("id")))
        return NDBT_FAILED;

      if(restarter.waitConnected() != 0)
        return NDBT_FAILED;
    }

    if(restarter.waitClusterStarted(1))
      return NDBT_FAILED;

    // Reset ndbd(s)
    SqlResultSet ndbds;
    if (!atrt.getNdbds(clusterId, ndbds))
      return NDBT_FAILED;

    while(ndbds.next())
    {
      int nodeId = ndbds.columnAsInt("node_id");
      int processId = ndbds.columnAsInt("id");
      ndbout << "Reset node " << nodeId << endl;

      if (!atrt.resetProc(processId))
        return NDBT_FAILED;

    }

    if (restarter.waitClusterNoStart())
      return NDBT_FAILED;

  }


  // All nodes are in no start, start them up again
  clusters.reset();
  while (clusters.next())
  {
    uint clusterId= clusters.columnAsInt("id");
    SqlResultSet tmp_result;
    if (!atrt.getConnectString(clusterId, tmp_result))
      return NDBT_FAILED;

    NdbRestarter restarter(tmp_result.column("connectstring"));
    g_err << "Cluster '" << clusters.column("name")
          << "@" << tmp_result.column("connectstring") << "'" << endl;

    if (restarter.waitClusterNoStart())
      return NDBT_FAILED;

    ndbout << "Starting and wait for started..." << endl;
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}



NDBT_TESTSUITE(testUpgrade);
TESTCASE("Upgrade_NR1",
	 "Test that one node at a time can be upgraded"){
  INITIALIZER(runCheckStarted);
  STEP(runUpgrade_NR1);
  FINALIZER(runRestoreProcs);
}
TESTCASE("Upgrade_NR2",
	 "Test that one node in each nodegroup can be upgradde simultaneously"){
  INITIALIZER(runCheckStarted);
  STEP(runUpgrade_NR2);
  FINALIZER(runRestoreProcs);
}
TESTCASE("Upgrade_NR3",
	 "Test that one node in each nodegroup can be upgrade simultaneously"){
  INITIALIZER(runCheckStarted);
  STEP(runUpgrade_NR3);
  FINALIZER(runRestoreProcs);
}
NDBT_TESTSUITE_END(testUpgrade);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testUpgrade);
  testUpgrade.setCreateAllTables(true);
  return testUpgrade.execute(argc, argv);
}

