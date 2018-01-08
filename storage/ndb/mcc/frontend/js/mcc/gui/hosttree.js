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
 ***                      Host tree manipulation functions                  ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.gui.hosttree
 *
 *  Description:
 *      Tree model, widget and manipulation functions
 *
 *  External interface: 
 *      mcc.gui.hosttree.hostTreeSetPath: Set path to selected tree node
 *      mcc.gui.hosttree.hostTreeSetup: Setup the entire host tree view
 *      mcc.gui.hosttree.getCurrentHostTreeItem: Get current selection
 *      mcc.gui.hosttree.resetHostTreeItem: Reset the selected item
 *
 *  External data: 
 *      None
 *
 *  Internal interface:
 *      getStorageItem: Wrap tree/store item into storage item for convenience
 *      hostTreeCheckItemAcceptance: Validate dnd attempts
 *      hostTreeOnMouseDown: Set selected item when the mouse is pressed
 *      hostTreeGetIconClass: Return the appropriate icon depending on tree node
 *      hostTreeViewSetup: Setup the host tree and model
 *      hostTreeMenuSetup: Setup a simple context sensitive pulldown menu
 *      hostTreeButtonSetup: Setup a buttons for adding and deleting processes
 *      addProcess: Check that node id is valid, add the process to the storage
 *      addProcessDlgSetup: Setup a dialog for adding new proesses
 *      showAddProcessDialog: Show dialog for adding processes
 *      
 *
 *  Internal data: 
 *      hostTree: The host tree
 *      treeExpanded: Keep track of initial expansion of entire tree
 *      addProcessDlg: Dialog for adding processes
 *      hostTreeItem: Selected tree node with related storage items
 *      
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.gui.hosttree");

dojo.require("dijit.Tree");
dojo.require("dojox.grid.DataGrid");
dojo.require("dijit.tree.ForestStoreModel");
dojo.require("dijit.tree.dndSource");
dojo.require("dijit.Menu");
dojo.require("dijit.MenuItem");
dojo.require("dijit.form.Form");
dojo.require("dijit.form.Button");
dojo.require("dijit.form.TextBox");
dojo.require("dijit.form.FilteringSelect");
dojo.require("dijit.form.NumberSpinner");
dojo.require("dijit.Dialog");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.gui.hosttree.hostTreeSetPath = hostTreeSetPath;
mcc.gui.hosttree.getCurrentHostTreeItem = getCurrentHostTreeItem;
mcc.gui.hosttree.resetHostTreeItem = resetHostTreeItem;
mcc.gui.hosttree.hostTreeSetup = hostTreeSetup;

/******************************* Internal data ********************************/

var hostTree = null;
var treeExpanded = false; 
var addProcessDlg = null;

// Keep track of the selected item in the tree
var hostTreeItem= {
    treeNode: null,         // node in the tree
    treeItem: null,         // item in hostTreeStorage
    storageItem: null,      // item in hostStorage or processStorage
    dndSourceItem: null     // needed to revert when dnd is aborted
};

/****************************** Implementation  *******************************/

// Wrap a tree or store item into a storage item for convenience
function getStorageItem(item) {
    return new mcc.storage.hostTreeStorage().StorageItem(
            mcc.storage.hostTreeStorage(), item);
}

// Accept moving process onto host. Update host attribute accordingly
function hostTreeCheckItemAcceptance(target, source, position) {
    var retval = false; 

    // Verify valid move
    if (target && hostTreeItem.treeItem.isType("process")) {
        var targetTreeNode = dijit.getEnclosingWidget(target);
        var targetItem = getStorageItem(targetTreeNode.item);
        if (targetItem.isType("host")) {
            hostTreeItem.storageItem.setValue("host", targetItem.getId());
            mcc.storage.processStorage().save();
            retval = true;
        }
        // Only allowed to move api processes onto wildcard host
        if (targetItem.isType("anyHost") && 
                hostTreeItem.ptypeItem.getValue("name") == "api") {
            hostTreeItem.storageItem.setValue("host", targetItem.getId());
            mcc.storage.processStorage().save();
            retval = true;
        }
    }
    
    // If invalid move, revert to original value
    if (!retval && hostTreeItem.treeItem.isType("process")) {
        hostTreeItem.storageItem.setValue("host",
                hostTreeItem.dndSourceItem.getId());
        mcc.storage.processStorage().save();
    }
    return retval;
}

