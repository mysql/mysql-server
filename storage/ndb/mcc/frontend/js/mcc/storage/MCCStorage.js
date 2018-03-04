/*
Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
 ***                      Storage instances for MCC data                    ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.storage.MCCStorage
 *
 *  Description:
 *      Storage instances wrapping the mcc stores
 *
 *  External interface: 
 *      mcc.storage.MCCStorage.initialize: Intitialize storages
 *      mcc.storage.MCCStorage.clusterStorage: Get cluster storage;
 *      mcc.storage.MCCStorage.processTypeStorage: Get process type storage
 *      mcc.storage.MCCStorage.processStorage: Get process storage
 *      mcc.storage.MCCStorage.processTreeStorage: Get process tree storage
 *      mcc.storage.MCCStorage.getHostResourceInfo: Fetch resource information
 *      mcc.storage.MCCStorage.hostStorage: Get host storage
 *      mcc.storage.MCCStorage.hostTreeStorage: Get host tree storage
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      newProcessTypeItem: For new process types: Add to process tree
 *      setProcessValue: If updating process, check if tree should be updated
 *      newProcessItem: For new processes: Add to host tree and process tree
 *      deleteProcessItem: Delete process from host tree and process tree too
 *      debugProcessTree: Special debug output for process tree
 *      setHostValue: If updating value, check if tree should be updated
 *      newHostItem: For new hosts: Get hw details and add to host tree storage
 *      deleteHostItem: Delete host/processes from host- and hostTreeStorage
 *      debugHostTree: Special debug output for host tree
 *      initializeHostTreeStorage: Re-create host tree based on hosts/processes
 *      initializeProcessTreeStorage: Re-create tree based on processes/types
 *      initializeProcessTypeStorage: Add default cluster processes if necessary
 *
 *  Internal data: 
 *      clusterStorage: Storage for cluster
 *      processTypeStorage: Storage for process types
 *      processStorage: Storage for processes
 *      processTreeStorage: Storage for process tree
 *      hostStorage: Storage for hosts
 *      hostTreeStorage: Storage for host tree
 *      initializeStorageId: Initialize nextId based on storage contents
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.storage.MCCStorage");

dojo.require("dojo.DeferredList");

dojo.require("mcc.util");
dojo.require("mcc.storage.stores");
dojo.require("mcc.storage.Storage");
dojo.require("mcc.server");
dojo.require("mcc.configuration");

/**************************** External interface  *****************************/

mcc.storage.MCCStorage.initialize = initialize;

mcc.storage.MCCStorage.clusterStorage = getClusterStorage;
mcc.storage.MCCStorage.processTypeStorage = getProcessTypeStorage;
mcc.storage.MCCStorage.processStorage = getProcessStorage;
mcc.storage.MCCStorage.processTreeStorage = getProcessTreeStorage;
mcc.storage.MCCStorage.getHostResourceInfo = getHostResourceInfo;
mcc.storage.MCCStorage.hostStorage = getHostStorage;
mcc.storage.MCCStorage.hostTreeStorage = getHostTreeStorage;

/******************************* Internal data ********************************/

var clusterStorage = null;
var processTypeStorage = null;
var processStorage = null;
var processTreeStorage = null;
var hostStorage = null;
var hostTreeStorage = null;

/****************************** Implementation  *******************************/

/**************************** Cluster storage *********************************/

function getClusterStorage() {
    if (!clusterStorage) {
        clusterStorage = new mcc.storage.Storage({
            name: "Cluster storage", 
            store: mcc.storage.stores.clusterStore
        });
    }
    return clusterStorage;
}

/************************** Process type storage ******************************/

// Special handling for new process types: Add to process tree
function newProcessTypeItem(item) {
    this.inherited(arguments);
    processTreeStorage.getItems({name: item.familyLabel}).then(function (items) {
        if (!items || items.length == 0) {
            processTreeStorage.newItem({
                id: item.id,
                type: "processtype",
                name: item.familyLabel
            });
            processTreeStorage.save();
        }
    });
}

function getProcessTypeStorage() {
    if (!processTypeStorage) {
        processTypeStorage = new mcc.storage.Storage({
            name: "Process type storage", 
            store: mcc.storage.stores.processTypeStore,
            newItem: newProcessTypeItem
        });
    }
    return processTypeStorage;
}

