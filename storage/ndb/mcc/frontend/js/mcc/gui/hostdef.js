/*
Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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
 ***                              Host definition                           ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.gui.hostdef
 *
 *  Description:
 *      Show and edit host details such as HW resource information
 *
 *  External interface:
 *      mcc.gui.hostdef.hostGridSetup: Setup the grid, connect to storage
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      addHostsDialogSetup: Setup a dialog for adding new hosts
 *      editHostsDialogSetup: Setup a dialog for editing selected hosts
 *      addHostList: Split hostlist, add individual hosts. Check if host exists
 *      saveSelectedHosts: Save selected hosts after editing
 *      getFieldTT: Get tooltip text for a specific field
 *
 *  Internal data: 
 *      hostGrid: The data grid connected to the host storage
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.gui.hostdef");

dojo.require("dojox.grid.DataGrid");
dojo.require("dojo.DeferredList");
dojo.require("dijit.form.Form");
dojo.require("dijit.form.NumberSpinner");
dojo.require("dijit.form.TextBox");
dojo.require("dojox.grid.cells.dijit");
dojo.require("dijit.Dialog");
dojo.require("dijit.Tooltip");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.gui.hostdef.hostGridSetup = hostGridSetup;

/******************************* Internal data ********************************/

var hostGrid = null;

/****************************** Implementation  *******************************/

// Split the hostlist, add individual hosts. Check if host exists
function addHostList(event) {
    var hosts= dijit.byId('hostlist').getValue().split(",");
    var valid= true; 
    
    // Prevent default submit handling
    dojo.stopEvent(event);

    // Strip leading/trailing spaces, check multiple occurrences
    for (var i in hosts) {
        hosts[i]= hosts[i].replace(/^\s*/, "").replace(/\s*$/, "");
        if (hosts[i].length > 0 && dojo.indexOf(hosts, hosts[i]) != i) {
            alert("Hostname '" + hosts[i] + "' is entered more than once");
            return;
        }
    }

    // Loop over all new hosts and check if name already exists
    var waitConditions = [];
    for (var i in hosts) {
        waitConditions[i] = new dojo.Deferred();
        // Fetch from hostStore to see if the host exists
        (function (host, waitCondition) {
            mcc.storage.hostStorage().getItems({name: host, 
                    anyHost: false}).then(
                    function (items) {
                if (items && items.length > 0) {
                    alert("Host '" + host + "' already exists");
                    waitCondition.resolve(false);
                } else {
                    waitCondition.resolve(true);
                }
            });
        })(hosts[i], waitConditions[i]);
    }
    
    // Wait for all hosts to be checked - add all or none
    var waitList = new dojo.DeferredList(waitConditions);
    waitList.then(function (res) {
        if (res) {
            for (var i in res) {
                if (!res[i][1]) {
                    return;
                }
            }
            // If we haven't returned already, all can be added
            for (var i in hosts) {
                if (hosts[i].length == 0) continue;
                mcc.storage.hostStorage().newItem({
                    name: hosts[i],
                    anyHost: false
                }, true);
            }
            dijit.byId('addHostsDlg').hide();
        }
    });
}

// Setup a dialog for adding new hosts
function addHostsDialogSetup() {
    var addHostsDlg = null; 
    var hostlist = null;
    // Create the dialog if it does not already exist
    if (!dijit.byId("addHostsDlg")) {
        addHostsDlg= new dijit.Dialog({
            id: "addHostsDlg",
            title: "Add new hosts",
            content: "\
                <form id='addHostsForm' data-dojo-type='dijit.form.Form'>\
                    <p>\
                        Host name(s): \
                        <span class=\"helpIcon\" id=\"hostlist_qm\">[?]</span>\
                        <span id='hostlist'></span>\
                    </p>\
                    <div data-dojo-type='dijit.form.Button' \
                        data-dojo-props=\"onClick:\
                            function() {\
                                dijit.byId(\'hostlist\').setValue(\'\');\
                                dijit.byId(\'addHostsDlg\').hide();\
                            }\">Cancel</div> \
                    \
                    <div data-dojo-type='dijit.form.Button' type='submit'\
                        id='saveHosts'>Add\
                    </div>\
                </form>"
        });
        // Must connect outside of html to be in scope of function
        dojo.connect(dijit.byId("addHostsForm"), "onSubmit", addHostList);
        var hostlist= new dijit.form.TextBox({style: "width: 250px"}, "hostlist");
        var hostlist_tt = new dijit.Tooltip({
            connectId: ["hostlist", "hostlist_qm"],
            label: "Comma separated list of names or ip addresses of \
                    additional hosts to use for running MySQL cluster"
        });
    }
}

