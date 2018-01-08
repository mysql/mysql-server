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
 ***                     Configuration wizard defintition                   ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.gui.wizard
 *
 *  Description:
 *      Pages and navigation functions for the wizard
 *
 *  External interface: 
 *      mcc.gui.wizard.enterFirst: Select and enter first page
 *      mcc.gui.wizard.nextPage: Show next page
 *      mcc.gui.wizard.prevPage: Show previous page
 *      mcc.gui.wizard.lastPage: Show last page
 *      mcc.gui.wizard.reloadPage: Reload current page
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      initialize: Initialize the config wizard
 *      cancelTabChange: Setup a timer to stay at old child
 *      configWizardMenuSetup: Setup settings menu for the configuration wizard
 *      configWizardEnableButtons: Setup buttons for previous/next page
 *
 *  Internal data: 
 *      initializeClusterDef: True if initial start or reset
 *      configWizardPageIds: Array of page ids
 *      configWizardPages: Array of pages with content and enter/exit functions
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
        Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.gui.wizard");

dojo.require("dijit.form.Button");
dojo.require("dijit.DropDownMenu");
dojo.require("dijit.MenuItem");
dojo.require("dijit.Dialog");

dojo.require("mcc.util");
dojo.require("mcc.storage");
dojo.require("mcc.configuration");
dojo.require("mcc.userconfig");
dojo.require("mcc.gui");

/**************************** External interface  *****************************/

mcc.gui.wizard.enterFirst = enterFirst;
mcc.gui.wizard.nextPage = nextPage;
mcc.gui.wizard.prevPage = prevPage;
mcc.gui.wizard.lastPage = lastPage;
mcc.gui.wizard.reloadPage = reloadPage;

/******************************* Internal data ********************************/

var initializeClusterDef = true;

var configWizardPageIds = [
    "configWizardDefineCluster",
    "configWizardDefineHosts",
    "configWizardDefineProcesses",
    "configWizardDefineParameters",
    "configWizardDeployConfig"
];

