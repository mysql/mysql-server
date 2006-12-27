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

#include <signaldata/DumpStateOrd.hpp>
#include <NdbGrep.hpp>
#include <NdbOut.hpp>
#include <NDBT_Output.hpp>
#include <NdbConfig.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>
#include <NDBT.hpp>
#include <NdbSleep.h>
#include <random.h>
#include <NdbTick.h>

#define CHECK(b, m) { int _xx = b; if (!(_xx)) { \
  ndbout << "ERR: "<< m \
           << "   " << "File: " << __FILE__ \
           << " (Line: " << __LINE__ << ")" << "- " << _xx << endl; \
  return NDBT_FAILED; } }


int 
NdbGrep::start(){

  return 1;
}

int 
NdbGrep::stop(){

  return 1;
}


int 
NdbGrep::query(){

  return 1;
}


int 
NdbGrep::verify(NDBT_Context * ctx){
  
  if (!isConnected())
    return -1;

  char cheat_table[255];
  BaseString::snprintf(cheat_table, 255, "TEST_DB/def/%s",ctx->getTab()->getName());

  char buf[255];
  BaseString::snprintf(buf, 255, "testGrepVerify -c \"nodeid=%d;host=%s\" -t %s -r %d", 
	   4,  //cheat. Hardcoded nodeid....
	   ctx->getRemoteMgm(),
	   cheat_table,
	   ctx->getNumRecords());
  

  ndbout << "buf: "<< buf <<endl;
  int res = system(buf);  

  ndbout << "res: " << res << endl;

  return res;
  

  

}


// Master failure
int
NFDuringGrepM_codes[] = {
  10003,
  10004,
  10005,
  10007,
  10008,
  10009,
  10010,
  10012,
  10013
};

// Slave failure
int
NFDuringGrepS_codes[] = {
  10014,
  10015,
  10016,
  10017,
  10018,
  10020
};

// Master takeover etc...
int
NFDuringGrepSL_codes[] = {
  10001,
  10002,
  10021
};

int 
NdbGrep::NFMaster(NdbRestarter& _restarter){
  const int sz = sizeof(NFDuringGrepM_codes)/sizeof(NFDuringGrepM_codes[0]);
  return NF(_restarter, NFDuringGrepM_codes, sz, true);
}

int 
NdbGrep::NFMasterAsSlave(NdbRestarter& _restarter){
  const int sz = sizeof(NFDuringGrepS_codes)/sizeof(NFDuringGrepS_codes[0]);
  return NF(_restarter, NFDuringGrepS_codes, sz, true);
}

int 
NdbGrep::NFSlave(NdbRestarter& _restarter){
  const int sz = sizeof(NFDuringGrepS_codes)/sizeof(NFDuringGrepS_codes[0]);
  return NF(_restarter, NFDuringGrepS_codes, sz, false);
}

