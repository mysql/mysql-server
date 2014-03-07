/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

/******************************************************************************
 ***                                                                        ***
 ***                 Unit tests: Node distribution utilities                ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.tests.cluster
 *
 *  Test cases: 
 *      Positive: 
 *          posNoItems: Verify distribution for all hosts with no items
 *          posItems: Verify distribution for all hosts with items stored
 *          posHostNoItems: Verify distribution for one host with no items
 *          posHostItems1: Verify distribution for one host with items
 *          posHostItems2: Verify distribution for one host with items
 *          posHostItems3: Verify distribution for non-existing host with items
 *
 *      Negative:
 *
 *  Support functions:
 *      resetStorage: Make storage empty
 *      setStorage: Initialize storage with known values
 *      
 *
 *  Todo: 
 *      Implement negative test cases
 *      Rewrite when datastore module has been rewritten
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.tests.cluster");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/****************************** Support functions *****************************/

// Delete storage items
function resetStorage() {
    var waitConditions = [
        new dojo.Deferred(),
        new dojo.Deferred(),
        new dojo.Deferred(),
        new dojo.Deferred(),
        new dojo.Deferred()
    ];

    var waitList= new dojo.DeferredList(waitConditions);

    mcc.storage.stores.processTypeStore().fetch({
        query: {},
        onItem: function (item) {
            mcc.storage.stores.processTypeStore().deleteItem(item)
        },
        onComplete: function () {
            mcc.storage.stores.processTypeStore().save();
            waitConditions[0].resolve();
        }
    });

    mcc.storage.stores.processStore().fetch({
        query: {},
        onItem: function (item) {
            mcc.storage.stores.processStore().deleteItem(item)
        },
        onComplete: function () {
            mcc.storage.stores.processStore().save();
            waitConditions[1].resolve();
        }
    });

    mcc.storage.stores.processTreeStore().fetch({
        query: {},
        onItem: function (item) {
            mcc.storage.stores.processTreeStore().deleteItem(item)
        },
        onComplete: function () {
            mcc.storage.stores.processTreeStore().save();
            waitConditions[2].resolve();
        }
    });

    mcc.storage.stores.hostStore().fetch({
        query: {},
        onItem: function (item) {
            mcc.storage.stores.hostStore().deleteItem(item)
        },
        onComplete: function () {
            mcc.storage.stores.hostStore().save();
            waitConditions[3].resolve();
        }
    });

    mcc.storage.stores.hostTreeStore().fetch({
        query: {},
        onItem: function (item) {
            mcc.storage.stores.hostTreeStore().deleteItem(item)
        },
        onComplete: function () {
            mcc.storage.stores.hostTreeStore().save();
            waitConditions[4].resolve();
        }
    });

/*
    mcc.storage.hostStorage().deleteStorage().then(function () {
        mcc.storage.processStorage().deleteStorage().then(function () {
            mcc.storage.processTypeStorage().deleteStorage().then(function () {
                mcc.util.dbg("Deleted storages");
                waitCondition.resolve();
            });
        });
    });*/
    return waitList;
}

// Add known contents
function setStorage() {
    var waitCondition = new dojo.Deferred();
    // Add three process types
    mcc.storage.processTypeStorage().newItem(
            {name: "A1", family: "A", familyLabel: "A", currSeq: 1,
            minNodeId: 0, maxNodeId: 0, nodeLabel: "def_a1"});
    mcc.storage.processTypeStorage().newItem(
            {name: "A2", family: "A", familyLabel: "A", currSeq: 1,
            minNodeId: 0, maxNodeId: 0, nodeLabel: "def_a2"});
    mcc.storage.processTypeStorage().newItem(
            {name: "B", family: "B", familyLabel: "B", currSeq: 1,
            minNodeId: 0, maxNodeId: 0, nodeLabel: "def_b"});

    // Add two hosts
    mcc.storage.hostStorage().newItem(
            {name: "Host_1"});
    mcc.storage.hostStorage().newItem(
            {name: "Host_2"});

    mcc.storage.processTypeStorage().getItems({}).then(function (ptypes) {
        mcc.storage.hostStorage().getItems({}).then(function (hosts) {
            // Host 1: Add two instances of first, one of second, none of third
            mcc.storage.processStorage().newItem(
                    {name: "a1_0", processtype: ptypes[0].getId(), 
                    host: hosts[0].getId(), NodeId: 0});
            mcc.storage.processStorage().newItem(
                    {name: "a1_1", processtype: ptypes[0].getId(), 
                    host: hosts[0].getId(), NodeId: 1});
            mcc.storage.processStorage().newItem(
                    {name: "b_2", processtype: ptypes[1].getId(), 
                    host: hosts[0].getId(), NodeId: 2});

            // Host 2: Add one instance of first, none of second, none of third
            mcc.storage.processStorage().newItem(
                    {name: "a1_3", processtype: ptypes[0].getId(), 
                    host: hosts[1].getId(), NodeId: 3});
            waitCondition.resolve();
        });
    });
    return waitCondition;
}

