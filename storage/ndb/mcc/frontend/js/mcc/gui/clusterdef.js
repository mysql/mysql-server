/*
Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
 *      mcc.gui.clusterdef.getSSHPwd: Get password for ssh
 *      mcc.gui.clusterdef.getSSHUser: Get ClusterLvL user from variable rather than iterate storage.
 *      mcc.gui.clusterdef.getSSHkeybased: Get ClusterLvL key-based auth from variable rather than iterate storage.
 *      mcc.gui.clusterdef.getOpenFW: Same as above.
 *      mcc.gui.clusterdef.getInstallCl: Same as above.
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
mcc.gui.clusterdef.getSSHUser = getSSHUser;
mcc.gui.clusterdef.getSSHkeybased = getSSHkeybased;
mcc.gui.clusterdef.getOpenFW = getOpenFW;
mcc.gui.clusterdef.getInstallCl = getInstallCl;
//New SSH stuff
mcc.gui.clusterdef.getClSSHPwd = getClSSHPwd;
mcc.gui.clusterdef.getClSSHUser = getClSSHUser;
mcc.gui.clusterdef.getClSSHKeyFile = getClSSHKeyFile;


/******************************* Internal data ********************************/

var ssh_pwd = "";
var ssh_user = "";
var ssh_keybased = false;
var openFW = false;
var installCl = "NONE";
var ssh_ClKeyUser = "";
var ssh_ClKeyPass = "";
var ssh_ClKeyFile = "";
/****************************** Implementation  *******************************/
// Get SSH password.
function getSSHPwd() {
    return ssh_pwd;
}

// Get (ClusterLvL) user.
function getSSHUser() {
    return ssh_user;
}

// Get (ClusterLvL) is auth key-based.
function getSSHkeybased() {
    return ssh_keybased;
}

// Get (ClusterLvL) OpenFW ports.
function getOpenFW() {
    return openFW;
}

// Get (ClusterLvL) "Install Cluster".
function getInstallCl() {
    return installCl;
}

// Get NEW ClusterLvL SSHPwd
function getClSSHPwd() {
    return ssh_ClKeyPass;
}

// Get NEW ClusterLvL SSHUser
function getClSSHUser() {
    return ssh_ClKeyUser;
}

