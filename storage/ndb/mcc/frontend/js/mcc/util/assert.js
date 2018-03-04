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
 ***                             Assertion utilities                        ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.assert
 *
 *  Description:
 *      Utilities to check assertions and throw exceptions if not met
 *
 *  External interface: 
 *      mcc.util.assert: Assert expression with text message
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
 *
 *  Internal data: 
 *      assertExc: Anonymous assert exception class
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.assert");

dojo.require("mcc.util");


/**************************** External interface  *****************************/

mcc.util.assert.assert = assert;

/****************************** Implementation  *******************************/

// Anonymous exception class
var assertExc = dojo.declare(null, {
    msg: null,
    constructor: function (msg) {
        this.msg = msg;
    },
    toString: function () {
        return ("AssertException: " + this.msg);
    }
});

// Check expression, if false, throw exception
function assert(expression, message) {
    if (!expression) {
        throw new assertExc(message);
    }
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Assertion module initialized");
});