/***************************** Process storage ********************************/

// If updating value, check if tree should be updated
function setProcessValue(process, attr, val) {
    this.inherited(arguments);
    
    // Name updates should be propagated
    if (attr == "name" || attr == "status") {
        hostTreeStorage.getItem(process.getId()).then(function (treeProc) {
            treeProc.setValue("name", process.getValue("name"));
            hostTreeStorage.save();
        });
        processTreeStorage.getItem(process.getId()).then(function (treeProc) {
            treeProc.setValue("name", process.getValue("name"));
            if (process.getValue("status")) {
                treeProc.setValue("status", process.getValue("status"));
            }
            processTreeStorage.save();
        });
    }    
}

// Special handling for new processes: Add to host tree and process tree
function newProcessItem(item) {

    // Add the new process instance to the processStorage
    this.inherited(arguments);

    // Fetch the prototypical process type from the process type storage
    processTypeStorage.getItem(item.processtype).then(function (ptype) {
        processTypeStorage.getItems({family: ptype.getValue("family")}).then(
                function (pfam) {
            // Update sequence number
            var currSeq = pfam[0].getValue("currSeq");            
            pfam[0].setValue("currSeq", ++currSeq);
            processTypeStorage.save();
            
            // Get the appropriate process type tree item 
            processTreeStorage.getItem(pfam[0].getId()).then(function (treeptype) {
                // Add as child in process tree
                processTreeStorage.newItem({id: item.id, type: "process",
                    name: item.name}, 
                    {parent: treeptype.item, attribute: "processes"});
                processTreeStorage.save();
            });
        });
    });

    // Fetch the host from the host tree
    hostTreeStorage.getItem(item.host).then(function (treehost) {
        // Add the new process to hostTreeStore as host child
        hostTreeStorage.newItem({id: item.id, type: "process",
                name: item.name}, 
                {parent: treehost.item, attribute: "processes"});
    });
}

// Delete the given process from processStorage as well as trees
function deleteProcessItem(item) {
    var processId = null;

    // Get process id depending on item type
    if (item.constructor == this.StorageItem) {
        processId = item.getId();
    } else {
        processId = processStorage.store().getValue(item, "id");
    }

    // Reset all predefined parameter values for this id
    mcc.configuration.resetDefaultValueInstance(processId);

    // Delete process from processStorage
    this.inherited(arguments);

    // Fetch the process entry from the process tree and delete
    processTreeStorage.getItem(processId).then(function (treeItem) {
        treeItem.deleteItem();
        // If selected in the process or deployment tree, reset
        if (mcc.gui.getCurrentProcessTreeItem().storageItem &&
            treeItem.getId() == mcc.gui.getCurrentProcessTreeItem().storageItem.getId()) {
            mcc.util.dbg("Deleting selected process, reset current process tree item");
            mcc.gui.resetProcessTreeItem();
        }
        if (mcc.gui.getCurrentDeploymentTreeItem().storageItem &&
            treeItem.getId() == mcc.gui.getCurrentDeploymentTreeItem().storageItem.getId()) {
            mcc.util.dbg("Deleting selected process, reset current deployment tree item");
            mcc.gui.resetDeploymentTreeItem();
        }
    });

    // Fetch the process entry from the host tree and delete
    hostTreeStorage.getItem(processId).then(function (treeItem) {
        treeItem.deleteItem();
        // If selected in the host tree, reset
        if (mcc.gui.getCurrentHostTreeItem().storageItem &&
            treeItem.getId() == mcc.gui.getCurrentHostTreeItem().storageItem.getId()) {
            mcc.util.dbg("Deleting selected process, reset current host tree item");
            mcc.gui.resetHostTreeItem();
        }
    });
}

function getProcessStorage() {
    if (!processStorage) {
        processStorage = new mcc.storage.Storage({
            name: "Process storage", 
            store: mcc.storage.stores.processStore,
            setValue: setProcessValue,
            newItem: newProcessItem,
            deleteItem: deleteProcessItem
        });
    }
    return processStorage;
}

