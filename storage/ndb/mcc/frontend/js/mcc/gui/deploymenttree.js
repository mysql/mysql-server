/*
Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

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
 ***                   Process tree manipulation functions                  ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.gui.deploymenttree
 *
 *  Description:
 *      Tree model, widget and manipulation functions
 *
 *  External interface:
 *      mcc.gui.deploymenttree.deploymentTreeSetup: Setup the entire tree view
 *      mcc.gui.deploymenttree.startStatusPoll: Start status polling
 *      mcc.gui.deploymenttree.stopStatusPoll: Stop status polling
 *      mcc.gui.deploymenttree.getCurrentDeploymentTreeItem: Get selection
 *      mcc.gui.deploymenttree.resetDeploymentTreeItem: Reset the selected item
 *      mcc.gui.deploymenttree.getStatii: Return current status vector
 *      mcc.gui.deploymenttree.getMgmtArraySize: Actually # of MGMT nodes in cluster.
 *      mcc.gui.deploymenttree.getMgmtArrayInfo: NodeID , host and port for each MGMT node.
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *      deploymentTreeSetPath: Set path to tree node
 *      updateDeploymentTreeView: Update view based on selection
 *      getStorageItem: Wrap tree/store item into storage item for convenience
 *      deploymentTreeOnMouseDown: Set selected item when the mouse is pressed
 *      deploymentTreeGetIconClass: Return the tree node's corresponding icon
 *      deploymentTreeViewSetup: Setup the process tree and model
 *
 *  Internal data:
 *      deploymentTree: The deployment tree
 *      treeExpanded: Keep track of initial expansion of entire tree
 *      deploymentTreeItem: Selected tree node with related storage items
 *      _statii: current status vector
 *  Unit test interface:
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *
 ******************************************************************************/

/****************************** Import/export *********************************/

dojo.provide('mcc.gui.deploymenttree');

dojo.require('dijit.Tree');
dojo.require('dojox.grid.DataGrid');
dojo.require('dijit.tree.ForestStoreModel');
dojo.require('dijit.Menu');
dojo.require('dijit.MenuItem');
dojo.require('dijit.form.Button');
dojo.require('dijit.form.TextBox');
dojo.require('dijit.form.FilteringSelect');
dojo.require('dijit.form.NumberSpinner');
dojo.require('dijit.Tooltip');

dojo.require('mcc.util');
dojo.require('mcc.storage');
dojo.require('mcc.server');

/***************************** External interface *****************************/

mcc.gui.deploymenttree.deploymentTreeSetup = deploymentTreeSetup;
mcc.gui.deploymenttree.startStatusPoll = startStatusPoll;
mcc.gui.deploymenttree.stopStatusPoll = stopStatusPoll;
mcc.gui.deploymenttree.getCurrentDeploymentTreeItem = getCurrentDeploymentTreeItem;
mcc.gui.deploymenttree.resetDeploymentTreeItem = resetDeploymentTreeItem;
mcc.gui.deploymenttree.getStatii = getStatii;
mcc.gui.deploymenttree.setPollTimeOut = setPollTimeOut;
mcc.gui.deploymenttree.getMgmtArraySize = getMgmtArraySize;
mcc.gui.deploymenttree.getMgmtArrayInfo = getMgmtArrayInfo;
/******************************** Internal data *******************************/

var deploymentTree = null;
var treeExpanded = false;

var timeoutPending = false;
var NO_POLLING = 0;
var POLL_UNTIL_ERROR = 1;
var POLL_UNCONDITIONALLY = 2;
var pollMode = NO_POLLING;
var mgmdArray = null;
var pollMgmdIx = 0;
var pollTO = 5000; // Default poll timeout in milliseconds.
var MgmtArrayInfo = [];

// Keep track of the selected item in the tree
var deploymentTreeItem = {
    treeNode: null,
    treeItem: null,
    storageItem: null,
    dndSourceItem: null
};

