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
 ***                             Cluster definition                         ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.gui.clusterdef
 *
 *  Description:
 *      Define cluster properties
 *
 *  External interface: 
 *      mcc.gui.clusterdef.showClusterDefinition: Show/edit stored information
 *      mcc.gui.clusterdef.saveClusterDefinition: Save entered information
 *      mcc.gui.clusterdef.getSSHPwd: : Get password for ssh
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *     clusterDefinitionSetup: Create all necessary widgets
 *
 *  Internal data: 
 *      ssh_pwd: Password for ssh
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.gui.clusterdef");

dojo.require("dojox.grid.DataGrid");
dojo.require("dijit.form.CheckBox");
dojo.require("dijit.form.TextBox");
dojo.require("dijit.form.FilteringSelect");
dojo.require("dijit.form.NumberSpinner");
dojo.require("dijit.Tooltip");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.gui.clusterdef.showClusterDefinition = showClusterDefinition;
mcc.gui.clusterdef.saveClusterDefinition = saveClusterDefinition;
mcc.gui.clusterdef.getSSHPwd = getSSHPwd;

/******************************* Internal data ********************************/

var ssh_pwd = "";

/****************************** Implementation  *******************************/

// Get SSH password
function getSSHPwd() {
    return ssh_pwd;
}

// Save data from widgets into the cluster data object
function saveClusterDefinition() {

    // Get hold of storage objects
    var clusterStorage = mcc.storage.clusterStorage();
    var hostStorage = mcc.storage.hostStorage();

    // Get the (one and only) cluster item and update it
    clusterStorage.getItem(0).then(function (cluster) {

        // If toggling the keybased ssh on, reset and disable user/pwd
        if (dijit.byId("sd_keybased").get("value")) {
            dijit.byId("sd_user").set("disabled", true);
            dijit.byId("sd_user").set("value", "");
            dijit.byId("sd_pwd").set("disabled", true);
            dijit.byId("sd_pwd").set("value", "");
        } else {
            dijit.byId("sd_user").set("disabled", false);
            dijit.byId("sd_pwd").set("disabled", false);
        }

        // If there was a change in credentials, try to connect to all hosts
        var old_user = cluster.getValue("ssh_user");
        var old_keybased = cluster.getValue("ssh_keybased");
        var old_pwd = ssh_pwd; 

        if (old_user != dijit.byId("sd_user").getValue() ||
            old_keybased != dijit.byId("sd_keybased").getValue() ||
            old_pwd != dijit.byId("sd_pwd").getValue()) {

            // Store SSH details
            cluster.setValue("ssh_keybased", 
                    dijit.byId("sd_keybased").getValue());
            cluster.setValue("ssh_user", 
                    dijit.byId("sd_user").getValue());

            // The password is not stored in cluster store, only as a variable
            ssh_pwd = dijit.byId("sd_pwd").getValue();

            // Try to reconnect all hosts to get resource information
            mcc.util.dbg("Re-fetch resource information");
            hostStorage.forItems({}, function (host) {
                if (!host.getValue("anyHost")) {
                    mcc.util.dbg("Re-fetch resource information for host " + 
                            host.getValue("name"));
                    mcc.storage.getHostResourceInfo(host.getValue("name"), 
                            host.getId(), false);
                }
            });
        }

        // Cluster details
        cluster.setValue("name", 
                dijit.byId("cd_name").getValue());

        // Warn if web app or realtime
        if (cluster.getValue("apparea") == "simple testing" &&
                dijit.byId("cd_apparea").getValue() != "simple testing") {
            alert("Please note that with this application area, the " +
                    "configuration tool will set parameter values " +
                    "assuming that the hosts are used for running " +
                    "MySQL Cluster exclusively. The cluster will need " +
                    "much time for initialization, maybe in the order of " +
                    "one hour or more, and will consume most of the hosts' " +
                    "RAM, hence leaving the hosts unusable for other " +
                    "applications. ");
        }
        cluster.setValue("apparea", 
                dijit.byId("cd_apparea").getValue());
        cluster.setValue("writeload", 
                dijit.byId("cd_writeload").getValue());

        clusterStorage.save();
    });

    // Make array of host list
    var newHosts = dijit.byId("cd_hosts").getValue().split(",");

    // Strip leading/trailing spaces
    for (var i in newHosts) {
        newHosts[i] = newHosts[i].replace(/^\s*/, "").replace(/\s*$/, "");
        if (newHosts[i].length > 0) {
            if (dojo.indexOf(newHosts, newHosts[i]) != i) {
                alert("Hostname '" + newHosts[i] + 
                        "' is entered more than once");
                return;
            }
        }
    }

    // Update hostStore based on the host list
    hostStorage.getItems({anyHost: false}).then(function (hosts) {
        var oldHosts = []; 

        // Build list of old hosts, check removal, but keep wildcard host
        for (var i in hosts) {
            oldHosts[i] = hosts[i].getValue("name");
            if (dojo.indexOf(newHosts, oldHosts[i]) == -1 /*&& 
                    !hosts[i].getValue("anyHost")*/) {
                hostStorage.deleteItem(hosts[i], true);
            }
        }

        // Add new hosts
        for (var i in newHosts) {
            if (newHosts[i].length > 0 && 
                        dojo.indexOf(oldHosts, newHosts[i]) == -1) {
                hostStorage.newItem({
                    name: newHosts[i],
                    anyHost: false
                }, false);
            }
        }
        hostStorage.save();
    });
}

