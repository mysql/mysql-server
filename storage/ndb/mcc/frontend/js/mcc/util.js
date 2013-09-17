/*
Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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
 ***              External interface wrapper for MCC utilities              ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide("mcc.util");

/**************************** Logging utilities  ******************************/

dojo.require("mcc.util.log");

mcc.util.inf = mcc.util.log.inf;
mcc.util.dbg = mcc.util.log.dbg;
mcc.util.tst = mcc.util.log.tst;
mcc.util.wrn = mcc.util.log.wrn;
mcc.util.err = mcc.util.log.err;

/***************************** Cookie utilities  ******************************/

dojo.require("mcc.util.cookies");

mcc.util.getCookie = mcc.util.cookies.getCookie;
mcc.util.setCookie = mcc.util.cookies.setCookie;
mcc.util.resetCookies = mcc.util.cookies.resetCookies;

/****************************** HTML utilities  *******************************/

dojo.require("mcc.util.html");

mcc.util.startTable = mcc.util.html.startTable;
mcc.util.tableRow = mcc.util.html.tableRow;
mcc.util.endTable = mcc.util.html.endTable;
mcc.util.setupWidgets = mcc.util.html.setupWidgets;
mcc.util.updateWidgets = mcc.util.html.updateWidgets;
mcc.util.getDocUrlRoot = mcc.util.html.getDocUrlRoot;

/***************************** Cluster utilities  *****************************/

dojo.require("mcc.util.cluster");

mcc.util.getColleagueNodes = mcc.util.cluster.getColleagueNodes;
mcc.util.getNodeDistribution = mcc.util.cluster.getNodeDistribution;
mcc.util.checkValidNodeId = mcc.util.cluster.checkValidNodeId;
mcc.util.getNextNodeId = mcc.util.cluster.getNextNodeId; 

/****************************** Assert utilities  *****************************/

dojo.require("mcc.util.assert");

mcc.util.assert = mcc.util.assert.assert;

/***************************** Platform utilities  ****************************/

dojo.require("mcc.util.platform");

mcc.util.isWin = mcc.util.platform.isWin;
mcc.util.dirSep = mcc.util.platform.dirSep;
mcc.util.terminatePath = mcc.util.platform.terminatePath;
mcc.util.quotePath = mcc.util.platform.quotePath;
mcc.util.unixPath = mcc.util.platform.unixPath;
mcc.util.winPath = mcc.util.platform.winPath;

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Utilities module initialized");
});