/************************** Process tree storage ******************************/

function debugProcessTree() {
    mcc.util.dbg(this.name + " contents:");
    var that = this;
    this.forItems({}, function (item) {
        processTypeStorage.ifItemId(item.getId(), function () {
            mcc.util.dbg("   " + item.getId() + ": process type");
            var processes = item.getValues("processes");
            for (var i in processes) {
                mcc.util.dbg("    + " + that.store().getIdentity(processes[i]));
            }
        });
    });
}

function getProcessTreeStorage() {
    if (!processTreeStorage) {
        processTreeStorage = new mcc.storage.Storage({
            name: "Process tree storage", 
            store: mcc.storage.stores.processTreeStore,
            debug: debugProcessTree
        });
    }
    return processTreeStorage;
}

/****************************** Host storage **********************************/

// If updating value, check if tree should be updated
function setHostValue(host, attr, val) {
    this.inherited(arguments);
    
    // Name updates should be propagated
    if (attr == "name") {
        hostTreeStorage.getItem(host.getId()).then(function (treeHost) {
            treeHost.setValue(attr, val);
            hostTreeStorage.save();
        });
    }    
}

// Utility function for getting predefined directory names
function getPredefinedDirectory(uname, type) {
    var dir = null;
    var dirs = {
        SunOS: {
            installdir: "/usr/local/bin/",
            datadir: "/var/lib/mysql-cluster/"
        },
        Linux: {
            installdir: "/usr/local/bin/",
            datadir: "/var/lib/mysql-cluster/"
        },
        CYGWIN: {
            installdir: "C:\\Program Files\\MySQL\\",
            datadir: "C:\\Program Data\\MySQL\\"
        },
        Windows: {
            installdir: "C:\\Program Files\\MySQL\\",
            datadir: "C:\\Program Data\\MySQL\\"
        },
        unknown: {
            installdir: "/usr/local/bin/",
            datadir: "/var/lib/mysql-cluster/"
        }
    }
    if (!uname || !dirs[uname]) {
        uname = "unknown";
    }
    if (!type || (type != "installdir" && type != "datadir")) {
        type = "installdir";
    }
    return dirs[uname][type];
}

// Set default values for installdir etc. 
function setDefaultHostDirsUnlessOverridden(hostId, platform, fetchStatus) {
    hostStorage.getItem(hostId).then(function (host) {
        if (fetchStatus) {
            host.setValue("hwResFetch", fetchStatus);
        }
        if (!host.getValue("installdir")) {
            host.setValue("installdir", 
                    getPredefinedDirectory(
                        platform, "installdir"));
            host.setValue("installdir_predef", true);
        }
        if (!host.getValue("datadir")) {
            host.setValue("datadir",
                    getPredefinedDirectory(
                        platform, "datadir"));
            host.setValue("datadir_predef", true);
        }
        hostStorage.save();
    });
}

