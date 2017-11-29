/*
Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
 ***           External interface wrapper for user configurations           ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide("mcc.userconfig");

/************************** User config utilities  ****************************/

dojo.require("mcc.userconfig.userconfigjs");
mcc.userconfig.setConfigFile = mcc.userconfig.userconfigjs.setConfigFile;
mcc.userconfig.getConfigFile = mcc.userconfig.userconfigjs.getConfigFile;
mcc.userconfig.setConfigFileContents = mcc.userconfig.userconfigjs.setConfigFileContents;
mcc.userconfig.getConfigFileContents = mcc.userconfig.userconfigjs.getConfigFileContents;
mcc.userconfig.getDefaultCfg = mcc.userconfig.userconfigjs.getDefaultCfg;
mcc.userconfig.getConfKey = mcc.userconfig.userconfigjs.getConfKey;
mcc.userconfig.setConfKey = mcc.userconfig.userconfigjs.setConfKey;
mcc.userconfig.resetConfKey = mcc.userconfig.userconfigjs.resetConfKey;
mcc.userconfig.writeConfigFile = mcc.userconfig.userconfigjs.writeConfigFile;

/******************************** Initialize  *********************************/

dojo.ready(function () {
    console.log("Userconfig module initialized");
});
