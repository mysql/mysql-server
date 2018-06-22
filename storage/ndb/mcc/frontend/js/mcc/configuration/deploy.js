/*
Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
 *      mcc.configuration.deploy.getConfigurationFile: Get lines for config.ini and my.cnf
 *      mcc.configuration.deploy.deployCluster: Create dirs, distribute files
 *      mcc.configuration.deploy.startCluster: Deploy configuration, start procs
 *      mcc.configuration.deploy.stopCluster: Stop processes
 *      mcc.configuration.deploy.installCluster: (Optionally) Install Cluster on requested host(s).
 *      mcc.configuration.deploy.clServStatus: Check state of Cluster services.
 *      mcc.configuration.deploy.determineClusterRunning: Determine just if Cluster services are in any sort of running state.
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
 *      getInstallCommands: List of commands to install Cluster on host(s).
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
mcc.configuration.deploy.installCluster = installCluster;
mcc.configuration.deploy.clServStatus = clServStatus;
mcc.configuration.deploy.determineClusterRunning = determineClusterRunning;

// List of statuses: "CONNECTED", "STARTED", "STARTING", "SHUTTING_DOWN", "NO_CONTACT", "UNKNOWN" + "undefined"
function clServStatus() {
    // Check all status information in the process tree
    var stat = [];
    mcc.storage.processStorage().getItems().then(function (processes) {
        for (var p in processes) {
            // No need to check API nodes.
            if (processes[p].item.processtype[0] < 4) {
                mcc.storage.processTreeStorage().getItem(processes[p].item.id[0]).then(
                function (proc) {
                    // No need to iterate over top level items.
                    if (mcc.storage.processTreeStorage().store().getValue(proc.item, "type") == "process") {
                        stat.push(mcc.storage.processTreeStorage().store().getValue(proc.item, "name") + ":" + 
                            mcc.storage.processTreeStorage().store().getValue(proc.item, "status"));
                    };
                });
            };
        };
        mcc.util.dbg("STAT is: " + stat);
    });
    return stat;
    // Simple Cluster, NOT started, stat is:
    //0:  "Management node 1:undefined"
    //1:  "SQL node 1:undefined"
    //2:  "Multi threaded data node 1:undefined"
    //then Management node 1:CONNECTED,SQL node 1:UNKNOWN,Multi threaded data node 1:NO_CONTACT
    // and so on.
}

function determineClusterRunning(clSt) {
    if (clSt.length <= 0) { return false;};
    var inp = [];
    for (var i in clSt) {
        inp = clSt[i].split(":");
        if (["CONNECTED", "STARTED", "STARTING", "SHUTTING_DOWN"].indexOf(inp[1]) >= 0) {
            mcc.util.dbg(inp[0] + " is in running state.")
            return true;
        }
    }
    return false;
}
/******************************* Internal data ********************************/

