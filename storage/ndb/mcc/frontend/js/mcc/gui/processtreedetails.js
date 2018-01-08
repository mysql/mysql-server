/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
 ***             Process tree details viewing and manipulation              ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.gui.processtreedetails
 *
 *  Description:
 *      Functions for displaying and editing details for processes and types
 *
 *  External interface: 
 *      mcc.gui.processtreedetails.updateProcessTreeSelectionDetails: Update
 *
 *  External data: 
 *      None
 *
 *  Internal interface:
 *      processTypeSelectionDetailsSetup: Setup widgets for process type
 *      processInstanceSelectionDetailsSetup: Setup widgets for process instance
 *
 *  Internal data:
 *      None
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.gui.processtreedetails");

dojo.require("dijit.form.FilteringSelect");
dojo.require("dijit.form.Button");
dojo.require("dijit.form.ToggleButton");
dojo.require("dijit.form.NumberSpinner");
dojo.require("dijit.form.TextBox");
dojo.require("dijit.form.CheckBox");

dojo.require("mcc.util");
dojo.require("mcc.storage");
dojo.require("mcc.configuration");

/**************************** External interface  *****************************/

mcc.gui.processtreedetails.updateProcessTreeSelectionDetails =
        updateProcessTreeSelectionDetails

/****************************** Implementation  *******************************/

// Get cluster app area
function getAppArea() {
    var waitCondition = new dojo.Deferred();
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        waitCondition.resolve(cluster.getValue("apparea"));
    });
    return waitCondition;
}

