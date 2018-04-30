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
 ***                   Configuration parameter calculations                 ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.configuration.calculations
 *
 *  Description:
 *      Calculating environment dependent parameters for processes and types
 *
 *  External interface:
 *      mcc.configuration.calculations.autoConfigure: Auto add processes
 *      mcc.configuration.calculations.instanceSetup: Predef instance params
 *      mcc.configuration.calculations.typeSetup: Predef type params
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      hwDepParams: Calculate hw dependent data node parameters
 *      ndb_mgmd_setup: ndb_mgmd process specific parameter assignments
 *      ndbd_setup: ndbd process specific parameter assignments
 *      mysqld_setup: mysqld process specific parameter assignments
 *
 *  Internal data: 
 *      None
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
        Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.configuration.calculations");

dojo.require("mcc.util");
dojo.require("mcc.storage");
dojo.require("mcc.configuration");

/**************************** External interface  *****************************/

mcc.configuration.calculations.autoConfigure = autoConfigure;
mcc.configuration.calculations.instanceSetup = instanceSetup;
mcc.configuration.calculations.typeSetup = typeSetup;

/****************************** Implementation  *******************************/

// Add processes to the cluster if none exists already
function autoConfigure() {
    var waitCondition = new dojo.Deferred();
    // If no processes, add
    mcc.storage.processStorage().getItems({}).then(function (items) {
        // Shortcut if we have processes already
        if (items && items.length > 0) {
            mcc.util.dbg("Processes already exist, not adding default")
            waitCondition.resolve();
            return;
        }
        mcc.storage.hostStorage().getItems({}).then(function (hosts) {
            var anyHost = null;

            // Remove the wildcard host from the list
            for (var i in hosts) {
                if (hosts[i].getValue("anyHost")) {
                    anyHost = hosts[i];
                    hosts.splice(i, 1);
                    break;
                }
            }

            // First sort by name to get repeatable allocation (needed for tests)
            hosts.sort(function (h1, h2) {
                if (h1.getValue("name") < h2.getValue("name")) {
                    return -1;
                } else {
                    return 1;
                }
            });

            // Shortcut if we have no hosts, or only wildcard host
            if (!hosts || hosts.length == 0 || 
                    (hosts.length == 1 && hosts[0].getValue("anyHost"))) {
                alert("No hosts - unable to add default processes")
                mcc.util.dbg("No hosts - unable to call add default processes")
                waitCondition.resolve();
                return;
            }
            
            var typeIds = [];
            var names = [];
            var familyHead = [];    // Ptype hashed on family name
            var typeHead = [];      // Ptype hashed on type name
            var dataNodeId = 1;
            var mgmtNodeID = 49;
            var sqlNodeID = 53;
            var apiNodeID = 231;
            var otherNodeId = 49; 

            // Get ids of all process types
            mcc.storage.processTypeStorage().forItems({}, function (pType) {
                if (!familyHead[pType.getValue("family")]) {
                    familyHead[pType.getValue("family")] = pType;
                }
                typeHead[pType.getValue("name")] = 
                        familyHead[pType.getValue("family")];
                typeIds[pType.getValue("name")] = pType.getId();
                names[pType.getValue("name")] = pType.getValue("nodeLabel");
            },
            function () {
                // Add new process following same ID rules set in MCCStorage.js::initializeProcessTypeStorage
                var NID = 0;
                function newProcess(pname, host) {
                    switch(pname) {
                        case "ndb_mgmd":
                            NID = mgmtNodeID;
                            mgmtNodeID++;
                            otherNodeId++;
                            break;
                        case "mysqld":
                            NID = sqlNodeID;
                            sqlNodeID++;
                            otherNodeId++;
                            break;
                        case "api":
                            NID = apiNodeID;
                            apiNodeID++;
                            otherNodeId++;
                            break;
                        default:
                            NID = dataNodeId;
                            dataNodeId++;
                    }

                    mcc.storage.processStorage().newItem({
                        name: names[pname] + " " + 
                                typeHead[pname].getValue("currSeq"),
                        host: host.getId(),
                        processtype: typeIds[pname],
                        NodeId: NID,
                        seqno: typeHead[pname].getValue("currSeq")
                    });
                }

                // Sort host array on RAM
                hosts.sort(function (a, b) {
                    // Treat unefined ram as smallest
                    if (!a.getValue("ram") && !b.getValue("ram")) {
                        return 0;
                    }
                    if (!a.getValue("ram")) {
                        return -1;
                    }
                    if (!b.getValue("ram")) {
                        return 1;
                    }

                    // Put largest ram at end where ndbds are allocated
                    if (+a.getValue("ram") < +b.getValue("ram")) {
                        return -1;
                    } else if (+a.getValue("ram") > +b.getValue("ram")) {
                        return 1;
                    } else {
                        return 0;
                    }
                });
                
                if (hosts.length == 1) {
                    // One host: 1*mgmd + 3*api + 2*mysqld + 2*ndbd
                    newProcess("ndb_mgmd", hosts[0]);
                    for (var i = 0; i < 3; ++i) {
                        newProcess("api", hosts[0]);
                    }
                    for (var i = 0; i < 2; ++i) {
                        newProcess("mysqld", hosts[0]);
                    }
                    for (var i = 0; i < 2; ++i) {
                        newProcess("ndbmtd", hosts[0]);
                    }
                } else if (hosts.length == 2) {
                    // Two hosts: 1*mgmd + 2*api + 1*mysqld + 1*ndbd, 
                    //            2*api + 1*mysqld + 1*ndbd 
                    newProcess("ndb_mgmd", hosts[0]);
                    for (var i = 0; i < 2; ++i) {
                        newProcess("api", hosts[0]);
                    }
                    newProcess("mysqld", hosts[0]);
                    newProcess("ndbmtd", hosts[0]);

                    for (var i = 0; i < 2; ++i) {
                        newProcess("api", hosts[1]);
                    }
                    newProcess("mysqld", hosts[1]);
                    newProcess("ndbmtd", hosts[1]);
                } else if (hosts.length == 3) {
                    // Three hosts: 1*mgmd + 3*api + 2*mysqld, 
                    //              1*ndbd, 
                    //              1*ndbd
                    newProcess("ndb_mgmd", hosts[0]);
                    for (var i = 0; i < 3; ++i) {
                        newProcess("api", hosts[0]);
                    }
                    for (var i = 0; i < 2; ++i) {
                        newProcess("mysqld", hosts[0]);
                    }
                    for (var i = 0; i < 2; ++i) {
                        newProcess("ndbmtd", hosts[i + 1]);
                    }

                } else if (hosts.length > 3) {
                    // N>3 hosts: First, divide hosts into groups
                    var nNDBD = Math.floor(hosts.length / 4) * 2;
                    var nSQL = hosts.length - nNDBD;

                    // Use 2 hosts for 1*mgmds + 2*api on each
                    for (var i = 0; i < 2; ++i) {
                        if (otherNodeId <= 255) {
                            newProcess("ndb_mgmd", hosts[i]);
                        }
                        if (otherNodeId <= 255) {
                            newProcess("api", hosts[i]);
                        }
                        if (otherNodeId <= 255) {
                            newProcess("api", hosts[i]);
                        }
                    }
                    // Possibly two more api on third host
                    if (hosts.length > 4) {
                        if (otherNodeId <= 255) {
                            newProcess("api", hosts[2]);
                        }
                        if (otherNodeId <= 255) {
                            newProcess("api", hosts[2]);
                        }
                    }
                    // Use N - (N DIV 4)*2 hosts for mysqlds, two on each
                    for (var i = 0; i < nSQL; ++i) {
                        if (otherNodeId <= 255) {
                            newProcess("mysqld", hosts[i]);
                        }
                        if (otherNodeId <= 255) {
                            newProcess("mysqld", hosts[i]);
                        }
                    }
                    // Use (N DIV 4)*2 hosts for data nodes, one on each
                    for (var i = nSQL; i < nSQL + nNDBD; ++i) {
                        if (dataNodeId <= 48) {
                            newProcess("ndbmtd", hosts[i]);
                        }
                    }
                }
                mcc.util.dbg("Default processes added")
                waitCondition.resolve();
            });
        });
    });
    return waitCondition;
}