// Get current selection
function getCurrentHostTreeItem() {
    return hostTreeItem;
}

// Reset the selected item, e.g. if the selected item is deleted
function resetHostTreeItem() {
    hostTreeItem.treeNode = null; 
    hostTreeItem.treeItem = null; 
    hostTreeItem.storageItem = null; 
    hostTreeItem.dndSourceItem = null; 
    hostTreeItem.ptypeItem = null; 
}

// Set selected item when the mouse is pressed
function hostTreeOnMouseDown(event) {
    resetHostTreeItem();
    hostTree.selectedNode = dijit.getEnclosingWidget(event.target);
    hostTreeItem.treeNode = hostTree.selectedNode;

    if (hostTree.selectedNode.item && !hostTree.selectedNode.item.root) {
        hostTreeItem.treeItem = getStorageItem(
                hostTree.selectedNode.item);
        // Fetch the storage item
        if (hostTreeItem.treeItem.isType("host") || 
                hostTreeItem.treeItem.isType("anyHost")) {
            mcc.storage.hostStorage().getItem(hostTreeItem.treeItem.getId()).
                    then(function (host) {
                    hostTreeItem.storageItem = host;
                });
        } else if (hostTreeItem.treeItem.isType("process")) {
            mcc.storage.processStorage().getItem(hostTreeItem.treeItem.getId()).
                    then(function (process) {
                hostTreeItem.storageItem = process;
                mcc.storage.hostStorage().getItem(process.getValue("host")).
                        then(function (host) {
                    hostTreeItem.dndSourceItem = host;
                });
                mcc.storage.processTypeStorage().getItem(
                        process.getValue("processtype")).
                        then(function (ptype) {
                    hostTreeItem.ptypeItem = ptype;
                });
            });
        }
    }

    mcc.gui.updateHostTreeSelectionDetails();
}

// Set path to node selection
function hostTreeSetPath(path) {
    hostTree.set("path", path);
}

// Return the appropriate icon depending on tree node and state
function hostTreeGetIconClass(item, opened) {
    if (!item || item.root) {
        return (opened ? "dijitFolderOpened" : "dijitFolderClosed");
    }

    var type = mcc.storage.hostTreeStorage().store().getValue(item, "type");

    if (type == "host") {
        return "dijitIconDatabase";
    }

    if (type == "anyHost") {
        return "anyHostIcon";
    }

    return "dijitIconFunction";
}

// Setup the host tree and model
function hostTreeViewSetup(clusterName) {
    var hostTreeModel = new dijit.tree.ForestStoreModel({
        store: mcc.storage.hostTreeStorage().store(),
        query: {},
        rootLabel: clusterName,
        rootId: "root",
        childrenAttrs: ["processes"]
    });

    // A dummy data grid to get a properly formatted header
    var treeViewHeaderGrid= new dojox.grid.DataGrid({
        autoHeight: true,
        structure: [{
            name: clusterName + " topology",
            width: "100%"
        }],
    }, "hostTreeHeader");
    treeViewHeaderGrid.startup();

    // The tree widget itself with model and overriding functions
    hostTree= new dijit.Tree({
        model: hostTreeModel,
        showRoot: false, 
        id: "hostTree",
        getIconClass: hostTreeGetIconClass,
        dndController: "dijit.tree.dndSource",
        checkItemAcceptance: hostTreeCheckItemAcceptance,
        onMouseDown: hostTreeOnMouseDown,
        expandAll: function() { 
            var _this = this; 

            function expand(node) { 
                var def = new dojo.Deferred(); 

                // Expand the node 
                _this._expandNode(node).addCallback(function() { 

                    // When expanded, expand() non-leaf childs recursively
                    var childBranches = dojo.filter(
                            node.getChildren() || [], function (node) { 
                                return node.isExpandable; 
                            });

                    defs = dojo.map(childBranches, expand); 

                    // When recursive calls finish, signal that I'm finished 
                    new dojo.DeferredList(defs).addCallback(function () { 
                        def.callback(); 
                    }); 
                }); 
                return def; 
            } 
            return expand(this.rootNode); 
        }
    }, "hostTree");
    hostTree.startup();

    // Expand entire tree first time it is displayed
    if (!treeExpanded) {
        hostTree.expandAll();
        treeExpanded = true;
    }
}

