/*
Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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
 ***       External interface wrapper for user configurations IE11          ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide('mcc.userconfigIE');

/************************** User config utilities *****************************/
dojo.require('mcc.userconfig.userconfigjsIE');
mcc.userconfig.setConfigFile = mcc.userconfig.userconfigjsIE.setConfigFile;
mcc.userconfig.getConfigFile = mcc.userconfig.userconfigjsIE.getConfigFile;
mcc.userconfig.setConfigFileContents = mcc.userconfig.userconfigjsIE.setConfigFileContents;
mcc.userconfig.getConfigFileContents = mcc.userconfig.userconfigjsIE.getConfigFileContents;
mcc.userconfig.getDefaultCfg = mcc.userconfig.userconfigjsIE.getDefaultCfg;
mcc.userconfig.writeConfigFile = mcc.userconfig.userconfigjsIE.writeConfigFile;
mcc.userconfig.setOriginalStore = mcc.userconfig.userconfigjsIE.setOriginalStore;
mcc.userconfig.setIsNewConfig = mcc.userconfig.userconfigjsIE.setIsNewConfig;
mcc.userconfig.getIsNewConfig = mcc.userconfig.userconfigjsIE.getIsNewConfig;
mcc.userconfig.compareStores = mcc.userconfig.userconfigjsIE.compareStores;
mcc.userconfig.isShadowEmpty = mcc.userconfig.userconfigjsIE.isShadowEmpty;
//-
mcc.userconfig.getConfigProblems = mcc.userconfig.userconfigjsIE.getConfigProblems;
mcc.userconfig.setCcfgPrGen = mcc.userconfig.userconfigjsIE.setCcfgPrGen;
mcc.userconfig.setMsgForGenPr = mcc.userconfig.userconfigjsIE.setMsgForGenPr;
mcc.userconfig.wasCfgStarted = mcc.userconfig.userconfigjsIE.wasCfgStarted;
mcc.userconfig.setCfgStarted = mcc.userconfig.userconfigjsIE.setCfgStarted;
/******************************** Initialize **********************************/
dojo.ready(function () {
    console.info('[INF]UserconfigIE module initialized');
});