var configWizardPages = {
    configWizardDefineCluster: {
        content: "\
            <div dojoType='dijit.layout.BorderContainer'\
                    gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='top'\
                        class='content-tab-top-panel'>\
                    <h2>Cluster Type and SSH Credentials</h2>\
                    <span class='content-tab-sub-title'>MySQL Cluster is able \
                            to operate in various configurations. Please \
                            specify the settings below to define the right \
                            cluster type that fits your use case. If you intend\
                            to use remote hosts for deploying MySQL Cluster, \
                            SSH must be enabled. Unless key based SSH is \
                            possible, you must submit your user name and \
                            password below.\
                    </span>\
                </div>\
                <div dojoType='dijit.layout.ContentPane' region='center' \
                        class='content-tab-center-panel'>\
                    <div id='clusterDetailsHeader'>\
                    </div>\
                    <div id='clusterDetails'>\
                        <table border='0' width='100%'>\
                        <tr><td style='width:30%;'><label for='cd_name'>Cluster name</label> \
                            <span class='helpIcon' id='cd_name_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='cd_name'></div>\
                            </td></tr>\
                        <tr><td style='width:30%;'><label for='cd_hosts'>Host list</label> \
                            <span class='helpIcon' id='cd_hosts_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='cd_hosts'></div>\
                            </td></tr>\
                        <tr><td style='width:30%;'><label for='cd_apparea'>Application area</label> \
                            <span class='helpIcon' id='cd_apparea_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='cd_apparea'></div>\
                            </td></tr>\
                        <tr><td style='width:30%;'><label for='cd_writeload'>Write load</label> \
                            <span class='helpIcon' id='cd_writeload_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='cd_writeload'></div>\
                            </td></tr>\
                        </table>\
                    </div>\
                    <div id='sshDetailsHeader'>\
                    </div>\
                    <div id='sshDetails'>\
                        <table border='0' width='100%'>\
                        <tr><td style='width:30%;'><label for='sd_keybased'>Key based SSH</label>\
                            <span class='helpIcon' id='sd_keybased_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='sd_keybased'></div>\
                            </td></tr>\
                        <tr><td style='width:30%;'><label for='sd_user'>User name</label>\
                            <span class='helpIcon' id='sd_user_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='sd_user'></div>\
                            </td></tr>\
                        <tr><td style='width:30%;'><label for='sd_pwd'>Password</label>\
                            <span class='helpIcon' id='sd_pwd_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='sd_pwd'></div>\
                            </td></tr>\
                        </table>\
                    </div>\
                    <div id='installDetailsHeader'>\
                    </div>\
                    <div id='installDetails'>\
                        <table border='0' width='100%'>\
                        <tr><td style='width:30%;'><label for='sd_installCluster'>Install MySQL Cluster</label>\
                            <span class='helpIcon' id='sd_installCluster_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='sd_installCluster'></div>\
                            </td></tr>\
                        <tr><td style='width:30%;'><label for='sd_openfw'>Open FW ports</label>\
                            <span class='helpIcon' id='sd_openfw_qm'>[?]\
                            </span></td>\
                            <td width='70%'><div id='sd_openfw'></div>\
                            </td></tr>\
                        </table>\
                    </div>\
                </div>\
            </div>",
        enter: function() {
            mcc.util.dbg("Enter configWizardDefineCluster");
            mcc.gui.showClusterDefinition(initializeClusterDef);
            initializeClusterDef = false; 
        },
        exit: function() {
            mcc.util.dbg("Exit configWizardDefineCluster");
            mcc.gui.saveClusterDefinition();
            // Check errors after a timeout to allow hosts to be contacted
            setTimeout(
                function() {
                    mcc.storage.hostStorage().getItems().then(function (hosts) {
                        var errMsgSummary = null;
                        for (var i in hosts) {
                            var errMsg = hosts[i].getValue("errMsg");
                            if (errMsg) {
                                if (!errMsgSummary) {
                                    errMsgSummary = "There were errors when connecting to remote host(s)\n";
                                    errMsgSummary += "using Cluster-wide credentials. Probably host(s) have\n";
                                    errMsgSummary += "their own credentials which you can set on next page (Edit host(s)):\n\n"
                                }
                                errMsgSummary += "Host '" + hosts[i].getValue("name") + "': " + errMsg + "\n"
                            }
                        }
                        if (errMsgSummary) {                
                            errMsgSummary += "\nPress 'OK' to continue to the next page anyway, or 'Cancel' to stay at the previous page";
                            mcc.storage.hostStorage().save();                
                            if (!confirm(errMsgSummary)) {
                                cancelTabChange("configWizardDefineCluster");
                            }
                        }
                    });
                }, 1000);
        }
    },
    configWizardDefineHosts: {
        content: "\
            <div dojoType='dijit.layout.BorderContainer' \
                    gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='top'\
                        class='content-main-top-panel'>\
                    <h2>Select and Edit Hosts</h2>\
                    <span class='content-tab-sub-title'>MySQL Cluster can \
                            be deployed on several hosts. Please select the \
                            desired hosts by pressing the <i>Add host</i> \
                            button below and enter a comma separated list of \
                            host names or ip addresses. Resource information \
                            is automatically retrieved from the added host if \
                            this is checked in the settings menu, and if the \
                            required SSH credentials have been submitted. \
                            When a host has been added, the corresponding \
                            information can be edited by double clicking a cell\
                            in the grid. If you want to apply the same changes\
                            to several hosts, multiple rows can be selected \
                            and the <i>Edit selected host(s)</i> button can \
                            be pressed, which shows a dialog where the editing\
                            can be done. Hosts can be deleted by selecting the \
                            corresponding rows in the table and pressing the \
                            <i>Remove selected host(s)</i> button. If a host \
                            is removed, processes configured to run on that \
                            host will also be removed from the configuration.\
                    </span>\
                 </div>\
                <div dojoType='dijit.layout.ContentPane' region='center'\
                        class='content-tab-center-panel'>\
                    <div id='hostGrid'></div>\
                 </div>\
                 <div dojoType='dijit.layout.ContentPane' region='bottom'\
                        class='content-tab-bottom-panel'>\
                    <span>\
                        <div id='addHostsButton'></div>\
                        <div id='removeHostsButton'></div>\
                        <div id='editHostsButton'></div>\
                        <div id='refreshHostsButton'></div>\
                        <div id='toggleHostInfoButton'></div>\
                    </span>\
                </div>\
            </div>",                   
        enter: function() {
            mcc.util.dbg("Enter configWizardDefineHosts");
            mcc.gui.hostGridSetup();
        },
        exit: function() {
            mcc.util.dbg("Exit configWizardDefineHosts");
            mcc.storage.hostStorage().save();
        }
    },
    configWizardDefineProcesses: {
        content: "\
            <div dojoType='dijit.layout.BorderContainer' gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='top'>\
                    <h2>Define Processes and Cluster Topology</h2>\
                    <span class='content-tab-sub-title'>Various processes may \
                            be part of a MySQL Cluster configuration. Please \
                            refer to the <a href='" + mcc.util.getDocUrlRoot() + 
                            "mysql-cluster.html'>MySQL Cluster \
                            Documentation</a> for a description of the \
                            different process types. If you have added hosts \
                            previously, a default configuration will be \
                            suggested the first time you enter this page. This \
                            configuration may be modified by moving processes \
                            between hosts by drag and drop, or by adding and \
                            removing processes. You may also go back to the \
                            previous page and add more hosts before editing \
                            the topology. The special entry labelled <i>Any \
                            host</i> in the tree below represents an arbitrary \
                            host. On this special tree entry, only <i>API</i> \
                            processes can be moved or added. These processes \
                            will not be required to run on a particular host, \
                            but may execute anywhere.\
                    </span>\
                </div>\
                <div dojoType='dijit.layout.ContentPane' region='center'\
                        class='content-tab-center-panel'>\
                    <div dojoType='dijit.layout.BorderContainer'\
                            gutters='false'>\
                        <div dojoType='dijit.layout.ContentPane' \
                                region='leading'\
                                splitter='true' style='padding:0'>\
                            <div dojoType='dijit.layout.BorderContainer'\
                                    gutters='false' class='content-tree-panel'>\
                                <div dojoType='dijit.layout.ContentPane' \
                                        region='top'\
                                        style='height: 22px; padding:0;'>\
                                    <div id='hostTreeHeader'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='center'>\
                                    <div id='hostTree'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='bottom' style='height: 28px;'>\
                                    <span>\
                                        <div id='addProcessButton'></div>\
                                        <div id='deleteProcessButton'></div>\
                                    </span>\
                                </div>\
                            </div>\
                        </div>\
                    <!-- Details pane -->\
                    <div dojoType='dijit.layout.ContentPane' region='center'\
                             style='padding:0'>\
                        <div dojoType='dijit.layout.StackContainer'\
                                id='hostTreeSelectionDetails' \
                                class='content-tree-details-panel'>\
                            <div dojoType='dijit.layout.ContentPane'\
                                title='No Details' \
                                id='noHostTreeSelectionDetails'>\
                            </div>\
                            <div dojoType='dijit.layout.ContentPane'\
                                    title='Host Details' \
                                    id='hostSelectionDetails'\
                                    style='padding: 0'>\
                                <div dojoType='dijit.layout.BorderContainer'\
                                        gutters='false'>\
                                    <div dojoType='dijit.layout.ContentPane'\
                                            region='top' style='height: 22px;\
                                                    padding:0; '>\
                                        <div id='hostDetailsHeader'></div>\
                                    </div>\
                                    <div dojoType='dijit.layout.ContentPane'\
                                            region='center' id='hostDetails'>\
                                    </div>\
                                </div>\
                            </div>\
                            <div dojoType='dijit.layout.ContentPane'\
                                    title='Process Details'\
                                    id='processSelectionDetails'\
                                     style='padding:0'>\
                                <div dojoType='dijit.layout.BorderContainer'\
                                        gutters='false'>\
                                    <div dojoType='dijit.layout.ContentPane'\
                                            region='top'\
                                            style='height: 22px; \
                                                    padding:0; '>\
                                        <div id='processDetailsHeader'></div>\
                                    </div>\
                                    <div dojoType='dijit.layout.ContentPane'\
                                            region='center' \
                                            id='processDetails'>\
                                    </div>\
                                </div>\
                            </div>\
                        </div>\
                    </div>\
                </div>\
            </div>",
        enter: function () {
            mcc.util.dbg("Enter configWizardDefineProcesses");
            mcc.gui.hostTreeSetup();
            mcc.gui.hostTreeSelectionDetailsSetup();
            mcc.gui.updateHostTreeSelectionDetails();
        },
        exit: function () {
            mcc.util.dbg("Exit configWizardDefineProcesses");
        }
    },
    configWizardDefineParameters: {
        content: "\
            <div dojoType='dijit.layout.BorderContainer' gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='top'>\
                    <h2>Define Processes Parameters</h2>\
                    <span class='content-tab-sub-title'>The processes in your \
                            MySQL Cluster configuration can be tuned by setting\
                            a number of configuration parameters. Please refer \
                            to the <a href='" + mcc.util.getDocUrlRoot() + 
                            "mysql-cluster.html'>MySQL Cluster \
                            Documentation</a> for a description of the \
                            different process parameters. This page allows you \
                            to define a subset of the configuration parameters.\
                            Below, you will see your \
                            processes to the left grouped by process type. \
                            If you select a process type entry in the tree, \
                            you may set parameters that will be applied to all \
                            instances of that process. However, if you want to \
                            set a parameter specifically for one process, you \
                            may do so by selecting the process instance in the \
                            tree and set the desired parameter. This tool \
                            suggests predefined settings for the different \
                            parameteres based on the hardware resources and the\
                            cluster topology. The predefined settings may be \
                            overridden by pressing the <i>Override</i> button \
                            to the very right of the configuration parameter. \
                            If you want to cancel your setting, you may revert \
                            to the predefined value by pressing the \
                            <i>Revert</i> button which shows up when a \
                            parameter is overridden. \
                    </span>\
                </div>\
                <div dojoType='dijit.layout.ContentPane' region='center'\
                        class='content-tab-center-panel'>\
                    <div dojoType='dijit.layout.BorderContainer' \
                            gutters='false'>\
                        <div dojoType='dijit.layout.ContentPane' \
                                region='leading'\
                                splitter='true' style='padding: 0'>\
                            <div dojoType='dijit.layout.BorderContainer'\
                                    gutters='false' class='content-tree-panel'>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='top' style='height: 22px; \
                                        padding:0;'>\
                                    <div id='processTreeHeader'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='center'>\
                                    <div id='processTree'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='bottom' style='height: 28px;'>\
                                    <span>\
                                        <div id='advancedModeBox'></div>\
                                        <label for='advancedModeBox'>Show advanced configuration options</label>\
                                    </span>\
                                </div>\
                            </div>\
                        </div>\
                        <!-- Details pane -->\
                        <div dojoType='dijit.layout.ContentPane' \
                                region='center'\
                                style='padding:0; '>\
                            <div dojoType='dijit.layout.StackContainer'\
                                    class='content-tree-details-panel'\
                                    id='processTreeSelectionDetails'>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        title='No Details'\
                                        id='noProcessTreeSelectionDetails'>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        title='Process Type Details'\
                                        id='processTypeSelectionDetails'\
                                        style='padding: 0'>\
                                   <div dojoType='dijit.layout.BorderContainer'\
                                            gutters='false'>\
                                       <div dojoType='dijit.layout.ContentPane'\
                                                region='top'\
                                                style='height: 22px; \
                                                padding:0;'>\
                                            <div id='processTypeDetailsHeader'>\
                                            </div>\
                                        </div>\
                                    <div dojoType='dijit.layout.StackContainer'\
                                                region='center'\
                                                id='processTypeDetails'>\
                                        </div>\
                                    </div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        title='Process Instance Details'\
                                        id='processInstanceSelectionDetails'\
                                        style='padding: 0'>\
                                   <div dojoType='dijit.layout.BorderContainer'\
                                            gutters='false'>\
                                       <div dojoType='dijit.layout.ContentPane'\
                                                region='top'\
                                                style='height: 22px; \
                                                padding:0; '>\
                                        <div id='processInstanceDetailsHeader'>\
                                            </div>\
                                        </div>\
                                    <div dojoType='dijit.layout.StackContainer'\
                                                region='center'\
                                                id='processInstanceDetails'>\
                                        </div>\
                                    </div>\
                                </div>\
                            </div>\
                        </div>\
                    </div>\
                </div>\
            </div>",
        enter: function () {
            mcc.util.dbg("Enter configWizardDefineParameters");
            mcc.gui.processTreeSetup();
            mcc.gui.updateProcessTreeSelectionDetails();
        },
        exit: function () {
            mcc.util.dbg("Exit configWizardDefineParameters");
        }
    },
    configWizardDeployConfig: {
        content: "\
            <div dojoType='dijit.layout.BorderContainer' gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='top'>\
                    <h2>Deploy Configuration and start MySQL Cluster</h2>\
                    <span class='content-tab-sub-title'>Your \
                            MySQL Cluster configuration can be reviewed \
                            below. To the left are the processes you have \
                            defined, ordered by their startup sequence.\
                            Please select a process to view its startup \
                            command(s) and configuration file. Note that some \
                            processes do not have configuration files. \
                            At the bottom of the center panel, there are \
                            buttons to <i>Deploy</i>, <i>Start</i> and <i>Stop\
                            </i> your cluster. Please note that starting the \
                            cluster may take up to several minutes depending \
                            on the configuration you have defined. \
                            In the process tree, the icons reflect the \
                            status of the process as reported by the management\
                            daemon: <span class=\"dijitIconFunction\" \
                            style=\"position: relative; overflow: auto; display:\
                            inline-block; width:17px\">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>\
                            &nbsp;:&nbsp;<i>unknown</i> or \
                            if the management daemon does not reply, \
                            <img src=img/greenlight.gif></img>&nbsp;:&nbsp;\
                            <i>connected</i> or\
                            <i>started</i>, <img src=img/yellowlight.gif></img>&nbsp;:&nbsp;\
                            <i>starting</i> or\
                            <i>shutting down</i>, and <img src=img/redlight.gif></img>&nbsp;:&nbsp;\
                            <i>not connected</i> or\
                            <i>stopped</i>. \
                    </span>\
                </div>\
                <div dojoType='dijit.layout.ContentPane' region='center'\
                        class='content-tab-center-panel'>\
            <div dojoType='dijit.layout.BorderContainer' gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='leading'\
                        splitter='true' style='padding: 0'>\
                    <div dojoType='dijit.layout.BorderContainer'\
                            gutters='false' class='content-tree-panel'>\
                        <div dojoType='dijit.layout.ContentPane'\
                                region='top' style='height: 22px; \
                                padding:0;'>\
                            <div id='deploymentTreeHeader'></div>\
                        </div>\
                        <div dojoType='dijit.layout.ContentPane'\
                                region='center'>\
                            <div id='deploymentTree'></div>\
                        </div>\
                    </div>\
                </div>\
                <!-- Details pane -->\
                <div dojoType='dijit.layout.ContentPane' region='center'\
                        style='padding:0; margin:0; border: 0;'>\
                    <div dojoType='dijit.layout.BorderContainer'\
                            class='content-tree-details-panel'\
                            id='deploymentTreeSelectionDetails'>\
                        <div dojoType='dijit.layout.ContentPane'\
                                title='Startup Command'\
                                style='padding:0; height: 30%'\
                                region='top'\
                                splitter='true'\
                                id='startupSelectionDetails'>\
                            <div dojoType='dijit.layout.BorderContainer'\
                                    gutters='false'>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='top'\
                                        style='height: 22px; padding:0; '>\
                                    <div id='startupDetailsHeader'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='center'\
                                        id='startupDetails'>\
                                </div>\
                            </div>\
                        </div>\
                        <div dojoType='dijit.layout.ContentPane'\
                                title='Configuration File'\
                                region='center'\
                                style='padding:0; '\
                                id='configSelectionDetails'>\
                            <div dojoType='dijit.layout.BorderContainer'\
                                    gutters='false'>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='top'\
                                        style='height: 22px; padding:0; '>\
                                    <div id='configDetailsHeader'>\
                                    </div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='center'\
                                        id='configDetails'>\
                                </div>\
                            </div>\
                        </div>\
                    </div>\
                </div>\
                <!-- Footer -->\
                <div data-dojo-type='dijit.layout.ContentPane'\
                        data-dojo-props='region:\"bottom\", splitter:false'>\
                    <div data-dojo-type='dijit.form.Button'\
                        data-dojo-props='iconClass: \"installIcon\"'\
                        onClick='mcc.configuration.installCluster()'\
                        id='configWizardInstallCluster'>Install cluster\
                    </div>\
                    <div data-dojo-type='dijit.form.Button'\
                        data-dojo-props='iconClass: \"deployIcon\"'\
                        onClick='mcc.configuration.deployCluster()'\
                        id='configWizardDeployCluster'>Deploy cluster\
                    </div>\
                    <div data-dojo-type='dijit.form.Button'\
                        data-dojo-props='iconClass: \"startIcon\"'\
                        onClick='mcc.configuration.startCluster()'\
                        id='configWizardStartCluster'>Start cluster\
                    </div>\
                    <div data-dojo-type='dijit.form.Button'\
                        data-dojo-props='iconClass: \"stopIcon\"'\
                        onClick='mcc.configuration.stopCluster()'\
                        id='configWizardStopCluster'>Stop cluster\
                    </div>\
                </div>\
            </div>",
        enter: function () {
            mcc.util.dbg("Enter configWizardDeployConfig");
            mcc.configuration.setupContext().then(function () {
                dijit.byId("configWizardInstallCluster").setDisabled(true);
                mcc.gui.deploymentTreeSetup();
                mcc.gui.startStatusPoll(true);
                var clRunning = mcc.configuration.clServStatus();
                dijit.byId("configWizardStopCluster").setDisabled(!mcc.configuration.determineClusterRunning(clRunning));
                dijit.byId("configWizardStartCluster").setDisabled(mcc.configuration.determineClusterRunning(clRunning));
            });
        },
        exit: function () {
            mcc.util.dbg("Exit configWizardDeployConfig");
            mcc.gui.stopStatusPoll();
        }
    }
};