var cluster = null;         // The cluster item
var clusterItems = [];      // All items, indexed by their id
var hosts = [];             // All hosts            --- " ---
var processes = [];         // All processes        --- " ---
var processTypes = [];      // All process types    --- " ---
var processTypeMap = {};    // All process types indexed by name
var processFamilyMap = {};  // All process types indexed by family
var fileExists = [];        // All mysqlds - Nodeid and a boolean telling if datadir files exists  
var forceStop = false;

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
        alert("With an odd number of data nodes, the number of replicas " +
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

            //Temporary fix for Windows malloc/touch slowness
            if ((processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndbd" || 
                processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndbmtd") && (mcc.util.isWin(hosts[h].getValue("uname")))) {
                var DataMem = getEffectiveInstanceValue(proc, 
                        "DataMemory"); //MB
                if (DataMem > 20480) {
                    if (!confirm("Please note that the current values of the " +
                            "data layer configuration parameter " +
                            "DataMemory might be too big for Windows. " +
                            "Please press the Cancel button below to cancel deployment " + 
                            "and lower the value to <= 20480MB, or " +
                            "press OK to continue (but Cluster might not start). ")) {
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

            // Only ndb_mtd processes have ServerPort
            if ((processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndbmtd") || 
                (processTypes[proc.getValue("processtype")].getValue("name") == 
                    "ndbd")){
                port = getEffectiveInstanceValue(proc, "ServerPort");
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

// Generate configuration files
function getConfigurationFile(process) {
    /*
        Hard-coded process type store defines ID range, name and family for each process:
        {
        "identifier": "id",
        "label": "name",
        "items": [
            {
                "id": 0,
                "name": "ndb_mgmd",
                "family": "management",
                "familyLabel": "Management layer",
                "nodeLabel": "Management node",
                "minNodeId": 49,
                "maxNodeId": 255,
                "currSeq": 2
            },
        which is linked to process store holding processes on hosts 
        (ProcessTypeStore.ID == ProcessStore.ProcessType):
        {
            "identifier": "id",
            "label": "name",
            "items": [
                {
                    "id": 8,
                    "name": "Management node 1",
                    "host": 6,
                    "processtype": 0,
                    "NodeId": 49,
                    "seqno": 1
                },
        which is in turn linked to host tree store which we loop cause
        it tells us all process belonging to each host
        (ProcessStore.Host == HostTreeStore.ID):
            {
                "id": 6,
                "type": "host",
                "name": "127.0.0.1",
                "processes": [
                    {
                        "_reference": 8
                    },
                    {
                        "_reference": 9
                    },
                    {
                        "_reference": 10
                    },
                    {
                        "_reference": 11
                    },
                    {
                        "_reference": 12
                    }
                ]
            },
        and links to (also in hosttree store, referrence == ID):
            {
                "id": 8,
                "type": "process",
                "name": "Management node 1"
            },
            {
                "id": 9,
                "type": "process",
                "name": "API node 1"
            },
        which is linked to host store to provide humanly readable info (ID to ID):
        {
            {
                "id": 6,
                "name": "127.0.0.1",
                "anyHost": false,
                "hwResFetch": "OK",
                "hwResFetchSeq": 2,
                "ram": 524155,
                "cores": 88,
                "uname": "Windows",
                "osver": "10",
                "osflavor": "Microsoft Windows Server 2016 Standard",
                "dockerinfo": "NOT INSTALLED",
                "installdir_predef": true,
                "datadir_predef": true,
                "diskfree": "456G",
                "fqdn": "...",
                "internalIP": "...",
                "openfwhost": false,
                "installonhost": false,
                "installonhostrepourl": "",
                "installonhostdockerurl": "",
                "installonhostdockernet": "",
                "installdir": "F:\\SomeDir\\",
                "datadir": "C:\\Users\\user\\MySQL_Cluster\\"
            },
        So, this code loops all the families and types of processes over all hosts
        and parameters values making necessary entries to configuration files.
        There are 4 types of parameter values fetched:
            1) Default parameter value.
            2) Top-level parameter value (i.e. values that go into [DEFAULT] sections).
            3) Process-level parameter values.
            4) User modified parameter values.
        If value is undefined, we skip processing it.
        If value is not visible on certain level, we skip processing it
            (parameters.js::VisibleType/VisibleInstance).
        Range check of parameter values is done elsewhere.
        User-modified values have greatest weight.
        
        Initially, all parameters are set to DefaultValueType from parameters.js. User
        modifications are saved in appropriate store so it can be recreated upon loading
        configuration.
        
        For composing config.ini, we loop all parameters from families MANAGEMENT and DATA
            found in parameters.js.
        For composing my.cnf, we loop all parameters from family SQL found in parameters.js.
        Parameters in family API are not processed.
    */
    var hostItem = clusterItems[process.getValue("host")];
    var ptype = clusterItems[process.getValue("processtype")].getValue("name");

    if (ptype == "api") { return null; }

    // Structure to return
    var configFile = {
        host: hostItem.getValue("name"),
        fqdn: hostItem.getValue("fqdn"), //don't think it's really necessary here.
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
        //Hardcoded things for my.cnf mainly from Cluster and Host levels.    
        configFile.name = "my.cnf";
        addln(configFile, "# Configuration file for "
              + cluster.getValue("name"));
        addln(configFile, "# Generated by mcc");
        addln(configFile, "#");
        addln(configFile, "[mysqld]");
        addln(configFile, "log-error=mysqld."+process.getValue("NodeId")+".err")
        addln(configFile, "datadir=\""+configFile.path+"data"+"\"");
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
        var family = "sql";
        // Get the prototypical process type, output header
        var ptype = processFamilyMap[family];
        var ptypes = processFamilyTypes(family);
        for (var t in ptypes) {
            // Loop over all processes of this type
            var processes = processTypeInstances(ptypes[t].
                    getValue("name"));
            for (var p in processes) {
                var id = processes[p].getId();
                ptype.getValue("name")

                for (var para in mcc.configuration.getAllPara(family)) {
                    //Instance value of parameter.
                    var val = processes[p].getValue(para);
                    
                    //Is it for this concrete node?
                    if ((para == "NodeId") && (val != process.getValue("NodeId"))) {break;};
                    
                    //These are processed separately above.
                    if (para == "NodeId") {continue;};
                    if (para == "HostName") {continue;};
                    if (para == "DataDir") {continue;};
                    if (para == "Portbase") {continue;};
                    if (para == "Port") {continue;};
                    if (para == "Socket") {continue;};
                    
                    //Is parameter meant for different config file (parameters.js::destination)?
                    if (mcc.configuration.getPara(family, null, para, "destination") 
                        == "config.ini") {continue;};

                    //DEFAULT value, if any from parameters.js::DefaultValueType.
                    val = getEffectiveInstanceValue(processes[p], para);
                    
                    //No suffixes for now.
                    suffix = "";
                    
                    //Value on upper level (i.e. for all nodes of the same family).
                    var tVal = undefined;
                    
                    //Parameter name, if any.
                    var pname = mcc.configuration.getPara(family, null, para, "attribute");
                    
                    var usrVal = undefined;
                    if (pname != undefined) {
                        try {
                            tVal = getEffectiveTypeValue(processFamilyMap[family], pname);
                        } catch (e) {
                            tVal = undefined;
                        }
                        //Value on the process level.
                        usrVal = process.getValue(pname);
                    };
                    //User modified value always takes precedence.
                    if (usrVal != undefined) {
                        mcc.util.dbg("Writing usrval");
                        //Write to my.cnf USER modified value (if any).
                        if (usrVal == false) { usrVal = 0};
                        if (usrVal == true) { usrVal = 1};
                        addln(configFile, mcc.configuration.getPara(
                            family, null, para, "attribute")
                            + "=" + usrVal + suffix);
                        continue;
                    } else {
                        //Otherwise, check that DEFAULT section value exists.
                        if (tVal != undefined) {
                            mcc.util.dbg("Writing tVal");
                            //Write to my.cnf default value (if any).
                            if (tVal == false) { tVal = 0};
                            if (tVal == true) { tVal = 1};
                            addln(configFile, mcc.configuration.getPara(
                                family, null, para, "attribute")
                                + "=" + tVal + suffix);
                            continue;
                        } else {
                            //Try writing default parameter value, if any.
                            if (val != undefined) {
                                mcc.util.dbg("Writing val");
                                //Write to my.cnf default value (if any).
                                if (val == false) { val = 0};
                                if (val == true) { val = 1};
                                addln(configFile, mcc.configuration.getPara(
                                    family, null, para, "attribute")
                                    + "=" + val + suffix);
                                continue;
                            }
                        }
                    }    
                }
            }
        };
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

// Get the connect string for this cluster, use FQDN/internalIP and not hostname.
function getConnectstring() {
    var connectString = "";
    var mgmds = processTypeInstances("ndb_mgmd");
    // Loop over all ndb_mgmd processes
    for (var i in mgmds) {
        var port = getEffectiveInstanceValue(mgmds[i], "Portnumber");
        // Use internal IP instead of name:
        var host = clusterItems[mgmds[i].getValue("host")].getValue("internalIP");
        connectString += host + ":" + port + ",";
    }
    return connectString;
}

/*************************** Progress bar handling ****************************/

// Show progress bar
function updateProgressDialog(title, subtitle, props) {
    //Reset variable.
    forceStop = false;
    //Determine who called update by examining title.
    var firstWord = title.replace(/ .*/,'');
    if (["Deploying", "Installing", "Starting", "Stopping"].indexOf(firstWord) < 0) {
        firstWord = "Stopping";
    }
    //We know which procedure is running now.
    //Pass the info to dialog setup.
    if (!dijit.byId("progressBarDialog")) {
        progressBarDialogSetup(firstWord);
        dijit.byId("progressBarDialog").show();
    }

    dijit.byId("progressBarDialog").set("title", title);
    dojo.byId("progressBarSubtitle").innerHTML = subtitle;
    dijit.byId("configWizardProgressBar").update(props);
}

// Setup a dialog for showing progress
function progressBarDialogSetup(procRunning) {
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
                </div>",
                _onKey: function() { }
        });
    }
    //Trap the CLOSE button only for START/INSTALL Cluster.
    console.log("PROCRUNNING IS ::: " + procRunning);
    if (procRunning != "Stopping") { //"Deploying" runs before "Starting" so can't exclude it.
        dojo.style(dijit.byId("progressBarDialog").closeButtonNode,"display","true");
        pBarDlg.onCancel=function(evt){
            mcc.util.dbg("Operation " + procRunning + " cluster has been cancelled!");
            var clRunning = clServStatus();
            dijit.byId("configWizardStopCluster").setDisabled(!determineClusterRunning(clRunning));
            dijit.byId("configWizardStartCluster").setDisabled(determineClusterRunning(clRunning));
            dijit.byId("configWizardDeployCluster").setDisabled(true);
            forceStop = true;
        }
    } else {
        dojo.style(dijit.byId("progressBarDialog").closeButtonNode,"display","none");
    }
}

function removeProgressDialog() {
    if (dijit.byId("progressBarDialog")) {
        dijit.byId("progressBarDialog").destroyRecursive();
    }
}

/****************** Handling install commands display *************************/
function commandsDialogSetup(instCmd) {
    /*
    var commandsDlg = null;
    // Create the dialog if it does not already exist
    if (!dijit.byId("commandsDlg")) {
        commandsDlg= new dijit.Dialog({
            id: "commandsDlg",
            title: "Review and approve install commands",
            style: {width: "500px"},
            content: "\
                <form id='reviewInstallCommandsForm' data-dojo-type='dijit.form.Form'>\
                    <p>Please confirm running install commands.\
                    </p>\
                    <p>\
                    <br />Commands: \
                    <br /><span id='sd_installCommands'></span>\
                    </p>\
                    <div data-dojo-type='dijit.form.Button' type='submit'\
                        id='confirmInstallCommands'>OK\
                    </div>\
                    <div data-dojo-type='dijit.form.Button' type='button'\
                        id='cancelInstallCommands'>CANCEL\
                    </div>\
                </form>"
        });

        // Define widgets
        var sd_installCommands = new dijit.form.SimpleTextarea({
            //disabled: true,
            rows: "30",
            columns: "1",
            style: "width: 450px;"},// resize : none
            "sd_installCommands");
    }
    dijit.byId("sd_installCommands").setValue(dojo.toJson(instCmd));
    */
    var res = confirm("Confirm executing following commands:\n\n"+dojo.toJson(instCmd));
    return res;
}


/****************** Directory and startup command handling ********************/

function isFirstStart(nodeid) {
//Mysqld shall be started for the first time if the file does not exist in datadir
    for (i = 0; i < fileExists.length; i++) { 
        if (fileExists[i].nodeid == nodeid) {
            mcc.util.dbg("IsFirstStart ("+nodeid+") returns " + fileExists[i].fileExist);
            return !fileExists[i].fileExist;
        }
    }
    mcc.util.dbg("isFirstStart failed - should never happen");
    return false;
}

function getCheckCommands() {

    // Array to return
    var checkDirCommands = [];

    var processes = processTypeInstances("mysqld");

    // Loop over all mysqld processes
    for (var i in processes) {
        var process = processes[i];

        // Get process type, nodeid and host
        var ptype = clusterItems[process.getValue("processtype")];
        var nodeid = process.getValue("NodeId");
        var host = clusterItems[process.getValue("host")];

         // Get datadir and dir separator
         var datadir = mcc.util.unixPath(
                            mcc.util.terminatePath(
                            getEffectiveInstanceValue(process, "DataDir")));
         var dirSep = mcc.util.dirSep(datadir);

         // Initialize fileExist for the check command
         // The info will be updated by the result from checkFile in sendFileOps
         fileExists.push({
                          nodeid: nodeid,
                          fileExist: false
                       });
          // Push check file command
         checkDirCommands.push({
                        cmd: "checkFileReq",
                        host: host.getValue("name"),
                        path: datadir+"data"+dirSep,
                        name: "auto.cnf",  
                        msg: "File checked"               
                    });
    }
    return checkDirCommands;
}

// Generate the array of install commands for all required host(s).
function getInstallCommands() {
    // Array to return
    var installCommands = [];

    // Loop over all cluster hosts
    // Loop over all hosts and send install command(s) if requested.
    
      // openfwhost
      // installonhost
      // osver
      // osflavor
      // installonhostrepourl
      // installonhostdockerurl
      // installonhostdockernet IF empty THEN --net=host ELSE --net=FirstWordIn(installonhostdockernet, split by space))
      // dockerinfo (docker_info in back-end server) NOT INSTALLED, NOT RUNNING, RUNNING

    for (var h in hosts) {
        var dirs = [];
        var ports = [];

        var hostId = hosts[h].getId();
        var hostName = hosts[h].getValue("name");
        var instOnHost = hosts[h].getValue("installonhost");
        var instOnHostRepo = hosts[h].getValue("installonhostrepourl");
        var instOnHostDocker = hosts[h].getValue("installonhostdockerurl");
        var dockInf = hosts[h].getValue("dockerinfo");
        mcc.util.dbg("Check installation on host " + hostName);
        var platform = host[h].getValue("uname");
        //"WINDOWS", "CYGWIN", "DARWIN", "SunOS", "Linux"
        var flavor = host[h].getValue("osflavor");
        // RPM/YUM: "ol": OS=el, "fedora": OS=fc, "centos": OS=el, "rhel": OS=el, 
        // RPM:ZYpp: "opensuse": OS=sles
        // There is no "latest" for APT repo. Also, there is no way to discover newest.
        //DPKG/APT: "ubuntu": from APT, OS=ubuntu, "debian": from APT, OS=debian
        var ver = host[h].getValue("osver");
        mcc.util.dbg("Platform & OS details " + platform + ", " + flavor + ", " + ver);
        var array = ver.split('.');
        ver = array[0]; //Take just MAJOR

        /*Proto-message block:
                installCommands.push({
                    host: host.getValue("name") Host IP.
                    path: datadir,              DataDir.
                    name: null,                 Name of command.
                    terminal: false             Is failure terminal for operation?
                });

        COMMAND NAMES:
            CheckExists - Does MySQL installation exists on host?
            UpdManager  - Update package manager.
            AddUtils    - Install curl wget unzip zip. We actually need just wget for now.
            AddEPEL     - Tests use PERL :-/.
            Wget        - Get file from provided URL.
            Install     - Install REPO file.
            DisableMySQLd   - RPM manipulation.
            EnableCluster   - RPM manipulation.
            InstallMGMT - Commands to install MGMT node. Must install Cli too.
            InstallCli  - Commands to install client tools.
            InstallData - Commands to install DATA node.
            InstallSQL  - Commands to install mysqld.
        */
        if (instOnHost) {
            //Determine type of install:
            if (instOnHostRepo == "") {
                //Try Docker
                if (instOnHostDocker == "") {
                    //ERROR condition; INSTALL but both REPO and DOCKER urls are empty.
                    //Return empty array (meaning abort).
                    mcc.util.dbg("Both Docker and Repo URLs are empty for host " + hostName + "! Aborting.");
                    alert("Both Docker and Repo URLs are empty for host " + hostName + "! Aborting.");
                    installCommands = []; //Or installCommands.length = 0
                    break; //No installation will be done since one host failed completely.
                } else {
                    //DOCKER install
                    mcc.util.dbg("Will use Docker for host " + hostName + ".");
                    alert("Docker installation not functional yet!");
                    return installCommands;
                    
                    //Make list of commands.
                    installCommands.push({
                        host: hostName,
                        command: "Docker",
                        name: instOnHostDocker,
                        terminal: false
                    });
                    if (dockInf == "NOT INSTALLED") {
                        //We need to determine OS first.
                        installCommands.push({
                            host: hostName,
                            command: "CMD to install Docker.",
                            name: "",
                            terminal: false
                        });
                        installCommands.push({
                            host: hostName,
                            command: "CMD to start Docker.",
                            name: "",
                            terminal: false
                        });
                    } else {
                        if (dockInf == "NOT RUNNING") {
                            installCommands.push({
                                host: hostName,
                                command: "CMD to start Docker.",
                                name: "",
                                terminal: false
                            });
                        }                            
                    }
                    installCommands.push({
                        host: hostName,
                        command: "WGET docker image to ~install",
                        name: instOnHostRepo,
                        terminal: false
                    });
                }
            } else {
                //REPO install
                //1) Check for existing MySQL installation. If exists, bail out.
                //2) Update SW with wget and such
                //etc.
                mcc.util.dbg("Will use REPO for host " + hostName + ".");
                //platform, flavor, ver!
                if (platform == "Linux") {
                    if (["ol","centos","rhel"].indexOf(flavor) >= 0) {
                        //RPM/YUM
                        installCommands.push({
                            host: hostName,
                            command: "rpm -qa | grep mysql",
                            name: "CheckExists",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum update",
                            name: "UpdManager",
                            terminal: false
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum install curl wget unzip zip",
                            name: "AddUtils",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo rpm -ivh http://dl.fedoraproject.org/pub/epel/7/x86_64/Packages/e/epel-release-7-11.noarch.rpm",
                            name: "AddEPEL",
                            terminal: true
                        });
                        
                        installCommands.push({
                            host: hostName,
                            command: "wget " + instOnHostRepo,
                            name: "Wget",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo rpm -ivh " + instOnHostRepo,
                            name: "Install",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum-config-manager --disable mysql57-community",
                            name: "DisableMySQLd",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum-config-manager --enable mysql-cluster-7.6-community",
                            name: "EnableCluster",
                            terminal: true
                        });
                        //Put all commands in, we will check later which ones to run.
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum install mysql-cluster-community-management-server",
                            name: "InstallMGMT",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum install mysql-cluster-community-client",
                            name: "InstallCli",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum install mysql-cluster-community-data-node",
                            name: "InstallData",
                            terminal: true
                        });
                        installCommands.push({
                            host: hostName,
                            command: "sudo yum install mysql-cluster-community-server",
                            name: "InstallSQL",
                            terminal: true
                        });


                    } else {
                        if (["opensuse"].indexOf(flavor) >= 0) {
                            //RPM/ZYpp
                            mcc.util.dbg("Repo RPM/ZYpp installation selected for host " + hostName + ".");
                            alert("REPOSITORY RPM/ZYpp installation not functional yet!");
                            return installCommands;
                        } else {
                            if (["ubuntu","debian"].indexOf(flavor) >= 0) {
                                //DPKG/APT
                                mcc.util.dbg("Repo DPKG/APT installation selected for host " + hostName + ".");
                                alert("REPOSITORY DPKG/APT installation not functional yet!");
                                return installCommands;
                            } else {
                                if (["fedora"].indexOf(flavor) >= 0) {
                                    //RPM/DNF from 22 up
                                    mcc.util.dbg("Repo RPM/DNF installation selected for host " + hostName + ".");
                                    alert("REPOSITORY RPM/DNF installation not functional yet!");
                                    return installCommands;
                                } else {
                                    //Unknown Linux...
                                    mcc.util.dbg("Repo installation on unknown OS selected for host " + hostName + ".");
                                    alert("REPOSITORY installation not functional for unknown OS!");
                                    return installCommands;
                                }
                            }
                        }
                    }
                } else {
                    if (platform == "WINDOWS") {
                        mcc.util.dbg("Repo Windows installation selected for host " + hostName + ".");
                        alert("REPOSITORY installation not available for Windows!");
                        return installCommands;
                    } else {
                        if (platform == "DARWIN") {
                            mcc.util.dbg("Repo MacOSX installation selected for host " + hostName + ".");
                            alert("REPOSITORY installation not available for MacOSX!");
                            return installCommands;
                        } else {
                            if (platform == "SunOS") {
                                mcc.util.dbg("Repo Solaris installation selected for host " + hostName + ".");
                                alert("REPOSITORY installation not available for Solaris!");
                                return installCommands;
                            } else {
                                //Unknown platform!
                                mcc.util.dbg("Repo installation on unknown OS selected for host " + hostName + ".");
                                alert("REPOSITORY installation not available for unknown OS!");
                                return installCommands;
                            }
                        }
                    }
                }
            }
        };
    }    
    return installCommands;
}

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
                createDirCommands.push({
                    host: host.getValue("name"),
                    path: datadir + "tmp" + dirSep,
                    name: null
                });
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
                                    
    // Get connect string; FQDN and not hostname.
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
        // lastNdbdCmd COULD be NULL!!!
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

        if (isFirstStart(nodeid)) { 
            // First start of mysqld
            var fsc = new ProcessCommand(
                      host, 
                      basedir,
                      "mysqld"+(isWin?".exe":""));
            fsc.addopt("--defaults-file", datadir+"my.cnf");
            fsc.addopt("--initialize-insecure");
            fsc.progTitle = "Initializing (insecure) node "+ nodeid +" ("+ptype+")";
            scmds.unshift(fsc);
        }
      
        // inside cmd.exe for redirect of stdin (FIXME: not sure if that 
        // will work on Cygwin
        if (isWin) {
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
        //This is CLIENT command thus probably in /usr/bin and not /usr/sbin 
        //(default install directory) so we have to use autocomplete.
        if(!isWin) {
            var instDir = sc.msg.file.path;
            //NO path prefix!
            sc.msg.file.path = '';
            //Look for ndb_mgm in InstallDirectory, /usr/bin' and in PATH.
            //When properly installed, ndb_mgm will be in /usr/bin.
            //Being that there are configurations which do not point to 
            //binaries in InstallDir but rather one step up, I'm adding BIN and SBIN too.
            var fake1 = "";
            var fake2 = "";
            if (instDir.slice(-1) == "/") {
                fake1 = instDir + "bin";
                fake2 = instDir + "sbin";
            } else {
                fake1 = instDir + "/bin";
                fake2 = instDir + "/sbin";
            }
            sc.msg.file.autoComplete = [instDir, fake1, fake2, '/usr/bin/',''];
        }
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
        //This is CLIENT command thus probably in /usr/bin and not /usr/sbin 
        //(default install directory) so we have to use autocomplete.
        if(!isWin) {
            var instDir = sc.msg.file.path;
            //NO path prefix!
            sc.msg.file.path = '';
            //Look for mysqladmin in InstallDirectory, /usr/bin' and in PATH.
            //When properly installed, mysqladmin will be in /usr/bin.
            //Being that there are configurations which do not point to 
            //binaries in InstallDir but rather one step up, I'm adding BIN and SBIN too.
            var fake1 = "";
            var fake2 = "";
            if (instDir.slice(-1) == "/") {
                fake1 = instDir + "bin";
                fake2 = instDir + "sbin";
            } else {
                fake1 = instDir + "/bin";
                fake2 = instDir + "/sbin";
            }
            sc.msg.file.autoComplete = [instDir, fake1, fake2, '/usr/bin/',''];
        }
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
 
    if (createCmd.cmd == "checkFileReq") {
        // Assert if the file exists
        mcc.server.checkFileReq(
            createCmd.host,
            createCmd.path,
            createCmd.name,
            createCmd.msg,
            createCmd.overwrite,
            function () {
                fileExists[curr].fileExist=true; 
                curr++;
                if (curr == createCmds.length) {
                    waitCondition.resolve();
                } else {
                    sendFileOp(createCmds, curr, waitCondition);
                }
            },  
            function (errMsg) {
                mcc.util.dbg("File does not exits for "+curr);              
                fileExists[curr].fileExist=false; 
                curr++;
                if (curr == createCmds.length) {
                    waitCondition.resolve();
                } else {
                    sendFileOp(createCmds, curr, waitCondition);
                }    
            }
        );

    } else if (createCmd.cmd == "appendFileReq") {
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

// Install cluster: Installs cluster on host(s).
function installCluster(silent, fraction) {
    // External wait condition
    var waitCondition = new dojo.Deferred();
    var waitList = [];
    var waitAll = null;

    mcc.util.dbg("Preparing install commands...");
    // Get the install commands
    var installCmd = getInstallCommands();

    // Prevent additional error messages
    var alerted = false; 

    // Check if all went OK.
    if (installCmd.length <= 0) {
        //Bail out of procedure, host with wrong parameters detected.
        mcc.util.dbg("List of install commands for all hosts returns empty...");
        alert("No commands to execute, aborting.");
        waitCondition.resolve(false);
        return waitCondition;
    }
    mcc.util.dbg("Finished creating list of install commands...");
    //Show install commands to user and let him decide whether to continue.
    dijit.byId("commandsDlg").show();
    if (commandsDialogSetup(installCmd)) {
        //Continue with install.
        mcc.util.dbg("Executing install commands...");
        
    } else {
        //No install.
        mcc.util.dbg("You chose not to install Cluster.");
        waitCondition.resolve(false);
        return waitCondition;
        
    }
    //dijit.byId("commandsDlg").show();

    //If user choose to continue with installation, display progress.
/*
    updateProgressDialog("Installing Cluster on host(s)", 
            "Running install commands", 
            {maximum: fraction? fraction * installCmd.length : installCmd.length}
    );

    mcc.util.dbg("Executing install commands...");
*/
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
  var clRunning = [];
  
  clRunning = clServStatus();
  dijit.byId("configWizardStopCluster").setDisabled(!determineClusterRunning(clRunning));
  dijit.byId("configWizardStartCluster").setDisabled(determineClusterRunning(clRunning));
  
  deployCluster(true, 10).
    then(function (deployed) {
        if (!deployed) {
          mcc.util.dbg("Not starting cluster due to previous error");
          dijit.byId("configWizardDeployCluster").setDisabled(false);
          return;
        }

        //Check for files. If file exists, initialization will be skipped
        var checkCmds = getCheckCommands();
        sendFileOps(checkCmds).then(function () {        
            mcc.util.dbg("Starting cluster...");
            var commands = _getClusterCommands(getStartProcessCommands, 
                               ["management", "data", "sql"]);
            var currseq = 0;

            function onTimeout() {
              if (commands[currseq].isDone()) {
                ++currseq;
                updateProgressAndStartNext();
              } else {
                mcc.util.dbg("returned false for "+commands[currseq].progTitle)
                //This is where stuck commands end up.
                clRunning = clServStatus();
                dijit.byId("configWizardStopCluster").setDisabled(!determineClusterRunning(clRunning));
                dijit.byId("configWizardStartCluster").setDisabled(determineClusterRunning(clRunning));
                dijit.byId("configWizardDeployCluster").setDisabled(true);
                if (forceStop) {
                    mcc.util.dbg("Cluster start aborted!");
                    alert("Cluster start aborted!");
                    removeProgressDialog();
                    waitCondition.resolve();
                    return;
                }

                timeout = setTimeout(onTimeout, 2000);
              }
            }

            function onError(errMsg) {
                alert(errMsg);
                removeProgressDialog();
                clRunning = clServStatus();
                dijit.byId("configWizardStopCluster").setDisabled(!determineClusterRunning(clRunning));
 
                //Notify user of the location of log files.
                displayLogFilesLocation();
                
                waitCondition.resolve();
            }

            function onReply(rep) {
                mcc.util.dbg("Got reply for: "+commands[currseq].progTitle);
                // Start status polling timer after mgmd has been started
                // Ignore errors since it may not be available right away
                if (currseq == 0) {
                  mcc.gui.startStatusPoll(false);
                }
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

                    clRunning = clServStatus();
                    dijit.byId("configWizardStopCluster").setDisabled(!determineClusterRunning(clRunning));
                    dijit.byId("configWizardStartCluster").setDisabled(determineClusterRunning(clRunning));
                    dijit.byId("configWizardDeployCluster").setDisabled(true);
                    waitCondition.resolve();
                    return;
                }
                clRunning = clServStatus();
                dijit.byId("configWizardStopCluster").setDisabled(!determineClusterRunning(clRunning));
                dijit.byId("configWizardStartCluster").setDisabled(determineClusterRunning(clRunning));
                dijit.byId("configWizardDeployCluster").setDisabled(true);
                if (forceStop) {
                    mcc.util.dbg("Cluster start aborted!");
                    alert("Cluster start aborted!");
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
                /**
                 * Temporarily commented out
                if (forceStop) {
                   mcc.util.dbg("Cluster start aborted!");
                   alert("Cluster start aborted!");
                   removeProgressDialog();
                   waitCondition.resolve();
                   return;
                }
                */

                mcc.server.doReq("executeCommandReq",
                       {command: commands[currseq].msg}, cluster,
                       onReply, onError);
            } 
            // Initiate startup sequence by calling onReply
            updateProgressAndStartNext();
          });
    });
    return waitCondition;
}

function displayLogFilesLocation() {
    var processesOnHost = [];
    var redoLogChecked = false;
    for (var p in processes) {
        if (!processesOnHost[processes[p].getValue("host")]) {
            processesOnHost[processes[p].getValue("host")] = [];
        }
        // Append process to array
        processesOnHost[processes[p].getValue("host")].push(processes[p]);
    }

    // Do search for each host individually
    for (var h in hosts) {
        var dirs = [];

        var hostId = hosts[h].getId();
        var hostName = hosts[h].getValue("name");

        // One loop 
        for (var p in processesOnHost[hostId]) {
            // Process instance
            var proc = processesOnHost[hostId][p];

            // Various attributes
            var id = proc.getId();
            var nodeid = proc.getValue("NodeId");
            var name = proc.getValue("name");
            var dir = null;
            // All processes except api have datadir
            if (processTypes[proc.getValue("processtype")].getValue("name") != 
                    "api") {
                dir = hostName + ":" + processTypes[proc.getValue("processtype")].getValue("name") + ": " +  getEffectiveInstanceValue(proc, "DataDir");
            }
            if (dir) {
                // Store this process' datadir on its array index
                dirs[p] = dir;
            }
        }
    }
    if (dirs.length > 0) {
        var msg = "Please check log files in following locations for more clues:\n";
        for (var dir in dirs) {
            msg += dirs[dir] + "\n";
        };
        alert(msg);
    }
}

// Stop cluster
function stopCluster() {
    // CHECK if Cluster is running at all first.
    // External wait condition
    var old_forceStop = forceStop;
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
        //No need to *alert* on each failure since it is perfectly possible service was not even started.
        mcc.util.dbg("Error occurred while stopping cluster: "+errMsg);
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
      if (cc.msg.isCommand) {
        result = cc.check_result(rep);
        if (result == "retry") {
            mcc.util.dbg("Retrying: "+commands[currseq].progTitle);
            setTimeout(updateProgressAndStopNext, 2000);
            return;
        }
        if (result == "error") {
            onError(rep.body.out, rep);
            dijit.byId("configWizardStopCluster").setDisabled(true);
            dijit.byId("configWizardStartCluster").setDisabled(false);
            dijit.byId("configWizardDeployCluster").setDisabled(false);
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
        dijit.byId("configWizardStopCluster").setDisabled(true);
        dijit.byId("configWizardStartCluster").setDisabled(false);
        dijit.byId("configWizardDeployCluster").setDisabled(false);
        removeProgressDialog();
        mcc.gui.stopStatusPoll();
        waitCondition.resolve();
        if (old_forceStop) {
            //Notify user of the location of log files.
            displayLogFilesLocation();
        };
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