// Calculate hw dependent data node parameters
function hwDepParams(processTypeName) {

    // Single deferred to callback
    var waitCondition = new dojo.Deferred();

    // Array of deferrers to wait for
    var waitConditions= [];
    var waitList; 

    // Fetch processes
    mcc.storage.processTypeStorage().getItems({name: processTypeName}).
            then(function (ptypes) {
        mcc.storage.processStorage().getItems({
                processtype: ptypes[0].getId()}).then(function (nodes) {
            for (var i in nodes) {
                // Run instance setup
                waitConditions[i] = instanceSetup(ptypes[0].
                        getValue("family"), nodes[i]);
            }

            // After looping over all processes, wait for DeferredList
            waitList = new dojo.DeferredList(waitConditions);
            waitList.then(function () {
                waitCondition.resolve();
            });
        });
    });

    return waitCondition;
}

// Calculate process type parameters depending on environment and external input
function typeSetup(processTypeItem) {

    var processFamilyName = processTypeItem.getValue("family");
    var waitCondition = new dojo.Deferred();
    mcc.util.dbg("Setup process type defaults for family " + processFamilyName);

    // Get the prototypical process type for this family
    mcc.storage.processTypeStorage().getItems({family: processFamilyName}).then(function(pTypes) {
        var processFamilyItem= pTypes[0]; 
        
        // Process type specific assignments
        if (processFamilyName == "management") {
            // Get portbase, set default port
            var pbase = processFamilyItem.getValue("Portbase");
            if (pbase === undefined) {
                pbase = mcc.configuration.getPara(processFamilyName, null, 
                        "Portbase", "defaultValueType");
            }
            mcc.configuration.setPara(processFamilyName, null, "Portnumber",
                    "defaultValueType", pbase);
            // Leave process type level datadir undefined
            waitCondition.resolve();
        } else if (processFamilyName == "data") {

            // Check parameters that depend on cluster defaults
            mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
                // Leave process type level datadir undefined

                // Check real time or web mode.
                // This is OK since all those values are "undefined" in parameters.ja.
                if (cluster.getValue("apparea") != "realtime") {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "HeartbeatIntervalDbDb", "defaultValueType", 15000);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "HeartbeatIntervalDbApi", "defaultValueType", 15000);
                } else {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "HeartbeatIntervalDbDb", "defaultValueType", 1500);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "HeartbeatIntervalDbApi", "defaultValueType", 1500);
                }

                // Check read/write load
                // This is OK since all those values are "undefined" in parameters.ja.
                if (cluster.getValue("writeload") == "high") {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "SendBufferMemory", "defaultValueType", 8);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "ReceiveBufferMemory", "defaultValueType", 8);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "RedoBuffer", "defaultValueType", 64);
                } else if (cluster.getValue("writeload") == "medium") {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "SendBufferMemory", "defaultValueType", 4);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "ReceiveBufferMemory", "defaultValueType", 4);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "RedoBuffer", "defaultValueType", 32);
                } else {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "SendBufferMemory", "defaultValueType", 2);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "ReceiveBufferMemory", "defaultValueType", 2);
                    mcc.configuration.setPara(processFamilyName, null, 
                            "RedoBuffer", "defaultValueType", 32);
                }

                // Get disk page buffer memory, assign shared global memory.
                var diskBuf = processFamilyItem.getValue(
                        mcc.configuration.getPara(processFamilyName, null, 
                                "DiskPageBufferMemory", "attribute"));
                if (!diskBuf) {
                    // If user didn't set it, read from parameters.js.
                    diskBuf = mcc.configuration.getPara(processFamilyName, null, 
                        "DiskPageBufferMemory", "defaultValueType");
                }
                
                // If user didn't set it, calculate.
                if (diskBuf > 8192) {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "SharedGlobalMemory", "defaultValueType", 1024);
                } else if (diskBuf > 64) {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "SharedGlobalMemory", "defaultValueType", 384);
                } else {
                    mcc.configuration.setPara(processFamilyName, null, 
                            "SharedGlobalMemory", "defaultValueType", 32);
                }

                // Restrict MaxNoOfTables
                var maxTab = processFamilyItem.getValue(
                        mcc.configuration.getPara(processFamilyName, null, 
                                "MaxNoOfTables", "attribute"));
                if (maxTab) {
                    //IF user has set it THEN do check.
                    if (maxTab > 20320) {
                        processFamilyItem.setValue(
                            mcc.configuration.getPara(processFamilyName, null, 
                                "MaxNoOfTables", "attribute"), 20320);
                    } else if (maxTab < 128) {
                        processFamilyItem.setValue(
                            mcc.configuration.getPara(processFamilyName, null, 
                                "MaxNoOfTables", "attribute"), 128);
                    }
                    mcc.storage.processStorage().save();
                }

                // Calculate datamem, indexmem, and maxexecthreads
                hwDepParams("ndbd").then(function () {
                    hwDepParams("ndbmtd").then(function () {

                        // Get predefined data node parameters
                        var params = mcc.configuration.getAllPara("data");

                        function setLow(param) {
                            var low = undefined;
                            // Loop over instance values, collect min, set
                            for (var i in params[param].defaultValueInstance) {
                                var curr = params[param].defaultValueInstance[i];
                                if (low === undefined || 
                                        (curr !== undefined && curr < low)) {
                                    low = curr;
                                }
                            }
                            mcc.util.dbg("Lowest value for " + param + 
                                    " now: " + low);
                            if (low !== undefined) {
                                mcc.configuration.setPara(processFamilyName, null, 
                                        param, "defaultValueType", low);
                            }
                        }

                        setLow("DataMemory");
                        setLow("IndexMemory");
                        setLow("MaxNoOfExecutionThreads");

                        // Get overridden redo log file size
                        var fileSz = processFamilyItem.getValue("FragmentLogFileSize");

                        // If not overridden, set value depending on app area
                        if (!fileSz) {
                            // Lower value if simple testing, easier on resources
                            if (cluster.getValue("apparea") == "simple testing") {
                                fileSz = 64;
                            } else {
                                fileSz = 256;                        
                            }
                            mcc.configuration.setPara(processFamilyName, null, 
                                    "FragmentLogFileSize", "defaultValueType", fileSz);
                        }
                        mcc.util.dbg("FragmentLogFileSize=" + fileSz);

                        // Caclulate and set number of files
                        var dataMem = mcc.configuration.getPara(processFamilyName, null, 
                                        "DataMemory", "defaultValueType");
                        var noOfFiles = 16;

                        // Use def value unless not simple testing and DataMem defined
                        if (cluster.getValue("apparea") != "simple testing" && dataMem) {
                            noOfFiles = Math.floor(6 * dataMem / fileSz / 4);
                        }

                        // At least three files in each set
                        if (noOfFiles < 3) {
                            noOfFiles = 3;
                        }
                        mcc.util.dbg("NoOfFragmentLogFiles=" + noOfFiles);
                        mcc.configuration.setPara(processFamilyName, null, 
                                "NoOfFragmentLogFiles", "defaultValueType", 
                                noOfFiles);

                        // Get number of data nodes
                        mcc.util.getNodeDistribution().then(function (nNodes) {
                            mcc.configuration.setPara(processFamilyName, null, 
                                    "NoOfReplicas", "defaultValueType", 
                                    2 - (nNodes['ndbd'] + nNodes['ndbmtd']) % 2);
                            waitCondition.resolve();
                        });
                    });
                });
            });
        } else if (processFamilyName == "sql") {
            // Get portbase, set default port
            var pbase = processFamilyItem.getValue("Portbase");
            if (pbase === undefined) {
                pbase = mcc.configuration.getPara(processFamilyName, null, 
                        "Portbase", "defaultValueType");
            }
            mcc.configuration.setPara(processFamilyName, null, "Port",
                    "defaultValueType", pbase);
            // Leave process type level socket and datadir undefined
            waitCondition.resolve();
        } else if (processFamilyName == "api") {
            waitCondition.resolve();
        }
    });
    return waitCondition;
}