/******************************* Implementation *******************************/
/**
 *Determines if value passed is actually empty.
 *
 * @param {any} val   Value to check.
 * @returns {Boolean}
 */
function isEmpty (val) {
    return !!((val === undefined || val == null || val.length <= 0));
}

/**
 *Create tooltips for DOM nodes.
 *
 * @param {[String]} cId DOM nodes names tooltip connects to.
 * @param {String} lbl text to show.
 * @param {String} pos position of tooltip
 * @returns fake
 */
function createTT (cId, lbl, pos) {
    if (!isEmpty(pos)) {
        return new dijit.Tooltip({
            connectId: cId,
            label: lbl,
            position: pos,
            destroyOnHide: true
        });
    } else {
        return new dijit.Tooltip({
            connectId: cId,
            label: lbl,
            destroyOnHide: true
        });
    }
}

function getMgmtArrayInfo () {
    if (MgmtArrayInfo.length < 1) {
        for (var x = 0; x < mgmdArray.length; x++) {
            var host = mgmdArray[x].getValue('HostName');
            var port = mgmdArray[x].getValue('Portnumber');
            var nodeID = mgmdArray[x].getValue('NodeId');
            // If host is not present in ProcessStore, look into HostStore.
            if (!host) {
                mcc.storage.hostStorage().getItems({ id: mgmdArray[x].getValue('host') }).then(
                    function (hosts) {
                        if (hosts[0]) {
                            host = hosts[0].getValue('name');
                        }
                    }
                );
            }
            // left here in case both above methods fail.
            if (!host) {
                host = mcc.configuration.getPara('management',
                    mgmdArray[x].getId(), 'HostName', 'defaultValueInstance');
            }

            // If not overridden, get predefined port from parameters.js
            if (!port) {
                port = mcc.configuration.getPara('management',
                    mgmdArray[x].getId(), 'Portnumber', 'defaultValueInstance');
            }
            MgmtArrayInfo.push({
                ID: nodeID,
                host: host,
                port: port
            });
        }
    }
    return MgmtArrayInfo;
}

function getMgmtArraySize () {
    return mgmdArray.length;
}

// Schedule a status poll
function schedulePoll (timeout) {
    if (timeoutPending) {
        console.debug('[DBG]Skipping since another poll is already scheduled');
        return;
    }
    timeoutPending = true;
    setTimeout(doPoll, timeout);
}

function setPollTimeOut (timeout) {
    console.debug('[DBG]Setting status poll timeout to ' + timeout + ' ms');
    pollTO = timeout;
}

// Wrap a tree or store item into a storage item for convenience
function getStorageItem (item) {
    return new mcc.storage.processTreeStorage().StorageItem(mcc.storage.processTreeStorage(), item);
}

// Get current selection
function getCurrentDeploymentTreeItem () {
    return deploymentTreeItem;
}
// Reset the selected item to undefined
function resetDeploymentTreeItem () {
    deploymentTreeItem.treeItem = null;
    deploymentTreeItem.treeNode = null;
    deploymentTreeItem.storageItem = null;
    deploymentTreeItem.dndSourceItem = null;
}