/****************************** Implementation  *******************************/

// Show next page
function nextPage() {
    var current = dojo.indexOf(configWizardPageIds, 
            dijit.byId("content-main-tab-container").selectedChildWidget.id);
    if (current != "configWizardDefineCluster") {
        //Ignore changes to Cluster lvl.
        var cs = mcc.storage.clusterStorage();
        var hs = mcc.storage.hostStorage();
        var ps = mcc.storage.processStorage();
        var pts= mcc.storage.processTypeStorage();
        var toWrite = cs.store()._getNewFileContentString() + ', ' + hs.store()._getNewFileContentString() + ', {}, ' + ps.store()._getNewFileContentString() + ', ' + pts.store()._getNewFileContentString();
        mcc.util.dbg("Executing cfg save.");
        var res = mcc.userconfig.writeConfigFile(toWrite,mcc.userconfig.getConfigFile());
    };
    dijit.byId("content-main-tab-container").selectChild(
            dijit.byId(configWizardPageIds[current + 1]));
    
}

// Show previous page
function prevPage() {
    var current = dojo.indexOf(configWizardPageIds, 
            dijit.byId("content-main-tab-container").selectedChildWidget.id);
    dijit.byId("content-main-tab-container").selectChild(
            dijit.byId(configWizardPageIds[current - 1]));
}

