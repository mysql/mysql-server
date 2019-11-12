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
 *      mcc.configuration.deploy.getStartProcessCommands: Get commands for starting process.
 *      mcc.configuration.deploy.getStopProcessCommands: Get commands for stopping Cluster.
 *      mcc.configuration.deploy.getConfigurationFile: Get lines for config.ini and my.cnf
 *      mcc.configuration.deploy.deployCluster: Create directories, distribute files
 *      mcc.configuration.deploy.startCluster: Deploy configuration, start processes.
 *      mcc.configuration.deploy.stopCluster: Stop processes.
 *      mcc.configuration.deploy.installCluster: (Optionally) Install Cluster on
 *          requested host(s).
 *      mcc.configuration.deploy.clServStatus: Check state of Cluster services.
 *      mcc.configuration.deploy.determineClusterRunning: Determine just if Cluster
 *          services are in any sort of running state.
 *      mcc.configuration.deploy.viewCmds: Save then view log of issued commands.
 *      mcc.configuration.deploy.setupClusterTools: Set up drop down button with
 *          commands that can be run independently.
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
 *      getInstallCommands: List of commands to install Cluster on host(s).
 *        hostName, hostID in Stores, platform, type: hName, platform, REPO/DOCKER, RPMYUM, DPKG...
 *      getPrepCmds(host, platform, url, version): Array of commands used to prepare Host.
 *        Params: IP address, "RPMYUM", repo URL, for example, OS MAJOR version.
 *      getHostPortPairs: Retrieve HOST:PORT pairs for FW manipulation.
 *      openFireWall: Open all necessary ports on all hosts user requested.
 *      saveConfigToFile(): Persist eventual changes to firewall and installation to config file.
 *      getPort: Get port value for MGMT process.
 *      checkServiceState(nodeId): For Windows; returns NOT INSTALLED, RUNNING, STOPPED.
 *      getKillProcessCommands(process): In scenario where ndb_mgm -e'SHUTDOWN' fails
 *        to take Cluster down, STOP will issue KILL -9/TASKLIST kill against
 *        primary Cluster MGMT node but this leaves us with (potentially) 1 more
 *        MGMT process and all of DATANode processes to kill. This function returns
 *        those commands.
 *      determineProcessRunning(clSt, nodeID): Does the node run already thus not
 *          requiring START or STOP command.
 *      getInstallServiceCommands(process): Generate the (array of) service installation
 *          commands for the given process(es).To be called as:
 *          var commands = _getClusterCommands(getInstallServiceCommands, ["management", "data","sql"]);
 *      getRemoveServiceCommands(process): Generate the (array of) service removal
 *          commands for the given process(es).To be called as:
 *              var commands = _getClusterCommands(getRemoveServiceCommands, ["management", "data","sql"]);
 *      moveProgress(title, text): Use "indeterminate" progress to have GUI for
 *          operations with undetermined progress.
 *      initMySQLds(inNeedOfInit): Initialize (insecure!) mysqlds.
 *      startDNodes(): Start Data nodes in parallel.
 *      startMySQLds(): Start SQL nodes in parallel.
 *      removeProgressDialog(): remove progress box from screen.
 *      determineAllClusterProcessesUp(clSt): This is used to signal "Show logs"
 *          should be available.
 *      displayLogFilesLocation(display): If display, alert user where log files
 *          for Cluster are. If not, return array of [hostName + ":" +
 *          processTypes[proc.getValue("processtype")].getValue("name") +
 *                   ": " +  getEffectiveInstanceValue(proc, "DataDir");
 *          Names are: 'ndb_mgmd', 'ndbd', 'ndbmtd' and 'mysqld'
 *      checkSSHConnections(): Check array of remote ssh connections in BE against
 *          hostnames in FE. BE will remove connections to non-existing hosts.
 *      setupSSHConnections(): Connect or refresh SSH connections to hosts.
 *      sendSSHCleanupReq(): Clean up array of permanent connections to remote hosts.
 *      listStartProcessCommands: Plain list of startup commands.
 *      verifyConfiguration: Self explanatory.
 *      getTimeStamp: Returns current timestamp [HH:mm:ss].
 *      cancelButton(buttonName): Cancel requested button state (busy -> normal)
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
 *        Implement unit tests.
 *
 *  IE considerations: Can not use "includes" but "IndexOf".
 *      No parallel execution, workers return promise.
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.require('dijit.ProgressBar');
dojo.require('dijit.DropDownMenu');
dojo.require('dijit.form.DropDownButton');
dojo.require('dijit.MenuItem');
dojo.require('dijit.registry');

dojo.provide('mcc.configuration.deploy');

dojo.require('mcc.configuration');
dojo.require('mcc.util');
dojo.require('mcc.storage');
if (!!window.MSInputMethodContext && !!document.documentMode) {
    dojo.require('mcc.userconfigIE');
} else {
    dojo.require('mcc.userconfig');
}

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
mcc.configuration.deploy.viewCmds = viewCmds;
mcc.configuration.deploy.setupClusterTools = setupClusterTools;
mcc.configuration.deploy.sendSSHCleanupReq = sendSSHCleanupReq;
mcc.configuration.deploy.listStartProcessCommands = listStartProcessCommands;
/******************************* Internal data ********************************/

var cluster = null;         // The cluster item
var clusterItems = [];      // All items, indexed by their id
var hosts = [];             // All hosts            --- " ---
var processes = [];         // All processes        --- " ---
var processTypes = [];      // All process types    --- " ---
var processTypeMap = {};    // All process types indexed by name
var processFamilyMap = {};  // All process types indexed by family
var fileExists = [];        // All mysqlds - Nodeid and a Boolean telling if datadir
// files exists. Now used for MGMT node *.bin.* file. Reset in DEPLOY.
// Set by checking in START at the beginning (to allow for proper init). Set to
// TRUE when START succeeds and at the beginning of STOP (though might not be necessary).
var forceStop = false;
var cmdLog = []; // Log of commands issued and SUCCESS/FAILED flag.
var waitWorkers = []; // List of defers for worker processes (Install/Deploy, !IE).
var Workers = []; // Array of worker processes (Install/Deploy, !IE).
var installProgress = 1;
var allHostsConnected = null;// If not all hosts connected, we disable DEPLOY, START and STOP.
var RemoteHostsList = []; // Filled with call to checkSSHConnections()
var debugKillPath = false; // variable to test kill path
/****************************** Implementation ********************************/

/******************************* Context setup ********************************/
// Fetch all cluster data into clusterItems list, create access structures
function setupContext () {
    // Reset data structures
    cluster = null;
    clusterItems = [];
    hosts = [];
    processes = [];
    processTypes = [];
    processTypeMap = {};
    processFamilyMap = {};
    allHostsConnected = false;
    RemoteHostsList = [];
    fileExists = [];

    var waitCondition = new dojo.Deferred(); // Final deferred to return.
    var waitStores = [  // Collection of individual stores defers.
        new dojo.Deferred(), // Individual stores defers.
        new dojo.Deferred(),
        new dojo.Deferred(),
        new dojo.Deferred()
    ];
    var waitAllStores = new dojo.DeferredList(waitStores); // Have all stores returned?
    // Get cluster item
    mcc.storage.clusterStorage().getItems().then(function (items) {
        cluster = items[0];
        console.info('[INF]Cluster item setup done');
        waitStores[0].resolve();
    });
    // Get process type items
    mcc.storage.processTypeStorage().getItems().then(function (items) {
        var waitItems = [];
        var waitAllItems = null;
        for (var i in items) {
            clusterItems[items[i].getId()] = items[i];
            processTypes.push(items[i]);
            processTypeMap[items[i].getValue('name')] = items[i];
            // The first ptype of the given family will be prototypical
            if (!processFamilyMap[items[i].getValue('family')]) {
                processFamilyMap[items[i].getValue('family')] = items[i];
                // Setup process family parameters
                waitItems.push(new dojo.Deferred());
                (function (i, j) {
                    mcc.configuration.typeSetup(items[i]).then(
                        function () {
                            console.debug('[DBG]' + 'Setup process family ' +
                                items[i].getValue('family') + ' done');
                            waitItems[j].resolve();
                        }
                    );
                })(i, waitItems.length - 1);
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
                var family = clusterItems[items[i].getValue('processtype')].getValue('family');
                mcc.configuration.instanceSetup(family, items[i]).then(
                    function () {
                        console.debug('[DBG]' + 'Setup process ' +
                            items[i].getValue('name') + ' done');
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
/**
 *Provides current timestamp.
 *
 * @returns String, in HH:mm:ss format
 */
function getTimeStamp () {
    var d = new Date();
    return '[' + ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2) + ':' +
        ('0' + d.getSeconds()).slice(-2) + ']';
}
/**
 *Cancels dijit.BusyButton.
 *
 * @param {string} buttonName    the Id of widget
 */
function cancelButton (buttonName) {
    try {
        if (dijit.byId(buttonName).isBusy) {
            dijit.byId(buttonName).cancel();
        }
    } catch (e) {
        console.error('[ERR]' + 'Failed to cancel the button');
    }
}

/**
 *Form SSH block for message body to send to hosts.
 * This is for worker threads (deploy/install).
 *
 * @param {string} hostName name of host to look for in hosts object
 * @returns {JSON} block ready to be put into command message
 */
function formProperSSHBlock (hostName) {
    if (mcc.util.isEmpty(hostName)) {
        return { keyBased: false, user: '', pwd: '' };
    }
    if (hostName === '127.0.0.1' || (hostName.toLowerCase() === 'localhost')) {
        return { keyBased: false, user: '', pwd: '' };
    }

    var hostSetUseSSH = false;
    var hostSetSSHPwd = '';
    var hostSetSSHUsr = '';
    var hostSetSSHKFile = '';
    var hostSetPwd = '';
    var hostSetUsr = '';
    for (var h in hosts) {
        if (hosts[h].getValue('name') === hostName) {
            hostSetUseSSH = hosts[h].getValue('key_auth');
            if (hostSetUseSSH) {
                hostSetSSHPwd = hosts[h].getValue('key_passp');
                hostSetSSHUsr = hosts[h].getValue('key_usr');
                hostSetSSHKFile = hosts[h].getValue('key_file');
            } else {
                hostSetPwd = hosts[h].getValue('usrpwd');
                hostSetUsr = hosts[h].getValue('usr');
            }
            break;
        }
    };
    if (hostSetUseSSH) {
        return {
            keyBased: true,
            key: '',
            key_user: hostSetSSHUsr,
            key_passp: hostSetSSHPwd,
            key_file: hostSetSSHKFile
        };
    } else {
        if (hostSetPwd || hostSetUsr) {
            return { keyBased: false, user: hostSetUsr, pwd: hostSetPwd };
        } else {
            // ClusterLevel
            if (mcc.gui.getSSHkeybased()) {
                return {
                    keyBased: true,
                    key: '',
                    key_user: mcc.gui.getClSSHUser(),
                    key_passp: mcc.gui.getClSSHPwd(),
                    key_file: mcc.gui.getClSSHKeyFile()
                };
            } else {
                return { keyBased: false, user: mcc.gui.getSSHUser(), pwd: mcc.gui.getSSHPwd() };
            }
        }
    }
}

/**
 *Pings all remote hosts. Back end allocates as many worker threads as there is remote hosts so
 * beware of resources.
 * Shows list of remote hosts with ping response
 *
 * @returns {dojo.Deferred()}
 */
function pingHosts () {
    var waitConnections = new dojo.Deferred();
    var hostsToCheck = '';
    var hostName = '';
    var ts = '';
    moveProgress('Connecting cluster', 'Pinging hosts.');
    if (!hosts || hosts.length <= 0) {
        ts = getTimeStamp();
        console.warn('[WRN]' + 'No hosts!');
        cmdLog.push(ts + 'PING   ::FAIL::No hosts\n');
        waitConnections.resolve(false);
        return;
    }
    for (var h = 0; h < hosts.length; h++) {
        if (hosts[h].getValue('anyHost')) { continue; }
        hostName = hosts[h].getValue('name');
        hostsToCheck += hostName + ';';
    }
    if (hostsToCheck.length > 5) {
        // Weed out last ";"
        console.debug('Hostlist to ping is ' + hostsToCheck);
        mcc.server.pingRemoteHostsReq(hostsToCheck.slice(0, -1),
            function (reply) {
                var res = String(reply.body.reply_type);
                ts = getTimeStamp();
                if (res === 'ERROR') {
                    console.error('[ERR]Ping remote hosts failed, check Python console.');
                    cmdLog.push(ts + 'PING   ::FAIL::General ping remote host(s) failure\n');
                    waitConnections.resolve(false);
                } else {
                    // Split response to host+status
                    res = res.slice(0, -1); // last ";"
                    var resArr = res.split(';');
                    var tmp = null;
                    var msgStatus = '';
                    for (var g = 0; g < resArr.length; g++) {
                        tmp = resArr[g].split(' is ');
                        if (resArr[g].indexOf('responding') > 1) {
                            cmdLog.push(ts + 'PING   ::FAIL::' + 'Unable to ping  ' + tmp[0] + '\n');
                            msgStatus += 'Unable to ping  ' + tmp[0] + '<br/>';
                        } else {
                            cmdLog.push(ts + 'PING   ::SUCC::' + 'Success pinging ' + tmp[0] + '\n');
                            msgStatus += 'Success pinging ' + tmp[0] + '<br/>';
                        }
                    }
                    if (msgStatus.length > 0) {
                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:teal">' + msgStatus + '</span>');
                    }
                    waitConnections.resolve(true);
                }
            },
            function (errMsg) {
                var wrn = 'Exception trying to ping remote host(s) ' + errMsg;
                ts = getTimeStamp();
                cmdLog.push(ts + 'PING   ::FAIL::' + wrn + '\n');
                console.warn(wrn);
                wrn = '<span style="font-size:140%;color:red">' + wrn + '</span>';
                mcc.util.displayModal('I', 0, wrn);
            }
        );
    } else {
        ts = getTimeStamp();
        console.warn('[WRN]' + 'No hosts in list!');
        cmdLog.push(ts + 'PING   ::FAIL::No remote hosts in list\n');
        waitConnections.resolve(false);
    }
    return waitConnections;
}
/**
 *Checks validity of remote hosts connections. If true, we do not need to re-run connect.
 *
 * @returns Deferred, resolves to true (all remote hosts conn.)/false (some/all  not connected).
 */
function checkSSHConnections () {
    var waitConnections = new dojo.Deferred();
    var hostsToCheck = '';
    // var hostId; // = null;
    var hostName = '';
    var ts = '';
    moveProgress('Connecting cluster', 'Checking permanent connections.');
    if (!hosts || hosts.length <= 0) {
        ts = getTimeStamp();
        console.warn('[WRN]' + 'No hosts in list!');
        cmdLog.push(ts + 'CHECKRC::FAIL::No remote host(s) in list\n');
        waitConnections.resolve(false);
        return;
    }

    for (var h = 0; h < hosts.length; h++) {
        if (hosts[h].getValue('anyHost')) { continue; }
        hostName = hosts[h].getValue('name');
        if (String(hostName).toLowerCase() === 'localhost' || hostName === '127.0.0.1') { continue; }
        hostsToCheck += hostName + ';';
    }
    console.debug('[DBG]HostsToCheck connections to: ' + hostsToCheck);
    if (hostsToCheck.length > 5) {
        // Weed out last ";"
        // Check and weed out false perm.remote connections!
        RemoteHostsList = [];
        mcc.server.listRemoteHostsReq(hostsToCheck.slice(0, -1),
            function (reply) {
                ts = getTimeStamp();
                if (String(reply.body.reply_type) !== 'ERROR') {
                    var hlpStr = String(reply.body.out).split(',');
                    for (h in hlpStr) {
                        if (hlpStr[h]) {
                            RemoteHostsList.push(hlpStr[h]);
                        }
                    }
                    cmdLog.push(ts + 'CHECKRC::SUCC::Checked ' + RemoteHostsList.length +
                        ' permanent connections to remote host(s)\n');
                    waitConnections.resolve(true);
                } else {
                    RemoteHostsList = [];
                    console.error('[ERR]listRemoteHostsReq returned error');
                    cmdLog.push(ts + 'CHECKRC::FAIL::Checked permanent connections to remote host(s)\n');
                    waitConnections.resolve(false);
                }
            },
            function (errMsg) {
                var wrn = 'Couldn\'t check permanent connections to remote host(s), err message is ' + errMsg;
                ts = getTimeStamp();
                cmdLog.push(ts + 'CHECKRC::FAIL::' + wrn + '\n');
                console.warn(wrn);
                RemoteHostsList = [];
                waitConnections.resolve(false);
            }
        );
    } else {
        RemoteHostsList = [];
        RemoteHostsList.push('localhost');
        waitConnections.resolve(true);
    }
    return waitConnections;
}

/**
 *Setup SSH connections to all hosts. checkSSHConnections() is always run before this. This is a
 * signal to back end to (re)create all SSH connections to remote host(s).
 *
 * @returns Deferred, resolves to true (all remote hosts conn.)/false (some) remote hosts !connected.
 */
function setupSSHConnections () {
    var waitConnections = new dojo.Deferred();
    if (RemoteHostsList.length === 0) {
        removeProgressDialog();
        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Error collecting remote host list!</span>');
        waitConnections.resolve(false);
    }
    if (RemoteHostsList.length === 1 && (
        String(RemoteHostsList[0]).toLowerCase() === 'localhost' ||
            String(RemoteHostsList[0]) === '127.0.0.1')) {
        // All (one) hosts local in one way or the other.
        removeProgressDialog();
        RemoteHostsList = [];
        allHostsConnected = true;
        waitConnections.resolve(true);
    }
    // Send echo 0 to every host.
    var chkHostSSHConn = [];
    var commands = {};
    var hostName = '';
    for (var h = 0; h < RemoteHostsList.length; h++) {
        hostName = RemoteHostsList[h];
        commands[h] = new ProcessCommand(hostName, '', 'echo');
        delete commands[h].msg.file.autoComplete;
        commands[h].isDone = false;
        commands[h].addopt('0');
        commands[h].Actual = 'echo 0';
        commands[h].progTitle = 'Checking SSH connections.';
        commands[h].msg.isCommand = true;
        chkHostSSHConn.push(commands[h]);
    }
    var doneStarting = false;
    var errorReplies = 0;
    var howMany = chkHostSSHConn.length;
    if (howMany === 0) { doneStarting = true; }
    var reqHead;
    function onErr (errMsg, errReply) {
        // No need to display each failure.
        console.error('[ERR]' + 'Error occurred while connecting to node: ' + errMsg);
        ++errorReplies;
        var progBar = dijit.byId('configWizardProgressBar');
        if (progBar) {
            var visualTile = dojo.query('.dijitProgressBarTile', progBar.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }
        var index = -1;
        // Match rep.head.seq with chkHostSSHConn[i].seq to determine which command returned!
        for (var i = 0; i < chkHostSSHConn.length; i++) {
            if (Number(chkHostSSHConn[i].seq) === Number(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        moveProgress('Connecting cluster', 'node ' + chkHostSSHConn[index].msg.file.hostName + ' connect fail.');
        console.warn(ts + '[ERR]CONNECT::FAIL::' + chkHostSSHConn[index].msg.file.hostName + ', ' +
            errMsg + '.');
        cmdLog.push(ts + 'CONNECT::FAIL::' + chkHostSSHConn[index].msg.file.hostName + ', ' +
            errMsg + '\n');
        --howMany;
        chkHostSSHConn[index].isDone = true;
        updateProgressAndConnectNextNode();
    }

    function onRep (rep) {
        var index = -1;
        var ro = String(rep.body.out);
        // console.debug("rep is %o", rep);
        // Match rep.head.seq with chkHostSSHConn[i].seq to determine which command returned!
        for (var i = 0; i < chkHostSSHConn.length; i++) {
            if (Number(chkHostSSHConn[i].seq) === Number(rep.head.rSeq)) {
                index = i;
                break;
            }
        }
        if (ro.indexOf('err code:') === 0) {
            onErr(ro, rep); // send string + full reply
            return;
        }
        moveProgress('Connecting cluster', 'node ' + chkHostSSHConn[index].msg.file.hostName + ' connected.');
        chkHostSSHConn[index].isDone = true;
        var ts = getTimeStamp();
        cmdLog.push(ts + 'CONNECT::SUCC::' + chkHostSSHConn[index].msg.file.hostName + '\n');
        console.debug('INF:' + ts + 'CONNECT::SUCC::' + chkHostSSHConn[index].msg.file.hostName + '.');
        --howMany;
        updateProgressAndConnectNextNode();
    }

    function updateProgressAndConnectNextNode () {
        if (doneStarting) { return; }
        if (howMany <= 0) {
            var message;
            if (chkHostSSHConn.length > 0) {
                message = errorReplies
                    ? 'Connecting to hosts has completed, but ' + errorReplies + ' out of ' +
                    chkHostSSHConn.length + ' connections failed' : 'Cluster nodes connected successfully.';
            }
            console.debug('[DBG]' + message);
            removeProgressDialog();
            if (errorReplies > 0) {
                allHostsConnected = false;
                message = '<span style="font-size:140%;color:darkorange">' + message + '</span>';
                mcc.util.displayModal('I', 0, message);
            } else { allHostsConnected = true; }
            doneStarting = true;
            waitConnections.resolve(errorReplies <= 0);
        }
    }

    // Initiate the sequence by sending cmd to all hosts. Synchronize
    // via SEQ of command that BE will return to us in response (rSEQ).
    var t = 0;
    if (howMany > 0) {
        moveProgress('Connecting cluster', 'node ' + chkHostSSHConn[t].msg.file.hostName);
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            chkHostSSHConn[t].seq = reqHead.seq;
            console.debug('[DBG]Sending connect to ' + chkHostSSHConn[t].msg.file.hostName + ' for execution.');
            mcc.server.doReq('executeCommandReq',
                { command: chkHostSSHConn[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= chkHostSSHConn.length) { t = -1; }
        } while (t >= 0);
    }
    // Check for finish.
    updateProgressAndConnectNextNode();
    return waitConnections;
}

/**
 *Drop down menu of commands we might wish to run independently from main procedures.
 *
 */
function setupClusterTools () {
    if (!dijit.byId('configWizardToolsCluster')) {
        var toolsMenu = new dijit.DropDownMenu({ style: 'display: none;' });
        var menuItem1 = new dijit.MenuItem({
            label: 'Copy SSH keys',
            onClick: function () {
                copyKeys().then(function (ok) {
                    removeProgressDialog();
                    var msg = '';
                    if (!ok) {
                        msg = '<span style="font-size:140%;color:red">PrivKey exchange operation failed!</span>';
                    } else {
                        if (cluster.getValue('MasterNode')) {
                            msg = '<span style="font-size:140%;color:teal">PrivKey exchange succeeded!<br/>' +
                                'Master node is set to ' + cluster.getValue('MasterNode') + '.</span>';
                        } else {
                            msg = '<span style="font-size:140%;color:orangered">PrivKey exchange interrupted!</span>';
                        }
                    }
                    mcc.util.displayModal('I', 0, msg);
                });
            }
        });
        toolsMenu.addChild(menuItem1);

        var menuItem2 = new dijit.MenuItem({
            label: 'Show FW host:port pairs',
            onClick: function () {
                var hpp = getHostPortPairs();
                if (hpp.length > 30) {
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:teal">' + JSON.stringify(hpp) + '</span>');
                } else {
                    var h = '';
                    for (var x in hpp) {
                        h += hpp[x] + '\n';
                    }
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:teal">' + h + '</span>');
                }
            }
        });
        toolsMenu.addChild(menuItem2);

        var menuItem3 = new dijit.MenuItem({
            label: 'Open Firewall ports',
            onClick: function () {
                // No point in running this if Cluster is up.
                var clRunning = [];
                clRunning = clServStatus();
                if (determineClusterRunning(clRunning)) {
                    var what = mcc.userconfig.setCcfgPrGen.apply(this,
                        mcc.userconfig.setMsgForGenPr('clRunning', ['openfw']));
                    if ((what || {}).text) {
                        mcc.util.displayModal('I', 3, what.text);
                    } else {
                        // since we showed this message already, now we'll just log
                        console.info('Cluster is running, no point in opening FW ports!');
                    }
                } else {
                    // First we need to be sure we have working connections to all hosts.
                    var waitCd = new dojo.Deferred();
                    checkAndConnectHosts(waitCd).then(function (ok) {
                        if (!allHostsConnected) {
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' +
                                'Can not proceed with command since not all Cluster nodes are connected!<br/>' +
                                'Please correct the problem and refresh status either by moving from/to this page ' +
                                'or by running "Connect remote hosts" from Cluster tools.<br/></span>');
                        } else {
                            // Make command array HOST: {port, port, ...} for each host. Pass for execution if wanted.
                            var FWcommands = getOpenFWPortsCommands();
                            console.debug('[DBG]' + 'Modifying firewalls...');
                            openFireWall(FWcommands, false).then(function (ok) {
                                removeProgressDialog();
                                if (!ok) {
                                    console.error('[ERR]' + 'Errors modifying firewall.');
                                } else {
                                    mcc.util.displayModal('I', 0, '<span style="color:teal">FW ports opened.</span>');
                                }
                            });
                        }
                    });
                }
            }
        });
        toolsMenu.addChild(menuItem3);

        var menuItem4 = new dijit.MenuItem({
            label: 'Verify configuration',
            onClick: function () {
                var clRunning = [];
                clRunning = clServStatus();
                var what;
                if (determineClusterRunning(clRunning)) {
                    what = mcc.userconfig.setCcfgPrGen.apply(this,
                        mcc.userconfig.setMsgForGenPr('clRunning', ['verifyConfiguration']));
                    if ((what || {}).text) {
                        console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                        mcc.util.displayModal('I', 3, what.text);
                    } else {
                        console.warn('Cluster is running, no point in verifying running configuration!');
                    }
                } else {
                    what = verifyConfiguration(false);
                    if (what) {
                        console.warn('[ERR]' + 'Failed to verify configuration.');
                        mcc.util.displayModal('H', 0, '<span style="font-size:130%;color:orangered">' + what +
                            '</span>', '<span style="font-size:140%;color:red">Configuration appears not valid!</span>');
                    } else {
                        console.info('[INF]' + 'Configuration appears valid.');
                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:teal">Configuration appears valid.</span>');
                    }
                }
            }
        });
        toolsMenu.addChild(menuItem4);

        var menuItem9 = new dijit.MenuSeparator({
        });
        toolsMenu.addChild(menuItem9);

        // No more than 14px for font-size. Won't look good.
        var menuItem5 = new dijit.MenuItem({
            label: 'DELETE all data and files',
            style: 'color:red; font-size:12px; font-weight:bold;',
            onClick: function () {
                var clRunning = [];
                clRunning = clServStatus();
                if (determineClusterRunning(clRunning)) {
                    var what = mcc.userconfig.setCcfgPrGen.apply(this,
                        mcc.userconfig.setMsgForGenPr('clRunning', ['deleteDatadir']));
                    if ((what || {}).text) {
                        console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                        mcc.util.displayModal('I', 3, what.text);
                    } else {
                        console.warn('Cluster is running, can\'t delete data files now!');
                    }
                } else {
                    // First we need to be sure we have working connections to all hosts.
                    var waitCd = new dojo.Deferred();
                    checkAndConnectHosts(waitCd).then(function (ok) {
                        console.debug('[DBG]done with call to checkAndConnectHosts, result is ' + ok);
                        if (!allHostsConnected) {
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' +
                                'Can\t proceed with command since not all Cluster nodes are connected!<br/>' +
                                'Please correct the problem and refresh status either by moving from/to this page ' +
                                'or by running "Connect remote hosts" from Cluster tools.</span>');
                        } else {
                            var doCleanUp = false;
                            doCleanUp = confirm('Do you wish to DELETE all files and DATA\nin DATADIR specified ' +
                                'on Hosts page?');
                            cleanUpPreviousRunDatadirs(doCleanUp).then(function (ok) {
                                removeProgressDialog();
                                if (ok) {
                                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:teal">Done ' +
                                        'cleaning up DATADIRs.</span>');
                                    cluster.setValue('Started', false);
                                    mcc.userconfig.setCfgStarted(false);
                                    saveConfigToFile();
                                    // replace shadow with contents of stores
                                    mcc.userconfig.setOriginalStore('cluster');
                                    mcc.userconfig.setOriginalStore('host');
                                    mcc.userconfig.setOriginalStore('process');
                                    mcc.userconfig.setOriginalStore('processtype');
                                } else {
                                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' +
                                        'Something went wrong with deleting DataDir(s)!\nPlease check log ' +
                                        'of commands and console.</span>');
                                }
                            });
                        }
                    });
                }
            }
        });
        toolsMenu.addChild(menuItem5);

        var menuItem10 = new dijit.MenuSeparator({
        });
        toolsMenu.addChild(menuItem10);

        // Item 6 - Remove Windows services.
        var menuItem6 = new dijit.MenuItem({
            label: 'Remove Windows services',
            onClick: function () {
                var clRunning = [];
                clRunning = clServStatus();
                if (determineClusterRunning(clRunning)) {
                    var what = mcc.userconfig.setCcfgPrGen.apply(this,
                        mcc.userconfig.setMsgForGenPr('clRunning', ['removeServices']));
                    if ((what || {}).text) {
                        console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                        mcc.util.displayModal('I', 3, what.text);
                    } else {
                        console.warn('Cluster is running, can\'t remove services now!');
                    }
                } else {
                    // This is overkill but better to have user informed.
                    var cmds = _getClusterCommands(getRemoveServiceCommands, ['management', 'data', 'sql']);
                    if (cmds.length <= 0) {
                        mcc.util.displayModal('I', 3, '<span style="font-size:140%;color:teal">No commands ' +
                            'generated for removing Windows services!<br/>Probably no Windows hosts in Cluster.</span>');
                        return;
                    }
                    // console.debug("[DBG]Services remove commands are:c%o", cmds);
                    removeServices().then(function (ok) {
                        console.info('[INF]Done removing services.');
                    });
                }
            }
        });
        toolsMenu.addChild(menuItem6);

        // Item 7 - Install Windows services.
        var menuItem7 = new dijit.MenuItem({
            label: 'Install processes as Windows service',
            onClick: function () {
                var clRunning = [];
                clRunning = clServStatus();
                if (determineClusterRunning(clRunning)) {
                    var what = mcc.userconfig.setCcfgPrGen.apply(this,
                        mcc.userconfig.setMsgForGenPr('clRunning', ['installServices']));
                    if ((what || {}).text) {
                        console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                        mcc.util.displayModal('I', 3, what.text);
                    } else {
                        console.warn('Cluster is running, can\'t install services now!');
                    }
                } else {
                    // This is overkill but better to have user informed.
                    var cmds = _getClusterCommands(getInstallServiceCommands, ['management', 'data', 'sql']);
                    if (cmds.length <= 0) {
                        mcc.util.displayModal('I', 3, '<span style="font-size:140%;color:orangered">No commands ' +
                            'generated for installing processes ' +
                            'as Windows services!<br/>Probably no Windows hosts in Cluster.</span>');
                        return;
                    }
                    console.debug('[DBG]Service install commands are: %o', cmds);
                    installServices().then(function (ok) {
                        modifyServices().then(function (ok) {
                            console.info('[INF]Done.');
                        });
                    });
                }
            }
        });
        toolsMenu.addChild(menuItem7);

        // Item 8 - Get log files locally.
        var menuItem8 = new dijit.MenuItem({
            label: 'Get log files',
            onClick: function () {
                // First we need to be sure we have working connections to all hosts.
                var waitCd = new dojo.Deferred();
                checkAndConnectHosts(waitCd).then(function (ok) {
                    if (!allHostsConnected) {
                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Can not proceed ' +
                            'with command since not all ' +
                            'Cluster nodes are connected!<br/>Please correct the problem and refresh status ' +
                            'either by moving from/to this page ' +
                            'or by running <strong>Connect remote hosts</strong> from Cluster tools.</span>');
                    } else {
                        var logCmd = getFetchlogCommands();
                        if (logCmd.length <= 0) {
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">No log ' +
                                'fetching commands generated!</span>');
                            return;
                        }
                        // Now we need to run those.
                        updateProgressDialog('Getting log files',
                            'Sending commands.',
                            { maximum: logCmd.length + 1,
                                progress: 1 }, false);
                        sendFileOps(logCmd).then(function (ok) {
                            removeProgressDialog();
                            console.info('[INF]Log commands fetched to [HOME]/.mcc.');
                            var msg = 'Log commands fetched to [HOME]/.mcc directory';
                            if (!ok) {
                                msg += ' but there were errors. Check command log.';
                            } else { msg += '.'; }
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' + msg + '</span>');
                        });
                    }
                });
            }
        });
        toolsMenu.addChild(menuItem8);

        // Item 11 - Connect remote hosts.
        var menuItem11 = new dijit.MenuItem({
            label: 'Connect remote hosts',
            onClick: function () {
                var msg = '';
                checkSSHConnections().then(function (ok) {
                    if (!ok) {
                        msg = '[ERR]Couldn\'t check permanent connections to remote host(s).\n';
                        console.error('[ERR]' + 'checkSSHConnections failed.');
                    }
                    setupSSHConnections().then(function (ok) {
                        console.info('[INF]Connecting to remote hosts finished.');
                        msg = 'Connecting to remote hosts finished';
                        if (!ok) {
                            console.error('[ERR]' + 'setupSSHConnections failed.');
                            msg += ' but there were errors. Check command log.';
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' + msg + '</span>');
                        } else {
                            msg += ' successfully.';
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' + msg + '</span>');
                        }
                    });
                });
            }
        });
        toolsMenu.addChild(menuItem11);

        // Item 12 - Plain ping hosts.
        var menuItem12 = new dijit.MenuItem({
            label: 'Ping remote hosts',
            onClick: function () {
                pingHosts().then(function (ok) {
                    removeProgressDialog();
                });
            }
        });
        toolsMenu.addChild(menuItem12);

        // Draw the drop down button with tools.
        var button = new dijit.form.DropDownButton({
            id: 'configWizardToolsCluster',
            label: 'Cluster tools',
            name: 'ddClusterTools',
            baseClass: 'fbtn',
            dropDown: toolsMenu
        });
        // Determine DOM node (always different).
        var widget = dijit.registry.byId('configWizardViewCmds');
        if (widget) {
            var dNode = widget.domNode.parentNode;
            if (dNode) {
                dNode.appendChild(button.domNode);
            } else {
                console.error('[ERR]Failed to obtain parent node, Cluster tools unavailable!');
                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Failed to obtain parent ' +
                    'node, Cluster tools unavailable!</span>');
            }
        } else {
            console.error('[ERR]Failed to obtain widget, Cluster tools unavailable!');
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Failed to obtain widget, ' +
                'Cluster tools unavailable!</span>');
        }
    }
}

/***************************** Context access *********************************/
/* Sample of processType store:                 Sample of processes store:
		{                                           {
			"id": 0,                                    "id": 7,
			"name": "ndb_mgmd",                         "name": "Management node 1",
			"family": "management",                     "host": 6,
			"familyLabel": "Management layer",          "processtype": 0,
			"nodeLabel": "Management node",             "NodeId": 49,
			"minNodeId": 49,                            "seqno": 1
			"maxNodeId": 52,                        },
			"currSeq": 2
        },
*/

/**
 *Retrieve all processes of a certain process type using the name of process type..
 *
 * @param {string} name  processType:Name(ndb_mgmd,mysqld...)->processType:Id <==> processes:processtype
 * @returns {any[]} Array[processStore items]
 */
function processTypeInstances (name) {
    var items = [];
    var typeId = processTypeMap[name].getId();
    for (var i in processes) {
        if (String(processes[i].getValue('processtype')) === String(typeId)) {
            items.push(processes[i]);
        }
    }
    return items;
}

/**
 *Retrieve all processTypes of a certain family (management, data, sql, api). For example, both
 * both name:ndbd and name:ndbmtd belong to same family (data) with processType:Id=2.
 *
 * @param {string} name  processTypeStore:family
 * @returns {any[]} Array[processTypeStore items]
 */
function processFamilyTypes (name) {
    var items = [];
    for (var i in processTypes) {
        if (processTypes[i].getValue('family') && name) {
            if (processTypes[i].getValue('family').toLowerCase() === name.toLowerCase()) {
                items.push(processTypes[i]);
            }
        }
    }
    return items;
}

/**
 * Retrieve all processes of a certain process family (management, data, sql, api).
 *
 * @param {string} name  processTypeStore:family
 * @returns {any[]} Array[processesStore items]
 */
function processFamilyInstances (name) {
    var items = [];
    var types = processFamilyTypes(name);
    for (var t in types) {
        items = items.concat(processTypeInstances(types[t].getValue('name')));
    }
    return items;
}

/**
 *Retrieve processItems in the order specified by the families argument(management, data, sql, api).
 *
 * @param {string[]} families  family names
 * @returns {any[]} Array[processStore items]
 */
function getProcessItemsByFamilies (families) {
    var processItems = [];
    for (var fx in families) {
        processItems = processItems.concat(processFamilyInstances(families[fx]));
    }
    return processItems;
}

/**
 *Retrieve status of all processes:"CONNECTED", "STARTED", "STARTING", "SHUTTING_DOWN",
 * "NO_CONTACT", "UNKNOWN" + JS "undefined". If MGMT node is down, this fails.
 *
 * Simple Cluster, NOT started, stat is [Management node 1:undefined, SQL node 1:undefined,
 * Multi threaded data node 1:undefined]
 *
 * Then [Management node 1:CONNECTED, SQL node 1:UNKNOWN, Multi threaded data node 1:NO_CONTACT]
 *
 * @returns Array[String] representing status of each Cluster process
 */
function clServStatus () {
    var stat = [];
    for (var p in processes) {
        // No need to check API nodes (processType:Id = 4).
        if (processes[p].item.processtype[0] < 4) {
            mcc.storage.processTreeStorage().getItem(processes[p].item.id[0]).then(
                function (proc) {
                    if (proc) {
                        // No need to iterate over top level items.
                        if (mcc.storage.processTreeStorage().store().getValue(
                            proc.item, 'type').toLowerCase() === 'process') {
                            stat.push(mcc.storage.processTreeStorage().store().getValue(proc.item, 'name') + ':' +
                                mcc.storage.processTreeStorage().store().getValue(proc.item, 'status'));
                        }
                    }
                });
        }
    }
    console.debug('STAT: ' + (stat || 'no status fetched'));
    return stat;
}

/**
 *IF there is a status at all, that means MGMT node is running and so is Cluster. So we just check
 * if any other node is up. This is used to control allowed actions like clicking StartCluster
 * button when (some) processes are already up and so on.
 *
 * @param {string[]} clSt  clServStatus()
 * @returns {Boolean} Cluster is up or down
 */
function determineClusterRunning (clSt) {
    if (clSt.length <= 0) { return false; }
    var inp = [];
    for (var i in clSt) {
        inp = clSt[i].split(':');
        if (['CONNECTED', 'STARTED', 'STARTING', 'SHUTTING_DOWN'].indexOf(inp[1]) >= 0) {
            console.debug('[DBG]' + inp[0] + ' is in running state.');
            return true;
        }
    }
    return false;
}

/**
 *Check status of Cluster processes. On 1st that is down, return false. It is signal, for example,
 * logs should be checked.
 *
 * @param {string} clSt  clServStatus()
 * @returns {Boolean} Cluster is fully up or not
 */
function determineAllClusterProcessesUp (clSt) {
    if (clSt.length <= 0) { return false; }
    var inp = [];
    for (var i in clSt) {
        inp = clSt[i].split(':');
        if (['CONNECTED', 'STARTED', 'STARTING', 'SHUTTING_DOWN'].indexOf(inp[1]) < 0) {
            console.debug('[DBG]' + inp[0] + ' is not in running state.');
            return false;
        }
    }
    return true;
}

/**
 *Check if process is up or down. This reduces number of commands sent to back end, i.e. why
 * Start running process or Stop process that is not running.
 *
 * @param {string[]} clSt   clServStatus()
 * @param {string} processname process to check
 * @returns {Boolean} Process is(not) running
 */
function determineProcessRunning (clSt, processname) {
    if (clSt.length <= 0) { return false; }
    var inp = [];
    for (var i in clSt) {
        inp = clSt[i].split(':');
        if (inp[0] && processname) {
            if (inp[0].toLowerCase() === processname.toLowerCase()) {
                if (['CONNECTED', 'STARTED', 'STARTING'].indexOf(inp[1]) >= 0) {
                    console.debug('[DBG]' + inp[0] + ' is in running state.');
                    return true;
                }
            }
        }
    }
    return false;
}

/*************************** Progress bar handling ****************************/
/**
 *Main progress dialog routine.
 *
 * @param {string} title         title of progress dialog, usually running procedure
 * @param {string} subtitle      text to show on progress bar, usually running operation
 * @param {Array} props          dojo properties such as Min, Max, Position etc.
 * @param {Boolean} indeterminate show endless progress or %
 */
function updateProgressDialog (title, subtitle, props, indeterminate) {
    // Reset global variable telling us if dialog was user cancelled.
    forceStop = false;
    // Determine who called update by examining title.
    var firstWord = title.replace(/\s.*/, ''); // You can use " " instead of \s.
    if (['Deploying', 'Installing', 'Starting', 'Stopping'].indexOf(firstWord) < 0) {
        firstWord = 'Stopping';
    }
    // We know which procedure is running now so pass the info to dialog setup.
    if (!dijit.byId('progressBarDialog')) {
        progressBarDialogSetup(firstWord, indeterminate);
        dijit.byId('configWizardProgressBar').startup();
    }
    dijit.byId('progressBarDialog').set('title', title);
    dojo.byId('progressBarSubtitle').innerHTML = subtitle;
    dijit.byId('configWizardProgressBar').update(props);
    dijit.byId('progressBarDialog').show();
}

/**
 *Creates/Initializes dialog with progress bar.
 *
 * @param {string} procRunning   operation that is running
 * @param {Boolean} indeterminate show % or endless progress
 */
function progressBarDialogSetup (procRunning, indeterminate) {
    var pBarDlg = null;
    // Create the dialog if it does not already exist
    if (!dijit.byId('progressBarDialog')) {
        if (!indeterminate) {
            pBarDlg = new dijit.Dialog({
                id: 'progressBarDialog',
                duration: 0,
                content: "\
                    <div id='progressBarSubtitle'></div>\
                    <div id='configWizardProgressBar'\
                        dojoType='dijit.ProgressBar'\
                        progress: '0%',\
                        annotate='true'>\
                    </div>",
                _onKey: function () {}
            });
        } else {
            pBarDlg = new dijit.Dialog({
                id: 'progressBarDialog',
                duration: 0,
                content: "\
                    <div id='progressBarSubtitle'></div>\
                    <div id='configWizardProgressBar'\
                        dojoType='dijit.ProgressBar'\
                        indeterminate: true,\
                        annotate='true'>\
                    </div>",
                _onKey: function () {}
            });
        }
    }
    // Trap the CLOSE button only for START/INSTALL Cluster.
    if (procRunning.toUpperCase() !== 'STOPPING' && procRunning.toUpperCase() !== 'SSH') {
        pBarDlg.onCancel = function (evt) {
            console.warn('[INF]' + 'Operation ' + procRunning + ' cluster has been cancelled!');
            // It's MT now so need more code here.
            if (procRunning.toString() === 'Installing') {
                console.debug('[DBG]procRunning calling doStopInstall');
                doStop(!!window.MSInputMethodContext && !!document.documentMode, 'Installation cancelled!', true,
                    'Install');
            }
            if (procRunning.toString() === 'Deploying') {
                console.debug('[DBG]procRunning calling doStopDeploy');
                doStop(!!window.MSInputMethodContext && !!document.documentMode, 'Deployment cancelled!', true,
                    'Deploy');
            }
            cancelButton('configWizardStopCluster');
            cancelButton('configWizardStartCluster');
            cancelButton('configWizardDeployCluster');
            cancelButton('configWizardInstallCluster');
            // this.destroyRecursive(false);?
            forceStop = true;
        };
    } else {
        dojo.style(dijit.byId('progressBarDialog').closeButtonNode, 'display', 'none');
    }
}
/**
 *Removes progress dialog from screen.
 *
 */
function removeProgressDialog () {
    if (dijit.registry.byId('progressBarDialog')) {
        do {
            if (dijit.registry.byId('progressBarDialog').open) {
                dijit.registry.byId('progressBarDialog').destroyRecursive();
                break;
            }
        } while (true);
    }
}
/**
 *Initializes/starts indeterminate progress for back-end and parallel operations.
 *
 * @param {string} title title of progress dialog
 * @param {string} text text to show on progress dialog
 */
function moveProgress (title, text) {
    updateProgressDialog(title, text,
        { indeterminate: true }, true);
}

/**
 *Retrieve HOST:PORT pairs depending on Cluster processes distribution for FW manipulation.
 *
 * @returns Array["host external IP address":"port that should be open"]
 */
function getHostPortPairs () {
    // One pass over processes to group by host, prepare for direct lookup
    var processesOnHost = [];
    var p;
    for (p in processes) {
        // Create array unless already present
        if (!processesOnHost[processes[p].getValue('host')]) {
            processesOnHost[processes[p].getValue('host')] = [];
        }
        // Append process to array
        processesOnHost[processes[p].getValue('host')].push(processes[p]);
    }
    var HostPortPairs = [];
    // Do verification for each host individually
    for (var h = 0; h < hosts.length; h++) {
        var hostId = hosts[h].getId();
        var hostName = hosts[h].getValue('name');
        console.debug('[DBG]' + 'Determining ports to open on host ' + hostName);
        // one loop
        var proc;
        for (p in processesOnHost[hostId]) {
            // process instance
            proc = processesOnHost[hostId][p];
            // various attributes needed
            var nodeid = proc.getValue('NodeId');
            var name = proc.getValue('name');
            var a = processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() + '';
            console.debug('[DBG]' + 'Check process ' + name + ' (' + nodeid + ')');
            var port = null;
            // only mysqld processes have socket and port
            if (a === 'mysqld') { port = getEffectiveInstanceValue(proc, 'Port'); }
            // only ndb_mgmd processes have portnumber
            if (a === 'ndb_mgmd') { port = getEffectiveInstanceValue(proc, 'Portnumber'); }
            // only ndbmtd/ndbd processes have ServerPort
            if ((a === 'ndbmtd') || (a === 'ndbd')) {
                port = getEffectiveInstanceValue(proc, 'ServerPort');
            }
            // Check that all processes have different port numbers
            if (port) { HostPortPairs.push(hostName + ':' + port); }
        }
    }
    return HostPortPairs;
}

/**
 *Compose array of FW manipulation commands based on output from getHostPortPairs, user preferences
 * for certain host and supported OS. Works for RPM/YUM platforms atm. Requires firewall-cmd and
 * sudo privilege.
 *
 * @returns Array[FW modification commands]
 */
function getOpenFWPortsCommands () {
    // var hostId; // = 0;
    var hostName = '';
    var openFWPortsOnHost = false;
    var platform = '';
    // "WINDOWS", "CYGWIN", "DARWIN", "SunOS", "Linux"
    var flavor = '';
    // RPM/YUM: "ol": OS=el, "fedora": OS=fc, "centos": OS=el, "rhel": OS=el,
    // RPM:ZYpp: "opensuse": OS=sles
    // There is no "latest" for APT repository. Also, there is no way to discover newest.
    // DPKG/APT: "ubuntu": from APT, OS=ubuntu, "debian": from APT, OS=debian
    var ver = '';
    var array = [];
    var anyHost = false;
    var modifyFWCmds = [];
    var HostPort = getHostPortPairs();
    console.info('[INF]' + 'Host:Port array is: ' + HostPort);
    for (var h = 0; h < hosts.length; h++) {
        anyHost = hosts[h].getValue('anyHost');
        if (anyHost) { continue; }
        if (hosts[h].getValue('name').toLowerCase() === 'localhost' ||
            hosts[h].getValue('name') === '127.0.0.1') { continue; }
        openFWPortsOnHost = hosts[h].getValue('openfwhost');
        console.debug('[DBG]' + 'For host ' + hosts[h].getValue('name') + ' OpenFW is ' +
            openFWPortsOnHost);
        platform = hosts[h].getValue('uname') || '';
        // "WINDOWS", "CYGWIN", "DARWIN", "SunOS", "Linux"
        if (platform.toUpperCase() === 'LINUX') { // Only Linux for now.
            // Skip default AnyHost used to init storage!
            if (openFWPortsOnHost && !anyHost) {
                // hostId = hosts[h].getId();
                hostName = hosts[h].getValue('name');
                console.debug('[DBG]' + 'Preparing host ' + hostName);
                // collecting more than necessary data.
                flavor = hosts[h].getValue('osflavor');
                ver = hosts[h].getValue('osver');
                console.debug('[DBG]Platform & OS details ' + platform + ', ' + flavor + ', ' + ver);
                array = ver.split('.');
                ver = array[0]; // Take just MAJOR
                var comm = {};
                for (var i in HostPort) {
                    if (hostName === HostPort[i].split(':')[0]) {
                        comm[i] = new ProcessCommand(hostName, '', 'sudo');
                        delete comm[i].msg.file.autoComplete;
                        comm[i].addopt('firewall-cmd');
                        comm[i].addopt('--zone=public');
                        comm[i].addopt('--permanent');
                        comm[i].addopt('--add-port=' + HostPort[i].split(':')[1] + '/tcp');
                        comm[i].progTitle = 'Configuring firewall @' + HostPort[i] + '.';
                        comm[i].msg.isCommand = true;
                        comm[i].Actual = 'sudo firewall-cmd --zone=public --permanent --add-port=' +
                            HostPort[i].split(':')[1] + '/tcp';
                        modifyFWCmds.push(comm[i]);
                    }
                }
                // delete comm;
                comm = null;
                // at the end of opening ports, add reload
                var comm1 = new ProcessCommand(hostName, '', 'sudo');
                delete comm1.msg.file.autoComplete;
                comm1.addopt('firewall-cmd');
                comm1.addopt('--reload');
                comm1.Actual = 'sudo firewall-cmd --reload';
                comm1.progTitle = 'Configuring firewall, reload.';
                comm1.msg.isCommand = true;
                modifyFWCmds.push(comm1);
            }
        }
    }
    if (modifyFWCmds.length > 0) {
        for (var z = 0; z < modifyFWCmds.length; z++) {
            delete modifyFWCmds[z].isDone;  // replace function
            modifyFWCmds[z].isDone = false; // with flag
        }
    }
    return modifyFWCmds;
}
/**
 *Sends commands created in getOpenFWPortsCommands to execution. Recursive call providing new index
 * into commands array. Writes result of each command to commands log. Alerts (if not silent)
 * and breaks on 1st error.
 *
 * @param {string[]} cmds  Array[FW modification commands] from getOpenFWPortsCommands
 * @param {number} curr  Index into execution sequence
 * @param {dojo.Deferred} def   Deferred to resolve after executing all commands
 * @param {Boolean} silent   Do not show alert
 *
 */
function openFWports (cmds, curr, def, silent) {
    mcc.server.doReq(
        'executeCommandReq', { command: cmds[curr].msg }, '', cluster,
        function () {
            var ts = getTimeStamp();
            cmdLog.push(ts + 'OpenFW ::SUCC::' + cmds[curr].Actual + '\n');
            // console.debug('[DBG]' + 'FW commands, reply function');
            curr++;
            if (Number(curr) === cmds.length) {
                console.debug('[DBG]' + 'No more FW commands');
                // write down HOST:PORT pairs to config.
                var tmp = getHostPortPairs();
                var tmpToStr = '';
                for (var i in tmp) {
                    tmpToStr = tmpToStr + tmp[i] + ',';
                }
                tmpToStr = tmpToStr.slice(0, -1); // Remove last ,
                console.info('[INF]' + 'Saving FWHostPortPairs ' + tmpToStr);
                cluster.setValue('FWHostPortPairs', tmpToStr);
                saveConfigToFile();
                def.resolve(true);
            } else {
                updateProgressDialog('Opening firewall ports',
                    cmds[curr].Actual, { progress: 10 * curr }, false);
                openFWports(cmds, curr, def, silent);
            }
        },
        function (errMsg) {
            var ts = getTimeStamp();
            cmdLog.push(ts + 'OpenFW ::FAIL::' + cmds[curr].Actual + ', ' + errMsg + '\n');
            var wrn = 'Unable to open ' + cmds[curr] + ': ' + errMsg;
            console.warn(wrn);
            if (!silent) {
                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
            }
            def.resolve(false);
        }
    );
}

/**
 *Main FW modification function. Calls openFWPorts to do the actual work and waits for resolved
 * promise. If saved FW configuration is the same as one that would run, does nothing.
 *
 * @param {[{}]} FWmodifyCmds  commands from getOpenFWPortsCommands
 * @param {Boolean} silent show alerts and confirm boxes
 * @returns {dojo.Deferred} Deferred, resolved to true (FW modification succeeded) or false (FW modification failed)
 */
function openFireWall (FWmodifyCmds, silent) {
    // must return promise.
    var waitCondition = new dojo.Deferred();
    if (FWmodifyCmds.length > 0) {
        // check if it should run at all
        var oldFWHPP = '';
        if (cluster.getValue('FWHostPortPairs')) {
            oldFWHPP = cluster.getValue('FWHostPortPairs');
        }
        var newFWHPP = '';
        var tmp = getHostPortPairs();
        for (var i in tmp) {
            newFWHPP = newFWHPP + tmp[i] + ',';
        }
        newFWHPP = newFWHPP.slice(0, -1); // Remove last ,
        var message = '';
        if (oldFWHPP === newFWHPP) {
            message = 'Exact commands to open FW ports already run once.\n';
        }
        if (oldFWHPP === newFWHPP && silent) {
            // nothing to do, modifications with exact same params already run successfully before
            console.info('[INF]Exact commands to open FW ports already run once. Skipping.');
            waitCondition.resolve(true);
        } else {
            if (!silent) {
                // confirm opening firewall ports
                if (!confirm(message + 'Open ports in firewall?')) {
                    waitCondition.resolve(true);
                } else {
                    openFWports(FWmodifyCmds, 0, waitCondition, false);
                }
            } else {
                if (oldFWHPP === newFWHPP) {
                    // nothing to do, modifications with exact same params already run successfully before
                    console.info('[INF]Exact commands to open FW ports already run once. Skipping.');
                    waitCondition.resolve(true);
                } else {
                    // just open the ports because FWmodifyCmds checks if user wants this
                    openFWports(FWmodifyCmds, 0, waitCondition, true);
                }
            }
        }
    } else {
        console.warn('[WRN]No FW open commands generated. Could be wrong settings or Host(s) platform is ' +
            'not RPMYUM or it\'s LOCALHOST deployment.');
        if (!silent) {
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                'No FW open commands generated!<br/>Could be wrong settings or Host(s) platform is not RPMYUM' +
                ' or it\'s LOCALHOST deployment.</span>');
        }
        waitCondition.resolve(false);
    }
    return waitCondition;
}
/**
 *Signals back end to clean up array of permanent SSH connections to remote hosts.
 * If it fails, it will show alert. In any case, result is written into commands log.
 * api.runSSHCleanupReq makes sure to use ClusterLvL credentials.
 *
 */
function sendSSHCleanupReq () {
    mcc.server.runSSHCleanupReq(function (reply) {
        console.debug('[DBG]' + 'runSSHCleanupReq, reply function');
        var ts = getTimeStamp();
        cmdLog.push(ts + 'CLEANUP::SUCC::Cleaned permanent connections to remote host(s)\n');
    },
    function (errMsg) {
        var wrn = "Couldn't clean up permanent connections to remote host(s)" + ': ' + errMsg;
        var ts = getTimeStamp();
        cmdLog.push(ts + 'CLEANUP::FAIL::' + errMsg + '\n');
        console.warn(wrn);
        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
    });
}
/**
 *Sends Copy keys request to back end. Logs overall result to commands log. For individual commands,
 * check Python console.
 *
 * @param {string} hlExtIP   List of Cluster hosts external IP addresses, comma separated
 * @param {string} hlIntIP   List of Cluster hosts internal IP addresses, not used atm
 * @param {dojo.Deferred} def  Deferred to resolve depending on outcome
 */
function sendCopyKeysReq (hlExtIP, hlIntIP, def) {
    console.debug('[DBG]sendCopyKeysReq got list of hosts: ' + hlExtIP);
    mcc.server.runcopyKeyReq(hlExtIP, hlIntIP, function (reply) {
        var ts = getTimeStamp();
        console.debug('[DBG]' + 'Public Key exchange, reply function');
        cmdLog.push(ts + 'DEPLOY ::SUCC::CopyPK ' + reply.body.result.contentString + '\n');
        // examine return here and decide how it went and then resolve.
        if (reply.body.result.contentString && String(reply.body.result.contentString) !== '') {
            console.error('[ERR]' + 'Error reply from exchanging keys is: ' + reply.body.result.contentString);
        }
        // write master node to Cluster store
        var inp = hlExtIP.split(',');
        cluster.setValue('MasterNode', inp[0]);
        saveConfigToFile();
        def.resolve(true);
    },
    function (errMsg) {
        var ts1 = getTimeStamp();
        var wrn = 'Couldn\'t exchange public keys: ' + errMsg;
        cmdLog.push(ts1 + 'DEPLOY ::FAIL::CopyPK ' + errMsg + '\n');
        console.warn(wrn);
        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
        // maybe clear master node from Cluster store too?
        def.resolve(false);
    });
}
/**
 *Main function to exchange public keys between all hosts allowing SSHing from master to the others.
 * Calls sendCopyKeysReq to do the actual work. Useful for running scripts from host within VPN.
 *
 * @returns Deferred, resolves to true if copy keys operation was successful
 */
function copyKeys () {
    // must return promise.
    var waitCondition = new dojo.Deferred();
    var msg = '';
    checkSSHConnections().then(function (ok) {
        if (!ok) {
            msg = '[ERR]Couldn\'t check permanent connections to remote host(s).\n';
            console.error('[ERR]' + 'checkSSHConnections failed.');
            waitCondition.resolve(false);
        } else {
            console.debug('[DBG] checkSSHConnections returns ' + RemoteHostsList);
            setupSSHConnections().then(function (ok) {
                console.info('[INF]Connecting to remote hosts finished.');
                msg = 'Connecting to remote hosts finished';
                if (!ok) {
                    console.error('[ERR]' + 'setupSSHConnections failed.');
                    msg += ' but there were errors. Check command log.';
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + msg + '</span>');
                    console.error(msg);
                    waitCondition.resolve(false);
                } else {
                    msg += ' successfully.';
                    console.info(msg);
                    // we now have remote hosts listed in RemoteHostsList
                    if (cluster.getValue('MasterNode')) {
                        // procedure already run successfully before
                        console.info('[INF]' + 'PKey exchange already run before\nMaster node is set to ' +
                            cluster.getValue('MasterNode') + '. Exiting.');
                        waitCondition.resolve(true);
                    } else {
                        var hostName = '';
                        // Is there more than one host?
                        if (RemoteHostsList.length > 0) {
                            for (var h = 0; h < RemoteHostsList.length; h++) {
                                if (h === 0) {
                                    hostName = RemoteHostsList[h];
                                } else {
                                    hostName += ',' + RemoteHostsList[h];
                                }
                            }
                            // Confirm exchanging public keys once again.
                            if (!confirm('Exchange public keys between Linux/Unix hosts (' + hostName + ') ?\nThis will make all ' +
                                'hosts available via SSH from ' + RemoteHostsList[0] + ' host.\nUseful for running scripts ' +
                                'from within private Cluster.')) {
                                waitCondition.resolve(true);
                            } else {
                                moveProgress('SSH', 'Waiting on back end to finish copying SSH keys...');
                                sendCopyKeysReq(hostName, hostName, waitCondition);
                            }
                        } else {
                            waitCondition.resolve(false);
                        }
                    }
                }
            });
        }
    });
    return waitCondition;
}

/**
 *Save changes made to Firewall, MasterNode, status of Start/Installation procedures to config file.
 *
 */
function saveConfigToFile () {
    var cs = mcc.storage.clusterStorage(); // cluster?
    var hs = mcc.storage.hostStorage();
    var ps = mcc.storage.processStorage();
    var pts = mcc.storage.processTypeStorage();
    var toWrite = '';
    // _getNewFileContentString generates STRING. I'm assuming it's flat JSON.
    cs.getItem(0).then(function (cluster) {
        // Remove passwords before writing configuration to file.
        var tmpcs = cs.store()._getNewFileContentString();
        var tmphs = hs.store()._getNewFileContentString();
        var tmpcsJSON = JSON.parse(tmpcs);
        var tmphsJSON = JSON.parse(tmphs);
        if (tmpcsJSON.items[0].ssh_pwd &&
            tmpcsJSON.items[0].ssh_pwd.length !== 0) {
            // replace pwd
            tmpcsJSON.items[0].ssh_pwd = '-';
        }
        if (tmpcsJSON.items[0].ssh_ClKeyPass &&
            tmpcsJSON.items[0].ssh_ClKeyPass.length !== 0) {
            // replace pwd
            tmpcsJSON.items[0].ssh_ClKeyPass = '-';
        }
        tmpcs = JSON.stringify(tmpcsJSON, null, '\t');// instead of contents of ClusterStore (cs).
        for (var i = 0; i < tmphsJSON.items.length; i++) {
            if (String(tmphsJSON.items[i].name) !== 'Any host') {
                if (tmphsJSON.items[i].usrpwd &&
                    tmphsJSON.items[i].usrpwd.length !== 0) {
                    tmphsJSON.items[i].usrpwd = '-';
                }
                if (tmphsJSON.items[i].key_passp &&
                    tmphsJSON.items[i].key_passp.length !== 0) {
                    tmphsJSON.items[i].key_passp = '-';
                }
            }
        }
        tmphs = JSON.stringify(tmphsJSON, null, '\t'); // instead of contents of HostStore (hs).
        toWrite = tmpcs + ', ' + tmphs + ', {}, ' +
            ps.store()._getNewFileContentString() + ', ' +
            pts.store()._getNewFileContentString();
    });
    console.info('[INF]Executing cfg save.');
    mcc.userconfig.writeConfigFile(toWrite, mcc.userconfig.getConfigFile());
}

/**
 *Deep checking of configuration parameters. Happens automatically, say, during DEPLOY procedure.
 *
 * @param {Boolean} silent do not show confirm boxes (only StartCluster)
 * @returns {String} list of problems or ''
 */
function verifyConfiguration (silent) {
    // One pass over processes to group by host, prepare for direct lookup
    var processesOnHost = [];
    var redoLogChecked = false;
    /*
    Since it's perfectly possible that some hosts have proper IntIP while others don't,
    we need validation of InternalIP addresses here.
    The check is in that we loop HOST store and check for hosts that have different
    ExtIP and IntIP. If all are as such, no problem. If some have different and
    some have the same, it's a problem that should be solved either by entering
    proper IntIP for hosts where ExtIP == IntIP or by setting IntIP to ExtIP for
    all hosts.
    */
    var hostDifferentIP = [];
    var hostSameIP = [];
    var extIp = '';
    var intIp = '';
    var fqdn = '';
    var msg = '';
    var msg1 = '';
    var usevpn = mcc.gui.getUseVPN();
    console.debug('[DBG]' + 'Checking external and internal IP addresses.');
    for (var z = 0; z < hosts.length; z++) {
        if (!hosts[z].getValue('anyHost')) {
            extIp = hosts[z].getValue('name') || '';
            intIp = hosts[z].getValue('internalIP') || '';
            fqdn = hosts[z].getValue('fqdn') || '';
            console.debug('[DBG]ExtIP ' + extIp + ', IntIP ' + intIp + ', fqdn ' + fqdn);
            if (extIp !== '127.0.0.1' && extIp.toUpperCase() !== 'LOCALHOST') {
                if (!intIp) {
                    console.warn('[WRN]Host ' + extIp + ' has no InternalIP defined. Setting to ' + extIp);
                    hosts[z].setValue('internalIP', extIp);
                    intIp = extIp;
                }
            }
            // Check only if ExtIP is not actually FQDN and if it's needed (!use internalIP).&& !usevpn
            // No, External IP should *always* be valid.
            if (extIp !== fqdn && extIp !== '127.0.0.1' && extIp.toUpperCase() !== 'LOCALHOST') {
                if (!mcc.util.ValidateIPAddress(extIp)) {
                    msg1 = 'Invalid external IP address ' + extIp;
                    console.warn(msg1);
                    mcc.userconfig.setCcfgPrGen('ConfigVerification', 'IPCheck', msg1, 'error', '', true);
                    msg += '<br />' + msg1;
                }
            }
            // Only important if running inside VPN. intIp doesn't have to be valid otherwise.
            // if (intIp && usevpn && !mcc.util.ValidateIPAddress(intIp)) {
            if (intIp && usevpn && !mcc.util.validateIPCondit(intIp, true, false, true)) {
                msg1 = 'Invalid internal IP address ' + intIp;
                console.warn(msg1);
                mcc.userconfig.setCcfgPrGen('ConfigVerification', 'IPCheck', msg1, 'error', '', true);
                msg += '<br />' + msg1;
            }
            if (extIp !== intIp) {
                hostDifferentIP.push(extIp);
            } else {
                hostSameIP.push(extIp);
            }
        }
    }
    // All IP addresses checked. The only way for below code failure to matter
    // is if user said he wants to run using Internal IP.
    if (usevpn) {
        if ((hostDifferentIP.length > 0 && hostSameIP.length === 0) ||
            (hostDifferentIP.length === 0 && hostSameIP.length > 0)) {
            console.debug('[DBG]' + 'External and internal IP addresses check out.');
        } else {
            msg1 = 'Configuration is invalid!<br/><br/>';
            msg1 += 'Some hosts use External and some Internal IP. ';
            msg1 += 'Please fix on Host definition page before proceeding. ';
            msg1 += 'Hosts with Ext and Int IP different:<br/>';
            for (z = 0; z < hostDifferentIP.length; z++) {
                msg1 += hostDifferentIP[z] + '<br/>';
            }
            msg1 += '<br/>Hosts with same Ext and Int IP:<br/>';
            for (z = 0; z < hostSameIP.length; z++) {
                msg1 += hostSameIP[z] + '<br/>';
            }
            msg += '<br />' + msg1;
            console.warn(msg1);
            mcc.userconfig.setCcfgPrGen('ConfigVerification', 'IPCheck', msg1, 'error', '', true);
        }
    }
    for (var p in processes) {
        // Create array unless already present
        if (!processesOnHost[processes[p].getValue('host')]) {
            processesOnHost[processes[p].getValue('host')] = [];
        }
        // Append process to array
        processesOnHost[processes[p].getValue('host')].push(processes[p]);
    }
    // Alert if NoOfReplicas==2 for an odd number of data nodes
    var noOfReplicas = parseInt(getEffectiveTypeValue(processFamilyMap['data'], 'NoOfReplicas'));
    if (processFamilyInstances('data').length % 2 === 1 && noOfReplicas === 2) {
        msg1 = 'With an odd number of data nodes, the number of replicas (NoOfReplicas) must be set to 1.';
        console.warn(msg1);
        mcc.userconfig.setCcfgPrGen('ConfigVerification', 'NoOfReplicas', msg1, 'error', '', true);
        msg += '<br />' + msg1;
    }
    // Do verification for each host individually
    for (var h = 0; h < hosts.length; h++) {
        var dirs = [];
        var ports = [];
        var files = [];
        var hostId = hosts[h].getId();
        var hostName = String(hosts[h].getValue('name'));
        if (hosts[h].getValue('anyHost')) { continue; }
        fqdn = hosts[h].getValue('fqdn') || '';

        console.debug('[DBG]Check processes on host ' + hostName);
        // One loop
        for (p in processesOnHost[hostId]) {
            // Process instance
            var proc = processesOnHost[hostId][p];
            // Various attributes
            // var id = proc.getId();
            var nodeid = proc.getValue('NodeId');
            var name = proc.getValue('name');
            console.debug('[DBG]Check process ' + name + ' (' + nodeid + ')');
            var file = null;
            var port = null;
            var dir = null;
            // ndbd have redo log
            if (String(cluster.getValue('apparea')).toLowerCase() !== 'simple testing' &&
                !redoLogChecked && (
                processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndbd' ||
                processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndbmtd')) {
                var nFiles = getEffectiveInstanceValue(proc, 'NoOfFragmentLogFiles');
                var fileSz = getEffectiveInstanceValue(proc, 'FragmentLogFileSize');
                var vol = nFiles * fileSz / 1000;
                if (mcc.util.isWin(hosts[h].getValue('uname'))) {
                    vol *= 4;
                }
                redoLogChecked = true;
                // Assume initializing 1G/min
                if (vol > 3) {
                    if (!silent) {
                        if (!confirm('Please note that the current values of the ' +
                                'data layer configuration parameters NoOfFragmentLogFiles and ' +
                                'FragmentLogFileSize mean that the ' +
                                'processes may need about ' + Math.floor(vol) +
                                ' minutes to start. Please press the ' +
                                'Cancel button below to cancel deployment, or press OK to continue.')) {
                            msg += '<br />Would take too long to start.';
                        }
                    } else {
                        console.warn('Please note that the current values of the ' +
                                'data layer configuration parameters NoOfFragmentLogFiles and ' +
                                'FragmentLogFileSize mean that the processes may need about ' +
                                Math.floor(vol) + ' minutes to start.');
                    }
                }
            }
            // Temporary fix for Windows malloc/touch slowness
            if ((processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndbd' ||
                processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndbmtd') &&
                (mcc.util.isWin(hosts[h].getValue('uname')))) {
                var DataMem = getEffectiveInstanceValue(proc, 'DataMemory'); // MB
                if (DataMem > 20480) {
                    if (!silent) {
                        if (!confirm('Please note that the current values of the ' +
                                'data layer configuration parameter DataMemory might be too big for Windows. ' +
                                'Please press the Cancel button below to cancel deployment ' +
                                'and lower the value to <= 20480MB, or ' +
                                'press OK to continue (but Cluster might not start).')) {
                            msg += '<br />DataMemory too big.';
                        }
                    } else {
                        console.warn('Please note that the current values of the ' +
                                'data layer configuration parameter DataMemory might be too big for Windows. ' +
                                'Please lower the value to <= 20480MB.');
                    }
                }
            }
            // Only mysqld processes have socket and port
            if (processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'mysqld') {
                file = getEffectiveInstanceValue(proc, 'Socket');
                port = getEffectiveInstanceValue(proc, 'Port');
            }
            // Only ndb_mgmd processes have portnumber
            if (processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndb_mgmd') {
                port = getEffectiveInstanceValue(proc, 'Portnumber');
            }
            // Only ndb_mtd processes have ServerPort
            if ((processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndbmtd') ||
                (processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() === 'ndbd')) {
                port = getEffectiveInstanceValue(proc, 'ServerPort');
            }
            // All processes except api have datadir
            if (processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() !== 'api') {
                dir = getEffectiveInstanceValue(proc, 'DataDir');
            }
            // Check that all processes have different directories
            if (dir) {
                if (dojo.indexOf(dirs, dir) !== -1) {
                    var other = processesOnHost[hostId][dojo.indexOf(dirs, dir)];
                    msg1 = 'Invalid configuration on host ' + hostName +
                        ': ' + name + ' (' + nodeid + ') has same data directory as ' + other.getValue('name') +
                        ' (' + other.getValue('NodeId') + ')';
                    msg += '<br />' + msg1;
                    console.warn(msg1);
                    mcc.userconfig.setCcfgPrGen('ConfigVerification', hostName, msg1, 'error', '', true);
                } else {
                    // Store this process' datadir on its array index
                    dirs[p] = dir;
                }
            }

            // Check that all processes have different port numbers
            if (port) {
                if (dojo.indexOf(ports, port) !== -1) {
                    var other1 = processesOnHost[hostId][dojo.indexOf(ports, port)];
                    msg1 = 'Invalid configuration on host <strong>' + hostName +
                            ': ' + name + '</strong> (' + nodeid + ') has same port number as ' +
                            other1.getValue('name') + ' (' + other1.getValue('NodeId') + ')';
                    console.warn(msg1);
                    mcc.userconfig.setCcfgPrGen('ConfigVerification', hostName, msg1, 'error', '', true);
                    msg += '<br />' + msg1;
                } else {
                    // Store this process' port number on its array index
                    ports[p] = port;
                }
            }

            // Check that all processes have different socket files
            if (file) {
                if (dojo.indexOf(files, file) !== -1) {
                    var other2 = processesOnHost[hostId][dojo.indexOf(files, file)];
                    msg1 = 'Invalid configuration on host <strong>' + hostName +
                            ': ' + name + '</strong> (' + nodeid + ') has same socket file as ' +
                            other2.getValue('name') + ' (' + other2.getValue('NodeId') + ')';
                    console.warn(msg1);
                    mcc.userconfig.setCcfgPrGen('ConfigVerification', hostName, msg1, 'error', '', true);
                    msg += '<br />' + msg1;
                } else {
                    // Store this process' socket file on its array index
                    files[p] = file;
                }
            }
        }
        // Now add ports for this host:
    } // FOR hosts
    return msg;
}

/*********************** Configuration file handling **************************/

/**
 *Main routine to distribute all configuration files over hosts. Logs actions to command log.
 *
 * @returns Deferred
 */
function distributeConfigurationFiles () {
    // External waitCondition
    var waitCondition = new dojo.Deferred();
    // Get all ndb_mgmds and all mysqlds
    var CfgProcs = processTypeInstances('ndb_mgmd').concat(processTypeInstances('mysqld'));
    // Array of wait conditions
    var waitList = [];
    var waitAll = null;
    // Loop over all ndb_mgmds and send a create file command
    for (var i in CfgProcs) {
        waitList.push(new dojo.Deferred());
        (function (i) {
            var configFile = getConfigurationFile(CfgProcs[i]);
            if (configFile.host && configFile.path &&
                    configFile.name) {
                console.debug('[DBG]Distribute configuration file for \'' +
                        CfgProcs[i].getValue('name') + '\'');
                var ts = getTimeStamp();
                cmdLog.push(ts + 'CREFILE::SUCC::' + configFile.host + ':' + configFile.path +
                    ':' + configFile.name + '\n');
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

// Routine that generate configuration files
function getConfigurationFile (process) {
    /*
        Hard-coded process type store defines ID range, name and family for each process:
        {
        "identifier": "id", "label": "name",
        "items": [
            {"id": 0,"name": "ndb_mgmd","family": "management",
            "familyLabel": "Management layer","nodeLabel": "Management node",
            "minNodeId": 49,"maxNodeId": 255,"currSeq": 2},
        which is linked to process store holding processes on hosts
        (ProcessTypeStore.ID == ProcessStore.ProcessType):
        {
            "identifier": "id", "label": "name",
            "items": [
                {"id": 8,"name": "Management node 1","host": 6,"processtype": 0,
                "NodeId": 49,"seqno": 1},
        which is in turn linked to host tree store which we loop cause
        it tells us all process belonging to each host
        (ProcessStore.Host == HostTreeStore.ID):
            {
                "id": 6, "type": "host", "name": "127.0.0.1",
                "processes": [
                    {"_reference": 8},{"_reference": 9},{"_reference": 10},
                    {"_reference": 11},{"_reference": 12}]
            },
        and links to (also in hosttree store, reference == ID):
            {"id": 8, "type": "process", "name": "Management node 1"},
            {"id": 9, "type": "process", "name": "API node 1"},
        which is linked to host store to provide humanly readable info (ID to ID):
            {"id": 6, "name": "127.0.0.1", "anyHost": false, "hwResFetch": "OK",
            "hwResFetchSeq": 2, "ram": 524155, "cores": 88, "uname": "Windows",
            "osver": "10", "osflavor": "Microsoft Windows Server 2016 Standard",
            "dockerinfo": "NOT INSTALLED", "installdir_predef": true,
            "datadir_predef": true, "diskfree": "456G", "fqdn": "...",
            "internalIP": "...", "openfwhost": false, "installonhost": false,
            "installonhostrepourl": "","installonhostdockerurl": "",
            "installonhostdockernet": "", "installdir": "F:\\SomeDir\\",
            "datadir": "C:\\Users\\user\\MySQL_Cluster\\"},
        This code loops all the families and types of processes over all hosts
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
    var hostItem = clusterItems[process.getValue('host')];
    var ptype = String(clusterItems[process.getValue('processtype')].getValue('name')) || '';

    function addln (cf, ln) {
        cf.msg += ln + '\n';
        cf.html += ln + '<br>';
    }

    if (ptype.toLowerCase() === 'api') { return null; }

    // Structure to return
    var configFile = {
        host: hostItem.getValue('name'),
        fqdn: hostItem.getValue('fqdn'), // Not really necessary any more.
        path: mcc.util.unixPath(getEffectiveInstanceValue(process, 'DataDir')),
        name: null,
        html: '#<br>',
        msg: '#\n'
    };

    if (ptype.toLowerCase() === 'ndb_mgmd') {
        var suffix = ';'
        configFile.name = 'config.ini';
        // If user modified things by hand, Cluster.Name can be different than mcc.userconfig.getConfigFile()
        configFile.html += '# Configuration file for ' + cluster.getValue('name') + '<br>#<br>';
        configFile.msg += '# Configuration file for ' + cluster.getValue('name') + '\n#\n';
        // Loop over all process families
        for (var family in processFamilyMap) {
            // Get the prototypical process type, output header
            var fType = processFamilyMap[family];
            // Special treatment of TCP buffers
            if (fType.getValue('name').toLowerCase() === 'ndbd') {
                configFile.html += '<br>[TCP DEFAULT]<br>';
                configFile.msg += '\n[TCP DEFAULT]\n';
                if (mcc.configuration.visiblePara('cfg', cluster.getValue('apparea'), 'data', 'SendBufferMemory')) {
                    configFile.html += 'SendBufferMemory=' + getEffectiveTypeValue(fType, 'SendBufferMemory') + 'M<br>';
                    configFile.msg += 'SendBufferMemory=' + getEffectiveTypeValue(fType, 'SendBufferMemory') + 'M\n';
                }
                if (mcc.configuration.visiblePara('cfg', cluster.getValue('apparea'), 'data', 'ReceiveBufferMemory')) {
                    configFile.html += 'ReceiveBufferMemory=' + getEffectiveTypeValue(fType, 'ReceiveBufferMemory') + 'M<br>';
                    configFile.msg += 'ReceiveBufferMemory=' + getEffectiveTypeValue(fType, 'ReceiveBufferMemory') + 'M\n';
                }
            }

            // Config file section header for type defaults except api
            if (fType.getValue('name').toLowerCase() !== 'api') {
                configFile.html += '<br>[' + fType.getValue('name').toUpperCase() + ' DEFAULT]<br>';
                configFile.msg += '\n[' + fType.getValue('name').toUpperCase() + ' DEFAULT]\n';
            }

            // Output process type defaults
            for (var p in mcc.configuration.getAllPara(family)) {
                var val = getEffectiveTypeValue(fType, p);
                if (val !== undefined) {
                    // Get the appropriate suffix (K, M, G, etc.)
                    suffix = mcc.configuration.getPara(family, null, p, 'suffix');
                    if (!suffix) { suffix = ''; }
                    // TCP buffers and mysqld options treated separately
                    if (String(p) !== 'SendBufferMemory' && String(p) !== 'ReceiveBufferMemory' &&
                            String(mcc.configuration.getPara(family, null, p, 'destination')) === 'config.ini' &&
                            mcc.configuration.visiblePara('cfg', cluster.getValue('apparea'), family, p)) {
                        configFile.html += mcc.configuration.getPara(
                            family, null, p, 'attribute') + '=' + val + suffix + '<br>';
                        configFile.msg += mcc.configuration.getPara(
                            family, null, p, 'attribute') + '=' + val + suffix + '\n';
                    }
                }
            }
            // Loop over all types in this family
            var ptypes = processFamilyTypes(family);
            for (var t in ptypes) {
                // Loop over all processes of this type
                var processes = processTypeInstances(ptypes[t].getValue('name'));
                for (p in processes) {
                    var id = processes[p].getId();
                    configFile.html += '<br>[' + fType.getValue('name').toUpperCase() + ']<br>';
                    configFile.msg += '\n[' + fType.getValue('name').toUpperCase() + ']\n';
                    for (var para in mcc.configuration.getAllPara(family)) {
                        var val1 = processes[p].getValue(para);
                        // Skip parameters not visible for instances
                        if (!mcc.configuration.getPara(family, id,
                            para, 'visibleInstance')) {
                            continue;
                        }
                        if (val1 === undefined) {
                            if (fType.getValue(para) === undefined) {
                                val1 = mcc.configuration.getPara(family, id, para, 'defaultValueInstance');
                            }
                        }
                        if (val1) {
                            suffix = mcc.configuration.getPara(family, null, para, 'suff') || '';
                            if (String(p) !== 'SendBufferMemory' &&
                                String(p) !== 'ReceiveBufferMemory' &&
                                String(mcc.configuration.getPara(family, null,
                                    para, 'destination')) === 'config.ini' &&
                                    mcc.configuration.visiblePara('cfg',
                                        cluster.getValue('apparea'),
                                        family, para)) {
                                configFile.html += mcc.configuration.getPara(family, null, para, 'attribute') +
                                    '=' + val1 + suffix + '<br>';
                                configFile.msg += mcc.configuration.getPara(family, null, para, 'attribute') +
                                    '=' + val1 + suffix + '\n';
                            }
                        }
                    }
                }
            }
        }
    } else if (ptype.toLowerCase() === 'mysqld') {
        // Hardcoded things for my.cnf mainly from Cluster and Host levels.
        configFile.name = 'my.cnf';
        addln(configFile, '# Configuration file for ' + cluster.getValue('name'));
        addln(configFile, '# Generated by mcc');
        addln(configFile, '#');
        addln(configFile, '[mysqld]');
        addln(configFile, 'log-error=mysqld.' + process.getValue('NodeId') + '.err');
        addln(configFile, 'datadir="' + configFile.path + 'data' + '"');
        addln(configFile, 'tmpdir="' + configFile.path + 'tmp' + '"');
        addln(configFile, 'basedir="' + mcc.util.unixPath(getEffectiveInstalldir(hostItem)) + '"');
        addln(configFile, 'port=' + getEffectiveInstanceValue(process, 'Port'));
        addln(configFile, 'ndbcluster=on');
        addln(configFile, 'ndb-nodeid=' + process.getValue('NodeId'));
        addln(configFile, 'ndb-connectstring=' + getConnectstring());
        if (!mcc.util.isWin(hostItem.getValue('uname'))) {
            addln(configFile, 'socket="' + getEffectiveInstanceValue(process, 'Socket') + '"');
            addln(configFile, 'daemonize=ON'); // 7.6.5 and up
        }
        if ((configFile.host === '127.0.0.1') || (configFile.host.toLowerCase() === 'localhost')) {
            addln(configFile, 'bind-address=127.0.0.1');
        }
        // ptype mysqld == family sql
        // Get the prototypical process type, output header
        var pfType = processFamilyMap['sql'];
        var pftypes = processFamilyTypes('sql');
        for (var z in pftypes) {
            // Loop over all processes of this type
            var procs = processTypeInstances(pftypes[z].getValue('name'));
            for (var p1 in procs) {
                // var id = procs[p1].getId();
                pfType.getValue('name');
                for (var para1 in mcc.configuration.getAllPara('sql')) {
                    // Instance value of parameter.
                    var vald1 = procs[p1].getValue(para1);
                    // Is it for this concrete node?
                    if ((para1 === 'NodeId') && (String(vald1) !== String(process.getValue('NodeId')))) { break; }
                    // These are processed separately above.
                    if (para1 === 'NodeId') { continue; }
                    if (para1 === 'HostName') { continue; }
                    if (para1 === 'DataDir') { continue; }
                    if (para1 === 'Portbase') { continue; }
                    if (para1 === 'Port') { continue; }
                    if (para1 === 'Socket') { continue; }
                    // Is parameter meant for different config file (parameters.js::destination)?
                    if (String(mcc.configuration.getPara('sql', null, para1, 'destination')) === 'config.ini') {
                        continue;
                    }
                    // No suffixes for now.
                    // Value on upper level (i.e. for all nodes of the same family).
                    var tVal;
                    // Parameter name, if any.
                    var pname = mcc.configuration.getPara('sql', null, para1, 'attribute');
                    var usrVal;
                    if (pname !== undefined) {
                        try {
                            tVal = getEffectiveTypeValue(processFamilyMap['sql'], pname);
                        } catch (e) {
                            tVal = undefined;
                        }
                        // Value on the process level.
                        usrVal = process.getValue(pname);
                    }
                    // User modified value always takes precedence.
                    if (usrVal !== undefined) {
                        // Write to my.cnf USER modified value (if any).
                        if (usrVal) {
                            usrVal = 1;
                        } else {
                            usrVal = 0;
                        }
                        addln(configFile, mcc.configuration.getPara(
                            'sql', null, para1, 'attribute') + '=' + usrVal);
                        continue;
                    } else {
                        // Otherwise, check that DEFAULT section value exists.
                        if (tVal !== undefined) {
                            // Write to my.cnf default value (if any).
                            if (tVal) {
                                tVal = 1;
                            } else {
                                tVal = 0;
                            }
                            addln(configFile, mcc.configuration.getPara(
                                'sql', null, para1, 'attribute') + '=' + tVal);
                            continue;
                        } else {
                            // Try writing default parameter value, if any.
                            if (getEffectiveInstanceValue(procs[p1], para1)) {
                                addln(configFile, mcc.configuration.getPara('sql', null, para1,
                                    'attribute') + '=' +
                                (getEffectiveInstanceValue(procs[p1], para1) ? 1 : 0));
                                continue;
                            }
                        }
                    }
                }
            }
        }
    } else {
        return null;
    }
    return configFile;
}

/************************* Parameter access support ***************************/
// Get the effective parameter value for a given process
function getEffectiveInstanceValue (process, parameter) {
    // First see if instance overrides
    var val = process.getValue(parameter);
    var id = process.getId();
    // If not overridden for instance, see if type overrides
    if (val === undefined) {
        var ptype = clusterItems[process.getValue('processtype')];
        // Get the prototypical type representing the family
        ptype = processFamilyMap[ptype.getValue('family')];
        val = ptype.getValue(parameter);
        // If not overridden for type, check predefined instance value
        if (val === undefined) {
            val = mcc.configuration.getPara(ptype.getValue('family'), id, parameter, 'defaultValueInstance');
            // If no predefined instance value, use predefined type value
            if (val === undefined) {
                val = mcc.configuration.getPara(ptype.getValue('family'), null, parameter, 'defaultValueType');
            }
        }
    }
    return val;
}

// Get the effective parameter value for a given process type
function getEffectiveTypeValue (ptype, parameter) {
    // First see if type overrides
    var val = ptype.getValue(parameter);
    // If not overridden for type, get default type value
    if (val === undefined) {
        val = mcc.configuration.getPara(ptype.getValue('family'), null, parameter, 'defaultValueType');
    }
    return val;
}

/**
 *Get the effective install directory for the given host from HostStore
 *
 * @param {object} host item from hostStore
 * @returns {string} directory where MCC looks for Cluster/MySQL binaries
 */
function getEffectiveInstalldir (host) {
    return host.getValue('installdir');
}

// Get the connect string for this cluster, use FQDN/internalIP and not hostname.
function getConnectstring () {
    var connectString = '';
    var mgmds = processTypeInstances('ndb_mgmd');
    // Loop over all ndb_mgmd processes
    for (var i in mgmds) {
        var port = getEffectiveInstanceValue(mgmds[i], 'Portnumber');
        var host = '';
        if (mcc.gui.getUseVPN()) {
            // Use internal IP instead of name:
            host = clusterItems[mgmds[i].getValue('host')].getValue('internalIP');
        } else {
            // User said no VPN.
            host = clusterItems[mgmds[i].getValue('host')].getValue('name');
        }
        connectString += host + ':' + port + ',';
    }
    return connectString;
}

function getPort (ID) {
    var port = '1186';
    var mgmds = processTypeInstances('ndb_mgmd');
    /*
    0: {…}  ItemIndex
    item: {…}
        NodeId: Array [ 49 ]
        host: Array [ 7 ]   HostID
        id: Array [ 10 ]
        name: Array [ "Management node 1" ]
        processtype: Array [ 0 ]    MGMT
        seqno: Array [1] It is the 1st MGMT node (next one, NodeId=50, would have 2 here).
    */
    // Loop over all ndb_mgmd processes, take the one with ID passed in call.
    for (var i in mgmds) {
        if (String(mgmds[i].getValue('NodeId')) === String(ID)) {
            port = getEffectiveInstanceValue(mgmds[i], 'Portnumber');
            break;
        }
    }
    return port;
}

/****************** Directory and startup command handling ********************/
/*
This function and checkCommands is overkill. Sending checkFileReq over all hosts
costs time. Current working:
    o gather checkFile commands (getCheckCommands).
    o send checkCommands to sendFileOps
    o sendFileOps sends checkCommands to sendFileOp which sends actual checkFileReq
    to API unit and provides the answer.
    o answer is then filled into fileExists array for fast lookup as below
Each call to check file/directory to back end will set us back few seconds.
a) Are mysqld(s) initialized?
    When deploying Cluster we do initialize now. Checking made sense when
    initialization was part of START procedure.
b) Was first MGMT node ever started (--initial vs --reload):
    This is legit concern but it's not worth sending checkFileReq to all SQL nodes
    too. So we actually need new check away from sendFileOps(check) that sends just
    inquiry about primary MGMT node  history.
*/
function isFirstStart (nodeid) {
    if (fileExists.length === 0) {
        // Not initialized. Should not happen EVER but it does when clicking
        // nodes in tree before deploying/starting... This displays potentially wrong
        // startup command in right pane.
        console.error('[ERR]' + ' fileExists is uninitialized!');
    }
    for (var i = 0; i < fileExists.length; i++) {
        if (parseInt(fileExists[i].nodeid) === parseInt(nodeid)) {
            console.debug('[DBG]IsFirstStart (' + nodeid + ') returns ' + !fileExists[i].fileExist);
            return !fileExists[i].fileExist;
        }
    }
    console.warn('[WRN]isFirstStart failed - should only happen when not deployed.');
    // We could disable START button here maybe?
    return false;
}

function getCheckCommands () {
    // 1st mgmt NodeID is MgmdInfo[0].ID
    var MgmdInfo = mcc.gui.getMgmtArrayInfo();
    // Array to return
    var checkDirCommands = [];
    var i, process, nodeid, host, datadir, dirSep;
    var processes = processTypeInstances('mysqld');
    fileExists = [];
    // Loop over all mysqld processes
    for (i = 0; i < processes.length; i++) {
        process = processes[i];

        // Get process type, nodeid and host
        nodeid = process.getValue('NodeId');
        host = clusterItems[process.getValue('host')];

        // Get datadir and dir separator
        datadir = mcc.util.unixPath(
            mcc.util.terminatePath(
                getEffectiveInstanceValue(process, 'DataDir')));
        dirSep = mcc.util.dirSep(datadir);

        // Initialize fileExist for the check command
        // The info will be updated by the result from checkFile in sendFileOps
        fileExists.push({
            nodeid: nodeid,
            fileExist: false
        });
        // Push check file command
        // mysqld
        checkDirCommands.push({
            cmd: 'checkFileReq',
            host: host.getValue('name'),
            path: datadir + 'data' + dirSep,
            name: 'auto.cnf',
            msg: 'File checked'
        });
    }

    // Loop over all ndb_mgmd processes
    processes = processTypeInstances('ndb_mgmd');
    for (i = 0; i < processes.length; i++) {
        process = processes[i];
        nodeid = process.getValue('NodeId');
        host = clusterItems[process.getValue('host')];
        datadir = mcc.util.unixPath(
            mcc.util.terminatePath(
                getEffectiveInstanceValue(process, 'DataDir')));
        dirSep = mcc.util.dirSep(datadir);
        if (parseInt(nodeid) === parseInt(MgmdInfo[0].ID)) {
            console.debug('[DBG]GetCheckCommands MGMT.');
            fileExists.push({
                nodeid: nodeid,
                fileExist: false
            });
            // 1st ndb_mgmd
            checkDirCommands.push({
                cmd: 'checkFileReq',
                host: host.getValue('name'),
                path: datadir,
                name: 'ndb_49_config.bin.1',
                msg: 'File checked'
            });
            break; // We only need first.
        }
    }
    return checkDirCommands;
}

// Generate array of commands to get logs to [HOME]/.mcc
function getFetchlogCommands () {
    // Array to return
    var fetchLogCommands = [];
    // Same prefix (YYYY-MM-DD_H-m) for all log files.
    var d = new Date();
    var mnth = d.getMonth() + 1;
    var df = d.getFullYear() + '-' + ((mnth < 10) ? '0' + mnth : mnth) + '-' + ((d.getDate() < 10) ? '0' + d.getDate() : d.getDate()) + '_' + d.getHours() + '-' + d.getMinutes();
    var sn = '';
    var dn = '';

    // Loop over all cluster processes
    for (var i = 0; i < processes.length; i++) {
        sn = '';
        dn = '';
        var process = processes[i];
        // Get process type
        var ptype = clusterItems[process.getValue('processtype')];

        // Directories only for non-ndbapi commands
        if (ptype.getValue('name').toLowerCase() !== 'api') {
            // Get host
            var host = clusterItems[process.getValue('host')];
            var nodeid = process.getValue('NodeId'); // So we can form log file name.

            // Get datadir and dir separator
            var datadir = mcc.util.unixPath(
                mcc.util.terminatePath(
                    getEffectiveInstanceValue(process, 'DataDir')));
            var dirSep = mcc.util.dirSep(datadir);
            /*
            Form sourceName and destinationName
            If it's DNODE 1 then log file is ndb_1_out.log. If it's SQL
            node 54, then mysqld.54.err etc. DATADIR ends with /NodeID/
            ndb_49_cluster.log and ndb_49_out.log.
            */
            if (ptype.getValue('name').toLowerCase() === 'mysqld') {
                sn = 'mysqld.' + nodeid + '.err';
                dn = df + '_' + sn;
                fetchLogCommands.push({
                    cmd: 'getLogsReq',
                    host: host.getValue('name'),
                    sourcePath: datadir + 'data' + dirSep,
                    sourceName: sn,
                    destinationName: dn
                });
            } else if (ptype.getValue('name').toLowerCase() === 'ndb_mgmd') {
                // We have 2 files here.
                sn = 'ndb_' + nodeid + '_cluster.log';
                dn = df + '_' + sn;
                fetchLogCommands.push({
                    cmd: 'getLogsReq',
                    host: host.getValue('name'),
                    sourcePath: datadir,
                    sourceName: sn,
                    destinationName: dn
                });
                sn = 'ndb_' + nodeid + '_out.log';
                dn = df + '_' + sn;
                fetchLogCommands.push({
                    cmd: 'getLogsReq',
                    host: host.getValue('name'),
                    sourcePath: datadir,
                    sourceName: sn,
                    destinationName: dn
                });
            } else {
                // It's DNODE
                sn = 'ndb_' + nodeid + '_out.log';
                dn = df + '_' + sn;
                fetchLogCommands.push({
                    cmd: 'getLogsReq',
                    host: host.getValue('name'),
                    sourcePath: datadir,
                    sourceName: sn,
                    destinationName: dn
                });
            }
        }
    }
    return fetchLogCommands;
}

/**
 *Generate the proper array of install command for the hosts by process.
 *
 * @param {string} hostName host external IP address
 * @param {string} platform uname
 * @param {string} iType DOCKER/REPO
 * @param {string} flavor actual OS
 * @returns
 */
function getInstallCommands (hostName, platform, iType, flavor) {
    // Type of the process, hostName, platform, REPO/DOCKER, RPMYUM, DPKG...
    console.info('[INF]Host ' + hostName + ' getting install commands.');
    var installCommands = [];
    var what2;
    // Do verification for each host individually. Unused atm.
    if (iType.toUpperCase() === 'DOCKER') {
        what2 = mcc.userconfig.setCcfgPrGen.apply(this,
            mcc.userconfig.setMsgForGenPr('unsuppInstType', [iType.toUpperCase()]));
        if ((what2 || {}).text) { mcc.util.displayModal('I', 3, what2.text); }
        // DOCKER install
        console.info('[INF]Will use Docker for host ' + hostName + '.');
        return installCommands;
    } else {
        // REPO install
        // SQL node and wherever COMMUNITY is installed, requires EPEL!
        console.debug('[DBG]Will use REPO for host ' + hostName + '.');
        // platform, flavor, ver!
        if (platform.toUpperCase() === 'LINUX') {
            // No "includes" but "indexOf" to make IE11 happy...
            if (['ol', 'centos', 'rhel'].indexOf(flavor.toLowerCase()) >= 0) {
                console.debug('[DBG]INSTALL: Pushing RPM/YUM commands for host ' + hostName);
                // RPM/YUM
                console.debug('[DBG]\tPushing MGMD');
                var ss = new ProcessCommand(hostName, '', 'sudo');
                delete ss.msg.file.autoComplete; // Don't want ac for SUDO, RPM...
                ss.addopt('yum');
                ss.addopt('install');
                ss.addopt('-y');
                ss.addopt('mysql-cluster-community-management-server');
                ss.progTitle = 'Installing community MGMT server.';
                ss.msg.toRun = 'sudo yum install -y mysql-cluster-community-management-server';
                ss.msg.isCommand = true;
                installCommands.push(ss);

                var st = new ProcessCommand(hostName, '', 'sudo');
                delete st.msg.file.autoComplete;
                st.addopt('yum');
                st.addopt('install');
                st.addopt('-y');
                st.addopt('mysql-cluster-community-client');
                st.progTitle = 'Installing client tools along with MGMT server.';
                st.msg.toRun = 'sudo yum install -y mysql-cluster-community-client';
                st.msg.isCommand = true;
                installCommands.push(st);

                console.debug('[DBG]' + '\tPushing NDBD');
                var sa = new ProcessCommand(hostName, '', 'sudo');
                delete sa.msg.file.autoComplete;
                sa.addopt('yum');
                sa.addopt('install');
                sa.addopt('-y');
                sa.addopt('mysql-cluster-community-data-node');
                sa.progTitle = 'Installing community DNODE.';
                sa.msg.toRun = 'sudo yum install -y mysql-cluster-community-data-node';
                sa.msg.isCommand = true;
                installCommands.push(sa);

                console.debug('[DBG]' + '\tPushing MYSQLD');
                var sb = new ProcessCommand(hostName, '', 'sudo');
                delete sb.msg.file.autoComplete;
                sb.addopt('yum');
                sb.addopt('install');
                sb.addopt('-y');
                sb.addopt('mysql-cluster-community-server');
                sb.progTitle = 'Installing community MySQL server.';
                sb.msg.toRun = 'sudo yum install -y mysql-cluster-community-server';
                sb.msg.isCommand = true;
                installCommands.push(sb);

                return installCommands;
            } else { // flavor !== 'ol, centos, rhel'
                what2 = mcc.userconfig.setCcfgPrGen.apply(this,
                    mcc.userconfig.setMsgForGenPr('unsuppInstOS', [flavor.toUpperCase() || 'Unknown OS']));
                if ((what2 || {}).text) { mcc.util.displayModal('I', 3, what2.text); }
                if (['opensuse'].indexOf(flavor) >= 0) {
                    console.info('[INF]' + 'Repo RPM/ZYpp installation selected for host ' + hostName + '.');
                    return installCommands;
                } else {
                    if (['ubuntu', 'debian'].indexOf(flavor.toLowerCase()) >= 0) {
                        console.info('[INF]' + 'Repo DPKG/APT installation selected for host ' + hostName + '.');
                        return installCommands;
                    } else {
                        if (['fedora'].indexOf(flavor.toLowerCase()) >= 0) {
                            // RPM/DNF from 22 up
                            console.info('[INF]' + 'Repo RPM/DNF installation selected for host ' + hostName + '.');
                            return installCommands;
                        } else {
                            // Unknown Linux...
                            console.info('[INF]' + 'Repo installation on unknown OS selected for host ' + hostName + '.');
                            return installCommands;
                        }
                    }
                }
            }
        } else { // platform !== 'LINUX'
            what2 = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('unsuppInstPlat', [platform.toUpperCase() || 'Unknown platform']));
            if ((what2 || {}).text) { mcc.util.displayModal('I', 3, what2.text); }
            if (platform.toUpperCase() === 'WINDOWS') {
                console.info('[INF]' + 'Repo Windows installation selected for host ' + hostName + '.');
                return installCommands;
            } else {
                if (platform.toUpperCase() === 'DARWIN') {
                    console.info('[INF]' + 'Repo MacOSX installation selected for host ' + hostName + '.');
                    return installCommands;
                } else {
                    if (platform.toUpperCase() === 'SUNOS') {
                        console.info('[INF]' + 'Repo Solaris installation selected for host ' + hostName + '.');
                        return installCommands;
                    } else {
                        // Unknown platform!
                        console.info('[INF]' + 'Repo installation on unknown OS selected for host ' + hostName + '.');
                        return installCommands;
                    }
                }
            }
        }
    }
}

// Generate the (array of) directory creation commands for all processes
function getCreateCommands () {
    // Array to return
    var createDirCommands = [];
    // Loop over all cluster processes
    for (var i = 0; i < processes.length; i++) {
        var process = processes[i];
        // Get process type
        var ptype = clusterItems[process.getValue('processtype')];
        // Directories only for non-ndbapi commands
        if (ptype.getValue('name').toLowerCase() !== 'api') {
            // Get host
            var host = clusterItems[process.getValue('host')];
            // Get datadir and dir separator
            var datadir = mcc.util.unixPath(
                mcc.util.terminatePath(
                    getEffectiveInstanceValue(process, 'DataDir')));
            var dirSep = mcc.util.dirSep(datadir);

            // Push the create datadir command unless mysqld
            if (ptype.getValue('name').toLowerCase() !== 'mysqld') {
                createDirCommands.push({
                    host: host.getValue('name'),
                    path: datadir,
                    name: null
                });
            } else {
                // All mysqlds also need data/test, data/mysql and socket dir
                var socketDir = mcc.util.unixPath(
                    getEffectiveInstanceValue(process, 'Socket'));
                // There might be a bug in not using sockSep!
                // var sockSep = mcc.util.dirSep(socketDir);
                socketDir = socketDir.substring(0, socketDir.lastIndexOf(dirSep));
                createDirCommands.push({
                    host: host.getValue('name'),
                    path: socketDir,
                    name: null
                });
                createDirCommands.push({
                    host: host.getValue('name'),
                    path: datadir + 'test' + dirSep,
                    name: null
                });
                createDirCommands.push({
                    host: host.getValue('name'),
                    path: datadir + 'mysql' + dirSep,
                    name: null
                });
                createDirCommands.push({
                    host: host.getValue('name'),
                    path: datadir + 'tmp' + dirSep,
                    name: null
                });
            }
        }
    }
    return createDirCommands;
}

// ProcessCommand Constructor
function ProcessCommand (h, p, n) {
    // Command description for use in html or messages
    this.html = {
        host: h,
        path: p,
        name: n,
        optionString: '<tr><td><b>Options</b></td>'
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
            sep: ' ',
            param: []
        }
    };

    var firstOpt = true;
    this.addopt = function (n) {
        var pa = { name: n };
        var opt = n;
        if (arguments.length > 1) {
            pa.val = arguments[1];
            opt += '=' + arguments[1];
        }
        this.msg.params.param.push(pa);
        this.html.optionString += (!firstOpt ? '<tr><td></td>' : '') + '<td>' +
            opt.replace(/</g, '&lt').replace(/>/g, '&gt') + '</td>';
        firstOpt = false;
    };
    this.isDone = function () { return true; };
}

// Generate the (array of) initialize command(s) for the given process
function getInitProcessCommands (process) {
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var nodeid = process.getValue('NodeId');

    // Startup commands only for non-ndbapi commands
    if (ptype.toLowerCase() === 'api') { return null; }

    // Get host
    var hostItem = clusterItems[process.getValue('host')];
    var host = hostItem.getValue('name');
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));

    // Get datadir
    var datadir = mcc.util.unixPath(getEffectiveInstanceValue(process, 'DataDir'))
    datadir = datadir + 'data' + mcc.util.dirSep(datadir);
    var basedir = getEffectiveInstalldir(hostItem);
    var initComm = [];
    if (ptype.toLowerCase() === 'mysqld') {
        // Check if SQL node had been initialized.
        if (isFirstStart(nodeid)) {
            var fsc = new ProcessCommand(
                host,
                mcc.util.unixPath(basedir),
                'mysqld' + (isWin ? '.exe' : ''));
            fsc.addopt('--no-defaults');
            fsc.addopt('--datadir=' + datadir);
            fsc.addopt('--initialize-insecure');
            fsc.progTitle = 'Initializing (insecure) node ' + nodeid + ' (' + ptype + ')';
            fsc.Actual = 'mysqld' + (isWin ? '.exe' : '') + '  --no-defaults --datadir=' +
                datadir + '--initialize-insecure';
            console.debug('mysqld init: ' + fsc.Actual);
            fsc.msg.procCtrl.NodeId = nodeid;
            fsc.msg.procCtrl.daemonWait = 5;
            initComm.push(fsc);
        }
    }
    return initComm;
}
/**
 * Generate the (array of) service installation commands for the given process(es).
 * Call as: var cmds = _getClusterCommands(getInstallServiceCommands, ["management", "data","sql"]);
 *
 *
 * @param {JSON} process ProcessStore.Item
 * @returns {[{}]} Array of command objects to execute
 */
function getInstallServiceCommands (process) {
    // Return values from sc create serviceName: 0 - created, 1073 - already exist.
    // sc create is "stupid" and only checks servicename. Rest can be garbage pointing
    // nowhere. Also, SC returns immediately while our SW returns when done.
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var nodeid = process.getValue('NodeId');

    // Nothing for API family. Maybe in the future if anyone wants to install
    // their NDB-API SW as a Windows service.
    if (ptype.toLowerCase() === 'api') { return null; }

    // Get host
    var hostItem = clusterItems[process.getValue('host')];
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));
    // Services only on Windows.
    if (!isWin) { return null; }

    // Get datadir
    var datadir_ = getEffectiveInstanceValue(process, 'DataDir');
    var datadir = mcc.util.unixPath(datadir_);

    var host = hostItem.getValue('name');
    var basedir = getEffectiveInstalldir(hostItem); // Where the executable are.

    // Get connect string; IntIP and not hostname.
    var connectString = getConnectstring();
    var instServiceComm = [];
    var MgmdInfo = mcc.gui.getMgmtArrayInfo();
    var totalMgmds = mcc.gui.getMgmtArraySize();
    var port = '';
    var conStr2ndMGMTNode = '';

    // ProcessCommand(h, p, n), see constructor above.
    var sc = new ProcessCommand(
        host, mcc.util.unixPath(basedir), ptypeItem.getValue('name') + '.exe');
    sc.progTitle = 'Installing node ' + nodeid + ' (' + ptype + ') as service.';
    sc.Actual = ptypeItem.getValue('name') + '.exe';
    sc.msg.procCtrl.waitForCompletion = true;
    sc.msg.procCtrl.noRaise = 1;
    delete sc.msg.file.autoComplete;
    if (ptype.toLowerCase() === 'mysqld') {
        sc.addopt('--install-manual');
        sc.addopt('N' + nodeid);
        sc.Actual += ' --install-manual N' + nodeid;
        // For SQL node:
        sc.addopt('--defaults-file', datadir + 'my.cnf');
        sc.Actual += ' --defaults-file=' + datadir + 'my.cnf';
    } else {
        // Different for Cluster processes.
        sc.addopt('--install', 'N' + nodeid);
        sc.Actual += ' --install=N' + nodeid;
    }

    if (ptype.toLowerCase() === 'ndb_mgmd') {
        conStr2ndMGMTNode = '';
        // Separate 1st node from second.
        if (parseInt(nodeid) === parseInt(MgmdInfo[0].ID)) { // 49
            // If it's 1st start, we do not care much.
            sc.addopt('--config-dir', datadir);
            sc.Actual += ' --config-dir=' + datadir;
            sc.addopt('--config-file', datadir + 'config.ini');
            sc.Actual += ' --config-file=' + datadir + 'config.ini';
            // We now use --initial always since we do not know and can not really
            // tell if user deployed new config in between 2 starts. And,
            // since 1st mgmt node is started with --initial, according to
            // the manual, we need to start 2nd one with --initial too (see below).
            sc.addopt('--initial');
            sc.Actual += ' --initial';

            // This is always needed.
            sc.addopt('--ndb-nodeid', process.getValue('NodeId'));
            sc.Actual += ' --ndb-nodeid=' + process.getValue('NodeId');
            if (totalMgmds > 1) {
                // There can be as much as two management nodes in Cluster.
                var nonodes = MgmdInfo[1].ID;
                sc.addopt('--nowait-nodes', nonodes);
                sc.Actual += ' --nowait-nodes=' + nonodes;
            }
        } else {
            // 2nd management server, NodeID 50
            // We need conn-string just to 1st mgmt node

            // Always needed.
            sc.addopt('--ndb-nodeid', process.getValue('NodeId'));
            sc.Actual += ' --ndb-nodeid=' + process.getValue('NodeId');
            sc.addopt('--config-dir', datadir);
            sc.Actual += ' --config-dir=' + datadir;
            sc.addopt('--initial');
            sc.Actual += ' --initial';

            var mgmds = processTypeInstances('ndb_mgmd');
            // Loop over all ndb_mgmd processes to check where 50 connects to.
            // Or, where 49 really is.
            for (var i in mgmds) {
                if (parseInt(MgmdInfo[0].ID) === parseInt(mgmds[i].getValue('NodeId'))) {
                    port = getEffectiveInstanceValue(mgmds[i], 'Portnumber');
                    var h = '';
                    if (mcc.gui.getUseVPN()) {
                        // Use internal IP instead of name:
                        h = clusterItems[mgmds[i].getValue('host')].getValue('internalIP');
                    } else {
                        // No VPN chosen by user.
                        h = clusterItems[mgmds[i].getValue('host')].getValue('name');
                    }
                    conStr2ndMGMTNode = h + ':' + port;
                    break;
                }
            }
            sc.addopt('--ndb-connectstring', conStr2ndMGMTNode);
            sc.Actual += ' --ndb-connectstring=' + conStr2ndMGMTNode;
            // We have to assume config.ini has changed, no cache.
            sc.addopt('--config-file', datadir + 'config.ini');
            sc.Actual += ' --config-file=' + datadir + 'config.ini';
        }
    }

    if (ptype.toLowerCase() === 'ndbd' || ptype.toLowerCase() === 'ndbmtd') {
        sc.progTitle = 'Installing node ' + nodeid + ' (' + ptype +
            ') as service N' + nodeid;
        sc.msg.procCtrl.waitForCompletion = true;
        sc.addopt('--ndb-nodeid', nodeid);
        sc.Actual += ' --ndb-nodeid=' + nodeid;
        sc.addopt('--ndb-connectstring', connectString);
        sc.Actual += ' --ndb-connectstring=' + connectString;
    }
    instServiceComm.push(sc);
    return instServiceComm;
}
/**
 *Install Cluster processes as Windows services on Windows hosts. Executes commands
 * prepared in getInstallServiceCommands().
 *
 * @returns {dojo.Deferred}
 */
function installServices () {
    // Before Cluster services can be installed to start manually, we need 2nd
    // set of commands: sc config service_name start= demand.

    var waitCondition = new dojo.Deferred();
    var instServiceComm = _getClusterCommands(getInstallServiceCommands,
        ['management', 'data', 'sql']);
    // For these commands, isDone is Function, isCommand = False.
    var errorReplies = 0;
    var howMany = instServiceComm.length;
    /* For debugging purposes.
        for (var i in instServiceComm) {
        console.debug("[DBG]INIT cmd["+i+"] is " + instServiceComm[i].Actual);
    }*/
    howMany = instServiceComm.length;
    console.debug('[DBG]Total of ' + howMany + ' service install commands generated.');

    var reqHead;
    function onErr (errMsg, errReply) {
        // No need to display each failure.1073, was already installed, just log.
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }

        var index = -1;
        // Match rep.head.rSeq with instServiceComm[i].seq to determine which command returned!
        for (var i = 0; i < instServiceComm.length; i++) {
            if (parseInt(instServiceComm[i].seq) === parseInt(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }

        var ts = getTimeStamp();
        // console.debug('[DBG]ErrReply: ' + JSON.stringify(errReply));
        console.error(ts + '[ERR]DEPLOY ::FAIL::' + instServiceComm[index].Actual + ', ' +
            errMsg + '.');
        cmdLog.push(ts + 'DEPLOY ::FAIL::' + instServiceComm[index].Actual + ', ' +
            errMsg + '\n');
        --howMany;
        updateProgressAndInstNextSvc();
    }

    function onRep (rep) {
        var ro = '';
        var index = -1;
        // Match rep.head.rSeq with instServiceComm[i].seq to determine which command returned!
        for (var i = 0; i < instServiceComm.length; i++) {
            if (parseInt(instServiceComm[i].seq) === parseInt(rep.head.rSeq)) {
                index = i;
                break;
            }
        }
        ro = String(rep.body.out);
        if (ro.indexOf('errcode:') === 0) {
            onErr(ro, rep);
            return;
        } else {
            var ts = getTimeStamp();
            cmdLog.push(ts + 'DEPLOY ::SUCC::' + instServiceComm[index].Actual + '\n');
            console.debug(ts + '[INF]DEPLOY ::SUCC::' + instServiceComm[index].Actual + '.');
        }
        // Success, wait next reply.
        --howMany;
        updateProgressAndInstNextSvc();
    }

    function updateProgressAndInstNextSvc () {
        if (howMany <= 0) {
            var message;
            if (instServiceComm.length > 0) {
                message = errorReplies ? 'Installing Windows services has completed, but ' + errorReplies + ' out of ' +
                    instServiceComm.length + ' commands failed' : 'Installing Windows services procedure completed.';
            } else {
                message = 'No Windows services to install.';
                errorReplies = 0;
            }
            console.debug('[DBG]' + message);
            waitCondition.resolve(errorReplies <= 0);
        }
    }
    // Initiate start sequence by sending cmd to all Windows hosts. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    if (howMany > 0) {
        moveProgress('Deploying cluster',
            'Installing Windows services...');
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            instServiceComm[t].seq = reqHead.seq;
            mcc.server.doReq('executeCommandReq',
                { command: instServiceComm[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= instServiceComm.length) { t = -1; }
        } while (t >= 0);
    }
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndInstNextSvc');
    updateProgressAndInstNextSvc();
    return waitCondition;
}

/**
 *Generate the (array of) installed service modification commands (autoStart -> manual)
 * for the given process(es). To be called as:
 *  var cmds = _getClusterCommands(getModifyServiceCommands, ["management", "data","sql"]);
 *
 * @param {{}} process ProcessStore.Item
 * @returns {[{}]} Array of service modification commands to execute
 */
function getModifyServiceCommands (process) {
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var nodeid = process.getValue('NodeId');

    // Nothing for API family. Maybe in the future if anyone wants to install
    // their NDB-API SW as a Windows service.
    if (ptype.toLowerCase() === 'api') { return null; }
    // mysqld already understands --install-manual
    if (ptype.toLowerCase() === 'mysqld') { return null; }

    // Get host
    var hostItem = clusterItems[process.getValue('host')];
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));
    // Services only on Windows.
    if (!isWin) { return null; }

    var modifySvcComm = [];
    var host = hostItem.getValue('name');

    var sc = new ProcessCommand(
        host, 'C:\\windows\\system32\\', 'sc.exe');
    sc.progTitle = 'Modifying node ' + nodeid + ' (' + ptype + ') as manual start service.';
    sc.Actual = 'sc config N' + nodeid + ' start= demand';
    sc.msg.procCtrl.waitForCompletion = true;
    delete sc.msg.file.autoComplete;
    sc.addopt('config');
    sc.addopt('N' + nodeid);
    sc.addopt('start=');
    sc.addopt('demand');

    modifySvcComm.push(sc);
    return modifySvcComm;
}

/**
 *Executes commands obtained from getModifyServiceCommands() (modify installed Windows services
 *   so they do not start at reboot.
 *
 * @returns {dojo.Deferred}
 */
function modifyServices () {
    // 2nd set of commands to start Cluster processes manually on Windows.
    var waitCondition = new dojo.Deferred();
    var modifySvcComm = _getClusterCommands(getModifyServiceCommands,
        ['management', 'data', 'sql']);
    // For these commands, isDone is Function, isCommand = False.
    var errorReplies = 0;
    var howMany = modifySvcComm.length;
    console.debug('[DBG]Total of ' + howMany + ' installed services modify commands generated.');

    var reqHead;
    function onErr (errMsg, errReply) {
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }

        var index = -1;
        // Match rep.head.rSeq with modifySvcComm[i].seq to determine which command returned!
        for (var i = 0; i < modifySvcComm.length; i++) {
            if (parseInt(modifySvcComm[i].seq) === parseInt(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        // console.error('[DBG]ErrReply: ' + JSON.stringify(errReply));
        console.error(ts + '[ERR]DEPLOY ::FAIL::' + modifySvcComm[index].Actual + ', ' +
            errMsg + '.');
        cmdLog.push(ts + 'DEPLOY ::FAIL::' + modifySvcComm[index].Actual + ', ' +
            errMsg + '\n');
        --howMany;
        updateProgressAndModifyNextSvc();
    }

    function onRep (rep) {
        var ro = '';
        var index = -1;
        // Match rep.head.rSeq with modifySvcComm[i].seq to determine which command returned!
        for (var i = 0; i < modifySvcComm.length; i++) {
            if (parseInt(modifySvcComm[i].seq) === parseInt(rep.head.rSeq)) {
                index = i;
                break;
            }
        }
        ro = String(rep.body.out);
        if (ro.indexOf('errcode:') === 0) {
            onErr(ro, rep);
            return;
        } else {
            var ts = getTimeStamp();
            cmdLog.push(ts + 'DEPLOY ::SUCC::' + modifySvcComm[index].Actual + '\n');
            console.debug(ts + '[INF]DEPLOY ::SUCC::' + modifySvcComm[index].Actual + '.');
        }
        // success, wait next reply.
        --howMany;
        updateProgressAndModifyNextSvc();
    }

    function updateProgressAndModifyNextSvc () {
        if (howMany <= 0) {
            var message;
            if (modifySvcComm.length > 0) {
                message = errorReplies ? 'Modifying Windows services has completed, but ' + errorReplies + ' out of ' +
                    modifySvcComm.length + ' commands failed' : 'Modifying Windows services procedure completed.';
            } else {
                message = 'No Windows services to modify.';
                errorReplies = 0;
            }
            console.debug('[DBG]' + message);
            if (modifySvcComm.length > 0) {
                removeProgressDialog();
            }
            waitCondition.resolve(errorReplies <= 0);
            // return;
        }
    }
    // Initiate start sequence by sending cmd to all Windows hosts. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    if (howMany > 0) {
        moveProgress('Deploying cluster',
            'Modifying Windows services...');
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            modifySvcComm[t].seq = reqHead.seq;
            mcc.server.doReq('executeCommandReq',
                { command: modifySvcComm[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= modifySvcComm.length) { t = -1; }
        } while (t >= 0);
    }

    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndModifyNextSvc');
    updateProgressAndModifyNextSvc();
    return waitCondition;
}

/**
 *Generate the (array of) service removal commands for the given process(es).
 *Call as: var cmds = _getClusterCommands(getRemoveServiceCommands, ["management", "data","sql"]);
 *
 * @param {*} process
 * @returns {[]} Array of commands to execute
 */
function getRemoveServiceCommands (process) {
    // Return values from sc delete serviceName: 0 - deleted, 1060 - doesn't exist.
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var nodeid = process.getValue('NodeId');

    // Nothing for API family. Maybe in the future if anyone wants to install
    // their NDB-API SW as a Windows service.
    if (ptype.toLowerCase() === 'api') { return null; }

    // Get host
    var hostItem = clusterItems[process.getValue('host')];
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));
    // Services only on Windows.
    if (!isWin) { return null; }

    var host = hostItem.getValue('name');
    var sc = new ProcessCommand(host, 'C:\\windows\\system32', 'sc.exe');
    sc.progTitle = 'Deleting N' + nodeid + ' service.';
    sc.Actual = 'sc delete N' + nodeid;
    sc.msg.procCtrl.waitForCompletion = true;
    sc.msg.procCtrl.noRaise = 1060; // Makes no diff. if we deleted it or it doesn't exist.
    delete sc.msg.file.autoComplete;
    sc.addopt('delete');
    sc.addopt('N' + nodeid);
    // Output: [SC] DeleteService SUCCESS, 0 or >=1 for error
    return sc;
}

/**
 *Remove installed Windows services. Runs commands from _getClusterCommands(getRemoveServiceCommands,
        ['management', 'data', 'sql'])
 *
 * @returns {dojo.Deferred}
 */
function removeServices () {
    var waitCondition = new dojo.Deferred();
    var rmSvcComm = _getClusterCommands(getRemoveServiceCommands,
        ['management', 'data', 'sql']);
    // For these commands, isDone is Function, isCommand = False.
    var errorReplies = 0;
    var howMany = rmSvcComm.length;
    console.debug('[DBG]Total of ' + howMany + ' installed services remove commands generated.');
    var reqHead;
    function onErr (errMsg, errReply) {
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }

        var index = -1;
        // Match rep.head.rSeq with rmSvcComm[i].seq to determine which command returned!
        for (var i = 0; i < rmSvcComm.length; i++) {
            if (parseInt(rmSvcComm[i].seq) === parseInt(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        console.error('[ERR]ErrReply: ' + JSON.stringify(errReply));
        console.error(ts + '[ERR]STOP   ::FAIL::' + rmSvcComm[index].Actual + ', ' +
            errMsg + '.');
        cmdLog.push(ts + 'STOP   ::FAIL::' + rmSvcComm[index].Actual + ', ' +
            errMsg + '\n');
        --howMany;
        updateProgressAndRemoveNextSvc();
    }

    function onRep (rep) {
        var ro = '';
        var index = -1;
        // console.debug("rep is " + JSON.stringify(rep));
        // Match rep.head.rSeq with rmSvcComm[i].seq to determine which command returned!
        for (var i = 0; i < rmSvcComm.length; i++) {
            if (parseInt(rmSvcComm[i].seq) === parseInt(rep.head.rSeq)) {
                index = i;
                break;
            }
        }
        ro = String(rep.body.out);
        if (ro.indexOf('errcode:') === 0) {
            onErr(ro, rep);
            return;
        } else {
            var ts = getTimeStamp();
            cmdLog.push(ts + 'STOP   ::SUCC::' + rmSvcComm[index].Actual + '\n');
            console.info(ts + '[INF]STOP   ::SUCC::' + rmSvcComm[index].Actual + '.');
        }
        // success, wait next reply.
        --howMany;
        updateProgressAndRemoveNextSvc();
    }

    function updateProgressAndRemoveNextSvc () {
        if (howMany <= 0) {
            var message;
            if (rmSvcComm.length > 0) {
                message = errorReplies ? 'Removing Windows services has completed, but ' +
                    errorReplies + ' out of ' + rmSvcComm.length +
                    ' commands failed' : 'Removing Windows services procedure completed.';
            } else {
                message = 'No Windows services to remove.';
                errorReplies = 0;
            }
            console.debug('[DBG]' + message);
            if (rmSvcComm.length > 0) {
                removeProgressDialog();
            }
            waitCondition.resolve(errorReplies <= 0);
        }
    }
    // Initiate start sequence by sending cmd to all Windows hosts. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    if (howMany > 0) {
        moveProgress('Stopping cluster',
            'Removing Windows services...');
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            rmSvcComm[t].seq = reqHead.seq;
            mcc.server.doReq('executeCommandReq',
                { command: rmSvcComm[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= rmSvcComm.length) { t = -1; }
        } while (t >= 0);
    }
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndRemoveNextSvc');
    updateProgressAndRemoveNextSvc();
    return waitCondition;
}
/**
 *Shows commands which will be used to start the process when user selects it in process-tree.
 *
 * @param {{}} process ProcessStore.Item
 * @returns {[]} Array of commands or NULL in case of error
 */
function listStartProcessCommands (process) {
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var nodeid = process.getValue('NodeId');
    var port = '';

    var totalMgmds = mcc.gui.getMgmtArraySize();
    var MgmdInfo = mcc.gui.getMgmtArrayInfo();
    var conStr2ndMGMTNode = '';

    // Startup commands only for non-ndbapi commands
    if (ptype.toLowerCase() === 'api') { return null; }

    // Get host
    var hostItem = clusterItems[process.getValue('host')];
    var host = hostItem.getValue('name');
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));

    // Get datadir
    var datadir_ = mcc.util.terminatePath(getEffectiveInstanceValue(process, 'DataDir'));
    var datadir = mcc.util.unixPath(datadir_);
    var basedir = getEffectiveInstalldir(hostItem);

    // Get connect string; IntIP and not hostname.
    var connectString = getConnectstring();
    var sc = new ProcessCommand(
        host,
        mcc.util.unixPath(basedir), // Always use unix basedir unless running under cmd.exe
        ptypeItem.getValue('name') + (isWin ? '.exe' : ''));

    // SC process commands were used to install Windows services. Not needed any more.
    var sComm = (isWin ? [] : [sc]);
    // Add process specific options
    if (ptype.toLowerCase() === 'ndb_mgmd') {
        conStr2ndMGMTNode = '';
        if (isWin) {
            var ss = new ProcessCommand(host, '', 'net');
            // Windows: Don't want ac for net cmd (it's in the path).
            delete ss.msg.file.autoComplete;
            ss.addopt('start');
            ss.addopt('N' + nodeid);
            sComm.push(ss);
        } else {
            // Separate 1st node from second.
            if (parseInt(nodeid) === parseInt(MgmdInfo[0].ID)) { // 49
                sc.addopt('--config-dir', mcc.util.unixPath(datadir));
                sc.addopt('--config-file', mcc.util.unixPath(datadir) + 'config.ini');
                sc.addopt('--initial');
                // This is always needed.
                sc.addopt('--ndb-nodeid', process.getValue('NodeId'));
                if (totalMgmds > 1) {
                    var nonodes = MgmdInfo[1].ID;
                    sc.addopt('--nowait-nodes', nonodes);
                }
            } else { // 50
                sc.addopt('--ndb-nodeid', process.getValue('NodeId'));
                sc.addopt('--config-dir', mcc.util.unixPath(datadir));
                sc.addopt('--initial');
                var mgmds = processTypeInstances('ndb_mgmd');
                // Loop over all ndb_mgmd processes
                for (var i in mgmds) {
                    if (parseInt(MgmdInfo[0].ID) === parseInt(mgmds[i].getValue('NodeId'))) {
                        port = getEffectiveInstanceValue(mgmds[i], 'Portnumber');
                        var h = '';
                        if (mcc.gui.getUseVPN()) {
                            // Use internal IP instead of name:
                            h = clusterItems[mgmds[i].getValue('host')].getValue('internalIP');
                        } else {
                            h = clusterItems[mgmds[i].getValue('host')].getValue('name');
                        }
                        conStr2ndMGMTNode = h + ':' + port;
                        break;
                    }
                }
                sc.addopt('--ndb-connectstring', conStr2ndMGMTNode);
                sc.addopt('--config-file', mcc.util.unixPath(datadir) + 'config.ini');
            }
        }// !isWin ndb_mgmd
        return sComm;
    }

    if (ptype.toLowerCase() === 'ndbd' || ptype.toLowerCase() === 'ndbmtd') {
        if (isWin) {
            var ssn = new ProcessCommand(host, '', 'net');
            delete ssn.msg.file.autoComplete; // Don't want ac for net cmd
            ssn.addopt('start');
            ssn.addopt('N' + nodeid);
            sComm.push(ssn);
        } else {
            sc.addopt('--ndb-nodeid', nodeid);
            sc.addopt('--ndb-connectstring', connectString);
        }
        return sComm;
    }

    if (ptype.toLowerCase() === 'mysqld') {
        if (isWin) {
            var ssd = new ProcessCommand(host, '', 'net');
            delete ssd.msg.file.autoComplete; // Don't want ac for net cmd
            ssd.addopt('start');
            ssd.addopt('N' + nodeid);
            sComm.push(ssd);
        } else {
            sc.addopt('--defaults-file', datadir + 'my.cnf');
            // Adding extra parameter ('&', '-D'...) breaks BE...
        }
        return sComm;
    }
    mcc.util.displayModal('I', 0,
        '<span style="font-size:140%;color:red">listStartProcessCommands, not supposed to happen!</span>');
    return null;
}

/**
 *Generate the (array of) startup command(s) for the given process.
 *
 * @param {{}} process ProcessStore.Item
 * @returns {[]} of start commands or NULL if error
 */
function getStartProcessCommands (process) {
    // Get process type
    /*  This is pre-set in store. ptype points to NAME member.
        "id": 0,
        "name": "ndb_mgmd",
        "family": "management",
        "id": 1,
        "name": "ndbd",
        "family": "data",
        "id": 2,
        "name": "ndbmtd",
        "family": "data",
        "id": 3,
        "name": "mysqld",
        "family": "sql",
    */
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var nodeid = process.getValue('NodeId');
    var port = '';

    var totalMgmds = mcc.gui.getMgmtArraySize();
    var MgmdInfo = mcc.gui.getMgmtArrayInfo();
    var conStr2ndMGMTNode = '';

    // Startup commands only for non-ndbapi commands
    if (ptype.toLowerCase() === 'api') { return null; }
    var processName = process.getValue('name');
    // Determine IF node is running...
    var processRunning = clServStatus();
    if (determineProcessRunning(processRunning, processName)) {
        // Already started, maybe from outside.
        return null;
    }

    // Get host
    var hostItem = clusterItems[process.getValue('host')];
    var host = hostItem.getValue('name');
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));

    // Get datadir
    var datadir_ = mcc.util.terminatePath(getEffectiveInstanceValue(process, 'DataDir'));
    var datadir = mcc.util.unixPath(datadir_);
    var basedir = getEffectiveInstalldir(hostItem);

    // Get connect string; IntIP and not hostname.
    var connectString = getConnectstring();

    var sc = new ProcessCommand(
        host,
        mcc.util.unixPath(basedir), // Always use unix basedir unless running under cmd.exe
        ptypeItem.getValue('name') + (isWin ? '.exe' : ''));
    sc.progTitle = 'Starting node ' + nodeid + ' (' + ptype + ')';
    sc.Actual = ptypeItem.getValue('name') + (isWin ? '.exe' : ''); // Just init.

    // SC process commands were used to install Windows services. Not needed any more.
    var startComm = (isWin ? [] : [sc]);
    // Add process specific options
    if (ptype.toLowerCase() === 'ndb_mgmd') {
        conStr2ndMGMTNode = '';
        var isDoneFunction = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'CONNECTED'; };
        if (isWin) {
            var ss = new ProcessCommand(host, '', 'net');
            // Don't want ac for net cmd (it's in the path).
            delete ss.msg.file.autoComplete;
            ss.addopt('start');
            ss.addopt('N' + nodeid);
            ss.progTitle = 'Starting service N' + nodeid;
            ss.isDone = isDoneFunction;
            ss.Actual = 'net start N' + nodeid;
            startComm.push(ss);
        } else {
            sc.isDone = isDoneFunction;
            // Separate 1st node from second.
            if (parseInt(nodeid) === parseInt(MgmdInfo[0].ID)) { // 49
                sc.addopt('--config-file', mcc.util.unixPath(datadir) + 'config.ini');
                sc.Actual += ' --config-file=' + mcc.util.unixPath(datadir) + 'config.ini';
                // We assume config.ini is always new; either 1st or modified.
                sc.addopt('--initial');
                sc.Actual += ' --initial';
                // This is always needed.
                sc.addopt('--ndb-nodeid', process.getValue('NodeId'));
                sc.Actual += ' --ndb-nodeid=' + process.getValue('NodeId');
                sc.addopt('--config-dir', mcc.util.unixPath(datadir));
                sc.Actual += ' --config-dir=' + mcc.util.unixPath(datadir);
                if (totalMgmds > 1) {
                    console.debug('[DBG]MGMT process 49, more MGMT nodes, adding --no-wait.');
                    var nonodes = MgmdInfo[1].ID; // String(MgmdInfo[1].ID) would be better?
                    sc.addopt('--nowait-nodes', nonodes);
                    sc.Actual += ' --nowait-nodes=' + nonodes;
                }
            } else { // 50
                // This is for sure 2nd MGMT node so I need conn-string just to
                // 1st mgmt node
                console.debug('[DBG]MGMT process 50.');
                // Always needed.
                sc.addopt('--ndb-nodeid', process.getValue('NodeId'));
                sc.Actual += ' --ndb-nodeid=' + process.getValue('NodeId');
                sc.addopt('--config-dir', mcc.util.unixPath(datadir));
                sc.Actual += ' --config-dir=' + mcc.util.unixPath(datadir);
                sc.addopt('--initial');
                sc.Actual += ' --initial';

                var mgmds = processTypeInstances('ndb_mgmd');
                // Loop over all ndb_mgmd processes
                for (var i in mgmds) {
                    if (parseInt(MgmdInfo[0].ID) === parseInt(mgmds[i].getValue('NodeId'))) {
                        port = getEffectiveInstanceValue(mgmds[i], 'Portnumber');
                        var h = '';
                        if (mcc.gui.getUseVPN()) {
                            // Use internal IP instead of name:
                            h = clusterItems[mgmds[i].getValue('host')].getValue('internalIP');
                        } else {
                            h = clusterItems[mgmds[i].getValue('host')].getValue('name');
                        }
                        conStr2ndMGMTNode = h + ':' + port;
                        break;
                    }
                }
                console.debug('[DBG]' + ' constr_2ndMGMTN: ' + conStr2ndMGMTNode);
                sc.addopt('--ndb-connectstring', conStr2ndMGMTNode);
                sc.Actual += ' --ndb-connectstring=' + conStr2ndMGMTNode;
                // Load new config.ini
                sc.addopt('--config-file', mcc.util.unixPath(datadir) + 'config.ini');
                sc.Actual += ' --config-file=' + mcc.util.unixPath(datadir) + 'config.ini';
            }
        }// !isWin ndb_mgmd
        return startComm;
    }

    if (ptype.toLowerCase() === 'ndbd' || ptype.toLowerCase() === 'ndbmtd') {
        var isDoneFunct = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'STARTED'; };
        if (isWin) {
            var ssn = new ProcessCommand(host, '', 'net');
            delete ssn.msg.file.autoComplete; // Don't want ac for net cmd
            ssn.addopt('start');
            ssn.addopt('N' + nodeid);
            ssn.progTitle = 'Starting service N' + nodeid;
            ssn.Actual = 'net start N' + nodeid;
            ssn.isDone = isDoneFunct;
            ssn.msg.procCtrl.NodeId = nodeid;
            startComm.push(ssn);
        } else {
            sc.isDone = isDoneFunct;
            sc.addopt('--ndb-nodeid', nodeid);
            sc.Actual += ' --ndb-nodeid=' + nodeid;
            sc.addopt('--ndb-connectstring', connectString);
            sc.Actual += ' --ndb-connectstring=' + connectString;
            sc.msg.procCtrl.NodeId = nodeid;
        }
        return startComm;
    }

    if (ptype.toLowerCase() === 'mysqld') {
        var isDoneFunc = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'CONNECTED'; };
        if (isWin) {
            var ssd = new ProcessCommand(host, '', 'net');
            delete ssd.msg.file.autoComplete; // Don't want ac for net cmd
            ssd.addopt('start');
            ssd.addopt('N' + nodeid);
            ssd.progTitle = 'Starting service N' + nodeid;
            ssd.Actual = 'net start N' + nodeid;
            ssd.isDone = isDoneFunc;
            ssd.msg.procCtrl.NodeId = nodeid;
            startComm.push(ssd);
        } else {
            sc.addopt('--defaults-file', datadir + 'my.cnf');
            // Invoking mysqld does not return
            sc.msg.procCtrl.waitForCompletion = false; // isDone will tell us.
            // above would hang forever if mysqld can not be started. The guard is to stop
            // START procedure by closing the progress window and checking the logs.
            // Use nohup on it so python server won't hang in shutdown
            sc.msg.procCtrl.nohup = true;
            sc.msg.procCtrl.getStd = false;
            sc.progTitle = 'Starting node ' + nodeid + ' (' + ptype + ')';
            sc.Actual += ' --defaults-file=' + datadir + 'my.cnf';
            // Adding extra parameters ('&', '-D'...) breaks BE.
            sc.msg.procCtrl.NodeId = nodeid;
            sc.isDone = isDoneFunc;
        }
        return startComm;
    }
    mcc.util.displayModal('I', 0,
        '<span style="font-size:140%;color:red">getStartProcessCommands, not supposed to happen!</span>');
    return null;
}
/**
 *Generate stop command object for the given process.
 *
 * @param {{}} process ProcessStore.Item
 * @param {Boolean} force force creation of Stop commands even if process appears to be down
 *
 * @returns {{}} JSON object representing command to stop the process
 */