// ndb_mgmd process specific parameter assignments
function ndb_mgmd_setup(processItem, processFamilyItem, host, waitCondition) {
    var id = processItem.getId();
    var datadir = mcc.util.unixPath(host.getValue("datadir"));
    var dirSep = mcc.util.dirSep(datadir);
    var processFamilyName = processFamilyItem.getValue("family");

    // Set datadir
    mcc.configuration.setPara(processFamilyName, id, "DataDir",
            "defaultValueInstance", datadir +
            processItem.getValue("NodeId") + dirSep);

    // Get colleague nodes, find own index on host
    mcc.util.getColleagueNodes(processItem).then(function (colleagues) {
        var myIdx = dojo.indexOf(colleagues, processItem.getId());

        // Get type's overridden port base
        var pbase = processFamilyItem.getValue("Portbase");

        // If not overridden, use type default
        if (pbase === undefined) {
            pbase = mcc.configuration.getPara(processFamilyName, null, 
                    "Portbase", "defaultValueType");
        }
        // Set port using retrieved portbase and node index on host
        mcc.configuration.setPara(processFamilyName, id, "Portnumber",
                "defaultValueInstance", myIdx + pbase);

        waitCondition.resolve();
    });
}

// ndbXd process specific parameter assignments
function ndbd_setup(processItem, processFamilyItem, host, waitCondition) {
    var id = processItem.getId();
    var datadir = mcc.util.unixPath(host.getValue("datadir"));
    var dirSep = mcc.util.dirSep(datadir);
    var processFamilyName = processFamilyItem.getValue("family");
        
    // Set datadir
    mcc.configuration.setPara(processFamilyName, id, "DataDir",
            "defaultValueInstance", datadir +
            processItem.getValue("NodeId") + dirSep);

    // Get cluster attributes
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        // Get node distribution (deferred)
        mcc.util.getNodeDistribution().then(function(nNodes) {
            var noOfMysqld= nNodes["mysqld"];
            var noOfNdbd= nNodes["ndbd"] + nNodes["ndbmtd"];
                        
            // Return overridden value, if defined, otherwise, return predefined
            function getRealValue(attr) {
                var val= processFamilyItem.getValue(attr);
                if (val === undefined) {
                    val= mcc.configuration.getPara(processFamilyName, null,
                        attr, "defaultValueType");
                }
                return val;
            }

            // Need these for calculations below
            var MaxNoOfTables= getRealValue("MaxNoOfTables");
            var sendreceive= getRealValue("SendBufferMemory");
            var DiskPageBufferMemory= getRealValue("DiskPageBufferMemory");
            var SharedGlobalMemory= getRealValue("SharedGlobalMemory");
            var RedoBuffer= getRealValue("RedoBuffer");

            // Change this setting if we support managing connection pooling
            var connectionPool= 1;

            // Temporary variables used in memory calculations
            var reserveMemoryToOS = 1024 * 1;
            var buffers = 300 * 1;
            var tableObjectMemory = MaxNoOfTables * 20 / 1024; // each ~ 20kB
            var attrsObjectMemory = tableObjectMemory * 6 * 200 / 1024 / 1024;
            var backup = 20;
            var indexes = (tableObjectMemory / 2) * 15 / 1024;
            var ops = 100000 / 1024;
            var connectionMemory= noOfMysqld * sendreceive * 2 * connectionPool
                    + 2 * 2 * sendreceive +
                    (noOfNdbd * (noOfNdbd - 1) * 2 * sendreceive);
            var multiplier = 800;

            // Get host ram and cores
            mcc.storage.hostStorage().getItem(processItem.getValue("host")).
                    then(function (host) {

                var machineRAM = host.getValue("ram");
                var machineCores = host.getValue("cores");

                // Get number of data nodes on this host (deferred)
                mcc.util.getNodeDistribution(host.getId()).
                        then(function(nNodesOnHost) {
                    var nNdbdOnHost = nNodesOnHost["ndbd"] + 
                            nNodesOnHost["ndbmtd"];

                    // Set number of cores
                    if (!isNaN(machineCores)) {
                        var nExecThreads = 2;

                        // Lower value if simple testing, easier on resources
                        if (cluster.getValue("apparea") != "simple testing") {
                            // Divide by number of data nodes
                            machineCores = machineCores / nNdbdOnHost; 
                            if (machineCores > 6) {
                                nExecThreads = 8;
                            } else if (machineCores > 3) {
                                nExecThreads = 4;
                            }
                        }

                        mcc.configuration.setPara(processFamilyName, id, 
                                "MaxNoOfExecutionThreads",
                                "defaultValueInstance", nExecThreads);
                    }

                    // Set IndexMemory
                    if (!isNaN(machineRAM)) {
                        var indexMemory = Math.floor((machineRAM - 
                                reserveMemoryToOS - buffers - 
                                DiskPageBufferMemory - connectionMemory - 
                                tableObjectMemory - attrsObjectMemory - 
                                indexes - RedoBuffer - ops - backup - 
                                SharedGlobalMemory) / (8 * nNdbdOnHost));

                        // Lower value if simple testing, easier on resources
                        if (cluster.getValue("apparea") == "simple testing") {
                            indexMemory = Math.floor(indexMemory / 4);
                        }

                        // Obey constraints
                        var indexConstraints = mcc.configuration.
                                getPara(processFamilyName, null,
                                "IndexMemory", "constraints");
                        if (indexMemory < indexConstraints.min) {
                            indexMemory = indexConstraints.min;
                        } else if (indexMemory > indexConstraints.max) {
                            indexMemory = indexConstraints.max;
                        }

                        mcc.configuration.setPara(processFamilyName, id, 
                                "IndexMemory",
                                "defaultValueInstance", indexMemory);

                        // Use overridden indexMemory for dataMemory calc
                        var realIndexMemory = getRealValue("IndexMemory");
                        // May not have been set yet
                        if (isNaN(realIndexMemory)) { 
                            realIndexMemory = indexMemory; 
                        }

                        // Set DataMemory
                        var dataMemory= Math.floor(multiplier * 
                                (machineRAM - reserveMemoryToOS - buffers -
                                DiskPageBufferMemory - connectionMemory -
                                tableObjectMemory - attrsObjectMemory - indexes-
                                RedoBuffer - ops - backup - SharedGlobalMemory -
                                realIndexMemory) / (1000 * nNdbdOnHost));

                        // Lower value if simple testing, easier on resources
                        if (cluster.getValue("apparea") == "simple testing") {
                            dataMemory = Math.floor(dataMemory / 4);
                        }

                        // Obey constraints
                        var dataConstraints = mcc.configuration.
                                getPara(processFamilyName, null,
                                "DataMemory", "constraints");
                        if (dataMemory < dataConstraints.min) {
                            dataMemory = dataConstraints.min;
                        } else if (dataMemory > dataConstraints.max) {
                            dataMemory = dataConstraints.max;
                        }

                        mcc.configuration.setPara(processFamilyName, id, "DataMemory",
                                "defaultValueInstance", dataMemory);
                        waitCondition.resolve();
                    } else {
                        waitCondition.resolve();
                    }
                });
            });
        });
    });
}

