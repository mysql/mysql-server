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
 *      hostHasCreds: Does HostStorage hold credentials
 *      createSSHBlock: New function to create SSH part of message, exclusively for private key auth.
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
mcc.server.api.hostDockerReq = hostDockerReq;
mcc.server.api.checkFileReq = checkFileReq;
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

// Check if Host storage holds credentials
function hostHasCreds (hostname) {
    hostCreds = false;
    mcc.util.dbg("Running hostHasCreds for host " + hostname);
    if ((hostname == "127.0.0.1") || (hostname == "localhost") || (hostname == '')) {
        mcc.util.dbg("Host " + hostname + " is local.");
    } else {
        mcc.util.dbg("Host " + hostname + " is NOT local.");
        mcc.storage.hostStorage().getItems({name: hostname}).then(
            function (hosts) {
                hostCreds = ((hosts[0].getValue("key_auth") && (hosts[0].getValue("key_usr") || hosts[0].getValue("key_passp")
                    || hosts[0].getValue("key_file"))) || (hosts[0].getValue("usr")))
            }
        );
    }
    return hostCreds;
}
// Send message as http post
function do_post(msg) {
    // Convert to json string
    var jsonMsg = dojo.toJson(msg);

    // Hide password from logged message
    if (msg.body.ssh && msg.body.ssh.pwd) {
        msg.body.ssh.pwd = "****";
    }
    // Hide passphrase from logged message
    if (msg.body.ssh && msg.body.ssh.key_passp) {
        msg.body.ssh.key_passp = "****";
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

// Create an SSH block. "Old" way function.
function getSSH(hostname, keyBased, user, pwd) {
    if ((keyBased) && ((hostname != "127.0.0.1") && (hostname != "localhost")
      && (hostname != ''))) {
        return {keyBased: true};
    } else {
        return {keyBased: false, user: user, pwd: ((pwd === undefined || pwd == null
          || pwd.length < 0 || pwd.substring(0,1) == '**') ? "" : pwd)};
    }
}

// Create SSH block, "new" way function, exclusively for key-auth.
function createSSHBlock(key_user, key_passp, key_file) {
    /*****************************************************************************
     *   We can auth in several ways:
     *   o USR/PWD (hosts[0].getValue("usr"), hosts[0].getValue("usrpwd"))
     *   o By key:
     *       - Automatically: ONLY hosts[0].getValue("key_usr") and/or 
     *         hosts[0].getValue("key_passp") provided
     *       - hosts[0].getValue("key_file") provided.
     *         In this case, usr/passp might be provided too.
     *   IF hosts[0].getValue("key_auth") THEN
     *       IF hosts[0].getValue("key_file") THEN
     *           IF hosts[0].getValue("key_file") THEN provide key_file
     *           IF hosts[0].getValue("key_usr") THEN provide key_usr
     *           IF hosts[0].getValue("key_passp") THEN provide key_passp
     *       ELSE
     *           IF hosts[0].getValue("key_usr") THEN provide key_usr
     *           IF hosts[0].getValue("key_passp") THEN provide key_passp
     *       END
     *   ELSE
     *       IF hosts[0].getValue("usr") THEN provide usr
     *       IF hosts[0].getValue("usrpwd") THEN provide usrpwd
     *   END
     *
     *   NEW: ClusterStorage got new members for detailed SSH config like the 
     *   above for HostStorage. Same logic apply.
     *      mcc.gui.getClSSHUser()
     *      mcc.gui.getClSSHPwd()
     *      mcc.gui.getClSSHKeyFile()
     *****************************************************************************/

    if (key_file) {
        if (key_user) {
            if (key_passp && key_passp.substring(0, 1) != "**") {
                return {keyBased: true, key: "", key_user: key_user, key_passp: key_passp, key_file: key_file};
            } else {
                return {keyBased: true, key: "", key_user: key_user, key_passp: "", key_file: key_file};
            }
        } else {
            if (key_passp && key_passp.substring(0, 1) != "**") {
                return {keyBased: true, key: "", key_user: "", key_passp: key_passp, key_file: key_file};
            } else {
                return {keyBased: true, key: "", key_user: "", key_passp: "", key_file: key_file};
            }
        }
    } else {
        //No key nor key_file; check key_user and key_passphrase.
        if (key_user) {
            if (key_passp && key_passp.substring(0, 1) != "**") {
                return {keyBased: true, key: "", key_user: key_user, key_passp: key_passp, key_file: ""};
            } else {
                return {keyBased: true, key: "", key_user: key_user, key_passp: "", key_file: ""};
            }
        } else {
            if (key_passp && key_passp.substring(0, 1) != "**") {
                return {keyBased: true, key: "", key_user: "", key_passp: key_passp, key_file: ""};
            } else {
                return {keyBased: true, key: "", key_user: "", key_passp: "", key_file: ""};
            }
        }
            
    }
}

// Send hostInfoReq
function hostInfoReq(hostname, onReply, onError) {
    // First, check if there are HOST bound credentials:
    mcc.util.dbg("Running hostInfoReq for host " + hostname);
    if (hostname != "") {
        if (hostHasCreds(hostname)) {
            mcc.util.dbg("Host " + hostname + " has creds.");
            mcc.storage.hostStorage().getItems({name: hostname}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            //Remote host with its own credentials (PK).
                            mcc.util.dbg("Running hostInfoReq for host " + hostname + " with Host keys");
                            var msg = {
                                head: getHead("hostInfoReq"),
                                body: {
                                    ssh: createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                        hosts[0].getValue("key_file")),
                                    hostName: hostname
                                }
                            }
                        } else {
                            //Remote host with its own credentials but not PK.
                            mcc.util.dbg("Running hostInfoReq for host " + hostname + " with Host creds");
                            var msg = {
                                head: getHead("hostInfoReq"),
                                body: {
                                    ssh: getSSH(hostname, false, 
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd")),
                                    hostName: hostname
                                }
                            }
                            
                        }
                        hosts[0].setValue("hwResFetchSeq", msg.head.seq);
                        mcc.storage.hostStorage().save();

                        // Call do_post, provide callbacks
                        do_post(msg).then(replyHandler(onReply, onError), 
                                errorHandler(msg.head, onError));
                    }
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostname + " has no creds.");
            mcc.util.dbg("SSHKeyBase is " + mcc.gui.getSSHkeybased());
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running hostInfoReq for host " + hostname + " with Cluster keys");
                var msg = {
                    head: getHead("hostInfoReq"),
                    body: {
                        ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                            mcc.gui.getClSSHKeyFile()),
                        hostName: hostname
                    }
                }

            } else {
                // Create default message
                mcc.util.dbg("Running hostInfoReq for host " + hostname + " with Cluster creds");
                var msg = {
                    head: getHead("hostInfoReq"),
                    body: {
                        ssh: getSSH(hostname, mcc.gui.getSSHkeybased(), 
                                mcc.gui.getSSHUser(),
                                mcc.gui.getSSHPwd()),
                        hostName: hostname
                    }
                };
            }
            
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
        }
    }
}