// Update details for current tree selection
function updateProcessTreeSelectionDetails() {
    // Get current selection
    var processTreeItem = mcc.gui.getCurrentProcessTreeItem();

    // Check whether simple testing
    getAppArea().then(function (appArea) {

        if (!processTreeItem.treeItem ||
            !mcc.storage.processTreeStorage().isItem(
                    processTreeItem.treeItem)) {
            mcc.util.dbg("No tree item selected");
            dijit.byId("processTreeSelectionDetails").
                    selectChild(dijit.byId("noProcessTreeSelectionDetails"));
        } else if (processTreeItem.treeItem.isType("processtype")) {

            // Focus on the selected tree item
            mcc.gui.processTreeSetPath(["root", "" +
                    processTreeItem.treeItem.getId()]);

            // Update based on corresponding processTypeStore
            var pTypeFam = processTreeItem.storageItem.getValue("family");

            // Setup gui unless already done
            if (!dijit.byId("pt_" + pTypeFam)) {
                processTypeSelectionDetailsSetup(pTypeFam, appArea);
            }

            // Setup process type defaults
            mcc.configuration.typeSetup(processTreeItem.storageItem).then(
                    function () {
                // Loop over all parameters, update widgets
                var parameters= mcc.configuration.getAllPara(pTypeFam);
                for (var i in parameters) {
                    if (mcc.configuration.isHeading(pTypeFam, i)) continue; 
                    if (mcc.configuration.getPara(pTypeFam, null, i, 
                            "visibleType") &&
                            mcc.configuration.visiblePara("gui", appArea, 
                                    pTypeFam, i)) {
                        mcc.util.updateWidgets(function(attr) {
                                return processTreeItem.storageItem.
                                        getValue(attr);
                            },
                            "pt_" + pTypeFam + "_",
                            mcc.configuration.getPara(
                                    pTypeFam, null, i, "attribute"), 
                            mcc.configuration.getPara(
                                    pTypeFam, null, i, "defaultValueType"));
                    }
                }
            });
            // Select the correct outermost and innermost stack pages
            dijit.byId("processTreeSelectionDetails").
                    selectChild(dijit.byId("processTypeSelectionDetails"));
            dijit.byId("processTypeDetails").
                    selectChild(dijit.byId("pt_" + pTypeFam));
        } else if (processTreeItem.treeItem.isType("process")) {
            // Get process type, then all from same family
            mcc.storage.processTypeStorage().getItem(
                    processTreeItem.storageItem.getValue("processtype")).then(
                    function (pType) {
                    // Get types of same family
                mcc.storage.processTypeStorage().getItems(
                        {family: pType.getValue("family")}).then(
                        function (ptypes) {
                    // The first corresponds to the id present in the tree
                    var pTypeItem = ptypes[0];
                    var pTypeFam = pTypeItem.getValue("family");

                    // Focus on the selected tree item
                    mcc.gui.processTreeSetPath(["root", "" + pTypeItem.getId(), 
                            "" + processTreeItem.storageItem.getId()]);

                    // Setup gui unless already done
                    if (!dijit.byId("pi_" + pTypeFam)) {
                        processInstanceSelectionDetailsSetup(pTypeFam);
                    }

                    // Setup defaults
                    mcc.configuration.typeSetup(pTypeItem).then(function () {
                        mcc.configuration.instanceSetup(pTypeFam,
                                processTreeItem.storageItem).then(function () {
                        var id = processTreeItem.storageItem.getId();
                        // Loop over all parameter and update widgets
                        var parameters= mcc.configuration.getAllPara(pTypeFam);
                        for (var i in parameters) {
                            if (mcc.configuration.isHeading(pTypeFam, i)) 
                                continue; 
                            // Use type value if overridden
                            var defaultValue= pTypeItem.getValue(
                                    mcc.configuration.getPara(
                                            pTypeFam, null, i, "attribute"));
                            
                            // Otherwise, use instance default or type default
                            if (defaultValue === undefined) {
                                defaultValue= mcc.configuration.getPara(
                                        pTypeFam, id, i, 
                                        "defaultValueInstance");
                                if (defaultValue === undefined) {
                                    defaultValue= mcc.configuration.getPara(
                                            pTypeFam, null, i, 
                                            "defaultValueType");
                                } 
                            }
                            
                            // Update if visible at this config level
                            if (mcc.configuration.getPara(pTypeFam, null, i,
                                    "visibleInstance") &&
                                    mcc.configuration.visiblePara(
                                            "gui", appArea, pTypeFam, i)) {
                                mcc.util.updateWidgets(function(attr) {
                                        return processTreeItem.
                                                storageItem.getValue(attr);
                                    },
                                    "pi_" + pTypeFam + "_",
                                    mcc.configuration.getPara(
                                            pTypeFam, null, i, "attribute"), 
                                    defaultValue);
                            }
                        }
                    });
                    });

                    // Select the correct outermost and innermost stack pages
                    dijit.byId("processTreeSelectionDetails").
                            selectChild(dijit.byId(
                                    "processInstanceSelectionDetails"));
                    dijit.byId("processInstanceDetails").
                            selectChild(dijit.byId("pi_" + pTypeFam));
                });
            });
        }
    });
}

// Setup widgets for the given process type
function processTypeSelectionDetailsSetup(processType, appArea) {

    // Add a new content pane to the stack
    var cp = new dijit.layout.ContentPane({id: "pt_" + processType});
    dijit.byId("processTypeDetails").addChild(cp); 

    // Header for the process type details unless already defined
    if (!dijit.byId("processTypeDetailsHeader")) {
        var processTypeDetailsHeader= new dojox.grid.DataGrid({
                structure: [{
                name: 'Process type property',
                width: '28%'
            },
            {
                name: 'Value',
                width: '60%'
            },
            {
                name: 'Override',
                width: '12%'
            }],
        }, "processTypeDetailsHeader");
        processTypeDetailsHeader.startup();
    }

    // Setup html contents of the details pane
    var contentString = mcc.util.startTable();
    for (var i in mcc.configuration.getAllPara(processType)) {
        if (mcc.configuration.getPara(processType, null, i, "visibleType") &&
            mcc.configuration.visiblePara("gui", appArea, processType, i)) {
            contentString += mcc.util.tableRow("pt_" + processType + "_",
                    mcc.configuration.getPara(processType, null, i, "label"), 
                    mcc.configuration.getPara(processType, null, i, "docurl"), 
                    mcc.configuration.getPara(processType, null, i, 
                            "attribute"),
                    mcc.configuration.getPara(processType, null, i, "tooltip"));
        }
    }
    contentString += mcc.util.endTable();
    cp.setContent(contentString);

    // Setup widgets of the details pane
    for (var i in mcc.configuration.getAllPara(processType)) {
        if (mcc.configuration.getPara(processType, null, i, "visibleType") &&
            mcc.configuration.visiblePara("gui", appArea, processType, i) &&
            !mcc.configuration.isHeading(processType, i)) {
            mcc.util.setupWidgets(function(attr, val) {
                    mcc.gui.getCurrentProcessTreeItem().
                            storageItem.setValue(attr, val);
                    mcc.storage.processTypeStorage().save();
                    updateProcessTreeSelectionDetails();
                },
                function(attr) {
                    mcc.gui.getCurrentProcessTreeItem().
                            storageItem.deleteAttribute(attr);
                    mcc.storage.processTypeStorage().save();
                    updateProcessTreeSelectionDetails();
                }, 
                "pt_" + processType + "_",
                mcc.configuration.getPara(processType, null, i, "attribute"), 
                mcc.configuration.getPara(processType, null, i, "widget"), 
                mcc.configuration.getPara(processType, null, i, "width"), 
                mcc.configuration.getPara(processType, null, i, 
                        "overridableType"),
                mcc.configuration.getPara(processType, null, i, "tooltip"),
                mcc.configuration.getPara(processType, null, i, "constraints"));
        }
    }
}

