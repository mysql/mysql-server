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
// dojo.require("dijit/form/SimpleTextarea");

/**************************** External interface  *****************************/

mcc.gui.hostdef.hostGridSetup = hostGridSetup;

/******************************* Internal data ********************************/

var hostGrid = null;

/****************************** Implementation  *******************************/

// Split the hostlist, add individual hosts. Check if host exists
function addHostList(event) {
    var hosts= dijit.byId('hostlist').getValue().split(",");
    var valid= true; 
    var newHostName = "";
    
    // Prevent default submit handling
    dojo.stopEvent(event);

    if (hosts.length <= 0) {
        mcc.util.dbg("Nothing in HOSTLIST");
        return
    }

    // Exclude localhost AND 127.0.0.1 if both in list.
    if (hosts.indexOf("localhost") >= 0 && hosts.indexOf("127.0.0.1") >= 0) {
        alert("localhost is already in the list!");
        return;
    }

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
            mcc.storage.hostStorage().getItems({name: host, anyHost: false}).then(
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
                newHostName = "";
                if (hosts[i].length == 0) continue;
                mcc.util.dbg("Adding new host " + hosts[i]);
                newHostName = hosts[i];
                mcc.storage.hostStorage().newItem({
                    name: hosts[i],
                    anyHost: false
                }, true);
                // Save credentials:
                var test = newHostName;
                mcc.storage.hostStorage().getItems({name: test, anyHost: false}).then(function (nhost){
                    mcc.util.dbg("Updating credentials for new host " + nhost[0].getValue("name"));
                    if (dijit.byId("sd_key_auth").get("checked")) {
                        nhost[0].setValue("usr", null);
                        nhost[0].setValue("usrpwd", null);
                        nhost[0].setValue("key_usr", dijit.byId("sd_key_usr").getValue());
                        nhost[0].setValue("key_passp", dijit.byId("sd_key_passp").getValue());
                        nhost[0].setValue("key_file", dijit.byId("sd_key_file").getValue());
                    } else {
                        nhost[0].setValue("usr", dijit.byId("sd_usr").getValue());
                        nhost[0].setValue("usrpwd", dijit.byId("sd_usrpwd").getValue());
                        nhost[0].setValue("key_usr", null);
                        nhost[0].setValue("key_passp", null);
                        nhost[0].setValue("key_file", null);
                    }
                    nhost[0].setValue("key_auth", dijit.byId("sd_key_auth").get("checked"));
                    if (dijit.byId("sd_IntIP").getValue())
                        nhost[0].setValue("internalIP", dijit.byId("sd_IntIP").get("value"));
                    mcc.util.dbg("Saving credentials for new host " + nhost[0].getValue("name"));
                    mcc.storage.hostStorage().save();
                    mcc.storage.getHostResourceInfo(
                      nhost[0].getValue("name"), nhost[0].getValue("id"), true, false);
                    //Clean up:
                    dijit.byId("sd_usr").setValue(null);
                    dijit.byId("sd_usrpwd").setValue(null);
                    dijit.byId("sd_key_usr").setValue(null);
                    dijit.byId("sd_key_passp").setValue(null);
                    dijit.byId("sd_key_file").setValue(null);
                });
            }
            dijit.byId('addHostsDlg').hide();
        }
    });
}

function updateHostAuth() {
    if (dijit.byId("sd_key_auth").get("checked")) {
        dijit.byId("sd_usr").set("disabled", true);
        dijit.byId("sd_usr").setValue(null);
        dijit.byId("sd_usrpwd").set("disabled", true);
        dijit.byId("sd_usrpwd").setValue(null);
        dijit.byId("sd_key_usr").set("disabled", false);
        dijit.byId("sd_key_passp").set("disabled", false);
        dijit.byId("sd_key_file").set("disabled", false);
    } else {
        dijit.byId("sd_usr").set("disabled", false);
        dijit.byId("sd_usrpwd").set("disabled", false);
        dijit.byId("sd_key_usr").set("disabled", true);
        dijit.byId("sd_key_usr").setValue(null);
        dijit.byId("sd_key_passp").set("disabled", true);
        dijit.byId("sd_key_passp").setValue(null);
        dijit.byId("sd_key_file").set("disabled", true);
        dijit.byId("sd_key_file").setValue(null);
    }
}

