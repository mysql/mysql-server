/*
Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

/******************************************************************************
 ***                                                                        ***
 ***                               API to server                            ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.server.api
 *
 *  Description:
 *      Interface to server based on http post
 *
 *  External interface:
 *      mcc.server.api.hostInfoReq: Get HW resource info for a host
 *      mcc.server.api.hostDockerReq: Get Docker status for a host
 *      mcc.server.api.createFileReq: Create a file with given contents
 *      mcc.server.api.appendFileReq: Append a file to another
 *      mcc.server.api.startClusterReq: Start cluster processes
 *      mcc.server.api.runMgmdCommandReq: Send command to an mgmd
 *      mcc.server.api.runcopyKeyReq: Generate and copy id_rsa.pub between OCI hosts.
 *      mcc.server.api.getLogsReq: Get (remote) log file locally to ~/.mcc
 *      mcc.server.api.dropStuffReq: Remove directory and its contents from host.
 *      mcc.server.api.doReq: Send request
 *      mcc.server.api.getHead: Create a message header block
 *      mcc.server.api.runSSHCleanupReq: Clean up permanent connection array in back end.
 *      mcc.server.api.listRemoteHostsReq: Weed out remote host connection(s) belonging
 *          to non-existent hosts.
 *      mcc.server.api.pingRemoteHostsReq: Plain ping from localhost to Cluster hosts.
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *      getSeqNo: Get next message sequence number
 *      doPost: Send message as json by http post
 *      errorHandler: Closure for handling errors by ignoring
 *      replyHandler: Closure for handling replies - check errMsg
 *      whichCredsForHost: Determine, by hostname, which credentials to use.
 *
 *  Internal data:
 *      None
 *
 *  Unit test interface:
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *      handle_getLogTailReq never called. Use to fetch log files locally in DEPLOY.
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/
dojo.provide('mcc.server.api');

dojo.require('mcc.util');
dojo.require('mcc.storage');

/**************************** External interface  *****************************/
mcc.server.api.hostInfoReq = hostInfoReq;
mcc.server.api.hostDockerReq = hostDockerReq;
mcc.server.api.checkFileReq = checkFileReq;
mcc.server.api.createFileReq = createFileReq;
mcc.server.api.appendFileReq = appendFileReq;
mcc.server.api.getLogsReq = getLogsReq;
mcc.server.api.runMgmdCommandReq = runMgmdCommandReq;
mcc.server.api.dropStuffReq = dropStuffReq;
mcc.server.api.doReq = doReq;
mcc.server.api.runcopyKeyReq = runcopyKeyReq;
// Decided to export getHead so to be able to link SEQ# into command body.
mcc.server.api.getHead = getHead;
mcc.server.api.runSSHCleanupReq = runSSHCleanupReq;
mcc.server.api.listRemoteHostsReq = listRemoteHostsReq;
mcc.server.api.pingRemoteHostsReq = pingRemoteHostsReq;
/******************************* Internal data ********************************/
var seqNo = 0;

/****************************** Implementation  *******************************/
// Get next sequence number
function getSeqNo () {
    seqNo += 2;
    return seqNo;
}

// Send message as http post
function doPost (msg) {
    console.info('[INF]Running doPost');
    // Convert to json string
    var jsonMsg = dojo.toJson(msg);

    // Hide password from logged message
    if (msg.body.ssh && msg.body.ssh.pwd) {
        msg.body.ssh.pwd = '****';
    }
    // Hide passphrase from logged message
    if (msg.body.ssh && msg.body.ssh.key_passp) {
        msg.body.ssh.key_passp = '****';
    }

    var dbgMsg = dojo.toJson(msg);
    if (dbgMsg.length > 1000) {
        // It's too much
        var length = 120;
        dbgMsg = dbgMsg.length > length ? dbgMsg.substring(0, length - 3) + '...' : dbgMsg;
    }
    // Return deferred from xhrPost
    return dojo.xhrPost({
        url: '/cmd',
        headers: { 'Content-Type': 'application/json' },
        postData: jsonMsg,
        handleAs: 'json'
    });
}