// Setup widgets for process instances of a given type
function processInstanceSelectionDetailsSetup(processType, appArea) {

    // Add a new content pane to the stack
    var cp= new dijit.layout.ContentPane({id: "pi_" + processType});
    dijit.byId("processInstanceDetails").addChild(cp); 

    // Header for the process instance details unless already defined
    if (!dijit.byId("processInstanceDetailsHeader")) {
        var processInstanceDetailsHeader= new dojox.grid.DataGrid({
                structure: [{
                name: 'Process property',
                width: '28%'
            },
            {
                name: 'Value',
                width: '60%'
            },
            {
                name: 'Override',
                width: '12%'
            }],
        }, "processInstanceDetailsHeader");
        processInstanceDetailsHeader.startup();
    }

    // Setup html contents of the details pane
    var contentString= mcc.util.startTable();

    for (var i in mcc.configuration.getAllPara(processType)) {
        if (mcc.configuration.getPara(processType, null, i, "visibleInstance") 
                && mcc.configuration.visiblePara("gui", appArea, processType, i)
                ) {
            contentString += mcc.util.tableRow("pi_" + processType + "_",
                    mcc.configuration.getPara(processType, null, i, "label"), 
                    mcc.configuration.getPara(processType, null, i, "docurl"), 
                    mcc.configuration.getPara(processType, null, i, 
                            "attribute"),
                    mcc.configuration.getPara(processType, null, i, "tooltip"));
        }
    }
    contentString += mcc.util.endTable();
    cp.setContent(contentString);

    // Setup widgets of the details pane
    for (var i in mcc.configuration.getAllPara(processType)) {
        if (mcc.configuration.getPara(processType, null, i, "visibleInstance") 
                && mcc.configuration.visiblePara("gui", appArea, processType, i)
                && !mcc.configuration.isHeading(processType, i)) {
            mcc.util.setupWidgets(function(attr, val) {
                    mcc.gui.getCurrentProcessTreeItem().
                            storageItem.setValue(attr, val);
                    mcc.storage.processStorage().save();
                    updateProcessTreeSelectionDetails();
                },
                function(attr) {
                    mcc.gui.getCurrentProcessTreeItem().
                            storageItem.deleteAttribute(attr);
                    mcc.storage.processStorage().save();
                    updateProcessTreeSelectionDetails();
                }, 
                "pi_" + processType + "_",
                mcc.configuration.getPara(processType, null, i, "attribute"), 
                mcc.configuration.getPara(processType, null, i, "widget"), 
                mcc.configuration.getPara(processType, null, i, "width"), 
                mcc.configuration.getPara(processType, null, i, 
                        "overridableInstance"),
                mcc.configuration.getPara(processType, null, i, "tooltip"),
                mcc.configuration.getPara(processType, null, i, "constraints")
            );
        }
    }
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Process tree selection details module initialized");
});

