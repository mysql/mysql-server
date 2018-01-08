/*
Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
 ***                          Cookie handling                               ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.cookies
 *
 *  Description:
 *      Interface to cookie handling
 *
 *  External interface: 
 *      mcc.util.cookies.getCookie: Get value for given cookie name
 *      mcc.util.cookies.setCookie: Set cookie to give value
 *      mcc.util.cookies.resetCookies: Clear all cookies, keep predefined
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      setupAllCookies: Setup array for all cookies
 *      setupPredefinedCookies: Assign all predefined cookies
 *      readCookies: Read all cookies from the browser context
 *      deleteCookies: Delete all cookies by resetting expiration
 *      retrieveCookies: Read cookies, check version re-read if required
 *      setCookieExpiration: Set cookie name, value and expiration
 *
 *  Internal data: 
 *      allCookies: Object with all cookies
 *      predefinedCookies: Predefined with defaults, some surviving clear config
 * 
 *  Unit test interface: 
 *      mcc.util.cookies.predefinedCookies: Get predefined cookie (or array)
 *      mcc.util.cookies.setupAllCookies: See setupAllCookies
 *      mcc.util.cookies.deleteCookies: See deleteCookies
 *      mcc.util.cookies.retrieveCookies: See retrieveCookies
 * 
 *  Todo:
 *      Exception handling
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.cookies");

dojo.require("mcc.util");

/**************************** External interface  *****************************/

mcc.util.cookies.getCookie = getCookie;
mcc.util.cookies.setCookie = setCookie;
mcc.util.cookies.resetCookies = resetCookies;

/******************************* Internal data ********************************/

// Note that cookie datatypes are strings, except those that are json structs
var allCookies = null; 
var predefinedCookies = null;

/****************************** Implementation  *******************************/

// Setup array for all cookies
function setupAllCookies() {
    // Delete current object if there is one
    if (allCookies) {
        allCookies = null; 
    }
    allCookies = {};
}

// Setup and assign predefined cookies
function setupPredefinedCookies() {
    // Delete current object if there is one
    if (predefinedCookies) {
        predefinedCookies = null; 
    }
    predefinedCookies = {};

    // Assign all individul cookies
    predefinedCookies["storeVersion"] = {
        value: null,
        survive: false,
        defaultValue: "0.2.6"
    }; 
    predefinedCookies["configLevel"] = {
        value: null,
        survive: true,
        defaultValue: "simple"
    };
    predefinedCookies["autoSave"] = {
        value: null,
        survive: true,
        defaultValue: "on"
    };
    predefinedCookies["getHostInfo"] = {
        value: null,
        survive: true,
        defaultValue: "on"
    };
    predefinedCookies["expiration"] = {
        value: null,
        survive: true,
        defaultValue: "5"
    };
}

// Clear cookie object, read all cookies, remove "mcc_" prefix from key
function readCookies() {
    mcc.util.dbg("Read cookies");

    // Delete and reset object
    setupAllCookies();

    // Get and iterate over all cookies
    var cookies = document.cookie.split(";");
    for (var i in cookies) {
        var key = cookies[i].substr(0, dojo.indexOf(cookies[i], "="));
        var value = unescape(cookies[i].substr(dojo.indexOf(cookies[i], "=") + 
                1));
        // Strip
        key = key.replace(/^\s+|\s+$/g,"");

        // Only those with mcc prefix
        if (key.substr(0, 4) == "mcc_") {
            key = key.substr(4);
            // Store in object
            allCookies[key] = value;
        }
    }
}

// Delete all cookies by resetting expiration, clear internal array
function deleteCookies() {
    mcc.util.dbg("Delete cookies");

    // Get and iterate over all cookies
    var cookies = document.cookie.split(";");
    for (var i in cookies) {
        var key = cookies[i].substr(0, dojo.indexOf(cookies[i], "="));
        // Strip
        key = key.replace(/^\s+|\s+$/g,"");
        // Only those with mcc prefix
        if (key.substr(0, 4) == "mcc_") {
            // Reset expiration
            setCookieExpiration(key, null, -1);
        }
    }

    // Delete and reset object
    setupAllCookies();
}

// Delete all cookies by resetting expiration. Carry over some cookies
function resetCookies() {
    mcc.util.dbg("Reset cookies");

    // Check all predefined
    for (var i in predefinedCookies) {

        // Remember those with a value that should survive
        if (allCookies[i] && 
            predefinedCookies[i].survive) {
            mcc.util.dbg("Remember value for predefined cookie " + i + 
                         ": " + allCookies[i]);
            predefinedCookies[i].value = allCookies[i];
        }
    }

    // Delete all cookies
    deleteCookies();

    // Restore the surviving predefined ones
    for (var i in predefinedCookies) {

        // Restore default if no value
        if (!predefinedCookies[i].value) {
            mcc.util.dbg("Assign default value for " + i + 
                         ": " + predefinedCookies[i].defaultValue);
            setCookie(i, predefinedCookies[i].defaultValue);

        // Restore value if surviving
        } else if (predefinedCookies[i].survive) {
            mcc.util.dbg("Carry over value for " + i + 
                         ": " + predefinedCookies[i].value);
            setCookie(i, predefinedCookies[i].value);
        }

        // Reset remembered value anyway
        predefinedCookies[i].value = null;
    }
}