// Same for EDIT dialog
function updateHostAuthE() {
    if (dijit.byId("sd_key_authedit").get("checked")) {
        dijit.byId("sd_usredit").set("disabled", true);
        dijit.byId("sd_usredit").setValue(null);
        dijit.byId("sd_usrpwdedit").set("disabled", true);
        dijit.byId("sd_usrpwdedit").setValue(null);
        dijit.byId("sd_key_usredit").set("disabled", false);
        dijit.byId("sd_key_passpedit").set("disabled", false);
        dijit.byId("sd_key_fileedit").set("disabled", false);
    } else {
        dijit.byId("sd_usredit").set("disabled", false);
        dijit.byId("sd_usrpwdedit").set("disabled", false);
        dijit.byId("sd_key_usredit").set("disabled", true);
        dijit.byId("sd_key_usredit").setValue(null);
        dijit.byId("sd_key_passpedit").set("disabled", true);
        dijit.byId("sd_key_passpedit").setValue(null);
        dijit.byId("sd_key_fileedit").set("disabled", true);
        dijit.byId("sd_key_fileedit").setValue(null);
    }
}

// Setup a dialog for adding new hosts
function addHostsDialogSetup() {
    var addHostsDlg = null; 
    var hostlist = null;
    var clusterStorage = mcc.storage.clusterStorage();
    
    // Create the dialogue if it does not already exist
    if (!dijit.byId("addHostsDlg")) {
        addHostsDlg= new dijit.Dialog({
            id: "addHostsDlg",
            title: "Add new host",
            content: "\
                <form id='addHostsForm' data-dojo-type='dijit.form.Form'>\
                    <p>\
                        Host name: \
                        <span class=\"helpIcon\" id=\"hostlist_qm\">[?]</span>\
                        <br /><span id='hostlist'></span>\
                        <br />Host internal IP (VPN): \
                        <span class=\"helpIcon\" id=\"sd_IntIP_qm\">[?]</span>\
                        <br /><span id='sd_IntIP'></span>\
                        <br /><br />Key-based auth: \
                        <span class=\"helpIcon\" id=\"sd_key_auth_qm\">[?]</span>\
                        <span id='sd_key_auth'></span>\
                        <br /><table>\
                            <tr>\
                                <td>User \
                                    <span class=\"helpIcon\" id=\"sd_key_usr_qm\">[?]</span>\
                                </td>\
                                <td>Passphrase \
                                    <span class=\"helpIcon\" id=\"sd_key_passp_qm\">[?]</span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='sd_key_usr'></span></td>\
                                <td><span id='sd_key_passp'></span></td>\
                            </tr>\
                        </table>\
                        <table>\
                            <tr>\
                                <td>Key file \
                                    <span class=\"helpIcon\" id=\"sd_key_file_qm\">[?]</span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='sd_key_file'></span></td>\
                            </tr>\
                        </table>\
                        <br /><br />Ordinary login:\
                        <br /><table>\
                            <tr>\
                                <td>User \
                                    <span class=\"helpIcon\" id=\"sd_usr_qm\">[?]</span>\
                                </td>\
                                <td>Password \
                                    <span class=\"helpIcon\" id=\"sd_usrpwd_qm\">[?]</span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='sd_usr'></span></td>\
                                <td><span id='sd_usrpwd'></span></td>\
                            </tr>\
                        </table>\
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
        var hostlist= new dijit.form.ValidationTextBox({style: "width: 245px"}, "hostlist");
        hostlist.required = true;
        hostlist.missingMessage = "This field must have value!";
        var hostlist_tt = new dijit.Tooltip({
            connectId: ["hostlist", "hostlist_qm"],
            label: "Name or ip addresses of host\
                    to use for running MySQL cluster"
        });

        var sd_IntIP = new dijit.form.ValidationTextBox({style: "width: 245px"}, "sd_IntIP");
        var sd_IntIP_tt = new dijit.Tooltip({
            connectId: ["sd_IntIP", "sd_IntIP_qm"],
            label: "If running Cluster inside VPN, internal ip addresses of host\
                    to use for running MySQL cluster"
        });

        // Get the (one and only) cluster item and fetch defaults
        clusterStorage.getItem(0).then(function (cluster) {
            var old_user = cluster.getValue("ssh_user");
            var old_keybased = cluster.getValue("ssh_keybased");
        
            var sd_key_auth = new dijit.form.CheckBox({}, "sd_key_auth");
            dojo.connect(sd_key_auth, "onChange", updateHostAuth);
            var sd_key_auth_tt = new dijit.Tooltip({
                connectId: ["sd_key_auth", "sd_key_auth_qm"],
                label: "Check this box if key based ssh login is enabled \
                        on this host."
            });

            //"Ordinary" USER/PASSWORD login
            var sd_usr = new dijit.form.TextBox({style: "width: 120px"}, "sd_usr");
            dojo.connect(sd_usr, "onChange", updateHostAuth);
            var sd_usr_tt = new dijit.Tooltip({
                connectId: ["sd_usr", "sd_usr_qm"],
                label: "User name for ssh login \
                        to the hosts running MySQL Cluster."
            });
            var sd_usrpwd = new dijit.form.TextBox({
                type: "password",
                style: "width: 120px"
            }, "sd_usrpwd");
            dojo.connect(sd_usrpwd, "onChange", updateHostAuth);
            var sd_usrpwd_tt = new dijit.Tooltip({
                connectId: ["sd_usrpwd", "sd_usrpwd_qm"],
                label: "Password for ssh login \
                        to the hosts running MySQL Cluster."
            });

            //Key based login.
            var sd_key_usr = new dijit.form.TextBox({style: "width: 120px"}, "sd_key_usr");
            dojo.connect(sd_key_usr, "onChange", updateHostAuth);
            var sd_key_usr_tt = new dijit.Tooltip({
                connectId: ["sd_key_usr", "sd_key_usr_qm"],
                label: "User name for key login \
                        if different than in key."
            });

            var sd_key_passp = new dijit.form.TextBox({
                type: "password",
                style: "width: 120px"
            }, "sd_key_passp");
            dojo.connect(sd_key_passp, "onChange", updateHostAuth);
            var sd_key_passp_tt = new dijit.Tooltip({
                connectId: ["sd_key_passp", "sd_key_passp_qm"],
                label: "Passphrase for the key."
            });

            var sd_key_file = new dijit.form.TextBox({style: "width: 245px"}, "sd_key_file");
            dojo.connect(sd_key_file, "onChange", updateHostAuth);
            var sd_key_file_tt = new dijit.Tooltip({
                connectId: ["sd_key_file", "sd_key_file_qm"],
                label: "Path to file containing the key."
            });
            
            //Init:
            if (old_keybased) {
                sd_key_auth.set("checked", old_keybased);
            } else {
                sd_usr.set("value", old_user);
            }
        })
    }
}

// Update and save the selected hosts after editing
function saveSelectedHosts(event) {
    // Prevent default submit handling
    dojo.stopEvent(event);

    var selection = hostGrid.selection.getSelected();
    if (selection && selection.length > 0) {
        for (var i in selection) {
            mcc.util.dbg("Updating selected[" + i + "]");
            var uname = mcc.storage.hostStorage().store().
                    getValue(selection[i], "uname");
            if (dijit.byId("uname").getValue() && dijit.byId("uname").getValue() != uname) {
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
            var hsval = mcc.storage.hostStorage().store().getValue(selection[i], "ram");
            if (dijit.byId("ram").getValue() && dijit.byId("ram").getValue() != hsval) {
                var val = dijit.byId("ram").getValue();
                if (val > 0 && val < 90000000) {
                    mcc.storage.hostStorage().store().setValue(selection[i], 
                        "ram", val);
                }
            }
            hsval = mcc.storage.hostStorage().store().getValue(selection[i], "cores");
            if (dijit.byId("cores").getValue() && dijit.byId("cores").getValue() != hsval) {
                var val = dijit.byId("cores").getValue();
                if (val > 0 && val < 5000) {
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
            hsval = mcc.storage.hostStorage().store().getValue(selection[i], "diskfree");
            if (dijit.byId("diskfree").getValue() && dijit.byId("diskfree").getValue() != hsval) {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                        "diskfree", dijit.byId("diskfree").getValue());
            }

            if (dijit.byId("sd_IntIPedit").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                        "internalIP", dijit.byId("sd_IntIPedit").getValue());
            }
            // Update credentials if necessary.
            if (dijit.byId("sd_usredit").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "usr", dijit.byId("sd_usredit").getValue());
            } else {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "usr", "");
            }
            if (dijit.byId("sd_usrpwdedit").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "usrpwd", dijit.byId("sd_usrpwdedit").getValue());
            } else {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "usrpwd", "");
            }
            if (dijit.byId("sd_key_usredit").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i],
                    "key_usr",dijit.byId("sd_key_usredit").getValue());
            } else {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "key_usr", "");
            }
            if (dijit.byId("sd_key_passpedit").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i],
                    "key_passp",dijit.byId("sd_key_passpedit").getValue());
            } else {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "key_passp", "");
            }
            if (dijit.byId("sd_key_fileedit").getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i],
                    "key_file", dijit.byId("sd_key_fileedit").getValue());
            } else {
                mcc.storage.hostStorage().store().setValue(selection[i], 
                    "key_file", "");
            }
            mcc.storage.hostStorage().store().setValue(selection[i],
                "key_auth", dijit.byId("sd_key_authedit").get("checked"));
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
                "this can be overridden for each process later in this wizard.",
        diskfree: "<i>DiskFree</i> Amount of free space (in GB) available on " +
                  "chosen Data directory disk. In case of failure to fetch," +
                  "unknown is displayed."
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
            style: {width: "720px"},
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
                            <td>DiskFree \
                                <span class=\"helpIcon\"\
                                    id=\"diskfree_qm\">[?]\
                                </span>\
                            </td>\
                        </tr>\
                        <tr>\
                            <td><span id='uname'></span></td>\
                            <td><span id='ram'></span></td>\
                            <td><span id='cores'></span></td>\
                            <td><span id='installdir'></span></td>\
                            <td><span id='datadir'></span></td>\
                            <td><span id='diskfree'></span></td>\
                        </tr>\
                    </table>\
                    <br />Host internal IP (VPN): \
                    <span class=\"helpIcon\" id=\"sd_IntIPedit_qm\">[?]</span>\
                    <br /><span id='sd_IntIPedit'></span>\
                    <br /><br />Key-based auth: \
                    <span class=\"helpIcon\" id=\"sd_key_authedit_qm\">[?]</span>\
                    <span id='sd_key_authedit'></span>\
                    <br /><table>\
                        <tr>\
                            <td>User \
                                <span class=\"helpIcon\" id=\"sd_key_usredit_qm\">[?]</span>\
                            </td>\
                            <td>Passphrase \
                                <span class=\"helpIcon\" id=\"sd_key_passpedit_qm\">[?]</span>\
                            </td>\
                        </tr>\
                        <tr>\
                            <td><span id='sd_key_usredit'></span></td>\
                            <td><span id='sd_key_passpedit'></span></td>\
                        </tr>\
                    </table>\
                    <table>\
                        <tr>\
                            <td>Key file \
                                <span class=\"helpIcon\" id=\"sd_key_fileedit_qm\">[?]</span>\
                            </td>\
                        </tr>\
                        <tr>\
                            <td><span id='sd_key_fileedit'></span></td>\
                        </tr>\
                    </table>\
                    <br /><br />Ordinary login:\
                    <br /><table>\
                        <tr>\
                            <td>User \
                                <span class=\"helpIcon\" id=\"sd_usredit_qm\">[?]</span>\
                            </td>\
                            <td>Password \
                                <span class=\"helpIcon\" id=\"sd_usrpwdedit_qm\">[?]</span>\
                            </td>\
                        </tr>\
                        <tr>\
                            <td><span id='sd_usredit'></span></td>\
                            <td><span id='sd_usrpwdedit'></span></td>\
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
            constraints: {min: 1, max: 90000000, places: 0}
        }, "ram");
        var cores = new dijit.form.NumberSpinner({
            style: "width: 80px",
            constraints: {min: 1, max: 5000, places: 0, format:"####"}
        }, "cores");
        var installdir = new dijit.form.TextBox({style: "width: 170px"}, 
                "installdir");
        var datadir = new dijit.form.TextBox({style: "width: 170px"}, 
                "datadir");
        var diskfree = new dijit.form.TextBox({style: "width: 100px"}, 
                "diskfree");

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
        var diskfree_tt = new dijit.Tooltip({
            connectId: ["diskfree_qm"],
            label: getFieldTT("diskfree")
        });

        var sd_IntIPedit = new dijit.form.ValidationTextBox({style: "width: 245px"}, "sd_IntIPedit");
        var sd_IntIPedit_tt = new dijit.Tooltip({
            connectId: ["sd_IntIPedit", "sd_IntIPedit_qm"],
            label: "If running Cluster inside VPN, internal ip addresses of host\
                    to use for running MySQL cluster"
        });
        
        // Credentials definition
        var sd_key_authedit = new dijit.form.CheckBox({}, "sd_key_authedit");
        dojo.connect(sd_key_authedit, "onChange", updateHostAuthE);
        var sd_key_authedit_tt = new dijit.Tooltip({
            connectId: ["sd_key_authedit", "sd_key_authedit_qm"],
            label: "Check this box if key based ssh login is enabled \
                    on this host."
        });

        //"Ordinary" USER/PASSWORD login
        var sd_usredit = new dijit.form.TextBox({style: "width: 120px"}, "sd_usredit");
        dojo.connect(sd_usredit, "onChange", updateHostAuthE);
        var sd_usredit_tt = new dijit.Tooltip({
            connectId: ["sd_usredit", "sd_usredit_qm"],
            label: "User name for ssh login \
                    to the hosts running MySQL Cluster."
        });
        var sd_usrpwdedit = new dijit.form.TextBox({
            type: "password",
            style: "width: 120px"
        }, "sd_usrpwdedit");
        dojo.connect(sd_usrpwdedit, "onChange", updateHostAuthE);
        var sd_usrpwdedit_tt = new dijit.Tooltip({
            connectId: ["sd_usrpwdedit", "sd_usrpwdedit_qm"],
            label: "Password for ssh login \
                    to the hosts running MySQL Cluster."
        });

        //Key based login.
        var sd_key_usredit = new dijit.form.TextBox({style: "width: 120px"}, "sd_key_usredit");
        dojo.connect(sd_key_usredit, "onChange", updateHostAuthE);
        var sd_key_usredit_tt = new dijit.Tooltip({
            connectId: ["sd_key_usredit", "sd_key_usredit_qm"],
            label: "User name for key login \
                    if different than in key."
        });

        var sd_key_passpedit = new dijit.form.TextBox({
            type: "password",
            style: "width: 120px"
        }, "sd_key_passpedit");
        dojo.connect(sd_key_passpedit, "onChange", updateHostAuthE);
        var sd_key_passpedit_tt = new dijit.Tooltip({
            connectId: ["sd_key_passpedit", "sd_key_passpedit_qm"],
            label: "Passphrase for the key."
        });

        var sd_key_fileedit = new dijit.form.TextBox({style: "width: 245px"}, "sd_key_fileedit");
        dojo.connect(sd_key_fileedit, "onChange", updateHostAuthE);
        var sd_key_fileedit_tt = new dijit.Tooltip({
            connectId: ["sd_key_fileedit", "sd_key_fileedit_qm"],
            label: "Path to file containing the key."
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

    // Button for removing a host. Connect to storeDeleteHost function
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

    // Button for refreshing host information.
    var refreshButton= new dijit.form.Button({
        label: "Refresh selected host(s)",
        iconClass: "dijitIconNewTask"
    }, "refreshHostsButton");
    dojo.connect(refreshButton, "onClick", function() {
        var selection = hostGrid.selection.getSelected();
        if (selection && selection.length > 0) {
            // Get the row index of the last item
            mcc.util.dbg("Running refresh on selection: " + selection);
            for (var i in selection) {
                mcc.storage.getHostResourceInfo(selection[i], 
                  mcc.storage.hostStorage().store().getValue(selection[i],"id"),
                  false, true);
            }
        }   
    });

    // Button for editing the host. Show Edit hosts dialog on click
    var editButton= new dijit.form.Button({
        label: "Edit selected host",
        iconClass: "dijitIconEdit"
    }, "editHostsButton");
    dojo.connect(editButton, "onClick", function () {
        // Get the (one and only) cluster item and fetch default credentials.
        var old_user = "";
        var old_keybased = "";
        mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
            old_user = cluster.getValue("ssh_user");
            old_keybased = cluster.getValue("ssh_keybased");
        });

        // Check if selected Host has configured credentials already,
        // potentially, from using "Add Host" button.
        var selection = hostGrid.selection.getSelected();
        if (selection && selection.length > 0) {
            for (var i in selection) {
                dijit.byId("uname").setValue(mcc.storage.hostStorage().store().getValue(selection[i], "uname"));//null);
                dijit.byId("ram").setValue(mcc.storage.hostStorage().store().getValue(selection[i], "ram"));//null);
                dijit.byId("cores").setValue(mcc.storage.hostStorage().store().getValue(selection[i], "cores"));//null);
                dijit.byId("installdir").setValue(null);
                dijit.byId("datadir").setValue(null);
                dijit.byId("diskfree").setValue(mcc.storage.hostStorage().store().getValue(selection[i], "diskfree")); //null);

                var val = mcc.storage.hostStorage().store().getValue(selection[i], "internalIP");
                if (val) {
                    dijit.byId("sd_IntIPedit").setValue(val);
                } else {
                    dijit.byId("sd_IntIPedit").setValue(null);
                }

                var hasCreds = false;
                var val = mcc.storage.hostStorage().store().getValue(selection[i], "usr");
                if (val) {
                    dijit.byId("sd_usredit").setValue(val);
                    hasCreds = true;
                } else {
                    dijit.byId("sd_usredit").setValue(null);
                }
                val = mcc.storage.hostStorage().store().getValue(selection[i], "usrpwd");
                if (val) {
                    dijit.byId("sd_usrpwdedit").setValue(val);
                    hasCreds = true;
                } else {
                    dijit.byId("sd_usrpwdedit").setValue(null);
                }
                var valku = mcc.storage.hostStorage().store().getValue(selection[i], "key_usr");
                if (valku) {
                    dijit.byId("sd_key_usredit").setValue(valku);
                    hasCreds = true;
                } else {
                    dijit.byId("sd_key_usredit").setValue(null);
                }
                var valkp = mcc.storage.hostStorage().store().getValue(selection[i], "key_passp");
                if (valkp) {
                    dijit.byId("sd_key_passpedit").setValue(valkp);
                    hasCreds = true;
                } else {
                    dijit.byId("sd_key_passpedit").setValue(null);
                }
                var valkf = mcc.storage.hostStorage().store().getValue(selection[i], "key_file");
                if (valkf) {
                    dijit.byId("sd_key_fileedit").setValue(valkf);
                    hasCreds = true;
                } else {
                    dijit.byId("sd_key_fileedit").setValue(null);
                }
                val = mcc.storage.hostStorage().store().getValue(selection[i], "key_auth");
                if (val || valku || valkp || valkf) {
                    dijit.byId("sd_key_authedit").set("checked", val);
                    hasCreds = true;
                } else {
                    dijit.byId("sd_key_authedit").setValue(null);
                }
                
                // Check if there are credentials at host level.
                if (!hasCreds) {
                    mcc.util.dbg("No saved credentials for host " + selection[i] + ", switching to default.");
                    if (old_keybased) {
                        dijit.byId("sd_key_authedit").set("checked", old_keybased);
                    } else {
                        dijit.byId("sd_usredit").set("value", old_user);
                    }
                }
            }
        }
       
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
            constraint: {min: 1, max: 90000000, places: 0}
        },
        {
            width: '9%',
            field: "cores",
            name: "CPU cores",
            editable: true,
            type: dojox.grid.cells._Widget,
            widgetClass: "dijit.form.NumberSpinner", 
            constraint: {min: 1, max: 5000, places: 0}
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
        },
        {
            width: '9%',
            field: "diskfree",
            name: "DiskFree",
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
                                    getPredefinedDirectory(inValue, "installdir");
                            mcc.util.dbg("Update predfined installdir to " + dir);
                            host.setValue("installdir", dir);
                        }
                        if (host.getValue("datadir_predef") == true) {
                            var dir = mcc.storage.hostStorage().
                                    getPredefinedDirectory(inValue, "datadir");
                            mcc.util.dbg("Update predfined datadir to " + dir);
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


