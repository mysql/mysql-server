/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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

"use strict";

var stats = {
  "wait_until_ready_timeouts" : 0 ,
  "node_ids"                  : [],
  "connections"               : { "successful" : 0,
                                  "failed"     : 0,
                                  "closed"     : 0
                                },
  "connect"                   : { "join"    : 0,
                                  "connect" : 0,
                                  "queued"  : 0
                                },
  "simultaneous_disconnects"  : 0  // this should always be zero
};

var conf             = require("./path_config"),
    jones            = require("database-jones"),
    adapter          = require(conf.binary),
    udebug           = unified_debug.getLogger("NdbConnection.js"),
    stats_module     = require(jones.api.stats),
    QueuedAsyncCall  = require(jones.common.QueuedAsyncCall).QueuedAsyncCall,
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


logReadyNodes = function(ndb_cluster_connection, nnodes) {
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
};


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

  if(! this.asyncNdbContext) {
    this.asyncNdbContext = new AsyncNdbContext(this.ndb_cluster_connection);
  }

  return this.asyncNdbContext;
};


NdbConnection.prototype.close = function(userCallback) {
  var self = this;
  var nodeId = self.ndb_cluster_connection.node_id();
  var apiCall;

  function disconnect() {
    stats.connections.closed++;
    if(self.asyncNdbContext) {
      self.asyncNdbContext["delete"]();  // C++ Destructor
      self.asyncNdbContext = null;    
    }
    udebug.log_notice("Node", nodeId, "disconnecting.");

    self.isConnected = false;
    if(self.ndb_cluster_connection) {
      apiCall = new QueuedAsyncCall(self.execQueue, userCallback);
      apiCall.description = "DeleteNdbClusterConnection";
      apiCall.ndb_cluster_connection = self.ndb_cluster_connection;
      apiCall.run = function() {
        this.ndb_cluster_connection["delete"](this.callback);
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
