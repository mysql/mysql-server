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

