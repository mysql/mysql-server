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
 ***                    Unit tests: Cookie handling                         ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.tests.cookies
 *
 *  Test cases: 
 *      Positive: 
 *          posVerifyPredefinedTypes: Predefined values shall have type "string"
 *          posResetWithoutCookies: Reset shall assign default values to predef
 *          posResetWithPredefinedCookies: Reset shall keep predef w survive
 *          posResetWithCookies: Reset shall remove non-predef
 *          posStartWithoutCookies: Initial start shall set predefined
 *          posStartWithCookies: Start with non-predef shall keep cookies
 *          posStartWithCookiesAndUpgrade: Start with non-predef shall delete
 *          posCleanup: Cleanup after tests to reset default values
 * 
 *      Negative: 
 *
 *  Todo:
 *      Implement negative test cases
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.tests.cookies");

dojo.require("mcc.util");

/******************************* Test cases  **********************************/

// Check that all predefined values have default type string
function posVerifyPredefinedTypes() {
    // Assert that we have an array
    doh.t(mcc.util.cookies.predefinedCookies());
    // Check each entry
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + ")");
        doh.t(mcc.util.cookies.predefinedCookies(p).value == null);
        doh.t(mcc.util.cookies.predefinedCookies(p).defaultValue != null);
        doh.t(mcc.util.cookies.predefinedCookies(p).defaultValue != undefined);
        doh.t(typeof(mcc.util.cookies.predefinedCookies(p).defaultValue) == 
                "string");
    }
    // Assert that length is 5
    doh.t(i == 5);
}

// Check that reset with no cookies present assigns default values
function posResetWithoutCookies() {
    // Delete all cookies
    mcc.util.cookies.deleteCookies();

    // Verify values
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + ") deleted");
        doh.t(mcc.util.getCookie(p) == undefined);
        doh.t(typeof(mcc.util.getCookie(p)) == "undefined");
        doh.t(mcc.util.getCookie(p) === undefined);
    }
    // Assert that length is 5
    doh.t(i == 5);

    // Reset cookies
    mcc.util.cookies.resetCookies();

    // Check that predefined cookies have default values
    i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + ") got default value");
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue);
    }
    // Assert that length is 5
    doh.t(i == 5);
}

// Check reset with non-default values for predef cookies retains values
function posResetWithPredefinedCookies() {
    // Delete all cookies
    mcc.util.cookies.deleteCookies();

    // Verify values
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking that " + p + " is deleted");
        doh.t(mcc.util.getCookie(p) == undefined);
        doh.t(typeof(mcc.util.getCookie(p)) == "undefined");
        doh.t(mcc.util.getCookie(p) === undefined);
    }

    // Set non-default values for all predefined cookies
    for (var p in mcc.util.cookies.predefinedCookies()) {
        var nonDefaultValue = null;
        nonDefaultValue = mcc.util.cookies.predefinedCookies(p).defaultValue + 
                "non-default";
        mcc.util.tst("Assign non-default value to " + p + ": " + 
                nonDefaultValue);
        mcc.util.setCookie(p, nonDefaultValue);
    }

    // Reset cookies (keep the non-default values)
    mcc.util.cookies.resetCookies();

    // Check that predefined cookies have kept the non-default values
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + "): " + 
                mcc.util.getCookie(p));
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        if (mcc.util.cookies.predefinedCookies(p).survive) {
            doh.t(mcc.util.getCookie(p) == 
                    mcc.util.cookies.predefinedCookies(p).defaultValue + 
                            "non-default");
        } else {
            doh.t(mcc.util.getCookie(p) == 
                    mcc.util.cookies.predefinedCookies(p).defaultValue);
        }
    }
    doh.t(i == 5);
}

// Check that reset with cookies present deletes non-predefined cookies
function posResetWithCookies() {
    // First delete, then reset cookies to get default values only
    mcc.util.cookies.deleteCookies();
    mcc.util.cookies.resetCookies();

    // Store a test cookie
    mcc.util.tst("Set test cookie");
    mcc.util.setCookie("testCookieName", "testCookieValue");

    // Reset cookies (will delete non-predefined)
    mcc.util.cookies.resetCookies();

    // Verify that test cookie is gone
    mcc.util.tst("Verify test cookie gone: " +  
            mcc.util.getCookie("testCookieName"));
    doh.t(mcc.util.getCookie("testCookieName") == undefined);
    doh.t(typeof(mcc.util.getCookie("testCookieName")) == "undefined");
    doh.t(mcc.util.getCookie("testCookieName") === undefined);

    // Check that predefined kept their default value
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + "): " + 
                mcc.util.getCookie(p));
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue);
    }
    doh.t(i == 5);
}