// Get host resource information
function getHostResourceInfo(hostName, hostId, showAlert, override) {
    // First, get the host item and see if there are undefined values
    hostStorage.getItem(hostId).then(function (host) {
        var nm = host.getValue("name");
        mcc.util.dbg("Running getHostResourceInfo for host " + nm);
        if (!host.getValue("uname") || !host.getValue("ram") || 
                !host.getValue("cores") || !host.getValue("installdir") ||
                !host.getValue("datadir") || override) {
            // There are undefined values, try a new fetch if requests allowed
            if (mcc.util.getCookie("getHostInfo") == "on") {
                // Set fetch status and clear previous values
                host.setValue("hwResFetch", "Fetching...");
                if (host.getValue("installdir_predef")) {
                    host.deleteAttribute("installdir");
                }
                if (host.getValue("datadir_predef")) {
                    host.deleteAttribute("datadir");
                }
                hostStorage.save();
                mcc.util.dbg("Sending hostInfoReq for host " + nm);
                mcc.server.hostInfoReq(nm, function (reply) {
                        mcc.util.dbg("Hardware resources for " + nm + ": " 
                                + "ram = " + reply.body.hostRes.ram + 
                                ", cores = " + reply.body.hostRes.cores +
                                ", uname = " + reply.body.hostRes.uname +
                                ", installdir = " + reply.body.hostRes.installdir +
                                ", datadir = " + reply.body.hostRes.datadir +
                                ", diskfree = " + reply.body.hostRes.diskfree +
                                ", fqdn = " + reply.body.hostRes.fqdn);

                        // Bail out if new request sent
                        if (reply.head.rSeq != host.getValue("hwResFetchSeq")) {
                            mcc.util.dbg("Cancel reply to previous request");
                            return;
                        }
                        
                        // Delete error message
                        host.deleteAttribute("errMsg");

                        // Set resource info
                        if (!host.getValue("ram") || override) {
                            host.setValue("ram", reply.body.hostRes.ram);
                        }
                        if (!host.getValue("cores") || override) {
                            host.setValue("cores", reply.body.hostRes.cores);
                        }
                        if (!host.getValue("uname") || override) {
                            host.setValue("uname", reply.body.hostRes.uname);
                        }
                        if (!host.getValue("installdir") && 
                                reply.body.hostRes.installdir) {
                            var path = mcc.util.terminatePath(
                                    reply.body.hostRes.installdir);
                            if (mcc.util.isWin(host.getValue("uname"))) {
                                path = mcc.util.winPath(path);
                            } else {
                                path = mcc.util.unixPath(path);
                            }
                            host.setValue("installdir", path);
                            host.setValue("installdir_predef", true);
                        }
                        if (!host.getValue("datadir") &&
                                reply.body.hostRes.datadir) {
                            var path = reply.body.hostRes.datadir.replace(/(\r\n|\n|\r)/gm,"");
                            path = mcc.util.terminatePath(path);
                            if (mcc.util.isWin(host.getValue("uname"))) {
                                path = mcc.util.winPath(path);
                            } else {
                                path = mcc.util.unixPath(path);
                            }
                            host.setValue("datadir", path);
                            host.setValue("datadir_predef", true);
                        }
                        if (!host.getValue("diskfree") || override) {
                            host.setValue("diskfree", reply.body.hostRes.diskfree);
                        }
                        if (!host.getValue("fqdn") || override) {
                            host.setValue("fqdn", reply.body.hostRes.fqdn);
                        }
                        if (!host.getValue("internalIP")) {
                            // Use FQDN as default value for internal IP.
                            host.setValue("internalIP", host.getValue("fqdn"));
                        }
                        hostStorage.save();

                        // Set predefined OS specific install path and data dir
                        setDefaultHostDirsUnlessOverridden(hostId, 
                                host.getValue("uname"), "OK");
                    },
                    function (errMsg, reply) {
                        // Bail out if new request sent
                        if (reply.head.rSeq != host.getValue("hwResFetchSeq")) {
                            mcc.util.dbg("Cancel reply to previous request");
                            return;
                        }

                        // Update status, set default values
                        setDefaultHostDirsUnlessOverridden(hostId, "unknown", 
                                "Failed");
                        if (showAlert) {
                            alert("Could not obtain resource information for " + 
                                    "host '" + hostName + "': " + errMsg + 
                                    ". Please " +
                                    "click the appropriate cell in the host " +
                                    "definition page to edit hardware " +
                                    "resource information manually");
                        }
                        // Also save error message, can be viewed in the grid
                        host.setValue("errMsg", errMsg);                       
                        hostStorage.save();
                    }
                );
            } else {
                mcc.util.dbg("No host resource information fetched from host " + 
                        hostName);
                setDefaultHostDirsUnlessOverridden(hostId, "unknown", "N/A");
            }
        }
    });
}

// Special handling for new hosts: Get hw details and add to host tree storage
function newHostItem(item, showAlert) {
    this.inherited(arguments);
    mcc.util.dbg("New host name is " + item.name);
    if (!item.anyHost && item.name != '') {
        hostTreeStorage.newItem({id: item.id, type: "host", name: item.name});
        hostTreeStorage.save();
    } else {
        hostTreeStorage.newItem({id: item.id, type: "anyHost", name: item.name});
    }
    // Get hardware resources unless this is a wildcard host
    if (!item.anyHost && item.name != '') {
        mcc.util.dbg("getHostResourceInfo for " + item.name);
        getHostResourceInfo(item.name, item.id, showAlert, true);
    } else {
        mcc.util.dbg("Skip obtaining hwresource information for wildcard host");
    }
}

