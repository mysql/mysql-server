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
 *      mcc.configuration.deploy.getStartrProcessCommands: Get command for process
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
mcc.configuration.deploy.getStartProcessCommands = getStartProcessCommands;
mcc.configuration.deploy.getConfigurationFile = getConfigurationFile;
mcc.configuration.deploy.deployCluster = deployCluster;
mcc.configuration.deploy.startCluster = startCluster;
mcc.configuration.deploy.stopCluster = stopCluster;

function logIt(x) {
    var msg = "Logged "+x.toString()+":";
    mcc.util.dbg(msg);
    console.log(x);
    alert(msg);
}

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


// Retrieve processItems in the order specified by the families argument
function getProcessItemsByFamilies(families) {
    var processItems = [];
    for (var fx in families) {
        processItems = processItems.concat(processFamilyInstances(families[fx]));
    }
    return processItems;
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

    // Get all ndb_mgmds and all mysqlds
    var cfg_procs = processTypeInstances("ndb_mgmd").concat(processTypeInstances("mysqld"));

    // Array of wait conditions
    var waitList = [];
    var waitAll = null;

    // Loop over all ndb_mgmds and send a create file command
    for (var i in cfg_procs) {
        waitList.push(new dojo.Deferred);
        (function (i) {
            var configFile = getConfigurationFile(cfg_procs[i]);
            if (configFile.host && configFile.path && 
                    configFile.name) {
                mcc.util.dbg("Distribute configuration file for '" + 
                        cfg_procs[i].getValue("name") + "'");
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
    var hostItem = clusterItems[process.getValue("host")];
    var ptype = clusterItems[process.getValue("processtype")].getValue("name");

    if (ptype == "api") { return null; }

    // Structure to return
    var configFile = {
        host: hostItem.getValue("name"),
        path: mcc.util.unixPath(getEffectiveInstanceValue(process, "DataDir")),
        name: null,
        html:  "#<br>",
        msg: "#\n"
    };

    if (ptype == "ndb_mgmd") {
        configFile.name = "config.ini";

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
    else if (ptype == "mysqld") {
        function addln(cf, ln) {
            cf.msg += ln + "\n";
            cf.html += ln + "<br>";
        }
            
        configFile.name = "my.cnf";
        addln(configFile, "# Configuration file for "
              + cluster.getValue("name"));
        addln(configFile, "# Generated by mcc");
        addln(configFile, "#");
        addln(configFile, "[mysqld]");
        addln(configFile, "log-error=mysqld."+process.getValue("NodeId")+".err")
        addln(configFile, "datadir=\""+configFile.path+"\"");
        addln(configFile, "tmpdir=\""+configFile.path+"tmp"+"\"");
        addln(configFile, "basedir=\""+mcc.util.unixPath(getEffectiveInstalldir(hostItem))+"\"");
        addln(configFile, "port="+getEffectiveInstanceValue(process, "Port"));
        addln(configFile, "ndbcluster=on");
        addln(configFile, "ndb-nodeid="+process.getValue("NodeId"));
        addln(configFile, "ndb-connectstring="+getConnectstring());
        if (!mcc.util.isWin(hostItem.getValue("uname"))) {
            addln(configFile, "socket=\""+
                  getEffectiveInstanceValue(process, "Socket")+"\"");            
        }
    } else {
        return null;
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
					createDirCommands.push({
                        host: host.getValue("name"),
                        path: datadir + "tmp" + dirSep,
                        name: "mysql_install_db.bat",
                        msg: "\""+installDir+installSep+"bin"+installSep+"mysqld.exe\" --lc-messages-dir=\""+installDir+installSep+"share\" --bootstrap --basedir=\""+
                            installDir+"\" --datadir=\""+datadir+
                            "\" --loose-skip-ndbcluster --max_allowed_packet=8M --default-storage-engine=myisam --net_buffer_length=16K < \""+
                            datadir+dirSep+"tmp"+dirSep+"install.sql\"\n",
                        overwrite: true
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

// ProcessCommand Constructor
function ProcessCommand(h, p, n) {
    // Command description for use in html or messages
    this.html = {
            host: h,
            path: p,
            name: n,
            optionString: "<tr><td><b>Options</b></td>"
    };

    this.msg = {
        file: {
            hostName: h,
            path: p,
            name: n,
            autoComplete: true
        },
        procCtrl: {
            getStd: true,
            waitForCompletion: true
        },
        params: {
            sep: " ",
            param: []
        }
    };

    var firstOpt = true;
    this.addopt = function (n) {
        var pa = { name: n }
        var opt = n;
        if (arguments.length > 1) {
            pa.val = arguments[1];
            opt += "="+arguments[1];
        }

        this.msg.params.param.push(pa);
        this.html.optionString += (!firstOpt?"<tr><td></td>":"")+"<td>"+opt.replace(/</g, "&lt").replace(/>/g, "&gt")+"</td>";
        firstOpt = false;
    };
    this.isDone = function () { return true; };
}

var lastNdbdCmd = null;
var ndbdids = [];

// Generate the (array of) startup command(s) for the given process
function getStartProcessCommands(process) {
    // Get process type
    var ptypeItem = clusterItems[process.getValue("processtype")];
    var ptype = ptypeItem.getValue("name");
    var nodeid = process.getValue("NodeId");
    
    // Startup commands only for non-ndbapi commands
    if (ptype == "api") { return null; }
    
    // Get host
    var hostItem = clusterItems[process.getValue("host")];
    var host = hostItem.getValue("name");
    var isWin = mcc.util.isWin(hostItem.getValue("uname"));
    
    // Get datadir
    var datadir_ = mcc.util.terminatePath(getEffectiveInstanceValue(process, "DataDir"));
    var datadir = mcc.util.unixPath(datadir_);
    var basedir = getEffectiveInstalldir(hostItem);
                                    
    // Get connect string
    var connectString = getConnectstring();

    var sc = new ProcessCommand(
        host,
        mcc.util.unixPath(basedir), // Always use unix basedir unless running under cmd.exe
        ptypeItem.getValue("name")+(isWin?".exe":""));
    
    sc.progTitle = "Starting node "+nodeid+" ("+ptype+")";
    var scmds = [sc];

    // Add process specific options
    if (ptype == "ndb_mgmd") {
        var isDoneFunction = function () { return mcc.gui.getStatii(nodeid) == "CONNECTED"; };
        if (isWin) {
            sc.addopt("--install", "N"+nodeid);            
            sc.progTitle = "Installing node "+nodeid+" ("+ptype+
                ") as service N"+nodeid;
            sc.msg.procCtrl.noRaise = 1; // --install returns 1 on success
            
            var ss = new ProcessCommand(host, "", "net");
            delete ss.msg.file.autoComplete; // Don't want ac for net cmd
            ss.addopt("start");
            ss.addopt("N"+nodeid);
            ss.progTitle = "Starting service N"+nodeid;
	    ss.isDone = isDoneFunction;
            scmds.push(ss);
        } else {
	    sc.isDone = isDoneFunction;
        }
        
        sc.addopt("--initial");
        sc.addopt("--ndb-nodeid", process.getValue("NodeId"));

	if (isWin) {
            sc.addopt("--config-dir", mcc.util.unixPath(datadir));
            sc.addopt("--config-file", mcc.util.unixPath(datadir) + 
                      "config.ini");
	} else {
            sc.addopt("--config-dir", mcc.util.unixPath(datadir));
            sc.addopt("--config-file", mcc.util.unixPath(datadir) + 
                      "config.ini");
	}

        return scmds;
    } 
    
    if (ptype == "ndbd" || ptype == "ndbmtd") {
        var isDoneFunction = function () { return mcc.gui.getStatii(nodeid) == "STARTING"; };
        if (isWin) {
            sc.addopt("--install", "N"+nodeid);            
            sc.progTitle = "Installing node "+nodeid+" ("+ptype+
                ") as service N"+nodeid;
            sc.msg.procCtrl.noRaise = 1; // --install returns 1 on success
            
            var ss = new ProcessCommand(host, "", "net");
            delete ss.msg.file.autoComplete; // Don't want ac for net cmd
            ss.addopt("start");
            ss.addopt("N"+nodeid);
            ss.progTitle = "Starting service N"+nodeid;

            ss.isDone = isDoneFunction;
            lastNdbdCmd = ss;
            scmds.push(ss);
        } else {
	    sc.isDone = isDoneFunction; 
            lastNdbdCmd = sc;
        }
        
        sc.addopt("--ndb-nodeid", nodeid);
        sc.addopt("--ndb-connectstring", connectString);
        ndbdids.push(nodeid);
        return scmds;
    }
    
    if (ptype == "mysqld") {
        // Change isDone for the last data node to make it wait for all
        // ndbds to become STARTED
        lastNdbdCmd.isDone = function () {
	    for (var i in ndbdids) {
	        if (mcc.gui.getStatii(ndbdids[i]) != "STARTED") { 
	            mcc.util.dbg("Still waiting for node "+ndbdids[i]);
	            return false; 
	        }
	    }
	    return true;
        };

        var isDoneFunction = function () { return mcc.gui.getStatii(nodeid) == "CONNECTED"; };
      
        // With FreeSSHd (native windows) we need to run install_db command
        // inside cmd.exe for redirect of stdin (FIXME: not sure if that 
        // will work on Cygwin
        if (isWin) {
            var langdir = basedir + "share";
            var tmpdir = datadir_ + "tmp";
			var midb = new ProcessCommand(host, tmpdir, "mysql_install_db.bat");
			midb.progTitle = "Running mysql_install_db.bat for node "+nodeid;
			scmds.unshift(midb);
			
            var ic = new ProcessCommand(host, "C:\\Windows\\System32", "cmd.exe");
            delete ic.msg.file.autoComplete; // Don't want ac for cmd.exe
            ic.addopt("/C");
            ic.addopt(basedir+"bin\\mysqld.exe");

            ic.addopt("--lc-messages-dir", mcc.util.unixPath(langdir));
            ic.addopt("--bootstrap");
            ic.addopt("--basedir", mcc.util.unixPath(basedir));
            ic.addopt("--datadir", datadir);
            ic.addopt("--tmpdir", mcc.util.unixPath(tmpdir));
            ic.addopt("--log-warnings", "0");
            ic.addopt("--loose-skip-ndbcluster");
            ic.addopt("--max_allowed_packet","8M");
            ic.addopt("--default-storage-engine","myisam");
            ic.addopt("--net_buffer_length","16K");

            ic.addopt("<");
            ic.addopt(tmpdir + "\\install.sql");
            ic.progTitle = "Running mysqld --bootstrap for node "+nodeid;
            //scmds.unshift(ic);

            sc.addopt("--install");
            sc.addopt("N"+nodeid);
            sc.addopt("--defaults-file", datadir+"my.cnf");
            sc.msg.procCtrl.noRaise = 1; // --install returns 1 on success
            sc.progTitle = "Installing node "+nodeid+" ("+ptype+
            ") as service N"+nodeid;

            var ss = new ProcessCommand(host, "", "net");
            delete ss.msg.file.autoComplete; // Don't want ac for net cmd
            ss.addopt("start");
            ss.addopt("N"+nodeid);
            ss.progTitle = "Starting service N"+nodeid;
            ss.isDone = isDoneFunction;
            scmds.push(ss);
        } 
        else {
            // Non-windows uses install_db script
            var ic = new ProcessCommand(host, basedir, "mysql_install_db");
            ic.addopt("--no-defaults");
            ic.addopt("--datadir", datadir);
            ic.addopt("--basedir", basedir);
            ic.progTitle = "Running mysql_install_db for node "+nodeid;
            scmds.unshift(ic);

            sc.addopt("--defaults-file", datadir+"my.cnf");
            // Invoking mysqld does not return
            sc.msg.procCtrl.waitForCompletion = false;
            // Use nohup on it so python server won't hang in shutdown
            sc.msg.procCtrl.nohup = true;
            sc.msg.procCtrl.getStd = false;
            sc.msg.procCtrl.daemonWait = 10;
            sc.progTitle = "Starting node "+nodeid+" ("+ptype+")";
	    sc.isDone = isDoneFunction;            
        }

        return scmds;
    }
    alert("Not supposed to happen!");
    return null;
}


// Generate the stop command for the given process
function getStopProcessCommands(process) {
    
    var nodeid = process.getValue("NodeId");
    var hostItem = clusterItems[process.getValue("host")];
    var host = hostItem.getValue("name");
    var isWin = mcc.util.isWin(hostItem.getValue("uname"));

    var ptypeItem = clusterItems[process.getValue("processtype")];
    var ptype = ptypeItem.getValue("name");

    var basedir = mcc.util.unixPath(getEffectiveInstalldir(hostItem));

    var stopCommands = [];
    if (ptype == "ndb_mgmd") {
        var sc = new ProcessCommand(host, basedir, "ndb_mgm"+(isWin?".exe":""));
        sc.addopt("--ndb-connectstring", getConnectstring());
        sc.addopt("--execute", "shutdown");
        sc.progTitle = "Running ndb_mgm -e shutdown to take down cluster";

	if(!isWin) {
	  sc.isDone = function () 
	    { return mcc.gui.getStatii(nodeid) =="UNKNOWN" };
	}
        stopCommands.push(sc);
    }

    if (ptype == "mysqld") {
        var sc = new ProcessCommand(host, basedir, 
                                    "mysqladmin"+(isWin?".exe":""));
        sc.addopt("--port", getEffectiveInstanceValue(process, "Port"));
        sc.addopt("--user", "root");
        sc.addopt("shutdown");
        sc.addopt("--socket", mcc.util.quotePath(
            getEffectiveInstanceValue(process, "Socket")));
        sc.progTitle = "mysqldadmin shutdown on node "+nodeid;
        sc.nodeid = nodeid;
	if (!isWin) {
	  sc.isDone = function () 
	    { return mcc.gui.getStatii(nodeid) == "NO_CONTACT" };
	}
        stopCommands.push(sc);
    }
 
    if (isWin) {
        ssc = new ProcessCommand(host, "", "net");
        delete ssc.msg.file.autoComplete; // Don't want ac for net cmd
        ssc.addopt("stop");
        ssc.addopt("N"+nodeid);
        ssc.progTitle = "Stopping service N"+nodeid;

        //ssc.msg.procCtrl.noRaise = 1; // --remove returns 1 on success
        //stopCommands.push(ssc); 2 -> already stopped

        rsc = new ProcessCommand(host, basedir, 
                                 ptypeItem.getValue("name")+(isWin?".exe":""));
       
		rsc.check_result = function (rep) { 
			if (rep.body.out.search(/Service successfully removed/) != -1 ||
				rep.body.out.search(/The service doesn't exist/) != -1) {
				return "ok";
			}
			if (rep.body.out.search(/Failed to remove the service/) != -1) {
				return "retry";
			}
			return "error";
		};
		if(ptype == "mysqld") {
            rsc.addopt("--remove");
            rsc.addopt("N"+nodeid);
        } else {
            rsc.addopt("--remove", "N"+nodeid);
        }
        rsc.progTitle = "Removing service N"+nodeid;
		rsc.msg.isCommand = true;
        stopCommands.push(rsc);
    }

    return stopCommands;
}


function _getClusterCommands(procCommandFunc, families) {
    var procItems = getProcessItemsByFamilies(families);
    
    var commands = [];
    for (var pix in procItems) {
        commands = commands.concat(procCommandFunc(procItems[pix]));
    }
    return commands;
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
  var timeout = null;
    
  deployCluster(true, 10).
    then(function (deployed) {
	if (!deployed) {
	  mcc.util.dbg("Not starting cluster due to previous error");
	  return;
	}
	
	mcc.util.dbg("Starting cluster...");
	var commands = _getClusterCommands(getStartProcessCommands, 
					   ["management", "data", "sql"]);
	var currseq = 0;

	function onTimeout() {
	  if (commands[currseq].isDone()) {
	    ++currseq;
	    updateProgressAndStartNext();
	  } else {
	    console.log(commands[currseq].isDone);
	    mcc.util.dbg("returned false for "+commands[currseq].progTitle)
	    timeout = setTimeout(onTimeout, 2000);
	  }	    
	}
	  
	function onError(errMsg) {
	  alert(errMsg);
	  removeProgressDialog();
	  waitCondition.resolve();
	}

	function onReply(rep) {
	  mcc.util.dbg("Got reply for: "+commands[currseq].progTitle);
	  console.log(rep.body);
	  // Start status polling timer after mgmd has been started
	  // Ignore errors since it may not be available right away           
	  if (currseq == 0) { mcc.gui.startStatusPoll(false); } 
	  onTimeout();
	}
        
	function updateProgressAndStartNext() {
	  if (currseq >= commands.length) {
	    mcc.util.dbg("Cluster started");
	    updateProgressDialog("Starting cluster", 
				 "Cluster started", 
				 {progress: "100%"});
	    alert("Cluster started");
	    removeProgressDialog();
	    waitCondition.resolve();
	    return;
	  }
	  mcc.util.dbg("commands["+currseq+"].progTitle: " + 
		       commands[currseq].progTitle);
	  updateProgressDialog("Starting cluster",
			       commands[currseq].progTitle,
			       {maximum: commands.length, 
				   progress: currseq});
	  
	  mcc.server.doReq("executeCommandReq", 
			   {command: commands[currseq].msg}, cluster,
			   onReply, onError);
	  
	} 
	// Initiate startup sequence by calling onReply
	updateProgressAndStartNext();            
      });
  
  return waitCondition;
}

// Stop cluster
function stopCluster() {
    // External wait condition
    var waitCondition = new dojo.Deferred();
    mcc.util.dbg("Stopping cluster...");

    var commands = _getClusterCommands(getStopProcessCommands, 
                                       ["sql", "management", "data"]);
    var currseq = 0;
    var errorReplies = 0;
    var timeout;

    function onTimeout() {
      if (commands[currseq].isDone()) {
	++currseq;
	updateProgressAndStopNext();
      } else {
	timeout = setTimeout(onTimeout, 2000);
      }	    
    }

    function onError(errMsg, errReply) {
        mcc.util.dbg("stopCluster failed: "+errMsg);
        alert("Error occured while stopping cluster: `"+errMsg+
	      "' (Press OK to continue)");
        ++errorReplies;

        var cwpb = dijit.byId("configWizardProgressBar");
        var visualTile = dojo.query(".dijitProgressBarTile", cwpb.domNode)[0];
        visualTile.style.backgroundColor = "#FF3366";
	++currseq;
        updateProgressAndStopNext();
    }

    function onReply(rep) {
      mcc.util.dbg("Got reply for: "+commands[currseq].progTitle);
	  var cc = commands[currseq];
	  console.log(rep.body, cc)
	  if (cc.msg.isCommand) {
		result = cc.check_result(rep);
		if (result == "retry") {
			alert("check_status returned 'retry', retry in 2 sec...");
			setTimeout(updateProgressAndStopNext, 2000);
			return;
		}
		if (result == "error") {
			onError(rep.body.out, rep);
			return;
		}
	  }
      onTimeout();
    }
    
    function updateProgressAndStopNext() {
      if (currseq >= commands.length) {
	var message = errorReplies ? 
	  "Stop procedure has completed, but " + errorReplies + " out of " + 
	  commands.length + " commands failed" : 
	  "Cluster stopped successfully";

	mcc.util.dbg(message);
	updateProgressDialog(message, "", {progress: "100%"});
	alert(message);
	removeProgressDialog();
	mcc.gui.stopStatusPoll();
	waitCondition.resolve();
	return;
      }

      mcc.util.dbg("Stopping cluster: `" + commands[currseq].progTitle + "'");
      updateProgressDialog("Stopping cluster" + 
			   (errorReplies ? 
			    " (" + errorReplies + " failed command(s))":
			    ""), 
			   commands[currseq].progTitle, 
			   {maximum: commands.length, progress: currseq });
      mcc.server.doReq("executeCommandReq", 
		       {command: commands[currseq].msg}, cluster, 
		       onReply, onError);
    }

    // Initiate stop sequence
    updateProgressAndStopNext();            
    return waitCondition; 
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Configuration deployment module initialized");
});