int 
NdbGrep::NF(NdbRestarter& _restarter, int *NFDuringGrep_codes, const int sz, bool onMaster){
  {
    int nodeId = _restarter.getMasterNodeId();

    CHECK(_restarter.restartOneDbNode(nodeId, false, true, true) == 0,
	  "Could not restart node "<< nodeId);
    
    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
	  "waitNodesNoStart failed");
    
    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");

    NdbSleep_SecSleep(10);
  }

  CHECK(_restarter.waitClusterStarted() == 0,
	"waitClusterStarted failed");

  int nNodes = _restarter.getNumDbNodes();

  myRandom48Init(NdbTick_CurrentMillisecond());

  for(int i = 0; i<sz; i++){

    int error = NFDuringGrep_codes[i];
    unsigned int backupId;

    const int masterNodeId = _restarter.getMasterNodeId();
    CHECK(masterNodeId > 0, "getMasterNodeId failed");
    int nodeId;

    nodeId = masterNodeId;
    if (!onMaster) {
      int randomId;
      while (nodeId == masterNodeId) {
	randomId = myRandom48(nNodes);
	nodeId = _restarter.getDbNodeId(randomId);
      }
    }

    g_err << "NdbGrep::NF node = " << nodeId 
	   << " error code = " << error << " masterNodeId = "
	   << masterNodeId << endl;


    int val = DumpStateOrd::CmvmiSetRestartOnErrorInsert;
    CHECK(_restarter.dumpStateOneNode(nodeId, &val, 1) == 0,
	  "failed to set RestartOnErrorInsert");
    CHECK(_restarter.insertErrorInNode(nodeId, error) == 0,
	  "failed to set error insert");
   
    g_info << "error inserted"  << endl;

    g_info << "starting backup"  << endl;
    int r = start();
    g_info << "r = " << r
	   << " (which should fail) started with id = "  << backupId << endl;
    if (r == 0) {
      g_err << "Grep should have failed on error_insertion " << error << endl
	    << "Master = " << masterNodeId << "Node = " << nodeId << endl;
    }

    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
	  "waitNodesNoStart failed");

    g_info << "number of nodes running " << _restarter.getNumDbNodes() << endl;

    if (_restarter.getNumDbNodes() != nNodes) {
      g_err << "Failure: cluster not up" << endl;
      return NDBT_FAILED;
    }

    NdbSleep_SecSleep(1);

    g_info << "starting new backup"  << endl;
    CHECK(start() == 0,
	  "failed to start backup");
    g_info << "(which should succeed) started with id = "  << backupId << endl;

    g_info << "starting node"  << endl;
    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");

    CHECK(_restarter.waitClusterStarted() == 0,
	  "waitClusterStarted failed");
    g_info << "node started"  << endl;

    CHECK(_restarter.insertErrorInNode(nodeId, 10099) == 0,
	  "failed to set error insert");
  }

  return NDBT_OK;
}

int
FailS_codes[] = {
  10023,
  10024,
  10025,
  10026,
  10027,
  10028,
 10029,
  10030,
  10031
};

int
FailM_codes[] = {
  10023,
  10024,
  10025,
  10026,
  10027,
  10028,
  10029,
  10030,
  10031
};

int 
NdbGrep::FailMaster(NdbRestarter& _restarter){
  const int sz = sizeof(FailM_codes)/sizeof(FailM_codes[0]);
  return Fail(_restarter, FailM_codes, sz, true);
}

int 
NdbGrep::FailMasterAsSlave(NdbRestarter& _restarter){
  const int sz = sizeof(FailS_codes)/sizeof(FailS_codes[0]);
  return Fail(_restarter, FailS_codes, sz, true);
}

int 
NdbGrep::FailSlave(NdbRestarter& _restarter){
  const int sz = sizeof(FailS_codes)/sizeof(FailS_codes[0]);
  return Fail(_restarter, FailS_codes, sz, false);
}

int 
NdbGrep::Fail(NdbRestarter& _restarter, int *Fail_codes, const int sz, bool onMaster){

  CHECK(_restarter.waitClusterStarted() == 0,
	"waitClusterStarted failed");

  int nNodes = _restarter.getNumDbNodes();

  myRandom48Init(NdbTick_CurrentMillisecond());

  for(int i = 0; i<sz; i++){
    int error = Fail_codes[i];
    unsigned int backupId;

    const int masterNodeId = _restarter.getMasterNodeId();
    CHECK(masterNodeId > 0, "getMasterNodeId failed");
    int nodeId;

    nodeId = masterNodeId;
    if (!onMaster) {
      int randomId;
      while (nodeId == masterNodeId) {
	randomId = myRandom48(nNodes);
	nodeId = _restarter.getDbNodeId(randomId);
      }
    }

    g_err << "NdbGrep::Fail node = " << nodeId 
	   << " error code = " << error << " masterNodeId = "
	   << masterNodeId << endl;

    CHECK(_restarter.insertErrorInNode(nodeId, error) == 0,
	  "failed to set error insert");
   
    g_info << "error inserted"  << endl;
    g_info << "waiting some before starting backup"  << endl;

    g_info << "starting backup"  << endl;
    int r = start();
    g_info << "r = " << r
	   << " (which should fail) started with id = "  << backupId << endl;
    if (r == 0) {
      g_err << "Grep should have failed on error_insertion " << error << endl
	    << "Master = " << masterNodeId << "Node = " << nodeId << endl;
    }

    CHECK(_restarter.waitClusterStarted() == 0,
	  "waitClusterStarted failed");

    CHECK(_restarter.insertErrorInNode(nodeId, 10099) == 0,
	  "failed to set error insert");
  }

  return NDBT_OK;
}