// Update and save the selected hosts after editing
function saveSelectedHosts(event) {
    // Prevent default submit handling
    dojo.stopEvent(event);

    var selection = hostGrid.selection.getSelected();
    if (selection && selection.length > 0) {
        for (var i in selection) {
            var uname = mcc.storage.hostStorage().store().
                    getValue(selection[i], "uname");
            if (dijit.byId("uname").getValue()) {
                uname = dijit.byId("uname").getValue();
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "uname", uname);
                mcc.util.dbg("Updating uname, check predef dirs");
                // If we changed platform, we need to update predef dirs
                if (mcc.storage.hostStorage().store().getValue(selection[i], 
                        "installdir_predef") == true) {
                    var dir = mcc.storage.hostStorage().
                            getPredefinedDirectory(uname, "installdir");
                    mcc.util.dbg("Update predfined installdir to " + dir);
                    mcc.storage.hostStorage().store().setValue(selection[i], 
                        "installdir", dir);
                }
                if (mcc.storage.hostStorage().store().getValue(selection[i], 
                        "datadir_predef") == true) {
                    var dir = mcc.storage.hostStorage().
                            getPredefinedDirectory(uname, "datadir");
                    mcc.util.dbg("Update predfined datadir to " + dir);
                    mcc.storage.hostStorage().store().setValue(selection[i], 
                        "datadir", dir);
                }
            }
            if (dijit.byId("ram").getValue()) {
                var val = dijit.byId("ram").getValue();
                if (val > 0 && val < 1000000) {
                    mcc.storage.hostStorage().store().setValue(selection[i], 
                        "ram", val);
                }
            }
            if (dijit.byId("cores").getValue()) {
                var val = dijit.byId("cores").getValue();
                if (val > 0 && val < 100) {
                    mcc.storage.hostStorage().store().setValue(selection[i], 
                        "cores", val);
                }
            }
            if (dijit.byId("installdir").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "installdir", 
                    mcc.util.terminatePath(dijit.byId("installdir").
                            getValue()));
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "installdir_predef", false);
            }
            if (dijit.byId("datadir").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "datadir", 
                    mcc.util.terminatePath(dijit.byId("datadir").getValue()));
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "datadir_predef", false);
            }
        }
        mcc.storage.hostStorage().save();
    }
    dijit.byId("editHostsDlg").hide();
}

// Get tooltip texts for various host attributes
function getFieldTT(field) {
    var fieldTT = {
        name: "<i>Host</i> is the name or ip address of the host as " +
                    "known by the operating system",
        hwResFetch: "<i>Resource info</i> indicates the status of automatic " +
                    "retrieval of hardware resource information. <i>N/A</i> " +
                    "means that automatic fetching of information is turned " +
                    "off, <i>Fetching</i> means that a request for " +
                    "information has been sent to the host, <i>OK</i> means " +
                    "that information has been fetched, and <i>Failed</i> " +
                    "means that information could not be obtained",
        uname: "<i>Platform</i> is given by the type of hardware, " +
                    "operating system and system software running on the host",
        ram: "<i>Memory</i> is the size of the internal memory of " +
                    "the host, expressed in <b>M</b>ega<b>B</b>ytes",
        cores: "<i>CPU cores</i> is the number of cores of host's CPU. " +
                    "A multi core CPU can do several things simultaneously.",
        installdir: "<i>Installation directory</i> is the directory where " +
                "the MySQL Cluster software is installed. ",
        datadir: "<i>Data directory</i> is the directory for storing " +
                "data, log files, etc. for MySQL Cluster. Data directories " +
                "for individual processes are defined automatically by " +
                "appending process ids to this root path. If you want to " +
                "have different data directories for different processes, " +
                "this can be overridden for each process later in this wizard."
    }
    return fieldTT[field];
}

