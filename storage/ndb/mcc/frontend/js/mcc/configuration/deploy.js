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
 ***                      Configuration export utilities                    ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.configuration.deploy
 *
 *  Description:
 *      Utilities for deploying configuration parameters, startup commands, etc.
 *
 *  External interface: 
 *      mcc.configuration.deploy.setupContext: Get all cluster items
 *      mcc.configuration.deploy.getStartupCommand: Get command for process
 *      mcc.configuration.deploy.getConfigurationFile: Get config.ini
 *      mcc.configuration.deploy.deployCluster: Create dirs, distribute files
 *      mcc.configuration.deploy.startCluster: Deploy configuration, start procs
 *      mcc.configuration.deploy.stopCluster: Stop processes
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      processTypeInstances: Retrieve all processes of a certain process type
 *      processFamilyTypes: Retrieve all process types of a process family
 *      processFamilyInstances: Retrieve all processes of a process family
 *      distributeConfigurationFiles: Distribute all configuration files
 *      getEffectiveInstanceValue: Get effective parameter value for a process
 *      getEffectiveTypeValue: Get effective parameter value for a process type
 *      getEffectiveInstalldir: Get effective install directory for given host
 *      getConnectstring: Get the connect string for this cluster
 *      getCreateDirCommands: Generate cluster directory creation commands
 *      getStartClusterCommands: Start processes - build array of process groups
 *
 *  Internal data: 
 *      cluster:          The cluster item
 *      clusterItems:     All items, indexed by their id
 *      hosts:            All hosts, indexed by order of presence
 *      processes:        All processes        --- " ---
 *      processTypes:     All process types    --- " ---
 *      processTypeMap:   All process types indexed by name
 *      processFamilyMap: All process types indexed by family
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
        Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.require("dijit.ProgressBar");

dojo.provide("mcc.configuration.deploy");

dojo.require("mcc.configuration");
dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.configuration.deploy.setupContext = setupContext;
mcc.configuration.deploy.getStartupCommand = getStartupCommand;
mcc.configuration.deploy.getConfigurationFile = getConfigurationFile;
mcc.configuration.deploy.deployCluster = deployCluster;
mcc.configuration.deploy.startCluster = startCluster;
mcc.configuration.deploy.stopCluster = stopCluster;

/******************************* Internal data ********************************/

var cluster = null;         // The cluster item
var clusterItems = [];      // All items, indexed by their id
var hosts = [];             // All hosts            --- " ---
var processes = [];         // All processes        --- " ---
var processTypes = [];      // All process types    --- " ---
var processTypeMap = {};    // All process types indexed by name
var processFamilyMap = {};  // All process types indexed by family

/****************************** Implementation ********************************/

/******************************* Context setup ********************************/

// Fetch all cluster data into clusterItems list, create access structures
function setupContext() {

    // Reset data structures
    cluster = null;
    clusterItems = [];
    hosts = [];
    processes = [];
    processTypes = [];
    processTypeMap = {};
    processFamilyMap = {};

    var waitCondition = new dojo.Deferred();
    var waitStores = [new dojo.Deferred(), 
        new dojo.Deferred(), 
        new dojo.Deferred(), 
        new dojo.Deferred()
    ];
    var waitAllStores = new dojo.DeferredList(waitStores);
    // Get cluster item
    mcc.storage.clusterStorage().getItems().then(function (items) {
        cluster = items[0];
        mcc.util.dbg("Cluster item setup done");
        waitStores[0].resolve();
    });
    // Get process type items
    mcc.storage.processTypeStorage().getItems().then(function (items) {
        var waitItems = [];
        var waitAllItems = null;
        for (var i in items) {
            clusterItems[items[i].getId()] = items[i];
            processTypes.push(items[i]);
            processTypeMap[items[i].getValue("name")] = items[i];
            // The first ptype of the given family will be prototypical 
            if (!processFamilyMap[items[i].getValue("family")]) {
                processFamilyMap[items[i].getValue("family")] = items[i];
                // Setup process family parameters
                waitItems.push(new dojo.Deferred());
                (function (i, j) {
                    mcc.configuration.typeSetup(items[i]).then(
                        function () {
                            mcc.util.dbg("Setup process family " + 
                                items[i].getValue("family") + " done");
                            waitItems[j].resolve();
                        }
                    );
                })(i, waitItems.length-1);
            }
        }
        waitAllItems = new dojo.DeferredList(waitItems);
        waitAllItems.then(function (res) {
            waitStores[1].resolve();
        });
    });
    // Get process items
    mcc.storage.processStorage().getItems().then(function (items) {
        var waitItems = [];
        var waitAllItems = null;
        for (var i in items) {
            waitItems[i] = new dojo.Deferred();
            clusterItems[items[i].getId()] = items[i];
            processes.push(items[i]);
            // Setup process parameters
            (function (i) {
                var family = clusterItems[items[i].getValue("processtype")].
                        getValue("family");
                mcc.configuration.instanceSetup(family, items[i]).then(
                    function () {
                        mcc.util.dbg("Setup process " + 
                            items[i].getValue("name") + " done");
                        waitItems[i].resolve();
                    }
                );
            })(i);
        }
        waitAllItems = new dojo.DeferredList(waitItems);
        waitAllItems.then(function (res) {
            waitStores[2].resolve();
        });
    });
    // Get host items
    mcc.storage.hostStorage().getItems().then(function (items) {
        for (var i in items) {
            clusterItems[items[i].getId()] = items[i];
            hosts.push(items[i]);
        }
        waitStores[3].resolve();
    });
    waitAllStores.then(function () {
        waitCondition.resolve();
    });

    return waitCondition;
}

/****************************** Context access ********************************/

