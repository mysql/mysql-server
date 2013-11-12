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
 ***                       Unit tests: Assert utilities                     ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.tests.assert
 *
 *  Test cases: 
 *      Positive: 
 *          posAssertFalse: Make assert fail, check exception
 *          posAssertTrue: Make assert not fail, check no exception
 *
 *      Negative: 
 *          None
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.tests.assert");

dojo.require("mcc.util");

/******************************* Test cases  **********************************/

// Make and assert fail, verify that an exception is thrown
function posAssertFalse() {
    try {
        mcc.util.assert(false, "False");
        // Should never get here
        doh.t(false);
    } catch (e) {
        mcc.util.tst("Got exception as expected");
        doh.t(true);
    }
}

// Make and assert fail, verify that an exception is thrown
function posAssertTrue() {
    try {
        mcc.util.assert(true, "True");
        // Should get here
        doh.t(true);
    } catch (e) {
        mcc.util.tst("Got exception, not as expected");
        doh.t(false);
    }
}

/*************************** Register test cases  *****************************/

doh.register("mcc.util.tests.assert", [
    posAssertFalse,
    posAssertTrue
]);