// Send hostDockerReq
function hostDockerReq(hostname, onReply, onError) {
    // First, check if there are HOST bound credentials:
    mcc.util.dbg("Running hostDockerReq for host " + hostname);
    if (hostname != "") {
        if (hostHasCreds(hostname)) {
            mcc.util.dbg("Host " + hostname + " has creds.");
            mcc.storage.hostStorage().getItems({name: hostname}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            //Remote host with its own credentials (PK).
                            mcc.util.dbg("Running hostDockerReq for host " + hostname + " with Host keys");
                            var msg = {
                                head: getHead("hostDockerReq"),
                                body: {
                                    ssh: createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                        hosts[0].getValue("key_file")),
                                    hostName: hostname
                                }
                            }
                        } else {
                            //Remote host with its own credentials but not PK.
                            mcc.util.dbg("Running hostDockerReq for host " + hostname + " with Host creds");
                            var msg = {
                                head: getHead("hostDockerReq"),
                                body: {
                                    ssh: getSSH(hostname, false, 
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd")),
                                    hostName: hostname
                                }
                            }
                            
                        }

                        // Call do_post, provide callbacks
                        do_post(msg).then(replyHandler(onReply, onError), 
                                errorHandler(msg.head, onError));
                    }
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostname + " has no creds.");
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running hostDockerReq for host " + hostname + " with Cluster keys");
                var msg = {
                    head: getHead("hostDockerReq"),
                    body: {
                        ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                            mcc.gui.getClSSHKeyFile()),
                        hostName: hostname
                    }
                }

            } else {
                // Create default message
                mcc.util.dbg("Running hostDockerReq for host " + hostname + " with Cluster creds");
                var msg = {
                    head: getHead("hostDockerReq"),
                    body: {
                        ssh: getSSH(hostname, mcc.gui.getSSHkeybased(), 
                                mcc.gui.getSSHUser(),
                                mcc.gui.getSSHPwd()),
                        hostName: hostname
                    }
                };
            }
        }
    }
}