function updateDeploymentTreeView () {
    var process = deploymentTreeItem.storageItem;

    // If no process selected, or if type selected, reset view and focus
    if (!deploymentTreeItem.treeItem || !process ||
        !mcc.storage.processStorage().isItem(process) ||
        deploymentTreeItem.treeItem.isType('processtype')) {
        dijit.byId('startupDetails').setContent('');
        dijit.byId('configDetails').setContent('');

        // Focus on the selected tree item if it is a process type
        if (deploymentTreeItem.treeItem &&
            deploymentTreeItem.treeItem.isType('processtype')) {
            deploymentTreeSetPath(['root', '' +
                    deploymentTreeItem.storageItem.getId()]);
        }

        return;
    }

    // Get process type, then all from same family
    mcc.storage.processTypeStorage().getItem(process.getValue('processtype')).then(function (pType) {
        // Get types of same family
        mcc.storage.processTypeStorage().getItems({ family: pType.getValue('family') }).then(function (ptypes) {
            // The first corresponds to the id present in the tree
            var pTypeItem = ptypes[0];
            // Focus on the selected tree item
            deploymentTreeSetPath(['root', '' + pTypeItem.getId(), '' + process.getId()]);
            // Update view of configuration file
            var configFile = mcc.configuration.getConfigurationFile(process);
            if (!configFile || !configFile.html) {
                dijit.byId('configDetails').setContent('No configuration file for this process');
            } else {
                dijit.byId('configDetails').setContent(
                    "<table width='100%'>\
                        <tr>\
                            <td width='100px'><b>Host</b></td>\
                            <td>" + configFile.host + '</td>\
                        </tr>\
                        <tr>\
                            <td><b>Path</b></td>\
                            <td>' + configFile.path + '</td>\
                        </tr>\
                        <tr>\
                            <td><b>File</b></td>\
                            <td>' + configFile.name + "</td>\
                        </tr>\
                        <tr valign='top'>\
                            <td><b>Contents</b></td>\
                            <td>" + configFile.html + '</td>\
                        </tr>\
                    </table>');
            }
            // Update view of startup command
            var cmds = mcc.configuration.listStartProcessCommands(process);
            if (!cmds || cmds.length === 0) {
                dijit.byId('startupDetails').setContent('No startup command for this process');
            } else {
                var contents = '';
                for (var i in cmds) {
                    contents += "<table width='100%'>\
                                <tr>\
                                    <td width='100px'><b>Host</b></td>\
                                    <td>" + cmds[i].html.host + '</td>\
                                </tr>\
                                <tr>\
                                    <td><b>Path</b></td>\
                                    <td>' + cmds[i].html.path + '</td>\
                                </tr>\
                                <tr>\
                                    <td><b>Executable</b></td>\
                                    <td>' + cmds[i].html.name + '</td>\
                                </tr>' + cmds[i].html.optionString +
                            '</table><br>';
                }
                dijit.byId('startupDetails').setContent(contents);
            }
        });
    });
}

// Set selected item when the mouse is pressed
function deploymentTreeOnMouseDown (event) {
    resetDeploymentTreeItem();
    deploymentTree.selectedNode = dijit.getEnclosingWidget(event.target);
    deploymentTreeItem.treeNode = deploymentTree.selectedNode;

    if (deploymentTree.selectedNode.item) {
        deploymentTreeItem.treeItem = deploymentTree.selectedNode.item;
    }

    if (deploymentTree.selectedNode.item &&
        !deploymentTree.selectedNode.item.root) {
        deploymentTreeItem.treeItem = getStorageItem(deploymentTree.selectedNode.item);
        // Fetch the storage item
        if (deploymentTreeItem.treeItem.isType('processtype')) {
            mcc.storage.processTypeStorage().getItem(
                deploymentTreeItem.treeItem.getId()).then(
                function (processtype) {
                    deploymentTreeItem.storageItem = processtype;
                    updateDeploymentTreeView();
                });
        } else if (deploymentTreeItem.treeItem.isType('process')) {
            mcc.storage.processStorage().getItem(
                deploymentTreeItem.treeItem.getId()).then(
                function (process) {
                    deploymentTreeItem.storageItem = process;
                    updateDeploymentTreeView();
                }
            );
        } else {
            updateDeploymentTreeView();
        }
    }
}

// Set path to node selection
function deploymentTreeSetPath (path) {
    deploymentTree.set('path', path);
}