/******************************* Test cases  **********************************/

// Verify distribution with no nodes or process types
function posNoItems() {
    var postCondition = new doh.Deferred();
    // Empty stores with no items
    resetStorage().then(function () {
        // Get node distribution across all nodes
        mcc.util.getNodeDistribution().then(function (nNodes) {
            if (!nNodes) {
                mcc.util.tst("No process types found");
                postCondition.callback(true);
            } else {
                mcc.util.tst("Found process types");
                for (var i in nNodes) {
                    mcc.util.tst("nNodes[" + i + "]= " + nNodes[i]);
                }
                postCondition.errback(false);
            }
        });
    });
    return postCondition; 
}

// Verify distribution with nodes and process types
function posItems() {
    var postCondition = new doh.Deferred();
    resetStorage().then(function () {
        // Setup known distribution
        setStorage().then(function () {
            // Get node distribution across all nodes
            mcc.util.getNodeDistribution().then(function (nNodes) {
                if (!nNodes) {
                    mcc.util.tst("No process types found");
                    postCondition.errback(false);
                } else {
                    mcc.util.tst("Found process types");
                    if (nNodes["A1"] == 3 && nNodes["A2"] == 1 && 
                            nNodes["B"] == 0) {
                        postCondition.callback(true);
                    } else {
                        for (var i in nNodes) {
                            mcc.util.tst("nNodes[" + i + "]= " + nNodes[i]);
                        }
                        postCondition.errback(false);
                    }
                } 
            });
        });
    });
    return postCondition; 
}

// Verify distribution with no nodes or process types for non-existing host
function posHostNoItems() {
    var postCondition = new doh.Deferred();
    // Setup empty distribution
    resetStorage().then(function () {
        // Get node distribution for a non-existing host
        mcc.util.getNodeDistribution(0).then(function (nNodes) {
            if (!nNodes || nNodes.length == 0) {
                mcc.util.tst("No process types found");
                postCondition.callback(true);
            } else {
                mcc.util.tst("Found process types");
                for (var i in nNodes) {
                    mcc.util.tst("nNodes[" + i + "]= " + nNodes[i]);
                }
                postCondition.errback(false);
            }
        });
    });
    return postCondition; 
}

// Verify distribution with nodes and process types for one host
function posHostItems1() {
    var postCondition = new doh.Deferred();
    resetStorage().then(function () {
        // Setup known distribution
        setStorage().then(function () {
            // Get node distribution for one host
            mcc.storage.hostStorage().getItems({name: "Host_1"}).
                    then(function (hosts) {
                mcc.util.getNodeDistribution(hosts[0].getId()).then(
                        function (nNodes) {
                    if (!nNodes) {
                        mcc.util.tst("No process types found");
                        postCondition.errback(false);
                    } else {
                        mcc.util.tst("Found process types");
                        if (nNodes["A1"] == 2 && nNodes["A2"] == 1 && 
                                nNodes["B"] == 0) {
                            postCondition.callback(true);
                        } else {
                            postCondition.errback(false);
                        }
                    } 
                });
            });
        });
    });
    return postCondition; 
}

// Verify distribution with nodes and process types for one host 
function posHostItems2() {
    var postCondition = new doh.Deferred();
    resetStorage().then(function () {
        // Setup known distribution
        setStorage().then(function () {
            // Get node distribution for one host
            mcc.storage.hostStorage().getItems({name: "Host_2"}).
                    then(function (hosts) {
                mcc.util.getNodeDistribution(hosts[0].getId()).then(
                        function (nNodes) {
                    if (!nNodes) {
                        mcc.util.tst("No process types found");
                        postCondition.errback(false);
                    } else {
                        mcc.util.tst("Found process types");
                        if (nNodes["A1"] == 1 && nNodes["A2"] == 0 && 
                                nNodes["B"] == 0) {
                            postCondition.callback(true);
                        } else {
                            postCondition.errback(false);
                        }
                    } 
                });
            });
        });
    });

    return postCondition; 
}

// Verify distribution with nodes and process types for a non-existing host
function posHostItems3() {
    var postCondition = new doh.Deferred();
    resetStorage().then(function () {
        // Setup known distribution
        setStorage();
        // Get node distribution for non-existing host
        mcc.util.getNodeDistribution(2).then(function (nNodes) {
            if (!nNodes) {
                mcc.util.tst("No process types found");
                postCondition.errback(false);
            } else {
                mcc.util.tst("Found process types");
                if (nNodes["A1"] == 0 && nNodes["A2"] == 0 && nNodes["B"] == 0) {
                    postCondition.callback(true);
                } else {
                    postCondition.errback(false);
                }
            } 
        });
    });
    return postCondition; 
}

/*************************** Register test cases  *****************************/

doh.register("mcc.util.tests.cluster", [
    posNoItems,
    posItems,
    posHostNoItems,
    posHostItems1,
    posHostItems2,
    posHostItems3
]);


