/*
Copyright (c) 2012, 2019 Oracle and/or its affiliates. All rights reserved.

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
 *      mcc.storage.stores.clusterVersionStore: Return store with supported
 *          Cluster versions.
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

/******************************* Import/export ********************************/
dojo.provide('mcc.storage.stores');

dojo.require('dojo.data.ItemFileWriteStore');
dojo.require('dojo.data.ItemFileReadStore');

if (!!window.MSInputMethodContext && !!document.documentMode) {
    dojo.require('mcc.userconfigIE');
} else {
    dojo.require('mcc.userconfig');
}

dojo.require('mcc.util');
dojo.require('mcc.server');

/***************************** External interface *****************************/
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
mcc.storage.stores.clusterVersionStore = getClusterVersionStore;
mcc.storage.stores.clusterStoreOriginal = getClusterStoreOriginal;
mcc.storage.stores.processTypeStoreOriginal = getProcessTypeStoreOriginal;
mcc.storage.stores.processStoreOriginal = getProcessStoreOriginal;
mcc.storage.stores.hostStoreOriginal = getHostStoreOriginal;

/******************************** Internal data *******************************/
var clusterStore = null;
var processTypeStore = null;
var processStore = null;
var processTreeStore = null;
var hostStore = null;
var hostTreeStore = null;
var loadStore = null;
var appAreaStore = null;
var clusterVersionStore = null;
// --Backup original stores so to know when changes are made. IF null, new config.
var clusterStoreOriginal = null;
var processTypeStoreOriginal = null;
var processStoreOriginal = null;
var hostStoreOriginal = null;

/******************************* Implementation *******************************/

/**************************** Get hold of stores ******************************/
function getClusterStore () {
    return clusterStore;
}

function getProcessTypeStore () {
    return processTypeStore;
}

function getProcessStore () {
    return processStore;
}

function getProcessTreeStore () {
    return processTreeStore;
}

function getHostStore () {
    return hostStore;
}

function getHostTreeStore () {
    return hostTreeStore;
}

function getLoadStore () {
    return loadStore;
}

function getAppAreaStore () {
    return appAreaStore;
}

function getClusterVersionStore () {
    return clusterVersionStore;
}

function getClusterStoreOriginal () {
    return clusterStoreOriginal;
}

function getProcessTypeStoreOriginal () {
    return processTypeStoreOriginal;
}

function getProcessStoreOriginal () {
    return processStoreOriginal;
}

function getHostStoreOriginal () {
    return hostStoreOriginal;
}

/******************************** Setup stores ********************************/

// Setup store for cluster information with default values
function clusterStoreSetup () {
    var confData = mcc.userconfig.getConfigFileContents();

    if (confData) {
        console.debug('[DBG]stores.clusterStoreSetup from config');
        var arr = JSON.parse('[' + confData + ']');
        var clusterStoreContents1 = JSON.parse(JSON.stringify(arr[0]));
        clusterStore = new dojo.data.ItemFileWriteStore({ data: clusterStoreContents1 });
        clusterStore.data = clusterStore._jsonData;
        console.debug('[DBG]clusterStoreSetup from config, moved _jsonData -> data.');
    } else {
        console.debug('[DBG]clusterStoreSetup from cookie.');
        var clusterStoreCookie = mcc.util.getCookie('clusterStore');
        var clusterStoreContents = { identifier: 'id',
            label: 'name',
            items: [
                {
                    id: 0,
                    ssh_keybased: true,
                    ssh_user: '',
                    name: 'MyCluster',
                    apparea: 'simple testing',
                    writeload: 'medium',
                    ClusterVersion: '8.0'
                }
            ] };
        // In the presence of a cookie, use contents from it
        if (clusterStoreCookie !== undefined) { clusterStoreContents = dojo.fromJson(clusterStoreCookie); }
        clusterStore = new dojo.data.ItemFileWriteStore({ data: clusterStoreContents });
    }
    clusterStore.save();
}