// Show last page
function lastPage() {
    dijit.byId("content-main-tab-container").selectChild(
            dijit.byId(configWizardPageIds[configWizardPageIds.length - 1]));
}

// Reload current page
function reloadPage() {
    var wiz = dijit.byId("content-main-tab-container");
    var pages = wiz.getChildren();
    var current = wiz.selectedChildWidget;

    if (current) {
        mcc.util.dbg("Reload page " + current.id);

        // Keep toplevel child
        for (var i in current.getChildren()) {
            mcc.util.dbg("Destroy " + current.getChildren()[i].id);
            current.getChildren()[i].destroyRecursive();
        }

        // Reset content and re-enter
        if (configWizardPages[current.id].content) {
            current.setContent(configWizardPages[current.id].content);
        }
        if (configWizardPages[current.id].enter) {
            configWizardPages[current.id].enter();
        }
    }
}

// Setup buttons for previous/next page.
function configWizardEnableButtons(page) {
    if (page.isFirstChild) {
        dijit.byId("configWizardPrevPage").setAttribute("disabled", true);
    } else {
        dijit.byId("configWizardPrevPage").setAttribute("disabled", false);
    }

    if (page.isLastChild) {
        dijit.byId("configWizardNextPage").setAttribute("disabled", true);
        dijit.byId("configWizardLastPage").setAttribute("disabled", true);
    } else {
        dijit.byId("configWizardNextPage").setAttribute("disabled", false);
        dijit.byId("configWizardLastPage").setAttribute("disabled", false);
    }
}