// Send checkFile
function checkFileReq(hostname, path, filename, contents, overwrite, 
        onReply, onError) {
    if (hostname != "") {
        if (hostHasCreds(hostname)) {
            mcc.storage.hostStorage().getItems({name: hostname}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            //Remote host with its own credentials (PK).
                            mcc.util.dbg("Running checkFileReq for host " + hostname + " with Host keys");
                            var msg = {
                                head: getHead("checkFileReq"),
                                body: {
                                    ssh: createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                        hosts[0].getValue("key_file")),
                                    file: {
                                        hostName: hostname,
                                        path: path
                                    }
                                }
                            }
                        } else {
                            //Remote host with its own credentials but not PK.
                            mcc.util.dbg("Running checkFileReq for host " + hostname + " with Host creds");
                            var msg = {
                                head: getHead("checkFileReq"),
                                body: {
                                    ssh: getSSH(hostname, false, 
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd")),
                                    file: {
                                        hostName: hostname,
                                        path: path
                                    }
                                }
                            }
                        }

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
                    }
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostname + " has no creds.");
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running checkFileReq for host " + hostname + " with Cluster keys");
                var msg = {
                    head: getHead("checkFileReq"),
                    body: {
                        ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                            mcc.gui.getClSSHKeyFile()),
                        file: {
                            hostName: hostname,
                            path: path
                        }
                    }
                }

            } else {
                // Create default message
                mcc.util.dbg("Running checkFileReq for host " + hostname + " with Cluster creds");
                var msg = {
                    head: getHead("checkFileReq"),
                    body: {
                        ssh: getSSH(hostname, mcc.gui.getSSHkeybased(), 
                                mcc.gui.getSSHUser(),
                                mcc.gui.getSSHPwd()),
                        file: {
                            hostName: hostname,
                            path: path
                        }
                    }
                };
            }
            
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
        }
    }
}

