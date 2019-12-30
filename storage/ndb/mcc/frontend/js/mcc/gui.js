/*
Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

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
 ***              External interface wrapper for gui elements               ***
 ***                                                                        ***
 ******************************************************************************/
dojo.provide('mcc.gui');

/**************************** Wizard definition *******************************/
dojo.require('mcc.gui.wizard');

mcc.gui.enterFirst = mcc.gui.wizard.enterFirst;
mcc.gui.nextPage = mcc.gui.wizard.nextPage;
mcc.gui.prevPage = mcc.gui.wizard.prevPage;
mcc.gui.lastPage = mcc.gui.wizard.lastPage;
mcc.gui.reloadPage = mcc.gui.wizard.reloadPage;
mcc.gui.getClCfgProblems = mcc.gui.wizard.getClCfgProblems;
mcc.gui.getHoCfgProblems = mcc.gui.wizard.getHoCfgProblems;
mcc.gui.getPrCfgProblems = mcc.gui.wizard.getPrCfgProblems;
mcc.gui.getGeCfgProblems = mcc.gui.wizard.getGeCfgProblems;

/**************************** Cluster definition ******************************/
dojo.require('mcc.gui.clusterdef');

mcc.gui.showClusterDefinition = mcc.gui.clusterdef.showClusterDefinition;
mcc.gui.saveClusterDefinition = mcc.gui.clusterdef.saveClusterDefinition;
mcc.gui.getSSHPwd = mcc.gui.clusterdef.getSSHPwd;
mcc.gui.getSSHUser = mcc.gui.clusterdef.getSSHUser;
mcc.gui.getSSHkeybased = mcc.gui.clusterdef.getSSHkeybased;
mcc.gui.getOpenFW = mcc.gui.clusterdef.getOpenFW;
mcc.gui.getInstallCl = mcc.gui.clusterdef.getInstallCl;
mcc.gui.getUseVPN = mcc.gui.clusterdef.getUseVPN;
mcc.gui.setLocalHome = mcc.gui.clusterdef.setLocalHome;
// New Cluster-level SSH stuff
mcc.gui.getClSSHPwd = mcc.gui.clusterdef.getClSSHPwd;
mcc.gui.getClSSHUser = mcc.gui.clusterdef.getClSSHUser;
mcc.gui.getClSSHKeyFile = mcc.gui.clusterdef.getClSSHKeyFile;

/***************************** Host definition ********************************/
dojo.require('mcc.gui.hostdef');

mcc.gui.hostGridSetup = mcc.gui.hostdef.hostGridSetup;

/******************************** Host tree ***********************************/
dojo.require('mcc.gui.hosttree');

mcc.gui.hostTreeSetPath = mcc.gui.hosttree.hostTreeSetPath;
mcc.gui.getCurrentHostTreeItem = mcc.gui.hosttree.getCurrentHostTreeItem;
mcc.gui.resetHostTreeItem = mcc.gui.hosttree.resetHostTreeItem;
mcc.gui.hostTreeSetup = mcc.gui.hosttree.hostTreeSetup;

/************************ Host tree selection details *************************/
dojo.require('mcc.gui.hosttreedetails');

mcc.gui.hostTreeSelectionDetailsSetup = mcc.gui.hosttreedetails.hostTreeSelectionDetailsSetup;
mcc.gui.updateHostTreeSelectionDetails = mcc.gui.hosttreedetails.updateHostTreeSelectionDetails

/****************************** Process tree **********************************/
dojo.require('mcc.gui.processtree');

mcc.gui.processTreeSetPath = mcc.gui.processtree.processTreeSetPath;
mcc.gui.getCurrentProcessTreeItem = mcc.gui.processtree.getCurrentProcessTreeItem;
mcc.gui.resetProcessTreeItem = mcc.gui.processtree.resetProcessTreeItem;
mcc.gui.processTreeSetup = mcc.gui.processtree.processTreeSetup;

/********************** Process tree selection details *************************/
dojo.require('mcc.gui.processtreedetails');

mcc.gui.updateProcessTreeSelectionDetails = mcc.gui.processtreedetails.updateProcessTreeSelectionDetails;

/****************************** Process tree **********************************/
dojo.require('mcc.gui.deploymenttree');

mcc.gui.deploymentTreeSetup = mcc.gui.deploymenttree.deploymentTreeSetup;
mcc.gui.startStatusPoll = mcc.gui.deploymenttree.startStatusPoll;
mcc.gui.stopStatusPoll = mcc.gui.deploymenttree.stopStatusPoll;
mcc.gui.getCurrentDeploymentTreeItem = mcc.gui.deploymenttree.getCurrentDeploymentTreeItem;
mcc.gui.resetDeploymentTreeItem = mcc.gui.deploymenttree.resetDeploymentTreeItem;
mcc.gui.getStatii = mcc.gui.deploymenttree.getStatii;
mcc.gui.setPollTimeOut = mcc.gui.deploymenttree.setPollTimeOut;
mcc.gui.getMgmtArraySize = mcc.gui.deploymenttree.getMgmtArraySize;
mcc.gui.getMgmtArrayInfo = mcc.gui.deploymenttree.getMgmtArrayInfo;

/******************************** Initialize **********************************/
dojo.ready(function () {
    console.info('[INF]GUI module initialized');
});