// Setup help menu for the configuration wizard
function helpMenuSetup() {
    var menu = new dijit.DropDownMenu({style: "display: none;"});
    var menuItemContents = new dijit.MenuItem({
        label: "Contents",
        onClick: function() {
            window.open("hlp/html/help.html",
                "MySQL Cluster Configuration",
                "scrollbars=1, \
                width=1170, \
                height=750, \
                screenX=20, \
                screenY=100, \
                left=20, \
                top=100");
        }
    });
    menu.addChild(menuItemContents);

    var menuItemContext = new dijit.MenuItem({
        label: "Current page",
        onClick: function() {
            var wiz = dijit.byId("content-main-tab-container");
            var current = wiz.selectedChildWidget;
            var helpLabel = "";

            if (current) {
                helpLabel = "#" + current.id;
            }
            window.open("hlp/html/help_cnt.html" + helpLabel, 
                "MySQL Cluster Configuration",
                "scrollbars=1, \
                width=1170, \
                height=750, \
                screenX=20, \
                screenY=100, \
                left=20, \
                top=100");
        }
    });
    menu.addChild(menuItemContext);

    var menuItemAbout = new dijit.MenuItem({
        label: "About",
        onClick: function() {
            if (!dijit.byId("aboutDialog")) {
                var dlg = new dijit.Dialog({
                    id: "aboutDialog",
                    title: "About MySQL Cluster Configuration Tool",
                    content: "\
                            <div><img src='img/content-title.png'></div>\
                            <p>Version: mysql-5.7-cluster-7.6</p>\
                            <button id='termsButton' \
                                    data-dojo-type='dijit.form.Button'\
                                    type='button'>\
                                Close \
                                <script type='dojo/method' \
                                        data-dojo-event='onClick'\
                                        data-dojo-args='evt'>\
                                    dijit.byId('aboutDialog').hide();\
                                </script>\
                            </button>"
                });
            }
            dijit.byId("aboutDialog").show();
        }
    });
    menu.addChild(menuItemAbout);

    var menuButton= new dijit.form.DropDownButton({
        label: "Help",
        dropDown: menu,
        baseClass: "menuButton",
        showLabel: true,
        style: "float: right; ",
        id: "configWizardHelpButton"
    });
    dojo.byId("content-main-tab-container_tablist").appendChild(menuButton.domNode);
}

