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
 ***                                Data stores                             ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.storage.stores
 *
 *  Description:
 *      Setup data stores, initialize from cookies, extend save functions
 *
 *  External interface: 
 *      mcc.storage.stores.initialize: Initialize stores
 *      mcc.storage.stores.setupSaveExtensions: Auto save to cookies
 *      mcc.storage.stores.resetSaveExtensions: Turn off auto save to cookies
 *      mcc.storage.stores.clusterStore: Return cluster store
 *      mcc.storage.stores.processTypeStore: Return process type store
 *      mcc.storage.stores.processStore: Return process store
 *      mcc.storage.stores.processTreeStore: Return process tree store
 *      mcc.storage.stores.hostStore: Return host store
 *      mcc.storage.stores.hostTreeStore: Return host tree store
 *      mcc.storage.stores.loadStore: Return load store
 *      mcc.storage.stores.appAreaStore: Return app area store
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
 *
 *  Internal data: 
 *      See storage overview below
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *
 ***************************** Store overview *********************************
 *
 * hostStore:           (id)    | name       | ...
 * hostTreeStore:       (id fk) | procs (fk) | ...
 * processTypeStore:    (id)    | name       | ...
 * processStore:        (id)    | name       | ptype (fk) | host (fk) | ...
 * processTreeStore:    (id fk) | procs (fk) | ...
 * clusterStore:        (id)    | name       | ...
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.storage.stores");

dojo.require("dojo.data.ItemFileWriteStore");
dojo.require("dojo.data.ItemFileReadStore");

dojo.require("mcc.util");

/**************************** External interface  *****************************/

mcc.storage.stores.initialize = initialize;

mcc.storage.stores.setupSaveExtensions = setupSaveExtensions;
mcc.storage.stores.resetSaveExtensions = resetSaveExtensions;

mcc.storage.stores.clusterStore = getClusterStore;
mcc.storage.stores.processTypeStore = getProcessTypeStore;
mcc.storage.stores.processStore = getProcessStore;
mcc.storage.stores.processTreeStore = getProcessTreeStore;
mcc.storage.stores.hostStore = getHostStore;
mcc.storage.stores.hostTreeStore = getHostTreeStore;
mcc.storage.stores.loadStore = getLoadStore;
mcc.storage.stores.appAreaStore = getAppAreaStore;

/******************************* Internal data ********************************/

var clusterStore = null;
var processTypeStore = null;
var processStore = null;
var processTreeStore = null;
var hostStore = null;
var hostTreeStore = null;
var loadStore = null;
var appAreaStore = null;

/****************************** Implementation  *******************************/

/*************************** Get hold of stores *******************************/

function getClusterStore() {
    return clusterStore;
}

function getProcessTypeStore() {
    return processTypeStore;
}

function getProcessStore() {
    return processStore;
}

function getProcessTreeStore() {
    return processTreeStore;
}

function getHostStore() {
    return hostStore;
}

function getHostTreeStore() {
    return hostTreeStore;
}

function getLoadStore() {
    return loadStore;
}

function getAppAreaStore() {
    return appAreaStore;
}

/******************************** Setup stores ********************************/

// Setup store for cluster information with default values
function clusterStoreSetup() {
    var clusterStoreCookie = mcc.util.getCookie("clusterStore");
    var clusterStoreContents = {identifier: "id", label: "name", items: [
        {
            id: 0,
            ssh_keybased: true,
            ssh_user: "",
            name: 'MyCluster', 
            apparea: 'simple testing',
            writeload: 'medium'
        }
    ]};

    // In the presence of a cookie, use contents from it
    if (clusterStoreCookie !== undefined) {
        clusterStoreContents = dojo.fromJson(clusterStoreCookie);
    }
    clusterStore= new dojo.data.ItemFileWriteStore({
        data: clusterStoreContents
    });   
    clusterStore.save();
}