// Generic error handler closure
function errorHandler (req, onError) {
    if (onError) {
        return onError;
    } else {
        return function (error) {
            console.error('[ERR]An error occurred while executing "%s(%i"): %s', req.cmd, req.seq, error);
        };
    }
}

// Generic reply handler closure
function replyHandler (onReply, onError) {
    /*
    If this function fails it means descendants implemented reply/error that does not cover for
    the response we got from BE (i.e. looking for wrong/non-existent member).
    */
    return function (reply) {
        if (reply && reply.stat && String(reply.stat.errMsg) !== 'OK') {
            if (onError) {
                onError(reply.stat.errMsg, reply);
            } else {
                console.error('[ERR]In onError of reply function?');
            }
        } else {
            onReply(reply);
        }
    };
}

// Create a message header block
function getHead (cmd) {
    return { cmd: cmd, seq: getSeqNo() };
}

/**
 *Form SSH block for message body to send to hosts.
 * This is for worker threads (deploy/install).
 *
 * @param {String} hostName name of host to look for in hosts object
 * @param {Number} seq synchronization number from head of message
 * @param {String} req name of the request, mainly so that hostInfoReq can update
 * host store
 * @returns {JSON} block ready to be put into command message
 */
function formProperSSHBlock (hostName, seq, req) {
    console.info('[INF]Runing formProperSSHBlock for ' + hostName);
    var hostSetUseSSH = false;
    var hostSetSSHPwd = '';
    var hostSetSSHUsr = '';
    var hostSetSSHKFile = '';
    var hostSetPwd = '';
    var hostSetUsr = '';

    if (req !== 'hostInfoReq') {
        // return obvious two first
        if (hostName === '127.0.0.1' || (hostName.toLowerCase() === 'localhost')) {
            return { keyBased: false, user: '', pwd: '' };
        }

        if (mcc.gui.getSSHkeybased()) {
            return {
                keyBased: true,
                key: '', // if getClSSHKeyFile() is empty, then it's id_rsa by default. Can't be both.
                key_user: mcc.gui.getClSSHUser(),
                key_passp: mcc.gui.getClSSHPwd(),
                key_file: mcc.gui.getClSSHKeyFile()
            };
        };
    }

    mcc.storage.hostStorage().getItems({ name: hostName }).then(function (hosts) {
        if (hosts[0]) {
            hostSetUseSSH = hosts[0].getValue('key_auth');
            if (hostSetUseSSH) {
                hostSetSSHPwd = hosts[0].getValue('key_passp');
                hostSetSSHUsr = hosts[0].getValue('key_usr');
                hostSetSSHKFile = hosts[0].getValue('key_file');
            } else {
                hostSetPwd = hosts[0].getValue('usrpwd');
                hostSetUsr = hosts[0].getValue('usr');
            }
            if (req === 'hostInfoReq') {
                // so that others might call it too
                hosts[0].setValue('hwResFetchSeq', seq);
                // changing field will trigger save in Store.js
            }
        } else {
            console.debug('[DBG]Host %s not found in host store!', hostName);
        }
    });

    // return obvious two first
    if (hostName === '127.0.0.1' || (hostName.toLowerCase() === 'localhost')) {
        return { keyBased: false, user: '', pwd: '' };
    }

    if (mcc.gui.getSSHkeybased()) {
        console.debug('[DBG]SSHBlock-CLUSTER SSH creds.');
        return {
            keyBased: true,
            key: '', // if getClSSHKeyFile() is empty, then it's id_rsa by default. Can't be both.
            key_user: mcc.gui.getClSSHUser(),
            key_passp: mcc.gui.getClSSHPwd(),
            key_file: mcc.gui.getClSSHKeyFile()
        };
    };

    if (hostSetUseSSH) {
        console.debug('[DBG]SSHBlock-HOST SSH creds.');
        return {
            keyBased: true,
            key: '',
            key_user: hostSetSSHUsr,
            key_passp: hostSetSSHPwd,
            key_file: hostSetSSHKFile };
    } else {
        if (hostSetPwd || hostSetUsr) {
            console.debug('[DBG]SSHBlock-HOST creds.');
            return {
                keyBased: false,
                user: hostSetUsr,
                pwd: hostSetPwd
            };
        } else {
            // this is final safety if all above fails
            console.debug('[DBG]SSHBlock-CLUSTER creds.');
            return {
                keyBased: false,
                user: mcc.gui.getSSHUser(),
                pwd: mcc.gui.getSSHPwd()
            };
        }
    }
}

