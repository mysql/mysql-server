/*
Copyright (c) 2012, 2019 Oracle and/or its affiliates. All rights reserved.

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
 ***                   Logging to the console in the browser                ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.util.log
 *
 *  Description:
 *      Interface to write log messages to the console
 *
 *  External interface:
 *      mcc.util.log.inf: Information
 *      mcc.util.log.dbg: Debug
 *      mcc.util.log.tst: Test
 *      mcc.util.log.wrn: Warning
 *      mcc.util.log.err: Error
 *      mcc.util.log.isEmpty: check if value is empty
 *      mcc.util.log.displayModal
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *      doLog: Write the log message
 *
 *  Internal data:
 *      logDestination: Function to receive text string
 *
 *  Unit test interface:
 *      mcc.util.log.setDestination: Set log destination
 *      mcc.util.log.resetDestination: Reset to default (console.log)
 *
 *  Todo:
 *      Toggle log on/off different log types
 *      Filter based on caller function name/module
 *      Support for dumping arrays, objects etc.
 *
 ******************************************************************************/

/****************************** Import/export *********************************/
dojo.provide('mcc.util.log');

dojo.require('mcc.util');

/**************************** External interface ******************************/
/* mcc.util.log.inf = inf;
mcc.util.log.dbg = dbg;
mcc.util.log.tst = tst;
mcc.util.log.wrn = wrn;
mcc.util.log.err = err;*/
mcc.util.log.padR = padR;
mcc.util.log.isEmpty = isEmpty;
mcc.util.log.displayModal = displayModal;

/******************************* Internal data ********************************/
var logDestination = window.console.log;

/******************************* Implementation *******************************/
function padR (width, string, padding) {
    return (width <= string.length) ? string : padR(width, string + padding, padding);
}
/*
function doLog (logFunction, logType, logMessage) {
    if (!logType || typeof (logType) != 'string') {
        throw new TypeError();
    }
    if (!logMessage || (typeof (logMessage) != 'string' && typeof (logMessage) != 'number')) {
        throw new TypeError();
    }
    if (logFunction) {
        if (typeof (logFunction) == 'function') {
            logFunction.apply(window.console, [logType + ' ' + logMessage]);
        } else if (typeof (logFunction) == 'object') {
            logFunction(logType + ' ' + logMessage);
        }
    }
}
*/
/*
function inf (logMessage) {
    doLog(logDestination, '[INF]', logMessage);
}

function dbg (logMessage) {
    doLog(logDestination, '[DBG]', logMessage);
}

function tst (logMessage) {
    doLog(logDestination, '[TST]', logMessage);
}

function wrn (logMessage) {
    doLog(logDestination, '[WRN]', logMessage);
}

function err (logMessage) {
    doLog(logDestination, '[ERR]', logMessage);
}
*/
/**
 *Determines if value passed is actually empty.
 *
 * @param {object} val   Value to check.
 * @returns {boolean} "emptiness" status of object
 */
function isEmpty (val) {
    return !!((val === undefined || val == null || val.length <= 0 ||
        (Object.keys(val).length === 0 && val.constructor === Object)));
}

/**************************** Unit test interface *****************************/
if (mcc.util.tests) {
    mcc.util.log.setDestination = function (dest) {
        logDestination = dest;
    };
    mcc.util.log.resetDestination = function () {
        logDestination = console.log;
    };
}

/**
 *Function to display simple modal box.
 *
 * @param {string} type I - just body node, H - body & header, F - body, header and footer nodes
 * @param {number} ttl  if set, time to autoclose in seconds ( <= 0 infinite, max 5sec)
 * @param {string} body  HTML string going into body node
 * @param {string} header HTML string going into header node
 * @param {string} footer  HTML string going into footer node
 */
function displayModal (type, ttl, body, header, footer) {
    var modal, span;
    if (ttl > 5) {
        // no point in having window linger around for more than 5 seconds
        console.warn('[WRN]Modal[TTL]' + ttl + ' too long, setting to 5s.');
        ttl = 5;
    }
    switch (type) {
        case 'I':
            modal = document.getElementById('myModalInfo');
            span = document.getElementsByClassName('closei')[0];
            document.getElementById('myModalInfoBodyText').innerHTML = body;
            break;
        case 'H':
            modal = document.getElementById('myModalInfoH');
            span = document.getElementsByClassName('closeih')[0];
            document.getElementById('myModalInfoHHeader').innerHTML = header;
            document.getElementById('myModalInfoHBodyText').innerHTML = body;
            break;
        case 'F':
            modal = document.getElementById('myModalInfoH');
            span = document.getElementsByClassName('closeihf')[0];
            document.getElementById('myModalInfoHFHeader').innerHTML = header;
            document.getElementById('myModalInfoHFBodyText').innerHTML = body;
            document.getElementById('myModalInfoHFFooter').innerHTML = footer;
            break;
    }
    span.onclick = function () {
        modal.style.display = 'none';
    }
    // When the user clicks anywhere outside of the modal, close it
    window.onclick = function (event) {
        if (event.target === modal) {
            modal.style.display = 'none';
        }
    }
    modal.style.display = 'block';
    modal.focus();
    if (ttl > 0) {
        setTimeout(function () {
            modal.style.display = 'none';
        }, 1000 * ttl);
    }
}
/******************************** Initialize **********************************/
dojo.ready(function () {
    console.info('[INF]Logging module initialized');
});