// Setup a simple context sensitive pulldown menu for the tree nodes
function hostTreeMenuSetup() {
    var hostTreeMenu = new dijit.Menu({
        targetNodeIds: ["hostTree"]
    });
    hostTreeMenu.startup();
    dojo.connect(hostTreeMenu, "_openMyself", null, function(e) {
        var children= hostTreeMenu.getChildren();
        // Remove existing menu choices
        hostTreeMenu.focusedChild = null;
        for (i in children) {
            hostTreeMenu.removeChild(children[i]);
        }
        // Rebuild menu depending on tree node
        if (hostTreeItem.treeItem.isType("process")) {
            hostTreeMenu.addChild(new dijit.MenuItem({
                label: "Del process",
                onClick: function() {
                    mcc.storage.processStorage().deleteItem(
                            hostTreeItem.storageItem)
                    // Reset selected and reset focus
                    resetHostTreeItem();
                    mcc.gui.updateHostTreeSelectionDetails(hostTreeItem);
                }
            }));
        } else if (hostTreeItem.treeItem.isType("host") || 
                hostTreeItem.treeItem.isType("anyHost")) {
            hostTreeMenu.addChild(new dijit.MenuItem({
                label: "Add process",
                onClick: showAddProcessDialog
            }));
        } 
    });
}

// Setup a buttons for adding and deleting processes
function hostTreeButtonSetup() {
    
    function addTreeItem() {
        if (hostTreeItem.treeItem && 
                (hostTreeItem.treeItem.isType("host") || 
                hostTreeItem.treeItem.isType("anyHost"))) {
            showAddProcessDialog();
        } else {
            alert("Please select a host");
        }
    }

    function deleteTreeItem() {
        if (hostTreeItem.treeItem.isType("anyHost")) {
            alert("The wildcard host cannot be deleted");
        } else if (hostTreeItem.treeItem.isType("host")) {
            alert("Please go back to the 'Define hosts' page to delete hosts");
        } else if (hostTreeItem.treeItem.isType("process")) {
            mcc.storage.processStorage().deleteItem(
                    hostTreeItem.storageItem);
            // Reset selected and reset focus
            resetHostTreeItem();
            mcc.gui.updateHostTreeSelectionDetails(hostTreeItem);
        } else {
            alert("Please select a process to delete");
        }
    }

    var addButton= new dijit.form.Button({label: "Add process",
            iconClass: "dijitIconAdd"}, "addProcessButton");
    var deleteButton= new dijit.form.Button({label: "Del process",
            iconClass: "dijitIconDelete"}, "deleteProcessButton");
    
    dojo.connect(addButton, "onClick", addTreeItem);
    dojo.connect(deleteButton, "onClick", deleteTreeItem);
}

// Check that the node id is valid, add the process to the storage
function addProcess(event) {
    var processType = dijit.byId("processtype").getValue();
    var nodeId = -1;
    
    // Prevent default submit handling
    dojo.stopEvent(event);

    // Get node id, handle callbacks
    mcc.storage.processTypeStorage().getItem(processType).then(function (ptype){        
        mcc.util.getNextNodeId(ptype.getValue("minNodeId"),
                ptype.getValue("maxNodeId")).then(
                function (nodeId) {
            if (nodeId >= 0) {
                mcc.util.checkValidNodeId(processType, nodeId, true).then(
                        function (res) {
                    if (res) {
                        mcc.storage.processStorage().newItem({
                            name: dijit.byId("processname").get("value"),
                            processtype: +processType,
                            host: hostTreeItem.treeItem.getId(),
                            NodeId: nodeId,
                            seqno: dijit.byId("seqno").get("value")
                        })
                        dijit.byId("addProcessDlg").hide();
                    }
                    return res;
                });
            } else {
                alert("Unable to get a new node id");
            }
        });
    });
}

