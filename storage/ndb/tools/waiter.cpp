/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <time.h>
#include <ndb_global.h>
#include <ndb_opts.h>

#include <mgmapi.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>

#include <NDBT.hpp>

#include <kernel/NodeBitmask.hpp>

static int
waitClusterStatus(const char* _addr, ndb_mgm_node_status _status);

static int _no_contact = 0;
static int _not_started = 0;
static int _single_user = 0;
static int _timeout = 120;
static const char* _wait_nodes = 0;
static const char* _nowait_nodes = 0;
static NdbNodeBitmask nowait_nodes_bitmask;

const char *load_default_groups[]= { "mysql_cluster",0 };

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_waiter"),
  { "no-contact", 'n', "Wait for cluster no contact",
    (uchar**) &_no_contact, (uchar**) &_no_contact, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "not-started", NDB_OPT_NOSHORT, "Wait for cluster not started",
    (uchar**) &_not_started, (uchar**) &_not_started, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "single-user", NDB_OPT_NOSHORT,
    "Wait for cluster to enter single user mode",
    (uchar**) &_single_user, (uchar**) &_single_user, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "timeout", 't', "Timeout to wait in seconds",
    (uchar**) &_timeout, (uchar**) &_timeout, 0,
    GET_INT, REQUIRED_ARG, 120, 0, 0, 0, 0, 0 }, 
  { "wait-nodes", 'w', "Node ids to wait on, e.g. '1,2-4'",
    (uchar**) &_wait_nodes, (uchar**) &_wait_nodes, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "nowait-nodes", NDB_OPT_NOSHORT,
    "Nodes that will not be waited for, e.g. '2,3,4-7'",
    (uchar**) &_nowait_nodes, (uchar**) &_nowait_nodes, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
}

static void usage()
{
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

extern "C"
void catch_signal(int signum)
{
}

#include "../src/common/util/parse_mask.hpp"

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  load_defaults("my",load_default_groups,&argc,&argv);

#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_waiter.trace";
#endif

#ifndef _WIN32
  // Catching signal to allow testing of EINTR safeness
  // with "while killall -USR1 ndbwaiter; do true; done"
  signal(SIGUSR1, catch_signal);
#endif

  if (handle_options(&argc, &argv, my_long_options,
                     ndb_std_get_one_option))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  const char* connect_string = argv[0];
  if (connect_string == 0)
    connect_string = opt_ndb_connectstring;

  enum ndb_mgm_node_status wait_status;
  if (_no_contact)
  {
    wait_status= NDB_MGM_NODE_STATUS_NO_CONTACT;
  }
  else if (_not_started)
  {
    wait_status= NDB_MGM_NODE_STATUS_NOT_STARTED;
  }
  else if (_single_user)
  {
    wait_status= NDB_MGM_NODE_STATUS_SINGLEUSER;
  }
  else 
  {
    wait_status= NDB_MGM_NODE_STATUS_STARTED;
  }

  if (_nowait_nodes)
  {
    int res = parse_mask(_nowait_nodes, nowait_nodes_bitmask);
    if(res == -2 || (res > 0 && nowait_nodes_bitmask.get(0)))
    {
      ndbout_c("Invalid nodeid specified in nowait-nodes: %s", 
               _nowait_nodes);
      exit(-1);
    }
    else if (res < 0)
    {
      ndbout_c("Unable to parse nowait-nodes argument: %s",
               _nowait_nodes);
      exit(-1);
    }
  }

  if (_wait_nodes)
  {
    if (_nowait_nodes)
    {
      ndbout_c("Can not set both wait-nodes and nowait-nodes.");
      exit(-1);
    }

    int res = parse_mask(_wait_nodes, nowait_nodes_bitmask);
    if (res == -2 || (res > 0 && nowait_nodes_bitmask.get(0)))
    {
      ndbout_c("Invalid nodeid specified in wait-nodes: %s",
               _wait_nodes);
      exit(-1);
    }
    else if (res < 0)
    {
      ndbout_c("Unable to parse wait-nodes argument: %s",
               _wait_nodes);
      exit(-1);
    }

    // Don't wait for any other nodes than the ones we have set explicitly
    nowait_nodes_bitmask.bitNOT();
  }

  if (waitClusterStatus(connect_string, wait_status) != 0)
    return NDBT_ProgramExit(NDBT_FAILED);
  return NDBT_ProgramExit(NDBT_OK);
}

#define MGMERR(h) \
  ndbout << "latest_error="<<ndb_mgm_get_latest_error(h) \
	 << ", line="<<ndb_mgm_get_latest_error_line(h) \
	 << endl;

NdbMgmHandle handle= NULL;

Vector<ndb_mgm_node_state> ndbNodes;