// Setup a dialog for editing hosts
function editHostsDialogSetup() {
    var editHostsDlg = null; 
    // Create the dialog if it does not already exist
    if (!dijit.byId("editHostsDlg")) {
        editHostsDlg= new dijit.Dialog({
            id: "editHostsDlg",
            title: "Edit selected host(s)",
            style: {width: "620px"},
            content: "\
                <form id='editHostsForm' data-dojo-type='dijit.form.Form'>\
                    <p>Please edit the fields you want to change. The changes \
                        will be applied to all selected hosts. Fields that \
                        are not edited in the form below will be left \
                        unchanged.\
                    </p>\
                    <p>\
                    <table>\
                        <tr>\
                            <td>Platform \
                                <span class=\"helpIcon\"\
                                    id=\"uname_qm\">[?]\
                                </span>\
                            </td>\
                            <td>Memory (MB) \
                                <span class=\"helpIcon\"\
                                    id=\"ram_qm\">[?]\
                                </span>\
                            </td>\
                            <td>CPU cores \
                                <span class=\"helpIcon\"\
                                    id=\"cores_qm\">[?]\
                                </span>\
                            </td>\
                            <td>MySQL Cluster install directory \
                                <span class=\"helpIcon\"\
                                    id=\"installdir_qm\">[?]\
                                </span>\
                            </td>\
                            <td>MySQL Cluster data directory \
                                <span class=\"helpIcon\"\
                                    id=\"datadir_qm\">[?]\
                                </span>\
                            </td>\
                        </tr>\
                        <tr>\
                            <td><span id='uname'></span></td>\
                            <td><span id='ram'></span></td>\
                            <td><span id='cores'></span></td>\
                            <td><span id='installdir'></span></td>\
                            <td><span id='datadir'></span></td>\
                        </tr>\
                    </table>\
                    </p>\
                    <div data-dojo-type='dijit.form.Button' \
                        data-dojo-props=\"onClick:\
                            function() {\
                                dijit.byId(\'editHostsDlg\').hide();\
                            }\">Cancel</div> \
                    \
                    <div data-dojo-type='dijit.form.Button' type='submit'\
                        id='saveSelection'>Save\
                    </div>\
                </form>"
        });
        // Must connect outside of html to be in scope of function
        dojo.connect(dijit.byId("editHostsForm"), "onSubmit", 
                saveSelectedHosts);

        // Define widgets
        var uname = new dijit.form.TextBox({style: "width: 60px"}, "uname");
        var ram = new dijit.form.NumberSpinner({
            style: "width: 80px",
            constraints: {min: 1, max: 1000000, places: 0}
        }, "ram");
        var cores = new dijit.form.NumberSpinner({
            style: "width: 80px",
            constraints: {min: 1, max: 100, places: 0, format:"####"}
        }, "cores");
        var installdir = new dijit.form.TextBox({style: "width: 170px"}, 
                "installdir");
        var datadir = new dijit.form.TextBox({style: "width: 170px"}, 
                "datadir");

        // Define tooltips
        var uname_tt = new dijit.Tooltip({
            connectId: ["uname_qm"],
            label: getFieldTT("uname")
        });
        var ram_tt = new dijit.Tooltip({
            connectId: ["ram_qm"],
            label: getFieldTT("ram")
        });
        var cores_tt = new dijit.Tooltip({
            connectId: ["cores_qm"],
            label: getFieldTT("cores")
        });
        var installdir_tt = new dijit.Tooltip({
            connectId: ["installdir_qm"],
            label: getFieldTT("installdir")
        });
        var datadir1_tt = new dijit.Tooltip({
            connectId: ["datadir_qm"],
            label: getFieldTT("datadir")
        });
   }
}

