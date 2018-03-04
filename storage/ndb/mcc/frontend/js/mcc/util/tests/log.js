/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/******************************************************************************
 ***                                                                        ***
 ***           Unit tests: Logging to the console in the browser            ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.tests.log
 *
 *  Test cases: 
 *      Positive: 
 *          posLogtypes
 *          posArguments
 *
 *      Negative: 
 *          negNull
 *          negUndefined
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.tests.log");

dojo.require("mcc.util");

/******************************* Test cases  **********************************/

function posLogtypes() {
    var output = "";
    mcc.util.log.setDestination(function (str) {
        output = str;
    });
    mcc.util.inf("inf");
    doh.t(output == "[INF] inf");

    mcc.util.dbg("dbg");
    doh.t(output == "[DBG] dbg");

    mcc.util.tst("tst");
    doh.t(output == "[TST] tst");

    mcc.util.wrn("wrn");
    doh.t(output == "[WRN] wrn");

    mcc.util.err("err");
    doh.t(output == "[ERR] err");

    mcc.util.log.resetDestination();
}

function posArguments() {
    var output = "";
    mcc.util.log.setDestination(function (str) {
        output = str;
    });

    mcc.util.inf("Hei");
    doh.t(output == "[INF] Hei");

    mcc.util.inf("Hei" + " du");
    doh.t(output == "[INF] Hei du");

    mcc.util.inf(1);
    doh.t(output == "[INF] 1");

    mcc.util.inf(1 + 1);
    doh.t(output == "[INF] 2");

    mcc.util.inf(+"1" + 1);
    doh.t(output == "[INF] 2");

    mcc.util.inf("1" + 1);
    doh.t(output == "[INF] 11");

    mcc.util.log.resetDestination();
}

function negNull() {
    var ex = null; 
    try {
        mcc.util.inf(null);
    } catch (e) {
        ex = e;
        if (e instanceof TypeError) {
            console.log("Type error as expected");
        } else { 
            console.log("Unexpected exception: " + e.constructor.name);
            throw e;
        }
    } finally {
        doh.t(ex instanceof TypeError);
    }
}

function negUndefined() {
    var ex = null; 
    try {
        mcc.util.inf(undefined);
    } catch (e) {
        ex = e;
        if (e instanceof TypeError) {
            console.log("Type error as expected");
        } else { 
            console.log("Unexpected exception: " + e.constructor.name);
            throw e;
        }
    } finally {
        doh.t(ex instanceof TypeError);
    }
}

/*************************** Register test cases  *****************************/

doh.register("mcc.util.tests.log", [
    posLogtypes,
    posArguments,
    negNull,
    negUndefined
]);