// Simulate a session starting without any existing cookies
function posStartWithoutCookies() {
    // Delete all cookies
    mcc.util.cookies.deleteCookies();

    // Verify values
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + ") deleted");
        doh.t(mcc.util.getCookie(p) == undefined);
        doh.t(typeof(mcc.util.getCookie(p)) == "undefined");
        doh.t(mcc.util.getCookie(p) === undefined);
    }
    doh.t(i == 5);

    // Retrieve cookies (simulate an ordinary start with version upgrade)
    mcc.util.cookies.retrieveCookies();

    // Check that predefined cookies have default values
    i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + "): " + 
                mcc.util.getCookie(p));
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue);
    }
    doh.t(i == 5);
}

// Simulate a session starting with existing cookies
function posStartWithCookies() {
    // First delete, then reset cookies to get default values only
    mcc.util.cookies.deleteCookies();
    mcc.util.resetCookies();

    // Store a test cookie
    mcc.util.tst("Set test cookie");
    mcc.util.setCookie("testCookieName", "testCookieValue");

    // Delete the cookie array (not the cookies themselves)
    mcc.util.tst("Delete cookie array");
    mcc.util.cookies.setupAllCookies();

    // Retrieve cookies (simulate an ordinary start without version upgrade)
    mcc.util.cookies.retrieveCookies();

    // Verify that test cookie value is retrieved
    mcc.util.tst("Verify cookie: " +  mcc.util.getCookie("testCookieName"));
    doh.t(mcc.util.getCookie("testCookieName") == "testCookieValue");
    doh.t(typeof(mcc.util.getCookie("testCookieName")) == "string");
    doh.t(mcc.util.getCookie("testCookieName") === "testCookieValue");

    // Check that predefined kept their default value
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + "): " + 
                mcc.util.getCookie(p));
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue);
    }
    doh.t(i == 5);
}

// Simulate a session starting with existing cookies and version upgrade
function posStartWithCookiesAndUpgrade() {
    // Save old default to be restored
    var oldVer = mcc.util.cookies.predefinedCookies("storeVersion").
            defaultValue;

    // First delete, then reset cookies to get default values only
    mcc.util.cookies.deleteCookies();
    mcc.util.resetCookies();

    // Set non-default values for all predefined cookies
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        var nonDefaultValue= null;
        nonDefaultValue= mcc.util.cookies.predefinedCookies(p).defaultValue + 
                "non-default";
        mcc.util.tst("Assign non-default value to " + p + 
                " (#" + ++i + "): " + nonDefaultValue);
        mcc.util.setCookie(p, nonDefaultValue);
    }
    doh.t(i == 5);

    // Store a test cookie
    mcc.util.tst("Set test cookie");
    mcc.util.setCookie("testCookieName", "testCookieValue");

    // Delete the cookie array (not the cookies themselves)
    mcc.util.tst("Delete cookie array");
    mcc.util.cookies.setupAllCookies();

    // Change store version to force upgrade
    mcc.util.cookies.predefinedCookies("storeVersion").defaultValue += "new";

    // Retrieve cookies (simulate an ordinary start with version upgrade)
    mcc.util.cookies.retrieveCookies();

    // Verify that test cookie is gone
    mcc.util.tst("Verify test cookie gone: " +  
            mcc.util.getCookie("testCookieName"));
    doh.t(mcc.util.getCookie("testCookieName") == undefined);
    doh.t(typeof(mcc.util.getCookie("testCookieName")) == "undefined");
    doh.t(mcc.util.getCookie("testCookieName") === undefined);

    // Check that predefined cookies have kept the non-default values
    i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + "): " + 
                     mcc.util.getCookie(p));
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        if (mcc.util.cookies.predefinedCookies(p).survive) {
            doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue
                        + "non-default");
        } else {
            doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue);
        }
    }
    doh.t(i == 5);
    // Restore the old version
    mcc.util.cookies.predefinedCookies("storeVersion").defaultValue = oldVer;
}

// Delete cookies and revert to defaults to avoid mess
function posCleanup() {
    // Delete, then reset cookies to get default values only
    mcc.util.cookies.deleteCookies();
    mcc.util.resetCookies();

    // Check that predefined have their default value
    var i = 0;
    for (var p in mcc.util.cookies.predefinedCookies()) {
        mcc.util.tst("Checking " + p + " (#" + ++i + "): " + 
                mcc.util.getCookie(p));
        doh.t(mcc.util.getCookie(p) != undefined);
        doh.t(mcc.util.getCookie(p) != null);
        doh.t(typeof(mcc.util.getCookie(p)) != "undefined");
        doh.t(mcc.util.getCookie(p) !== undefined);
        doh.t(mcc.util.getCookie(p) == 
                mcc.util.cookies.predefinedCookies(p).defaultValue);
    }
    doh.t(i == 5);
}

/*************************** Register test cases  *****************************/

doh.register("mcc.util.tests.cookies", [
    posVerifyPredefinedTypes,
    posResetWithoutCookies,
    posResetWithPredefinedCookies,
    posResetWithCookies,
    posStartWithoutCookies,
    posStartWithCookies,
    posStartWithCookiesAndUpgrade,
    posCleanup
]);