// Setup stores for process instances, types and their relationship
function processStoreSetup () {
    var confData = mcc.userconfig.getConfigFileContents();
    var processStoreContents = { identifier: 'id', label: 'name', items: [] };
    var processTypeStoreContents = { identifier: 'id', label: 'name', items: [] };

    if (confData) {
        // PROCESS, PROCESSTYPE and PROCESSTREE.
        // PROCESS:
        console.debug('[DBG]processStoreSetup from config.');
        var arr = JSON.parse('[' + confData + ']');
        var processStoreContents1 = JSON.parse(JSON.stringify(arr[3]));
        var itCnt = 0;
        try {
            itCnt = arr[3].items.length;
        } catch (err) {
            itCnt = 0;
        }
        if (itCnt > 0) {
            processStore = new dojo.data.ItemFileWriteStore({ data: processStoreContents1 });
            processStore.data = processStore._jsonData;
            console.debug('[DBG]processStoreSetup from config, moved _jsonData -> data.');
        } else {
            processStore = new dojo.data.ItemFileWriteStore({ data: processStoreContents });
            console.debug('[DBG]processStoreSetup from config, empty.');
        }
        console.debug('[DBG]processStoreSetup from config, process store set.');

        // PROCESSTYPE:
        var processTypeStoreContents1 = JSON.parse(JSON.stringify(arr[4]));
        itCnt = 0;
        try {
            itCnt = arr[4].items.length;
        } catch (err) {
            itCnt = 0;
        }
        if (itCnt > 0) {
            processTypeStore = new dojo.data.ItemFileWriteStore({ data: processTypeStoreContents1 });
            processTypeStore.data = processTypeStore._jsonData;
            console.debug('[DBG]processTypeStoreSetup from config, moved _jsonData -> data.');
        } else {
            processTypeStore = new dojo.data.ItemFileWriteStore({ data: processTypeStoreContents });
            console.debug('[DBG]processTypeStore from config, empty.');
        }
        console.debug('[DBG]processTypeStoreSetup from config, process store set.');

        // The tree store is not persistent, it is re-created from the other stores
        processTreeStore = new dojo.data.ItemFileWriteStore({ data: { identifier: 'id', label: 'name', items: [] } });
    } else {
        console.debug('[DBG]processStoreSetup from cookies.');
        var processStoreCookie = mcc.util.getCookie('processStore');
        var processTypeStoreCookie = mcc.util.getCookie('processTypeStore');
        // In the presence of a cookie, use contents from it
        if (processStoreCookie !== undefined) { processStoreContents = dojo.fromJson(processStoreCookie); }
        if (processTypeStoreCookie !== undefined) { processTypeStoreContents = dojo.fromJson(processTypeStoreCookie); }

        processStore = new dojo.data.ItemFileWriteStore({ data: processStoreContents });
        processTypeStore = new dojo.data.ItemFileWriteStore({ data: processTypeStoreContents });

        // The tree store is not persistent, it is re-created from the other stores
        processTreeStore = new dojo.data.ItemFileWriteStore({
            data: { identifier: 'id', label: 'name', items: [] }
        });
    }
    // Save stores
    processStore.save();
    processTypeStore.save();
    processTreeStore.save();
}