// Setup the host grid with support for adding, deleting and editing hosts
function hostGridSetup() {
    
    if (!dijit.byId("hostGrid")) {
        mcc.util.dbg("Setup host definition widgets");
    } else {
        return;
    }

    // Button for adding a host. Show add hosts dialog on click
    var addButton= new dijit.form.Button({
        label: "Add host",
        iconClass: "dijitIconAdd"
    }, "addHostsButton");
    dojo.connect(addButton, "onClick", function () {
        dijit.byId("hostlist").setValue("");
        dijit.byId("addHostsDlg").show();
    });

    // Button for removing a host. Connect to storeDeleteHost function */
    var removeButton= new dijit.form.Button({
        label: "Remove selected host(s)",
        iconClass: "dijitIconDelete"
    }, "removeHostsButton");
    dojo.connect(removeButton, "onClick", function() {
        var selection = hostGrid.selection.getSelected();
        if (selection && selection.length > 0) {
            // Get the row index of the last item
            var lastIdx = 0;
            for (var i in selection) {
                lastIdx = hostGrid.getItemIndex(selection[i]);
                mcc.storage.hostStorage().deleteItem(selection[i]);
            }
            // If there is an item at lastIdx, select it, otherwise select first
            if (hostGrid.getItem(lastIdx)) {
                hostGrid.selection.setSelected(lastIdx, true);
            } else if (lastIdx > 0) {
                hostGrid.selection.setSelected(lastIdx - 1, true);
            }
        }   
    });

    // Button for adding a host. Show add hosts dialog on click
    var editButton= new dijit.form.Button({
        label: "Edit selected host(s)",
        iconClass: "dijitIconEdit"
    }, "editHostsButton");
    dojo.connect(editButton, "onClick", function () {
        dijit.byId("uname").setValue(null);
        dijit.byId("ram").setValue(null);
        dijit.byId("cores").setValue(null);
        dijit.byId("installdir").setValue(null);
        dijit.byId("datadir").setValue(null);
        dijit.byId("editHostsDlg").show();
    });

    // Layout for the host grid
    var hostGridDefinitions = [{
            width: '8%',
            field: "name", 
            editable: false,
            name: "Host"
        },
        {
            width: '11%',
            field: "hwResFetch", 
            editable: false,
            name: "Resource info"
        },
        {
            width: '8%',
            field: "uname", 
            editable: true,
            name: "Platform"
        },
        {
            width: '11%',
            field: "ram",
            name: "Memory (MB)",
            editable: true,
            type: dojox.grid.cells._Widget,
            widgetClass: "dijit.form.NumberSpinner", 
            constraint: {min: 1, max: 1000000, places: 0}
        },
        {
            width: '9%',
            field: "cores",
            name: "CPU cores",
            editable: true,
            type: dojox.grid.cells._Widget,
            widgetClass: "dijit.form.NumberSpinner", 
            constraint: {min: 1, max: 100, places: 0}
        },
        {
            width: '26%',
            field: "installdir",
            name: "MySQL Cluster install directory",
            editable: true
        },
        {
            width: '26%',
            field: "datadir",
            name: "MySQL Cluster data directory",
            editable: true
    }];

    // Check number within range
    function isIntegerRange(n, min, max) {
        return !isNaN(n) && n % 1 == 0 && n >= min && n <= max;
    }

    // Validate user input. Needed if user types illegal values
    function applyCellEdit(inValue, inRowIndex, inAttrName) {
        var revert= false;
        for (var i in hostGridDefinitions) {
            if (inAttrName == hostGridDefinitions[i].field) {
                column= hostGridDefinitions[i];
                if (column.constraint && 
                    !isIntegerRange(inValue, column.constraint.min, 
                            column.constraint.max)) {
                    revert= true;
                }
                // Possibly add other checks as well
                mcc.storage.hostStorage().getItem(
                        hostGrid._by_idx[inRowIndex].idty).
                        then(function (host) {
                    var value = inValue;
                    if (revert) {
                        value = host.getValue(inAttrName); 
                    }
                    // If updating directories, set predef flag to false
                    if (inAttrName == "installdir" || inAttrName == "datadir") {
                        mcc.util.dbg("Overriding " + inAttrName + ", set flag");
                        host.setValue(inAttrName + "_predef", false);
                        value = mcc.util.terminatePath(value);
                    }
                    // If updating platform, update predef dirs too
                    if (inAttrName == "uname") {
                        mcc.util.dbg("Update platform, check predef dirs");
                        if (host.getValue("installdir_predef") == true) {
                            var dir = mcc.storage.hostStorage().
                                    getPredefinedDirectory(inValue, 
                                            "installdir");
                            mcc.util.dbg("Update predfined installdir to " + 
                                    dir);
                            host.setValue("installdir", dir);
                        }
                        if (host.getValue("datadir_predef") == true) {
                            var dir = mcc.storage.hostStorage().
                                    getPredefinedDirectory(inValue, 
                                            "datadir");
                            mcc.util.dbg("Update predfined datadir to " + 
                                    dir);
                            host.setValue("datadir", dir);
                        }
                    }
                    host.setValue(inAttrName, value);
                    mcc.storage.hostStorage().save();
                    hostGrid.onApplyCellEdit(value, inRowIndex, inAttrName);
                });
                break;
            }
        }
    }

    // Define the host grid, don't show wildcard host
    hostGrid= new dojox.grid.DataGrid({
        autoHeight: true,
        query: {anyHost: false},
        queryOptions: {},
        canSort: function(col) {return false},
        store: mcc.storage.hostStorage().store(),
        singleClickEdit: true,
        doApplyCellEdit: applyCellEdit,
        doStartEdit: function (cell, idx) {
            // Avoid inheriting previously entered value as default
            cell.widget=null; hostGrid.onStartEdit(cell, idx);
        },
        onBlur: function () {
            // Apply unsaved edits if leaving page
            if (hostGrid.edit.isEditing()) {
                mcc.util.dbg("Applying unsaved edit...");
                hostGrid.edit.apply();
            }
        },
        structure: hostGridDefinitions
    }, "hostGrid");

    function showTT(event) {
        var editMsg = null;
        var colMsg = getFieldTT(event.cell.field);

        if (event.cell.editable) {
            editMsg = "Click the cell for editing, or select one or more rows\
                and press the edit button below the grid.";
        } else {
            editMsg = "This cell cannot be edited.";
        }

        if (event.rowIndex < 0) {
            msg = colMsg;
        } else {
            msg = editMsg;
            if (!event.grid.store.getValue(event.grid.getItem(event.rowIndex), 
                    event.cell.field)) {
                msg = "Ellipsis ('...') means the value was not retrieved " + 
                        "automatically. " + msg;
            }
        }

        if (msg) {
            dijit.showTooltip(msg, event.cellNode, ["after"]);
        }
    };

    function hideTT(event) {
        dijit.hideTooltip(event.cellNode);
    }; 
    
    dojo.connect(hostGrid, "onCellClick", function(e) {
        if (e.cell.field == "hwResFetch") {
            mcc.storage.hostStorage().getItem(hostGrid._by_idx[e.rowIndex].idty).then(function (host) {
                var errMsg = host.getValue("errMsg");
                if (errMsg) {
                    alert("Error: " + errMsg);                    
                }
            });
        }
    });

    dojo.connect(hostGrid, "onCellMouseOver", showTT);
    dojo.connect(hostGrid, "onCellMouseOut", hideTT); 
    dojo.connect(hostGrid, "onHeaderCellMouseOver", showTT);
    dojo.connect(hostGrid, "onHeaderCellMouseOut", hideTT); 

    var hostgrid_tt = new dijit.Tooltip({
        connectId: ["hostGrid"],
        label: "Click a cell in the grid to edit, or select one or more rows\
                and press the edit button below the grid. All cells except \
                <i>Host</i> and <i>Resource info</i> are editable."
    });

    hostGrid.startup();
    addHostsDialogSetup();
    editHostsDialogSetup();
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Host definition module initialized");
});