// Return the appropriate icon depending on tree node and state
function deploymentTreeGetIconClass (item, opened) {
    if (!item || item.root) {
        return (opened ? 'dijitFolderOpened' : 'dijitFolderClosed');
    }

    if (mcc.storage.processTreeStorage().store().getValue(item, 'type').toLowerCase() === 'processtype') {
        return (opened ? 'dijitFolderOpened' : 'dijitFolderClosed');
    }

    if (mcc.storage.processTreeStorage().isItem(item)) {
        var stat = mcc.storage.processTreeStorage().store().getValue(item, 'status');
        if (stat === 'CONNECTED') {
            return 'greenLight';
        } else if (stat === 'STARTED') {
            return 'greenLight';
        } else if (stat === 'STARTING') {
            return 'yellowLight';
        } else if (stat === 'SHUTTING_DOWN') {
            return 'yellowLight';
        } else if (stat === 'NO_CONTACT') {
            return 'redLight';
        } else if (stat === 'UNKNOWN') {
            return 'dijitIconFunction';
        } else {
            return 'dijitIconFunction';
        }
    }
    return 'dijitIconFunction';
}

// Setup the deployment tree and model
function deploymentTreeViewSetup (clusterName) {
    var deploymentTreeModel = new dijit.tree.ForestStoreModel({
        store: mcc.storage.processTreeStorage().store(),
        query: {},
        rootId: 'root',
        rootLabel: clusterName,
        mayHaveChildren: function (item) {
            if (item.root || getStorageItem(item).isType('processtype')) {
                return true;
            }
            return false;
        },
        // We need to override getChildren to collect instances of same family
        getChildren: function (treeitem, onChildren) {
            var children = [];
            var storageItem = getStorageItem(treeitem);
            if (treeitem.root) {
                var familymap = [];
                mcc.storage.processTreeStorage().forItems({ type: 'processtype' },
                    function (ptype) {
                        if (!familymap[ptype.getValue('name')]) {
                            familymap[ptype.getValue('name')] = ptype.getId();
                            children.push(ptype.item);
                        }
                    },
                    function () {
                        onChildren(children);
                    }
                );
            } else if (storageItem.isType('processtype')) {
                // Get all ptypes of the same family
                mcc.storage.processTreeStorage().forItems({
                    type: 'processtype',
                    name: storageItem.getValue('name')
                },
                function (ptype) {
                    children = children.concat(ptype.getValues('processes'));
                },
                function () {
                    onChildren(children);
                });
            }
        }
    });

    // A dummy data grid to get a properly formatted header
    if (!dijit.byId('deploymentTreeHeader')) {
        var treeViewHeaderGrid = new dojox.grid.DataGrid({
            structure: [{
                name: clusterName + ' processes',
                width: '100%'
            }]
        }, 'deploymentTreeHeader');
        treeViewHeaderGrid.startup();
    }

    // The tree widget itself with model and overriding functions
    deploymentTree = new dijit.Tree({
        model: deploymentTreeModel,
        showRoot: false,
        id: 'deploymentTree',
        getIconClass: deploymentTreeGetIconClass,
        onMouseDown: deploymentTreeOnMouseDown,
        _onNodeMouseEnter: function (node, evt) {
            if (String(node.item.type) === 'process' && node.item.status) {
                dijit.showTooltip('Status: ' + node.item.status, node.domNode);
            }
        },
        _onNodeMouseLeave: function (node, evt) {
            if (String(node.item.type) === 'process') {
                dijit.hideTooltip(node.domNode);
            }
        },
        expandAll: function () {
            var _this = this;
            function expand (node) {
                var def = new dojo.Deferred();
                // Expand the node
                _this._expandNode(node).addCallback(function () {
                    // When expanded, expand() non-leaf children recursively
                    var childBranches = dojo.filter(
                        node.getChildren() || [], function (node) {
                            return node.isExpandable;
                        });
                    var defs = dojo.map(childBranches, expand);
                    // When recursive calls finish, signal that I'm finished
                    new dojo.DeferredList(defs).addCallback(function () {
                        def.callback();
                    });
                });
                return def;
            }
            return expand(this.rootNode);
        }
    }, 'deploymentTree');
    deploymentTree.startup();

    // Expand entire tree first time it is displayed
    if (!treeExpanded) {
        deploymentTree.expandAll();
        treeExpanded = true;
    }
}