// Setup settings menu for the configuration wizard
function configWizardMenuSetup() {

    var menu = new dijit.DropDownMenu({style: "display: none;"});
    /*var menuItemClear= new dijit.MenuItem({
        id: "configWizardMenuClear",
        label: "Clear configuration and restart",
        iconClass:"dijitIconDelete",
        onClick: function() {
            mcc.util.resetCookies(); 
            mcc.configuration.resetDefaultValueInstance();
            mcc.storage.initializeStores(); 
            mcc.storage.initializeStorage().then(function () {
                // Dirty fix to handle when we're already on page 0
                initializeClusterDef = true;
                reloadPage();
                dijit.byId("content-main-tab-container").selectChild(
                        dijit.byId(configWizardPageIds[0]));
            });
        }
    });
    menu.addChild(menuItemClear);*/

    var menuItemAutoSave= new dijit.CheckedMenuItem({
        label: "Automatically save configuration as cookies",
        checked: (mcc.util.getCookie("autoSave") == "on"),
        disabled: true,
        onChange: function(val) {
            if (val) {
                mcc.util.setCookie("autoSave", "on");
                mcc.storage.setupSaveExtensions(); 
            } else {
                mcc.util.setCookie("autoSave", "off");
                mcc.storage.resetSaveExtensions(); 
                mcc.util.resetCookies(); 
            }
        }
    });
    menu.addChild(menuItemAutoSave);

    var menuItemLevel= new dijit.CheckedMenuItem({
        id: "advancedModeMenuItem",
        label: "Show advanced configuration options",
        checked: (mcc.util.getCookie("configLevel") == "advanced"),
        onChange: function(val) { 
            if (val) {
                mcc.util.setCookie("configLevel", "advanced");
            } else {
                mcc.util.setCookie("configLevel", "simple");
            }
            reloadPage();
        }
    });
    menu.addChild(menuItemLevel);

    var menuItemHostInfo= new dijit.CheckedMenuItem({
        label: "Automatically get resource information for new hosts",
        checked: (mcc.util.getCookie("getHostInfo") == "on"),
        onChange: function(val) { 
            if (val) {
                mcc.util.setCookie("getHostInfo", "on");
            } else {
                mcc.util.setCookie("getHostInfo", "off");
            }
            reloadPage();
        }
    });
    menu.addChild(menuItemHostInfo);

    var menuButton= new dijit.form.DropDownButton({
        label: "Settings",
        baseClass: "menuButton",
        dropDown: menu,
        showLabel: true,
        style: "float: right; ",
        id: "configWizardMenuButton"
    });
    dojo.byId("content-main-tab-container_tablist").appendChild(menuButton.domNode);
}