// mysqld process specific parameter assignments
function mysqld_setup(processItem, processFamilyItem, host, waitCondition) {
    var id = processItem.getId();
    var datadir = host.getValue("datadir");
    var dirSep = mcc.util.dirSep(datadir);
    var processFamilyName = processFamilyItem.getValue("family");

    // Set datadir and socket
    mcc.configuration.setPara(processFamilyName, id, "DataDir",
            "defaultValueInstance", datadir +
            processItem.getValue("NodeId") + dirSep);

    mcc.configuration.setPara(processFamilyName, id, "Socket",
            "defaultValueInstance", datadir +
            processItem.getValue("NodeId") + dirSep + 
            "mysql.socket");

    // Get colleague nodes, find own index on host
    mcc.util.getColleagueNodes(processItem).then(function (colleagues) {
        var myIdx = dojo.indexOf(colleagues, processItem.getId());

        // Get type's overridden port base
        var pbase = processFamilyItem.getValue("Portbase");

        // If not overridden, use type's predefined'
        if (pbase === undefined) {
            pbase = mcc.configuration.getPara(processFamilyName, null, 
                    "Portbase", "defaultValueType");
        }

        // Set port using retrieved portbase and node index on host
        mcc.configuration.setPara(processFamilyName, id, "Port",
                "defaultValueInstance", myIdx + pbase);

        waitCondition.resolve();
    });
}