// Setup the deployment tree with the cluster name as heading
function deploymentTreeSetup () {
    // Fetch cluster name, to be used in tree heading
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        deploymentTreeViewSetup(cluster.getValue('name'));
    });

    // Header for the startup command
    if (!dijit.byId('startupDetailsHeader')) {
        var startupDetailsHeader = new dojox.grid.DataGrid({
            structure: [{
                name: 'Startup command',
                width: '100%'
            }]
        }, 'startupDetailsHeader');
        startupDetailsHeader.startup();
    }

    // Header for the configuration file
    if (!dijit.byId('configDetailsHeader')) {
        var configDetailsHeader = new dojox.grid.DataGrid({
            structure: [{
                name: 'Configuration file',
                width: '100%'
            }]
        }, 'configDetailsHeader');
        configDetailsHeader.startup();
    }

    // Create tooltip widget and connect to the buttons
    createTT(['configWizardInstallCluster'], 'Install Cluster on host(s) if requested', ['below']);
    createTT(['configWizardDeployCluster'], 'Create necessary directories and distribute \
        configuration files', ['above']);
    createTT(['configWizardStartCluster'], 'Start all cluster processes', ['below']);
    createTT(['configWizardStopCluster'], 'Stop all cluster processes', ['below']);
    createTT(['configWizardViewCmds'], 'Save and display commands issued.', ['below']);
    // Update selection view
    updateDeploymentTreeView();
}

var _statii = {};
function getStatii (nodeid) {
    return _statii[nodeid] ? _statii[nodeid].status : 'UNKNOWN';
}

// Receive status reply
function receiveStatusReply (reply) {
    // NO_POLLING = 0; POLL_UNTIL_ERROR = 1; POLL_UNCONDITIONALLY = 2;
    if (reply && reply.body && reply.body.reply_properties) {
        _statii = reply.body.reply_properties;
        for (var i in reply.body.reply_properties) {
            var curr = reply.body.reply_properties[i];
            mcc.storage.processStorage().getItems({ NodeId: i }).then(function (processes) {
                if (processes && processes[0]) {
                    mcc.storage.processTreeStorage().getItem(processes[0].getId()).then(
                        function (proc) {
                            proc.setValue('status', curr.status);
                        }
                    );
                }
            });
        }
        if (pollMode > NO_POLLING) {
            schedulePoll(pollTO);
        }
    } else {
        receiveStatusError('No mgmd reply', null);
    }
}

// Receive error reply
function receiveStatusError (errMsg, reply) {
    console.debug('[ERR]Error while retrieving status: ' + errMsg + ', index is ' + pollMgmdIx);

    ++pollMgmdIx;
    if (pollMode > NO_POLLING && pollMgmdIx < mgmdArray.length) {
        doPoll();
        return;  // Try another mgmd
    }
    // None of the mgmds returned status, reset all status information in the process tree
    _statii = {};
    mcc.storage.processStorage().getItems().then(function (processes) {
        for (var p in processes) {
            mcc.storage.processTreeStorage().getItem(processes[p].getId()).then(
                function (proc) {
                    proc.deleteAttribute('status');
                }
            );
        }
    });
    pollMgmdIx = 0;
    if (pollMode === POLL_UNCONDITIONALLY) {
        schedulePoll(pollTO);
    }
}

