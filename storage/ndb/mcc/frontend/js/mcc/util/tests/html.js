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
 ***                        Unit tests: HTML utilities                      ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.tests.html
 *
 *  Test cases: 
 *      Positive: 
 *          posStartTable: Generate and check a start table string
 *          posTableRow: Generate and check a table row string
 *          posEndTable: Generate and check an end table string
 *
 *      Negative: 
 *
 *  Todo: 
 *      Implement negative test cases
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.tests.html");

dojo.require("dojo.DeferredList");

dojo.require("mcc.util");

/******************************* Test cases  **********************************/

// Generate a start table string, verify contents
function posStartTable() {
    var startString = mcc.util.startTable();
    mcc.util.tst("Start table: " + startString);
    doh.t(startString == "<table width=\"100%\" cellspacing=\"0\">");
}

// Generate a table row string, verify contents
function posTableRow() {
    var rowString1 = mcc.util.tableRow("prefix", "label", "url", 
            "attr", "tooltip");
    var background1 = rowString1.split(":")[1].split(";")[0];

    // Should have one "url"
    doh.t(rowString1.split("<a href=\"url\"").length == 2);
    // Should have one "label"
    doh.t(rowString1.split("label").length == 2);
    // Should have one "prefix_attr"
    doh.t(rowString1.split("id=\"prefixattr\"").length == 2);
    // Should have one "prefix_attr_ctrl"
    doh.t(rowString1.split("id=\"prefixattr_ctrl\"").length == 2);

    var rowString2 = mcc.util.tableRow("prefix", "label", null, 
            "attr", "tooltip");

    // Verify that url == null is left out
    doh.t(rowString2.indexOf("<a href=\"url\"") == -1);
    // Verify that background color isn't present
    doh.t(rowString2.indexOf("background-color") == -1);

    var rowString3 = mcc.util.tableRow("prefix", "label", null, 
            "attr", "tooltip");
    var background3 = rowString3.split(":")[1].split(";")[0];

    // Verify that background alternates back to first value
    doh.t(background1 == background3);
}

// Generate an end table string, verify contents
function posEndTable() {
    var endString = mcc.util.endTable();
    mcc.util.tst("End table: " + endString);
    doh.t(endString == "</table>");
}

/*************************** Register test cases  *****************************/

doh.register("mcc.util.tests.html", [
    posStartTable,
    posTableRow,
    posEndTable
]);