function getStopProcessCommands (process, force) {
    console.debug('StopProcess ' + process.getValue('name'));
    var nodeid = process.getValue('NodeId');
    var processName = process.getValue('name');
    var hostItem = clusterItems[process.getValue('host')];
    var host = hostItem.getValue('name');
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));

    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');

    var basedir = mcc.util.unixPath(getEffectiveInstalldir(hostItem));
    var port = '1186';

    // Get datadir if we need to read PID file and kill process forcibly.
    var datadir_ = mcc.util.terminatePath(getEffectiveInstanceValue(process, 'DataDir'));
    var datadir = mcc.util.unixPath(datadir_);

    var MgmdInfo = mcc.gui.getMgmtArrayInfo();
    var actual = '';
    var stopCommands = [];

    // Determine IF node is running...
    var processRunning = clServStatus();
    if (!determineProcessRunning(processRunning, processName) && !force) {
        // Already stopped. Also, here we assume there is no Windows Service either
        // which might not be true.
        return stopCommands; // Empty here.
    }
    var sc;
    var instDir = '';
    var fake1 = '';
    var fake2 = '';
    // It only causes problems if we try and stop multiple MGMT servers.
    if (ptype.toLowerCase() === 'ndb_mgmd' && parseInt(nodeid) === parseInt(MgmdInfo[0].ID)) {
        sc = new ProcessCommand(host, basedir, 'ndb_mgm' + (isWin ? '.exe' : ''));
        // no --ndb-connectstring as nodes are stopping and thus it can point to stopped node
        sc.addopt('localhost');
        port = String(getPort(nodeid));
        sc.addopt(port);
        if (debugKillPath) {
            sc.addopt('--execute', 'dwnsht'); // Testing, get to KILL path.
        } else {
            sc.addopt('--execute', 'shutdown');
        }
        sc.addopt('--connect-retries', '2');
        sc.progTitle = 'Running ndb_mgm -e shutdown to take down cluster';
        actual = 'ndb_mgm localhost ' + port + ' --execute=shutdown --connect-retries=2';
        sc.Actual = actual;
        sc.msg.procCtrl.NodeId = nodeid;
        sc.msg.procCtrl.daemonWait = 8; // Estimated 8s to wait for shutdown command to return.
        sc.msg.procCtrl.kill = true;
        sc.isDone = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'UNKNOWN'; };

        // This is CLIENT command thus probably in /usr/bin and not /usr/sbin
        // (default install directory) so we have to use autocomplete.
        if (!isWin) {
            instDir = String(sc.msg.file.path);
            // NO path prefix!
            sc.msg.file.path = '';
            // Look for ndb_mgm in InstallDirectory, /usr/bin' and in PATH.
            // When properly installed, ndb_mgm will be in /usr/bin.
            // Being that there are configurations which do not point to
            // binaries in InstallDir but rather one step up, I'm adding BIN and SBIN too.
            fake1 = '';
            fake2 = '';
            if (instDir.slice(-1) === '/') {
                fake1 = instDir + 'bin';
                fake2 = instDir + 'sbin';
            } else {
                fake1 = instDir + '/bin';
                fake2 = instDir + '/sbin';
            }
            sc.msg.file.autoComplete = [instDir, fake1, fake2, '/usr/bin/', ''];
            sc.msg.procCtrl.getPID = 'cat ' + datadir + '*.pid';
        } else {
            sc.msg.procCtrl.servicename = 'N' + nodeid;
            sc.msg.procCtrl.getPID = 'C:/Windows/system32/sc.exe queryex N' + nodeid;
            // Sending full parse line ( + ' | FIND "PID"';) kills Python:
        }
        stopCommands.push(sc);
    }
    // if 1st MGMT node is down and we received "force" we should try with 2nd too
    if (ptype.toLowerCase() === 'ndb_mgmd' && mcc.gui.getMgmtArraySize() > 1 &&
        parseInt(nodeid) === parseInt(MgmdInfo[1].ID) && force) {
        sc = new ProcessCommand(host, basedir, 'ndb_mgm' + (isWin ? '.exe' : ''));
        // no --ndb-connectstring as nodes are stopping and thus it can point to stopped node
        sc.addopt('localhost');
        port = String(getPort(nodeid));
        sc.addopt(port);
        if (debugKillPath) {
            sc.addopt('--execute', 'dwnsht'); // Testing, get to KILL path.
        } else {
            sc.addopt('--execute', 'shutdown');
        }
        sc.addopt('--connect-retries', '2');
        sc.progTitle = 'Running ndb_mgm -e shutdown to take down cluster, 2nd management node';
        actual = 'ndb_mgm localhost ' + port + ' --execute=shutdown --connect-retries=2';
        sc.Actual = actual;
        sc.msg.procCtrl.NodeId = nodeid;
        sc.msg.procCtrl.daemonWait = 8; // Estimated 8s to wait for shutdown command to return.
        sc.msg.procCtrl.kill = true;
        sc.isDone = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'UNKNOWN'; };

        // This is CLIENT command thus probably in /usr/bin and not /usr/sbin
        // (default install directory) so we have to use autocomplete.
        if (!isWin) {
            instDir = String(sc.msg.file.path);
            // NO path prefix!
            sc.msg.file.path = '';
            // Look for ndb_mgm in InstallDirectory, /usr/bin' and in PATH.
            // When properly installed, ndb_mgm will be in /usr/bin.
            // Being that there are configurations which do not point to
            // binaries in InstallDir but rather one step up, I'm adding BIN and SBIN too.
            fake1 = '';
            fake2 = '';
            if (instDir.slice(-1) === '/') {
                fake1 = instDir + 'bin';
                fake2 = instDir + 'sbin';
            } else {
                fake1 = instDir + '/bin';
                fake2 = instDir + '/sbin';
            }
            sc.msg.file.autoComplete = [instDir, fake1, fake2, '/usr/bin/', ''];
            sc.msg.procCtrl.getPID = 'cat ' + datadir + '*.pid';
        } else {
            sc.msg.procCtrl.servicename = 'N' + nodeid;
            sc.msg.procCtrl.getPID = 'C:/Windows/system32/sc.exe queryex N' + nodeid;
            // Sending full parse line ( + ' | FIND "PID"';) kills Python:
        }
        stopCommands.push(sc);
    }

    if (ptype.toLowerCase() === 'mysqld') {
        if (!isWin) {
            var scd = new ProcessCommand(host, basedir, 'mysqladmin');
            scd.addopt('--port', getEffectiveInstanceValue(process, 'Port'));
            scd.addopt('--user', 'root');
            if (debugKillPath) {
                scd.addopt('shtdwn'); // Testing, get to KILL path.
            } else {
                scd.addopt('shutdown');
            }
            scd.addopt('--socket', mcc.util.quotePath(
                getEffectiveInstanceValue(process, 'Socket')));
            scd.progTitle = 'mysqladmin shutdown on node ' + nodeid;
            scd.msg.procCtrl.NodeId = nodeid;
            actual = 'mysqladmin --port=' + getEffectiveInstanceValue(process, 'Port') +
                ' --user=root shutdown --socket=' + getEffectiveInstanceValue(process, 'Socket');
            scd.Actual = actual;
            scd.msg.procCtrl.kill = true;
            scd.msg.procCtrl.getPID = 'cat ' + datadir + 'data/*.pid';
            scd.msg.procCtrl.daemonWait = 3;

            // This is CLIENT command thus probably in /usr/bin and not /usr/sbin
            // (default install directory) so we have to use autocomplete.
            var instDirD = String(scd.msg.file.path);
            // NO path prefix!
            scd.msg.file.path = '';
            // Look for mysqladmin in InstallDirectory, /usr/bin' and in PATH.
            // When properly installed, mysqladmin will be in /usr/bin.
            // Being that there are configurations which do not point to
            // binaries in InstallDir but rather one step up, I'm adding BIN and SBIN too.
            var fake1d = '';
            var fake2d = '';
            if (instDirD.slice(-1) === '/') {
                fake1d = instDirD + 'bin';
                fake2d = instDirD + 'sbin';
            } else {
                fake1d = instDirD + '/bin';
                fake2d = instDirD + '/sbin';
            }
            scd.msg.file.autoComplete = [instDirD, fake1d, fake2d, '/usr/bin/', ''];
            scd.isDone = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'NO_CONTACT'; };
            stopCommands.push(scd);
        } else {
            var ssc = new ProcessCommand(host, '', 'net');
            delete ssc.msg.file.autoComplete; // Don't want ac for net cmd
            ssc.addopt('stop');
            if (debugKillPath) {
                ssc.addopt('NMNMNMNMN' + nodeid); // Testing, get to KILL path.
            } else {
                ssc.addopt('N' + nodeid);
            }
            ssc.progTitle = 'Stopping (' + ptype + ') service N' + nodeid;
            actual = 'net stop N' + nodeid;
            ssc.Actual = actual;
            ssc.isDone = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'NO_CONTACT'; };
            // There is no single command to get PID and kill it on Windows so this
            // is in request_handler.py::385; Check if it's NET STOP!
            // Actually, there is a single command but that requires shell evaluation in
            // back end which is deemed high security risk.
            ssc.msg.procCtrl.kill = true;
            ssc.msg.procCtrl.daemonWait = 5;
            ssc.msg.procCtrl.nodeid = nodeid;
            ssc.msg.procCtrl.servicename = 'N' + nodeid;
            ssc.msg.procCtrl.getPID = 'C:/Windows/System32/sc.exe queryex N' + nodeid;
            // + ' | FIND "PID"'; << This requires shell expansion.

            stopCommands.push(ssc); // 2 -> already stopped
        }// Win
    }// ptype == "mysqld"
    return stopCommands;
}