/**
 *Sends hostInfoReq to BE
 *
 * @param {String} hostname external IP address of Host
 * @param {function} onReply response handler
 * @param {function} onError error response handler
 */
function hostInfoReq (hostname, onReply, onError) {
    console.info('[INF]' + 'Running hostInfoReq for host ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('hostInfoReq called with invalid host name!');
        return
    }
    var seqno = getHead('hostInfoReq'); // to get SeqNumber!
    var msg = {
        head: seqno,
        body: {
            ssh: formProperSSHBlock(hostname, seqno.seq, 'hostInfoReq'),
            hostName: hostname
        }
    }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/**
 *Send hostDockerReq to BE.
 *
 * @param {String} hostname external IP address of Host
 * @param {function} onReply response handler
 * @param {function} onError error response handler
 */
function hostDockerReq (hostname, onReply, onError) {
    console.info('[INF]Runing hostDockerReq for ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('hostDockerReq called with invalid host name!');
        return
    }
    var msg = {
        head: getHead('hostDockerReq'),
        body: {
            ssh: formProperSSHBlock(hostname, 0, 'hostDockerReq'),
            hostName: hostname
        }
    }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/**
 *Syncing routine between FE and BE.
    FE sends list of hosts from user configuration. It is then matched against list of perm. conn.
    to remote hosts in BE. When permanent connection in BE does not match any of the hosts in
    configuration, it is destroyed. New permanent connection(s) will be created upon arrival of the
    next command for new host.
 *
 * @param {String} hostlist comma separated list of external IP address of Hosts
 * @param {function} onReply response handler
 * @param {function} onError error response handler
 */
function listRemoteHostsReq (hostslist, onReply, onError) {
    console.info('[INF]Runing listRemoteHostsReq for ' + hostslist);
    // body.reply_type: 'OK'/'ERROR'
    var msg = {
        head: getHead('listRemoteHostsReq'),
        body: {
            ssh: {
                keyBased: true,
                key: '',
                key_user: mcc.gui.getClSSHUser(),
                key_passp: mcc.gui.getClSSHPwd(),
                key_file: mcc.gui.getClSSHKeyFile()
            }, // dummy
            hostName: 'hostlist',   // dummy
            data: hostslist
        }
    };
    // Call doPost, provide callbacks
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}
/**
 *Pings list of remote hosts in back end.
 *
 * @param {String} hostslist List of hosts to ping separated by semicolon
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function pingRemoteHostsReq (hostslist, onReply, onError) {
    console.info('[INF]' + 'Pinging remote hosts.');
    var msg;
    msg = {
        head: getHead('pingRemoteHostsReq'),
        body: {
            ssh: { keyBased: false, key: '', key_user: '', key_passp: '', key_file: '' }, // dummy
            hostName: 'hostlist',   // dummy
            data: hostslist
        }
    };
    // Call doPost, provide callbacks
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

// Send checkFile
function checkFileReq (hostname, path, filename, contents, overwrite, onReply, onError) {
    console.info('[INF]Runing checkFileReq for host ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('checkFileReq called with invalid host name!');
        return
    }
    var msg = {
        head: getHead('checkFileReq'),
        body: {
            ssh: formProperSSHBlock(hostname, 0, 'checkFileReq'),
            file: {
                hostName: hostname,
                path: path
            }
        }
    }
    if (filename) { msg.body.file.name = filename; }
    if (contents) { msg.body.contentString = contents; }
    if (overwrite) { msg.body.file.overwrite = overwrite; }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

// Send createFile
function createFileReq (hostname, path, filename, contents, overwrite, onReply, onError) {
    console.info('[INF]' + 'Running createFileReq for host ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('createFileReq called with invalid host name!');
        return
    }
    var msg = {
        head: getHead('createFileReq'),
        body: {
            ssh: formProperSSHBlock(hostname, 0, 'createFileReq'),
            file: {
                hostName: hostname,
                path: path
            }
        }
    }
    if (filename) { msg.body.file.name = filename; }
    if (contents) { msg.body.contentString = contents; }
    if (overwrite) { msg.body.file.overwrite = overwrite; }

    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/**
 *Sends request to append one file to other to BE.
 *
 * @param {String} hostname ExternalIP address (FQDN) of recipient.
 * @param {String} srcPath Remote path to file in question.
 * @param {String} srcName Remote name of file in question.
 * @param {String} destPath Remote destination path.
 * @param {String} destName Remote destination file name.
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function appendFileReq (hostname, srcPath, srcName, destPath, destName, onReply, onError) {
    console.info('[INF]' + 'Running appendFileReq for host ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('appendFileReq called with invalid host name!');
        return
    }
    var msg = {
        head: getHead('appendFileReq'),
        body: {
            ssh: formProperSSHBlock(hostname, 0, 'appendFileReq'),
            sourceFile: {
                hostName: hostname,
                path: srcPath,
                name: srcName
            },
            destinationFile: {
                hostName: hostname,
                path: destPath,
                name: destName
            }
        }
    }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}
/**
 *Sends request to collect log from remote host to BE.
 *
 * @param {String} hostname ExternalIP address (FQDN) of host where log is.
 * @param {String} srcPath Remote path to log.
 * @param {String} srcName The name of the log file.
 * @param {String} destName The name of local file.
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function getLogsReq (hostname, srcPath, srcName, destName, onReply, onError) {
    console.info('[INF]' + 'Running getLogsReq for host ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('getLogsReq called with invalid host name!');
        return
    }
    var msg = {
        head: getHead('getLogsReq'),
        body: {
            ssh: formProperSSHBlock(hostname, 0, 'getLogsReq'),
            sourceFile: {
                hostName: hostname,
                path: srcPath,
                name: srcName
            },
            destinationFile: {
                // hostName: hostname, Always "localhost"
                // path: destPath, Always ~/.mcc
                name: destName
            }
        }
    }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/**
 *Send drop path request to BE
 *
 * @param {String} reqName Name of the request BE recognizes.
 * @param {String} hostname External IP (FQDN) of host where path is located.
 * @param {String} datadir Path to drop.
 * @param {JSON} msghead HEAD of message
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function dropStuffReq (reqName, hostname, datadir, msghead, onReply, onError) {
    console.info('[INF]' + 'Running dropStuffReq for host ' + hostname);
    // Create message but check if HEAD with SEQ is passed from elsewhere to
    // allow for batch execution.
    var msg;
    if (typeof msghead == 'string' && msghead.length < 1) {
        // HEAD is empty string.
        msg = {
            head: getHead(reqName),
            body: { file: { path: datadir, host: hostname } }
        };
    } else {
        msg = {
            head: msghead,
            body: { file: { path: datadir, host: hostname } }
        };
    }

    if (hostname === undefined || hostname.length <= 0) {
        console.warn('[WRN]dropStuffReq called with invalid host name!');
        return
    }
    msg.body.ssh = formProperSSHBlock(hostname, 0, 'dropStuffReq')
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/**
 *Sends request to collect information about status of Cluster or stop collecting information to BE.
 *
 * @param {String} hostname External IP address of host running management process.
 * @param {Number} port NDB Cluster management node port.
 * @param {String} instDir Location of binaries. This is from Host configuration but BE tries
 * to locate binary also in path, /sbin, /bin.
 * @param {String} uname Platform.
 * @param {String} cmd Command to execute. Serves to send special codes like 'STOP' too.
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function runMgmdCommandReq (hostname, port, instDir, uname, cmd, onReply, onError) {
    console.info('[INF]' + 'Running runMgmdCommandReq for host ' + hostname);
    if (hostname === undefined || hostname.length <= 0) {
        console.warn('[WRN]runMgmdCommandReq called with invalid host name!');
        return
    }
    var msg = {
        head: getHead('runMgmdCommandReq'),
        body: {
            ssh: formProperSSHBlock(hostname, 0, 'runMgmdCommandReq'),
            hostName: hostname,
            port: port,
            inst_dir: instDir,
            uname: uname,
            mgmd_command: cmd
        }
    }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/**
 *Send request to BE to clean up its array of permanent SSH connections to remote hosts.
 * This called, for example, when starting MCC as BE could have been running different
 * configuration previously.
 * Since BE deals with everything, no parameters are passed in.
 *
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function runSSHCleanupReq (onReply, onError) {
    function provideClCreds () {
        if (mcc.gui.getSSHkeybased()) {
            return {
                keyBased: true,
                key: '',
                key_user: mcc.gui.getClSSHUser(),
                key_passp: mcc.gui.getClSSHPwd(),
                key_file: mcc.gui.getClSSHKeyFile()
            };
        } else {
            // this is final safety if all above fails
            return { keyBased: false, user: mcc.gui.getSSHUser(), pwd: mcc.gui.getSSHPwd() };
        }
    }
    // Sincxe it's a list of hosts, this can only work with ClusterLvL SSH auth.
    console.info('[INF]Running runSSHCleanupReq');
    var msg = {
        head: getHead('SSHCleanupReq'),
        body: {
            ssh: provideClCreds(),
            hostName: 'hostlist'   // dummy
        }
    };
    // Call doPost, provide callbacks
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}
/**
 *Generate, if needs be, and copy public key between OCI hosts. One set of credentials
 * (ClusterLvL/Any of the hosts) should fit all. This command should be called with
 * list of hosts involved (external IP addresses) and a single set of credentials.
 *
 * @param {String} hostlist hosts to exchange keys between, comma separated
 * @param {String} hostlistinternal Unused
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function runcopyKeyReq (hostlist, hostlistinternal, onReply, onError) {
    function provideClCreds () {
        if (mcc.gui.getSSHkeybased()) {
            return {
                keyBased: true,
                key: '',
                key_user: mcc.gui.getClSSHUser(),
                key_passp: mcc.gui.getClSSHPwd(),
                key_file: mcc.gui.getClSSHKeyFile()
            };
        } else {
            // this is final safety if all above fails
            return { keyBased: false, user: mcc.gui.getSSHUser(), pwd: mcc.gui.getSSHPwd() };
        }
    }
    // Since it's a list of hosts, this can only work with ClusterLvL SSH auth.
    console.info('[INF]Running copyKeyReq for OCI hosts with Cluster keys, list is ' + hostlist);
    var msg = {
        head: getHead('copyKeyReq'),
        body: {
            ssh: provideClCreds(),
            hostName: 'hostlist',   // dummy
            port: 1111,             // dummy
            hosts: hostlist,
            hostsinternal: hostlistinternal
        }
    };
    // Call doPost, provide callbacks
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}
/**
 *Function to send custom requests from other units.
 *
 * @param {String} reqName The name of the request BE recognizes.
 * @param {JSON} body BODY of the message to be sent.
 * @param {JSON} msghead HEAD of the message to be sent.
 * @param {*} cluster Unused.
 * @param {function} onReply Generic reply handler
 * @param {function} onError Generic error handler
 */
function doReq (reqName, body, msghead, cluster, onReply, onError) {
    console.info('[INF]' + 'Running doReq ' + reqName);
    // Create message
    var msg;
    if (typeof msghead == 'string' && msghead.length < 1) {
        // HEAD is empty string.
        msg = {
            head: getHead(reqName),
            body: body
        };
    } else {
        msg = {
            head: msghead,
            body: body
        };
    }
    // Try to fish hostName from message body.
    var hostNameFromBody = '';
    try {
        hostNameFromBody = body['command']['file'].hostName;
    } catch (err) {
        hostNameFromBody = '';
        console.debug('[DBG]' + 'FAILED to obtain HostName from command. Proceeding with empty HostName');
    }
    if (hostNameFromBody === undefined || hostNameFromBody.length <= 0) {
        console.warn('[WRN]doReq called with invalid host name!');
    }
    if (hostNameFromBody) {
        msg.body.ssh = formProperSSHBlock(hostNameFromBody, 0, 'doReq');
    } else {
        msg.body.ssh = { keyBased: false };
    }
    doPost(msg).then(replyHandler(onReply, onError), errorHandler(msg.head, onError));
}

/******************************** Initialize  *********************************/
dojo.ready(function () {
    console.info('[INF]' + 'Server api module initialized');
});