// Send createFile
function createFileReq(hostname, path, filename, contents, overwrite, 
        onReply, onError) {
    if (hostname != "") {
        if (hostHasCreds(hostname)) {
            mcc.storage.hostStorage().getItems({name: hostname}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            //Remote host with its own credentials (PK).
                            mcc.util.dbg("Running createFileReq for host " + hostname + " with Host keys");
                            var msg = {
                                head: getHead("createFileReq"),
                                body: {
                                    ssh: createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                        hosts[0].getValue("key_file")),
                                    file: {
                                        hostName: hostname,
                                        path: path
                                    }
                                }
                            };
                        } else {
                            //Remote host with its own credentials but not PK.
                            mcc.util.dbg("Running createFileReq for host " + hostname + " with Host creds");
                            var msg = {
                                head: getHead("createFileReq"),
                                body: {
                                    ssh: getSSH(hostname, false, 
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd")),
                                    file: {
                                        hostName: hostname,
                                        path: path
                                    }
                                }
                            }
                            
                        }

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
                    }
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostname + " has no creds.");
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running createFileReq for host " + hostname + " with Cluster keys");
                try {
                    var msg = {
                        head: getHead("createFileReq"),
                        body: {
                            ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                                mcc.gui.getClSSHKeyFile()),
                            file: {
                                hostName: hostname,
                                path: path
                            }
                        }
                    };
                } catch (e){
                    //IF mcc.gui.getSSHPwd() fails, which is the only case,
                    //then we can safely assume we are BEFORE gui initialization,
                    //which means on welcome page and not yet on content. Thus,
                    //no creds are available, we're on lacalhost.
                    mcc.util.dbg("Inside CATCH");
                    msg = {
                        head: getHead("createFileReq"),
                        body: {
                            ssh: getSSH("localhost", false, "",""),
                            file: {
                                hostName: hostname,
                                path: path
                            }
                        }
                    };
                };
            } else {
                // Create default message
                mcc.util.dbg("Running createFileReq for host " + hostname + " with Cluster creds");
                try {
                    var msg = {
                        head: getHead("createFileReq"),
                        body: {
                            ssh: getSSH(hostname, mcc.gui.getSSHkeybased(), 
                                    mcc.gui.getSSHUser(),
                                    mcc.gui.getSSHPwd()),
                            file: {
                                hostName: hostname,
                                path: path
                            }
                        }
                    };
                } catch (e){
                    //IF mcc.gui.getSSHPwd() fails, which is the only case,
                    //then we can safely assume we are BEFORE gui initialization,
                    //which means on welcome page and not yet on content. Thus,
                    //no creds are available, we're on lacalhost.
                    mcc.util.dbg("Inside CATCH");
                    msg = {
                        head: getHead("createFileReq"),
                        body: {
                            ssh: getSSH("localhost", false, "",""),
                            file: {
                                hostName: hostname,
                                path: path
                            }
                        }
                    };
                };
            }
            
            
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
        }
    }
}

