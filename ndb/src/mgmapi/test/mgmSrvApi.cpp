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

/****************************************************
 *  Name: 
 *       mgmSrvApi.cpp
 *
 *  Description:
 *      Test the bahaviour of the Management Server 
 *      API based on the tests specified in the 
 *      "Test Specification for the Management 
 *       Server API" document
 *  
 *****************************************************/
#include <ndb_global.h>
#include "mgmapi.h"
#include "mgmapi_commands.h"
#include <NdbMain.h>
#include <NdbOut.hpp>

/**
 * The pupose of this test program is to 
 * verify that the Management Server
 * API is functioning properly, i.e. a handle 
 * can be created/destroyed properly, the 
 * connection to the NDB nodes is established 
 * correctly, and all the errors are handled in 
 * a proper way.
 * USE: mgmSrvApi -n -i 
 *
 * @param       n     Number of nodes to crash
 *
 **/
NDB_COMMAND(mgmSrvApi, "mgmSrvApi", "mgmSrvApi -n <Number of nodes to crash> -i <Node ID to be crashed> -p <IP address:port number>", "Management Server API testing", 65535){

  const char *Addr = 0;
  int i;
  int nodesNo = 0; // Number of nodes to crash
  int ndbID = 0;  // Node ID to be crashed 

  i = 1;
  while (argc > 1) {
    if (strcmp(argv[i], "-n") == 0) 
      nodesNo = atoi(argv[i+1]);
 
    if (strcmp(argv[i], "-i") == 0) 
      ndbID = atoi(argv[i+1]);

    if (strcmp(argv[i], "-p") == 0)
       Addr = argv[i+1];
    
    argc -= 1;
    i = i + 1; 
  }

  /*  
   * Create a handle
   */
  ndbout << "Creating handle..." << endl;
  NdbMgmHandle h = ndb_mgm_create_handle();   
 
  /*  
   * Perfom the connection
   */
  ndbout << "Connecting..." << endl;
  if (ndb_mgm_connect(h, Addr) == -1) {
    ndbout << "Connection to " << Addr << " failed" << endl;
    exit(-1);
  }

  /*  
   * Get status of a node
   */
  ndbout << "Getting status..." << endl;
  
  struct ndb_mgm_cluster_state * status;
  struct ndb_mgm_node_state * nodes;

  status = ndb_mgm_get_status(h);
  nodes = status->node_states;
  if (nodes->node_status == 1) {
    ndbout << "No contact established" << endl;
    //  exit(-1);
  }
    
  /*  
   * Stop the NDB nodes
   */
  ndbout << "Stopping the node(s)" << endl;
  const int * list;

  if (nodesNo == 0) // all nodes stopped by definition
    ndbID = 0;

  list = &ndbID;
  if (ndb_mgm_stop(h, nodesNo, list) != 1) {
    ndbout << nodesNo << " NDB node(s) not stopped " << endl;
  }
 
  /*  
   * Disconnect from the management server
   */
  ndbout << "Disconnecting..." << endl;
  ndb_mgm_disconnect(h);

  /*  
   * Destroy the handle
   */
  ndbout << "Destroying the handle..." << endl;
  ndb_mgm_destroy_handle(&h);

  return 0;
}