/**
 *Generate array of KILL process commands in case ndb_mgm fails to stop Cluster.
  We also need to stop extra ndb_mgmd (if there) and ndbmtd processes.
 *Primary ndb_mgmd(49) and all of mysqld processes already issue kill upon failure
to stop by regular means.
 *
 * @param {{}} process ProcessStore.Item
 * @returns {[{}]} commands to kill stray Cluster processes
 */
function getKillProcessCommands (process) {
    var nodeid = process.getValue('NodeId');
    var stopCommands = [];
    var ptypeItem = clusterItems[process.getValue('processtype')];
    var ptype = ptypeItem.getValue('name');
    var MgmdInfo = mcc.gui.getMgmtArrayInfo();

    // if (ptype.toLowerCase() === 'ndb_mgmd' && parseInt(nodeid) === parseInt(MgmdInfo[0].ID)) {
    //     return stopCommands; // empty here since 1st MGMT node carries its own kill with command block
    // }

    var hostItem = clusterItems[process.getValue('host')];
    var host = hostItem.getValue('name');
    var isWin = mcc.util.isWin(hostItem.getValue('uname'));

    var basedir = mcc.util.unixPath(getEffectiveInstalldir(hostItem));
    var port = '1186';

    // Get datadir if we need to read PID file and kill process forcibly.
    var datadir_ = mcc.util.terminatePath(getEffectiveInstanceValue(process, 'DataDir'));
    var datadir = mcc.util.unixPath(datadir_);

    // We already tried to stop primary MGMT process so just check for subsequent
    // ndb_mgmd's, if any, in an effort of normal shutdown.
    if (ptype.toLowerCase() === 'ndb_mgmd' && parseInt(nodeid) !== parseInt(MgmdInfo[0].ID)) {
        var sc = new ProcessCommand(host, basedir, 'ndb_mgm' + (isWin ? '.exe' : ''));
        sc.addopt('localhost');
        port = String(getPort(nodeid));
        sc.addopt(port);
        if (debugKillPath) {
            sc.addopt('--execute', 'shtdwn'); // Testing, get to KILL path on node 50.
        } else {
            sc.addopt('--execute', 'shutdown');
        }
        sc.addopt('--connect-retries', '2');
        sc.progTitle = 'Running ndb_mgm -e shutdown to take down cluster on 2nd MGMT node.';
        sc.Actual = 'ndb_mgm localhost ' + port + ' --execute=shutdown --connect-retries=2';
        sc.msg.procCtrl.NodeId = nodeid;
        sc.msg.procCtrl.daemonWait = 4;
        sc.isDone = function () { return mcc.gui.getStatii(nodeid).toUpperCase() === 'UNKNOWN'; };
        // This is CLIENT command thus probably in /usr/bin and not /usr/sbin
        // (default install directory) so we have to use autocomplete.
        if (!isWin) {
            var instDir = String(sc.msg.file.path);
            // NO path prefix!
            sc.msg.file.path = '';
            // Look for ndb_mgm in InstallDirectory, /usr/bin' and in PATH.
            // When properly installed, ndb_mgm will be in /usr/bin.
            // Being that there are configurations which do not point to
            // binaries in InstallDir but rather one step up, I'm adding BIN and SBIN too.
            var fake1 = '';
            var fake2 = '';
            if (instDir.slice(-1) === '/') {
                fake1 = instDir + 'bin';
                fake2 = instDir + 'sbin';
            } else {
                fake1 = instDir + '/bin';
                fake2 = instDir + '/sbin';
            }
            sc.msg.file.autoComplete = [instDir, fake1, fake2, '/usr/bin/', ''];
            delete sc.isDone;
            sc.isDone = false;
            sc.msg.procCtrl.kill = true;
            sc.msg.procCtrl.getPID = 'cat ' + datadir + '*.pid';
        } else {
            delete sc.isDone;
            sc.isDone = false;
            sc.msg.procCtrl.kill = true;
            sc.msg.procCtrl.servicename = 'N' + nodeid;
            sc.msg.procCtrl.getPID = 'C:/Windows/system32/sc.exe queryex N' + nodeid;
        }
        stopCommands.push(sc);
    }

    if (ptype.toLowerCase() === 'ndbd' || ptype.toLowerCase() === 'ndbmtd') {
        // KILL data node processes. This executes only if all ndb_mgm report FAIL
        // and we were forced to kill ndb_mgmd process(es).
        var ssc;

        // Linux has just KILL - PID. Windows have NET STOP then TASKKILL /PID.
        // Most of the logic is in back end.
        if (isWin) {
            ssc = new ProcessCommand(host, '', 'net');
            delete ssc.msg.file.autoComplete; // Don't want ac for net cmd
            ssc.addopt('stop');
            if (debugKillPath) {
                ssc.addopt('NMNMNMNMN' + nodeid); // Testing, get to KILL path.
            } else {
                ssc.addopt('N' + nodeid);
            }
            ssc.progTitle = 'Stopping (' + ptype + ') service N' + nodeid;
            ssc.Actual = 'net stop N' + nodeid;
            delete ssc.isDone;
            ssc.isDone = false;
            // There is no single command to get PID and kill it on Windows so this
            // is in request_handler.py::385; Check if it's NET STOP!
            ssc.msg.procCtrl.kill = true;
            ssc.msg.procCtrl.daemonWait = 2;
            ssc.msg.procCtrl.nodeid = nodeid;
            ssc.msg.procCtrl.servicename = 'N' + nodeid;
            ssc.msg.procCtrl.getPID = 'C:/Windows/system32/sc.exe queryex N' + nodeid;
            ssc.msg.procCtrl.getPIDchild = 'wmic process where (ParentProcessId=' + ') get ProcessId';
        } else {
            // Full cmd line with pipes et all fails in BE on subprocess.check_call
            // (needs shell=True which is not safe to use).
            // Command 'DNODE' signals back end to specially process it.
            ssc = new ProcessCommand(host, '', 'DNODE');
            ssc.progTitle = 'Killing (' + ptype + ') process with Id' + nodeid;
            ssc.Actual = 'kill -9 $(cat ' + datadir + '*.pid)';
            ssc.msg.procCtrl.getPID = 'cat ' + datadir + '*.pid';
            // | grep 'Angel pid:' | tail -n1 | cut -d ':' -f5"; will kill Python off...
            ssc.msg.procCtrl.getPIDchild = 'cat ' + datadir + '*out.log';
            ssc.msg.procCtrl.kill = true;
            delete ssc.isDone;
            // There is no DNODE binary anywhere, it's just signal for
            // request_handler::start_process to do a kill.
            delete ssc.msg.file.autoComplete;
            ssc.isDone = false;
        }
        stopCommands.push(ssc);
    }
    return stopCommands;
}