// Setup store for hosts and host tree
function hostStoreSetup () {
    var confData = mcc.userconfig.getConfigFileContents();
    var hostStoreContents = { identifier: 'id', label: 'name', items: [] };
    if (confData) {
        console.debug('[DBG]hostStoreSetup from config');
        var arr = JSON.parse('[' + confData + ']');
        var hostStoreContents1 = JSON.parse(JSON.stringify(arr[1]));
        var itCnt = 0;
        try {
            itCnt = arr[1].items.length;
        } catch (err) {
            itCnt = 0;
        }
        if (itCnt > 0) {
            hostStore = new dojo.data.ItemFileWriteStore({ data: hostStoreContents1 });
            hostStore.data = hostStore._jsonData;
            console.debug('[DBG]hostStoreSetup from config, moved _jsonData -> data.');
        } else {
            hostStore = new dojo.data.ItemFileWriteStore({ data: hostStoreContents });
            console.debug('[DBG]hostStoreSetup from config, empty.');
        }
        console.debug('[DBG]hostStoreSetup from config, host store set.');
        // The tree store is not persistent, it is re-created form the other stores
        hostTreeStore = new dojo.data.ItemFileWriteStore({ data: { identifier: 'id', label: 'name', items: [] } });
    } else {
        console.debug('[DBG]hostStoreSetup from cookie.');
        var hostStoreCookie = mcc.util.getCookie('hostStore');
        // In the presence of a cookie, use contents from it
        if (hostStoreCookie !== undefined) { hostStoreContents = dojo.fromJson(hostStoreCookie); }
        hostStore = new dojo.data.ItemFileWriteStore({ data: hostStoreContents });
        // The tree store is not persistent, it is re-created form the other stores
        hostTreeStore = new dojo.data.ItemFileWriteStore({ data: { identifier: 'id', label: 'name', items: [] }
        });
    }
    hostStore.save();
    hostTreeStore.save();
}

// Setup stores for load types and application area to be used by widgets
function miscStoreSetup () {
    loadStore = new dojo.data.ItemFileWriteStore({
        data: {
            identifier: 'name',
            label: 'name',
            items: []
        }
    });
    loadStore.newItem({ name: 'low' });
    loadStore.newItem({ name: 'medium' });
    loadStore.newItem({ name: 'high' });
    loadStore.save();

    appAreaStore = new dojo.data.ItemFileWriteStore({
        data: {
            identifier: 'name',
            label: 'name',
            items: []
        }
    });
    appAreaStore.newItem({ name: 'simple testing' });
    appAreaStore.newItem({ name: 'web application' });
    appAreaStore.newItem({ name: 'realtime' });
    appAreaStore.save();

    clusterVersionStore = new dojo.data.ItemFileWriteStore({
        data: {
            identifier: 'name',
            label: 'name',
            items: []
        }
    });
    clusterVersionStore.newItem({ name: '7.6' });
    clusterVersionStore.newItem({ name: '8.0' });
    clusterVersionStore.save();
}

/********************************* Persistence ********************************/

// Add/delete dummy item, needed to get proper content string for empty store
function forceSaveStore (store) {
    store.newItem({ id: -1 });
    store.fetchItemByIdentity({
        identity: -1,
        onItem: function (item) {
            store.deleteItem(item);
            store.save();
        }
    });
}

// Save stores to cookies
function forceSaveStores () {
    console.debug('[DBG]Force save data stores');

    forceSaveStore(clusterStore);
    forceSaveStore(processTypeStore);
    forceSaveStore(processStore);
    forceSaveStore(hostStore);
}

// Save extension for a single store
function saveExtension (cookieName) {
    return function (onComplete, onError, contents) {
        mcc.util.setCookie(cookieName, contents);
        onComplete();
    };
}

// Setup save extensions and forcefully save all stores
function setupSaveExtensions () {
    clusterStore._saveEverything = saveExtension('clusterStore');
    processTypeStore._saveEverything = saveExtension('processTypeStore');
    processStore._saveEverything = saveExtension('processStore');
    hostStore._saveEverything = saveExtension('hostStore');
    // Force save all stores to flush
    forceSaveStores();
}

// Reset save extensions for stores
function resetSaveExtensions () {
    clusterStore._saveEverything = null;
    processStore._saveEverything = null;
    processTypeStore._saveEverything = null;
    hostStore._saveEverything = null;
}

/********************************* Initialize *********************************/
// Re-initialize stores
function initialize () {
    clusterStoreSetup();
    processStoreSetup();
    hostStoreSetup();
    miscStoreSetup();
    // Enable cookie based persistence?
    if (mcc.util.getCookie('autoSave') === 'on') { setupSaveExtensions(); }
}

dojo.ready(function () {
    initialize();
    console.info('[INF]Data store module initialized');
});
