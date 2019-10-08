/*
Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

/*
DEPLOYWORKER: We pass HOST and array of commands. Credentials are
not necessary since permanent connections to every host are already set up
by now. Worker can post message to main window so to simulate progress:
    - msg: id, host, currseq/sequence, cmd, succ, terminal, done
id                  - worker ID
host                - HOST ext.IP
currseq/sequence    - progress
cmd                 - command that just finished
succ                - OK/FAILED
terminal            - true/false, is failure terminal
done                - IF (succ == FAILED && terminal) || currseq = sequence
this.postMessage({id: e.data.id, sum: sum});
*/

var seqNo = 0;
var ToRun = []; // Array of commands this worker has to run.
var host = ''; // Host assigned to this worker.
var SSHBlock = {}; // Credentials block to append to JSON message.
var workerID = null; // Id assigned to this worker.

// Get next sequence number
function getSeqNo () {
    seqNo += 2;
    return seqNo;
}

// All of the workers have same sequence...2,4,6...
/**
 *Form proper head member of JSON message to be passed to back end, deploy worker.
 *
 * @param {*} cmd   String, command to be sent, request_handler determines how to handle it from here
 * @returns         JSON, representing HEAD member of message
 */
function getHead (cmd) {
    return { cmd: cmd, seq: getSeqNo() };
}
/**
 *Send synchronous XHR request to back end, deploy worker.
 *
 * @param {String} method 'POST' here always
 * @param {String} url /cmd here always
 * @param {{JSON}} msg message to back end
 * @returns  {dojo.Deferred} Promisse, resolve/reject with info
 */
function makeRequest (method, url, msg) {
    return new Promise(function (resolve, reject) {
        var jsonMsg = JSON.stringify(msg);
        var xhr = new XMLHttpRequest();
        xhr.open(method, url);
        xhr.setRequestHeader('Content-type', 'application/json');
        xhr.onload = function () {
            if (this.status >= 200 && this.status < 300) {
                resolve(xhr.response);
            } else {
                // eslint-disable-next-line prefer-promise-reject-errors
                reject({
                    status: this.status,
                    statusText: xhr.statusText
                });
            }
        };
        xhr.onerror = function () {
            console.error('[ERR]XHR ERROR reject ' + xhr.statusText + ' status ' + this.status);
            reject({
                status: this.status,
                statusText: xhr.statusText
            });
        };
        xhr.send(jsonMsg);
    });
}

/**
 *Generic error handler closure, deploy worker.
 *
 * @param {{JSON}} req Message sent to BE
 * @param {function} onError
 * @returns {function}
 */
function errorHandler (req, onError) {
    if (onError) {
        return onError;
    } else {
        return function (error) {
            console.error("[ERR]An error occurred while executing '" + req.cmd +
                    ' (' + req.seq + ")': " + error);
        };
    }
}

/**
 *Generic reply handler closure, deploy worker.
 *
 * @param {function} onReply
 * @param {function} onError
  */
function replyHandler (onReply, onError) {
    return function (reply) {
        reply = JSON.parse(reply);
        var errM = '';
        if (reply && reply.stat) {
            errM = reply.stat.errMsg + '';
        } else {
            errM = 'OK';
        }
        if (reply && reply.stat && errM !== 'OK') {
            if (onError) {
                onError(reply.stat.errMsg, reply);
            } else {
                console.error(errM);
            }
        } else {
            onReply(reply);
        }
    };
}

self.onmessage = function (e) {
    var data = e.data;
    switch (data.cmd) {
        case 'start':
            console.debug('[DBG]Worker' + workerID + '@' + host + ' responding to START.');
            self.postMessage({ Id: workerID, host: host, progress: '0', cmd: 'STARTED', succ: 'OK', terminal: false, done: false });
            DeployHost(host, ToRun, workerID);
            break;
        case 'stop':
            // To be able to use same response handler. Otherwise, too much info for simple STOP.
            console.debug('[DBG]Worker' + workerID + '@' + host + ' responding to STOP.');
            if (!data.Terminal) {
                // Resolve to TRUE.
                self.postMessage({ Id: workerID, host: host, progress: '100', cmd: 'STOPPED', succ: 'OK', terminal: false, done: true });
            } // else, main code will resolve to false.
            self.close(); // Terminates the worker.
            break;
        case 'receive':
            workerID = data.Id;
            ToRun = data.msg;
            host = data.host;
            console.debug('[DBG]Worker' + workerID + '@' + host + ' responding to RECV. Total # of commands to run is ' + ToRun.length);
            SSHBlock = data.SSHBlock;
            self.postMessage({ Id: workerID, host: host, progress: '0', cmd: 'RECEIVED', succ: 'OK', terminal: false, done: false });
            break;
        default:
            self.postMessage({ Id: workerID, host: host, cmd: 'Unknown command: ' + data.msg });
    }
};