// Setup stores for process instances, types and their relationship
function processStoreSetup() {
    var processStoreCookie = mcc.util.getCookie("processStore");
    var processStoreContents = {identifier: "id", label: "name", items: []};

    var processTypeStoreCookie = mcc.util.getCookie("processTypeStore");
    var processTypeStoreContents = {identifier: "id", label: "name", items: []};

    // In the presence of a cookie, use contents from it
    if (processStoreCookie !== undefined) {
        processStoreContents = dojo.fromJson(processStoreCookie);
    }
    if (processTypeStoreCookie !== undefined) {
        processTypeStoreContents = dojo.fromJson(processTypeStoreCookie);
    }

    processStore = new dojo.data.ItemFileWriteStore({
        data: processStoreContents
    });
    processTypeStore = new dojo.data.ItemFileWriteStore({
        data: processTypeStoreContents
    });
    
    // The tree store is not persistent, it is re-created form the other stores
    processTreeStore = new dojo.data.ItemFileWriteStore({
        data: {identifier: "id", label: "name", items: []}
    });

    // Save stores
    processStore.save();
    processTypeStore.save();
    processTreeStore.save();
};

// Setup store for hosts and host tree
function hostStoreSetup() {
    var hostStoreCookie = mcc.util.getCookie("hostStore");
    var hostStoreContents = {identifier: "id", label: "name", items: []};

    // In the presence of a cookie, use contents from it
    if (hostStoreCookie !== undefined) {
        hostStoreContents = dojo.fromJson(hostStoreCookie);
    }

    hostStore = new dojo.data.ItemFileWriteStore({data: hostStoreContents});

    // The tree store is not persistent, it is re-created form the other stores
    hostTreeStore = new dojo.data.ItemFileWriteStore({
        data: {identifier: "id", label: "name", items: []}
    });

    hostStore.save();
    hostTreeStore.save();
}

// Setup stores for load types and application area to be used by widgets
function miscStoreSetup() {
    loadStore = new dojo.data.ItemFileWriteStore({
        data: {
            identifier: "name", 
            label: "name", 
            items: []
        }
    });
    loadStore.newItem({name: "low"});
    loadStore.newItem({name: "medium"});
    loadStore.newItem({name: "high"});
    loadStore.save();

    appAreaStore = new dojo.data.ItemFileWriteStore({
        data: {
            identifier: "name", 
            label: "name", 
            items: []
        }
    });
    appAreaStore.newItem({name: "simple testing"});
    appAreaStore.newItem({name: "web application"});
    appAreaStore.newItem({name: "realtime"});
    appAreaStore.save();
}

/******************************** Persistence *********************************/

// Add/delete dummy item, needed to get proper content string for empty store
function forceSaveStore(store) {
    store.newItem({id: -1});
    store.fetchItemByIdentity({
        identity: -1,
        onItem: function (item) {
            store.deleteItem(item);
            store.save();
        }
    });
}

// Save stores to cookies
function forceSaveStores() {
    mcc.util.dbg("Force save data stores");

    forceSaveStore(clusterStore);
    forceSaveStore(processTypeStore);
    forceSaveStore(processStore);
    forceSaveStore(hostStore);
}

// Save extension for a single store
function saveExtension(cookieName) {
    return function (onComplete, onError, contents) {
        mcc.util.setCookie(cookieName, contents);
        onComplete();
    };
}

// Setup save extensions and forcefully save all stores
function setupSaveExtensions() {
    mcc.util.dbg("Setup save extensions for stores");

    clusterStore._saveEverything = saveExtension("clusterStore");
    processTypeStore._saveEverything = saveExtension("processTypeStore");
    processStore._saveEverything = saveExtension("processStore");
    hostStore._saveEverything = saveExtension("hostStore");

    // Force save all stores to flush to cookies
    forceSaveStores();
}

// Reset save extensions for stores
function resetSaveExtensions() {
    mcc.util.dbg("Reset save extensions for stores");

    clusterStore._saveEverything = null;
    processStore._saveEverything = null;
    processTypeStore._saveEverything = null;
    hostStore._saveEverything = null;
}

/******************************** Initialize  *********************************/

// Re-initialize stores
function initialize() {
    clusterStoreSetup();
    processStoreSetup();
    hostStoreSetup();
    miscStoreSetup();

    // Enable cookie based persistence?
    if (mcc.util.getCookie("autoSave") == "on") {
        setupSaveExtensions();
    }
}

dojo.ready(function() {
    initialize();
    mcc.util.dbg("Data store module initialized");
});

