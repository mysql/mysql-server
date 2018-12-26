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
 ***        External interface wrapper for configuration utilities          ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide("mcc.configuration");

/**************************** Export utilities ********************************/

dojo.require("mcc.configuration.deploy");

mcc.configuration.setupContext = mcc.configuration.deploy.setupContext;
mcc.configuration.getStartProcessCommands = 
            mcc.configuration.deploy.getStartProcessCommands;
mcc.configuration.getConfigurationFile = 
            mcc.configuration.deploy.getConfigurationFile;

mcc.configuration.deployCluster = mcc.configuration.deploy.deployCluster;
mcc.configuration.startCluster = mcc.configuration.deploy.startCluster;
mcc.configuration.stopCluster = mcc.configuration.deploy.stopCluster;
mcc.configuration.installCluster = mcc.configuration.deploy.installCluster;
mcc.configuration.clServStatus = mcc.configuration.deploy.clServStatus;
mcc.configuration.determineClusterRunning = mcc.configuration.deploy.determineClusterRunning;
/************************* Parameter definitions ******************************/

dojo.require("mcc.configuration.parameters");

mcc.configuration.getPara = mcc.configuration.parameters.getPara;
mcc.configuration.setPara = mcc.configuration.parameters.setPara;
mcc.configuration.visiblePara = mcc.configuration.parameters.visiblePara;
mcc.configuration.isHeading = mcc.configuration.parameters.isHeading;
mcc.configuration.getAllPara = mcc.configuration.parameters.getAllPara;
mcc.configuration.resetDefaultValueInstance = 
        mcc.configuration.parameters.resetDefaultValueInstance;

/************************* Parameter calculations *****************************/

dojo.require("mcc.configuration.calculations");

mcc.configuration.autoConfigure = mcc.configuration.calculations.autoConfigure;
mcc.configuration.instanceSetup = mcc.configuration.calculations.instanceSetup;
mcc.configuration.typeSetup = mcc.configuration.calculations.typeSetup;

/******************************** Initialize  *********************************/

dojo.ready(function() {
    mcc.util.dbg("Configuration utilities module initialized");
});