int 
getStatus(){
  int retries = 0;
  struct ndb_mgm_cluster_state * status;
  struct ndb_mgm_node_state * node;
  
  ndbNodes.clear();

  while(retries < 10){
    status = ndb_mgm_get_status(handle);
    if (status == NULL){
      ndbout << "status==NULL, retries="<<retries<<endl;
      MGMERR(handle);
      retries++;
      ndb_mgm_disconnect(handle);
      if (ndb_mgm_connect(handle,0,0,1)) {
        MGMERR(handle);
        g_err  << "Reconnect failed" << endl;
        break;
      }
      continue;
    }
    int count = status->no_of_nodes;
    for (int i = 0; i < count; i++){
      node = &status->node_states[i];      
      switch(node->node_type){
      case NDB_MGM_NODE_TYPE_NDB:
        if (!nowait_nodes_bitmask.get(node->node_id))
          ndbNodes.push_back(*node);
	break;
      case NDB_MGM_NODE_TYPE_MGM:
        /* Don't care about MGM nodes */
	break;
      case NDB_MGM_NODE_TYPE_API:
        /* Don't care about API nodes */
	break;
      default:
	if(node->node_status == NDB_MGM_NODE_STATUS_UNKNOWN ||
	   node->node_status == NDB_MGM_NODE_STATUS_NO_CONTACT){
	  retries++;
	  ndbNodes.clear();
	  free(status); 
	  status = NULL;
          count = 0;

	  ndbout << "kalle"<< endl;
	  break;
	}
	abort();
	break;
      }
    }
    if(status == 0){
      ndbout << "status == 0" << endl;
      continue;
    }
    free(status);
    return 0;
  }

  return -1;
}

char*
getTimeAsString(char* pStr)
{
  time_t now;
  now= ::time((time_t*)NULL);

  struct tm* tm_now;
#ifdef NDB_WIN32
  tm_now = localtime(&now);
#else
  tm_now = ::localtime(&now); //uses the "current" timezone
#endif

  BaseString::snprintf(pStr, 9,
	   "%02d:%02d:%02d",
	   tm_now->tm_hour,
	   tm_now->tm_min,
	   tm_now->tm_sec);

  return pStr;
}

static int
waitClusterStatus(const char* _addr,
		  ndb_mgm_node_status _status)
{
  int _startphase = -1;

#ifndef NDB_WIN
  /* Ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
#endif

  handle = ndb_mgm_create_handle();
  if (handle == NULL){
    g_err << "Could not create ndb_mgm handle" << endl;
    return -1;
  }
  g_info << "Connecting to mgmsrv at " << _addr << endl;
  if (ndb_mgm_set_connectstring(handle, _addr))
  {
    MGMERR(handle);
    g_err  << "Connectstring " << _addr << " invalid" << endl;
    return -1;
  }
  if (ndb_mgm_connect(handle,0,0,1)) {
    MGMERR(handle);
    g_err  << "Connection to " << _addr << " failed" << endl;
    return -1;
  }

  int attempts = 0;
  int resetAttempts = 0;
  const int MAX_RESET_ATTEMPTS = 10;
  bool allInState = false;

  Uint64 time_now = NdbTick_CurrentMillisecond();
  Uint64 timeout_time = time_now + 1000 * _timeout;

  while (allInState == false){
    if (_timeout > 0 && time_now > timeout_time){
      /**
       * Timeout has expired waiting for the nodes to enter
       * the state we want
       */
      bool waitMore = false;
      /**
       * Make special check if we are waiting for
       * cluster to become started
       */
      if(_status == NDB_MGM_NODE_STATUS_STARTED){
	waitMore = true;
	/**
	 * First check if any node is not starting
	 * then it's no idea to wait anymore
	 */
	for (size_t n = 0; n < ndbNodes.size(); n++){
	  if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED &&
	      ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTING)
	    waitMore = false;

	}
      }

      if (!waitMore || resetAttempts > MAX_RESET_ATTEMPTS){
	g_err << "waitNodeState("
	      << ndb_mgm_get_node_status_string(_status)
	      <<", "<<_startphase<<")"
	      << " timeout after " << attempts << " attempts" << endl;
	return -1;
      }

      g_err << "waitNodeState("
	    << ndb_mgm_get_node_status_string(_status)
	    <<", "<<_startphase<<")"
	    << " resetting timeout "
	    << resetAttempts << endl;

      timeout_time = time_now + 1000 * _timeout;

      resetAttempts++;
    }

    if (attempts > 0)
      NdbSleep_MilliSleep(100);
    if (getStatus() != 0){
      return -1;
    }

    /* Assume all nodes are in state(if there is any) */
    allInState = (ndbNodes.size() > 0);

    /* Loop through all nodes and check their state */
    for (size_t n = 0; n < ndbNodes.size(); n++) {
      ndb_mgm_node_state* ndbNode = &ndbNodes[n];

      assert(ndbNode != NULL);

      g_info << "Node " << ndbNode->node_id << ": "
	     << ndb_mgm_get_node_status_string(ndbNode->node_status)<< endl;

      if (ndbNode->node_status !=  _status)
	  allInState = false;
    }

    if (!allInState) {
      char time[9];
      g_info << "[" << getTimeAsString(time) << "] "
             << "Waiting for cluster enter state "
             << ndb_mgm_get_node_status_string(_status) << endl;
    }

    attempts++;
    
    time_now = NdbTick_CurrentMillisecond();
  }
  return 0;
}

template class Vector<ndb_mgm_node_state>;
