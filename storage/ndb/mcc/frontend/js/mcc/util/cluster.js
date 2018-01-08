/*
Copyright (c) 2012, 2017 Oracle and/or its affiliates. All rights reserved.

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

/******************************************************************************
 ***                                                                        ***
 ***                          Cluster utilities                             ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.cluster
 *
 *  Description:
 *      Various cluster related utilities
 *
 *  External interface: 
 *      mcc.util.cluster.getColleagueNodes: Get ids of nodes of same type
 *      mcc.util.cluster.getNodeDistribution: Get #nodes of various types
 *      mcc.util.cluster.checkValidNodeId: Check if the node id is valid
 *      mcc.util.cluster.getNextNodeId: Get next available id in a given range
 *      mcc.util.cluster.setConfigFile: Set the name of configuration user selected.
 *      mcc.util.cluster.setConfigFileContents: Set contents of configuration file to variable
 *      mcc.util.cluster.getConfigFileContents: Retrieve contents of configuration file from variable
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
 *
 *  Internal data: 
 *      configFile: The name of the configuration file in use.
 *      configFileContents: The contents of the configuration file in use.
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Exception handling
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.cluster");

dojo.require("dojo.data.ItemFileWriteStore");
dojo.require("dojo.DeferredList");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.util.cluster.getColleagueNodes = getColleagueNodes;
mcc.util.cluster.getNodeDistribution = getNodeDistribution;
mcc.util.cluster.checkValidNodeId = checkValidNodeId;
mcc.util.cluster.getNextNodeId = getNextNodeId;

/****************************** Internal data   *******************************/

/****************************** Implementation  *******************************/

// Query processes to find first vacant node id within a given interval
function getNextNodeId(minId, maxId) {
    var waitCondition = new dojo.Deferred();
    var nodeids = [];
    mcc.util.assert((minId < maxId), "minId < maxId");
    mcc.storage.processStorage().forItems({NodeId: "*"}, function (process) {
        nodeids[process.getValue("NodeId")] = process.getId();
    },
    // onComplete
    function () {
        for (var i= minId; i<= maxId; i++) {
            if (!nodeids[i]) {
                waitCondition.resolve(i);
                return;
            }
        }
        // Return -1 if none found
        waitCondition.resolve(-1);
    });
    return waitCondition; 
}

// Check if the given node id is valid
function checkValidNodeId(processtype, nodeid, doAlert) {
    var waitCondition = new dojo.Deferred();

    // Check node id
    if (!nodeid || isNaN(nodeid) || nodeid % 1 != 0) {
        if (doAlert) {
            alert("Nodeid '" + nodeid + "' is invalid");
        }
        waitCondition.resolve(false);
    } else {
        // Check node id range for process type
        mcc.storage.processTypeStorage().getItem(processtype).
                then(function (ptype) {
            if (nodeid < ptype.getValue("minNodeId") || 
                    nodeid > ptype.getValue("maxNodeId")) {
                if (doAlert) {
                    alert("Nodeid '" + nodeid + "' is outside valid range (" +
                            ptype.getValue("minNodeId") + " - " +
                            ptype.getValue("maxNodeId") + ")");
                }
                waitCondition.resolve(false);
            } else {
                // Try to fetch an existing process with the given node id
                mcc.storage.processStorage().getItems({NodeId: +nodeid}).then(
                        function (processes) {
                    if (processes && processes.length > 0) {
                        if (doAlert) {
                            alert("Process '" + nodeid +
                                  "' already exists store");
                        }
                        waitCondition.resolve(false);
                    } else {    
                        waitCondition.resolve(true);
                    }
                });
            }
        });
    }
    return waitCondition;
}

// Get an array of ids of nodes of same type on same host
function getColleagueNodes(process) {
    // Single deferred to callback
    var waitCondition = new dojo.Deferred();

    // Fetch processes of same type on same host
    mcc.storage.processStorage().getItems({
        processtype: process.getValue("processtype"), 
        host: process.getValue("host")
    }).then(function (processes) {
        var ids = [];
        for (var p in processes) {
            ids.push(processes[p].getId());
        }
        waitCondition.resolve(ids);
    });
    return waitCondition;
}

// Get the number of nodes of various types, for given host or entire cluster
function getNodeDistribution(hostid) {
    // Single deferred to callback
    var waitCondition = new dojo.Deferred();

    // Fetch all process types
    mcc.storage.processTypeStorage().getItems().then(function (ptypes) {

        // Array of deferreds to wait for
        var waitConditions= [];
        var waitList; 

        // If no ptypes, resolve deferred and return
        if (!ptypes || ptypes.length == 0) {
            waitCondition.resolve(null);
            return;
        }

        // Loop over all process types, get all processes with this ptype
        for (var i in ptypes) {
            
            // Add a new waitCondition for this process type
            waitConditions[i]= new dojo.Deferred();

            // Must use closure to keep context on callback
            (function (waitCond) {
                mcc.storage.processStorage().getItems({
                    processtype: ptypes[i].getId(),
                    host: (hostid !== undefined ? hostid : "*")
                }).then(function (items) {
                    // Resolve wait condition
                    waitCond.resolve((items ? items.length : 0));
                })
            })(waitConditions[i]);
        }

        // After looping over all ptypes, create and wait for DeferredList
        waitList = new dojo.DeferredList(waitConditions);
        waitList.then(function (res) {
            // nNodes must be hashed on process type names
            var nNodes = null; 
            if (res && res.length > 0) {
                nNodes = {};
            }
            for (var i in res) {
                var pName = ptypes[i].getValue("name");
                nNodes[pName] = res[i][1];
            };
            waitCondition.resolve(nNodes);
        });
    });
    return waitCondition;
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Cluster utilities module initialized");
});