// Setup a timer to stay at old child
function cancelTabChange(stayAt) {
    setTimeout("dijit.byId(\"content-main-tab-container\").selectChild(dijit.byId(\"" + 
            stayAt + "\"))", 5);
}

// Select and enter first page
function enterFirst() {
    var firstPage = dijit.byId(configWizardPageIds[0]);

    dijit.byId("content-main-tab-container").selectChild(firstPage);
    firstPage.setContent(configWizardPages[firstPage.id].content);
    configWizardPages[firstPage.id].enter();

    // Enable buttons as appropriate
    configWizardEnableButtons(firstPage);
}

// Initialize the config wizard
function initialize() {
    // Setup the menus
    helpMenuSetup();
    configWizardMenuSetup();

    // Install child selection monitor: Check validity, call exit/enter
    dijit.byId("content-main-tab-container").watch("selectedChildWidget", 
            function (name, prev, next) {
        // Log transition
        mcc.util.dbg("Wizard page: " + prev.id + " -> " + next.id);
        configWizardEnableButtons(next);

        // Check validity of transition
        if (false) {
            alert("Illegal transition");
            cancelTabChange(prev.id);
        } else {
            var prevIndex = dojo.indexOf(configWizardPageIds, prev.id);
            var nextIndex = dojo.indexOf(configWizardPageIds, next.id);
            var preCondition = new dojo.Deferred();

            // Check if we need some action on this transition
            if (prevIndex <= 1 && nextIndex >= 2) {
                preCondition = mcc.configuration.autoConfigure();
            } else {
                preCondition.resolve();
            }

            preCondition.then(function () {
                
                // Exit old page
                if (configWizardPages[prev.id].exit) {
                    configWizardPages[prev.id].exit();
                }

                // Teardown old content
                for (var i in prev.getChildren()) {
                    prev.getChildren()[i].destroyRecursive();
                }

                // Setup new content
                if (configWizardPages[next.id].content) {
                    next.setContent(configWizardPages[next.id].content);
                }

                // Enter new page
                if (configWizardPages[next.id].enter) {
                    configWizardPages[next.id].enter();
                }
            });
        }
    });
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    initialize();
    mcc.util.dbg("Configuration wizard module initialized");
});