// Send appendFile
function appendFileReq(hostname, srcPath, srcName, destPath, destName, 
        onReply, onError) {
    if (hostname != "") {
        if (hostHasCreds(hostname)) {
            mcc.storage.hostStorage().getItems({name: hostname}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            mcc.util.dbg("Running appendFileReq for host " + hostname + " with Host keys");
                            //Remote host with its own credentials (PK).
                            var msg = {
                                head: getHead("appendFileReq"),
                                body: {
                                    ssh: createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                        hosts[0].getValue("key_file")),
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
                        } else {
                            //Remote host with its own credentials but not PK.
                            mcc.util.dbg("Running appendFileReq for host " + hostname + " with Host creds");
                            var msg = {
                                head: getHead("appendFileReq"),
                                body: {
                                    ssh: getSSH(hostname, false,
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd")),
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
                        }

                        // Call do_post, provide callbacks
                        do_post(msg).then(replyHandler(onReply, onError),
                                errorHandler(msg.head, onError));
                    }
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostname + " has no creds.");
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running appendFileReq for host " + hostname + " with Cluster keys");
                var msg = {
                    head: getHead("appendFileReq"),
                    body: {
                        ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                            mcc.gui.getClSSHKeyFile()),
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

            } else {
                // Create default message
                mcc.util.dbg("Running appendFileReq for host " + hostname + " with Cluster creds");
                var msg = {
                    head: getHead("appendFileReq"),
                    body: {
                        ssh: getSSH(hostname, mcc.gui.getSSHkeybased(), 
                                mcc.gui.getSSHUser(),
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
            }
            // Call do_post, provide callbacks
            do_post(msg).then(replyHandler(onReply, onError), 
                    errorHandler(msg.head, onError));
        }
    }
}

// Send mgmd command
function runMgmdCommandReq(hostname, port, cmd, onReply, onError) {
    if (hostname != "") {
        if (hostHasCreds(hostname)) {
            mcc.storage.hostStorage().getItems({name: hostname}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            //Remote host with its own credentials (PK).
                            mcc.util.dbg("Running runMgmdCommandReq for host " + hostname + " with Host keys");
                            var msg = {
                                head: getHead("runMgmdCommandReq"),
                                body: {
                                    ssh: createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                        hosts[0].getValue("key_file")),
                                    hostName: hostname,
                                    port: port,
                                    mgmd_command: cmd
                                }
                            };
                        } else {
                            //Remote host with its own credentials but not PK.
                            mcc.util.dbg("Running runMgmdCommandReq for host " + hostname + " with Host creds");
                            var msg = {
                                head: getHead("runMgmdCommandReq"),
                                body: {
                                    ssh: getSSH(hostname, false, 
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd")),
                                    hostName: hostname,
                                    port: port,
                                    mgmd_command: cmd
                                }
                            }
                        }
                        
                        // Call do_post, provide callbacks
                        do_post(msg).then(replyHandler(onReply, onError), 
                                errorHandler(msg.head, onError));
                    }
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostname + " has no creds.");
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running runMgmdCommandReq for host " + hostname + " with Cluster keys");
                var msg = {
                    head: getHead("runMgmdCommandReq"),
                    body: {
                        ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                            mcc.gui.getClSSHKeyFile()),
                            hostName: hostname,
                            port: port,
                            mgmd_command: cmd
                    }
                }

            } else {
                // Create default message
                mcc.util.dbg("Running runMgmdCommandReq for host " + hostname + " with Cluster creds");
                var msg = {
                    head: getHead("runMgmdCommandReq"),
                    body: {
                        ssh: getSSH(hostname, mcc.gui.getSSHkeybased(), 
                                mcc.gui.getSSHUser(),
                                mcc.gui.getSSHPwd()),
                            hostName: hostname,
                            port: port,
                            mgmd_command: cmd
                    }
                };
            }

            // Call do_post, provide callbacks
            do_post(msg).then(replyHandler(onReply, onError), 
                    errorHandler(msg.head, onError));
        }

    }
}

// Send reqName with body ssh: prop is injected into body
function doReq(reqName, body, cluster, onReply, onError) {
    // Create message
    var msg = {
        head: getHead(reqName),
        body: body
    };
    // Try to fish hostName from message body.
    var hostName_fromBody = "";
    
    try {
        hostName_fromBody = body['command']['file'].hostName;
    }
    catch(err) {
        hostName_fromBody = "";
        mcc.util.dbg("FAILED to obtain HostName from command.");
    }
    mcc.util.dbg("Message is for " + hostName_fromBody);
    
    if (hostName_fromBody != "") {
        if (hostHasCreds(hostName_fromBody)) {
            mcc.storage.hostStorage().getItems({name: hostName_fromBody}).then(
                function (hosts) {
                    if (hosts[0]) {
                        if (hosts[0].getValue("key_auth")) {
                            mcc.util.dbg("Running doReq for host " + hostName_fromBody + " with Host keys");
                            msg.body.ssh = createSSHBlock(hosts[0].getValue("key_usr"), hosts[0].getValue("key_passp"),
                                    hosts[0].getValue("key_file"));
                        } else {
                            mcc.util.dbg("Running doReq for host " + hostName_fromBody + " with Host creds");
                            msg.body.ssh = getSSH(hostName_fromBody, false, 
                                            hosts[0].getValue("usr"),
                                            hosts[0].getValue("usrpwd"))
                        }
                    };
                }
            );
        } else {
            // Do it the old way.
            mcc.util.dbg("Host " + hostName_fromBody + " has no creds.");
            if (mcc.gui.getSSHkeybased()) {
                //New Cluster-LvL SSH code.
                mcc.util.dbg("Running doReq for host " + hostName_fromBody + " with Cluster keys");
                msg.body.ssh = createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
                            mcc.gui.getClSSHKeyFile());
            } else {
                // Create default message
                mcc.util.dbg("Running doReq for host " + hostName_fromBody + " with Cluster creds");
                msg.body.ssh = getSSH(hostName_fromBody, mcc.gui.getSSHkeybased(), 
                                mcc.gui.getSSHUser(),
                                mcc.gui.getSSHPwd());
            }

        }
        // Call do_post, provide callbacks
        do_post(msg).then(replyHandler(onReply, onError), 
                          errorHandler(msg.head, onError));
    } else {
        mcc.util.dbg("FAILED to obtain HostName from command.");
        msg.body.ssh = getSSH(false, "", "");
        // Call do_post, provide callbacks
        do_post(msg).then(replyHandler(onReply, onError), 
                          errorHandler(msg.head, onError));

    }
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Server api module initialized");
});
