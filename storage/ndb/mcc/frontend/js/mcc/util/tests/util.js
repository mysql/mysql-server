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
 ***                Unit test wrapper for MCC utilities                     ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide("mcc.util.tests.util");

dojo.require("mcc.util.tests.assert");
dojo.require("mcc.util.tests.log");
dojo.require("mcc.util.tests.cluster");
dojo.require("mcc.util.tests.cookies");
dojo.require("mcc.util.tests.html");

// Run gui tests if in a browser environment
if (dojo.isBrowser) {
    doh.registerUrl("mcc.util.tests.html_gui", 
                    "../../../js/mcc/util/tests/html_gui.html");
}