// Retrieve all cookies, check version, clear if required
function retrieveCookies() {
    mcc.util.dbg("Retrieve cookies");

    // Read all cookies
    readCookies(); 

    // Check current version vs. stored version
    if (!allCookies["storeVersion"] ||
        allCookies["storeVersion"] != 
        predefinedCookies["storeVersion"].defaultValue) {
        // Reset cookies due to different version
        mcc.util.dbg("Incompatible store version: " + 
                    allCookies["storeVersion"] + " vs. " + 
                    predefinedCookies["storeVersion"].defaultValue);
        resetCookies();
    }
}

// Get cookie value
function getCookie(key) {
    mcc.util.assert(allCookies, "No cookies present");
    return allCookies[key];
}

// Store a cookie with the given name, value and default expiration
function setCookie(name, value) {
    //Stores that keep sensitive information should NOT be kept in cookies!
    if (name == "clusterStore") {
        return; //Do NOT set cookies for cluster!
    }
    if (name == "hostStore") {
        return; //Do NOT set cookies for hosts!
        // Skip over Empty/AnyHost
        if (value.length > 200) {
            mcc.util.dbg("Removing passwords from Host cookie store.");
            // Remove password.
            var startIndex = value.indexOf("\"usrpwd\": \"");
            var endIndex = 0;
            while (startIndex > -1 && startIndex < value.length) {
                for (i = startIndex;; i++) {
                    if (value.charAt(i) == ",") {
                        endIndex = i;
                        break;
                    }
                    if (i == value.length - 1) {
                        break;
                    }
                }
                if (endIndex > startIndex) {
                    value = value.replace(value.slice(startIndex, endIndex), "\"usrpwd\": \"\"");
                }
                startIndex = value.indexOf("\"usrpwd\": \"", endIndex);
                endIndex = 0;
            }
            // Remove passphrase.
            var startIndex = value.indexOf("\"key_passp\": \"");
            var endIndex = 0;
            while (startIndex > -1 && startIndex < value.length) {
                for (i = startIndex;; i++) {
                    if (value.charAt(i) == ",") {
                        endIndex = i;
                        break;
                    }
                    if (i == value.length - 1) {
                        break;
                    }
                }
                if (endIndex > startIndex) {
                    value = value.replace(value.slice(startIndex, endIndex), "\"key_passp\": \"\"");
                }
                startIndex = value.indexOf("\"key_passp\": \"", endIndex);
                endIndex = 0;
            }        
            // Remove private key.
            var startIndex = value.indexOf("\"key\": \"");
            var endIndex = 0;
            while (startIndex > -1 && startIndex < value.length) {
                for (i = startIndex;; i++) {
                    if (value.charAt(i) == ",") {
                        endIndex = i;
                        break;
                    }
                    if (i == value.length - 1) {
                        break;
                    }
                }
                if (endIndex > startIndex) {
                    value = value.replace(value.slice(startIndex, endIndex), "\"key\": \"\"");
                }
                startIndex = value.indexOf("\"key\": \"", endIndex);
                endIndex = 0;
            }        
        }
    }
    var expiration = +predefinedCookies["expiration"].defaultValue;
    // If there isn't a default, use 5 days
    if (!expiration) {
        expiration = 5;
    }
    setCookieExpiration(name, value, expiration);
}

// Store a cookie with the given name (prefixed), value and expiration date
function setCookieExpiration(name, value, expiration) {
    var cookieExpiration = new Date();
    var cookieName = null; 
    var cookieValue = null;
    var key = null;  

    mcc.util.assert(allCookies, "No cookies present");

    // Create date for expiration
    cookieExpiration.setDate(cookieExpiration.getDate() + expiration);
    cookieValue = escape(value) + "; expires=" + 
            cookieExpiration.toUTCString() + "; path=" + escape("/");

    // Prefix if necessary
    if (name.substr(0, 4) == "mcc_") {
        key = name.substr(4);
        cookieName = name;
    } else {
        key = name;
        cookieName = "mcc_" + name;
    }

    // Store the cookie in document and object
    document.cookie = cookieName + " = " + cookieValue;
    allCookies[key] = value;
}

/**************************** Unit test interface  ****************************/

if (mcc.util.tests) {
    mcc.util.cookies.predefinedCookies = function (key) {
        if (key) {
            return predefinedCookies[key];
        } else {
            return predefinedCookies;
        }
    };
    mcc.util.cookies.setupAllCookies = setupAllCookies;
    mcc.util.cookies.deleteCookies = deleteCookies;
    mcc.util.cookies.retrieveCookies = retrieveCookies;
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    setupPredefinedCookies();
    retrieveCookies();
    mcc.util.dbg("Cookies module initialized");
});



