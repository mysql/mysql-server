/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include <ndb_global.h>
#include <ndb_opts.h>
#include <time.h>

#include <mgmapi.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <portlib/ndb_localtime.h>

#include <NdbToolsProgramExitCodes.hpp>

#include <kernel/NodeBitmask.hpp>

#include "my_alloc.h"

static int
waitClusterStatus(const char* _addr, ndb_mgm_node_status _status);

static int _no_contact = 0;
static int _not_started = 0;
static int _single_user = 0;
static int _timeout = 120; // Seconds
static const char* _wait_nodes = 0;
static const char* _nowait_nodes = 0;
static NdbNodeBitmask nowait_nodes_bitmask;

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "no-contact", 'n', "Wait for cluster no contact",
    &_no_contact, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "not-started", NDB_OPT_NOSHORT, "Wait for cluster not started",
    &_not_started, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "single-user", NDB_OPT_NOSHORT,
    "Wait for cluster to enter single user mode",
    &_single_user, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "timeout", 't', "Timeout to wait in seconds",
    &_timeout, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    120, 0, 0, nullptr, 0, nullptr },
  { "wait-nodes", 'w', "Node ids to wait on, e.g. '1,2-4'",
    &_wait_nodes, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "nowait-nodes", NDB_OPT_NOSHORT,
    "Nodes that will not be waited for, e.g. '2,3,4-7'",
    &_nowait_nodes, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options
};

extern "C"
void catch_signal(int signum)
{
}

#include "../src/common/util/parse_mask.hpp"

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);

#ifndef NDEBUG
  opt_debug= "d:t:O,/tmp/ndb_waiter.trace";
#endif

#ifndef _WIN32
  // Catching signal to allow testing of EINTR safeness
  // with "while killall -USR1 ndbwaiter; do true; done"
  signal(SIGUSR1, catch_signal);
#endif

  if (opts.handle_options())
    return NdbToolsProgramExitCode::WRONG_ARGS;

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
    return NdbToolsProgramExitCode::FAILED;

  return NdbToolsProgramExitCode::OK;
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
      if (ndb_mgm_connect(handle, opt_connect_retries - 1, opt_connect_retry_delay, 1)) {
        MGMERR(handle);
        ndberr  << "Reconnect failed" << endl;
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

static
char*
getTimeAsString(char* pStr, size_t len)
{
  // Get current time
  time_t now;
  time(&now);

  // Convert to local timezone
  tm tm_buf;
  ndb_localtime_r(&now, &tm_buf);

  // Print to string buffer
  BaseString::snprintf(pStr, len,
                       "%02d:%02d:%02d",
                       tm_buf.tm_hour,
                       tm_buf.tm_min,
                       tm_buf.tm_sec);
  return pStr;
}

static int
waitClusterStatus(const char* _addr,
		  ndb_mgm_node_status _status)
{
  int _startphase = -1;

#ifndef _WIN32
  /* Ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
#endif

  handle = ndb_mgm_create_handle();
  if (handle == NULL){
    ndberr << "Could not create ndb_mgm handle" << endl;
    return -1;
  }

  if (ndb_mgm_set_connectstring(handle, _addr))
  {
    MGMERR(handle);
    if (_addr != nullptr)
    {
      ndberr << "Connectstring " << _addr << " is invalid" << endl;
    }
    else
    {
      ndberr << "Connectstring is invalid" << endl;
    }
    return -1;
  }
  char buf[1024];
  ndbout << "Connecting to management server at "
         << ndb_mgm_get_connectstring(handle, buf, sizeof(buf)) << endl;
  if (ndb_mgm_connect(handle, opt_connect_retries - 1, opt_connect_retry_delay, 1)) {
    MGMERR(handle);
    ndberr << "Connection to "
           << ndb_mgm_get_connectstring(handle, buf, sizeof(buf)) << " failed"
           << endl;
    return -1;
  }

  int attempts = 0;
  int resetAttempts = 0;
  const int MAX_RESET_ATTEMPTS = 10;
  bool allInState = false;

  NDB_TICKS start = NdbTick_getCurrentTicks();
  NDB_TICKS now = start;

  while (allInState == false){
    if (_timeout > 0 &&
        NdbTick_Elapsed(start,now).seconds() > (Uint64)_timeout){
      /**
       * Timeout has expired waiting for the nodes to enter
       * the state we want
       */
      bool waitMore = false;
      /**
       * Make special check if we are waiting for
       * cluster to become started
       */
      if(_status == NDB_MGM_NODE_STATUS_STARTED)
      {
        waitMore = true;
        /**
         * First check if any node is not starting
         * then it's no idea to wait anymore
         */
        for (unsigned n = 0; n < ndbNodes.size(); n++)
        {
          if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED &&
              ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTING)
          {
            waitMore = false;
            break;
          }
        }
      }

      if (!waitMore || resetAttempts > MAX_RESET_ATTEMPTS){
	ndberr << "waitNodeState("
	      << ndb_mgm_get_node_status_string(_status)
	      <<", "<<_startphase<<")"
	      << " timeout after " << attempts << " attempts" << endl;
	return -1;
      }

      ndberr << "waitNodeState("
	    << ndb_mgm_get_node_status_string(_status)
	    <<", "<<_startphase<<")"
	    << " resetting timeout "
	    << resetAttempts << endl;

      start = now;

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
    for (unsigned n = 0; n < ndbNodes.size(); n++) {
      ndb_mgm_node_state* ndbNode = &ndbNodes[n];

      require(ndbNode != NULL);

      ndbout << "Node " << ndbNode->node_id << ": "
	     << ndb_mgm_get_node_status_string(ndbNode->node_status)<< endl;

      if (ndbNode->node_status !=  _status)
	  allInState = false;
    }

    if (!allInState) {
      char timestamp[9];
      ndbout << "[" << getTimeAsString(timestamp, sizeof(timestamp)) << "] "
             << "Waiting for cluster enter state "
             << ndb_mgm_get_node_status_string(_status) << endl;
    }

    attempts++;
    
    now = NdbTick_getCurrentTicks();
  }
  return 0;
}

template class Vector<ndb_mgm_node_state>;