// Poll the mgmd for status
function doPoll () {
    timeoutPending = false;
    var mgmd = mgmdArray[pollMgmdIx];
    if (!mgmd) { return; };
    // Parameter value can have any of 3 sources:
    // a) default value from parameters.js for unset parameter
    // b) value entered by user
    // c) auto calculated value (i.e. 2nd SQL node can not have the same port as
    // 1st one on the same host).
    // We find those values in different Stores. mgmdArray comes from ProcessStore.
    // Different processes will have, occasionally, different parameter names.
    // To peek into SQL nodes, getItems({name: "ndb_mgmd"} should become
    // getItems({name: "mysqld"} and then PortNumber becomes Port and so on.
    // So we look from Top (user entry) -> Down (param default) for those we need.

    // Get the appropriate hostname and port number from ProcessStore.
    var host = mgmd.getValue('HostName');
    var port = mgmd.getValue('Portnumber');
    var instDir = '';
    var uname = '';

    // If host is not present in ProcessStore, look into HostStore.
    if (!host) {
        mcc.storage.hostStorage().getItems({ id: mgmd.getValue('host') }).then(
            function (hosts) {
                if (hosts[0]) {
                    host = hosts[0].getValue('name');
                }
            }
        );
    }

    // left here in case both above methods fail.
    if (!host) {
        host = mcc.configuration.getPara('management', mgmd.getId(), 'HostName', 'defaultValueInstance');
    }
    // If not overridden, get predefined port from parameters.js
    if (!port) {
        port = mcc.configuration.getPara('management', mgmd.getId(), 'Portnumber', 'defaultValueInstance');
    }

    // Get installdir since ndb_mgm may not be in PATH.
    mcc.storage.hostStorage().getItems({ name: host }).then(
        function (hosts) {
            if (hosts[0]) {
                instDir = mcc.util.unixPath(hosts[0].getValue('installdir'));
                uname = hosts[0].getValue('uname');
            }
        }
    );
    mcc.server.runMgmdCommandReq(host, port, instDir, uname, 'get status', receiveStatusReply, receiveStatusError);
}

// Get list of mgmds from store.
function getMgmdArray () {
    MgmtArrayInfo = [];
    /* ProcessType storage:                     Process store:
        "identifier": "id",                         "identifier": "id",
        "label": "name",                            "label": "name",
        "items": [                                  "items": [
            {                                           {
                "id": 0,                                    "id": 0,
                "name": "ndb_mgmd",                         "name": "Management node 1",
                "family": "management",                     "host": 6,
                "familyLabel": "Management layer",          "processtype": 0, (id => processtype)
                "nodeLabel": "Management node",             "NodeId": 49,
                "minNodeId": 49,                            "seqno": 1
                "maxNodeId": 255,                       }
                "currSeq": 2
            }, ...
    */
    return mcc.storage.processTypeStorage().getItems({ name: 'ndb_mgmd' }).then(
        function (mgmdType) {
            mcc.storage.processStorage().getItems({ processtype: mgmdType[0].getId() }).then(
                function (mgmds) { mgmdArray = mgmds; });
        });
}

// Since HOST:PORT connection is not desirable due to firewalls, we connect to
// HOST:22 and issue status commands as if they were "normal" commands.
// To speed polling up, these "management" connections should stay open at all times.
// Function getMgmdArray collects ndb_mgmd process(es) information to use in doPoll.

// Start polling the mgmd for status
function startStatusPoll (stopOnError) {
    pollMode = stopOnError ? POLL_UNTIL_ERROR : POLL_UNCONDITIONALLY;
    console.debug('[DBG]startStatusPoll(' + stopOnError + ') -> pollmode: ' + pollMode);
    // Back end permanent connections will survive change of IP (new permanent connection)
    // but not change of creds for same host.
    getMgmdArray().then(function () { if (!timeoutPending) { pollMgmdIx = 0; doPoll(); } });
}

// Stop status polling
function stopStatusPoll (reason) {
    // if (!reason || reason.length === 0)
    if (reason && String(reason) === 'STOP') {
        // Page is exiting or Cluster stopping, send fake "get status" to clear MGMT connections.
        mcc.server.runMgmdCommandReq('127.0.0.1', '1189', '', '', 'STOP get status',
            receiveStatusReply, receiveStatusError);
    }
    pollMode = 0;
    console.debug('[DBG]stopStatusPoll(' + reason + ')');
}

/********************************* Initialize *********************************/

dojo.ready(function initialize () {
    console.info('[INF]Deployment tree definition module initialized');
});
