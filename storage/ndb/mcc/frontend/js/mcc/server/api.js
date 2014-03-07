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
 *      mcc.server.api.createFileReq: Create a file with given contents
 *      mcc.server.api.appendFileReq: Append a file to another
 *      mcc.server.api.startClusterReq: Start cluster processes
 *      mcc.server.api.runMgmdCommandReq: Send command to an mgmd
 *      mcc.server.api.doReq: Send request
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      getSeqNo: Get next message sequence number
 *      do_post: Send message as json by http post
 *      errorHandler: Closure for handling errors by ignoring
 *      replyHandler: Closure for handling replies - check errMsg
 *      getHead: Create a message header block
 *      getSSH: Create an SSH block
 *
 *  Internal data: 
 *      None
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.server.api");

dojo.require("mcc.util");
dojo.require("mcc.storage");

/**************************** External interface  *****************************/

mcc.server.api.hostInfoReq = hostInfoReq;
mcc.server.api.createFileReq = createFileReq;
mcc.server.api.appendFileReq = appendFileReq;
mcc.server.api.runMgmdCommandReq = runMgmdCommandReq;
mcc.server.api.doReq = doReq;

/******************************* Internal data ********************************/

var seqNo = 0;

/****************************** Implementation  *******************************/

// Get next sequence number
function getSeqNo() {
    seqNo += 2;
    return seqNo; 
}

// Send message as http post
function do_post(msg) {
    // Convert to json string
    var jsonMsg = dojo.toJson(msg);

    // Hide password from logged message
    if (msg.body.ssh && msg.body.ssh.pwd) {
        msg.body.ssh.pwd = "****";
    }
    var dbgMsg = dojo.toJson(msg);
    mcc.util.dbg("Sending message: " + dbgMsg);

    // Return deferred from xhrPost
    return dojo.xhrPost({
        url: "/cmd",
        headers: { "Content-Type": "application/json" },
        postData: jsonMsg,
        handleAs: "json"
    });
}

// Generic error handler closure
function errorHandler(req, onError) {
    if (onError) {
        return onError;
    } else {
        return function (error) {
            msg.util.err("An error occurred while executing '" + req.cmd + 
                    " (" + req.seq + ")': " + error);
        }
    }
}

// Generic reply handler closure
function replyHandler(onReply, onError) {
    return function (reply) {
        if (reply && reply.stat && reply.stat.errMsg != "OK") {
            if (onError) {
                onError(reply.stat.errMsg, reply);
            } else {
                alert(reply.stat.errMsg);
            }
        } else {
            onReply(reply);
        }
    }
}

// Create a message header block
function getHead(cmd) {
    return {cmd: cmd, seq: getSeqNo() };
}

// Create an SSH block
function getSSH(keyBased, user, pwd) {
    if (keyBased) {
        return {keyBased: true};
    } else {
        return {user: user, pwd: pwd};
    }
}

// Send hostInfoReq
function hostInfoReq(hostname, onReply, onError) {
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        // Create message
        var msg = {
            head: getHead("hostInfoReq"),
            body: {
                ssh: getSSH(cluster.getValue("ssh_keybased"), 
                        cluster.getValue("ssh_user"),
                        mcc.gui.getSSHPwd()),
                hostName: hostname
            }
        };

        // Register last seq no
        mcc.storage.hostStorage().getItems({name: hostname}).then(
            function (hosts) {
                if (hosts[0]) {
                    hosts[0].setValue("hwResFetchSeq", msg.head.seq);
                    mcc.storage.hostStorage().save();

                    // Call do_post, provide callbacks
                    do_post(msg).then(replyHandler(onReply, onError), 
                            errorHandler(msg.head, onError));
                }
            }
        );
    });
}

// Send createFile
function createFileReq(hostname, path, filename, contents, overwrite, 
        onReply, onError) {
    // Get SSH info from cluster storage
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        // Create message
        var msg = {
            head: getHead("createFileReq"),
            body: {
                ssh: getSSH(cluster.getValue("ssh_keybased"), 
                        cluster.getValue("ssh_user"),
                        mcc.gui.getSSHPwd()),
                file: {
                    hostName: hostname,
                    path: path
                }
            }
        };
        if (filename) {
            msg.body.file.name = filename;
        }
        if (contents) {
            msg.body.contentString = contents;
        }
        if (overwrite) {
            msg.body.file.overwrite = overwrite;
        }
        // Call do_post, provide callbacks
        do_post(msg).then(replyHandler(onReply, onError), 
                errorHandler(msg.head, onError));
    });
}

// Send appendFile
function appendFileReq(hostname, srcPath, srcName, destPath, destName, 
        onReply, onError) {
    // Get SSH info from cluster storage
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        // Create message
        var msg = {
            head: getHead("appendFileReq"),
            body: {
                ssh: getSSH(cluster.getValue("ssh_keybased"), 
                        cluster.getValue("ssh_user"),
                        mcc.gui.getSSHPwd()),
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
        };

        // Call do_post, provide callbacks
        do_post(msg).then(replyHandler(onReply, onError), 
                errorHandler(msg.head, onError));
    });
}

// Send mgmd command
function runMgmdCommandReq(hostname, port, cmd, onReply, onError) {
    // Get SSH info from cluster storage
    mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
        // Create message
        var msg = {
            head: getHead("runMgmdCommandReq"),
            body: {
                ssh: getSSH(cluster.getValue("ssh_keybased"), 
                        cluster.getValue("ssh_user"),
                        mcc.gui.getSSHPwd()),
                hostname: hostname,
                port: port,
                mgmd_command: cmd
            }
        };

        // Call do_post, provide callbacks
        do_post(msg).then(replyHandler(onReply, onError), 
                errorHandler(msg.head, onError));
    });
}

// Send reqName with body ssh: prop is injected into body
function doReq(reqName, body, cluster, onReply, onError) {
    // Create message
    var msg = {
        head: getHead(reqName),
        body: body
    };
    
    msg.body.ssh = getSSH(cluster.getValue("ssh_keybased"), 
                          cluster.getValue("ssh_user"),
                          mcc.gui.getSSHPwd());

    // Call do_post, provide callbacks
    do_post(msg).then(replyHandler(onReply, onError), 
                      errorHandler(msg.head, onError));
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Server api module initialized");
});

