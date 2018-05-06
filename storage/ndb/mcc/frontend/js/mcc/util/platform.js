/*
Copyright (c) 2012, 2017 Oracle and/or its affiliates. All rights reserved.

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
 ***                            Platform utilities                          ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.platform
 *
 *  Description:
 *      Various platform related utilities, in particular path management
 *
 *  External interface: 
 *      mcc.util.platform.isWin: Check if platform is to be considered win
 *      mcc.util.platform.dirSep: Return predominant directory separator in path
 *      mcc.util.platform.terminatePath: Terminate path with predominant dir sep
 *      mcc.util.platform.quotePath: Replace \ by \\
 *      mcc.util.platform.unixPath: Replace \ by / 
 *      mcc.util.platform.winPath: Replace / by \
 *      mcc.util.platform.countOccurrences: Count occurrences of sub-string inside string w/wo overlapping.
 *      mcc.util.platform.ValidateIPAddress: Validate IP address provided.
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
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

dojo.provide("mcc.util.platform");

/**************************** External interface  *****************************/

mcc.util.platform.isWin = isWin;
mcc.util.platform.dirSep = dirSep;
mcc.util.platform.terminatePath = terminatePath;
mcc.util.platform.quotePath = quotePath;
mcc.util.platform.unixPath = unixPath;
mcc.util.platform.winPath = winPath;
mcc.util.platform.countOccurrences = countOccurrences;
mcc.util.platform.ValidateIPAddress = ValidateIPAddress;

/****************************** Implementation  *******************************/
// Count # of occ. of substring inside string.
// countOccurrences("mccmccmcc", "mccmcc", false) = 1
// countOccurrences("mccmccmcc", "mccmcc", true) = 2
function countOccurrences(string, subString, overlap) {
    string += "";
    subString += "";
    if (subString.length <= 0) return (string.length + 1);

    var n = 0,
        pos = 0,
        step = overlap ? 1 : subString.length;

    while (true) {
        pos = string.indexOf(subString, pos);
        if (pos >= 0) {
            ++n;
            pos += step;
        } else break;
    }
    return n;
}

// Check validity of IP address provided.
function ValidateIPAddress(ipAddress) {
    if (ipAddress == "127.0.0.1") {
        return false;
    } else {
        if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ipAddress))  
        {  
            return true;
        }
    }
    return false;
}

// Check if current platform is to be considered windows or not
function isWin(platform) {
    return platform == "CYGWIN" || platform == "Windows";
}

// Get the predominant dir separator
function dirSep(path) {
    // If the path contains at least one /, or no \, use /
    if (path.indexOf("/") != -1 || path.indexOf("\\") == -1) {
        return "/";
    } else {
        return "\\";
    }
}

// Make sure directory path is properly terminated
function terminatePath(path) {
    var sep = dirSep(path);
    if (path.lastIndexOf(sep) != path.length-1) {
        path += sep;
    }
    return path;
}

// Quote backslashes
function quotePath(path) {
    return path.replace(/\\/g, "\\\\");
}

// Return path with unix style separators
function unixPath(path) {
    return path.replace(/\\/g, "/");
}

// Return path with win style separators
function winPath(path) {
    return path.replace(/\//g, "\\");
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize() {
    mcc.util.dbg("Platform utilities module initialized");
});


