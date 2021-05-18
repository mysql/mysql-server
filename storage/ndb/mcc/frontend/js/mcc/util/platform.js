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
 *      mcc.util.platform.validateIPConditionally(ip, ExclPub, ExclPriv, ExclSpec)
 *          No RegEx used. Flags: Exclude public, private or special (0.0.0.0, 127.0.0.1) addresses.
 *          validateIPConditionally(ip, false, false, false) is the same as ValidateIPAddress.
 *      mcc.util.platform.isSafari: tells us if we're running inside Safari browser
 *      mcc.util.platform.isIE: tells us if we're running inside IE browser
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

/****************************** Import/export *********************************/
dojo.provide('mcc.util.platform');

/**************************** External interface ******************************/
mcc.util.platform.isWin = isWin;
mcc.util.platform.dirSep = dirSep;
mcc.util.platform.terminatePath = terminatePath;
mcc.util.platform.quotePath = quotePath;
mcc.util.platform.unixPath = unixPath;
mcc.util.platform.winPath = winPath;
mcc.util.platform.countOccurrences = countOccurrences;
mcc.util.platform.ValidateIPAddress = ValidateIPAddress;
mcc.util.platform.validateIPCondit = validateIPCondit;
mcc.util.platform.isSafari = isSafari;
mcc.util.platform.isIE = isIE;

/****************************** Implementation ********************************/

/**
 *Count # of occ. of substring inside string. W/O overlap:
 *countOccurrences("mccmccmcc", "mccmcc", false) = 1
 *countOccurrences("mccmccmcc", "mccmcc", true) = 2
 * @param {*} string    String, base string
 * @param {*} subString String, pattern
 * @param {*} overlap   Boolean, account for overlap
 * @returns             Number, # of occurrences
 */
function countOccurrences (string, subString, overlap) {
    string += '';
    subString += '';
    if (subString.length <= 0) return (string.length + 1);
    var n = 0;
    var pos = 0;
    var step = overlap ? 1 : subString.length;
    while (true) {
        pos = string.indexOf(subString, pos);
        if (pos >= 0) {
            ++n;
            pos += step;
        } else break;
    }
    return n;
}

/**
 *Checks validity of IPv4 address provided.
 *
 * @param {*} ipAddress String
 * @returns             Boolean
 */
function ValidateIPAddress (ipAddress) {
    ipAddress += '';
    if (ipAddress === '127.0.0.1' || ipAddress.toUpperCase() === 'LOCALHOST') {
        return false;
    } else {
        if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ipAddress)) {
            return true;
        }
    }
    return false;
}
/**
 *Checks validity of IPv4 address provided, with conditions.
 *
 * @param {String} ip IP address to check
 * @param {Boolean} ExclPub in/exclude public addresses
 * @param {Boolean} ExclPriv in/exclude private addresses
 * @param {Boolean} ExclSpec in/exclude special addresses
 * @returns {Boolean}
 */
function validateIPCondit (ip, ExclPub, ExclPriv, ExclSpec) {
    ip += '';
    if (!ip || ip.length < 7) { return false; }
    if (!ip || ip === '127.0.0.1' || ip.toUpperCase() === 'LOCALHOST') {
        return false;
    }
    var ipa = ip.split('.');
    // General format of IPv4 address.
    if (ipa.length !== 4) {
        return false;
    }

    // Check numbers are in range.
    for (var c = 0; c < 4; c++) {
        if (1 / Number(ipa[c]) < 0 || Number(ipa[c]) > 255 || isNaN(parseFloat(ipa[c])) || !isFinite(ipa[c]) ||
            ipa[c].indexOf(' ') !== -1) {
            return false;
        }
    }
    /*
    Exclusion code depending on flags.
    rfc1918
    Private IP addresses: 10.0.0.0 to 10.255.255.255. 172.16.0.0 to 172.31.255.255. 192.168.0.0 to 192.168.255.255.
    or
    10.0.0.0        -   10.255.255.255  (10/8 prefix)
    172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
    192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
    */
    var isSpec = (ipa[0] === '0' || (ipa[0] === '127' && ipa[1] === '0')); // [0]));
    var isPriv = ((ipa[0] === '192' && ipa[1] === '168') || (ipa[0] === '172' && Number(ipa[1]) >= 16 &&
        Number(ipa[1]) <= 31) || (ipa[0] === '10'));
    var isPub = !isPriv;
    if (ExclSpec && isSpec) {
        return false;
    }
    if (ExclPriv && isPriv) {
        return false;
    }
    if (ExclPub && isPub) {
        return false;
    }
    return true;
}

/**
 *Check if current platform is to be considered as Windows or not
 *
 * @param {*} platform  String, platform name
 * @returns             Boolean
 */
function isWin (platform) {
    if (platform) {
        return platform.toUpperCase() === 'CYGWIN' || platform.toUpperCase() === 'WINDOWS';
    } else {
        return false;
    }
}

/**
 *Get the predominant dir separator.
 *
 * @param {*} path  String, path to check dirSep for
 * @returns         String, / or \\
 */
function dirSep (path) {
    // If the path contains at least one /, or no \, use /
    if (path) {
        if (path.indexOf('/') !== -1 || path.indexOf('\\') === -1) {
            return '/';
        } else {
            return '\\';
        }
    } else {
        return '';
    }
}

/**
 *Make sure directory path is properly terminated.
 *
 * @param {*} path  String, path to terminate
 * @returns         String, properly terminated path or ''
 */
function terminatePath (path) {
    if (path) {
        var sep = dirSep(path);
        if (path.lastIndexOf(sep) !== path.length - 1) {
            path += sep;
        }
        return path;
    } else {
        return '';
    }
}

/**
 *Quote backslashes.
 *
 * @param {*} path  String
 * @returns         String, input with quoted backslashes or ''
 */
function quotePath (path) {
    if (path) {
        return path.replace(/\\/g, '\\\\');
    } else {
        return '';
    }
}

/**
 *Make path with unix style separators.
 *
 * @param {*} path  String, path to convert
 * @returns         String, path with unix style separators
 */
function unixPath (path) {
    if (path) {
        return path.replace(/\\/g, '/');
    } else {
        return '';
    }
}

/**
 *Make path with Windows style separators.
 *
 * @param {*} path  String, path to convert
 * @returns         String, path with Windows style separators
 */
function winPath (path) {
    if (path) {
        return path.replace(/\//g, '\\');
    } else {
        return '';
    }
}
/**
 *Tells us if we're running inside Safari browser.
 *
 * @returns {boolean}
 */
function isSafari () {
    return navigator.vendor && navigator.vendor.indexOf('Apple') > -1 &&
        navigator.userAgent &&
        navigator.userAgent.indexOf('CriOS') === -1 &&
        navigator.userAgent.indexOf('FxiOS') === -1;
    /*
    For desktop only:
    return window.safari !== undefined;
    */
}
/**
 *Tells us if we're running inside IE browser.
 *
 * @returns {boolean}
 */
function isIE () {
    return !!window.MSInputMethodContext && !!document.documentMode;
}
/******************************** Initialize **********************************/

dojo.ready(function initialize () {
    console.info('[INF]Platform utilities module initialized');
});
