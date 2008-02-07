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


#include <ndb_global.h>
#include <ndb_opts.h>

#include <mgmapi.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <NDBT.hpp>

static int
waitClusterStatus(const char* _addr, ndb_mgm_node_status _status);

enum ndb_waiter_options {
  OPT_WAIT_STATUS_NOT_STARTED = NDB_STD_OPTIONS_LAST,
  OPT_WAIT_STATUS_SINGLE_USER
};
NDB_STD_OPTS_VARS;

static int _no_contact = 0;
static int _not_started = 0;
static int _single_user = 0;
static int _timeout = 120;

const char *load_default_groups[]= { "mysql_cluster",0 };

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "no-contact", 'n', "Wait for cluster no contact",
    (uchar**) &_no_contact, (uchar**) &_no_contact, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "not-started", OPT_WAIT_STATUS_NOT_STARTED, "Wait for cluster not started",
    (uchar**) &_not_started, (uchar**) &_not_started, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "single-user", OPT_WAIT_STATUS_SINGLE_USER,
    "Wait for cluster to enter single user mode",
    (uchar**) &_single_user, (uchar**) &_single_user, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "timeout", 't', "Timeout to wait in seconds",
    (uchar**) &_timeout, (uchar**) &_timeout, 0,
    GET_INT, REQUIRED_ARG, 120, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage()
{
  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  load_defaults("my",load_default_groups,&argc,&argv);
  const char* _hostName = NULL;

#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_waiter.trace";
#endif

  if (handle_options(&argc, &argv, my_long_options,
                     ndb_std_get_one_option))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  _hostName = argv[0];

  if (_hostName == 0)
    _hostName= opt_connect_str;

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

  if (waitClusterStatus(_hostName, wait_status) != 0)
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

static int
waitClusterStatus(const char* _addr,
		  ndb_mgm_node_status _status)
{
  int _startphase = -1;

  /* Ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);

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
  int timeout_ms= _timeout * 10; /* In number of 100 milliseconds */
  while (allInState == false){
    if (_timeout > 0 && attempts > _timeout){
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
	      << " timeout after " << attempts <<" attemps" << endl;
	return -1;
      }

      g_err << "waitNodeState("
	    << ndb_mgm_get_node_status_string(_status)
	    <<", "<<_startphase<<")"
	    << " resetting number of attempts "
	    << resetAttempts << endl;
      attempts = 0;
      resetAttempts++;
    }

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
      g_info << "Waiting for cluster enter state "
             << ndb_mgm_get_node_status_string(_status)<< endl;
      NdbSleep_MilliSleep(100);
    }

    attempts++;
  }
  return 0;
}

template class Vector<ndb_mgm_node_state>;