// Retrieve all processes of a certain process type
function processTypeInstances(name) {
    var items = [];
    var typeId = processTypeMap[name].getId();
    for (var i in processes) {
        if (processes[i].getValue("processtype") == typeId) {
            items.push(processes[i]);
        }
    }
    return items;
}

// Retrieve all process types of a certain process family
function processFamilyTypes(name) {
    var items = [];
    for (var i in processTypes) {
        if (processTypes[i].getValue("family") == name) {
            items.push(processTypes[i]);
        }
    }
    return items;
}

// Retrieve all processes of a certain process family
function processFamilyInstances(name) {
    var items = [];
    var types = processFamilyTypes(name);
    for (var t in types) {
        items = items.concat(processTypeInstances(types[t].getValue("name")));
    }
    return items;
}

/************************* Configuration validation ***************************/

function verifyConfiguration() {
    // One pass over processes to group by host, prepare for direct lookup
    var processesOnHost = [];
    var redoLogChecked = false;
    for (var p in processes) {
        // Create array unless already present
        if (!processesOnHost[processes[p].getValue("host")]) {
            processesOnHost[processes[p].getValue("host")] = [];
        }
        // Append process to array
        processesOnHost[processes[p].getValue("host")].push(processes[p]);
    }

    // Alert if NoOfReplicas==2 for an odd number of data nodes
    var noOfReplicas = getEffectiveTypeValue(processFamilyMap['data'], 'NoOfReplicas');
    if (processFamilyInstances('data').length % 2 == 1 && noOfReplicas == 2) {
	alert("With an uneven number of data nodes, the number of replicas " +
	      "(NoOfReplicas) must be set to 1");
	return false;
    }

    // Do verification for each host individually
    for (var h in hosts) {
        var dirs = [];
        var ports = [];
        var files = [];

        var hostId = hosts[h].getId();
        var hostName = hosts[h].getValue("name");

        mcc.util.dbg("Check processes on host " + hostName);
        // One loop 
        for (var p in processesOnHost[hostId]) {
            // Process instance
            var proc = processesOnHost[hostId][p];
            mcc.util.dbg("Check process " + name + " (" + nodeid + ")");

            // Various attributes
            var id = proc.getId();
            var nodeid = proc.getValue("NodeId");
            var name = proc.getValue("name");

            var file = null;
            var port = null;
            var dir = null;

            // ndbds have redo log
            if (cluster.getValue("apparea") != "simple testing" && 
                !redoLogChecked && (
                processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndbd" || 
                processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndbmtd")) {
                var nFiles = getEffectiveInstanceValue(proc, 
                        "NoOfFragmentLogFiles");
                var fileSz = getEffectiveInstanceValue(proc, 
                        "FragmentLogFileSize");
                var vol = nFiles * fileSz / 1000;

                if (mcc.util.isWin(hosts[h].getValue("uname"))) {
                    vol *= 4;
                }
                redoLogChecked = true;
                // Assume initializing 1G/min
                if (vol > 3) {
                    if (!confirm("Please note that the current values of the " +
                            "data layer configuration parameters " +
                            "NoOfFragmentLogFiles and " +
                            "FragmentLogFileSize mean that the " +
                            "processes may need about " + Math.floor(vol) + 
                            " minutes to " +
                            "start. Please press the " +
                            "Cancel button below to cancel deployment, or " +
                            "press OK to continue. ")) {
                        return false; 
                    }
                }
            }

            // Only mysqld processes have socket and port
            if (processTypes[proc.getValue("processtype")].getValue("name") == 
                    "mysqld") {
                file = getEffectiveInstanceValue(proc, "Socket");
                port = getEffectiveInstanceValue(proc, "Port");
            }

            // Only ndb_mgmd processes have portnumber
            if (processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndb_mgmd") {
                port = getEffectiveInstanceValue(proc, "Portnumber");
            }

            // All processes except api have datadir
            if (processTypes[proc.getValue("processtype")].getValue("name") != 
                    "api") {
                dir = getEffectiveInstanceValue(proc, "DataDir");
            }

            // Check that all processes have different directories
            if (dir) {
                if (dojo.indexOf(dirs, dir) != -1) {
                    var other = processesOnHost[hostId]
                            [dojo.indexOf(dirs, dir)];
                    alert("Invalid configuration on host '" + hostName +
                            "': " + name + " (" + nodeid + 
                            ") has same data directory as " + 
                            other.getValue("name") + " (" + 
                            other.getValue("NodeId") + ")");    
                    return false;
                } else {
                    // Store this process' datadir on its array index
                    dirs[p] = dir;
                }
            }

            // Check that all processes have different port numbers
            if (port) {
                if (dojo.indexOf(ports, port) != -1) {
                    var other = processesOnHost[hostId]
                            [dojo.indexOf(ports, port)];
                    alert("Invalid configuration on host '" + hostName +
                            "': " + name + " (" + nodeid + 
                            ") has same port number as " + 
                            other.getValue("name") + " (" + 
                            other.getValue("NodeId") + ")");    
                    return false;
                } else {
                    // Store this process' port number on its array index
                    ports[p] = port;
                }
            }

            // Check that all processes have different socket files
            if (file) {
                if (dojo.indexOf(files, file) != -1) {
                    var other = processesOnHost[hostId]
                            [dojo.indexOf(files, file)];
                    alert("Invalid configuration on host '" + hostName +
                            "': " + name + " (" + nodeid + 
                            ") has same socket file as " + 
                            other.getValue("name") + " (" + 
                            other.getValue("NodeId") + ")");    
                    return false;
                } else {
                    // Store this process' socket file on its array index
                    files[p] = file;
                }
            }
        }
    }
    return true;
}

/*********************** Configuration file handling **************************/

// Distribute all configuration files
function distributeConfigurationFiles() {

    // External waitCondition
    var waitCondition = new dojo.Deferred();

    // Get all ndb_mgmds
    var ndb_mgmds = processTypeInstances("ndb_mgmd");

    // Array of wait conditions
    var waitList = [];
    var waitAll = null;

    // Loop over all ndb_mgmds and send a create file command
    for (var i in ndb_mgmds) {
        waitList.push(new dojo.Deferred);
        (function (i) {
            var configFile = getConfigurationFile(ndb_mgmds[i]);
            if (configFile.host && configFile.path && 
                    configFile.name) {
                mcc.util.dbg("Distribute configuration file for '" + 
                        ndb_mgmds[i].getValue("name") + "'");
                mcc.server.createFileReq(
                        configFile.host, 
                        configFile.path, 
                        configFile.name, 
                        configFile.msg,
                        true,
                        function () {
                            waitList[i].resolve();
                        }
                );
            }       
        })(i);
    }
    waitAll = new dojo.DeferredList(waitList);
    waitAll.then(function () {
        waitCondition.resolve();
    });
    return waitCondition;
}

// Generate configuration file
function getConfigurationFile(process) {

    // Structure to return
    var configFile = {
        host: null,
        path: null,
        name: "config.ini",
        html: null,
        msg: null
    };

    if (clusterItems[process.getValue("processtype")].getValue("name") == 
            "ndb_mgmd") {

        configFile.host = clusterItems[process.getValue("host")].
                getValue("name");
        configFile.path = getEffectiveInstanceValue(process, "DataDir");

        configFile.html = "#<br>"; 
        configFile.msg = "#\n"; 

        configFile.html += "# Configuration file for "
                + cluster.getValue("name") + "<br>#<br>";
        configFile.msg += "# Configuration file for "
                + cluster.getValue("name") + "\n#\n";

        // Loop over all process families
        for (var family in processFamilyMap) {
            // Get the prototypical process type, output header
            var ptype = processFamilyMap[family];
            if (true) {

                // Special treatment of TCP buffers
                if (ptype.getValue("name") == "ndbd") {
                    configFile.html += "<br>[TCP DEFAULT]<br>";
                    configFile.msg += "\n[TCP DEFAULT]\n";
                    if (mcc.configuration.visiblePara("cfg", 
                            cluster.getValue("apparea"), 
                            "data", "SendBufferMemory")) {
                        configFile.html += "SendBufferMemory=" + 
                                getEffectiveTypeValue(ptype, 
                                        "SendBufferMemory") +
                                        "M<br>";
                        configFile.msg += "SendBufferMemory=" + 
                                getEffectiveTypeValue(ptype, 
                                        "SendBufferMemory") +
                                        "M\n";
                    }
                    if (mcc.configuration.visiblePara("cfg", 
                            cluster.getValue("apparea"), 
                            "data", "ReceiveBufferMemory")) {
                        configFile.html += "ReceiveBufferMemory=" + 
                                getEffectiveTypeValue(ptype, 
                                        "ReceiveBufferMemory") +
                                        "M<br>";
                        configFile.msg += "ReceiveBufferMemory=" + 
                                getEffectiveTypeValue(ptype, 
                                        "ReceiveBufferMemory")  +
                                        "M\n";
                    }
                }

                // Config file section header for type defaults except api
                if (ptype.getValue("name") != "api") {
                    configFile.html += "<br>[" +
                            ptype.getValue("name").toUpperCase()
                            + " DEFAULT]<br>";
                    configFile.msg += "\n[" +
                            ptype.getValue("name").toUpperCase()
                            + " DEFAULT]\n";
                }

                // Output process type defaults
                for (var p in mcc.configuration.getAllPara(family)) {
                    var val = getEffectiveTypeValue(ptype, p);
                    if (val !== undefined) {
                        // Get the appropriate suffix (K, M, G, etc.)
                        var suffix = mcc.configuration.getPara(family, null, p, 
                                "suffix");
                        if (!suffix) {
                            suffix = "";
                        }
                        // TCP buffers and mysqld options treated separately
                        if (p != "SendBufferMemory" && 
                                p != "ReceiveBufferMemory" &&
                                mcc.configuration.getPara(family, null, p, 
                                        "destination") 
                                == "config.ini" &&
                                mcc.configuration.visiblePara("cfg", 
                                        cluster.getValue("apparea"), 
                                        family, p)) {
                            configFile.html += mcc.configuration.getPara(
                                    family, null, p, "attribute") + "=" + 
                                    val + suffix + "<br>";
                            configFile.msg += mcc.configuration.getPara(
                                    family, null, p, "attribute") + "=" + 
                                    val + suffix + "\n";
                        }
                    }
                }

                // Loop over all types in this family
                var ptypes = processFamilyTypes(family);
                for (var t in ptypes) {
                    // Loop over all processes of this type
                    var processes = processTypeInstances(ptypes[t].
                            getValue("name"));
                    for (var p in processes) {
                        var id = processes[p].getId();

                        configFile.html += "<br>[" +
                                ptype.getValue("name").toUpperCase() +
                                "]<br>";
                        configFile.msg += "\n[" +
                                ptype.getValue("name").toUpperCase() +
                                "]\n";

                        for (var para in mcc.configuration.getAllPara(family)) {
                            var val = processes[p].getValue(para);

                            // Skip parameters not visible for instances
                            if (!mcc.configuration.getPara(family, id, 
                                            para, "visibleInstance")) {
                                continue;
                            }

                            if (val === undefined) {
                                if (ptype.getValue(para) === undefined) {
                                    val= mcc.configuration.getPara(family, id, 
                                            para, "defaultValueInstance");
                                }
                            }

                            if (val) {
                                var suffix = mcc.configuration.getPara(family, 
                                        null, para, "suffix");
                                if (!suffix) {
                                    suffix = "";
                                }
                                if (p != "SendBufferMemory" && 
                                        p != "ReceiveBufferMemory" && 
                                        mcc.configuration.getPara(family, null, 
                                            para, "destination") == 
                                            "config.ini" &&
                                            mcc.configuration.visiblePara("cfg", 
                                                    cluster.getValue("apparea"), 
                                                    family, para)) {
                                        configFile.html += 
                                                mcc.configuration.getPara(
                                                    family, null, para, 
                                                    "attribute")
                                                + "=" + val + suffix + "<br>";
                                        configFile.msg += 
                                                mcc.configuration.getPara(
                                                    family, null, para, 
                                                    "attribute")
                                                + "=" + val + suffix + "\n";
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return configFile;
}

/************************* Parameter access support ***************************/

// Get the effective parameter value for a given process
function getEffectiveInstanceValue(process, parameter) {
    // First see if instance overrides
    var val = process.getValue(parameter);
    var id = process.getId();

    // If not overridden for instance, see if type overrides
    if (val === undefined) {
        var ptype = clusterItems[process.getValue("processtype")];
        // Get the prototypical type representing the family
        ptype = processFamilyMap[ptype.getValue("family")];
        val = ptype.getValue(parameter);
        // If not overridden for type, check predefined instance value
        if (val === undefined) {
            val = mcc.configuration.getPara(ptype.getValue("family"), id, 
                    parameter, "defaultValueInstance");
            // If no predefined instance value, use predefined type value
            if (val === undefined) {
                val = mcc.configuration.getPara(ptype.getValue("family"), null, 
                        parameter, "defaultValueType");
            }
        }
    }
    return val;
}

// Get the effective parameter value for a given process type
function getEffectiveTypeValue(ptype, parameter) {
    // First see if type overrides
    var val = ptype.getValue(parameter);
    // If not overridden for type, get default type value
    if (val === undefined) {
        val = mcc.configuration.getPara(ptype.getValue("family"), null, 
                parameter, "defaultValueType");
    }
    return val;
}

// Get the effective install directory for the given host
function getEffectiveInstalldir(host) {
    return host.getValue("installdir");
}

// Get the connect string for this cluster
function getConnectstring() {
    var connectString = "";
    var mgmds = processTypeInstances("ndb_mgmd");
    // Loop over all ndb_mgmd processes
    for (var i in mgmds) {
        var port = getEffectiveInstanceValue(mgmds[i], "Portnumber");
        var host = clusterItems[mgmds[i].getValue("host")].getValue("name");
        connectString += host + ":" + port + ",";
    }
    return connectString;
}

/*************************** Progress bar handling ****************************/

// Show progress bar
function updateProgressDialog(title, subtitle, props) {

    if (!dijit.byId("progressBarDialog")) {
        progressBarDialogSetup();
        dijit.byId("progressBarDialog").show();
    }

    dijit.byId("progressBarDialog").set("title", title);
    dojo.byId("progressBarSubtitle").innerHTML = subtitle;
    dijit.byId("configWizardProgressBar").update(props);
}

// Setup a dialog for showing progress
function progressBarDialogSetup() {
    var pBarDlg = null; 
    // Create the dialog if it does not already exist
    if (!dijit.byId("progressBarDialog")) {
        pBarDlg= new dijit.Dialog({
            id: "progressBarDialog",
            content: "\
                <div id='progressBarSubtitle'></div>\
                <div id='configWizardProgressBar'\
                    dojoType='dijit.ProgressBar'\
                    progress: '0%',\
                    annotate='true'>\
                </div>"
        });
    }
}

function removeProgressDialog() {
    if (dijit.byId("progressBarDialog")) {
        dijit.byId("progressBarDialog").destroyRecursive();
    }
}

/****************** Directory and startup command handling ********************/

// Generate the (array of) directory creation commands for all processes
function getCreateCommands() {

    // Array to return
    var createDirCommands = [];

    // Loop over all cluster processes
    for (var i in processes) {
        var process = processes[i];

        // Get process type
        var ptype = clusterItems[process.getValue("processtype")];

        // Directories only for non-ndbapi commands
        if (ptype.getValue("name") != "api") {
            // Get host
            var host = clusterItems[process.getValue("host")];

            // Get datadir and dir separator
            var datadir = mcc.util.unixPath(
                    mcc.util.terminatePath(
                            getEffectiveInstanceValue(process, "DataDir")));
            var dirSep = mcc.util.dirSep(datadir);

            // Push the create datadir command unless mysqld
            if (ptype.getValue("name") != "mysqld") {
                createDirCommands.push({
                    host: host.getValue("name"),
                    path: datadir,
                    name: null
                });
            } else {
                // All mysqlds also need data/test, data/mysql and socket dir
                var socketDir = mcc.util.unixPath(
                        getEffectiveInstanceValue(process, "Socket"));
                var sockSep = mcc.util.dirSep(socketDir);

                socketDir = socketDir.substring(0, socketDir.
                        lastIndexOf(dirSep));
                createDirCommands.push({
                    host: host.getValue("name"),
                    path: socketDir,
                    name: null
                });
                createDirCommands.push({
                    host: host.getValue("name"),
                    path: datadir + "test" + dirSep,
                    name: null
                });
                createDirCommands.push({
                    host: host.getValue("name"),
                    path: datadir + "mysql" + dirSep,
                    name: null
                });

                // Mysqlds on Windows need a tmpdir and an install.sql
                if (mcc.util.isWin(host.getValue("uname"))) {

                    var installDir = mcc.util.unixPath(
                            getEffectiveInstalldir(host));
                    var installSep = mcc.util.dirSep(installDir);

                    createDirCommands.push({
                        host: host.getValue("name"),
                        path: datadir + "tmp" + dirSep,
                        name: "install.sql",
                        msg: "use mysql;\n",
                        overwrite: true
                    });
                    createDirCommands.push({
                        cmd: "appendFileReq",
                        host: host.getValue("name"),
                        sourcePath: installDir + "share" + installSep,
                        sourceName: "mysql_system_tables.sql",
                        destinationPath: datadir + "tmp" + dirSep,
                        destinationName: "install.sql"
                    });
                    createDirCommands.push({
                        cmd: "appendFileReq",
                        host: host.getValue("name"),
                        sourcePath: installDir + "share" + installSep,
                        sourceName: "mysql_system_tables_data.sql",
                        destinationPath: datadir + "tmp" + dirSep,
                        destinationName: "install.sql"
                    });
                    createDirCommands.push({
                        cmd: "appendFileReq",
                        host: host.getValue("name"),
                        sourcePath: installDir + "share" + installSep,
                        sourceName: "fill_help_tables.sql",
                        destinationPath: datadir + "tmp" + dirSep,
                        destinationName: "install.sql"
                    });
                } else {
                    // Non-win mysqlds also need data/tmp
                    createDirCommands.push({
                        host: host.getValue("name"),
                        path: datadir + "tmp" + dirSep,
                        name: null
                    });
                }
            }
        }
    }
    return createDirCommands;
}

// Generate the (array of) startup command(s) for the given process
function getStartupCommand(process) {
    // Command description for use in html or messages
    var startupCommand = {
        html: {
            host: null,
            path: null,
            name: null,
            optionString: "<tr><td><b>Options</b></td>"
        },
        msg: {
        }
    };

    // Array of commands to return
    var startupCommands = [];

    // Get process type
    var ptype = clusterItems[process.getValue("processtype")];

    // Startup commands only for non-ndbapi commands
    if (ptype.getValue("name") != "api") {
        // Get host
        var host = clusterItems[process.getValue("host")];

        // Assign host name, install dir and process name
        startupCommand.html.host = host.getValue("name");
        startupCommand.html.path = mcc.util.unixPath(getEffectiveInstalldir(host));
        startupCommand.html.name = ptype.getValue("name");

        startupCommand.msg.file = {
            hostName: startupCommand.html.host,
            path: startupCommand.html.path,
            name: startupCommand.html.name,
            autoComplete: true
        };

        startupCommand.msg.procCtrl = {
            hup: false,
            getStd: false,
            waitForCompletion: true
        };

        // Windows: Append .exe to file name, set waitForCompletion = false
        if (mcc.util.isWin(host.getValue("uname"))) {
            startupCommand.msg.procCtrl.waitForCompletion = false;
            startupCommand.msg.file.name += ".exe";
            startupCommand.html.name += ".exe";
        }

        // Get datadir
        var datadir = mcc.util.terminatePath(getEffectiveInstanceValue(process, "DataDir"));

        // Get connect string
        var connectString = getConnectstring();

        // Add process specific options
        if (ptype.getValue("name") == "ndb_mgmd") {
            startupCommand.html.optionString +="<td>--initial</td></tr>" +
                    "<tr><td></td><td>--ndb-nodeid=" +
                    process.getValue("NodeId") + "</td></tr>" +
                    "<tr><td></td><td>--config-dir=" +
                    datadir + "</td></tr>" +
                    "<tr><td></td><td>--config-file=" + datadir + 
                    "config.ini</td></tr>" + "</table>";

            startupCommand.msg.params = {
                sep: " ",
                param: [
                    {name: "--initial"},
                    {name: "--ndb-nodeid", val: process.getValue("NodeId")},
                    {name: "--config-dir", val: mcc.util.quotePath(datadir)},
                    {name: "--config-file", val: mcc.util.quotePath(datadir + "config.ini")}
                ]
            };
        } else if (ptype.getValue("name") == "ndbd" || 
                ptype.getValue("name") == "ndbmtd") {
            startupCommand.html.optionString += 
                    "<td>--ndb-nodeid=" + process.getValue("NodeId") + 
                    "</td></tr>" + "<tr><td></td><td>--ndb-connectstring=" + 
                    connectString + "</td></tr>" + "</table>";

            startupCommand.msg.params = {
                sep: " ",
                param: [
                    {name: "--ndb-nodeid", val: process.getValue("NodeId")},
                    {name: "--ndb-connectstring", val: connectString}
                ]
            };
        } else if (ptype.getValue("name") == "mysqld") {
            var port = getEffectiveInstanceValue(process, "Port");
            var socket = getEffectiveInstanceValue(process, "Socket");

            startupCommand.html.optionString += 
                    "<td>--no-defaults</td></tr>" +
                    "<tr><td></td><td>--datadir=" + datadir + "</td></tr>" +
                    "<tr><td></td><td>--tmpdir=" + datadir + "tmp</td></tr>" +
                    "<tr><td></td><td>--basedir=" + startupCommand.html.path + "</td></tr>";

            startupCommand.msg.procCtrl = {
                hup: false,
                getStd: false,
                waitForCompletion: false,
                daemonWait: 5
            };

            startupCommand.msg.params = {
                sep: " ",
                param: [
                    {name: "--no-defaults"},
                    {name: "--datadir", val: mcc.util.quotePath(datadir)},
                    {name: "--tmpdir", val: mcc.util.quotePath(datadir + "tmp")},
                    {name: "--basedir", val: mcc.util.quotePath(getEffectiveInstalldir(host))},
                    {name: "--port", val: port},
                    {name: "--ndbcluster"},
                    {name: "--ndb-nodeid", val: process.getValue("NodeId")},
                    {name: "--ndb-connectstring", val: connectString}
                ]
            };

            // Add socket option unless windows
            if (!mcc.util.isWin(host.getValue("uname"))) {
                startupCommand.msg.params.param.push({
                    name: "--socket", 
                    val: mcc.util.quotePath(socket)
                });
                startupCommand.html.optionString += 
                    "<tr><td></td><td>--socket=" + socket + "</td></tr>";
            }

            // Add the rest of the option string
            startupCommand.html.optionString += "<tr><td></td><td>--port=" + port + "</td></tr>" +
                    "<tr><td></td><td>--ndbcluster</td></tr>" +
                    "<tr><td></td><td>--ndb-nodeid=" + 
                    process.getValue("NodeId") + "</td></tr>" +
                    "<tr><td></td><td>--ndb-connectstring=" + connectString + 
                    "</td></tr></table>";

            // We also need an init command for mysqld
            if (mcc.util.isWin(host.getValue("uname"))) {

                var basedir = getEffectiveInstalldir(host);
                var langdir = basedir + "share";
                var tmpdir = datadir + "tmp";

                startupCommands.push({
                    html: {
                        host: host.getValue("name"),
                        path: basedir,
                        name: "mysqld.exe",
                        optionString: "<tr><td><b>Options</b></td>" +
                                         "<td>--lc-messages-dir=" + langdir + "</td></tr>" +
                            "<tr><td></td><td>--bootstrap" + "</td></tr>" +
                            "<tr><td></td><td>--basedir=" + basedir + "</td></tr>" +
                            "<tr><td></td><td>--datadir=" + datadir + "</td></tr>" +
                            "<tr><td></td><td>--tmpdir=" + tmpdir + "</td></tr>" +
                            "<tr><td></td><td>--log-warnings=0" + "</td></tr>" +
                            "<tr><td></td><td>--loose-skip-ndbcluster" + "</td></tr>" +
                            "<tr><td></td><td>--max_allowed_packet=8M" + "</td></tr>" +
                            "<tr><td></td><td>--default-storage-engine=myisam" + "</td></tr>" +
                            "<tr><td></td><td>--net_buffer_length=16K" + "</td></tr>"
                    },
                    msg: {
                        file: {
                            hostName: host.getValue("name"),
                            path: mcc.util.unixPath(basedir),
                            name: "mysqld.exe",
                            autoComplete: true,
                            stdinFile: tmpdir + "/install.sql"
                        },
                        procCtrl: {
                            hup: false,
                            getStd: false,
                            waitForCompletion: true
                        },
                        params: {
                            sep: " ",
                            param: [
                                {name: "--lc-messages-dir", val: mcc.util.quotePath(langdir)},
                                {name: "--bootstrap"},
                                {name: "--basedir", val: mcc.util.quotePath(basedir)},
                                {name: "--datadir", val: mcc.util.quotePath(datadir)},
                                {name: "--tmpdir", val: mcc.util.quotePath(tmpdir)},
                                {name: "--log-warnings", val: 0},
                                {name: "--loose-skip-ndbcluster"},
                                {name: "--max-allowed-packet", val: "8M"},
                                {name: "--default-storage-engine", val: "myisam"},
                                {name: "--net_buffer_length", val: "16K"}
                            ]
                        }
                    }
                });
            } else {
                startupCommands.push({
                    html: {
                        host: host.getValue("name"),
                        path: getEffectiveInstalldir(host),
                        name: "mysql_install_db",
                        optionString: "<tr><td><b>Options</b></td>" +
                            "<td>--no-defaults</td></tr>" +
                            "<tr><td></td><td>--datadir=" + datadir + 
                            "</td></tr>" +
                            "<tr><td></td><td>--basedir=" +
                            startupCommand.html.path + 
                            "</td></tr></table>"
                    },
                    msg: {
                        file: {
                            hostName: startupCommand.html.host,
                            path: startupCommand.html.path,
                            name: "mysql_install_db",
                            autoComplete: true
                        },
                        procCtrl: {
                            hup: false,
                            getStd: false,
                            waitForCompletion: true
                        },
                        params: {
                            sep: " ",
                            param: [
                                {name: "--no-defaults"},
                                {name: "--datadir", val: datadir},
                                {name: "--basedir", val: 
                                        startupCommand.html.path}
                            ]
                        }
                    }
                });
            }
        }
        // Push the startup command for any process type
        startupCommands.push(startupCommand);
    }
    return startupCommands;
}

// Start cluster processes - build array of process groups
function getStartClusterCommands(ptype) {

    // Array of process groups to return
    var pgroups = [];

    // Append start commands for a set of processes
    function appendStartupCommands(procs) {
        var startAt = pgroups.length;
        for (var i in procs) {
            var cmds = getStartupCommand(procs[i]);
            for (var c in cmds) {
                var index = startAt + +c;
                if (!pgroups[index]) {
                    pgroups[index] = {
                        plist: [],
                        syncPolicy: {
                            type: "wait",
                            length: (cmds[0].msg.file.name == "ndbd" ||
                                        cmds[0].msg.file.name == "ndbmtd" ?
                                        5 : 2)
                        }                    
                    };
                }
                pgroups[index].plist.push(cmds[c].msg);
            }
        }
    }

    if (ptype) {
        // Restrict commands to submitted process type only
        appendStartupCommands(processFamilyInstances(ptype));
    } else {
        // Generate start commands for all ndb_mgmds
        appendStartupCommands(processFamilyInstances("management"));

        // Generate start commands for all ndbXds
        appendStartupCommands(processFamilyInstances("data"));

        // Generate start commands for all mysqlds
        appendStartupCommands(processFamilyInstances("sql"));
    }

    return pgroups;
}

// Generate the stop command for the given process
function getStopCommand(process) {
    // Command description for use in messages
    var stopCommand = {};

    // Get process type
    var ptype = clusterItems[process.getValue("processtype")];

    // Stop commands only for mysqld and ndb_mgmd
    if (ptype.getValue("name") == "mysqld" || ptype.getValue("name") == "ndb_mgmd") {
        // Get host
        var host = clusterItems[process.getValue("host")];

        // Assign host name, install dir and process name
        stopCommand.file = {
            hostName: host.getValue("name"),
            path: mcc.util.unixPath(getEffectiveInstalldir(host)),
            autoComplete: true
        };

        stopCommand.procCtrl = {
            hup: false,
            getStd: false,
            waitForCompletion: true
        };

        // Add process specific options
        if (ptype.getValue("name") == "ndb_mgmd") {
            stopCommand.file.name = "ndb_mgm";
            stopCommand.params = {
                sep: " ",
                param: [
                    {name: "--ndb-connectstring", val: getConnectstring()},
                    {name: "--execute", val: "shutdown"}
                ]
            };
        } else if (ptype.getValue("name") == "mysqld") {
            stopCommand.file.name = "mysqladmin";
            
            stopCommand.params = {
                sep: " ",
                param: [
                    {name: "--port", val: getEffectiveInstanceValue(process, "Port")},
                    {name: "--user", val: "root"},
                    {name: "shutdown"}
                ]
            };
            
            // Add socket option unless windows
            if (!mcc.util.isWin(host.getValue("uname"))) {
                stopCommand.params.param.push({
                    name: "--socket", 
                    val: mcc.util.quotePath(getEffectiveInstanceValue(process, "Socket"))
                });
            }
        }
        
        // Windows: Append .exe to file name, set waitForCompletion = false
        if (mcc.util.isWin(host.getValue("uname"))) {
            // stopCommand.procCtrl.waitForCompletion = false;
            stopCommand.file.name += ".exe";
        }
    }
    return stopCommand;
}

// Get stop commands for all mysqlds and for management (including ndbd)
function getStopClusterCommands() {

    // Array of process groups to return
    var pgroups = [];

    // Append stop commands for a set of processes
    function appendStopCommands(procs) {
        pgroups.push({
            plist: [],
            syncPolicy: {
                type: "wait",
                length: 2
            }                    
        });

        for (var i in procs) {
            pgroups[pgroups.length-1].plist.push(getStopCommand(procs[i]));
        }
    }

    appendStopCommands(processFamilyInstances("sql"));
    appendStopCommands(processFamilyInstances("management"));
    return pgroups;
}

// Send a create- or append command to the back end
function sendFileOp(createCmds, curr, waitCondition) {
    var createCmd = createCmds[curr];
    if (createCmd.cmd == "appendFileReq") {
        mcc.server.appendFileReq(
            createCmd.host,
            createCmd.sourcePath,
            createCmd.sourceName,
            createCmd.destinationPath,
            createCmd.destinationName,
            function () {
                curr++;
                if (curr == createCmds.length) {
                    waitCondition.resolve(true);
                } else {
                    updateProgressDialog("Deploying configuration", 
                            "Preparing files ", {progress: curr});
                    sendFileOp(createCmds, curr, waitCondition);
                }
            },  
            function (errMsg) {
                var wrn = "Unable to append file " + 
                            createCmd.sourcePath +
                            createCmd.sourceName + " to " +
                            createCmd.destinationPath +
                            createCmd.destinationName + " to " +
                            " on host " + createCmd.host +
                            ": " + errMsg;
                mcc.util.wrn(wrn);
                alert(wrn);
                waitCondition.resolve(false);
            }
        );
    } else {
        mcc.server.createFileReq(
            createCmd.host,
            createCmd.path,
            createCmd.name,
            createCmd.msg,
            createCmd.overwrite,
            function () {
                curr++;
                if (curr == createCmds.length) {
                    waitCondition.resolve(true);
                } else {
                    updateProgressDialog("Deploying configuration", 
                            "Creating directories ", {progress: curr});
                    sendFileOp(createCmds, curr, waitCondition);
                }
            },  
            function (errMsg) {
                var wrn = "Unable to create directory " + 
                            createCmd.path +
                            " on host " + createCmd.host +
                            ": " + errMsg; 
                mcc.util.wrn(wrn);
                alert(wrn);
                waitCondition.resolve(false);
            }
        );
    }
}

// Send all file ops
function sendFileOps(createCmds) {
    var waitCondition = new dojo.Deferred();
    sendFileOp(createCmds, 0, waitCondition); 
    return waitCondition;
}

// Deploy cluster: Create directories, distribute files
function deployCluster(silent, fraction) {

    // External wait condition
    var waitCondition = new dojo.Deferred();
    var waitList = [];
    var waitAll = null;

    // Get the create dir/file commands
    var createCmd = getCreateCommands();

    // Prevent additional error messages
    var alerted = false; 

    // Check if configuration is consistent
    if (!verifyConfiguration()) {
        waitCondition.resolve(false);
        return waitCondition;
    }

    // Show progress
    updateProgressDialog("Deploying configuration", 
            "Creating directories", 
            {maximum: fraction? fraction * createCmd.length : createCmd.length}
    );

    mcc.util.dbg("Creating directories...");

    sendFileOps(createCmd).then(function (ok) {
        if (!ok) {
            mcc.util.dbg("Aborting deployment due to previous error");
            waitCondition.resolve(false);
            removeProgressDialog();
            return;
        }
        var progress = createCmd.length;
        
        mcc.util.dbg("Directories created");
        mcc.util.dbg("Distributing configuration files...");
        updateProgressDialog("Deploying configuration", 
                "Distributing configuration files", {progress: progress++});
        distributeConfigurationFiles().then(function () {
            mcc.util.dbg("Configuration files distributed");
            if (!silent) {
                updateProgressDialog("Deploying configuration", 
                        "Configuration deployed", {progress: "100%"});
                alert("Directories created and configuration deployed");
                removeProgressDialog();
            }
            waitCondition.resolve(true);
        });
    });
    return waitCondition;
}

// Start cluster: Deploy configuration, start processes
function startCluster() {
    
    // External wait condition
    var waitCondition = new dojo.Deferred();
    
    deployCluster(true, 10).then(function (deployed) {
        if (deployed) {
            mcc.util.dbg("Starting cluster...");

            // Get the start cluster commands
            var commands = getStartClusterCommands();
            var layers = ["Management layer", "Data layer", "SQL layer", "SQL layer"];
            var procNames = ["Cluster", "Cluster", "SQL install", "SQL server"];
            var currseq = 0;

            function onError(errMsg) {
                alert(errMsg);
                removeProgressDialog();
                waitCondition.resolve();
            }

            function updateProgressAndStartNext() {
                if (currseq < commands.length) {
                    mcc.util.dbg("Starting " + procNames[currseq] + " processes");
                    updateProgressDialog("Starting cluster", 
                            "Starting " + procNames[currseq] +
                            " processes", {maximum: 5, progress: 1 + currseq});
                    mcc.server.startClusterReq([commands[currseq]], 
                        onReply, onError);                        
                    currseq++;        
                } else {
                    mcc.util.dbg("Cluster started");
                    updateProgressDialog("Starting cluster", 
                            "Cluster started", 
                            {progress: "100%"});
                    alert("Cluster started");
                    removeProgressDialog();
                    waitCondition.resolve();
                }
            }
            
            function onReply() {
                // Wait for processes at previous start level to be started
                var timer = null;
                if (commands[currseq-1].plist[0].procCtrl.waiForCompletion == false) {
                    mcc.util.dbg("Wait for " + procNames[currseq-1] + " processes to be started");  
                    mcc.storage.processTreeStorage().getItems({name: layers[currseq-1]}).then(function (pfam) {
                        var procs = pfam[0].getValues("processes");
                        for (var i in procs) {
                            var stat = mcc.storage.processTreeStorage().store().getValue(procs[i], "status");
                            if (stat && stat != "STARTED" && stat != "CONNECTED" && !timer) {
                                // Not all started, wait and call onReply again
                                mcc.util.dbg("Not all " + procNames[currseq-1] + " processes are stopped, continue waiting");
                                timer = setTimeout(onReply, 2000);
                                break;
                            }
                        }
                    });
                }
                if (!timer) {
                    mcc.util.dbg("All " + procNames[currseq-1] + " processes are started");
                    updateProgressAndStartNext();
                }
            }

            // Initiate startup sequence by calling onReply
            updateProgressAndStartNext();            

        } else {
            mcc.util.dbg("Not starting cluster due to previous error");
        }
    });

    return waitCondition;
}

// Stop cluster
function stopCluster() {
    // External wait condition
    var waitCondition = new dojo.Deferred();
    mcc.util.dbg("Stopping cluster...");

    // Get the start cluster commands
    var commands = getStopClusterCommands();
    var layers = ["SQL layer", "Management layer"];
    var procNames = ["SQL", "Cluster"];
    var currseq = 0;

    function onError(errMsg, errReply) {
        mcc.util.dbg("stopCluster failed: "+errMsg);
        alert("Error occured while stopping cluster: `"+errMsg+"' (Press OK to continue)");
        updateProgressAndStopNext();
    }

    function onReply() {
        mcc.util.dbg("Wait for " + procNames[currseq-1] + " processes to be stopped");  
        mcc.storage.processTreeStorage().getItems({name: layers[currseq-1]}).then(function (pfam) {
            var procs = pfam[0].getValues("processes");
            var timer = null;
            for (var i in procs) {
                var stat = mcc.storage.processTreeStorage().store().getValue(procs[i], "status");
                if (stat && stat != "STOPPED" && stat != "UNKNOWN" && stat != "NO_CONTACT" && !timer) {
                    // Not all stopped, wait and call onReply again
                    mcc.util.dbg("Not all " + procNames[currseq-1] + " processes are stopped, continue waiting");
                    timer = setTimeout(onReply, 2000);
                    break;
                }
            }
            if (!timer) {
                mcc.util.dbg("All " + procNames[currseq-1] + " processes are stopped");
                updateProgressAndStopNext();
            }
        });
    }

    function updateProgressAndStopNext() {
        if (currseq < commands.length) {
            mcc.util.dbg("Stopping " + procNames[currseq] + " processes");
            updateProgressDialog("Stopping cluster", 
                    "Stopping " + procNames[currseq] +
                    " processes", {maximum: 3, progress: 1 + currseq});
            mcc.server.startClusterReq([commands[currseq]], 
                onReply, onError);       
            currseq++;        
        } else {
            mcc.util.dbg("Cluster stopped");
            updateProgressDialog("Stopping cluster", 
                    "Cluster stopped", 
                    {progress: "100%"});
            alert("Cluster stopped");
            removeProgressDialog();
            waitCondition.resolve();
        }
    }

    // Initiate stop sequence
    updateProgressAndStopNext();            

    return waitCondition; 
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Configuration deployment module initialized");
});