// Get NEW ClusterLvL SSH PK file
function getClSSHKeyFile() {
    return ssh_ClKeyFile;
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
            dijit.byId("sd_ClkeyUser").set("disabled", false);
            dijit.byId("sd_ClkeyPass").set("disabled", false);
            dijit.byId("sd_ClkeyFile").set("disabled", false);
        } else {
            dijit.byId("sd_user").set("disabled", false);
            dijit.byId("sd_pwd").set("disabled", false);
            dijit.byId("sd_ClkeyUser").set("disabled", true);
            dijit.byId("sd_ClkeyUser").set("value", "");
            dijit.byId("sd_ClkeyPass").set("disabled", true);
            dijit.byId("sd_ClkeyPass").set("value", "");
            dijit.byId("sd_ClkeyFile").set("disabled", true);
            dijit.byId("sd_ClkeyFile").set("value", "");
        }
        
        // If there was a change in credentials, try to connect to all hosts
        var old_user = cluster.getValue("ssh_user");
        var old_keybased = cluster.getValue("ssh_keybased");
        var old_SSHClkeyUser = cluster.getValue("ssh_ClKeyUser");
        var old_ssh_ClKeyPass = cluster.getValue("ssh_ClKeyPass");
        var old_ssh_ClKeyFile = cluster.getValue("ssh_ClKeyFile");
        //var old_pwd = ssh_pwd; 
        var old_pwd = cluster.getValue("ssh_pwd");
        
        if (old_user != dijit.byId("sd_user").getValue() ||
            old_keybased != dijit.byId("sd_keybased").getValue() ||
            old_pwd != dijit.byId("sd_pwd").getValue() || 
            old_SSHClkeyUser != dijit.byId("sd_ClkeyUser").getValue() || 
            old_ssh_ClKeyPass != dijit.byId("sd_ClkeyPass").getValue() || 
            old_ssh_ClKeyFile != dijit.byId("sd_ClkeyFile").getValue()) {

            // Store SSH details
            cluster.setValue("ssh_keybased", 
                    dijit.byId("sd_keybased").getValue());
            cluster.setValue("ssh_user", 
                    dijit.byId("sd_user").getValue());
            cluster.setValue("ssh_pwd", 
                    dijit.byId("sd_pwd").getValue());

            cluster.setValue("ssh_ClKeyUser", dijit.byId("sd_ClkeyUser").getValue());
            cluster.setValue("ssh_ClKeyPass", dijit.byId("sd_ClkeyPass").getValue());
            cluster.setValue("ssh_ClKeyFile", dijit.byId("sd_ClkeyFile").getValue()); 

            ssh_pwd = dijit.byId("sd_pwd").getValue();
            ssh_user = dijit.byId("sd_user").getValue();
            ssh_keybased = dijit.byId("sd_keybased").getValue();

            ssh_ClKeyUser = dijit.byId("sd_ClkeyUser").getValue();
            ssh_ClKeyPass = dijit.byId("sd_ClkeyPass").getValue();
            ssh_ClKeyFile = dijit.byId("sd_ClkeyFile").getValue();
           
            // Try to reconnect all hosts to get resource information.
            mcc.util.dbg("Re-fetch host(s) resource information.");
            hostStorage.forItems({}, function (host) {
                if (!host.getValue("anyHost")) {
                    mcc.util.dbg("Re-fetch resource information for host " + 
                            host.getValue("name"));
                    mcc.storage.getHostResourceInfo(host.getValue("name"), 
                            host.getId(), false, false);
                }
            });
        }

        // Cluster details
        cluster.setValue("installCluster", 
            dijit.byId("sd_installCluster").textbox.value);

        cluster.setValue("openfw", 
                dijit.byId("sd_openfw").getValue());

        // Update global vars for later.
        openFW = dijit.byId("sd_openfw").getValue();
        installCl = dijit.byId("sd_installCluster").textbox.value;

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
    mcc.util.dbg("newhosts is " + newHosts);
    // Exclude localhost AND 127.0.0.1
    if (dijit.byId("cd_hosts").getValue().indexOf("localhost") >= 0 && 
        dijit.byId("cd_hosts").getValue().indexOf("127.0.0.1") >= 0) {
        alert("localhost is already in the list!");
        return;
    }
    // Do check:
    var notProperIP = 0;
    var properIP = 0;
    for (var i in newHosts) {
        if ((newHosts[i].trim()).length > 0) {
            if (newHosts[i].trim() == "localhost" || newHosts[i].trim() == "127.0.0.1") {
                notProperIP += 1;
            } else {
                properIP += 1;
            }
        }
    };
    if ((notProperIP > 1) || (notProperIP > 0 && properIP > 0)) {
        alert("Mixing localhost with remote hosts is not allowed!\nPlease change localhost/127.0.0.1 to a proper IP address\nif you want to use your box.");
        document.getElementById("cd_hosts").focus();
        return;
    };

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
        dijit.byId("sd_pwd").setValue(
                cluster.getValue("ssh_pwd"));
        
        ssh_pwd = cluster.getValue("ssh_pwd");
        ssh_user = cluster.getValue("ssh_user");
        ssh_keybased = cluster.getValue("ssh_keybased");
        
        dijit.byId("sd_ClkeyUser").setValue(
            cluster.getValue("ssh_ClKeyUser"));
        ssh_ClKeyUser = cluster.getValue("ssh_ClKeyUser");

        dijit.byId("sd_ClkeyPass").setValue(
            cluster.getValue("ssh_ClKeyPass"));
        ssh_ClKeyPass = cluster.getValue("ssh_ClKeyPass");

        dijit.byId("sd_ClkeyFile").setValue( 
            cluster.getValue("ssh_ClKeyFile"));
        ssh_ClKeyFile = cluster.getValue("ssh_ClKeyFile");

        // Cluster details
        dijit.byId("cd_name").setValue(
                cluster.getValue("name"));
        dijit.byId("cd_apparea").setValue(
                cluster.getValue("apparea"));
        dijit.byId("cd_writeload").setValue(
                cluster.getValue("writeload"));

        // Installation details
        if (cluster.getValue("installCluster")) {
            dijit.byId("sd_installCluster").textbox.value = 
                cluster.getValue("installCluster");
            installCl = cluster.getValue("installCluster");
        } else {
            //First run, set to NONE
            dijit.byId("sd_installCluster").textbox.value = "NONE";
            cluster.setValue("installCluster", "NONE");
        }
        if (cluster.getValue("openfw")) {
            dijit.byId("sd_openfw").setValue(
                cluster.getValue("openfw"));
            openFW = cluster.getValue("openfw");
        } else {
            //First run, set to NONE
            dijit.byId("sd_openfw").setValue(false);
            cluster.setValue("openfw", false);
        }

    });

    // If hosts exist, set value
    hostStorage.getItems({anyHost: false}).then(function (hosts) {
        var hostlist = "";
        if ((!hosts || hosts.length == 0) && initialize) {
        } else {
            for (var i in hosts) {
                if (i > 0) {
                    hostlist += ", ";
                }
                hostlist += hosts[i].getValue("name");
            }
        }
        if (hostlist) {
            // Do check:
            var notProperIP = 0;
            var properIP = 0;
            var ar = hostlist.split(",");
            for (var i in ar) {
                if ((ar[i].trim()).length > 0) {
                    if (ar[i].trim() == "localhost" || ar[i].trim() == "127.0.0.1") {
                        notProperIP += 1;
                    } else {
                        properIP += 1;
                    }
                }
            };
            if ((notProperIP > 1) || (notProperIP > 0 && properIP > 0)) {
                // This is actually invalid configuration read from store...
                alert("Mixing localhost with remote hosts is not allowed!\nInvalid configuration read from store.");
                document.getElementById("cd_hosts").focus();
            } else {
                dijit.byId("cd_hosts").setValue(hostlist);
            }
        } else {
            document.getElementById("cd_hosts").focus();
        }
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
            name: 'SSH property (Cluster-wide)',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, "sshDetailsHeader");
    sshDetailsHeader.startup();

    var installDetailsHeader = new dojox.grid.DataGrid({
        baseClass: "content-grid-header",
        autoHeight: true,
        structure: [{
            name: 'Install properties (Cluster-wide)',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, "installDetailsHeader");
    installDetailsHeader.startup();

    // Setup all the required widgets and connect them to the save function
    var cd_name = new dijit.form.TextBox({style: "width: 120px"}, "cd_name");
    //dojo.connect(cd_name, "onChange", saveClusterDefinition);
    cd_name.set("disabled", "disabled");
    var cd_name_tt = new dijit.Tooltip({
        connectId: ["cd_name", "cd_name_qm"],
        label: "Cluster name",
        destroyOnHide: true
    });

    var cd_hosts = new dijit.form.TextBox({style: "width: 250px"}, "cd_hosts");
    dojo.connect(cd_hosts, "onChange", saveClusterDefinition);
    var cd_hosts_tt = new dijit.Tooltip({
        connectId: ["cd_hosts", "cd_hosts_qm"],
        label: "Comma separated list of names or ip addresses of the hosts \
                to use for running MySQL Cluster",
        destroyOnHide: true
    });

    var cd_apparea = new dijit.form.FilteringSelect({style: "width: 120px", 
            store: mcc.storage.stores.appAreaStore()}, "cd_apparea");
    dojo.connect(cd_apparea, "onChange", saveClusterDefinition);
    var cd_apparea_tt = new dijit.Tooltip({
        connectId: ["cd_apparea", "cd_apparea_qm"],
        label: "Intended use of the application. \
                This information is used for determining the appropriate \
                value of various configuration parameters.",
        destroyOnHide: true
    });

    var cd_writeload = new dijit.form.FilteringSelect({style: "width: 120px", 
            store: mcc.storage.stores.loadStore()}, "cd_writeload");
    dojo.connect(cd_writeload, "onChange", saveClusterDefinition);
    var cd_writeload_tt = new dijit.Tooltip({
        connectId: ["cd_writeload", "cd_writeload_qm"],
        label: "Write load for the application. \
                This information is used for determining the appropriate \
                value of various configuration parameters.",
        destroyOnHide: true
    });

    var sd_keybased = new dijit.form.CheckBox({}, "sd_keybased");
    dojo.connect(sd_keybased, "onChange", saveClusterDefinition);
    var sd_keybased_tt = new dijit.Tooltip({
        connectId: ["sd_keybased", "sd_keybased_qm"],
        label: "Check this box if key based ssh login is enabled \
                on the hosts running MySQL Cluster.",
        destroyOnHide: true
    });

    var sd_user = new dijit.form.TextBox({style: "width: 120px"}, "sd_user");
    dojo.connect(sd_user, "onChange", saveClusterDefinition);
    var sd_user_tt = new dijit.Tooltip({
        connectId: ["sd_user", "sd_user_qm"],
        label: "User name for ssh login \
                to the hosts running MySQL Cluster.",
        destroyOnHide: true
    });

    var sd_pwd = new dijit.form.TextBox({
        type: "password",
        style: "width: 120px"
    }, "sd_pwd");
    dojo.connect(sd_pwd, "onChange", saveClusterDefinition);
    var sd_pwd_tt = new dijit.Tooltip({
        connectId: ["sd_pwd", "sd_pwd_qm"],
        label: "Password for ssh login \
                to the hosts running MySQL Cluster.",
        destroyOnHide: true
    });

//NEW SSH stuff:
    var sd_ClkeyUser = new dijit.form.TextBox({style: "width: 120px"}, "sd_ClkeyUser");
    dojo.connect(sd_ClkeyUser, "onChange", saveClusterDefinition);
    sd_ClkeyUser.setAttribute("disabled", true);
    var sd_key_usr_tt = new dijit.Tooltip({
        connectId: ["sd_ClkeyUser", "sd_ClkeyUser_qm"],
        label: "User name for key login \
                if different than in key.",
        destroyOnHide: true
    });

    var sd_ClkeyPass = new dijit.form.TextBox({
        type: "password",
        style: "width: 120px"
    }, "sd_ClkeyPass");
    dojo.connect(sd_ClkeyPass, "onChange", saveClusterDefinition);
    sd_ClkeyPass.setAttribute("disabled", true);
    var sd_key_passp_tt = new dijit.Tooltip({
        connectId: ["sd_ClkeyPass", "sd_ClkeyPass_qm"],
        label: "Passphrase for the key.",
        destroyOnHide: true
    });

    var sd_ClkeyFile = new dijit.form.TextBox({style: "width: 245px"}, "sd_ClkeyFile");
    dojo.connect(sd_ClkeyFile, "onChange", saveClusterDefinition);
    sd_ClkeyFile.setAttribute("disabled", true);
    var sd_key_file_tt = new dijit.Tooltip({
        connectId: ["sd_ClkeyFile", "sd_ClkeyFile_qm"],
        label: "Path to file containing the key.",
        destroyOnHide: true
    });
//-
    var sd_installCluster = new dijit.form.FilteringSelect({}, "sd_installCluster");
    var options=[];
    options.push({label: "BOTH", value: "BOTH", selected:false});
    options.push({label: "DOCKER", value: "DOCKER", selected:false});
    options.push({label: "REPO", value: "REPO", selected:false});
    options.push({label: "NONE", value: "NONE", selected:true});
    sd_installCluster.set("labelAttr", "label")
    sd_installCluster.set("searchAttr", "value");
    sd_installCluster.set("idProperty", "value");
    sd_installCluster.store.setData(options);
    dojo.connect(sd_installCluster, "onChange", saveClusterDefinition);
    var sd_installCluster_tt = new dijit.Tooltip({
        connectId: ["sd_installCluster", "sd_installCluster_qm"],
        label: "Try installing MySQL Cluster \
                on hosts using repo/docker or both.",
        destroyOnHide: true
    });

    var sd_openfw = new dijit.form.CheckBox({}, "sd_openfw");
    dojo.connect(sd_openfw, "onChange", saveClusterDefinition);
    var sd_openfw_tt = new dijit.Tooltip({
        connectId: ["sd_openfw", "sd_openfw_qm"],
        label: "Open necessary firewall ports for running MySQL Cluster.",
        destroyOnHide: true
    });
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Cluster definition module initialized");
});