// Delete a host and its processes from the hostStorage and hostTreeStorage
function deleteHostItem(item) {
    var hostId = null;

    // Get host id depending on item type
    if (item.constructor == this.StorageItem) {
        hostId = item.getId();
    } else {
        hostId = hostStorage.store().getValue(item, "id");
    }

    // Delete the host from hostStorage, save
    this.inherited(arguments);

    // Fetch the host tree item
    hostTreeStorage.getItem(hostId).then(function (treeHost) {
        // Delete the host from hostTreeStorage, save
        treeHost.deleteItem();
        // If selected in the host tree, reset
        if (mcc.gui.getCurrentHostTreeItem().storageItem &&
            treeHost.getId() == mcc.gui.getCurrentHostTreeItem().storageItem.getId()) {
            mcc.util.dbg("Deleting selected host, reset current host tree item");
            mcc.gui.resetHostTreeItem();
        }
    });

    // Fetch all processes for this host and delete them
    processStorage.forItems({host: hostId}, function (process) {
        process.deleteItem();
    });
}

function getHostStorage() {
    if (!hostStorage) {
        hostStorage = new mcc.storage.Storage({
            name: "Host storage",
            store: mcc.storage.stores.hostStore,
            setValue: setHostValue,
            newItem: newHostItem,
            deleteItem: deleteHostItem,
            getPredefinedDirectory: getPredefinedDirectory
        });
    }
    return hostStorage;
}

/**************************** Host tree storage *******************************/

function debugHostTree() {
    mcc.util.dbg(this.name + " contents:");
    var that = this;
    this.forItems({}, function (item) {
        hostStorage.ifItemId(item.getId(), function () {
            mcc.util.dbg("   " + item.getId() + ": host");
            var processes = item.getValues("processes");
            for (var i in processes) {
                mcc.util.dbg("    + " + that.store().getIdentity(processes[i]));
            }
        });
    });
}

function getHostTreeStorage() {
    if (!hostTreeStorage) {
        hostTreeStorage = new mcc.storage.Storage({
            name: "Host tree storage", 
            store: mcc.storage.stores.hostTreeStore,
            debug: debugHostTree
        });
    }
    return hostTreeStorage;
}

/******************************* Initialize ***********************************/

// Initialize nextId based on storage contents
function initializeStorageId() {
    mcc.storage.Storage.prototype.statics.nextId = 0;
    var waitList = [
            new dojo.Deferred(), new dojo.Deferred(), new dojo.Deferred()];
    var waitCondition = new dojo.DeferredList(waitList);

    function updateId(waitCondition) {
        return function (items) {
            for (var i in items) {
                if (items[i].getId() >= 
                        mcc.storage.Storage.prototype.statics.nextId) {
                    mcc.storage.Storage.prototype.statics.nextId = 
                            items[i].getId() + 1;
                }
            }
            waitCondition.resolve();
        }
    }

    processTypeStorage.getItems().then(updateId(waitList[0]));
    processStorage.getItems().then(updateId(waitList[1]));
    hostStorage.getItems().then(updateId(waitList[2]));

    waitCondition.then(function() {
        mcc.util.dbg("Initialized storage id to: " + 
               mcc.storage.Storage.prototype.statics.nextId);
    });
    return waitCondition; 
}

// Add wildcard host if host storage is empty
function initializeHostStorage() {
    var waitCondition = new dojo.Deferred();
    hostStorage.getItems().then(function (items) {
        if (!items || items.length == 0) {
            mcc.util.dbg("Add wildcard host");
            hostStorage.newItem({name: "Any host", anyHost: true});
            hostStorage.save();
        }
        waitCondition.resolve();
    });
    return waitCondition;
}

