/*
 Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include <my_config.h>
#include <unistd.h>

#include "all_tests.h"


int run_pool_test(QueryPlan *plan, Ndb *db1, int v) {
  int conn_retries = 0;
  int r;
  Ndb_cluster_connection &main_conn = db1->get_ndb_cluster_connection();  

  /* The pooled connection */  
  Ndb_cluster_connection * nc = new Ndb_cluster_connection(connect_string, & main_conn);
  
  /* Set name that appears in the cluster log file */
  nc->set_name("memcached.pool");

  detail(v, "#1 node_id: %d\n", nc->node_id());

  while(conn_retries < 5) {
    r = nc->connect(1,1,0);
    if(r == 0)         // success 
      break;
    else if(r == -1)   // unrecoverable error
      break;
    else if (r == 1 && conn_retries < 5) { // recoverable error
      sleep(1);
      conn_retries++;
    }
  }

  detail(v, "connect() returns %d\n", r);
  require(r >= 0);
  
  detail(v, "#2 node_id: %d\n", nc->node_id());
  
  if(nc->node_id() == 0) {
    detail(v, "starting connect thread\n");
    nc->start_connect_thread();
  }
  else {
    detail(v, "not starting connect thread\n");
  }

  detail(v, "#3 node_id: %d \n", nc->node_id());
    
  int ready_nodes = nc->wait_until_ready(2, 2);
  detail(v, "wait_until_ready(): %d \n",  ready_nodes);
  require(ready_nodes > 0);

  detail(v, "#4 node_id: %d \n", nc->node_id());
  
  // while(nc->node_id() == 0 && retries++ < 5) sleep(1);

  detail(v,  "Node %d connected to %s:%d\n",
         nc->node_id(), nc->get_connected_host(), nc->get_connected_port());  
  require(nc->node_id());

  Ndb *db2 = new Ndb(nc);
  require(db2);
  detail(v, "Created an Ndb object.\n");
  
  db2->init(4);

  NdbTransaction *tx = db2->startTransaction();
  require(tx);
  detail(v, "Started a transaction.\n");
  
  tx->close();  

  pass;
}

