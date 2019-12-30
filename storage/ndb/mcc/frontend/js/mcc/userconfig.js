/*
Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

/******************************************************************************
 ***                                                                        ***
 ***           External interface wrapper for user configurations           ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide('mcc.userconfig');

/************************** User config utilities *****************************/
dojo.require('mcc.userconfig.userconfigjs');
mcc.userconfig.setConfigFile = mcc.userconfig.userconfigjs.setConfigFile;
mcc.userconfig.getConfigFile = mcc.userconfig.userconfigjs.getConfigFile;
mcc.userconfig.setConfigFileContents = mcc.userconfig.userconfigjs.setConfigFileContents;
mcc.userconfig.getConfigFileContents = mcc.userconfig.userconfigjs.getConfigFileContents;
mcc.userconfig.getDefaultCfg = mcc.userconfig.userconfigjs.getDefaultCfg;
mcc.userconfig.writeConfigFile = mcc.userconfig.userconfigjs.writeConfigFile;
mcc.userconfig.setOriginalStore = mcc.userconfig.userconfigjs.setOriginalStore;
mcc.userconfig.isShadowEmpty = mcc.userconfig.userconfigjs.isShadowEmpty;
mcc.userconfig.setIsNewConfig = mcc.userconfig.userconfigjs.setIsNewConfig;
mcc.userconfig.getIsNewConfig = mcc.userconfig.userconfigjs.getIsNewConfig;
mcc.userconfig.compareStores = mcc.userconfig.userconfigjs.compareStores;
mcc.userconfig.getConfigProblems = mcc.userconfig.userconfigjs.getConfigProblems;
//- only list of problems not coming from shadow comparison needs setter too
mcc.userconfig.setCcfgPrGen = mcc.userconfig.userconfigjs.setCcfgPrGen;
mcc.userconfig.setMsgForGenPr = mcc.userconfig.userconfigjs.setMsgForGenPr;
mcc.userconfig.wasCfgStarted = mcc.userconfig.userconfigjs.wasCfgStarted;
mcc.userconfig.setCfgStarted = mcc.userconfig.userconfigjs.setCfgStarted;
/******************************** Initialize **********************************/
dojo.ready(function () {
    console.info('[INF]Userconfig module initialized');
});
