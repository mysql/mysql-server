/*
Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

/*
INSTALLWORKER: We pass HOST and array of commands. Credentials are
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
/**
 *Form proper head member of JSON message to be passed to back end, install worker.
 *
 * @param {*} cmd   String, command to be sent, request_handler determines how to handle it from here
 * @returns         JSON, representing HEAD member of message
 */
function getHead (cmd) {
    return { cmd: cmd, seq: getSeqNo() };
}
/**
 *Send synchronous XHR request to back end, install worker.
 *
 * @param {String} method 'POST' here always
 * @param {String} url /cmd here always
 * @param {JSON} msg message to back end
 * @returns {dojo.Deferred} Promisse, resolve/reject with info
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
            console.error('[ERR]XHR ERRO reject ' + xhr.statusText + ' status ' + this.status);
            // eslint-disable-next-line prefer-promise-reject-errors
            reject({
                status: this.status,
                statusText: xhr.statusText
            });
        };
        xhr.send(jsonMsg);
    });
}

/**
 *Generic error handler closure, install worker.
 *
 * @param {JSON} req Message received from BE
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
 *Generic reply handler closure, install worker.
 *
 * @param {function} onReply
 * @param {function} onError
  */
function replyHandler (onReply, onError) {
    return function (reply) {
        reply = JSON.parse(reply);
        console.debug('[DBG]InstWorker reply is %o', reply)
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
                console.error('[ERR]' + errM);
            }
        } else {
            onReply(reply);
        }
    };
}

self.onmessage = function (e) {
    var data = e.data;
    var body = data.msg;
    /*
    BODY structure is ARRAY of:
        body.html = {host: h, path: p, name: n, optionString: "<tr><td><b>Options</b></td>"};
        body.msg = {
            file: {hostName: h, path: p, name: n, autoComplete: true}, // NAME is command, like SUDO
            procCtrl: {getStd: true, waitForCompletion: true},
            params: {sep: " ", param: [body.msg.params.param.push(pa)]}
        };
        body.isDone = function () { return true; };
    */
    if (workerID) {
        if (data.Id) {
            if (Number(workerID) !== Number(data.Id)) {
                console.error('[ERR]This is not good... Worker' + workerID + ' got message for ' + data.Id);
            }
        }
    }

    switch (data.cmd) {
        case 'start':
            console.debug('[DBG]Worker' + workerID + '@' + host + ' responding to START.');
            self.postMessage({ Id: workerID, host: host, progress: '0', cmd: 'STARTED', succ: 'OK', terminal: false, done: false });
            InstallHost(host, ToRun, workerID);
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
            ToRun = body;
            try {
                // Take *any* command since they are all for the same host.
                host = ToRun[0]['file'].hostName;
            } catch (err) {
                host = data.host;
                // This is terminal...
                console.error('[ERR]FAILED to obtain HostName from command.');
            }
            console.debug('[DBG]Worker' + workerID + '@' + host + ' responding to RECV.\nTotal # of commands to run is ' + ToRun.length);
            SSHBlock = data.SSHBlock;
            // If data.host <> ToRun[0]['file'].hostName =>> trouble...
            self.postMessage({ Id: workerID, host: host, progress: '0', cmd: 'RECEIVED', succ: 'OK', terminal: false, done: false });
            break;
        default:
            self.postMessage('Unknown command: ' + data.msg);
    }
};

function doRequest (message, onReply, onError) {
    makeRequest('POST', '/cmd', message).then(replyHandler(onReply, onError), errorHandler(message.head, onError));
}

function InstallHost (host, Id) {
    var currSeq = 0;
    var errorReplies = 0;
    var timeout;
    var msg = {};
    var total = ToRun.length;

    function onTimeout () {
        if (ToRun[currSeq].isDone) {
            ++currSeq;
            updateProgressAndinstallNext(false);
        } else {
            timeout = setTimeout(onTimeout, 1000);
        }
    }

    function onError (errMsg, errReply, terminal) {
        if (terminal === undefined) {
            terminal = true;
        }
        ++errorReplies;
        self.postMessage({ Id: workerID, host: host, progress: currSeq + '/' + total, cmd: ToRun[currSeq].toRun, err: errMsg, succ: 'FAILED', terminal: terminal, done: false });
        if (!terminal) {
            ++currSeq;
        }
        updateProgressAndinstallNext(terminal);
    }

    function onReply (rep) {
        if (Number(rep.body.exitstatus) === 0) {
            // move on
            ToRun[currSeq].isDone = true;
            self.postMessage({ Id: workerID, host: host, progress: currSeq + '/' + total, cmd: ToRun[currSeq].toRun, succ: 'OK', terminal: false, done: false });
        }
        if ((Number(rep.body.exitstatus) > 0) && (String(ToRun[currSeq].params.param[0].name) === 'rpm')) {
            // Check what's going on with RPM. search -1 means NOT FOUND. 0 is FOUND.
            if (rep.body.err[0].search('warning') === 0) {
                // Just a warning, proceed.
                ToRun[currSeq].isDone = true;
                self.postMessage({ Id: workerID, host: host, progress: currSeq + '/' + total, cmd: ToRun[currSeq].toRun, succ: 'OK', terminal: false, done: false });
            } else {
                ToRun[currSeq].isDone = true;
                onError(rep.body.err, rep, false);
                return;
            }
        } else {
            if (String(ToRun[currSeq].params.param[0].name) === 'systemctl') {
                if (Number(rep.body.exitstatus) === 5) {
                    // Probably just fresh box and no service to STOP. Just record the error and move on.
                    ToRun[currSeq].isDone = true;
                    onError(rep.body.err, rep, false);
                    return;
                } else {
                    if (Number(rep.body.exitstatus) === 1) {
                        // Probably just box and without service to STOP. Just record the error and move on.
                        ToRun[currSeq].isDone = true;
                        onError(rep.body.err, rep, false);
                        return;
                    }
                }
            } else {
                // Unhandled errors. !RPM, !SYSTEMCTL.
                if (rep.body.exitstatus >= 1) {
                    // ERROR, check if terminal
                    console.error('[ERR]Worker' + workerID + '::OnReply, TERMINAL ERROR, ' + ToRun[currSeq].toRun + '@' + ToRun[currSeq].file.hostName);
                    ToRun[currSeq].isDone = true;
                    onError(rep.body.err, rep, true);
                    return;
                }
            }
        }
        onTimeout();
    }

    function updateProgressAndinstallNext (terminated) {
        if (terminated) {
            self.postMessage({ Id: workerID, host: host, progress: currSeq, cmd: 'TERMINALERR', succ: 'OK', terminal: true, done: true });
            clearTimeout(timeout);
            return;
        } else {
            if (currSeq >= ToRun.length) {
                var mess = errorReplies ? 'Install procedure has completed, but ' + errorReplies + ' out of ' +
                    ToRun.length + ' commands failed' : 'Cluster installed successfully';
                clearTimeout(timeout);
                self.postMessage({ Id: workerID, host: host, progress: '100', cmd: mess, succ: 'OK', terminal: false, done: true });
                return;
            }
        }
        // Prep the message:
        msg = {
            head: getHead('executeCommandReq'),
            body: { command: ToRun[currSeq] }
        };
        msg.body.ssh = SSHBlock;
        doRequest(msg, onReply, onError);
    }
    // Initiate install sequence
    console.debug('[DBG]Worker' + workerID + ' updateProgressAndinstallNext, initiate.');
    updateProgressAndinstallNext(false);
}