/**
 *Array of commands for all families. Aggregation of Start/Stop commands per process.
 *
 * @param {String} procCommandFunc Which command array to generate (Init, Start, Stop...)
 * @param {String} families which families we generate commands for (any/all mgmt, data, sql)
 * @param {Boolean} force If force is passed(for StopCommands), generate them even if processes
 * might seem down.
 *
 * @returns {[{}]} objects representing commands to run on hosts
 */
function _getClusterCommands (procCommandFunc, families, force) {
    var procItems = getProcessItemsByFamilies(families);
    var commands = [];
    var cmd;
    for (var pix in procItems) {
        if (force) {
            cmd = procCommandFunc(procItems[pix], force);
        } else {
            cmd = procCommandFunc(procItems[pix]);
        }
        // Avoid NULL commands (i.e. process already started/stopped).
        if (cmd) {
            commands = commands.concat(cmd);
        }
    }
    return commands;
}

/**
 *Send a get logs, check, create or append command to the back end for execution.
 *
 * @param {[{}]} createCmds array of JSON objects representing commands
 * @param {Number} curr Sequence
 * @param {dojo.Deferred} waitCondition resolve and return to calling function after execution
 */
function sendFileOp (createCmds, curr, waitCondition) {
    var createCmd = createCmds[curr];
    if (forceStop) {
        waitCondition.resolve(false);
    } else {
        if (String(createCmd.cmd) === 'checkFileReq') {
            // Assert if the file exists
            console.debug('[DBG]sendFileOp, checkFileReq, ' + createCmd.name);
            mcc.server.checkFileReq(
                createCmd.host,
                createCmd.path,
                createCmd.name,
                createCmd.msg,
                createCmd.overwrite,
                function () {
                    console.debug('[DBG]' + 'File exist for ' + curr);
                    fileExists[curr].fileExist = true;
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'CHKFILE::SUCC::' + createCmd.host + ':' + createCmd.path +
                        ':' + createCmd.name + '\n');
                    curr++;
                    if (Number(curr) === createCmds.length) {
                        waitCondition.resolve();
                    } else {
                        sendFileOp(createCmds, curr, waitCondition);
                    }
                },
                function (errMsg) {
                    console.debug('[DBG]' + 'File does not exist for ' + curr);
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'CHKFILE::FAIL::' + createCmd.host + ':' + createCmd.path + ':' +
                        createCmd.name + '\n');
                    fileExists[curr].fileExist = false;
                    curr++;
                    if (Number(curr) === createCmds.length) {
                        waitCondition.resolve();
                    } else {
                        sendFileOp(createCmds, curr, waitCondition);
                    }
                }
            );
        } else if (String(createCmd.cmd) === 'appendFileReq') {
            mcc.server.appendFileReq(
                createCmd.host,
                createCmd.sourcePath,
                createCmd.sourceName,
                createCmd.destinationPath,
                createCmd.destinationName,
                function () {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'APPFILE::SUCC::' + createCmd.host + ':' + createCmd.path + ':' +
                        createCmd.name + '\n');
                    curr++;
                    if (Number(curr) === createCmds.length) {
                        waitCondition.resolve(true);
                    } else {
                        updateProgressDialog('Deploying configuration',
                            'Preparing file ' + createCmd.destinationName, { progress: curr }, false);
                        sendFileOp(createCmds, curr, waitCondition);
                    }
                },
                function (errMsg) {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'APPFILE::FAIL::' + createCmd.host + ':' + createCmd.path + ':' +
                        createCmd.name + '\n');
                    var wrn = 'Unable to append file ' +
                                createCmd.sourcePath +
                                createCmd.sourceName + ' to ' +
                                createCmd.destinationPath +
                                createCmd.destinationName + ' to ' +
                                ' on host ' + createCmd.host +
                                ':<br/>' + errMsg;
                    console.warn(wrn);
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
                    waitCondition.resolve(false);
                }
            );
        } else if (String(createCmd.cmd) === 'getLogsReq') {
            mcc.server.getLogsReq(
                createCmd.host,
                createCmd.sourcePath,
                createCmd.sourceName,
                createCmd.destinationName,
                function () {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'GETLOG ::SUCC::' + createCmd.host + ':' + createCmd.sourcePath +
                        createCmd.sourceName + ' downloaded to [HOME]/.mcc/' + createCmd.destinationName + '\n');
                    curr++;
                    if (Number(curr) === createCmds.length) {
                        waitCondition.resolve(true);
                    } else {
                        updateProgressDialog('Getting log files', createCmd.host +
                        ':' + createCmd.sourcePath + '->>>[HOME]/.mcc/' +
                        createCmd.destinationName, { progress: curr + 1 }, false);
                        sendFileOp(createCmds, curr, waitCondition);
                    }
                },
                function (errMsg) {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'GETLOG ::FAIL::' + createCmd.host + ':' + createCmd.sourcePath +
                        createCmd.sourceName + '\n');
                    var wrn = 'Unable to get log file ' +
                                createCmd.sourcePath +
                                createCmd.sourceName +
                                ' on host ' + createCmd.host +
                                ':<br/>' + errMsg;
                    console.warn(wrn);
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
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
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'CREFILE::SUCC::' + createCmd.host + ':' + createCmd.path + ':' +
                        createCmd.name + '\n');
                    curr++;
                    if (Number(curr) === createCmds.length) {
                        waitCondition.resolve(true);
                    } else {
                        updateProgressDialog('Deploying configuration',
                            'Creating directory ' + createCmd.path, { progress: curr }, false);
                        sendFileOp(createCmds, curr, waitCondition);
                    }
                },
                function (errMsg) {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'CREFILE::FAIL::' + createCmd.host + ':' + createCmd.path + ':' +
                        createCmd.name + '\n');
                    var wrn = 'Unable to create directory ' +
                                createCmd.path +
                                ' on host ' + createCmd.host +
                                ':<br/>' + errMsg;
                    console.warn(wrn);
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
                    waitCondition.resolve(false);
                }
            );
        }
    }
}