// Select cluster object from store, fill in values into the widgets
function showClusterDefinition(initialize) {
    
    // Setup if required
    if (!dijit.byId("clusterDetailsHeader")) {
        clusterDefinitionSetup();
    }

    // Get hold of storage objects
    var clusterStorage = mcc.storage.clusterStorage();
    var hostStorage = mcc.storage.hostStorage();

    clusterStorage.getItem(0).then(function (cluster) {
        // SSH details
        dijit.byId("sd_keybased").setValue(
                cluster.getValue("ssh_keybased"));
        dijit.byId("sd_user").setValue(
                cluster.getValue("ssh_user"));
        dijit.byId("sd_pwd").setValue(ssh_pwd);

        // Cluster details
        dijit.byId("cd_name").setValue(
                cluster.getValue("name"));
        dijit.byId("cd_apparea").setValue(
                cluster.getValue("apparea"));
        dijit.byId("cd_writeload").setValue(
                cluster.getValue("writeload"));
    });

    // If hosts exist, set value
    hostStorage.getItems({anyHost: false}).then(function (hosts) {
        var hostlist = "";
        if ((!hosts || hosts.length == 0) && initialize) {
            hostlist = "127.0.0.1";
        } else {
            for (var i in hosts) {
                if (i > 0) {
                    hostlist += ", ";
                } 
                hostlist += hosts[i].getValue("name");
            }
        }
        dijit.byId("cd_hosts").setValue(hostlist);
    });
}

// Setup the page with widgets for the cluster definition
function clusterDefinitionSetup() {

    // Setup the required headers
    var clusterDetailsHeader = new dojox.grid.DataGrid({
        baseClass: "content-grid-header",
        autoHeight: true,
        structure: [{
            name: 'Cluster property',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, "clusterDetailsHeader");
    clusterDetailsHeader.startup();

    var sshDetailsHeader = new dojox.grid.DataGrid({
        baseClass: "content-grid-header",
        autoHeight: true,
        structure: [{
            name: 'SSH property',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, "sshDetailsHeader");
    sshDetailsHeader.startup();

    // Setup all the required widgets and connect them to the save function
    var cd_name = new dijit.form.TextBox({style: "width: 120px"}, "cd_name");
    dojo.connect(cd_name, "onChange", saveClusterDefinition);
    var cd_name_tt = new dijit.Tooltip({
        connectId: ["cd_name", "cd_name_qm"],
        label: "Cluster name"
    });

    var cd_hosts = new dijit.form.TextBox({style: "width: 250px"}, "cd_hosts");
    dojo.connect(cd_hosts, "onChange", saveClusterDefinition);
    var cd_hosts_tt = new dijit.Tooltip({
        connectId: ["cd_hosts", "cd_hosts_qm"],
        label: "Comma separated list of names or ip addresses of the hosts \
                to use for running MySQL cluster"
    });

    var cd_apparea = new dijit.form.FilteringSelect({style: "width: 120px", 
            store: mcc.storage.stores.appAreaStore()}, "cd_apparea");
    dojo.connect(cd_apparea, "onChange", saveClusterDefinition);
    var cd_apparea_tt = new dijit.Tooltip({
        connectId: ["cd_apparea", "cd_apparea_qm"],
        label: "Intended use of the application. \
                This information is used for determining the appropriate \
                value of various configuration parameters."
    });

    var cd_writeload = new dijit.form.FilteringSelect({style: "width: 120px", 
            store: mcc.storage.stores.loadStore()}, "cd_writeload");
    dojo.connect(cd_writeload, "onChange", saveClusterDefinition);
    var cd_writeload_tt = new dijit.Tooltip({
        connectId: ["cd_writeload", "cd_writeload_qm"],
        label: "Write load for the application. \
                This information is used for determining the appropriate \
                value of various configuration parameters."
    });

    var sd_keybased = new dijit.form.CheckBox({}, "sd_keybased");
    dojo.connect(sd_keybased, "onChange", saveClusterDefinition);
    var sd_keybased_tt = new dijit.Tooltip({
        connectId: ["sd_keybased", "sd_keybased_qm"],
        label: "Check this box if key based ssh login is enabled \
                on the hosts running MySQL Cluster."
    });

    var sd_user = new dijit.form.TextBox({style: "width: 120px"}, "sd_user");
    dojo.connect(sd_user, "onChange", saveClusterDefinition);
    var sd_user_tt = new dijit.Tooltip({
        connectId: ["sd_user", "sd_user_qm"],
        label: "User name for ssh login \
                to the hosts running MySQL Cluster."
    });

    var sd_pwd = new dijit.form.TextBox({
        type: "password",
        style: "width: 120px"
    }, "sd_pwd");
    dojo.connect(sd_pwd, "onChange", saveClusterDefinition);
    var sd_pwd_tt = new dijit.Tooltip({
        connectId: ["sd_pwd", "sd_pwd_qm"],
        label: "Password for ssh login \
                to the hosts running MySQL Cluster."
    });
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Cluster definition module initialized");
});