function doRequest (message, onReply, onError) {
    makeRequest('POST', '/cmd', message).then(replyHandler(onReply, onError), errorHandler(message.head, onError));
}
/**
 *Main function to deploy Cluster on hosts.
 *
 * @param {String} host name of host this worker is assigned to
 * @param {Number} Id ThreadId this worker is assigned
 */
function DeployHost (host, Id) {
    var currSeq = 0;
    var msg = {};
    var total = ToRun.length;

    function onError (errMsg, errReply, terminal) {
        if (terminal === undefined) {
            console.error('[ERR]Worker[' + workerID + ' reports terminal(undefined) error ' + errMsg);
            terminal = true;
        }
        if (!terminal) {
            console.error('[ERR]Worker[' + workerID + ' reports error ' + errMsg);
            if (ToRun[currSeq].filename) {
                self.postMessage({
                    Id: workerID,
                    host: host,
                    progress: currSeq + '/' + total,
                    cmd: 'Creating file ' + ToRun[currSeq].path + ToRun[currSeq].filename + ' on host ' +
                        ToRun[currSeq].host,
                    err: errMsg,
                    succ: 'FAILED',
                    terminal: terminal,
                    done: false });
            } else {
                self.postMessage({
                    Id: workerID,
                    host: host,
                    progress: currSeq + '/' + total,
                    cmd: 'Creating directory ' + ToRun[currSeq].path + ' on host ' + ToRun[currSeq].host,
                    err: errMsg,
                    succ: 'FAILED',
                    terminal: terminal,
                    done: false });
            }
            ++currSeq;
        }
        updateProgressAndDeployNext(terminal);
    }

    function onReply (rep) {
        var cmdmsg = '';
        if (ToRun[currSeq].filename) {
            cmdmsg = 'Creating file ' + ToRun[currSeq].path + ToRun[currSeq].filename + ' on host ' + ToRun[currSeq].host;
        } else {
            cmdmsg = 'Creating directory ' + ToRun[currSeq].path + ' on host ' + ToRun[currSeq].host;
        }
        self.postMessage({
            Id: workerID,
            host: host,
            progress: currSeq + '/' + total,
            cmd: cmdmsg,
            succ: 'OK',
            terminal: false,
            done: false });
        ++currSeq;
        updateProgressAndDeployNext(false);
    }

    function updateProgressAndDeployNext (terminated) {
        if (terminated) {
            self.postMessage({
                Id: workerID,
                host: host,
                progress: currSeq,
                cmd: 'TERMINALERR',
                succ: 'OK',
                terminal: true,
                done: true });
            return;
        } else {
            if (currSeq >= total) {
                var mess = 'Cluster deployed successfully';
                self.postMessage({
                    Id: workerID,
                    host: host,
                    progress: '100',
                    cmd: mess,
                    succ: 'OK',
                    terminal: false,
                    done: true });
                return;
            }
        }
        // Prep the message:
        msg = {
            head: getHead('createFileReq'),
            body: { command: ToRun[currSeq] }
        };

        msg.body.ssh = SSHBlock;
        msg.body.file = {
            hostName: ToRun[currSeq].host,
            path: ToRun[currSeq].path
        };
        if (ToRun[currSeq].filename) {
            msg.body.file.name = ToRun[currSeq].filename;
        }
        if (ToRun[currSeq].contents) {
            msg.body.contentString = ToRun[currSeq].contents;
        }
        if (ToRun[currSeq].overwrite) {
            msg.body.file.overwrite = ToRun[currSeq].overwrite;
        }

        doRequest(msg, onReply, onError);
    }
    // Initiate deploy sequence
    updateProgressAndDeployNext(false);
}