/**
 *Send all file ops to execution (sendFileOp)
 *
 * @param {[{}]} createCmds array of JSON objects representing commands
 * @returns {dojo.Deferred}
 */
function sendFileOps (createCmds) {
    var waitCondition = new dojo.Deferred();
    sendFileOp(createCmds, 0, waitCondition);
    return waitCondition;
}
/**
 *Aggregate function. Checks SSH connections to remote hosts and connects them.
 *
 * @param {dojo.Deferred} waitCond from caller
 * @returns {dojo.Deferred} same wait condition from caller resolved (true-succ, false-failure)
 */
function checkAndConnectHosts (waitCond) {
    console.debug('[DBG]enter checkAndConnectHosts');
    checkSSHConnections().then(function (ok) {
        if (!ok) {
            console.error('[ERR]checkAndConnectHosts, checkSSHConnections failed.');
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Couldn\'t check permanent ' +
                'connections to remote host(s).</span>');
        }
        setupSSHConnections().then(function (ok) {
            if (!ok) {
                console.error('[ERR]checkAndConnectHosts, setupSSHConnections failed.');
                console.error('[ERR]Connections to some hosts failed.');
            }
            waitCond.resolve(true);
        });
    });
    return waitCond;
}
/**
 *Format Cluster installation preparation commands.
 *
 * @param {String} host External IP address of host
 * @param {String} platform Installation platform (only RPMYUM for now)
 * @param {String} url URL to repository and file
 * @param {Number} version OS version Major
 * @param {Boolean} openFW Generate Open firewall ports commands. Not used.
 * @param {[String]} HPPairs Array of host:port_to_open items. Not used.
 * @returns {[{}]} Array of JSON objects representing commands
 */
function getPrepCmds (host, platform, url, version, openFW, HPPairs) {
    var installCommands = [];
    switch (platform) {
        case 'RPMYUM':
            var s1 = new ProcessCommand(host, '', 'sudo');
            delete s1.msg.file.autoComplete; // Don't want ac for SUDO, RPM...
            s1.addopt('yum');
            s1.addopt('install');
            s1.addopt('-y');
            s1.addopt('yum-utils');
            s1.progTitle = 'Installing YUM utils.';
            s1.msg.toRun = 'sudo yum install -y yum-utils';
            s1.msg.isCommand = true;
            installCommands.push(s1);

            var s2 = new ProcessCommand(host, '', 'sudo');
            delete s2.msg.file.autoComplete;
            s2.addopt('yum');
            s2.addopt('install');
            s2.addopt('-y');
            s2.addopt('wget');
            s2.addopt('unzip');
            s2.addopt('zip');
            s2.progTitle = 'Installing wget.';
            s2.msg.toRun = 'sudo yum install -y wget unzip zip';
            s2.msg.isCommand = true;
            installCommands.push(s2);

            var s13 = new ProcessCommand(host, '', 'sudo');
            delete s13.msg.file.autoComplete;
            s13.addopt('yum');
            s13.addopt('install');
            s13.addopt('-y');
            s13.addopt('lsof');
            s13.progTitle = 'Installing lsof.';
            s13.msg.toRun = 'sudo yum install -y lsof';
            s13.msg.isCommand = true;
            installCommands.push(s13);

            // EPEL for all!
            var s3 = new ProcessCommand(host, '', 'sudo');
            delete s3.msg.file.autoComplete;
            s3.addopt('rpm');
            s3.addopt('-ivh');
            s3.addopt('https://dl.fedoraproject.org/pub/epel/epel-release-latest-' + version + '.noarch.rpm');
            s3.progTitle = 'Installing EPEL.';
            s3.msg.toRun = 'sudo rpm -ivh https://dl.fedoraproject.org/pub/epel/epel-release-latest-' + version + '.noarch.rpm';
            s3.msg.isCommand = true;
            installCommands.push(s3);

            // STOP/REMOVE on all.
            var s4 = new ProcessCommand(host, '', 'sudo');
            delete s4.msg.file.autoComplete;
            s4.addopt('systemctl');
            s4.addopt('stop');
            s4.addopt('mysqld.service');
            s4.progTitle = 'Stopping mysqld.service.';
            s4.msg.toRun = 'sudo systemctl stop mysqld.service';
            s4.msg.isCommand = true;
            installCommands.push(s4);

            var s11 = new ProcessCommand(host, '', 'sudo');
            delete s11.msg.file.autoComplete;
            s11.addopt('systemctl');
            s11.addopt('disable');
            s11.addopt('mysqld.service');
            s11.progTitle = 'Removing mysqld.service.';
            s11.msg.toRun = 'sudo systemctl disable mysqld.service';
            s11.msg.isCommand = true;
            installCommands.push(s11);

            // postfix introduces dependency on libmysql.
            var s12 = new ProcessCommand(host, '', 'sudo');
            delete s12.msg.file.autoComplete;
            s12.addopt('rpm');
            s12.addopt('-e');
            s12.addopt("$(rpm -qa '^postfix')");
            s12.progTitle = 'Removing SW!';
            s12.msg.toRun = "sudo rpm -e $(rpm -qa '^postfix')";
            s12.msg.isCommand = true;
            installCommands.push(s12);
            var s5 = new ProcessCommand(host, '', 'sudo');
            delete s5.msg.file.autoComplete;
            s5.addopt('rpm');
            s5.addopt('-e');
            s5.addopt("$(rpm -qa '^mysql*')");
            s5.progTitle = 'Removing SW!';
            s5.msg.toRun = "sudo rpm -e $(rpm -qa '^mysql*')";
            s5.msg.isCommand = true;
            installCommands.push(s5);
            var s0 = new ProcessCommand(host, '', 'sudo');
            delete s0.msg.file.autoComplete;
            s0.addopt('rpm');
            s0.addopt('-e');
            s0.addopt("$(rpm -qa '^mariadb*')");
            s0.msg.toRun = "sudo rpm -e $(rpm -qa '^mariadb*')";
            s0.progTitle = 'Removing SW!';
            s0.msg.isCommand = true;
            installCommands.push(s0);

            var s7 = new ProcessCommand(host, '', 'sudo');
            delete s7.msg.file.autoComplete;
            s7.addopt('rpm');
            s7.addopt('-ivh');
            s7.addopt(url);
            // s7.addopt('http://repo.mysql.com/mysql80-community-release-el7-3.noarch.rpm'); Test RPM!
            s7.progTitle = 'Installing REPO RPM.';
            // after testing revert change!!!
            // s7.msg.toRun = 'sudo rpm -ivh http://repo.mysql.com/mysql80-community-release-el7-3.noarch.rpm';
            s7.msg.toRun = 'sudo rpm -ivh ' + url;
            s7.msg.isCommand = true;
            installCommands.push(s7);

            var s8 = new ProcessCommand(host, '', 'sudo');
            delete s8.msg.file.autoComplete;
            s8.addopt('yum-config-manager');
            s8.addopt('--disable');
            var mcv = mcc.util.getClusterUrlRoot();
            if (mcv.indexOf('80') >= 0) {
                mcv = '80';
                s8.addopt('mysql80-community');
            } else {
                mcv = '57';
                s8.addopt('mysql57-community');
            }
            s8.progTitle = 'Configuring REPO RPM.';
            s8.msg.toRun = 'sudo yum-config-manager --disable mysql' + mcv + '-community';
            s8.msg.isCommand = true;
            installCommands.push(s8);
            var s9 = new ProcessCommand(host, '', 'sudo');
            delete s9.msg.file.autoComplete;
            s9.addopt('yum-config-manager');
            s9.addopt('--enable');
            if (mcv === '80') {
                mcv = '8.0';
                s9.addopt('mysql-cluster-8.0-community');
            } else {
                mcv = '7.6';
                s9.addopt('mysql-cluster-7.6-community');
            }
            s9.progTitle = 'Configuring REPO RPM, enabling Cluster.';
            s9.msg.toRun = 'sudo yum-config-manager --enable mysql-cluster-' + mcv + '-community';
            s9.msg.isCommand = true;
            installCommands.push(s9);

            break;
        default:
            console.info('[INF]Unsupported install platform (unknown).');
            var what2 = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('unsuppInstPlat', [platform]));
            if ((what2 || {}).text) { mcc.util.displayModal('I', 3, what2.text); }
    }
    return installCommands;
}
/**
 *Handler for communication between main code and worker threads executing installation commands.
 * Regular msg.data.cmd: STARTED, STOPPED, RECEIVED and TERMINALERR.
    TERMINALERR signals ERROR in execution on remote host occurred and worker want's to quit.
EXTRA: msg.data.cmd member is errmsg from non terminal errors
    IF progress: '100', success: 'OK', terminal: false, done: true i.e. end of run.
EXTRA: msg.data.cmd member is errmsg from self.postMessage({Id: workerID, host: host,
    progress: currseq + "/" + total , cmd: instPrepCmds[currseq].progTitle + ":" + errMsg,
    //success: 'FAILED', terminal: terminal, done: false});
    It might yet be possible to recover unless terminal=true in which case next message will be TERMINALERR.
To STOP from us, worker responds with STOPPED before closing itself.
We need TRY-CATCH in situation where, say half of workers finished and then
we cancelled operation cause of failure on stalled host or similar.
 *
 * @param {{}} msg JSON message for/from worker thread
 */