// Setup a dialog for adding new proesses
function addProcessDlgSetup() {
    var subtypes; 

    if (!dijit.byId("addProcessDlg")) {
        new dijit.Dialog({
            id: "addProcessDlg",
            title: "Add new process",
            content: "\
            <form id='addProcessForm' data-dojo-type='dijit.form.Form'>\
                <p>Select process type: <span id=\"processtype\"></span></p>\
                <p>Enter process name:  <span id=\"processname\"></span></p>\
                <span id=\"seqno\"></span>\
                <div id=cancelProcDlgButton data-dojo-type='dijit.form.Button'\
                        data-dojo-props=\"onClick:\
                            function() {\
                                dijit.byId(\'addProcessDlg\').hide();\
                            }\">\
                    Cancel\
                </div>\
                <div id=addProcDlgButton data-dojo-type='dijit.form.Button'\
                        type='submit'>\
                    Add\
                </div>\
            </form>"
        });
        
        dojo.connect(dijit.byId("addProcessForm"), "onSubmit", addProcess);

        subtypes= new dijit.form.FilteringSelect({
            style: "width: 150px",
            store: mcc.storage.processTypeStorage().store(), 
            searchAttr: "nodeLabel", 
            onChange: function(e) {
                mcc.storage.processTypeStorage().getItem(subtypes.getValue()).
                        then(function (ptype) {
                        
                    // Get the prototypical type to get seq no
                    mcc.storage.processTypeStorage().getItems(
                            {family: ptype.getValue("family")}).then(
                            function (pfam) {

                        var pname = ptype.getValue("nodeLabel");
                        var pseq = pfam[0].getValue("currSeq");
                        dijit.byId("processname").set("value", pname + 
                                " " + pseq);
                        dijit.byId("seqno").set("value", pseq);
                    });
                });
            }
        }, "processtype");

        new dijit.form.TextBox(
            {style: "width: 150px"}, "processname");

        new dijit.form.TextBox(
            {style: "width: 150px", type: "hidden"}, "seqno");

        mcc.storage.processTypeStorage().getItems({}).then(function (items) {
            dijit.byId("processtype").set("value",
                    items[0].getId());
        });
    }
}

// Show dialog for adding processes. Refresh contents according to process type
function showAddProcessDialog() {
    // Restrict query if wildcard host selected
    if (hostTreeItem.treeItem.isType("anyHost")) {
        mcc.storage.processTypeStorage().getItems({name: "api"}).then(
            function (items) {
                dijit.byId("processtype").set("value", items[0].getId());
                dijit.byId("processtype").set("query", {family: "api"});
            });
    } else {
        dijit.byId("processtype").set("query", {});
    }

    // Get value from process type widget, update name and process id
    mcc.storage.processTypeStorage().getItem(
            dijit.byId("processtype").getValue()).then(function (ptype) {

        // Get the prototypical type to get seq no
        mcc.storage.processTypeStorage().getItems(
                {family: ptype.getValue("family")}).then(function (pfam) {

            var pname = ptype.getValue("nodeLabel");
            var pseq = pfam[0].getValue("currSeq");
            dijit.byId("processname").set("value", pname + " " + pseq);                    
            dijit.byId("seqno").set("value", pseq);                    
        });
    });
    dijit.byId("addProcessDlg").show();
}

// Setup the entire host tree view
function hostTreeSetup() {
    // Fetch cluster name, to be used in tree heading
    mcc.storage.clusterStorage().getItem(0).then(function(cluster) {
        hostTreeViewSetup(cluster.getValue("name"));
    });
    hostTreeMenuSetup();
    hostTreeButtonSetup();
    addProcessDlgSetup();
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Host tree definition module initialized");
});