// Re-create host tree based on hosts and processes
function initializeHostTreeStorage() {
    var waitCondition = new dojo.Deferred();
    hostStorage.forItems({}, function (host) {
        hostTreeStorage.newItem({
            id: host.getId(),
            type: host.getValue("anyHost") ? "anyHost" : "host",
            name: host.getValue("name")
        });
        // Get the recently added host tree item
        hostTreeStorage.getItem(host.getId(host)).then(function (treeitem) {
            processStorage.forItems({host: +host.getId()}, function (process) {
                hostTreeStorage.newItem({
                        id: process.getId(),
                        type: "process",
                        name: process.getValue("name")
                    }, 
                    {parent: treeitem.item, attribute: "processes"}
                );
            });
        });
    }, 
    function (items, request) {
        hostTreeStorage.save();
        mcc.util.dbg("Re-created host tree");
        hostTreeStorage.debug();
        waitCondition.resolve();
    });
    return waitCondition;
}

// Re-create process tree based on process types and processes
function initializeProcessTreeStorage() {
    var waitCondition = new dojo.Deferred();
    processTypeStorage.forItems({}, function (ptype) {
        mcc.util.assert(ptype, "No process type");
        // Add item unless family already existing 
        processTreeStorage.getItems({name: ptype.getValue("familyLabel")}).then(function (treeitems) {
            var treeitem = treeitems[0];
            if (!treeitem || !processTreeStorage.isItem(treeitem)) {
                processTreeStorage.newItem({
                    id: ptype.getId(),
                    type: "processtype",
                    name: ptype.getValue("familyLabel")
                });
            }
            // Fetch the appropriate item
            processTreeStorage.getItems({name: ptype.getValue("familyLabel")}).then(function (treeitems) {
                var treeitem = treeitems[0];
                processStorage.forItems({processtype: ptype.getId(ptype)},
                        function (process) {
                    processTreeStorage.newItem({
                            id: +process.getId(),
                            type: "process",
                            name: process.getValue("name")
                        },
                        {
                            parent: treeitem.item,
                            attribute: "processes"
                    });
                });
            });
        });
    },
    function() {
        processTreeStorage.save();
        mcc.util.dbg("Re-created process tree");
        processTreeStorage.debug();
        waitCondition.resolve();
    });
    return waitCondition;
}

// Add default cluster processes if necessary
function initializeProcessTypeStorage() {
    var waitCondition = new dojo.Deferred();

    processTypeStorage.getItems().then(function (processTypes) {
        if (processTypes.length == 0) {
            processTypeStorage.newItem({
                name: "ndb_mgmd", 
                family: "management", 
                familyLabel: "Management layer",
                nodeLabel: "Management node",
                minNodeId: 49, 
                maxNodeId: 255,
                currSeq: 1
            }); 
            processTypeStorage.newItem({
                name: "ndbd", 
                family: "data", 
                familyLabel: "Data layer", 
                nodeLabel: "Single threaded data node",
                minNodeId: 1, 
                maxNodeId: 48,
                currSeq: 1
            }); 
            processTypeStorage.newItem({
                name: "ndbmtd", 
                family: "data", 
                familyLabel: "Data layer", 
                nodeLabel: "Multi threaded data node",
                minNodeId: 1, 
                maxNodeId: 48,
                currSeq: 1
            }); 
            processTypeStorage.newItem({
                name: "mysqld", 
                family: "sql", 
                familyLabel: "SQL layer", 
                nodeLabel: "SQL node",
                minNodeId: 49, 
                maxNodeId: 255,
                currSeq: 1
            }); 
            processTypeStorage.newItem({
                name: "api", 
                family: "api", 
                familyLabel: "API layer", 
                nodeLabel: "API node",
                minNodeId: 49, 
                maxNodeId: 255,
                currSeq: 1
            }); 
        } else {
            mcc.util.dbg("Process types already exist, not adding defaults");
        }
        waitCondition.resolve();
    });
    return waitCondition;
}

// Coordinate all initialization, return wait condition
function initialize() {
    var initId = initializeStorageId().then(initializeProcessTypeStorage);

    return new dojo.DeferredList([
        initId.then(initializeProcessTreeStorage),
        initId.then(initializeHostTreeStorage).then(initializeHostStorage)
    ]);

}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    getClusterStorage();
    getProcessTypeStorage();
    getProcessStorage();
    getProcessTreeStorage();
    getHostStorage();
    getHostTreeStorage();
    mcc.util.dbg("MCC storage class module initialized");
});