function handleMessageFromInstallWorker (msg) {
    var ts = getTimeStamp();
    try {
        if (msg.data.cmd.toUpperCase() === 'STOPPED') {
            console.info('[INF]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host + ' has finished.');
            waitWorkers[msg.data.Id].resolve(true);
        } else {
            if (msg.data.cmd.toUpperCase() === 'TERMINALERR') {
                console.info('[INF]' + 'Stopping Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                    ' after reported terminal error in installation.');
                waitWorkers[msg.data.Id].resolve(false);
                // Do terminal stuff.
                Workers[msg.data.Id].postMessage({ cmd: 'stop', Terminal: true });
            } else {
                if (msg.data.cmd.toUpperCase() === 'RECEIVED') {
                    // Check all went well with recv! Change WORKER.JS code too.
                    console.debug('[DBG]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                        ' has received commands to run.');
                } else {
                    if (msg.data.cmd.toUpperCase() === 'STARTED') {
                        console.debug('[DBG]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                            ' started installation.');
                    } else {
                        // These are actual commands executed OR error messages.
                        // Do we have end of run?
                        if (msg.data.progress.toString() === '100' && msg.data.succ.toUpperCase() === 'OK' &&
                            !msg.data.terminal && msg.data.done) {
                            // EOR for this worker, send stop cmd.
                            console.debug('[DBG]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                                ' is about to be stopped.');
                            Workers[msg.data.Id].postMessage({ cmd: 'stop', Terminal: false });
                        } else {
                            if (msg.data.terminal && msg.data.succ.toUpperCase() === 'FAILED') {
                                // This means at least 1 host is not properly installed thus
                                // Cluster will not start. Send STOP to all workers.
                                // The offending worker will be stopped in its own message loop.
                                for (var i = 0; i < Workers.length; i++) {
                                    if (i !== Number(msg.data.Id)) {
                                        console.info('[INF]' + 'Shutting down workers.');
                                        Workers[i].postMessage({ cmd: 'stop', Id: i, Terminal: false });
                                    }
                                }
                                console.info('[INF]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                                    ' reported terminal error in installation.\nCommand ' + msg.data.cmd + '.');
                                cmdLog.push(ts + 'INSTALL::STOP::' + 'Worker' + msg.data.Id + '@' +
                                    Workers[msg.data.Id].host + '::' + msg.data.cmd + ' ERROR:' + msg.data.err + '\n');

                                // There is a remote possibility that, say, SQL node reports as STARTED to
                                // management node while actual start command subsequently fails. 1st
                                // invalidates progress dialog and 2nd requests does color change on null object.
                                var cwpb = dijit.byId('configWizardProgressBar');
                                if (cwpb) {
                                    var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
                                    visualTile.style.backgroundColor = '#FF3366';
                                }
                                updateProgressDialog('Cluster installation failed!', '', { progress: '100%' }, false);
                                // alert to progress: currseq + "/" + total , cmd: instPrepCmds[currseq].progTitle + ":" + errMsg
                                // and wait for worker to request closing.
                            } else {
                                if (!msg.data.terminal && msg.data.succ.toUpperCase() === 'FAILED') {
                                    // Workers[msg.data.Id].cmd
                                    console.info('[INF]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                                    ' reported failed command in installation.\nCommand ' + msg.data.cmd + '.');
                                    // Failure already has \n
                                    cmdLog.push(ts + 'INSTALL::FAIL::' + 'Worker' + msg.data.Id + '@' +
                                        Workers[msg.data.Id].host + '::' + msg.data.cmd + ' ERROR:' + msg.data.err);
                                    ++installProgress;
                                    updateProgressDialog('Installing cluster', msg.data.cmd,
                                        { progress: installProgress }, false);
                                    // Ordinary error, safe to ignore.
                                } else {
                                    // This is ordinary response from executing some command.
                                    console.debug('[DBG]' + 'Worker' + msg.data.Id + '@' + Workers[msg.data.Id].host +
                                        ' reported success running command\n' + msg.data.cmd);
                                    cmdLog.push(ts + 'INSTALL::SUCC::' + 'Worker' + msg.data.Id + '@' +
                                        Workers[msg.data.Id].host + '::' + msg.data.cmd + '\n');
                                    ++installProgress;
                                    updateProgressDialog('Installing cluster', msg.data.cmd,
                                        { progress: installProgress }, false);
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (E) {
        console.debug('[DBG]' + 'Worker' + msg.data.Id + ' already stopped.');
    }
}

function doStop (isIE, msg, closeProgress, whichP) {
    // Structured as-is on purpose! Need few ms to allow workers to shut themselves down.
    // Please do not "optimize" alrt(msg).
    // Sequence: Main -> worker "stop". Worker -> main "STOPPED", thread closes.
    // Main recv STOPPED -> resolve promise for that worker.
    // I need IE11 part as well for prior-to-actual-installation stop.
    if (isIE) {
        if (whichP.toLowerCase() === 'install') {
            cancelButton('configWizardInstallCluster');
        } else {
            if (whichP.toLowerCase() === 'deploy') {
                cancelButton('configWizardDeployCluster');
            }
        }
        alert(msg);
    } else {
        // CLOSE workers!
        for (var i = 0; i < Workers.length; i++) {
            console.info('[INF]Shutting down workers.');
            Workers[i].postMessage({ cmd: 'stop', Id: i, Terminal: false });
        }
        if (whichP.toLowerCase() === 'install') {
            cancelButton('configWizardInstallCluster');
        } else {
            if (whichP.toLowerCase() === 'deploy') {
                cancelButton('configWizardDeployCluster');
            }
        }
        alert(msg);
        waitWorkers = [];
        Workers = [];
    }
    if (closeProgress) { removeProgressDialog(); }
}
/**
 *Main routine to install Cluster binaries on hosts.
 *
 * @returns {dojo.Deferred}
 */
function installCluster () {
    dijit.hideTooltip(dojo.byId('configWizardInstallCluster'));
    dojo.byId('configWizardInstallCluster').blur();
    document.getElementById('startupDetails').focus();
    // CHECK if Cluster can be installed on platform. RPM/YUM is the only supported for now.
    /*
        Logically, 2-phase process
            PREPARATION of the host
            and
            INSTALLATION of SW depending on PROCESSES running on some host.
        PREPARATION loops *HOSTS* and installs various UTILS and EPEL.
        INSTALLATION loops *PROCESSES* and installs main binaries.
    */
    // There are TWO messaging mechanisms ;
    // one is employed by, say, DEPLOY to create directories and it has entire
    // message formed in one go like the proto message block, other is by using
    // PROCESSCOMANDS (sc) employed by, say, START/STOP cluster.
    // For install, we use PROCESSCOMANDS (sc).

    var hostName = '';
    var instOnHost = false;
    var instOnHostRepo = '';
    var instOnHostDocker = '';
    var platform = '';
    // "WINDOWS", "CYGWIN", "DARWIN", "SunOS", "Linux"
    var flavor = '';
    // RPM/YUM: "ol": OS=el, "fedora": OS=fc, "centos": OS=el, "rhel": OS=el,
    // RPM:ZYpp: "opensuse": OS=sles
    // There is no "latest" for APT repo. Also, there is no way to discover newest.
    // DPKG/APT: "ubuntu": from APT, OS=ubuntu, "debian": from APT, OS=debian
    var ver = '';
    var array = [];
    var anyH = false;
    var instPrepCmds = [];
    var instInstallCmds = [];
    // Check if some host is already installed in configuration file so we can offer "Install only new hosts"
    var alreadyInstalled = false;
    // Check if user wants to reinstall on all hosts regardless of above.
    var fullReinstall = true;
    var what;
    // Reset stuff.
    installProgress = 1;
    Workers = [];
    waitWorkers = [];
    var clRunning = [];
    clRunning = clServStatus();
    if (determineClusterRunning(clRunning)) {
        cancelButton('configWizardInstallCluster');
        what = mcc.userconfig.setCcfgPrGen.apply(this,
            mcc.userconfig.setMsgForGenPr('clRunning', ['installCluster']));
        if ((what || {}).text) {
            console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
            mcc.util.displayModal('I', 3, what.text);
        }
        console.warn('Can\'t initiate installation while Cluster is running!');
        return;
    }

    // Check if we already had successful installation on (some of the) host(s).
    // Upon first install, hosts[h].getValue("SWInstalled") will always be FALSE so
    // if (alreadyInstalled) will not run.
    for (var h = 0; h < hosts.length; h++) {
        if (!hosts[h].getValue('anyHost')) {
            if (hosts[h].getValue('SWInstalled') === true) {
                alreadyInstalled = true;
                break;
            }
        }
    }
    if (alreadyInstalled) {
        if (confirm('Install SW for new host(s) only?')) {
            // Do not reinstall on already installed hosts.
            fullReinstall = false;
        }
    }

    var SSHBl = {};
    for (h = 0; h < hosts.length; h++) {
        anyH = hosts[h].getValue('anyHost');
        if (anyH) { continue; }
        // For host to get SW installed we need:
        //  o confirmation from user that we are allowed to install SW on host
        //  o and, if SW is already installed on host, that user wants to do reinstall.
        instOnHost = hosts[h].getValue('installonhost') && (hosts[h].getValue('SWInstalled') !== true ||
            fullReinstall);
        // only Linux and RPMYUM for now
        instOnHost = instOnHost && (hosts[h].getValue('uname').toLowerCase() === 'linux');
        // Skip default AnyHost used to init storage!
        if (instOnHost && !anyH) {
            hostName = hosts[h].getValue('name');
            console.debug('[DBG]Preparing host ' + hostName);
            SSHBl = formProperSSHBlock(hostName);
            if (!SSHBl) {
                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">No SSH block formed, ' +
                    'can\'t continue.</span>');
                break;
            }
            instOnHostRepo = hosts[h].getValue('installonhostrepourl');
            instOnHostDocker = hosts[h].getValue('installonhostdockerurl');
            // dockInf = hosts[h].getValue('dockerinfo');
            console.debug('[DBG]Check installation on host ' + hostName);
            platform = hosts[h].getValue('uname');
            // "WINDOWS", "CYGWIN", "DARWIN", "SunOS", "Linux"
            flavor = hosts[h].getValue('osflavor');
            // RPM/YUM: "ol": OS=el, "fedora": OS=fc, "centos": OS=el, "rhel": OS=el,
            // RPM:ZYpp: "opensuse": OS=sles
            // There is no "latest" for APT repo. Also, there is no way to discover newest.
            // DPKG/APT: "ubuntu": from APT, OS=ubuntu, "debian": from APT, OS=debian
            ver = hosts[h].getValue('osver');
            console.debug('[DBG]' + 'Platform & OS details ' + platform + ', ' + flavor + ', ' + ver);
            array = ver.split('.');
            ver = array[0]; // Take just MAJOR
            // Determine type of install:
            var tmp = [];
            var tmp1 = [];
            var tmpcons = [];
            if (instOnHostRepo === '') {
                // Try Docker
                if (instOnHostDocker === '') {
                    // ERROR condition; INSTALL but both REPO and DOCKER urls are empty.
                    // Return empty array (meaning abort).
                    console.debug('[DBG]Both Docker and Repo URLs are empty for host %s! Aborting.', hostName);
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Both Docker and ' +
                        'Repo URLs are empty for host ' + hostName + '! Aborting.</span>');
                    break; // No installation will be done since one host failed completely.
                } else {
                    // DOCKER install
                    console.debug('[DBG]Will use Docker for host .', hostName);
                    what = mcc.userconfig.setCcfgPrGen.apply(this,
                        mcc.userconfig.setMsgForGenPr('useDockerHostNF', [hostName]));
                    if ((what || {}).text) {
                        mcc.util.displayModal('I', 3, what.text);
                    }
                    console.warn('Docker installation not available.');
                }
            } else {
                // REPO install, check supported OS
                console.debug('[DBG]Will use REPO for host .', hostName);
                // platform, flavor, ver!
                if (platform.toUpperCase() === 'LINUX') {
                    if (['ol', 'centos', 'rhel'].indexOf(flavor.toLowerCase()) >= 0) {
                        console.debug('[DBG]INSTALL CLUSTER: Pushing RPM/YUM commands for host ' + hostName);
                        // Get the actual commands.
                        // Since this is on host-by-host basis, we should use TMP  to fill in array
                        // for worker threads. We also need to push new dojo.deferred for each host
                        // with commands to execute (array.length > 0).
                        // We then send RECEIVE message to worker with both prep and install commands.

                        // DEPLOY opens FW ports.
                        tmp = getPrepCmds(hostName, 'RPMYUM', instOnHostRepo, ver, 'NO', '');
                        for (var i = 0; i < tmp.length; i++) {
                            instPrepCmds.push(tmp[i]);
                            tmpcons.push(tmp[i].msg);
                            delete tmpcons[tmpcons.length - 1].isDone; // function
                            tmpcons[tmpcons.length - 1].isDone = false;// flag
                            // rm function from ProcessCommand object as it is not serializable.
                            delete tmpcons[tmpcons.length - 1].addopt;
                            delete tmpcons[tmpcons.length - 1].html; // not needed
                        }
                        // Get the actual commands for each process running on each host in Cluster.
                        tmp1 = getInstallCommands(hostName, platform, 'REPO', flavor);
                        for (i = 0; i < tmp1.length; i++) {
                            instInstallCmds.push(tmp1[i]);
                            tmpcons.push(tmp1[i].msg);
                            delete tmpcons[tmpcons.length - 1].isDone; // function
                            tmpcons[tmpcons.length - 1].isDone = false;// flag
                            // rm function from ProcessCommand object as it is not serializable.
                            delete tmpcons[tmpcons.length - 1].addopt;
                            delete tmpcons[tmpcons.length - 1].html; // not needed
                        }
                        if (!window.MSInputMethodContext && !document.documentMode) {
                            // New deferred waiting for just this worker.
                            waitWorkers.push(new dojo.Deferred());
                            var worker = new Worker('../js/mcc/worker/installWorker.js');
                            worker.host = hostName;
                            // worker.addEventListener('message', handleMessageFromInstallWorker);
                            worker.onmessage = handleMessageFromInstallWorker;
                            // exact worker is addressed as Workers[index]
                            Workers.push(worker);
                            // Send data to execute to worker. Nothing begins until START message is sent.
                            i = Workers.length - 1;
                            // First message is used to send ALL commands and credentials to run them.
                            Workers[Workers.length - 1].postMessage(
                                {
                                    cmd: 'receive',
                                    host: hostName,
                                    Id: i,
                                    SSHBlock: SSHBl,
                                    msg: tmpcons
                                }
                            );
                            worker = null;
                        }
                        tmp = [];
                        tmp1 = [];
                        tmpcons = [];
                    } else {
                        what = mcc.userconfig.setCcfgPrGen.apply(this,
                            mcc.userconfig.setMsgForGenPr('unsuppInstOS', [platform.toUpperCase()]));
                        if ((what || {}).text) {
                            console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                            mcc.util.displayModal('I', 3, what.text);
                        }
                        console.warn('Installation not available for ' + platform.toUpperCase() + '!');
                    }
                } else {
                    console.warn('[WRN]Nothing to do for host %s', hostName);
                }
            }
        } else {
            console.warn('Installation only supported for Linux\nor no installation for host.');
        }
    }
    if (!Array.isArray(instInstallCmds) || !instInstallCmds.length) {
        // Bail out, no commands.
        doStop(!!window.MSInputMethodContext && !!document.documentMode, 'No preparation and/or installation commands generated!', false, 'Install');
        return;
    }
    console.info('[INF]' + 'Consolidating install commands arrays.');
    // There are two execution routines; 1) PARALLEL: Using web workers and promises and
    // 2) SEQUENTIAL: Using DOJO deferred since IE11 does not support promises.
    // New parallel code for modern browsers:
    // By this point, command arrays are already passed to each worker thread. We just need to send
    // START signal and monitor for success/failure in onmessage handler.
    // Old sequential code for IE11.
    // Now we need to pass instPrepCmds to API unit and check which, if any failed.

    // Failure to execute command with TERMINAL member set to TRUE is FATAL and
    // thus breaks installation procedure.

    // Merge the two command arrays into one, minding which comes first:
    for (var ip = 0; ip < instInstallCmds.length; ip++) {
        instPrepCmds.push(instInstallCmds[ip]);
    }

    // Add/Set isDone AS FLAG and NOT FUNCTION.
    for (ip = 0; ip < instPrepCmds.length; ip++) {
        delete instPrepCmds[ip].isDone; // function
        instPrepCmds[ip].isDone = false;// flag
    }
    // From this point on, instPrepCmds is consolidated array with all the preparation
    // AND installation commands!

    // Print the commands out so user can choose whether to proceed.
    var instMsgSummary = '\n====================================';
    instMsgSummary += "\nPress 'OK' to continue executing commands\nor 'Cancel' to stop the installation procedure.";
    instMsgSummary += '\n====================================';
    // Just to make IE11 happy, no arrow functions for this MAP/MAP.
    var result = instPrepCmds.map(function (a) {
        return '\n' + a.msg.file.name + '@' + a.msg.file.hostName + ', ' + a.msg.params.param.map(function (b) {
            return b.name;
        }).join(' ');
    });
    for (ip = 0; ip < result.length; ip++) {
        instMsgSummary += result[ip];
    }
    if (!confirm(instMsgSummary)) {
        doStop(!!window.MSInputMethodContext && !!document.documentMode, 'No installation chosen!', false, 'Install');
        return;
    }
    // First we need to be sure we have working connections to all hosts.
    var waitCd = new dojo.Deferred();
    checkAndConnectHosts(waitCd).then(function (ok) {
        if (!allHostsConnected) {
            var noGo = 'Can not proceed with installation since not all Cluster nodes are connected!\n' +
                'Please correct the problem and refresh status either by moving\n' +
                'from/to this page or by running "Connect remote hosts" from Cluster tools.';
            doStop(!!window.MSInputMethodContext && !!document.documentMode, noGo, false, 'Install');
            return;
        }
        // ! on IE
        if (!window.MSInputMethodContext && !document.documentMode) {
            // Employ the workers. All of the work is done in handleMessageFromInstallWorker so
            // here we need to wait for waitWorkers list to resolve and present results.
            updateProgressDialog('Installing cluster',
                'Starting workers.',
                { maximum: instPrepCmds.length + 1,
                    progress: installProgress }, false);
            var waitAll = null;
            waitAll = new dojo.DeferredList(waitWorkers);
            for (ip = 0; ip < Workers.length; ip++) {
                console.info('[INF]' + 'Starting workers.');
                Workers[ip].postMessage({ cmd: 'start', Id: ip });
            }
            // Wait...
            waitAll.then(function () {
                // Release resources:
                removeProgressDialog();
                waitWorkers = [];
                Workers = [];
                console.info('[INF]' + 'Install procedure ended.');
                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:teal">Cluster installed.</span>');
                cancelButton('configWizardInstallCluster');
            });
        } else {
            // Old, synchronous code which will work on IE11.
            console.warn('[WRN]Running old, synchronous code. Consider using modern browser.');
            // External wait condition
            var waitCondition = new dojo.Deferred();
            var currSeq = 0;
            var errorReplies = 0;
            var timeout;

            function onTimeout () {
                if (instPrepCmds[currSeq].isDone) {
                    ++currSeq;
                    updateProgressAndinstallNext(false);
                } else {
                    // console.debug("[DBG]" + "returned false for "+instPrepCmds[currseq].progTitle)
                    timeout = setTimeout(onTimeout, 2000);
                }
            }

            function onError (errMsg, errReply, terminal) {
                if (terminal === undefined) {
                    // We have BE thrown error, most likely not connected to hosts, so kill the process.
                    terminal = true;
                }
                console.error('[ERR]' + 'Error occurred while installing cluster: %s terminal = %s',
                    errMsg, terminal);
                ++errorReplies;
                var ts = getTimeStamp();
                if (terminal) {
                    cmdLog.push(ts + 'INSTALL::STOP::' + instPrepCmds[currSeq].progTitle + '@' +
                        instPrepCmds[currSeq].msg.file.hostName + '\n');
                    // There is a remote possibility that, say, SQL node reports as STARTED to
                    // management node while actual start command subsequently fails. 1st
                    // invalidates progress dialog and 2nd requests color change on null object.
                    var cwpb = dijit.byId('configWizardProgressBar');
                    if (cwpb) {
                        var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
                        visualTile.style.backgroundColor = '#FF3366';
                    }
                } else {
                    cmdLog.push(ts + 'INSTALL::FAIL::' + instPrepCmds[currSeq].progTitle + '@' +
                        instPrepCmds[currSeq].msg.file.hostName + '\n');
                    ++currSeq;
                }
                updateProgressAndinstallNext(terminal);
            }

            function onReply (rep) {
                console.debug('[DBG]Reply for: %s@%s is %s with ExitStatus of %s', instPrepCmds[currSeq].progTitle,
                    instPrepCmds[currSeq].msg.file.hostName, JSON.stringify(rep), rep.body.exitstatus);
                var ts = getTimeStamp();
                if (Number(rep.body.exitstatus) === 0) {
                    // move on
                    instPrepCmds[currSeq].isDone = true;
                    cmdLog.push(ts + 'INSTALL::SUCC::' + instPrepCmds[currSeq].progTitle + '@' +
                        instPrepCmds[currSeq].msg.file.hostName + '\n');
                }
                if ((Number(rep.body.exitstatus) > 0) &&
                    (instPrepCmds[currSeq].msg.params.param[0].name.toLowerCase() === 'rpm')) {
                    // Check what's going on with RPM. search -1 means NOT FOUND. 0 is FOUND.
                    if (String(rep.body.err[0]).search('warning') === 0) {
                        // Just a warning, proceed.
                        instPrepCmds[currSeq].isDone = true;
                        cmdLog.push(ts + 'INSTALL::WARN::' + instPrepCmds[currSeq].progTitle + '@' +
                            instPrepCmds[currSeq].msg.file.hostName + '\n');
                    } else {
                        instPrepCmds[currSeq].isDone = true;
                        onError(String(rep.body.out), rep, false);
                        return;
                    }
                } else {
                    if (instPrepCmds[currSeq].msg.params.param[0].name.toLowerCase() === 'systemctl') {
                        if (Number(rep.body.exitstatus) === 5) {
                            // Probably just fresh box and no service to STOP. Just record the error and move on.
                            instPrepCmds[currSeq].isDone = true;
                            onError(String(rep.body.out), rep, false);
                            return;
                        } else {
                            if (Number(rep.body.exitstatus) === 1) {
                                // Probably just box and without service to STOP. Just record the error and move on.
                                instPrepCmds[currSeq].isDone = true;
                                onError(String(rep.body.out), rep, false);
                                return;
                            }
                        }
                    } else {
                        // Unhandled errors. !RPM, !SYSTEMCTL.
                        if (Number(rep.body.exitstatus) >= 1) {
                            // Unhandled thus terminal.
                            instPrepCmds[currSeq].isDone = true;
                            onError(String(rep.body.out), rep, true);
                            return;
                        }
                    }
                }
                onTimeout();
            }

            function updateProgressAndinstallNext (terminated) {
                if (terminated) {
                    updateProgressDialog('Cluster installation failed!', '', { progress: '100%' }, false);
                    cancelButton('configWizardInstallCluster');
                    clearTimeout(timeout);
                    removeProgressDialog();
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' +
                        'Cluster installation failed!</span>');
                    waitCondition.resolve();
                    return;
                } else {
                    if (currSeq >= instPrepCmds.length) {
                        var mess = errorReplies ? 'Install procedure has completed, but ' + errorReplies +
                            ' out of ' + instPrepCmds.length + ' commands failed' : 'Cluster installed successfully';
                        console.info('[INF]' + mess);
                        updateProgressDialog(mess, '', { progress: '100%' }, false);
                        clearTimeout(timeout);
                        removeProgressDialog();
                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' + mess + '</span>');
                        cancelButton('configWizardInstallCluster');
                        // This is bad for the case when you a) add new host to Cluster
                        // and b) when you add new process to host (or change where process runs).
                        waitCondition.resolve();
                        return;
                    }
                }
                console.debug('[DBG]' + 'commands[%s].progTitle: %s', currSeq, instPrepCmds[currSeq].progTitle);
                updateProgressDialog('Installing cluster', instPrepCmds[currSeq].progTitle,
                    { maximum: instPrepCmds.length, progress: currSeq + 1 }, false);
                mcc.server.doReq('executeCommandReq', { command: instPrepCmds[currSeq].msg }, '', cluster,
                    onReply, onError);
            }
            // Initiate install sequence
            updateProgressAndinstallNext(false);
            return waitCondition;
        }
    });
}
/**
 *Handler for communication between main code and worker threads executing deploy commands.
 *Regular msg.data.cmd: STARTED, STOPPED, RECEIVED and TERMINALERR.
 TERMINALERR signals ERROR in execution on remote host occurred and worker want's to quit.
EXTRA: msg.data.cmd member is errmsg from non terminal errors IF
    progress: '100', success: 'OK', terminal: false, done: true i.e. end of run.
EXTRA: msg.data.cmd member is errmsg from self.postMessage(
    {Id: workerID, host: host, progress: currseq + "/" + total , cmd: instPrepCmds[currseq].progTitle + ":" + errMsg,
    //success: 'FAILED', terminal: terminal, done: false});
It might yet be possible to recover unless terminal=true in which case next message will be TERMINALERR.
To STOP from us, worker responds with STOPPED before closing itself.
 * @param {{}} msg JSON message for/from worker thread
 */
function handleMessageFromDeployWorker (msg) {
    var ts = getTimeStamp();
    try {
        if (msg.data.cmd.toUpperCase() === 'STOPPED') {
            console.info('[INF]Worker%s@%s has finished.', msg.data.Id, Workers[msg.data.Id].host);
            waitWorkers[msg.data.Id].resolve(true);
        } else {
            if (msg.data.cmd.toUpperCase() === 'TERMINALERR') {
                // Deployment errors are all treated as TERMINAL so this is just
                // for rejecting promise for specified worker.
                console.info('[INF]Stopping Worker%s@%s after reported terminal error in deployment.',
                    msg.data.Id, Workers[msg.data.Id].host);
                // So it does not resolve in STOPPED.
                Workers[msg.data.Id].postMessage({ cmd: 'stop', Id: msg.data.Id, Terminal: true });
                waitWorkers[msg.data.Id].resolve(false);
            } else {
                if (msg.data.cmd.toUpperCase() === 'RECEIVED') {
                    // Check all went well with recv! Change WORKER.JS code too.
                    console.debug('[DBG]Worker%s@%s has received commands to run.', msg.data.Id,
                        Workers[msg.data.Id].host);
                } else {
                    if (msg.data.cmd.toUpperCase() === 'STARTED') {
                        console.debug('[DBG]Worker%s@%s started deployment.', msg.data.Id,
                            Workers[msg.data.Id].host);
                    } else {
                        // These are actual commands executed OR error messages.
                        // Do we have end of run?
                        if (msg.data.progress.toString() === '100' && msg.data.succ.toUpperCase() === 'OK' &&
                            !msg.data.terminal && msg.data.done) {
                            // EOR for this worker, send stop cmd.
                            console.debug('[DBG]Worker%s@%s is about to be stopped.', msg.data.Id,
                                Workers[msg.data.Id].host);
                            Workers[msg.data.Id].postMessage({ cmd: 'stop', Id: msg.data.Id, Terminal: false });
                        } else {
                            if (msg.data.terminal && msg.data.succ.toUpperCase() === 'FAILED') {
                                // This means at least 1 host is not properly deployed to thus
                                // Cluster will not, most likely, start. Send STOP to all workers.
                                // The offending worker will be stopped in its own message loop
                                // (see 'TERMINALERR' above).
                                for (var i = 0; i < Workers.length; i++) {
                                    if (i !== Number(msg.data.Id)) {
                                        console.info('[INF]' + 'Shutting down workers.');
                                        Workers[i].postMessage({ cmd: 'stop', Id: i, Terminal: false });
                                    }
                                }
                                console.info('[INF]Worker%s@%s reported terminal error in deployment.\nCommand %s.',
                                    msg.data.Id, Workers[msg.data.Id].host, Workers[msg.data.Id].cmd);
                                cmdLog.push(ts + 'DEPLOY ::STOP::' + 'Worker' + msg.data.Id + '@' +
                                    Workers[msg.data.Id].host + '::' + Workers[msg.data.Id].cmd + '\n');
                                // There is a remote possibility that, say, SQL node reports as STARTED to
                                // management node while actual start command subsequently fails. 1st
                                // invalidates progress dialog while 2nd requests color change on null object.
                                var cwpb = dijit.byId('configWizardProgressBar');
                                if (cwpb) {
                                    var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
                                    visualTile.style.backgroundColor = '#FF3366';
                                }
                                updateProgressDialog('Cluster deployment failed!', '', { progress: '100%' }, false);
                                // alert to progress: currseq + "/" + total , cmd: instPrepCmds[currseq].progTitle + ":" + errMsg
                                // and wait for worker to request closing.
                            } else {
                                if (!msg.data.terminal && msg.data.succ.toUpperCase() === 'FAILED') {
                                    console.info('[INF]Worker%s@%s reported failed command in deployment.\nCommand %s.',
                                        msg.data.Id, Workers[msg.data.Id].host, msg.data.cmd);
                                    cmdLog.push(ts + 'DEPLOY ::FAIL::' + 'Worker' + msg.data.Id + '@' +
                                        Workers[msg.data.Id].host + '::' + msg.data.cmd + '\n');
                                    ++installProgress;
                                    updateProgressDialog('Deploying cluster',
                                        msg.data.cmd, { progress: installProgress }, false);
                                    // Ordinary error, safe to ignore.
                                } else {
                                    // This is ordinary response from executing some command.
                                    console.debug('[DBG]Worker%s@%s reported success running command\n%s',
                                        msg.data.Id, Workers[msg.data.Id].host, msg.data.cmd);
                                    cmdLog.push(ts + 'DEPLOY ::SUCC::' + 'Worker' + msg.data.Id + '@' +
                                        Workers[msg.data.Id].host + '::' + msg.data.cmd + '\n');
                                    ++installProgress;
                                    updateProgressDialog('Deploying cluster', msg.data.cmd,
                                        { progress: installProgress }, false);
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (E) {
        console.debug('[DBG]Worker%s already stopped.', msg.data.Id);
    }
}
/**
 *Part of deployment process Initializing datadir for mysqld(s)
 *
 * @param {Boolean} inNeedOfInit Not used atm.
 * @returns {dojo.Deferred}
 */
function initMySQLds (inNeedOfInit) {
    // inNeedOfInit not used atm.
    var waitCondition = new dojo.Deferred();
    var mysqldcommands = _getClusterCommands(getInitProcessCommands, ['sql']);
    // For these commands, isDone is Function, isCommand = False.
    var errorReplies = 0;
    var howMany = mysqldcommands.length;
    /* For DEBUG purposes
    for (var i in mysqldcommands) {
        console.debug('[DBG]INIT cmd[' + i + '] is ' + mysqldcommands[i].Actual);
    }*/
    howMany = mysqldcommands.length;
    console.debug('[DBG]Total of %i SQL init commands generated', howMany);
    var reqHead;
    var abortedInits = [];
    // There is no isDone for INITIALIZE commands.
    function onErr (errMsg, errReply) {
        // No need to display each failure.
        // --initialize specified but the data directory has files in it
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }
        var index = -1;
        // Match rep.head.rSeq with mysqldcommands[i].seq to determine which command returned!
        for (var i = 0; i < mysqldcommands.length; i++) {
            if (Number(mysqldcommands[i].seq) === Number(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        if ((JSON.stringify(errMsg)).indexOf('initialize specified but the data directory has files in it') > 0) {
            errMsg = 'data directory exists and has files in it. Aborting';
            abortedInits.push({ HOST: mysqldcommands[index].msg.file.hostName,
                MESSAGE: 'Aborted initialize since data dir exists and has files in it.' });
        }
        console.error('%s[ERR]DEPLOY ::FAIL::%s, %s.', ts, mysqldcommands[index].Actual, errMsg);
        cmdLog.push(ts + 'DEPLOY ::FAIL::' + mysqldcommands[index].Actual + ', ' + errMsg + '\n');
        --howMany;
        updateProgressAndInittNextMySQLd();
    }

    function onRep (rep) {
        var ro = '';
        var index = -1;
        // console.debug("rep is " + JSON.stringify(rep));
        // Match rep.head.rSeq with mysqldcommands[i].seq to determine which command returned!
        for (var i = 0; i < mysqldcommands.length; i++) {
            if (Number(mysqldcommands[i].seq) === Number(rep.head.rSeq)) {
                index = i;
                break;
            }
        }
        ro = String(rep.body.out);
        if (ro.indexOf('errcode:') === 0) {
            // Error :-/
            onErr(ro, rep);
            return;
        } else {
            var ts = getTimeStamp();
            cmdLog.push(ts + 'DEPLOY ::SUCC::' + mysqldcommands[index].Actual + '\n');
            console.info('%s[INF]DEPLOY ::SUCC::%s.', ts, mysqldcommands[index].Actual);
        }
        // START ended with success, wait next reply.
        --howMany;
        updateProgressAndInittNextMySQLd();
    }

    function updateProgressAndInittNextMySQLd () {
        if (howMany <= 0) {
            var message;
            if (mysqldcommands.length > 0) {
                message = errorReplies ? 'Initialize (SQL) procedure has completed, but ' + errorReplies + ' out of ' +
                    mysqldcommands.length + ' commands failed' : 'Cluster SQL nodes initialized successfully.';
            } else {
                message = 'No SQL nodes to initialize.';
                errorReplies = 0;
            }
            console.info('[INF]' + message);
            if (abortedInits.length > 0) {
                // IE11 does not know "table" function :-/
                if (!!window.MSInputMethodContext && !!document.documentMode) {
                    console.warn(abortedInits);
                } else {
                    console.table(abortedInits);
                }
            }
            waitCondition.resolve(errorReplies <= 0);
        }
    }
    // Initiate start sequence by sending cmd to all mysqlds. Synchronize
    // via SEQ of command that BE will return to us (in rSeq member of reply).
    var t = 0;
    if (howMany > 0) {
        moveProgress('Deploying cluster', 'Initializing mysqld(s)...');
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            mysqldcommands[t].seq = reqHead.seq;
            mcc.server.doReq('executeCommandReq',
                { command: mysqldcommands[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= mysqldcommands.length) { t = -1; }
        } while (t >= 0);
    }
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndInittNextMySQLd');
    updateProgressAndInittNextMySQLd();
    return waitCondition;
}

/**
 *Remove datadir(s) from previous run. Call mcc.server.dropStuffReq to do the work.
 *
 * @param {Boolean} runThis from confirmation box
 * @returns {dojo.Deferred}
 */
function cleanUpPreviousRunDatadirs (runThis) {
    var waitCondition = new dojo.Deferred();
    if (!runThis) {
        // Do not run the delete code.
        waitCondition.resolve(true);
        return waitCondition;
    }

    var rmDatadirCommands = [];
    var hostName = '';
    var platform = '';
    var datadir = '';
    var anyH = false;
    for (var h = 0; h < hosts.length; h++) {
        anyH = hosts[h].getValue('anyHost');
        if (anyH) { continue; }
        platform = hosts[h].getValue('uname');
        // "WINDOWS", "CYGWIN", "DARWIN", "SunOS", "Linux"
        hostName = hosts[h].getValue('name');
        datadir = hosts[h].getValue('datadir');
        console.debug('[DBG]Preparing commands to delete %s on host %s(%s).', datadir, hostName, platform);
        rmDatadirCommands.push({
            host: hostName,
            path: datadir,
            seq: -1
        });
    }
    if (rmDatadirCommands.length <= 0) {
        console.debug('[DBG]No delete code generated. Returning.');
        rmDatadirCommands = [];
        waitCondition.resolve(true);
        return waitCondition;
    }
    var errorReplies = 0;
    var howMany = rmDatadirCommands.length;
    /* For DEBUG purposes
    for (var i in rmDatadirCommands) {
        console.debug('[DBG]DELETE cmd[' + i + '] is ' + rmDatadirCommands[i].path);
    }*/
    console.debug('[DBG]Total of %i DELETE commands generated', howMany);
    // We should check if datadirs exist at all before running this.
    // Sort of alrt("datadir does not exist, configuration changed significantly, please redeploy.");
    var reqHead;
    function onErr (errMsg, errReply) {
        // No need to display each failure.
        console.error('ERRrepl is %o', errReply);
        var ts = getTimeStamp();
        var index = -1;
        // Match rep.head.rSeq with rmDatadirCommands[i].seq to determine which command returned!
        for (var i = 0; i < rmDatadirCommands.length; i++) {
            if (Number(rmDatadirCommands[i].seq) === Number(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        // This is needed because we call Python OS library to do the delete for us and it is not a
        // failure if directory we do not want to exist, doesn't exist.
        if (
            ( // It's Windows and allowed failure
                (JSON.stringify(errReply).indexOf('WindowsError') >= 0) &&
                ((errMsg.indexOf('[Error 2]') >= 0) || (errMsg.indexOf('[Error 3]') >= 0))
            ) || ( // It's *nix and allowed failure
                (JSON.stringify(errReply).indexOf('WindowsError') < 0) &&
                ((errMsg.indexOf('[Errno 2]') >= 0) || (errMsg.indexOf('[Errno 3]') >= 0))
            )
        ) {
            // 3: ERROR_PATH_NOT_FOUND & 2: ERROR_FILE_NOT_FOUND, real failure.
            console.debug('RMDIR, DIR does NOT exists');
            cmdLog.push(ts + 'DEPLOY ::SUCC::' + 'Removed(*) ' + rmDatadirCommands[index].path + '@' +
                rmDatadirCommands[index].host + '\n');
            console.info('%s[INF]DEPLOY ::SUCC::Removed(*) %s@%s.', ts, rmDatadirCommands[index].path,
                rmDatadirCommands[index].host);
            --howMany;
            updateProgressAndRemoveNextDatadir();
            return;
        }
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }
        console.warn('%s[ERR]DEPLOY ::FAIL::Removing %s@%s, %s.', ts, rmDatadirCommands[index].path,
            rmDatadirCommands[index].host, errMsg);
        cmdLog.push(ts + 'DEPLOY ::FAIL::' + 'Removing ' + rmDatadirCommands[index].path + '@' +
            rmDatadirCommands[index].host + ', ' + errMsg + '\n');
        --howMany;
        updateProgressAndRemoveNextDatadir();
    }

    function onRep (rep) {
        var ro = String(rep.body.out);
        var index = -1;
        // console.debug('rep is ' + JSON.stringify(rep));
        // Match rep.head.rSeq with rmDatadirCommands[i].seq to determine which command returned!
        for (var i = 0; i < rmDatadirCommands.length; i++) {
            if (Number(rmDatadirCommands[i].seq) === Number(rep.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        console.debug('%s RMDIR rep index is %i', ts, index);
        if (ro.indexOf('errcode:') === 0) {
            // Linux
            onErr(ro, rep);
            return;
        } else {
            cmdLog.push(ts + 'DEPLOY ::SUCC::' + 'Removed ' + rmDatadirCommands[index].path + '@' +
                rmDatadirCommands[index].host + '\n');
            console.info('%s[INF]DEPLOY ::SUCC::Removed %s@%s', ts, rmDatadirCommands[index].path,
                rmDatadirCommands[index].host);
        }
        // RM ended with success, wait next reply.
        --howMany;
        updateProgressAndRemoveNextDatadir();
    }

    function updateProgressAndRemoveNextDatadir () {
        if (howMany <= 0) {
            var message;
            if (rmDatadirCommands.length > 0) {
                message = errorReplies ? 'Removing DATADIR(s) procedure has completed, but ' + errorReplies +
                    ' out of ' + rmDatadirCommands.length + ' commands failed.' : 'DATADIR(s) removed successfully.';
                // We need to reset fileExists array after this operation:
                fileExists = [];
                var checkCmds = getCheckCommands();
                sendFileOps(checkCmds).then(function () {
                    console.info('[INF]' + message);
                    // Since we removed DATADIR(s), we know files are not there. Clean the flag in case
                    // RMDIR didn't fully finish before check was run.
                    for (var ii = 0; ii < fileExists.length; ii++) {
                        // console.log('fileExists[' + ii + ']=%o, setting to FALSE.', fileExists[ii]);
                        fileExists[ii].fileExist = false;
                    }
                    waitCondition.resolve(errorReplies <= 0);
                });
            } else {
                message = 'No DATADIR(s) to remove.';
                errorReplies = 0;
                console.info('[INF]' + message);
                waitCondition.resolve(true);
            }
        }
    }
    // Initiate start sequence by sending cmd to all mysqlds. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    if (howMany > 0) {
        moveProgress('Deploying cluster', 'Removing datadir(s)...');
        do {
            reqHead = mcc.server.getHead('dropStuffReq');
            // Remember SEQ number!
            rmDatadirCommands[t].seq = reqHead.seq;
            console.debug('[DBG]Sending RMDIR %s to %s', rmDatadirCommands[t].path, rmDatadirCommands[t].host);
            mcc.server.dropStuffReq('dropStuffReq', rmDatadirCommands[t].host, rmDatadirCommands[t].path, reqHead,
                onRep, onErr);
            ++t;
            if (t >= rmDatadirCommands.length) {
                t = -1;
                mcc.userconfig.setIsNewConfig(false);
            }
        } while (t >= 0);
    }
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndRemoveNextDatadir');
    updateProgressAndRemoveNextDatadir();
    return waitCondition;
}

/**
 *Main routine to deploy cluster: Create directories, distribute files, initialize mysqld's, open FW
 *
 * @param {Boolean} silent Show/Hide alerts during deployment
 * @param {Number} fraction How big progress bar is
 * @returns {dojo.Deferred}
 */
function deployCluster (silent, fraction) {
    dijit.hideTooltip(dojo.byId('configWizardDeployCluster'));
    dojo.byId('configWizardDeployCluster').blur();
    document.getElementById('startupDetails').focus();

    /*
    Essentially, we have 5 part waits:
        o cleanUpPreviousRunDatadirs: IF there is "Started" in Cluster lvl.
            configuration and user chooses to do so, this will remove all DATADIR(s)
            from all hosts (usually /home/MySQL_Cluster). Very useful if a) you need
            clean run, b) if nodes were moved around hosts and so on. Drawback; ALL
            data from Cluster is erased.
        o creating directories: sendFileOps is blocking for sendFileOp which has
            onreply/onerror for each type of file command. Starts with getCreateCommands.
        o distributing configuration files: distributeConfigurationFiles waits a
            list of sequential create and distribute files commands.
        o Opening FW ports.
        o Creating Windows services for processes. - REMOVED but will be back.
    */
    var clRunning = [];
    var what;
    clRunning = clServStatus();
    if (determineClusterRunning(clRunning)) {
        cancelButton('configWizardDeployCluster');
        what = mcc.userconfig.setCcfgPrGen.apply(this,
            mcc.userconfig.setMsgForGenPr('clRunning', ['deployCluster']));
        if ((what || {}).text) {
            console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
            mcc.util.displayModal('I', 3, what.text);
        }
        console.warn('Can\'t initiate deploy while Cluster is running!');
        return;
    }
    // First we need to be sure we have working connections to all hosts.
    var waitCd = new dojo.Deferred();
    var what1;
    checkAndConnectHosts(waitCd).then(function (ok) {
        if (!allHostsConnected) {
            what1 = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('hostsNotConn', ['deployCluster']));
            if ((what1 || {}).text) {
                console.warn(what1.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                mcc.util.displayModal('I', 0, what1.text);
            }
            cancelButton('configWizardDeployCluster');
            return;
        }
        var waitCondition = new dojo.Deferred();
        var uniqueH = [];
        var CntUniqHosts = -1;
        var SSHBl = '';
        // Get the create dirs&files commands
        var createCmd = getCreateCommands();
        // List into by-host array based on createCmd.host member.
        // Array of unique hosts. Of course, does NOT work in IE11...
        if (!window.MSInputMethodContext && !document.documentMode) {
            var CfgProcs = processTypeInstances('ndb_mgmd').concat(processTypeInstances('mysqld'));
            // Loop over all ndb_mgmds and form create file command
            for (var i in CfgProcs) {
                (function (i) {
                    var configFile = getConfigurationFile(CfgProcs[i]);
                    if (configFile.host && configFile.path && configFile.name) {
                        createCmd.push({
                            host: configFile.host,
                            path: configFile.path,
                            filename: configFile.name,
                            contents: configFile.msg,
                            overwrite: true
                        });
                    }
                })(i);
            }
            // Just to make IE syntax happy :-/
            for (i = 0; i < createCmd.length; i++) {
                if (uniqueH.indexOf(createCmd[i].host) === -1) {
                    uniqueH.push(createCmd[i].host);
                }
            }
            CntUniqHosts = uniqueH.length;
            // Reset global stuff.
            installProgress = 1;
            Workers = [];
            waitWorkers = [];
        } // There is no ELSE since IE11 can't handle promises so runs sequentially.
        // Prevent additional error messages (must have dropped this during coding of parallel run.)
        // var alerted = false;
        // Check if configuration is consistent
        var what = verifyConfiguration(false);
        if (what) {
            console.warn('[ERR]' + 'Failed to verify configuration.');
            mcc.util.displayModal('H', 0, '<span style="font-size:130%;color:orangered">' + what +
                '</span>', '<span style="font-size:140%;color:red">Configuration appears not valid!</span>');
            waitCondition.resolve(false);
            cancelButton('configWizardDeployCluster');
            return waitCondition;
        } else {
            // Parallel deploy from here.
            // Array of unique hosts uniqueH, CntUniqHosts = uniqueH.length;
            // Send SSH block and config files too.
            var checkCmds = getCheckCommands();
            // Make command array HOST: {port, port, ...} for each host. Pass for execution if wanted.
            moveProgress('Deploying cluster', 'Checking for previous deployment...');
            sendFileOps(checkCmds).then(function () {
                var doCleanUp = false;
                var inNeedOfInit = true;
                // Now we have fileExists array filled. Check that DEPLOY actually run:
                var ecSQL = false;
                var ecMGMT = false;
                for (var bb in fileExists) {
                    if (fileExists[bb].nodeid > 50) {
                        // SQL nodes initialized?
                        if (!isFirstStart(fileExists[bb].nodeid)) {
                            ecSQL = true;
                            break;
                        }
                    } else {
                        // Or MGMT node.
                        if (Number(fileExists[bb].nodeid) === 49) {
                            if (!isFirstStart(fileExists[bb].nodeid)) {
                                ecMGMT = true;
                            }
                        }
                    }
                }
                var wasStarted = (ecMGMT && ecSQL) ? 'MGMT node 49 and one SQL node were started ' : (ecMGMT)
                    ? 'At least MGMT node 49 was started ' : (ecSQL) ? 'At least one SQL node was started ' : '';
                if (ecMGMT || ecSQL) {
                    if (ecSQL) {
                        // We already have initialized (at least one) mysqld datadir.
                        inNeedOfInit = false;
                    }
                    doCleanUp = confirm(wasStarted + 'previously using same DATADIRs!\nIf you changed any parameter, ' +
                        'you need to delete old deployment.\nDo you wish to DELETE' +
                        ' all files and DATA from DATADIRs before new deployment?\n');
                }
                if (doCleanUp) {
                    // We are cleaning up so Init required.
                    inNeedOfInit = true;
                }
                cleanUpPreviousRunDatadirs(doCleanUp).then(function (ok) {
                    removeProgressDialog();
                    if (!ok) {
                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Something went ' +
                            'wrong with deleting DataDir(s)!<br/>Please check log of commands ' +
                            'and console.</span>');
                        waitCondition.resolve(false);
                        cancelButton('configWizardDeployCluster');
                        removeProgressDialog();
                        return waitCondition;
                    } else {
                        console.info('[INF]Cleaned up DATADIR(s)');
                    }
                    mcc.userconfig.setIsNewConfig(false);
                    var FWcommands = getOpenFWPortsCommands();
                    openFireWall(FWcommands, true).then(function (ok) {
                        if (!ok) {
                            console.warn('[WRN]Errors modifying firewall.');
                        }
                        // do we run on IE11?
                        if (!window.MSInputMethodContext && !document.documentMode) {
                            var tmpcons = [];
                            var worker = null;
                            for (var x = 0; x < CntUniqHosts; x++) {
                                // New deferred waiting for just this worker.
                                waitWorkers.push(new dojo.Deferred());
                                worker = new Worker('../js/mcc/worker/deployWorker.js');
                                worker.host = uniqueH[x];
                                worker.onmessage = handleMessageFromDeployWorker;
                                // exact worker is addressed as Workers[index]
                                Workers.push(worker);
                                // Send data to execute to worker. Nothing begins until START message is sent.
                                var hostName = String(uniqueH[x]);
                                SSHBl = formProperSSHBlock(hostName);
                                if (!SSHBl) {
                                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">No SSH block ' +
                                        'formed, can\'t continue.</span>');
                                    break;
                                }
                                // Get commands for this host only:
                                for (var i = 0; i < createCmd.length; i++) {
                                    if (String(createCmd[i].host) === String(uniqueH[x])) {
                                        tmpcons.push(createCmd[i]);
                                    }
                                }
                                // First message is used to send ALL commands and credentials to worker.
                                Workers[Workers.length - 1].postMessage(
                                    {
                                        cmd: 'receive',
                                        host: uniqueH[x],
                                        Id: x,
                                        SSHBlock: SSHBl,
                                        msg: tmpcons
                                    }
                                );
                                worker = null;
                                tmpcons = [];
                            }
                            // We have workers standing by.
                            updateProgressDialog('Deploying cluster',
                                'Starting workers.',
                                { maximum: createCmd.length + 1, progress: installProgress }, false);
                            // External wait condition
                            var waitAll = null;
                            waitAll = new dojo.DeferredList(waitWorkers);
                            for (var si = 0; si < Workers.length; si++) {
                                console.debug('[DBG]' + 'Starting workers.');
                                Workers[si].postMessage({ cmd: 'start', Id: si });
                            }
                            // Wait...
                            waitAll.then(function () {
                                removeProgressDialog();
                                console.info('[INF]' + 'Files deployed, initializing mysqld(s).');
                                var s = '';
                                initMySQLds(inNeedOfInit).then(function (ok) {
                                    if (!ok) {
                                        s = '(Some)mysqld(s) init failed. Please check web console (F12) or click ' +
                                        '<strong>View commands</strong> to see if mysqld(s) were already initialized.<br/>';
                                    }
                                    // remove existing windows services
                                    removeServices().then(function (ok) {
                                        removeProgressDialog();
                                        console.info('[INF]' + 'Deploy procedure ended.');
                                        if (cluster.getValue('MasterNode')) {
                                            s = s + '<br/><br/>Master host able to SSH to all other Cluster ' +
                                                '<br/><br/>hosts is ' + cluster.getValue('MasterNode') + '<br/>';
                                        }
                                        s = s + '<br/>Cluster deployment ended.<br/>';
                                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                                            s + '</span>');
                                        cluster.setValue('Started', false);
                                        mcc.userconfig.setCfgStarted(false);
                                        saveConfigToFile();
                                        // replace shadow with contents of stores since config is deployed
                                        mcc.userconfig.setOriginalStore('cluster');
                                        mcc.userconfig.setOriginalStore('host');
                                        mcc.userconfig.setOriginalStore('process');
                                        mcc.userconfig.setOriginalStore('processtype');
                                        cancelButton('configWizardDeployCluster');
                                        waitWorkers = [];
                                        Workers = [];
                                        waitCondition.resolve();
                                    });// RM services
                                });
                            });
                        } else {
                            // IE11 code, no promises thus no multitasking.
                            console.warn('[WRN]Running old, synchronous code. Consider using modern browser.');
                            // Show progress
                            updateProgressDialog('Deploying configuration',
                                'Creating directories',
                                { maximum: fraction ? fraction * createCmd.length : createCmd.length }, false);
                            console.info('[INF]' + 'Creating directories...');
                            // sendFileOps is blocking for sendFileOp which has onrReply/onerror for each type
                            // of file command.
                            sendFileOps(createCmd).then(function (ok) {
                                if (!ok) {
                                    console.error('[ERR]' + 'Aborting deployment due to previous error');
                                    waitCondition.resolve(false);
                                    removeProgressDialog();
                                    cancelButton('configWizardDeployCluster');
                                    return;
                                }
                                var progress = createCmd.length;
                                console.info('[INF]' + 'Directories created');
                                console.info('[INF]' + 'Distributing configuration files...');
                                updateProgressDialog('Deploying configuration',
                                    'Distributing configuration files', { progress: progress++ }, false);
                                // DistributeConfigurationFiles waits a list of sequential
                                // create files and distr. commands.
                                distributeConfigurationFiles().then(function () {
                                    console.info('[INF]' + 'Configuration files distributed');
                                    updateProgressDialog('Deploying configuration',
                                        'Configuration deployed', { progress: '100%' }, false);
                                    console.info('[INF]' + 'Configuration deployed, initializing mysqld(s).');
                                    var s = '';
                                    initMySQLds(inNeedOfInit).then(function (ok) {
                                        if (!ok) {
                                            s = '(Some)mysqld(s) init failed. Please check web console to see ' +
                                                'if mysqld(s) were already initialized.<br/>';
                                        }
                                        // First remove existing Windows services.
                                        removeServices().then(function (ok) {
                                            console.info('[INF]' + 'Deploy procedure ended.');
                                            removeProgressDialog();
                                            if (cluster.getValue('MasterNode')) {
                                                s += '<br/>Master host able to SSH to all other Cluster hosts is ' +
                                                    cluster.getValue('MasterNode') + '<br/>';
                                            }
                                            s += '<br/><br/>Cluster deployment ended.<br/>';
                                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                                                s + '</span>');
                                            cluster.setValue('Started', false);
                                            mcc.userconfig.setCfgStarted(false);
                                            saveConfigToFile();
                                            // replace shadow with contents of stores
                                            mcc.userconfig.setOriginalStore('cluster');
                                            mcc.userconfig.setOriginalStore('host');
                                            mcc.userconfig.setOriginalStore('process');
                                            mcc.userconfig.setOriginalStore('processtype');
                                            cancelButton('configWizardDeployCluster');
                                            waitCondition.resolve(true);
                                        });// RM services
                                    });// initMySQLds
                                });// distributeConfigurationFiles
                            });// sendFileOps
                        } // IE11
                    }); // Open FW
                }); // Remove datadirs.
            }); // file check cmds.
        }
        return waitCondition;
    });
}

/**
 *Support function to start mysqld processes in parallel.
 *
 * @returns {dojo.Deferred}
 */
function startMySQLds () {
    var wtCond = new dojo.Deferred();
    var doneStarting = false;
    var mysqldcommands = _getClusterCommands(getStartProcessCommands, ['sql']);
    // For these commands, isDone is Function, isCommand = False.
    var errorReplies = 0;
    var howMany = mysqldcommands.length;
    console.debug('[DBG]Total of %i SQL start commands generated', howMany);
    var tmOut;
    var reqHead;

    // Remember to resolve waitcondition!
    function onTimeout () {
        // Not a failure per se but rather waiting on startup.
        // This is where stuck commands end up.
        if (forceStop) {
            console.warn('[WRN]Cluster start aborted by user!');
            removeProgressDialog();
            wtCond.resolve(false);
            doneStarting = true;
            cancelButton('configWizardStartCluster');
            forceStop = false;
            // Notify user of the location of log files.
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                'Cluster start aborted! ' + displayLogFilesLocation(true) + '</span>');
            return;
        }
        if (howMany > 0) {
            for (var i = 0; i < mysqldcommands.length; i++) {
                if (mysqldcommands[i].isDone()) {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'STARTCL::SUCC::' + mysqldcommands[i].Actual + '\n');
                    console.debug('%s[DBG]STARTCL::SUCC::%s.', ts, mysqldcommands[i].Actual);
                    --howMany;
                }
            }
        }
        if (howMany > 0) {
            tmOut = setTimeout(onTimeout, 2000);
        } else {
            if (!doneStarting) {
                updateProgressAndStartNextMySQLd();
            } else { clearTimeout(tmOut); }
        }
    }

    function onErr (errMsg, errReply) {
        // No need to display each failure.
        console.error('[ERR]' + 'Error occurred while starting cluster SQL node : ' + errMsg);
        console.error('ERRrepl is %o', errReply);
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }
        var index = -1;
        // Match rep.head.rSeq with mysqldcommands[i].seq to determine which command returned!
        for (var i = 0; i < mysqldcommands.length; i++) {
            if (Number(mysqldcommands[i].seq) === Number(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        console.error('%s[ERR]STARTCL::FAIL::%s, %s', ts, mysqldcommands[index].Actual, errMsg);
        cmdLog.push(ts + 'STARTCL::FAIL::' + mysqldcommands[index].Actual + ', ' + errMsg + '\n');
        --howMany;
        updateProgressAndStartNextMySQLd();
    }

    function onRep (rep) {
        var ro = String(rep.body.out);
        // console.debug("rep is " + JSON.stringify(rep));
        if (ro.indexOf('errcode:') === 0) {
            onErr(ro, rep);
            return;
        }
        updateProgressAndStartNextMySQLd();
    }

    function updateProgressAndStartNextMySQLd () {
        if (doneStarting) { return; }
        if (howMany <= 0) {
            var message;
            if (mysqldcommands.length > 0) {
                message = errorReplies ? 'Start (SQL) procedure has completed, but ' + errorReplies + ' out of ' +
                    mysqldcommands.length + ' commands failed' : 'Cluster SQL nodes started successfully.';
            } else {
                message = 'No SQL nodes to start.';
                errorReplies = 0;
            }
            console.debug('[DBG]' + message);
            cancelButton('configWizardStartCluster');
            doneStarting = true; // This will clear timeout.
            wtCond.resolve(errorReplies <= 0);
            // return;
        } else {
            tmOut = setTimeout(onTimeout, 2000);
        }
    }
    // Initiate start sequence by sending cmd to all mysqlds. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    if (howMany > 0) {
        // moveProgressExecuted = true;
        moveProgress('Starting cluster', 'Starting SQL nodes...');
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            mysqldcommands[t].seq = reqHead.seq;
            console.debug('[DBG]Sending %s for execution.', mysqldcommands[t].Actual);
            mcc.server.doReq('executeCommandReq',
                { command: mysqldcommands[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= mysqldcommands.length) { t = -1; }
        } while (t >= 0);
    }
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndStartNextMySQLd');
    updateProgressAndStartNextMySQLd();
    return wtCond;
}
/**
 *Support function to start ndbmtd processes in parallel.
 *
 * @returns {dojo.Deferred}
 */
function startDNodes () {
    var wtCond = new dojo.Deferred();
    var doneStarting = false;
    var ndbmtdcommands = _getClusterCommands(getStartProcessCommands, ['data']);
    var errorReplies = 0;
    var howMany = ndbmtdcommands.length;
    var tmOut;
    var reqHead;
    function onTimeout () {
        // Not a failure per se but rather waiting on startup.
        // This is where stuck commands end up and where user-cancell is processed..
        if (forceStop) {
            console.warn('[WRN]Cluster start of DNodes aborted!');
            removeProgressDialog();
            wtCond.resolve(false);
            doneStarting = true;
            cancelButton('configWizardStartCluster');
            // Notify user of the location of log files.
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                'Cluster start aborted! ' + displayLogFilesLocation(true) + '</span>');
            forceStop = false;
            return;
        }
        if (howMany > 0) {
            for (var i = 0; i < ndbmtdcommands.length; i++) {
                if (ndbmtdcommands[i].isDone()) {
                    var ts = getTimeStamp();
                    cmdLog.push(ts + 'STARTCL::SUCC::' + ndbmtdcommands[i].Actual + '\n');
                    console.debug('[DBG]%sSTARTCL::SUCC::%s.', ts, ndbmtdcommands[i].Actual);
                    --howMany;
                }
            }
        }
        if (howMany > 0) {
            tmOut = setTimeout(onTimeout, 2000);
        } else {
            if (!doneStarting) {
                updateProgressAndStartNextndbmtd();
            } else { clearTimeout(tmOut); }
        }
    }

    function onErr (errMsg, errReply) {
        // No need to display each failure.
        console.error('[ERR]Error occurred while starting cluster DATA node : ' + errMsg);
        // console.error('ERRrepl is %o', errReply);
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }

        var index = -1;
        // Match rep.head.rSeq with ndbmtdcommands[i].seq to determine which command returned!
        for (var i = 0; i < ndbmtdcommands.length; i++) {
            if (Number(ndbmtdcommands[i].seq) === Number(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        console.debug('%s[ERR]STARTCL::FAIL::%s, %s.', ts, ndbmtdcommands[index].Actual, errMsg);
        cmdLog.push(ts + 'STARTCL::FAIL::' + ndbmtdcommands[index].Actual + ', ' +
            errMsg + '\n');
        --howMany;
        updateProgressAndStartNextndbmtd();
    }

    function onRep (rep) {
        var ro = String(rep.body.out);
        if (ro.indexOf('errcode:') === 0) {
            onErr(ro, rep);
            return;
        }
        updateProgressAndStartNextndbmtd();
    }

    function updateProgressAndStartNextndbmtd () {
        if (doneStarting) { return; }
        if (howMany <= 0) {
            var message;
            if (ndbmtdcommands.length > 0) {
                message = errorReplies ? 'Start (DNODE) procedure has completed, but ' + errorReplies + ' out of ' +
                    ndbmtdcommands.length + ' commands failed' : 'Cluster DATA nodes started successfully.';
            } else {
                message = 'No DNODE(s) nodes to start.';
                errorReplies = 0;
            }
            console.debug('[DBG]' + message);
            cancelButton('configWizardStartCluster');
            doneStarting = true;
            wtCond.resolve(errorReplies <= 0);
            // return;
        } else {
            tmOut = setTimeout(onTimeout, 2000);
        }
    }
    // Initiate start sequence by sending cmd to all ndbmtds. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    if (howMany > 0) {
        moveProgress('Starting cluster',
            'Starting DATA nodes...');
        do {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            ndbmtdcommands[t].seq = reqHead.seq;
            console.debug('[DBG]Sending %s for execution.', ndbmtdcommands[t].Actual);
            mcc.server.doReq('executeCommandReq',
                { command: ndbmtdcommands[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= ndbmtdcommands.length) { t = -1; }
        } while (t >= 0);
    }
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndStartNextndbmtd');
    updateProgressAndStartNextndbmtd();
    return wtCond;
}
/**
 *Main procedure to Start cluster processes. Split into:
 Firewall: openFireWall(OpenFWcmds)
 File check: sendFileOps(checkCmds)
 start MGMT(s) sequentially but with --no-wait-nodes.
 start DNode(s) in parallel.
 start SQL(s) in parallel.
We need to UPDATE sendFileOps(checkCmds) after start as that's when MGMT node bin file is created.
 *
 * @returns {dojo.Deferred}
 */
function startCluster () {
    dijit.hideTooltip(dojo.byId('configWizardStartCluster'));
    dojo.byId('configWizardStartCluster').blur();
    document.getElementById('startupDetails').focus();
    var clRunning = [];
    var what;
    clRunning = clServStatus();
    if (determineClusterRunning(clRunning)) {
        cancelButton('configWizardStartCluster');
        what = mcc.userconfig.setCcfgPrGen.apply(this,
            mcc.userconfig.setMsgForGenPr('clRunning', ['startCluster']));
        if ((what || {}).text) {
            console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
            mcc.util.displayModal('I', 3, what.text);
        }
        console.warn('Cluster is already running!');
        return;
    }
    // Check if configuration is consistent
    var what1 = verifyConfiguration(true);
    if (what1) {
        console.warn('[ERR]' + 'Failed to verify configuration.');
        mcc.util.displayModal('H', 0, '<span style="font-size:130%;color:orangered">' + what1 +
            '</span>', '<span style="font-size:140%;color:red">Configuration appears not valid!</span>');
        cancelButton('configWizardStartCluster');
        return;
    }

    // First we need to be sure we have working connections to all hosts.
    var waitCd = new dojo.Deferred();
    checkAndConnectHosts(waitCd).then(function (ok) {
        if (!allHostsConnected) {
            what = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr('hostsNotConn', ['deployCluster']));
            if ((what || {}).text) {
                console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                mcc.util.displayModal('I', 0, what.text);
            }
            cancelButton('configWizardStartCluster');
            return;
        }
        // External wait condition for entire Start.
        var waitCondition = new dojo.Deferred();
        var timeout = null;
        var mysqldStartErrorsExist = false;
        var dnodeStartErrorsExist = false;
        // mcc.gui.setPollTimeOut(2000); PUT BACK!!!
        mcc.gui.setPollTimeOut(5000);
        /* For commands formed below, host is determined by looking into MSG:
        var msg = {
            head: getHead(reqName),
            body: body
        };
        hostName_fromBody = body['command']['file'].hostName;
        */

        // We are on...
        moveProgress('Starting cluster', 'Checking configuration files...');
        // Check for files. If file exists, initialization will be skipped
        var checkCmds = getCheckCommands();
        // Make command array HOST: {port, port, ...} for each host. Pass for execution if wanted.
        sendFileOps(checkCmds).then(function () {
            console.info('[INF]START cluster, checked files.');
            var errCond = false;
            // Now we have fileExists array filled. Check that DEPLOY actually run:
            for (var bb in fileExists) {
                // Skip MGMT and DATA nodes.
                if (fileExists[bb].nodeid > 50) {
                    // SQL nodes initialized?
                    if (isFirstStart(fileExists[bb].nodeid)) {
                        errCond = true;
                        mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red"></span>Aborting ' +
                            'START as SQL node ' + fileExists[bb].nodeid + ' is not initialized!' +
                            '<br/>Please run DEPLOY!');
                        break;
                    }
                }
            }
            if (errCond) {
                console.error('[ERR] errorcond from START.');
                removeProgressDialog();
                cancelButton('configWizardStartCluster');
                waitCondition.resolve();
                return;
            }
            mcc.userconfig.setIsNewConfig(false);
            var FWcommands = getOpenFWPortsCommands();
            openFireWall(FWcommands, true).then(function (ok) {
                if (!ok) {
                    console.info('[INF]' + 'Errors modifying firewall.');
                }
                console.info('[INF]' + 'Starting cluster, preparing services...');
                // could be we have old services
                removeServices().then(function (ok) {
                    // so we install fresh
                    installServices().then(function (ok) {
                        // and modify to allow for manual start
                        modifyServices().then(function (ok) {
                            console.info('[INF]' + 'Starting cluster...');
                            var commands = _getClusterCommands(getStartProcessCommands, ['management']);
                            console.debug('[DBG]Total start MGMT commands generated ' + commands.length);
                            // console.debug([DBG]Start process commands for MGMT processes: %o', commands);
                            var currSeq = 0;
                            function onTimeout () {
                                // THIS will not work without noWait list for 1st mgmt node
                                // if there are more than one mgmt nodes in Cluster.
                                if (commands[currSeq].isDone()) {
                                    var ts = getTimeStamp();
                                    cmdLog.push(ts + 'STARTCL::SUCC::' + commands[currSeq].Actual + '\n');
                                    ++currSeq;
                                    updateProgressAndStartNext();
                                } else {
                                    // Not a failure per se but rather waiting on startup.
                                    // This is where stuck commands end up.
                                    clRunning = clServStatus();
                                    if (forceStop) {
                                        console.warn('[WRN]Cluster start aborted!');
                                        removeProgressDialog();
                                        waitCondition.resolve();
                                        cancelButton('configWizardStartCluster');
                                        // Notify user of the location of log files.
                                        mcc.util.displayModal('I', 0,
                                            '<span style="font-size:140%;color:orangered">' +
                                            'Cluster start aborted! ' +
                                            displayLogFilesLocation(true) + '</span>');
                                        forceStop = false;
                                        return;
                                    }
                                    timeout = setTimeout(onTimeout, 2000);
                                }
                            }
                            function onError (errMsg) {
                                removeProgressDialog();
                                var ts = getTimeStamp();
                                cmdLog.push(ts + 'STARTCL::FAIL::' + commands[currSeq].Actual + ':' + errMsg + '\n');
                                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Command failed<br/>' +
                                    commands[currSeq].Actual + ':' + errMsg + '<br/></span>');
                                clRunning = clServStatus();
                                // Notify user of the location of log files.
                                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                                    displayLogFilesLocation(true) + '</span>');
                                clearTimeout(timeout);
                                waitCondition.resolve();
                                cancelButton('configWizardStartCluster');
                            }
                            function onReply (rep) {
                                console.debug('[DBG]Got reply for: ' + commands[currSeq].Actual);
                                // Start status polling timer after mgmd has been started
                                // Ignore errors since it may not be available right away
                                if (currSeq === 0) {
                                    mcc.gui.startStatusPoll(false);
                                }
                                var ro = String(rep.body.out);
                                if (ro.indexOf('errcode:') === 0) {
                                    onError(ro, rep, false);
                                    return;
                                }
                                onTimeout();
                            }
                            function updateProgressAndStartNext () {
                                if (currSeq >= commands.length) {
                                    console.info('[INF]Trying to start DATA nodes now.');
                                    startDNodes().then(function (ok) {
                                        if (!ok) {
                                            dnodeStartErrorsExist = true;
                                            console.error('[ERR]Starting data nodes did not fully succeed.');
                                        }
                                        console.info('[INF]Trying to start SQL nodes now.');
                                        startMySQLds().then(function (ok) {
                                            if (!ok) {
                                                mysqldStartErrorsExist = true;
                                                console.error("[ERR]Starting mysqld's did not fully succeed.");
                                            }
                                            removeProgressDialog();
                                            mcc.util.displayModal('I', ok ? 3 : 0, ok
                                                ? '<span style="font-size:140%;color:teal">Cluster started!</span>'
                                                : '<span style="font-size:140%;color:darkorange">' +
                                                'Cluster started but there were errors!</span>');
                                            clearTimeout(timeout);
                                            // Since Cluster started, we know files are there.
                                            for (var ii = 0; ii < fileExists.length; ii++) {
                                                fileExists[ii].fileExist = true;
                                            }
                                            cancelButton('configWizardStartCluster');
                                            // IF Cluster starts, we can safely assume all hosts have SW installed.
                                            for (var h = 0; h < hosts.length; h++) {
                                                if (!hosts[h].getValue('anyHost')) {
                                                    hosts[h].setValue('SWInstalled', true);
                                                }
                                            }
                                            cluster.setValue('Started', true);
                                            mcc.userconfig.setCfgStarted(true);
                                            saveConfigToFile();
                                            // replace shadow with contents of stores
                                            mcc.userconfig.setOriginalStore('cluster');
                                            mcc.userconfig.setOriginalStore('host');
                                            mcc.userconfig.setOriginalStore('process');
                                            mcc.userconfig.setOriginalStore('processtype');
                                            clRunning = clServStatus();
                                            if (!determineAllClusterProcessesUp(clRunning) ||
                                                mysqldStartErrorsExist || dnodeStartErrorsExist) {
                                                // Notify user of the location of log files.
                                                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                                                    displayLogFilesLocation(true) + '</span>');
                                            }
                                            waitCondition.resolve();
                                        });
                                    });
                                }
                                clRunning = clServStatus();
                                if (forceStop) {
                                    console.warn('[WRN]Cluster start aborted!');
                                    removeProgressDialog();
                                    clearTimeout(timeout);
                                    waitCondition.resolve();
                                    cancelButton('configWizardStartCluster');
                                    forceStop = false;
                                    // Notify user of the location of log files.
                                    mcc.util.displayModal('I', 0,
                                        '<span style="font-size:140%;color:orangered">' +
                                        'Cluster start aborted! ' +
                                        displayLogFilesLocation(true) + '</span>');
                                    return;
                                }
                                if (currSeq < commands.length) {
                                    console.debug('[DBG]commands[%i].progTitle: %s', currSeq,
                                        commands[currSeq].Actual);
                                    updateProgressDialog('Starting cluster',
                                        commands[currSeq].progTitle,
                                        { maximum: commands.length, progress: currSeq + 1 }, false);
                                    mcc.server.doReq('executeCommandReq',
                                        { command: commands[currSeq].msg }, '', cluster, onReply, onError);
                                }
                            }
                            // Initiate startup sequence by calling onReply
                            updateProgressAndStartNext();
                        }); // modify services
                    }); // install services
                }); // remove services
            });
        });
        return waitCondition;
    });
}
/**
 *Utility function to collect log files location depending on Cluster configuration
 *
 * @param {Boolean} doDisplay Return entire message/just list of directories of interest
 * @returns {String}
 */
function displayLogFilesLocation (doDisplay) {
    var processesOnHost = [];
    // It has served the purpose...
    forceStop = false;
    for (var p in processes) {
        if (!processesOnHost[processes[p].getValue('host')]) {
            processesOnHost[processes[p].getValue('host')] = [];
        }
        // Append process to array
        processesOnHost[processes[p].getValue('host')].push(processes[p]);
    }
    // Do search for each host individually
    var dirs = [];
    var dir = '';
    for (var h = 0; h < hosts.length; h++) {
        var hostId = hosts[h].getId();
        var hostName = hosts[h].getValue('name');
        // One loop
        for (p in processesOnHost[hostId]) {
            // Process instance
            var proc = processesOnHost[hostId][p];
            // All processes except api have datadir
            if (processTypes[proc.getValue('processtype')].getValue('name').toLowerCase() !== 'api') {
                dir = hostName + ':' + processTypes[proc.getValue('processtype')].getValue('name') +
                    ': ' + getEffectiveInstanceValue(proc, 'DataDir');
                dirs.push(dir);
            }
        }
    }
    if (dirs.length > 0) {
        // Here we can fetch files to localhost for examination. Add to API unit.
        if (doDisplay) {
            var msg = 'Please check log files in following locations for more clues:\n';
            for (var dir1 = 0; dir1 < dirs.length; dir1++) {
                msg += dirs[dir1] + '<br/>';
            }
            return msg;
        } else {
            // Return the list.
            return dirs;
        }
    } else {
        if (doDisplay) {
            return 'Unable to collect log files location!';
        } else { return dirs; }
    }
}
/**
 *Utility function to stop mysqld processes over hosts. If process is dead, it will be skipped.
 *
 * @param {Boolean} oldForceStop user requested abort
 * @param {Boolean} forceShutdownSQL do we force creating stop commands even if process is down
 * @returns {dojo.Deferred}
 */
function stopMySQLd (oldForceStop, forceShutdownSQL) {
    var waitCondition = new dojo.Deferred();
    var mysqldComm = _getClusterCommands(getStopProcessCommands, ['sql'], forceShutdownSQL);
    var errorReplies = 0;
    var howMany = mysqldComm.length;
    var reqHead;
    console.debug('stopMySQLd, mysqldcommands=' + mysqldComm.length);

    function onErr (errMsg, errReply) {
        // No need to display each failure since it's perfectly possible service
        // was not even started. console.error('ERRrepl is %o', errReply);
        console.error('[ERR]Error occurred while stopping cluster SQL node : %s' +
            '\nTrying to kill also failed.', errMsg);
        ++errorReplies;
        var cwpb = dijit.byId('configWizardProgressBar');
        if (cwpb) {
            var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
            visualTile.style.backgroundColor = '#FF3366';
        }

        var index = -1;
        // Match rep.head.rSeq with mysqldcommands[i].seq to determine which command returned!
        for (var i = 0; i < mysqldComm.length; i++) {
            if (Number(mysqldComm[i].seq) === Number(errReply.head.rSeq)) {
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        console.debug('%s[ERR]STOPTCL::FAIL::%s, %s. KILL failed too, process still running.\n',
            ts, mysqldComm[index].Actual, errMsg);
        cmdLog.push(ts + 'STOPTCL::FAIL::' + mysqldComm[index].Actual + ', ' +
            errMsg + '. KILL failed too, process still running\n');
        --howMany;
        updateProgressAndStopNextMySQLd();
    }

    function onRep (rep) {
        var cc;
        var index = -1;
        var ro = String(rep.body.out);
        // console.debug("rep is " + JSON.stringify(rep));
        // Match rep.head.rSeq with mysqldcommands[i].seq to determine which command returned!
        for (var i = 0; i < mysqldComm.length; i++) {
            if (Number(mysqldComm[i].seq) === Number(rep.head.rSeq)) {
                cc = mysqldComm[i];
                index = i;
                break;
            }
        }
        var ts = getTimeStamp();
        // WRONG! Back end should emphasize KILL if there was one.
        if (ro.indexOf('terminated.') !== -1 || !ro) {
            // Unix returns nothing when KILL succeeds.
            // command.Actual failed but goal was met (i.e. process stopped).
            cmdLog.push(ts + 'STOPTCL::SUCC::' + mysqldComm[index].Actual + ', process ended\n');
            console.debug('%s[DBG]STOPTCL::SUCC::%s, process ended.', ts, mysqldComm[index].Actual);
        } else {
            if (ro.indexOf('errcode:') === 0) {
                onErr(ro, rep);
                return;
            } else {
                cmdLog.push(ts + 'STOPTCL::SUCC::' + mysqldComm[index].Actual + '\n');
                console.debug('%s[INF]STOPTCL::SUCC::%s.', ts, mysqldComm[index].Actual);
            }
        }

        if (mysqldComm[index].isDone instanceof Function) {
            console.debug('[DBG]FUNCTION rep[%i] isDone %s', index, mysqldComm[index].isDone());
        } else {
            if (cc.msg.isCommand) {
                // Only for --remove which is no more!
                var result = cc.check_result(rep);
                if (result.toLowerCase() === 'retry') {
                    console.debug('[DBG]Retrying: ' + mysqldComm[index].Actual);
                    mcc.server.doReq('executeCommandReq',
                        { command: cc.msg }, reqHead, cluster, onRep, onErr);
                    return;
                }
                if (result.toLowerCase() === 'error') {
                    cc.isDone = true;
                    onErr(ro, rep);
                    return;
                }
            } else {
                mysqldComm[index].isDone = true;
            }
        }
        // STOP ended with success, wait next reply.
        --howMany;
        updateProgressAndStopNextMySQLd();
    }

    function updateProgressAndStopNextMySQLd () {
        if (howMany <= 0) {
            var message;
            if (mysqldComm.length > 0) {
                message = errorReplies ? 'Stop procedure has completed, but ' + errorReplies + ' out of ' +
                    mysqldComm.length + ' commands failed' : 'Cluster SQL nodes stopped successfully.';
            } else {
                message = 'No SQL nodes to stop.';
                errorReplies = 0;
            }
            console.debug('[DBG]' + message);
            cancelButton('configWizardStopCluster');
            waitCondition.resolve(errorReplies <= 0);
        }
    }
    // Initiate stop sequence by sending shutdown to all mysqlds. Synchronize
    // via SEQ of command that BE will return to us.
    var t = 0;
    var doneIs = false;
    do {
        // Maybe SQL nodes are stopped while Cluster nodes aren't.
        try {
            // console.debug('STOP mysqld CMD is: ' + mysqldcommands[t].Actual);
            if (mysqldComm[t].isDone instanceof Function) {
                doneIs = mysqldComm[t].isDone();
            } else {
                doneIs = mysqldComm[t].isDone;
            }
        } catch (e) {
            // Uninitialized command. I.e. one out of 2 SQL nodes wasn't even started.
            console.warn('[WRN]Already stopped SQL process found at ' + t + '!');
            doneIs = true;
        }
        if (!doneIs) {
            reqHead = mcc.server.getHead('executeCommandReq');
            // Remember SEQ number!
            // console.debug('NOT doneIs for Command[%i].msg is %o', t, mysqldcommands[t].msg);
            mysqldComm[t].seq = reqHead.seq;
            mcc.server.doReq('executeCommandReq',
                { command: mysqldComm[t].msg }, reqHead, cluster, onRep, onErr);
            ++t;
            if (t >= mysqldComm.length) { t = -1; }
        } else {
            // Why stop nodes that aren't up.
            // console.debug('[DBG]doneIs for Command[%i]', t);
            mysqldComm.splice(t, 1);
            --howMany;
            // console.debug('[DGB]doneIs length of arr %i, howM %i', mysqldcommands.length, howMany);
            if (t >= mysqldComm.length) {
                t = -1;
            } else {
                if (howMany <= 0 && mysqldComm.length <= 0) { t = -1; }
            }
        }
    } while (t >= 0);
    // Check for finish.
    console.debug('[DBG]Starting updateProgressAndStopNextMySQLd');
    updateProgressAndStopNextMySQLd();
    return waitCondition;
}

/**
 *Main procedure to Stop cluster processes. Happens in stages:
 * stop mysqld processes in parallel. if any is stopped, skip over it. if stop fails, kill the process
 * issue ndb_mgm -e'SHUTDOWN' on primary management node. if it fails, do same on 2nd management node
 * (if exists), if it fails, kill all management and data node processes.
 *
 * Stop can be issued on partially started Cluster too.
 *
 * @returns (dojo.Deferred)
 */
function stopCluster () {
    dijit.hideTooltip(dojo.byId('configWizardStopCluster'));
    dojo.byId('configWizardStopCluster').blur();
    document.getElementById('startupDetails').focus();

    var clRunning = [];
    var mysqldStopErrorsExist = false;
    var totalMgmds = Number(mcc.gui.getMgmtArraySize());
    var rmSrvArr = [];
    var what;
    var forceStopCommands = false;

    clRunning = clServStatus();
    if (!determineClusterRunning(clRunning)) {
        // In unlikely case where some stray process we did not catch is actually running.
        if (!confirm('Cluster appears to be stopped.\nRun STOP commands anyway?')) {
            cancelButton('configWizardStopCluster');
            return;
        }
        forceStopCommands = true;
    }
    // First we need to be sure we have working connections to all hosts.
    var waitCd = new dojo.Deferred();
    checkAndConnectHosts(waitCd).then(function (ok) {
        if (!allHostsConnected) {
            what = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('hostsNotConn', ['stopCluster']));
            if ((what || {}).text) {
                console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                mcc.util.displayModal('I', 0, what.text);
            }
        }

        // Resetting this when displaying log files locations.
        // So, if START was interrupted, it was reset to FALSE before ending up here.
        var oldForceStop = forceStop;
        // External wait condition
        var waitCondition = new dojo.Deferred();
        console.info('[INF]' + 'Stopping cluster...');

        // Cluster has started, files are for sure there. Reset flags.
        if (cluster.getValue('Started') === true) {
            for (var ii = 0; ii < fileExists.length; ii++) {
                fileExists[ii].fileExist = true;
            }
        }
        // Just continue previous startStatusPoll(false); so to avoid problems with
        //  a) external to MCC startup of processes
        //  b) SSH management connection expiring.
        moveProgress('Stopping cluster', 'Issuing SHUTDOWN command(s) to SQL process(es).');
        // Call stopMySQLd, chain the rest.
        stopMySQLd(oldForceStop, forceStopCommands).then(function (ok) {
            // MGMT is just ndb_mgm SHUTDOWN on primary node. DNodes, if on Windows
            // get --remove commands but in separate execution array.
            // IF SHUTDOWN failed on primary node (40) we will issue KILL there. If we
            // went to KILL exec.path then we need to pick up getKillProcessCommands
            // which will contain ndb_mgm SHUTDOWN for secondary MGMT node (50) IF it's there +
            // NET STOP + KILL for DNodes on Windows or just KILL for DNodes on Unix if necessary.
            var commands = _getClusterCommands(getStopProcessCommands, ['management', 'data'], forceStopCommands);
            // We have stopped mysqlds, now, after stopping Cluster with
            // ndb_mgm SHUTDOWN we can remove Windows services too, if any.
            var currSeq = 0;
            var errorReplies = 0;
            var timeout;
            var fullDone = false;
            // failsafe, do not want to wait too long so getting current time.
            var t0 = performance.now();
            if (!ok) {
                console.warn('[WRN]Stopping mysqld\'s did not fully succeed.');
                mysqldStopErrorsExist = true;
            } else {
                console.info('[INF]Stopped mysqld\'s.');
            }
            function onTimeout () {
                var doneIs = false;
                // if we are on kill execpath, ClRunning will be false (we killed MGMT process)
                fullDone = !(determineClusterRunning(clServStatus())) && (currSeq >= commands.length);
                console.debug('[DBG]onTimeout, fullDone %s, %d/%d', fullDone, currSeq, commands.length);
                if (!fullDone) {
                    console.debug('[DBG]onTimeout, not fullDone');
                    // wait 100 seconds max!
                    if (performance.now() - t0 > 100000) {
                        console.warn('Tired of waiting on STOP to complete');
                        fullDone = true;
                    } else {
                        if (Math.floor(((performance.now() - t0) / 1000)) % 5 === 0 &&
                            (currSeq >= commands.length)) {
                            // notify every 5 seconds
                            console.info('[INF]Will wait ' + (Math.floor((100000 + t0 - performance.now()) / 1000)) +
                                's more for Cluster to fully stop.');
                            moveProgress(
                                'Stopping cluster' + (errorReplies ? ' (' + errorReplies + ' failed command(s))' : ''),
                                'waiting on full stop for ' + (Math.floor((100000 + t0 - performance.now()) / 1000)) +
                                ' more sec.'
                            );
                        }
                    }
                }
                if (fullDone) {
                    clearTimeout(timeout);
                    currSeq = commands.length;
                    updateProgressAndStopNext();
                } else {
                    if (currSeq < commands.length) {
                        try {
                            if (commands[currSeq].isDone instanceof Function) {
                                doneIs = commands[currSeq].isDone();
                            } else {
                                doneIs = commands[currSeq].isDone;
                            }
                        } catch (e) {
                            // Uninitialized command. I.e. one out of 2 DATA nodes wasn't even started.
                            console.warn('[WRN]Uninitialized data node process found at %i!', currSeq);
                            doneIs = true;
                        }
                        if (doneIs) {
                            var ts = getTimeStamp();
                            cmdLog.push(ts + 'STOPTCL::SUCC::' + commands[currSeq].Actual + '\n');
                            ++currSeq; // this one was growing unchecked...
                            updateProgressAndStopNext();
                        } else {
                            timeout = setTimeout(onTimeout, 1000);
                        }
                    } else {
                        if (!fullDone) {
                            timeout = setTimeout(onTimeout, 1000);
                        };
                    }
                }
            }
            function onError (errMsg, errReply) {
                var ts = getTimeStamp();
                cmdLog.push(ts + 'STOPTCL::FAIL::' + commands[currSeq].Actual + ':' + errMsg + '\n');
                // No need to display each failure since it's perfectly possible process
                // was not even started. console.error('ERRrepl is %o', errReply);
                console.error('[ERR]Error occurred while stopping Cluster: ' + errMsg);
                ++errorReplies;
                var cwpb = dijit.byId('configWizardProgressBar');
                if (cwpb) {
                    var visualTile = dojo.query('.dijitProgressBarTile', cwpb.domNode)[0];
                    visualTile.style.backgroundColor = '#FF3366';
                }
                if ((currSeq === 0 && totalMgmds === 1) || (currSeq === 1 && totalMgmds > 1)) {
                    // This is bad. (Both) ndb_mgm failed to stop Cluster and (both)
                    // KILL ndb_mgmd PID failed. Kill what can be killed.
                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands, ['data']));
                    Array.prototype.push.apply(commands, rmSrvArr);
                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                }
                ++currSeq;
                updateProgressAndStopNext();
            }
            function onReply (rep) {
                if (commands[currSeq]) {
                    console.debug('[DBG]Got reply for: ' + commands[currSeq].Actual);
                } else {
                    console.debug('[DBG]Got reply for seq %i but command is already null.', currSeq);
                    cancelButton('configWizardStopCluster'); // ?
                    return;
                }
                var ts = getTimeStamp();
                var cc = commands[currSeq];
                // check if isCommand member is set
                if (cc.msg.isCommand) {
                    // Only Windows::Service::remove are treated as commands. However,
                    // this is now separated to new function.
                    var result = cc.check_result(rep);
                    console.debug('[DBG]%s treated as CMD, result is %s.', commands[currSeq].Actual,
                        result);
                    if (result.toLowerCase() === 'retry') {
                        console.debug('[DBG]Retrying: ' + commands[currSeq].Actual);
                        setTimeout(updateProgressAndStopNext, 2000);
                        return;
                    }
                    if (result.toLowerCase() === 'error') {
                        cc.isDone = true;
                        onError(String(rep.body.out), rep);
                        cancelButton('configWizardStopCluster'); // ?
                        return;
                    }
                    cc.isDone = true;
                } else {
                    console.debug('[DBG]%s not treated as CMD.', cc.progTitle);
                    cc.isDone = true;
                    var ro = String(rep.body.out);
                    if (currSeq === 0) {
                        // First, check if KILLED
                        if ('servicename' in cc.msg.procCtrl) {
                            // Windows
                            if (ro.indexOf('terminated.') !== -1) {
                                // Killed.
                                cmdLog.push(ts + 'STOPTCL::SUCC::' + cc.Actual + ', process was killed\n');
                                console.info('[INF]' + 'STOPTCL::SUCC::' + cc.Actual + ', process was killed.');
                                if (totalMgmds > 1) {
                                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands,
                                        ['management']));
                                } else {
                                    // Nothing else to do but KILL DNode processes.
                                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands,
                                        ['data']));
                                    Array.prototype.push.apply(commands, rmSrvArr);
                                }
                            } else {
                                // console.debug("rmSrvArr is %o", rmSrvArr);
                                Array.prototype.push.apply(commands, rmSrvArr);
                            }
                        } else {
                            // Unix
                            if (!ro || ro === 'No output from command.') {
                                // Killed.
                                cmdLog.push(ts + 'STOPTCL::SUCC::' + cc.Actual + ', process ended\n');
                                console.info('[INF]' + 'STOPTCL::SUCC::' + cc.Actual + ', process ended.');
                                if (totalMgmds > 1) {
                                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands,
                                        ['management']));
                                } else {
                                    // Nothing else to do but KILL DNode processes.
                                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands,
                                        ['data']));
                                    Array.prototype.push.apply(commands, rmSrvArr);
                                }
                            } else {
                                // console.debug("[DBG]rmSrvArr is %o", rmSrvArr);
                                Array.prototype.push.apply(commands, rmSrvArr);
                            }
                        }
                    } else {
                        if ((currSeq === 1) && (totalMgmds > 1)) {
                            // Is it killed?
                            if ('servicename' in cc.msg.procCtrl) {
                                // Windows
                                if (ro.indexOf('terminated.') !== -1) {
                                    // Killed. So even 2nd ndb_mgm failed, we need to clean up manually.
                                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                                    console.info('[INF]' + 'STOPTCL::SUCC::' + cc.Actual + ', process was killed.');
                                    cmdLog.push(ts + 'STOPTCL::SUCC::' + cc.Actual + ', process was killed\n');
                                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands,
                                        ['data']));
                                    Array.prototype.push.apply(commands, rmSrvArr);
                                } else {
                                    // 2nd ndb_mgm succeeded, just remove services if any.
                                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                                    Array.prototype.push.apply(commands, rmSrvArr);
                                }
                            } else {
                                // Unix
                                if (!ro || ro === 'No output from command.') {
                                    // Killed. So even 2nd ndb_mgm failed, we need to clean up manually.
                                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                                    console.info('[INF]' + 'STOPTCL::SUCC::' + cc.Actual + ', process ended.');
                                    cmdLog.push(ts + 'STOPTCL::SUCC::' + cc.Actual + ', process ended\n');
                                    Array.prototype.push.apply(commands, _getClusterCommands(getKillProcessCommands,
                                        ['data']));
                                    Array.prototype.push.apply(commands, rmSrvArr);
                                } else {
                                    // 2nd ndb_mgm succeeded, just remove services if any.
                                    console.debug('[DBG]rmSrvArr is %o', rmSrvArr);
                                    Array.prototype.push.apply(commands, rmSrvArr);
                                }
                            }
                        } else {
                            // Other than ndb_mgm commands.
                            if ('servicename' in cc.msg.procCtrl) {
                                // Windows
                                if (ro.indexOf('terminated.') !== -1) {
                                    // Killed. So even 2nd ndb_mgm failed, we need to clean up manually.
                                    console.info('[INF]' + 'STOPTCL::SUCC::' + cc.Actual + ', process was killed.');
                                    cmdLog.push(ts + 'STOPTCL::SUCC::' + cc.Actual + ', process was killed\n');
                                }
                            } else {
                                // Unix
                                if (!ro || ro === 'No output from command.') {
                                    // Killed. So even 2nd ndb_mgm failed, we need to clean up manually.
                                    console.info('[INF]' + 'STOPTCL::SUCC::' + cc.Actual + ', process ended.');
                                    cmdLog.push(ts + 'STOPTCL::SUCC::' + cc.Actual + ', process ended\n');
                                }
                            }
                        }
                    }
                }
                onTimeout();
            }
            function updateProgressAndStopNext () {
                if (currSeq >= commands.length) {
                    // have we reached 100s of waiting?
                    if (!fullDone) {
                        onTimeout();
                    } else {
                        removeProgressDialog();
                        try {
                            clearTimeout(timeout);
                        } catch (error) {
                            console.debug('[DBG]Move on.');
                        }
                        var message = errorReplies ? 'Stop procedure has completed, but ' + errorReplies + ' out of ' +
                            commands.length + ' commands failed' : 'Cluster stopped successfully';
                        console.info('[INF]' + message);
                        cancelButton('configWizardStopCluster');
                        if (oldForceStop || mysqldStopErrorsExist || errorReplies > 0) {
                            // Notify user of the location of log files.
                            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered">' +
                                displayLogFilesLocation(true) + '</span>');
                        }
                        removeServices().then(function (ok) {
                            mcc.util.displayModal('I', 3, '<span style="font-size:140%;color:teal">Stop procedure ' +
                                'ended.</span>');
                        });
                        try {
                            removeProgressDialog();
                        } catch (error) {
                            console.debug('[DBG]Done.');
                        }
                        waitCondition.resolve();
                        return;
                    }
                }

                if (currSeq < commands.length) {
                    console.debug('[DBG]Stopping cluster: ' + commands[currSeq].Actual + '.');
                    moveProgress('Stopping cluster' +
                        (errorReplies ? ' (' + errorReplies + ' failed command(s))' : ''), commands[currSeq].progTitle);
                    mcc.server.doReq('executeCommandReq',
                        { command: commands[currSeq].msg }, '', cluster, onReply, onError);
                }
            }
            // Initiate stop sequence.
            updateProgressAndStopNext();
            return waitCondition;
        });
    });
}
/**
 *Main function to save commands issued from Deploy page during the session to external file
 * and display them to user.
 *
 * @returns Nothing
 */
function viewCmds () {
    dijit.hideTooltip(dojo.byId('configWizardViewCmds'));
    dojo.byId('configWizardViewCmds').blur();
    // dojo.byId(this.id).blur();
    document.getElementById('startupDetails').focus();
    if (cmdLog.length === 0) {
        console.info('Log file is empty!');
        cancelButton('configWizardViewCmds');
        mcc.util.displayModal('I', 3, '<span style="font-size:125%;color:teal">The log is empty.</span>');
        return;
    }
    var fname = '';
    var d = new Date();
    var mnth = d.getMonth() + 1;
    var df = d.getFullYear() + '-' + ((mnth < 10) ? '0' + mnth : mnth) + '-' +
        ((d.getDate() < 10) ? '0' + d.getDate() : d.getDate()) + '_' + d.getHours() + '-' + d.getMinutes();
    fname = df + '_cmdslog.txt';
    createLogFile(fname, cmdLog.toString()).then(function (ok) {
        if (!ok) {
            cancelButton('configWizardViewCmds');
            // At least show contents of unsaved log.
            mcc.util.displayModal('I', 0, '<span style="font-size:130%;color:red">' + cmdLog.toString() + '</span>');
        } else {
            // Read and open file fname
            var msg = {
                head: { cmd: 'readCfgFileReq', seq: 1 },
                body: {
                    ssh: { keyBased: false, user: '', pwd: '' },
                    hostName: 'localhost',
                    path: '~', // readCfgFileReq handler will append "/.mcc"
                    fname: fname,
                    phr: ''
                }
            };
            readCfgFileReq(
                msg,
                function (reply) {
                    var temp = [];
                    temp = reply.body.hostRes.contentString.split(',');
                    var temp1 = [];
                    for (var t in temp) {
                        temp1.push(temp[t].replace('\n', ''));
                    }
                    var temp2 = [];
                    for (var g in temp1) {
                        temp2.push(temp1[g].replace('\r', ''));
                    }
                    var lines = '';
                    for (var z in temp2) {
                        lines += temp2[z] + '<br/>';
                    }
                    lines = '<pre>' + lines + '</pre>';
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:black">' + lines + '</span>');
                    cancelButton('configWizardViewCmds');
                    // do not reset cmdLog for session, cmdLog = [];
                },
                function (errMsgFNC) {
                    mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Unable to read log file ' +
                        fname + ' in HOME/.mcc directory: ' + errMsgFNC + '</span>');
                    cancelButton('configWizardViewCmds');
                }
            );
        }
    });
}

function createLogFile (fname, contents) {
    var waitCondition = new dojo.Deferred();
    mcc.server.createFileReq(
        'localhost',
        mcc.util.unixPath('~/.mcc'), // Python OS module will fix slashes.
        fname,
        contents,
        true,
        function () {
            waitCondition.resolve(true);
        },
        function (errMsg) {
            var wrn = 'Creating file HOME/.mcc/' + fname + ' on localhost failed with: ' + errMsg;
            console.warn(wrn);
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + wrn + '</span>');
            waitCondition.resolve(false);
        }
    );
    return waitCondition;
}

function readCfgFileReq (message, onReply, onError) {
    var ms = message;
    // Call do_post, provide callbacks
    doPost(ms).then(replyHandler(onReply, onError),
        errorHandler(ms.head, onError));
}

// Generic reply handler closure
function replyHandler (onReply, onError) {
    return function (reply) {
        if (reply && reply.stat && reply.stat.errMsg.toUpperCase() !== 'OK') {
            if (onError) {
                onError(reply.stat.errMsg, reply);
            } else {
                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">' + reply.stat.errMsg + '</span>');
            }
        } else {
            // FIXME: This can not fail unless onReply is addressing some structure.member wrong.
            try {
                onReply(reply);
            } catch (E) {
                mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:red">Error in formatting reply.</span>');
            }
        }
    };
}

// Generic error handler closure
function errorHandler (req, onError) {
    if (onError) {
        return onError;
    } else {
        return function (error) {
            console.error('[ERR]An error occurred while executing "%s(%s)": %s', req.cmd, req.seq, error);
        };
    }
}

function doPost (msg) {
    // Convert to json string
    var jsonMsg = dojo.toJson(msg);
    // Return deferred from xhrPost
    return dojo.xhrPost({
        url: '/cmd',
        headers: { 'Content-Type': 'application/json' },
        postData: jsonMsg,
        handleAs: 'json'
    });
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    console.info('[INF]' + 'Configuration deployment module initialized');
});
