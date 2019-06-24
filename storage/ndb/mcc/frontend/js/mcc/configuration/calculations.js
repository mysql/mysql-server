/*
Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

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
 *      ndbMgmdSetup: ndb_mgmd process specific parameter assignments
 *      ndbdSetup: ndbd process specific parameter assignments
 *      mysqldSetup: mysqld process specific parameter assignments
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

/****************************** Import/export *********************************/
dojo.provide('mcc.configuration.calculations');

dojo.require('mcc.util');
dojo.require('mcc.storage');
dojo.require('mcc.configuration');
dojo.require('mcc.gui');
/**************************** External interface ******************************/
mcc.configuration.calculations.autoConfigure = autoConfigure;
mcc.configuration.calculations.instanceSetup = instanceSetup;
mcc.configuration.calculations.typeSetup = typeSetup;

// Add processes to the cluster if none exists already
function autoConfigure () {
    var waitCondition = new dojo.Deferred();
    // If no processes, add
    mcc.storage.processStorage().getItems({}).then(function (items) {
        // Shortcut if we have processes already
        if (items && items.length > 0) {
            console.debug('[DBG]Processes already exist, not adding default');
            waitCondition.resolve();
            return;
        }
        mcc.storage.hostStorage().getItems({}).then(function (hosts) {
            // Remove the wildcard host from the list
            for (var i in hosts) {
                if (hosts[i].getValue('anyHost')) {
                    hosts.splice(i, 1);
                    break;
                }
            }

            // First sort by name to get repeatable allocation (needed for tests)
            hosts.sort(function (h1, h2) {
                if (h1.getValue('name') < h2.getValue('name')) {
                    return -1;
                } else {
                    return 1;
                }
            });

            // Shortcut if we have no hosts, or only wildcard host
            if (!hosts || hosts.length === 0 ||
                    (hosts.length === 1 && hosts[0].getValue('anyHost'))) {
                mcc.util.displayModal('I', 3, '<span style="font-size:135%;color:orangered;">' +
                    'No hosts - unable to add default processes.</span>');
                console.warn('[WRN]No hosts - unable to add default processes');
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
                if (!familyHead[pType.getValue('family')]) {
                    familyHead[pType.getValue('family')] = pType;
                }
                typeHead[pType.getValue('name')] = familyHead[pType.getValue('family')];
                typeIds[pType.getValue('name')] = pType.getId();
                names[pType.getValue('name')] = pType.getValue('nodeLabel');
            },
            function () {
                // Add new process following same ID rules set in MCCStorage.js::initializeProcessTypeStorage
                var NID = 0;
                function newProcess (pname, host) {
                    switch (pname) {
                        case 'ndb_mgmd':
                            NID = mgmtNodeID;
                            mgmtNodeID++;
                            otherNodeId++;
                            break;
                        case 'mysqld':
                            NID = sqlNodeID;
                            sqlNodeID++;
                            otherNodeId++;
                            break;
                        case 'api':
                            NID = apiNodeID;
                            apiNodeID++;
                            otherNodeId++;
                            break;
                        default:
                            NID = dataNodeId;
                            dataNodeId++;
                    }

                    mcc.storage.processStorage().newItem({
                        name: names[pname] + ' ' + typeHead[pname].getValue('currSeq'),
                        host: host.getId(),
                        processtype: typeIds[pname],
                        NodeId: NID,
                        seqno: typeHead[pname].getValue('currSeq')
                    });
                }
                /*
                For uneven number of hosts you will have the first host as MGM server only
                so 1 host, all colocated (developers environment).
                2 hosts MGM + MySQL + DNode on all hosts
                3 hosts 1 x MGM process, 1 x MGM + MySQL + DNode, 1 x MySQL + DNode
                4 hosts 2 x MGM + MySQL + DNode, 2 x MySQL + DNode
                5 hosts 1 x MGM, 1 x MGM + MYSQL + DNode, 3 x MySQL + DNode
                and so forth up to 49 hosts
                */
                if (hosts.length === 1) {
                    // One host: 1*mgmd + 3*api + 2*mysqld + 2*ndbd
                    newProcess('ndb_mgmd', hosts[0]);
                    for (var i = 0; i < 3; ++i) {
                        newProcess('api', hosts[0]);
                    }
                    for (i = 0; i < 2; ++i) {
                        newProcess('mysqld', hosts[0]);
                    }
                    for (i = 0; i < 2; ++i) {
                        newProcess('ndbmtd', hosts[0]);
                    }
                } else if (hosts.length === 2) {
                    // Two hosts: 1*mgmd + 2*api + 1*mysqld + 1*ndbd on all hosts.
                    newProcess('ndb_mgmd', hosts[0]);
                    newProcess('ndb_mgmd', hosts[1]);
                    for (var i1 = 0; i1 < 2; ++i1) {
                        newProcess('api', hosts[0]);
                    }
                    newProcess('mysqld', hosts[0]);
                    newProcess('ndbmtd', hosts[0]);

                    for (i1 = 0; i1 < 2; ++i1) {
                        newProcess('api', hosts[1]);
                    }
                    newProcess('mysqld', hosts[1]);
                    newProcess('ndbmtd', hosts[1]);
                } else if (hosts.length === 3) {
                    // 1*mgmd,1*mgmd + mysqld + ndbd, 1*mysqld + ndbd
                    newProcess('ndb_mgmd', hosts[0]);
                    newProcess('ndb_mgmd', hosts[1]);
                    for (var i2 = 0; i2 < 3; ++i2) {
                        newProcess('api', hosts[0]);
                    }
                    for (i2 = 0; i2 < 2; ++i2) {
                        newProcess('mysqld', hosts[i2 + 1]);
                    }
                    for (i2 = 0; i2 < 2; ++i2) {
                        newProcess('ndbmtd', hosts[i2 + 1]);
                    }
                } else if (hosts.length === 4) {
                    // 2*mgmd + mysqld + ndbd, 2*mysqld + ndbd
                    newProcess('ndb_mgmd', hosts[0]);
                    newProcess('ndb_mgmd', hosts[1]);
                    newProcess('mysqld', hosts[0]);
                    newProcess('mysqld', hosts[1]);
                    newProcess('ndbmtd', hosts[0]);
                    newProcess('ndbmtd', hosts[1]);
                    newProcess('api', hosts[0]);
                    newProcess('api', hosts[0]);
                    newProcess('api', hosts[0]);
                    newProcess('mysqld', hosts[2]);
                    newProcess('ndbmtd', hosts[2]);
                    newProcess('mysqld', hosts[3]);
                    newProcess('ndbmtd', hosts[3]);
                } else if (hosts.length > 4) {
                    // 5 hosts 1*mgmd, 1*mgmd + mysqld + ndbd, 3*mysqld + ndbd
                    // No more than 2 MGMT nodes! Use 2 hosts for 1*mgmds + 2*api on each
                    for (var i3 = 0; i3 < 2; ++i3) {
                        if (otherNodeId <= 255) {
                            newProcess('ndb_mgmd', hosts[i3]);
                        }
                        if (otherNodeId <= 255) {
                            newProcess('api', hosts[i3]);
                        }
                        if (otherNodeId <= 255) {
                            newProcess('api', hosts[i3]);
                        }
                    }
                    // Put MySQL and DNodes on all but first.
                    // If number of hosts-1 (1st host doesn't have Data node) is
                    // not even number, we need to adjust number of Data nodes to even number!
                    var isEven = (hosts.length - 1) % 2 === 0;
                    for (i3 = 1; i3 < hosts.length; ++i3) {
                        if (otherNodeId <= 255) {
                            newProcess('mysqld', hosts[i3]);
                        }
                        // Skip adding ndbmtd to 2nd host if not even number of datanodes.
                        if (isEven) {
                            if (dataNodeId <= 48) {
                                newProcess('ndbmtd', hosts[i3]);
                            }
                        } else {
                            // There will be even number of ndbmtd processes now.
                            isEven = true;
                        }
                    }
                }
                console.debug('[DBG]Default processes added');
                waitCondition.resolve();
            });
        });
    });
    return waitCondition;
}

