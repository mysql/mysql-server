/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
#include <kernel/ndb_limits.h>
#include "../include/mgmcommon/LocalConfig.hpp"

#include <NDBT.hpp>

int 
waitClusterStatus(const char* _addr, ndb_mgm_node_status _status, unsigned int _timeout);

static const char* opt_connect_str= 0;
static int _no_contact = 0;
static int _timeout = 120;
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "no-contact", 'n', "Wait for cluster no contact",
    (gptr*) &_no_contact, (gptr*) &_no_contact, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "timeout", 't', "Timeout to wait",
    (gptr*) &_timeout, (gptr*) &_timeout, 0,
    GET_INT, REQUIRED_ARG, 120, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void print_version()
{
  printf("MySQL distrib %s, for %s (%s)\n",MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
  ndbPrintVersion();
}
static void usage()
{
  print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:O,/tmp/ndb_drop_table.trace");
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "ndb_tools",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  const char* _hostName = NULL;
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  char buf[255];
  _hostName = argv[0];

  if (_hostName == NULL){
    LocalConfig lcfg;
    if(!lcfg.init())
    {
      lcfg.printError();
      lcfg.printUsage();
      g_err  << "Error parsing local config file" << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    for (unsigned i = 0; i<lcfg.ids.size();i++)
    {
      MgmtSrvrId * m = &lcfg.ids[i];
      
      switch(m->type){
      case MgmId_TCP:
	snprintf(buf, 255, "%s:%d", m->name.c_str(), m->port);
	_hostName = buf;
	break;
      case MgmId_File:
	break;
      default:
	break;
      }
      if (_hostName != NULL)
	break;
    }
    if (_hostName == NULL)
    {
      g_err << "No management servers configured in local config file" << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
  }

  if (_no_contact) {
    if (waitClusterStatus(_hostName, NDB_MGM_NODE_STATUS_NO_CONTACT, _timeout) != 0)
      return NDBT_ProgramExit(NDBT_FAILED);
  } else if (waitClusterStatus(_hostName, NDB_MGM_NODE_STATUS_STARTED, _timeout) != 0)
    return NDBT_ProgramExit(NDBT_FAILED);

  return NDBT_ProgramExit(NDBT_OK);
}

#define MGMERR(h) \
  ndbout << "latest_error="<<ndb_mgm_get_latest_error(h) \
	 << ", line="<<ndb_mgm_get_latest_error_line(h) \
	 << endl;

NdbMgmHandle handle= NULL;

Vector<ndb_mgm_node_state> ndbNodes;
Vector<ndb_mgm_node_state> mgmNodes;
Vector<ndb_mgm_node_state> apiNodes;

int 
getStatus(){
  int retries = 0;
  struct ndb_mgm_cluster_state * status;
  struct ndb_mgm_node_state * node;
  
  ndbNodes.clear();
  mgmNodes.clear();
  apiNodes.clear();

  while(retries < 10){
    status = ndb_mgm_get_status(handle);
    if (status == NULL){
      ndbout << "status==NULL, retries="<<retries<<endl;
      MGMERR(handle);
      retries++;
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
	mgmNodes.push_back(*node);
	break;
      case NDB_MGM_NODE_TYPE_API:
	apiNodes.push_back(*node);
	break;
      default:
	if(node->node_status == NDB_MGM_NODE_STATUS_UNKNOWN ||
	   node->node_status == NDB_MGM_NODE_STATUS_NO_CONTACT){
	  retries++;
	  ndbNodes.clear();
	  mgmNodes.clear();
	  apiNodes.clear();
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
   
  g_err  << "getStatus failed" << endl;
  return -1;
}

int 
waitClusterStatus(const char* _addr,
		  ndb_mgm_node_status _status,
		  unsigned int _timeout)
{
  int _startphase = -1;

  int _nodes[MAX_NDB_NODES];
  int _num_nodes = 0;

  handle = ndb_mgm_create_handle();   
  if (handle == NULL){
    g_err << "handle == NULL" << endl;
    return -1;
  }
  g_info << "Connecting to mgmsrv at " << _addr << endl;
  if (ndb_mgm_connect(handle, _addr) == -1) {
    MGMERR(handle);
    g_err  << "Connection to " << _addr << " failed" << endl;
    return -1;
  }

  if (getStatus() != 0)
    return -1;
  
  // Collect all nodes into nodes
  for (size_t i = 0; i < ndbNodes.size(); i++){
    _nodes[i] = ndbNodes[i].node_id;
    _num_nodes++;
  }

  unsigned int attempts = 0;
  unsigned int resetAttempts = 0;
  const unsigned int MAX_RESET_ATTEMPTS = 10;
  bool allInState = false;    
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

    allInState = true;
    if (getStatus() != 0){
      g_err << "getStatus != 0" << endl;
      return -1;
    }

    // ndbout << "waitNodeState; _num_nodes = " << _num_nodes << endl;
    // for (int i = 0; i < _num_nodes; i++)
    //   ndbout << " node["<<i<<"] =" <<_nodes[i] << endl;

    for (int i = 0; i < _num_nodes; i++){
      ndb_mgm_node_state* ndbNode = NULL;
      for (size_t n = 0; n < ndbNodes.size(); n++){
	if (ndbNodes[n].node_id == _nodes[i])
	  ndbNode = &ndbNodes[n];
      }

      if(ndbNode == NULL){
	allInState = false;
	continue;
      }

      g_info << "State node " << ndbNode->node_id << " "
	     << ndb_mgm_get_node_status_string(ndbNode->node_status)<< endl;

      assert(ndbNode != NULL);

      if(_status == NDB_MGM_NODE_STATUS_STARTING && 
	 ((ndbNode->node_status == NDB_MGM_NODE_STATUS_STARTING && 
	   ndbNode->start_phase >= _startphase) ||
	  (ndbNode->node_status == NDB_MGM_NODE_STATUS_STARTED)))
	continue;

      if (_status == NDB_MGM_NODE_STATUS_STARTING){
	g_info << "status = "  
	       << ndb_mgm_get_node_status_string(ndbNode->node_status)
	       <<", start_phase="<<ndbNode->start_phase<<endl;
	if (ndbNode->node_status !=  _status) {
	  if (ndbNode->node_status < _status)
	    allInState = false;
	  else 
	    g_info << "node_status(" << (unsigned)ndbNode->node_status
		   << ") != _status("<< (unsigned)_status << ")" <<endl;
	} else if (ndbNode->start_phase < _startphase)
	  allInState = false;
      } else {
	if (ndbNode->node_status !=  _status) 
	  allInState = false;
      }
    }
    g_info << "Waiting for cluster enter state " 
	    << ndb_mgm_get_node_status_string(_status)<< endl;
    NdbSleep_SecSleep(1);
    attempts++;
  }
  return 0;
}

template class Vector<ndb_mgm_node_state>;
