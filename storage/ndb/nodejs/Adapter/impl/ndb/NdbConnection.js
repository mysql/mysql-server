/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

"use strict";

var stats = {
	"wait_until_ready_timeouts" : 0 ,
  "node_ids"                  : [],
  "connections"               : { "successful" : 0, "failed" : 0 },
  "connect"                   : { "join" : 0, "connect" : 0, "queued" : 0 },
	"simultaneous_disconnects"  : 0  // this should always be zero
};

var path             = require("path"),
    adapter          = require(path.join(mynode.fs.build_dir, "ndb_adapter.node")),
    udebug           = unified_debug.getLogger("NdbConnection.js"),
    stats_module     = require(mynode.api.stats),
    QueuedAsyncCall  = require("../common/QueuedAsyncCall.js").QueuedAsyncCall,
    logReadyNodes;

stats_module.register(stats, "spi","ndb","NdbConnection");

/* NdbConnection represents a single connection to MySQL Cluster.
   This connection may be shared by multiple DBConnectionPool objects 
   (for instance with different connection properties, or from different
   application modules).  Over in NdbConnectionPool.js, a table manages
   a single back-end NdbConnection per unique NDB connect string, and 
   maintains the referenceCount stored here.
*/

function NdbConnection(connectString) {
  var Ndb_cluster_connection   = adapter.ndb.ndbapi.Ndb_cluster_connection;
  this.ndb_cluster_connection  = new Ndb_cluster_connection(connectString);
  this.referenceCount          = 1;
  this.asyncNdbContext         = null;
  this.pendingConnections      = [];
  this.isConnected             = false;
  this.isDisconnecting         = false;
  this.execQueue               = [];
  this.ndb_cluster_connection.set_name("nodejs");
}


function logReadyNodes(ndb_cluster_connection, nnodes) {
  var node_id;
  if(nnodes < 0) {
    stats.wait_until_ready_timeouts++;
  }
  else {
    node_id = ndb_cluster_connection.node_id();
    if(nnodes > 0) {
      udebug.log_notice("Warning: only", nnodes, "data nodes are running.");
    }
    udebug.log_notice("Connected to cluster as node id:", node_id);
    stats.node_ids.push(node_id);
  }
  return nnodes;
}


NdbConnection.prototype.connect = function(properties, callback) {
  var self = this;

  function runCallbacks(a, b) {
    var i;
    for(i = 0 ; i < self.pendingConnections.length ; i++) {
      self.pendingConnections[i](a, b);
    }  
  }

  function onReady(cb_err, nnodes) {
    logReadyNodes(self.ndb_cluster_connection, nnodes);
    if(nnodes < 0) {
      runCallbacks("Timeout waiting for cluster to become ready.", self);
    }
    else {
      self.isConnected = true;
      runCallbacks(null, self);
    }
  }

  function onConnected(cb_err, rval) {
    var err;
    udebug.log("connect() onConnected rval =", rval);
    if(rval === 0) {
      stats.connections.successful++;
      self.ndb_cluster_connection.wait_until_ready(1, 1, onReady);
    }
    else {
      stats.connections.failed++;
      err = new Error(self.ndb_cluster_connection.get_latest_error_msg());
      err.sqlstate = "08000";
      runCallbacks(err, self);
    }
  }
  
  /* connect() starts here */
  if(this.isConnected) {
    stats.connect.join++;
    callback(null, this);
  }
  else {
    this.pendingConnections.push(callback);
    if(this.pendingConnections.length === 1) {
      stats.connect.connect++;
      this.ndb_cluster_connection.connect(
        properties.ndb_connect_retries, properties.ndb_connect_delay,
        properties.ndb_connect_verbose, onConnected);
    }
    else {
      stats.connect.queued++;
    }
  }
};


NdbConnection.prototype.getAsyncContext = function() {
  var AsyncNdbContext = adapter.ndb.impl.AsyncNdbContext;

  if(adapter.ndb.impl.MULTIWAIT_ENABLED) {
    if(! this.asyncNdbContext) {
      this.asyncNdbContext = new AsyncNdbContext(this.ndb_cluster_connection);
    }
  }
  else if(this.asyncNdbContext == null) {
    udebug.log_notice("NDB Async API support is disabled at build-time for " +
                      "MySQL Cluster 7.3.1 - 7.3.2.  Async API will not be used."
                     );
    this.asyncNdbContext = false;
  }
  return this.asyncNdbContext;
};


NdbConnection.prototype.close = function(userCallback) {
  var self = this;
  var nodeId = self.ndb_cluster_connection.node_id();
  var apiCall;

  function disconnect() {
    if(self.asyncNdbContext) { 
      self.asyncNdbContext.delete();  // C++ Destructor
      self.asyncNdbContext = null;    
    }
    udebug.log_notice("Node", nodeId, "disconnecting.");

    self.isConnected = false;
    if(self.ndb_cluster_connection) {
      apiCall = new QueuedAsyncCall(self.execQueue, userCallback);
      apiCall.description = "DeleteNdbClusterConnection";
      apiCall.ndb_cluster_connection = self.ndb_cluster_connection;
      apiCall.run = function() {
        this.ndb_cluster_connection.delete(this.callback);
      };
      apiCall.enqueue();
    }
  }

  /* close() starts here */
  if(! this.isConnected) {
    disconnect();  /* Free Resources anyway */
  }
  else if(this.isDisconnecting) { 
    stats.simultaneous_disconnects++;  // a very unusual situation
  }
  else { 
    this.isDisconnecting = true;

    /* Start by sending a "shutdown" message to the async listener thread */
    if(this.asyncNdbContext) { this.asyncNdbContext.shutdown(); }
    
    /* The AsyncNdbContext destructor is synchronous, in that it calls
       pthread_join() on the listener thread.  Nonetheless we want to 
       sleep here hoping the final async results will make their way 
       back to JavaScript.
    */
    setTimeout(disconnect, 100);  // milliseconds
  }
};

module.exports = NdbConnection;
