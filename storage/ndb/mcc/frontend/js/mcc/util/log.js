/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.log");

dojo.require("mcc.util");

/**************************** External interface  *****************************/

mcc.util.log.inf = inf;
mcc.util.log.dbg = dbg;
mcc.util.log.tst = tst;
mcc.util.log.wrn = wrn;
mcc.util.log.err = err;

/******************************* Internal data ********************************/

var logDestination = window.console.log; 

/****************************** Implementation  *******************************/

function doLog(logFunction, logType, logMessage) {

    if (!logType || typeof(logType) != "string") {
        throw new TypeError;
    }
    if (!logMessage || (typeof(logMessage) != "string" && 
                        typeof(logMessage) != "number")) {
        throw new TypeError;
    }
    if (logFunction) {
        if (typeof(logFunction) == "function") {
            logFunction.apply(window.console, [logType + " " + logMessage]);
        } else if (typeof(logFunction) == "object") {
            logFunction(logType + " " + logMessage);
        }
    }
}

function inf(logMessage) {
    doLog(logDestination, "[INF]", logMessage);
}

function dbg(logMessage) {
    doLog(logDestination, "[DBG]", logMessage);
}

function tst(logMessage) {
    doLog(logDestination, "[TST]", logMessage);
}

function wrn(logMessage) {
    doLog(logDestination, "[WRN]", logMessage);
}

function err(logMessage) {
    doLog(logDestination, "[ERR]", logMessage);
}

/**************************** Unit test interface  ****************************/

if (mcc.util.tests) {
    mcc.util.log.setDestination = function (dest) {
        logDestination = dest;
    }

    mcc.util.log.resetDestination = function () {
        logDestination = console.log;
    }
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Logging module initialized");
});