// Calculate predefined values for a given process type instance
function instanceSetup(processFamilyName, processItem) {
    // Wait condition to return
    var waitCondition = new dojo.Deferred();
    var id = processItem.getId();
    
    mcc.util.dbg("Setup process instance defaults for " + 
            processItem.getValue("name"));

    // For any process type, set HostName and datadir, unless wildcard host
    mcc.storage.hostStorage().getItem(processItem.getValue("host")).then(
            function (host) {
        if (host.getValue("anyHost")) {
            mcc.configuration.setPara(processFamilyName, id, "HostName",
                    "defaultValueInstance", null);
        } else {
            //Use HostName=internalIP to avoid mixing LOCAL & REMOTE hosts.
            mcc.configuration.setPara(processFamilyName, id, "HostName",
                    "defaultValueInstance", host.getValue("internalIP"));
        }

        // Get prototypical process type and do process specific assignments
        mcc.storage.processTypeStorage().getItems({family: processFamilyName}).then(function(ptypes) {
            var processFamilyItem = ptypes[0];
            if (processFamilyName == "management") {
                ndb_mgmd_setup(processItem, processFamilyItem, host, waitCondition);
            } else if (processFamilyName == "data") {
                ndbd_setup(processItem, processFamilyItem, host, waitCondition);
            } else if (processFamilyName == "sql") {
                mysqld_setup(processItem, processFamilyItem, host, waitCondition);
            } else if (processFamilyName == "api") {
                waitCondition.resolve();
            }            
        });
    });
    return waitCondition;
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Configuration calculations module initialized");
});

