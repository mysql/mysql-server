/*
Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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
 ***                   Process tree manipulation functions                  ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.gui.processtree
 *
 *  Description:
 *      Tree model, widget and manipulation functions
 *
 *  External interface: 
 *      mcc.gui.processtree.processTreeSetPath: Set path to selected tree node
 *      mcc.gui.processtree.processTreeSetup: Setup the entire process tree view
 *      mcc.gui.processtree.getCurrentProcessTreeItem: Get current selection
 *      mcc.gui.processtree.resetProcessTreeItem: Reset the selected item
 *
 *  External data: 
 *      None
 *
 *  Internal interface:
 *      getStorageItem: Wrap tree/store item into storage item for convenience
 *      processTreeOnMouseDown: Set selected item when the mouse is pressed
 *      processTreeGetIconClass: Return the tree node's corresponding icon
 *      processTreeViewSetup: Setup the process tree and model
 *
 *  Internal data: 
 *      processTree: The process tree
 *      treeExpanded: Keep track of initial expansion of entire tree
 *      processTreeItem: Selected tree node with related storage items
 *      
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.gui.processtree");

dojo.require("dijit.Tree");
dojo.require("dojox.grid.DataGrid");
dojo.require("dijit.tree.ForestStoreModel");
dojo.require("dijit.Menu");
dojo.require("dijit.MenuItem");
dojo.require("dijit.form.Button");
dojo.require("dijit.form.TextBox");
dojo.require("dijit.form.CheckBox");
dojo.require("dijit.form.FilteringSelect");
dojo.require("dijit.form.NumberSpinner");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.gui.processtree.processTreeSetPath = processTreeSetPath;
mcc.gui.processtree.getCurrentProcessTreeItem = getCurrentProcessTreeItem;
mcc.gui.processtree.resetProcessTreeItem = resetProcessTreeItem;
mcc.gui.processtree.processTreeSetup = processTreeSetup;

/******************************* Internal data ********************************/

var processTree = null;
var treeExpanded = false; 

// Keep track of the selected item in the tree
var processTreeItem= {
    treeNode: null,
    treeItem: null,
    storageItem: null,
    dndSourceItem: null
};

// Wrap a tree or store item into a storage item for convenience
function getStorageItem(item) {
    return new mcc.storage.processTreeStorage().StorageItem(
            mcc.storage.processTreeStorage(), item);
}

// Get current selection
function getCurrentProcessTreeItem() {
    return processTreeItem;
}
// Reset the selected item to undefined
function resetProcessTreeItem() {
    processTreeItem.treeItem = null; 
    processTreeItem.treeNode = null; 
    processTreeItem.storageItem = null; 
    processTreeItem.dndSourceItem = null; 
}

// Set selected item when the mouse is pressed
function processTreeOnMouseDown(event) {
    resetProcessTreeItem();
    processTree.selectedNode= dijit.getEnclosingWidget(event.target);
    processTreeItem.treeNode= processTree.selectedNode;

    if (processTree.selectedNode.item) {
        processTreeItem.treeItem= processTree.selectedNode.item;
    } 

    if (processTree.selectedNode.item && !processTree.selectedNode.item.root) {
        processTreeItem.treeItem = getStorageItem(
                processTree.selectedNode.item);
        // Fetch the storage item
        if (processTreeItem.treeItem.isType("processtype")) {
            mcc.storage.processTypeStorage().getItem(
                    processTreeItem.treeItem.getId()).
                    then(function (processtype) {
                    processTreeItem.storageItem = processtype;
                });
        } else if (processTreeItem.treeItem.isType("process")) {
            mcc.storage.processStorage().getItem(
                    processTreeItem.treeItem.getId()).
                    then(function (process) {
                    processTreeItem.storageItem = process;
                });
        }
    }

    // Update view of selection details
    mcc.gui.updateProcessTreeSelectionDetails();
}

// Set path to node selection
function processTreeSetPath(path) {
    processTree.set("path", path);
}

// Return the appropriate icon depending on tree node and state
function processTreeGetIconClass(item, opened) {
    if (!item || item.root) {
        return (opened ? "dijitFolderOpened" : "dijitFolderClosed");
    }
    return (mcc.storage.processTreeStorage().store().
            getValue(item, "type") == "processtype") ? 
            (opened ? "dijitFolderOpened" : "dijitFolderClosed") : 
                    "dijitIconFunction";
}

// Setup the process tree and model
function processTreeViewSetup(clusterName) {
    var processTreeModel = new dijit.tree.ForestStoreModel({
        store: mcc.storage.processTreeStorage().store(),
        query: {},
        rootId: "root",
        rootLabel: clusterName,
        mayHaveChildren: function (item) {
            if (item.root || getStorageItem(item).isType("processtype")) {
                return true;
            }
            return false;
        },
        // We need to override getChildren to collect instances of same family
        getChildren: function (treeitem, onChildren) {
            var children= [];
            var storageItem = getStorageItem(treeitem);
            if (treeitem.root) {
                var familymap= [];
                mcc.storage.processTreeStorage().forItems({type: "processtype"},
                    function (ptype) {
                        if (!familymap[ptype.getValue("name")]) {
                            familymap[ptype.getValue("name")] = ptype.getId();
                            children.push(ptype.item);
                        }
                    },
                    function () {
                        onChildren(children);
                    }
                );
            } else if (storageItem.isType("processtype")) {
                // Get all ptypes of the same family
                mcc.storage.processTreeStorage().forItems({
                    type: "processtype",
                    name: storageItem.getValue("name")
                },
                function (ptype) {
                    children = children.concat(ptype.getValues("processes"));
                },
                function () {
                    onChildren(children);
                });
            }
        }
    });

    // A dummy data grid to get a properly formatted header
    var treeViewHeaderGrid= new dojox.grid.DataGrid({
        structure: [{
            name: clusterName + " processes",
            width: "100%"
        }]
    }, "processTreeHeader");
    treeViewHeaderGrid.startup();

    // The tree widget itself with model and overriding functions
    processTree= new dijit.Tree({
        model: processTreeModel,
        showRoot: false, 
        id: "processTree",
        getIconClass: processTreeGetIconClass,
        onMouseDown: processTreeOnMouseDown,
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
    }, "processTree");
    processTree.startup();

    // Expand entire tree first time it is displayed
    if (!treeExpanded) {
        processTree.expandAll();
        treeExpanded = true;
    }
}

// Setup a check box to toggle advanced mode. Also update the omnipresent menu
function advancedModeCheckBoxSetup() {    
    var advancedModeBox = new dijit.form.CheckBox({
        label: "Show advanced configuration options",
        checked: (mcc.util.getCookie("configLevel") == "advanced"),
        onChange: function(val) { 
            if (val) {
                mcc.util.setCookie("configLevel", "advanced");
                dijit.byId("advancedModeMenuItem").set("checked", true);
            } else {
                mcc.util.setCookie("configLevel", "simple");
                dijit.byId("advancedModeMenuItem").set("checked", false);
            }
            mcc.gui.reloadPage();
        }
    }, "advancedModeBox");
}

// Setup the process tree with the cluster name as heading
function processTreeSetup() {
    // Fetch cluster name, to be used in tree heading
    mcc.storage.clusterStorage().getItem(0).then(function(cluster) {
        processTreeViewSetup(cluster.getValue("name"));
        advancedModeCheckBoxSetup();
    });
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Process tree definition module initialized");
});