// Calculate hw dependent data node parameters
function hwDepParams (processTypeName) {
    // Single deferred to callback
    var waitCondition = new dojo.Deferred();
    // Array of deferrers to wait for
    var waitConditions = [];
    var waitList;
    // Fetch processes
    mcc.storage.processTypeStorage().getItems({ name: processTypeName }).then(function (ptypes) {
        mcc.storage.processStorage().getItems({
            processtype: ptypes[0].getId() }).then(function (nodes) {
            for (var i in nodes) {
                // Run instance setup
                waitConditions[i] = instanceSetup(ptypes[0].getValue('family'), nodes[i]);
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
function typeSetup (processTypeItem) {
    var processFamilyName = String(processTypeItem.getValue('family'));
    var waitCondition = new dojo.Deferred();
    console.debug('[DBG]Setup process type defaults for family ' + processFamilyName);
    var pBase;
    // Get the prototypical process type for this family
    mcc.storage.processTypeStorage().getItems({ family: processFamilyName }).then(function (pTypes) {
        var processFamilyItem = pTypes[0];
        // Process type specific assignments
        if (processFamilyName === 'management') {
            // Get portbase, set default port
            pBase = processFamilyItem.getValue('Portbase');
            if (pBase === undefined) {
                pBase = mcc.configuration.getPara(processFamilyName, null,
                    'Portbase', 'defaultValueType');
            }
            mcc.configuration.setPara(processFamilyName, null, 'Portnumber',
                'defaultValueType', pBase);
            // Leave process type level datadir undefined
            waitCondition.resolve();
        } else if (processFamilyName === 'data') {
            // Get portbase, set default for ServerPort
            var spbase = processFamilyItem.getValue('Portbase');
            if (spbase === undefined) {
                spbase = mcc.configuration.getPara(processFamilyName, null,
                    'ServerPort', 'defaultValueType');
            }
            mcc.configuration.setPara(processFamilyName, null, 'ServerPort', 'defaultValueType', spbase);
            // Check parameters that depend on cluster defaults
            mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
                // Leave process type level datadir undefined. Check real time or web mode.
                // This is OK since all those values are "undefined" in parameters.ja.
                if (cluster.getValue('apparea').toString() !== 'realtime') {
                    mcc.configuration.setPara(processFamilyName, null,
                        'HeartbeatIntervalDbDb', 'defaultValueType', 15000);
                    mcc.configuration.setPara(processFamilyName, null,
                        'HeartbeatIntervalDbApi', 'defaultValueType', 15000);
                } else {
                    mcc.configuration.setPara(processFamilyName, null,
                        'HeartbeatIntervalDbDb', 'defaultValueType', 1500);
                    mcc.configuration.setPara(processFamilyName, null,
                        'HeartbeatIntervalDbApi', 'defaultValueType', 1500);
                }

                // Check read/write load
                // This is OK since all those values are "undefined" in parameters.ja.
                if (cluster.getValue('writeload').toString() === 'high') {
                    mcc.configuration.setPara(processFamilyName, null,
                        'SendBufferMemory', 'defaultValueType', 8);
                    mcc.configuration.setPara(processFamilyName, null,
                        'ReceiveBufferMemory', 'defaultValueType', 8);
                    mcc.configuration.setPara(processFamilyName, null,
                        'RedoBuffer', 'defaultValueType', 64);
                } else if (cluster.getValue('writeload').toString() === 'medium') {
                    mcc.configuration.setPara(processFamilyName, null,
                        'SendBufferMemory', 'defaultValueType', 4);
                    mcc.configuration.setPara(processFamilyName, null,
                        'ReceiveBufferMemory', 'defaultValueType', 4);
                    mcc.configuration.setPara(processFamilyName, null,
                        'RedoBuffer', 'defaultValueType', 32);
                } else {
                    mcc.configuration.setPara(processFamilyName, null,
                        'SendBufferMemory', 'defaultValueType', 2);
                    mcc.configuration.setPara(processFamilyName, null,
                        'ReceiveBufferMemory', 'defaultValueType', 2);
                    mcc.configuration.setPara(processFamilyName, null,
                        'RedoBuffer', 'defaultValueType', 32);
                }
                // Get disk page buffer memory, assign shared global memory.
                var diskBuf = processFamilyItem.getValue(
                    mcc.configuration.getPara(processFamilyName, null,
                        'DiskPageBufferMemory', 'attribute'));
                if (!diskBuf) {
                    // If user didn't set it, read from parameters.js.
                    diskBuf = mcc.configuration.getPara(processFamilyName, null,
                        'DiskPageBufferMemory', 'defaultValueType');
                }
                diskBuf = Number(diskBuf);
                // Default for SGM is 300M. Adjust if needed.
                if (diskBuf > 8192) {
                    mcc.configuration.setPara(processFamilyName, null,
                        'SharedGlobalMemory', 'defaultValueType', 1024);
                } else {
                    if (diskBuf > 64) {
                        mcc.configuration.setPara(processFamilyName, null,
                            'SharedGlobalMemory', 'defaultValueType', 400);
                    }
                }
                // Restrict MaxNoOfTables
                var maxTab = processFamilyItem.getValue(
                    mcc.configuration.getPara(processFamilyName, null,
                        'MaxNoOfTables', 'attribute'));
                if (maxTab) {
                    // IF user has set it THEN do check.
                    maxTab = Number(maxTab);
                    if (maxTab > 20320) {
                        processFamilyItem.setValue(
                            mcc.configuration.getPara(processFamilyName, null,
                                'MaxNoOfTables', 'attribute'), 20320);
                    } else if (maxTab < 128) {
                        processFamilyItem.setValue(
                            mcc.configuration.getPara(processFamilyName, null,
                                'MaxNoOfTables', 'attribute'), 128);
                    }
                    mcc.storage.processStorage().save();
                }
                // Calculate data-mem, index-mem, and maxexecthreads
                hwDepParams('ndbd').then(function () {
                    hwDepParams('ndbmtd').then(function () {
                        // Get predefined data node parameters
                        var params = mcc.configuration.getAllPara('data');
                        function setLow (param) {
                            var low; // = undefined;
                            // Loop over instance values, collect min, set
                            for (var i in params[param].defaultValueInstance) {
                                var curr = params[param].defaultValueInstance[i];
                                if (low === undefined ||
                                        (curr !== undefined && curr < low)) {
                                    low = curr;
                                }
                            }
                            console.debug('[DBG]Lowest value for ' + param + ' is now: ' + low);
                            if (low !== undefined) {
                                mcc.configuration.setPara(processFamilyName, null,
                                    param, 'defaultValueType', low);
                            }
                        }
                        // These are fine. Cluster is stopped.
                        setLow('DataMemory');
                        // setLow('IndexMemory');
                        // parameters.js, constraints: {min: 2, max: 72, thus 2.

                        // Silent change of defaults is handled in processtreedetails.js
                        // and html.js on widget level for NoOfReplicas and NoOfFragmentLogParts
                        // no more (NoOfFragmentLogFiles and FragmentLogFileSize).
                        
                        // NoOfFragmentLogParts and MaxNoOfExecutionThreads reintroduced, need
                        // handling here.
                        var x = parseInt(processFamilyItem.getValue('MaxNoOfExecutionThreads'));
                        if (!x) {
                            mcc.configuration.setPara(processFamilyName, null,
                                'MaxNoOfExecutionThreads', 'defaultValueType', 8);
                            x = 8;
                        }
                        var flp = 0;
                        switch (true) {
                            case (x <= 3): // 1
                                flp = 4;
                                break;
                            case (x <= 6): // 2
                                flp = 4;
                                break;
                            case (x <= 11): // 4
                                flp = 4;
                                break;
                            case (x <= 15): // 6
                                flp = 6;
                                break;
                            case (x <= 19): // 8
                                flp = 8;
                                break;
                            case (x <= 23): // 10
                                flp = 10;
                                break;
                            case (x <= 31): // 12
                                flp = 12;
                                break;
                            case (x <= 39): // 16
                                flp = 16;
                                break;
                            case (x <= 47): // 20
                                flp = 20;
                                break;
                            case (x <= 63): // 24
                                flp = 24;
                                break;
                            case (x <= 72): // 32
                                flp = 32;
                                break;
                            default:
                                console.warn('[WRN]Failed to set NoOfFragmentLogParts! Will be 32.');
                                flp = 32;
                                break;
                        }
                        mcc.configuration.setPara(processFamilyName, null,
                            'NoOfFragmentLogParts', 'defaultValueType', flp);

                        // Get overridden redo log file size
                        var fileSz = processFamilyItem.getValue('FragmentLogFileSize');

                        // If not overridden, set value depending on app area.
                        if (!fileSz) {
                            // Lower value if simple testing, easier on resources
                            if (cluster.getValue('apparea').toString() === 'simple testing') {
                                fileSz = 64;
                            } else {
                                fileSz = 256;
                            }
                            mcc.configuration.setPara(processFamilyName, null,
                                'FragmentLogFileSize', 'defaultValueType', fileSz);
                        }
                        console.debug('[DBG]FragmentLogFileSize=' + fileSz);

                        // Calculate and set number of files
                        var dataMem = mcc.configuration.getPara(processFamilyName, null,
                            'DataMemory', 'defaultValueType');
                        var noOfFiles = 16;
                        // Use def value unless not simple testing and DataMem defined
                        if (cluster.getValue('apparea').toString() !== 'simple testing' && dataMem) {
                            noOfFiles = Math.floor(6 * dataMem / fileSz / 4);
                        }
                        // At least three files in each set
                        if (noOfFiles < 3) { noOfFiles = 3; }
                        console.debug('[DBG]NoOfFragmentLogFiles=' + noOfFiles);
                        mcc.configuration.setPara(processFamilyName, null,
                            'NoOfFragmentLogFiles', 'defaultValueType', noOfFiles);
                        // Get number of data nodes. Prepare for 3 and 4 too.
                        mcc.util.getNodeDistribution().then(function (nNodes) {
                            // console.debug('[DBG]Setting NoOfReplicas');
                            mcc.configuration.setPara(processFamilyName, null, 'NoOfReplicas',
                                'defaultValueType', 2 - (nNodes['ndbd'] + nNodes['ndbmtd']) % 2);
                            waitCondition.resolve();
                        });
                    });// HW-dep params inner.
                });// HW-dep params outer
            });// Cluster storage.
        } else if (processFamilyName === 'sql') {
            // Get portbase, set default port
            pBase = processFamilyItem.getValue('Portbase');
            if (pBase === undefined) {
                pBase = mcc.configuration.getPara(processFamilyName, null,
                    'Portbase', 'defaultValueType');
            }
            mcc.configuration.setPara(processFamilyName, null, 'Port',
                'defaultValueType', pBase);
            // Leave process type level socket and datadir undefined
            waitCondition.resolve();
        } else if (processFamilyName === 'api') {
            waitCondition.resolve();
        }
    });
    return waitCondition;
}

// ndb_mgmd process specific parameter assignments
function ndbMmgmdSetup (processItem, processFamilyItem, host, waitCondition) {
    var id = processItem.getId();
    var datadir = mcc.util.unixPath(host.getValue('datadir'));
    var dirSep = mcc.util.dirSep(datadir);
    var processFamilyName = processFamilyItem.getValue('family');
    // Set datadir
    mcc.configuration.setPara(processFamilyName, id, 'DataDir',
        'defaultValueInstance', datadir + processItem.getValue('NodeId') + dirSep);
    var pBase = 0;
    pBase = processFamilyItem.getValue('Portbase');
    if (pBase === undefined) {
        pBase = mcc.configuration.getPara(processFamilyName, null, 'Portbase', 'defaultValueType');
    }
    mcc.util.getColleagueNodesAndPorts(processItem, pBase, processFamilyName).then(function (colleagues) {
        if (colleagues.length === 1) {
            // OLD code
            if (colleagues[0].port === 0) {
                colleagues[0].port = pBase;
            }
            if (colleagues[0].id !== id) {
                console.debug('[DBG]Error, %d <> %d!', id, colleagues[0].id);
            }
            mcc.configuration.setPara(processFamilyName, colleagues[0].id, 'Portnumber', 'defaultValueInstance', pBase);
        } else {
            // NEW code
            var myPort = 0;
            var myIdx = -1;
            var PortExists = false;
            for (var t in colleagues) {
                if (colleagues[t].id === id) {
                    myPort = +colleagues[t].port;
                    myIdx = +t;
                    if (colleagues[t].id !== id) {
                        console.debug('[DBG]Error, %d <> %d!', id, colleagues[t].id);
                    }
                    break;
                }
            }
            console.debug('[DBG]myPort before sorting is ' + myPort + ', with id:' + id);
            // check if ALL are new
            var allNew = true;
            if (myPort === pBase) {
                if (!mcc.configuration.getPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance')) {
                    var colleaguesToSort = [];
                    for (t in colleagues) {
                        colleaguesToSort.push(colleagues[t]);
                    }
                    // Sort nodes according to Port value
                    colleaguesToSort.sort(function (a, b) {
                        if (+a.port < +b.port) {
                            allNew = false;
                            return 1;
                        } else if (+a.port > +b.port) {
                            allNew = false;
                            return -1;
                        } else {
                            return 0;
                        }
                    });
                    if (allNew) {
                        // set Port for new ones as before to PortBase + index
                        myPort = +pBase + myIdx;
                        if (myIdx > 0) {
                            // check others
                            for (t in colleagues) {
                                console.debug('[DBG]Portnumber[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                                if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                                    // port already taken
                                    console.warn('ServerPort ' + myPort + ' is already taken, changing.');
                                    PortExists = true;
                                    break;
                                }
                            }
                            if (!PortExists) {
                                // not default for family, set it
                                mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'value', myPort);
                                // lock defaultValueInstance to sensible value, no changes for every click
                                if (!mcc.configuration.getPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance')) {
                                    mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance', myPort);
                                }
                            }
                        } else {
                            // all new, first one
                            mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'value', myPort);
                            mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance', myPort);
                        }
                    } else {
                        // there were some user entries
                        myPort = +colleaguesToSort[0].port + 1;
                        // check others
                        for (t in colleagues) {
                            console.debug('[DBG]Portnumber[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                            if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                                // port already taken
                                console.warn('Portnumber ' + myPort + ' is already taken, changing.');
                                PortExists = true;
                                break;
                            }
                        }
                        if (!PortExists) {
                            // not default for family, set it
                            mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'value', myPort);
                            // lock defaultValueInstance to sensible value, no changes for every click
                            if (!mcc.configuration.getPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance')) {
                                mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance', myPort);
                            }
                        }
                    }
                } else {
                    // could be some node changed value but that will get picked up in VerifyConfiguration
                    for (t in colleagues) {
                        console.debug('[DBG]Port[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                        if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                            // port already taken
                            console.warn('Portnumber ' + myPort + ' is already taken, changing.');
                            break;
                        }
                    }
                }
            } else {
                // it has value for Port entered by user
                for (t in colleagues) {
                    console.debug('[DBG]Portnumber[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                    if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                        // port already taken
                        console.warn('Portnumber ' + myPort + ' is already taken, changing.');
                        PortExists = true;
                        break;
                    }
                }
                // lock defaultValueInstance to sensible value, no changes for every click
                if (!PortExists && !mcc.configuration.getPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance')) {
                    mcc.configuration.setPara(processFamilyName, id, 'Portnumber', 'defaultValueInstance', myPort);
                }
            }
        }
        waitCondition.resolve();
    });
}

// ndbXd process specific parameter assignments
function ndbdSetup (processItem, processFamilyItem, host, waitCondition) {
    var id = processItem.getId();
    var datadir = mcc.util.unixPath(host.getValue('datadir'));
    var dirSep = mcc.util.dirSep(datadir);
    var processFamilyName = processFamilyItem.getValue('family');
    // Set datadir
    mcc.configuration.setPara(processFamilyName, id, 'DataDir',
        'defaultValueInstance', datadir + processItem.getValue('NodeId') + dirSep);
    var pBase = 0;
    pBase = processFamilyItem.getValue('Portbase');
    if (pBase === undefined) {
        pBase = mcc.configuration.getPara(processFamilyName, null, 'Portbase', 'defaultValueType');
    }
    mcc.util.getColleagueNodesAndPorts(processItem, pBase, processFamilyName).then(function (colleagues) {
        if (colleagues.length === 1) {
            // OLD code
            if (colleagues[0].port === 0) {
                colleagues[0].port = pBase;
            }
            if (colleagues[0].id !== id) {
                console.debug('[DBG]Error, %d <> %d!', id, colleagues[0].id);
            }
            mcc.configuration.setPara(processFamilyName, colleagues[0].id, 'ServerPort', 'defaultValueInstance', pBase);
        } else {
            // NEW code
            var myPort = 0;
            var myIdx = -1;
            var PortExists = false;
            for (var t in colleagues) {
                if (colleagues[t].id === id) {
                    myPort = +colleagues[t].port;
                    myIdx = +t;
                    if (colleagues[t].id !== id) {
                        console.debug('[DBG]Error, %d <> %d!', id, colleagues[t].id);
                    }
                    break;
                }
            }
            console.debug('[DBG]myPort before sorting is ' + myPort + ', with id:' + id);
            // check if ALL are new
            var allNew = true;
            if (myPort === pBase) {
                if (!mcc.configuration.getPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance')) {
                    var colleaguesToSort = [];
                    for (t in colleagues) {
                        colleaguesToSort.push(colleagues[t]);
                    }
                    // Sort nodes according to Port value
                    colleaguesToSort.sort(function (a, b) {
                        if (+a.port < +b.port) {
                            allNew = false;
                            return 1;
                        } else if (+a.port > +b.port) {
                            allNew = false;
                            return -1;
                        } else {
                            return 0;
                        }
                    });
                    if (allNew) {
                        // set Port for new ones as before to PortBase + index
                        myPort = +pBase + myIdx;
                        if (myIdx > 0) {
                            // check others
                            for (t in colleagues) {
                                console.debug('[DBG]ServerPort[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                                if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                                    // port already taken
                                    console.warn('ServerPort ' + myPort + ' is already taken, changing.');
                                    PortExists = true;
                                    break;
                                }
                            }
                            if (!PortExists) {
                                // not default for family, set it
                                mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'value', myPort);
                                // lock defaultValueInstance to sensible value, no changes for every click
                                if (!mcc.configuration.getPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance')) {
                                    mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance', myPort);
                                }
                            }
                        } else {
                            // all new, first one
                            mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'value', myPort);
                            mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance', myPort);
                        }
                    } else {
                        // there were some user entries
                        myPort = +colleaguesToSort[0].port + 1;
                        // check others
                        for (t in colleagues) {
                            console.debug('[DBG]ServerPort[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                            if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                                // port already taken
                                console.warn('ServerPort ' + myPort + ' is already taken, changing.');
                                PortExists = true;
                                break;
                            }
                        }
                        if (!PortExists) {
                            // not default for family, set it
                            mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'value', myPort);
                            // lock defaultValueInstance to sensible value, no changes for every click
                            if (!mcc.configuration.getPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance')) {
                                mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance', myPort);
                            }
                        }
                    }
                } else {
                    // could be some node changed value but that will get picked up in VerifyConfiguration
                    for (t in colleagues) {
                        console.debug('[DBG]Port[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                        if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                            // port already taken
                            console.warn('Port ' + myPort + ' is already taken, changing.');
                            break;
                        }
                    }
                }
            } else {
                // it has value for Port entered by user
                for (t in colleagues) {
                    console.debug('[DBG]ServerPort[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                    if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                        // port already taken
                        console.warn('ServerPort ' + myPort + ' is already taken, changing.');
                        PortExists = true;
                        break;
                    }
                }
                // lock defaultValueInstance to sensible value, no changes for every click
                if (!PortExists && !mcc.configuration.getPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance')) {
                    mcc.configuration.setPara(processFamilyName, id, 'ServerPort', 'defaultValueInstance', myPort);
                }
            }
        }
    });

    // Get cluster attributes
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        // Get node distribution (deferred)
        mcc.util.getNodeDistribution().then(function (nNodes) {
            var noOfMysqld = nNodes['mysqld'];
            var noOfNdbd = nNodes['ndbd'] + nNodes['ndbmtd'];
            // Return overridden value, if defined, otherwise, return predefined
            function getRealValue (attr) {
                var val = processFamilyItem.getValue(attr);
                if (val === undefined) {
                    val = mcc.configuration.getPara(processFamilyName, null,
                        attr, 'defaultValueType');
                }
                return val;
            }
            // Need these for calculations below
            var MaxNoOfTables = getRealValue('MaxNoOfTables');
            var sendreceive = getRealValue('SendBufferMemory');
            var DiskPageBufferMemory = getRealValue('DiskPageBufferMemory');
            var SharedGlobalMemory = getRealValue('SharedGlobalMemory');
            var RedoBuffer = getRealValue('RedoBuffer');
            // Change this setting if we support managing connection pooling
            var connectionPool = 1;
            // Temporary variables used in memory calculations
            var reserveMemoryToOS = 1024 * 1;
            var buffers = 300 * 1;
            var tableObjectMemory = MaxNoOfTables * 20 / 1024; // each ~ 20kB
            var attrsObjectMemory = tableObjectMemory * 6 * 200 / 1024 / 1024;
            var backup = 20;
            var indexes = (tableObjectMemory / 2) * 15 / 1024;
            var ops = 100000 / 1024;
            var connectionMemory = noOfMysqld * sendreceive * 2 * connectionPool +
                     2 * 2 * sendreceive + (noOfNdbd * (noOfNdbd - 1) * 2 * sendreceive);
            var multiplier = 800;
            // Get host ram and cores
            mcc.storage.hostStorage().getItem(processItem.getValue('host')).then(function (host) {
                var machineRAM = parseInt(host.getValue('ram'));
                // Get number of data nodes on this host (deferred)
                mcc.util.getNodeDistribution(host.getId()).then(function (nNodesOnHost) {
                    var nNdbdOnHost = nNodesOnHost['ndbd'] + nNodesOnHost['ndbmtd'];
                    // Set IndexMemory
                    if (!isNaN(machineRAM)) {
                        // Set DataMemory
                        var dataMemory = Math.floor(multiplier *
                                (machineRAM - reserveMemoryToOS - buffers -
                                DiskPageBufferMemory - connectionMemory -
                                tableObjectMemory - attrsObjectMemory - indexes -
                                RedoBuffer - ops - backup - SharedGlobalMemory) /
                                (1000 * nNdbdOnHost));
                        // Lower value if simple testing, easier on resources
                        if (cluster.getValue('apparea') === 'simple testing') {
                            dataMemory = Math.floor(dataMemory / 4);
                        }
                        // Obey constraints
                        var dataConstraints = mcc.configuration.getPara(processFamilyName, null,
                            'DataMemory', 'constraints');
                        if (dataMemory < dataConstraints.min) {
                            dataMemory = dataConstraints.min;
                        } else if (dataMemory > dataConstraints.max) {
                            dataMemory = dataConstraints.max;
                        }
                        mcc.configuration.setPara(processFamilyName, id, 'DataMemory',
                            'defaultValueInstance', dataMemory);
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
function mysqldSetup (processItem, processFamilyItem, host, waitCondition) {
    var id = processItem.getId();
    var datadir = host.getValue('datadir');
    var dirSep = mcc.util.dirSep(datadir);
    var processFamilyName = processFamilyItem.getValue('family');

    mcc.configuration.setPara(processFamilyName, id, 'DataDir', 'defaultValueInstance',
        datadir + processItem.getValue('NodeId') + dirSep);
    mcc.configuration.setPara(processFamilyName, id, 'DataDir', 'value',
        datadir + processItem.getValue('NodeId') + dirSep);
    mcc.configuration.setPara(processFamilyName, id, 'Socket', 'defaultValueInstance', datadir +
            processItem.getValue('NodeId') + dirSep + 'mysql.socket');

    var pBase = 0;
    pBase = processFamilyItem.getValue('Portbase');
    if (pBase === undefined) {
        pBase = mcc.configuration.getPara(processFamilyName, null, 'Portbase', 'defaultValueType');
    }
    mcc.util.getColleagueNodesAndPorts(processItem, pBase, processFamilyName).then(function (colleagues) {
        if (colleagues.length === 1) {
            // OLD code
            if (colleagues[0].port === 0) {
                colleagues[0].port = pBase;
            }
            if (colleagues[0].id !== id) {
                console.debug('[DBG]Error, %d <> %d!', id, colleagues[0].id);
            }
            console.debug('[DBG]Biggest Port in use is ' + colleagues[0].port + ', with id:' + id);
            mcc.configuration.setPara(processFamilyName, colleagues[0].id, 'Port', 'defaultValueInstance', pBase);
        } else {
            // NEW code
            var myPort = 0;
            var myIdx = -1;
            var PortExists = false;
            for (var t in colleagues) {
                if (colleagues[t].id === id) {
                    myPort = +colleagues[t].port;
                    myIdx = +t;
                    if (colleagues[t].id !== id) {
                        console.debug('[DBG]Error, %d <> %d!', id, colleagues[t].id);
                    }
                    break;
                }
            }
            console.debug('[DBG]myPort before sorting is ' + myPort + ', with id:' + id);
            // check if ALL are new
            var allNew = true;
            if (myPort === pBase) {
                if (!mcc.configuration.getPara(processFamilyName, id, 'Port', 'defaultValueInstance')) {
                    var colleaguesToSort = [];
                    for (t in colleagues) {
                        colleaguesToSort.push(colleagues[t]);
                    }
                    // Sort nodes according to Port value
                    colleaguesToSort.sort(function (a, b) {
                        if (+a.port < +b.port) {
                            allNew = false;
                            return 1;
                        } else if (+a.port > +b.port) {
                            allNew = false;
                            return -1;
                        } else {
                            return 0;
                        }
                    });
                    console.debug('[DBG]Biggest Port in use is ' + colleaguesToSort[0].port);
                    if (allNew) {
                        // set Port for new ones as before to PortBase + index
                        myPort = +pBase + myIdx;
                        if (myIdx > 0) {
                            // check others
                            for (t in colleagues) {
                                console.debug('[DBG]Port[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                                if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                                    // port already taken
                                    console.warn('Port ' + myPort + ' is already taken, changing.');
                                    PortExists = true;
                                    break;
                                }
                            }
                            if (!PortExists) {
                                // not default for family, set it
                                mcc.configuration.setPara(processFamilyName, id, 'Port', 'value', myPort);
                                // lock defaultValueInstance to sensible value, no changes for every click
                                if (!mcc.configuration.getPara(processFamilyName, id, 'Port', 'defaultValueInstance')) {
                                    mcc.configuration.setPara(processFamilyName, id, 'Port', 'defaultValueInstance', myPort);
                                }
                            }
                        } else {
                            // first one
                            mcc.configuration.setPara(processFamilyName, id, 'Port', 'value', myPort);
                            mcc.configuration.setPara(processFamilyName, id, 'Port', 'defaultValueInstance', myPort);
                        }
                    } else {
                        // there were some user entries
                        myPort = +colleaguesToSort[0].port + 1;
                        // check others
                        for (t in colleagues) {
                            console.debug('[DBG]Port[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                            if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                                // port already taken
                                console.warn('Port ' + myPort + ' is already taken, changing.');
                                PortExists = true;
                                break;
                            }
                        }
                        if (!PortExists) {
                            // not default for family, set it
                            mcc.configuration.setPara(processFamilyName, id, 'Port', 'value', myPort);
                            // lock defaultValueInstance to sensible value, no changes for every click
                            if (!mcc.configuration.getPara(processFamilyName, id, 'Port', 'defaultValueInstance')) {
                                mcc.configuration.setPara(processFamilyName, id, 'Port', 'defaultValueInstance', myPort);
                            }
                        }
                    }
                } else {
                    // could be some node changed value but that will get picked up in VerifyConfiguration
                    for (t in colleagues) {
                        console.debug('[DBG]Port[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                        if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                            // port already taken
                            console.warn('Port ' + myPort + ' is already taken, changing.');
                            break;
                        }
                    }
                }
            } else {
                // it has value for Port entered by user
                for (t in colleagues) {
                    console.debug('[DBG]Port[' + id + ']' + colleagues[t].port + ', MyPort[' + id + ']:' + myPort);
                    if (+colleagues[t].port === +myPort && +colleagues[t].id !== +id) {
                        // port already taken
                        console.warn('Port ' + myPort + ' is already taken, changing.');
                        PortExists = true;
                        break;
                    }
                }
                // lock defaultValueInstance to sensible value, no changes for every click
                if (!PortExists && !mcc.configuration.getPara(processFamilyName, id, 'Port', 'defaultValueInstance')) {
                    mcc.configuration.setPara(processFamilyName, id, 'Port', 'defaultValueInstance', myPort);
                }
            }
        }
        waitCondition.resolve();
    });
}

// Calculate predefined values for a given process type instance
function instanceSetup (processFamilyName, processItem) {
    // Wait condition to return
    var waitCondition = new dojo.Deferred();
    var id = processItem.getId();

    console.info('[INF]Setup process instance defaults for ' + processItem.getValue('name'));
    // For any process type, set HostName and datadir, unless wildcard host
    mcc.storage.hostStorage().getItem(processItem.getValue('host')).then(function (host) {
        if (host.getValue('anyHost')) {
            mcc.configuration.setPara(processFamilyName, id, 'HostName', 'defaultValueInstance', null);
        } else {
            if (mcc.gui.getUseVPN()) {
                mcc.configuration.setPara(processFamilyName, id, 'HostName',
                    'defaultValueInstance', host.getValue('internalIP'));
            } else {
                mcc.configuration.setPara(processFamilyName, id, 'HostName',
                    'defaultValueInstance', host.getValue('name'));
            }
        }

        // Get prototypical process type and do process specific assignments
        mcc.storage.processTypeStorage().getItems({ family: processFamilyName }).then(function (ptypes) {
            var processFamilyItem = ptypes[0];
            if (processFamilyName.toLowerCase() === 'management') {
                ndbMmgmdSetup(processItem, processFamilyItem, host, waitCondition);
            } else if (processFamilyName.toLowerCase() === 'data') {
                ndbdSetup(processItem, processFamilyItem, host, waitCondition);
            } else if (processFamilyName.toLowerCase() === 'sql') {
                mysqldSetup(processItem, processFamilyItem, host, waitCondition);
            } else if (processFamilyName.toLowerCase() === 'api') {
                waitCondition.resolve();
            }
        });
    });
    return waitCondition;
}

/********************************* Initialize *********************************/

dojo.ready(function () {
    console.info('[INF]Configuration calculations module initialized');
});
