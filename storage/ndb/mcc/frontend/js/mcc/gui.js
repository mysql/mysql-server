/*
Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
 ***              External interface wrapper for gui elements               ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide("mcc.gui");

/**************************** Wizard definition *******************************/

dojo.require("mcc.gui.wizard");

mcc.gui.enterFirst = mcc.gui.wizard.enterFirst;
mcc.gui.nextPage = mcc.gui.wizard.nextPage;
mcc.gui.prevPage = mcc.gui.wizard.prevPage;
mcc.gui.lastPage = mcc.gui.wizard.lastPage;
mcc.gui.reloadPage = mcc.gui.wizard.reloadPage;

/**************************** Cluster definition ******************************/

dojo.require("mcc.gui.clusterdef");

mcc.gui.showClusterDefinition = mcc.gui.clusterdef.showClusterDefinition;
mcc.gui.saveClusterDefinition = mcc.gui.clusterdef.saveClusterDefinition;
mcc.gui.getSSHPwd = mcc.gui.clusterdef.getSSHPwd;
mcc.gui.getSSHUser = mcc.gui.clusterdef.getSSHUser;
mcc.gui.getSSHkeybased = mcc.gui.clusterdef.getSSHkeybased;
mcc.gui.getOpenFW = mcc.gui.clusterdef.getOpenFW;
mcc.gui.getInstallCl = mcc.gui.clusterdef.getInstallCl;
//New Cluster-level SSH stuff
mcc.gui.getClSSHPwd = mcc.gui.clusterdef.getClSSHPwd;
mcc.gui.getClSSHUser = mcc.gui.clusterdef.getClSSHUser;
mcc.gui.getClSSHKeyFile = mcc.gui.clusterdef.getClSSHKeyFile;


/***************************** Host definition ********************************/

dojo.require("mcc.gui.hostdef");

mcc.gui.hostGridSetup = mcc.gui.hostdef.hostGridSetup;

/******************************** Host tree ***********************************/

dojo.require("mcc.gui.hosttree");

mcc.gui.hostTreeSetPath = mcc.gui.hosttree.hostTreeSetPath;
mcc.gui.getCurrentHostTreeItem = mcc.gui.hosttree.getCurrentHostTreeItem;
mcc.gui.resetHostTreeItem = mcc.gui.hosttree.resetHostTreeItem;
mcc.gui.hostTreeSetup = mcc.gui.hosttree.hostTreeSetup;

/************************ Host tree selection details *************************/

dojo.require("mcc.gui.hosttreedetails");

mcc.gui.hostTreeSelectionDetailsSetup =
        mcc.gui.hosttreedetails.hostTreeSelectionDetailsSetup;
mcc.gui.updateHostTreeSelectionDetails =
        mcc.gui.hosttreedetails.updateHostTreeSelectionDetails

/****************************** Process tree **********************************/

dojo.require("mcc.gui.processtree");

mcc.gui.processTreeSetPath = mcc.gui.processtree.processTreeSetPath;
mcc.gui.getCurrentProcessTreeItem = 
        mcc.gui.processtree.getCurrentProcessTreeItem;
mcc.gui.resetProcessTreeItem = mcc.gui.processtree.resetProcessTreeItem;
mcc.gui.processTreeSetup = mcc.gui.processtree.processTreeSetup;

/********************* Process tree selection details *************************/

dojo.require("mcc.gui.processtreedetails");

mcc.gui.updateProcessTreeSelectionDetails =
        mcc.gui.processtreedetails.updateProcessTreeSelectionDetails

/****************************** Process tree **********************************/

dojo.require("mcc.gui.deploymenttree");

mcc.gui.deploymentTreeSetup = mcc.gui.deploymenttree.deploymentTreeSetup;
mcc.gui.startStatusPoll = mcc.gui.deploymenttree.startStatusPoll;
mcc.gui.stopStatusPoll = mcc.gui.deploymenttree.stopStatusPoll;
mcc.gui.getCurrentDeploymentTreeItem = 
        mcc.gui.deploymenttree.getCurrentDeploymentTreeItem;
mcc.gui.resetDeploymentTreeItem = mcc.gui.deploymenttree.resetDeploymentTreeItem;
mcc.gui.getStatii = mcc.gui.deploymenttree.getStatii;

/******************************** Initialize  *********************************/

dojo.ready(function() {
    mcc.util.dbg("GUI module initialized");
});

