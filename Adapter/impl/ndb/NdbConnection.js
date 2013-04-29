/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

var adapter          = require(path.join(build_dir, "ndb_adapter.node")),
    udebug           = unified_debug.getLogger("NdbConnection.js"),
    stats_module     = require(path.join(api_dir,"stats.js")),
    stats            = stats_module.getWriter(["spi","ndb","NdbConnection"]),
    logReadyNodes;


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
  this.ndb_cluster_connection.set_name("nodejs");
}


function logReadyNodes(ndb_cluster_connection, nnodes) {
  var node_id;
  if(nnodes < 0) {
    stats.incr( [ "wait_until_ready","timeouts" ] );
  }
  else {
    node_id = ndb_cluster_connection.node_id();
    if(nnodes > 0) {
      udebug.log_notice("Warning: only", nnodes, "data nodes are running.");
    }
    udebug.log_notice("Connected to cluster as node id:", node_id);
    stats.push( [ "node_ids" ] , node_id);
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
    udebug.log("connect() onConnected rval =", rval);
    if(rval === 0) {
      stats.incr( [ "connections","successful" ]);
      self.ndb_cluster_connection.wait_until_ready(1, 1, onReady);
    }
    else {
      stats.incr( [ "connections","failed" ]);
      runCallbacks('NDB Connect failed ' + rval, self);
    }
  }
  
  /* connect() starts here */
  if(this.isConnected) {
    stats.incr( [ "connect", "join" ] );
    callback(null, this);
  }
  else {
    this.pendingConnections.push(callback);
    if(this.pendingConnections.length === 1) {
      stats.incr( [ "connect", "async" ] );
      this.ndb_cluster_connection.connect(
        properties.ndb_connect_retries, properties.ndb_connect_delay,
        properties.ndb_connect_verbose, onConnected);
    }
    else {
      stats.incr( [ "connect","queued" ] );
    }
  }
};


NdbConnection.prototype.connectSync = function(properties) {
  if(this.isConnected) {
    stats.incr( [ "connect", "join" ] );
    return true;
  }

  stats.incr( [ "connect", "sync" ] );
  var r, nnodes;
  r = this.ndb_cluster_connection.connect(properties.ndb_connect_retries,
                                          properties.ndb_connect_delay,
                                          properties.ndb_connect_verbose);
  if(r === 0) {
    stats.incr( [ "connections","successful" ]);
    nnodes = this.ndb_cluster_connection.wait_until_ready(1, 1);
    logReadyNodes(this.ndb_cluster_connection, nnodes);
    if(nnodes >= 0) {
      this.isConnected = true;
    }
  }
  
  return this.isConnected;
};


NdbConnection.prototype.getAsyncContext = function() {
  var AsyncNdbContext = adapter.ndb.impl.AsyncNdbContext;
  if(! this.asyncNdbContext) {
    this.asyncNdbContext = new AsyncNdbContext(this.ndb_cluster_connection);
  }
  return this.asyncNdbContext;
};


NdbConnection.prototype.close = function(userCallback) {
  var self = this;

  function disconnect() {
    if(self.asyncNdbContext) { 
      self.asyncNdbContext.delete();  // C++ Destructor
      self.asyncNdbContext = null;    
    }
    udebug.log("Disconnecting cluster");
    if(self.ndb_cluster_connection) {
      self.ndb_cluster_connection.delete();  // C++ Destructor
      self.ndb_cluster_connection = null;
    }
    self.isConnected = false;
    if(typeof userCallback === 'function') {
      userCallback();
    }
  }

  /* close() starts here */
  if(! this.isConnected) {
    disconnect();  /* Free Resources anyway */
  }
  else if(this.isDisconnecting) { 
    stats.incr("very_strange_simultaneous_disconnects");
  }
  else { 
    this.isDisconnecting = true;

    /* This sends a "shutdown" message to the async listener thread */
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
