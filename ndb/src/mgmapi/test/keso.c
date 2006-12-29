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

#include <mgmapi.h>

#ifdef VM_TRACE
#include <mgmapi_debug.h>
#endif

#include <NdbOut.hpp>

static int testConnect(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testDisconnect(NdbMgmHandle h, struct ndb_mgm_reply* reply);

static int testStatus(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testGetConfig(NdbMgmHandle h, struct ndb_mgm_reply* reply);
#ifdef VM_TRACE
static int testLogSignals(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testStartSignalLog(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testStopSignalLog(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testSetTrace(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testInsertError(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testDumpState(NdbMgmHandle h, struct ndb_mgm_reply* reply);
#endif
static int testFilterClusterLog(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testSetLogLevelClusterLog(NdbMgmHandle h, 
				     struct ndb_mgm_reply* reply);
static int testSetLogLevelNode(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testRestartNode(NdbMgmHandle h, struct ndb_mgm_reply* reply);
static int testGetStatPort(NdbMgmHandle h, struct ndb_mgm_reply* reply);

typedef int (*FUNC)(NdbMgmHandle h, struct ndb_mgm_reply* reply);

struct test_case {
  char name[255];
  FUNC func;
};

struct test_case test_connect_disconnect[] = {
  {"testConnect", &testConnect},
  {"testDisconnect", &testDisconnect}
};

struct test_case tests[] = {
  { "testStatus",           &testStatus           },
  { "testFilterClusterLog", &testFilterClusterLog },
  /*{ "testSetLogLevelClusterLog", &testSetLogLevelClusterLog },*/
  /*{ "testSetLogLevelNode",  &testSetLogLevelNode  },*/
  { "testRestartNode",      &testRestartNode      },
  { "testGetStatPort",      &testGetStatPort      },
#ifdef VM_TRACE
  { "testLogSignals",       &testLogSignals       },
  { "testStartSignalLog",   &testStartSignalLog   },
  { "testStopSignalLog",    &testStopSignalLog    },
  { "testSetTrace",         &testSetTrace         },
  { "testDumpState",        &testDumpState        },
  { "testInsertError",      &testInsertError      }
#endif
};

static int no_of_tests = sizeof(tests) / sizeof(struct test_case);
static int testFailed = 0;

static const char * g_connect_string = "localhost:2200";

int
main(int argc, const char** argv){

  struct ndb_mgm_reply reply;
  int i = 0;
  NdbMgmHandle h = NULL;

  if(argc > 1)
    g_connect_string = argv[1];

  ndbout_c("Using connectstring: %s", g_connect_string);

  for (i = 0; i < 2; i++) {
    ndbout_c("-- %s --", test_connect_disconnect[i].name);
    if (test_connect_disconnect[i].func(h, &reply) == 0) {
      ndbout_c("-- Passed --");
    } else {
      testFailed++;
      ndbout_c("-- Failed --");
    }
  }
  ndbout_c("-- %d passed, %d failed --", (2 - testFailed),  testFailed);


  h = ndb_mgm_create_handle();
  ndb_mgm_connect(h, g_connect_string);
  
  for (i = 0; i < no_of_tests; i ++) {
    ndbout_c("-- %s --", tests[i].name);
    if (tests[i].func(h, &reply) == 0) {
      ndbout_c("-- Passed --");
    } else {
      testFailed++;
      ndbout_c("-- Failed --");
      ndb_mgm_disconnect(h);
      ndb_mgm_connect(h, g_connect_string);      
    }
  }
  ndbout_c("-- %d passed, %d failed --", (no_of_tests - testFailed),  
	   testFailed);
  
  ndb_mgm_disconnect(h);

  return 0;
}

static 
int testConnect(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  h = ndb_mgm_create_handle();
  if (h != NULL) {
    if (ndb_mgm_connect(h, g_connect_string) == -1) {
      ndbout_c(g_connect_string);
      /*ndbout_c("last_error: %d", h->last_error); */
      return -1;
    } else {
      ndbout_c("Connected to localhost:37123");
    }
    
  } else {
    ndbout_c("Unable to create a NdbMgmHandle...");
    return -1;
  }

  return 0;
}

static
int testDisconnect(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  ndb_mgm_disconnect(h); 
  
  return 0;
}

static 
int testGetConfig(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int i = 0;
  struct ndb_mgm_configuration * config =  ndb_mgm_get_configuration(h, 0);
  if (config != NULL) {
    free(config);
  } else {
    ndbout_c("Unable to get config");
    return -1;
  }
  return 0;
}

static 
int testStatus(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int i = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    ndbout_c("Number of nodes: %d", cluster->no_of_nodes);
    for (i = 0; i < cluster->no_of_nodes; i++) {
      struct ndb_mgm_node_state state = cluster->node_states[i];
      ndbout_c("NodeId: %d (%s)-- %s", state.node_id,
	       ndb_mgm_get_node_type_string(state.node_type),
	       ndb_mgm_get_node_status_string(state.node_status));
	      
    }     
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }
  return 0;
}


#ifdef VM_TRACE

static 
int testLogSignals(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }
      
  rc = ndb_mgm_log_signals(h, nodeId, NDB_MGM_SIGNAL_LOG_MODE_INOUT, 
			   "CMVMI QMGR", 
			   reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;
}

static 
int testStartSignalLog(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }

  rc = ndb_mgm_start_signallog(h, nodeId, reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;
}

static 
int testStopSignalLog(NdbMgmHandle h, struct ndb_mgm_reply* reply) {

  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }

  rc = ndb_mgm_stop_signallog(h, nodeId, reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;

}

static 
int testSetTrace(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }
  
  rc = ndb_mgm_set_trace(h, nodeId, 2, reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;

}

static 
int testInsertError(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }
  
  rc = ndb_mgm_insert_error(h, nodeId, 9999, reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;

}

static 
int testDumpState(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  
  int rc = 0;
  int nodeId = 0;
  int dump[3];

  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }
  
  dump[0] = 1;
  dump[1] = 2;
  dump[2] = 3;
  rc = ndb_mgm_dump_state(h, nodeId, dump, 3, reply);

  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;
}

#endif

static 
int testFilterClusterLog(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int rc = 0;
  
  rc = ndb_mgm_filter_clusterlog(h, NDB_MGM_CLUSTERLOG_INFO, reply);
  if (rc == -1) {
    ndbout_c("rc = %d", reply->return_code);
    ndbout_c("%s", reply->message);   
    return -1;
  } 

  ndbout_c("%s", reply->message); 

  rc = ndb_mgm_filter_clusterlog(h, NDB_MGM_CLUSTERLOG_DEBUG, reply);
  if (rc == -1) {
    ndbout_c("rc = %d", reply->return_code);
    ndbout_c("%s", reply->message);   
    return -1;
  } 

  ndbout_c("%s", reply->message); 

  return rc;

}

static 
int testSetLogLevelClusterLog(NdbMgmHandle h, 
			      struct ndb_mgm_reply* reply) {
  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }

  rc = ndb_mgm_set_loglevel_clusterlog(h, nodeId,
				       NDB_MGM_EVENT_CATEGORY_CHECKPOINT,
				       5,
				       reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;

}

static 
int testSetLogLevelNode(NdbMgmHandle h, struct ndb_mgm_reply* reply) {

  int rc = 0;
  int nodeId = 0;
  struct ndb_mgm_cluster_state* cluster =  ndb_mgm_get_status(h);
  if (cluster != NULL) {
    if (cluster->no_of_nodes != 0) {
      nodeId = cluster->node_states[0].node_id;
    }
    free(cluster);
  } else {
    ndbout_c("Unable to get node status.");
    return -1;
  }

  rc = ndb_mgm_set_loglevel_node(h, nodeId,
				 NDB_MGM_EVENT_CATEGORY_STATISTIC,
				 15,
				 reply);
  if (rc != 0) {
    ndbout_c("rc = %d", reply->return_code);
  } 

  ndbout_c("%s", reply->message); 

  return rc;

}

static int 
testRestartNode(NdbMgmHandle h, struct ndb_mgm_reply* reply) {
  int restarts = 0;

  restarts = ndb_mgm_restart(h, 0, 0); /* Restart all */
  if (restarts == 0) {
    ndbout_c("No nodes restarted...");
    return -1;
  } else {
    ndbout_c("%d nodes restarted...", restarts);
  }
  
  return 0;
}

static 
int testGetStatPort(NdbMgmHandle h, struct ndb_mgm_reply* reply) {

  int rc = 0;
  rc = ndb_mgm_get_stat_port(h, reply);
  if (rc == 0) {
    ndbout_c("stat port %s", reply->message);
  } else {
    ndbout_c("failed");
  }

  return rc;
}
