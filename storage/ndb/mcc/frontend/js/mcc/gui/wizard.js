/*
Copyright (c) 2012, 2020, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

/******************************************************************************
 ***                                                                        ***
 ***                     Configuration wizard definition                    ***
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
 *      mcc.gui.wizard.enterFirst: Select and enter first page from welcome.html!
 *      mcc.gui.wizard.nextPage: Show next page
 *      mcc.gui.wizard.prevPage: Show previous page
 *      mcc.gui.wizard.lastPage: Show last page
 *      mcc.gui.wizard.reloadPage: Reload current page
 *      mcc.gui.wizard.getClCfgProblems
 *      mcc.gui.wizard.getHoCfgProblems
 *      mcc.gui.wizard.getPrCfgProblems
 *      mcc.gui.wizard.getGeCfgProblems
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
dojo.provide('mcc.gui.wizard');
dojo.require('dijit.form.Button');
dojo.require('dijit.form.DropDownButton');
dojo.require('dojox.form.BusyButton');
dojo.require('dijit.DropDownMenu');
dojo.require('dijit.MenuItem');
dojo.require('dijit.Dialog');

dojo.require('mcc.util');
dojo.require('mcc.storage');
dojo.require('mcc.configuration');
// "Special" units for IE11...
if (!!window.MSInputMethodContext && !!document.documentMode) {
    dojo.require('mcc.userconfigIE');
} else {
    dojo.require('mcc.userconfig');
}
dojo.require('mcc.gui');

/**************************** External interface  *****************************/
mcc.gui.wizard.enterFirst = enterFirst;
mcc.gui.wizard.nextPage = nextPage;
mcc.gui.wizard.prevPage = prevPage;
mcc.gui.wizard.lastPage = lastPage;
mcc.gui.wizard.reloadPage = reloadPage;
mcc.gui.wizard.getClCfgProblems = getClCfgProblems;
mcc.gui.wizard.getHoCfgProblems = getHoCfgProblems;
mcc.gui.wizard.getPrCfgProblems = getPrCfgProblems;
mcc.gui.wizard.getGeCfgProblems = getGeCfgProblems;

/******************************* Internal data ********************************/
var initializeClusterDef = true;

var configWizardPageIds = [
    'configWizardDefineCluster',
    'configWizardDefineHosts',
    'configWizardDefineProcesses',
    'configWizardDefineParameters',
    'configWizardDeployConfig'
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
                        <tr><td style='width:25%;'><label for='cd_name'>Cluster name</label> \
                            <span class='helpIcon' id='cd_name_qm'>[?]\
                            </span></td>\
                            <td width='75%'><div id='cd_name'></div>\
                            </td></tr>\
                        <tr><td style='width:25%;'><label for='cd_hosts'>Host list</label> \
                            <span class='helpIcon' id='cd_hosts_qm'>[?]\
                            </span></td>\
                            <td width='75%'><div id='cd_hosts' tabIndex=1></div>\
                            </td></tr>\
                        <tr></tr>\
                        </table>\
                        <table border='0' width='100%'>\
                        <tr><td style='width:25%;'><label for='cd_apparea'>Application area</label> \
                            <span class='helpIcon' id='cd_apparea_qm'>[?]\
                            </span></td>\
                            <td width='25%'><div id='cd_apparea' tabIndex=0></div>\
                            </td>\
                            <td style='width:25%;'><label for='cd_writeload'>Write load</label> \
                            <span class='helpIcon' id='cd_writeload_qm'>[?]\
                            </span></td>\
                            <td width='25%'><div id='cd_writeload' tabIndex=0></div>\
                            </td></tr>\
                        <tr><td style='width:25%;'><label for='cd_masternode'>Master node</label> \
                            <span class='helpIcon' id='cd_masternode_qm'>[?]\
                            </span></td>\
                            <td width='25%'><div id='cd_masternode' tabIndex=0></div>\
                            </td>\
                            <td style='width:25%;'><label for='cd_clver'>Cluster version</label> \
                            <span class='helpIcon' id='cd_clver_qm'>[?]\
                            </span></td>\
                            <td width='25%'><div id='cd_clver' tabIndex=0></div>\
                            </td>\
                         </tr>\
                        </table>\
                    </div>\
                    <div id='sshDetailsHeader'>\
                    </div>\
                    <div id='sshDetails'>\
                        <table border='0' width='100%'>\
                        <tr>\
                            <td><div id='sd_ResetCr'></div>\
                            </td>\
                            <td></td>\
                        </tr>\
                        <tr>\
                            <td style='width:25%;'>\
                                <label for='cbKeybased'>Key based SSH</label>\
                                <span class='helpIcon' id='sd_keybased_qm'>[?]\
                                </span>\
                            </td>\
                            <td width='10%'>\
                                <div id='cbKeybased' tabIndex=0></div>\
                            </td>\
                            <td style='width:25%;'>\
                                <label for='sd_ClkeyUser'>Key user:</label>\
                                <span class='helpIcon' id='sd_ClkeyUser_qm'>[?]\
                                </span>\
                            </td>\
                            <td>\
                                <div id='sd_ClkeyUser' tabIndex=0></div>\
                            </td>\
                        </tr>\
                        <tr><td style='width:25%;'><label for='sd_user'>User name</label>\
                            <span class='helpIcon' id='sd_user_qm'>[?]\
                            </span></td>\
                            <td width='25%'><div id='sd_user' tabIndex=0></div>\
                            </td>\
                            <td style='width:25%;'><label for='sd_ClkeyPass' id='labelforpp'>Key passphrase:</label>\
                                <span class='helpIcon' id='sd_ClkeyPass_qm'>[?]&nbsp&nbsp</span>\
                                <span><input type='button' class='tglbtn' id='togglePassphraseField' value=''></span>\
                            </td>\
                            <td width='25%'>\
                                <input id='sd_ClkeyPass' type='password' name='sd_ClkeyPass' value='' tabIndex=0 autocomplete='off' style='text-align: left;width:120px;height:100%;display=\"inline\"'>\
                            </td>\
                        </tr>\
                        <tr><td style='width:25%;'><label for='sd_pwd' id='labelforpwd'>Password&nbsp</label>\
                                <span class='helpIcon' id='sd_pwd_qm'>&nbsp[?]&nbsp&nbsp</span>\
                                <span><input type='button' class='tglbtn' id='togglePasswordField' value=''></span>\
                            </td>\
                            <td width='25%'>\
                                <input id='sd_pwd' type='password' name='sd_pwd' value='' autocomplete='off' tabIndex=0 style='text-align: left;width:120px;height:100%;display=\"inline\"'>\
                            </td>\
                            <td style='width:25%;'><label for='sd_ClkeyFile'>Key file:</label>\
                            <span class='helpIcon' id='sd_ClkeyFile_qm'>[?]\
                            </span></td>\
                            <td width='25%'><div id='sd_ClkeyFile' tabIndex=0></div>\
                            </td>\
                            </tr>\
                        </table>\
                    </div>\
                    <div id='installDetailsHeader'>\
                    </div>\
                    <div id='installDetails'>\
                        <table border='0' width='100%'>\
                            <tr><td style='width:25%;'><label for='sd_installCluster'>Install MySQL Cluster</label>\
                                <span class='helpIcon' id='sd_installCluster_qm'>[?]\
                                </span></td>\
                                <td width='25%'><div id='sd_installCluster' tabIndex=0></div>\
                                </td></tr>\
                            <tr><td style='width:25%;'><label for='cbOpenfw'>Open ports in firewall</label>\
                                <span class='helpIcon' id='sd_openfw_qm'>[?]\
                                </span></td>\
                                <td width='25%'><div id='cbOpenfw' tabIndex=0></div>\
                                </td>\
                            </tr>\
                            <tr><td style='width:25%;'><label for='cbUsevpn'>Use Internal IP address (VPN)</label>\
                                <span class='helpIcon' id='sd_usevpn_qm'>[?]\
                                </span></td>\
                                <td width='25%'><div id='cbUsevpn' tabIndex=0></div>\
                                </td>\
                            </tr>\
                        </table>\
                    </div>\
                </div>\
            </div>",
        enter: function () {
            console.info('[INF]Enter configWizardDefineCluster');
            // Either it is not running or we do not know yet as not all
            // modules have been initialized.
            mcc.gui.showClusterDefinition(initializeClusterDef);
            initializeClusterDef = false;
            // mcc.gui.setLocalHome(mcc.storage.getHostResourceInfo("LOCAL", -1, false, false));

            // user can skip over HOSTS page so better set the reminder sooner...
            // Did we already collected and showed this?
            var msg = '';
            var what = {};
            mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
                mcc.storage.hostStorage().getItems().then(function (hosts) {
                    console.debug('[DBG]Setting up pwd reminder for hosts.');
                    for (var i in hosts) {
                        if (hosts[i].getValue('usrpwd') && hosts[i].getValue('usrpwd').length === 1) {
                            what = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr(
                                'pwdMissingHosts', [hosts[i].getValue('name')]));
                            if ((what || {}).text) {
                                msg += what.text;
                            } // we already warned for this host
                        }
                        if (hosts[i].getValue('key_passp') && hosts[i].getValue('key_passp').length === 1) {
                            what = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr(
                                'ppMissingHosts', [hosts[i].getValue('name')]));
                            if ((what || {}).text) {
                                msg += what.text;
                            } // we already warned for this host
                        }
                    }
                });
            });
            if (msg) {
                mcc.util.displayModal('H', 0, '<span style="font-size:135%;color:orangered;">' + msg + '</span>',
                    '<span style="font-size:150%;color:red">Passwords missing for host(s):</span>');
            }
        },
        exit: function () {
            console.info('[INF]Exit configWizardDefineCluster');
            mcc.gui.saveClusterDefinition();
            // Do we have Cluster-wide credentials to work with at all?
            var proceed = false;
            if (mcc.gui.getSSHkeybased()) {
                var a = mcc.gui.getClSSHPwd();
                var b = mcc.gui.getClSSHUser();
                var c = mcc.gui.getClSSHKeyFile();
                if (a.length > 1 || b.length > 1 || c.length > 1) {
                    proceed = true;
                }
            } else {
                var d = mcc.gui.getSSHPwd();
                var e = mcc.gui.getSSHUser();
                if (d.length > 1 || e.length > 1) {
                    proceed = true;
                }
            }
            if (proceed) {
                // we can try polling for status now if we have structures initialized
                // do same thing for hosts (all next version)
                // mcc.gui.startStatusPoll(true);
            }
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('cluster')) {
                mcc.userconfig.setOriginalStore('cluster');
            }
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
        enter: function () {
            console.info('[INF]Enter configWizardDefineHosts');
            mcc.gui.hostGridSetup();

            var msg = '';
            var what = {};
            mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
                mcc.storage.hostStorage().getItems().then(function (hosts) {
                    console.debug('[DBG]Setting up pwd reminder for hosts.');
                    for (var i in hosts) {
                        if (hosts[i].getValue('usrpwd') && hosts[i].getValue('usrpwd').length === 1) {
                            what = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr(
                                'pwdMissingHosts', [hosts[i].getValue('name')]));
                            if ((what || {}).text) {
                                msg += what.text;
                            } // we already warned for this host
                        }
                        if (hosts[i].getValue('key_passp') && hosts[i].getValue('key_passp').length === 1) {
                            what = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr(
                                'ppMissingHosts', [hosts[i].getValue('name')]));
                            if ((what || {}).text) {
                                msg += what.text;
                            } // we already warned for this host
                        }
                    }
                });
            });
            if (msg) {
                mcc.util.displayModal('H', 0, '<span style="font-size:135%;color:orangered;">' + msg + '</span>',
                    '<span style="font-size:150%;color:red">Passwords missing for host(s):</span>');
            }
            // Fetch resources unless Cluster running and Res=OK. Mimics
            // auto refresh with Cluster-wide credentials.
            var clRunning = [];
            // storage initializes config module, still maybe a good idea to do TRY here?.
            clRunning = mcc.configuration.clServStatus();
            if (mcc.configuration.determineClusterRunning(clRunning)) {
                // Can not allow poking HOSTS while Cluster is alive.
                console.info('[INF]' + 'Cluster is running, not refreshing host info.');
                return;
            }

            // Do we have Cluster-wide credentials to work with at all?
            var proceed = false;
            if (mcc.gui.getSSHkeybased()) {
                var a = mcc.gui.getClSSHPwd();
                var b = mcc.gui.getClSSHUser();
                var c = mcc.gui.getClSSHKeyFile();
                if (a.length > 1 || b.length > 1 || c.length > 1) {
                    proceed = true;
                }
            } else {
                var d = mcc.gui.getSSHPwd();
                var e = mcc.gui.getSSHUser();
                if (d.length > 1 || e.length > 1) {
                    proceed = true;
                }
            }
            // if it's just locahost we should proceed.
            var hostStorage = mcc.storage.hostStorage();
            hostStorage.forItems({}, function (host) {
                if (!host.getValue('anyHost')) {
                    var nm = String(host.getValue('name')).toUpperCase();
                    if (nm === 'LOCALHOST' || nm === '127.0.0.1') {
                        proceed = true;
                        // There can only be 1 localhost so forEach will die now.
                    }
                }
            });
            if (!proceed) {
                console.info('[INF]' + 'No Cluster-wide credentials, not refreshing host info.');
                if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('host')) {
                    mcc.userconfig.setOriginalStore('host');
                }
                return;
            }

            var res = [];
            console.info('[INF]Fetch host(s) resource information.');
            // Send all hostInfo requests at once.
            // Ok status = OK. Failed status = Failed, N/A. Fetching status = Fetching...
            hostStorage.forItems({}, function (host) {
                if (!host.getValue('anyHost')) {
                    console.debug('[DBG]Fetch resource information for host ' + host.getValue('name'));
                    res.push('false');
                    // Does it need (re)fetch at all?
                    if (String(host.getValue('hwResFetch')) !== 'OK') {
                        // override existing info since HWResFetch failed...
                        mcc.storage.getHostResourceInfo(host.getValue('name'), host.getId(), false, true);
                    }
                }// !AnyHost
            }); // forItems
            // Check every second that we can proceed.
            var id = setInterval(waitret, 500);
            var errCnt = 0;
            var errMsg = '';
            function waitret () {
                hostStorage.getItems({ anyHost: false }).then(function (hosts) {
                    var what2 = {};
                    for (var i in hosts) {
                        if (String(hosts[i].getValue('hwResFetch')) === 'Fetching...') {
                            res[i] = 'false';
                        } else {
                            res[i] = 'true';
                            if (String(hosts[i].getValue('hwResFetch')) === 'Failed' ||
                                String(hosts[i].getValue('hwResFetch')) === 'N/A') {
                                errCnt += 1;
                                what2 = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr(
                                    'failedHWResFetchDirect', [hosts[i].getValue('name')]));
                                if ((what2 || {}).text) {
                                    errMsg += what2.text;
                                }
                            }
                        }
                    }
                });
                if (res.indexOf('false') === -1) {
                    clearInterval(id);
                    if (errCnt > 0) {
                        // No error per se. Maybe Cluster level creds were
                        // wrong or user intends to add host level creds etc...
                        console.error('[ERR]' + errMsg.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                    }
                }
            }
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('host')) {
                mcc.userconfig.setOriginalStore('host');
            }
        },
        exit: function () {
            // Make sure all hosts have up-to-date resources fetched.
            mcc.storage.hostStorage().getItems().then(function (hosts) {
                var errMsgSummary = '';
                for (var i in hosts) {
                    if (hosts[i].getValue('name').toString() !== 'Any host') {
                        var errMsg = hosts[i].getValue('errMsg');
                        if (errMsg) {
                            // genHostErrors
                            var what2 = mcc.userconfig.setCcfgPrGen.apply(this, mcc.userconfig.setMsgForGenPr(
                                'genHostErrors', [hosts[i].getValue('name'), errMsg]));
                            if ((what2 || {}).text) {
                                errMsgSummary += what2.text;
                            }
                        }
                    } // !AnyHost
                }
                if (errMsgSummary) {
                    // not shown for this host yet.
                    errMsgSummary = 'Summary of errors in host(s) information:\n' + errMsgSummary;
                    errMsgSummary += "\n\nPress 'OK' to continue to the next page anyway, or " +
                        "'Cancel' to stay at the previous page";
                    mcc.storage.hostStorage().save();
                    if (!confirm(errMsgSummary)) {
                        cancelTabChange('configWizardDefineHosts');
                    }
                }
            });
            console.info('[INF]Exit configWizardDefineHosts');
            mcc.storage.hostStorage().save();
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('host')) {
                mcc.userconfig.setOriginalStore('host');
            } else {
                if (!mcc.userconfig.getIsNewConfig()) {
                    // compare to find differences:
                    if (mcc.userconfig.isShadowEmpty('host')) {
                        dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'red');
                    } else {
                        dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'green');
                    }
                } else {
                    dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'green');
                }
            }
        }
    },
    configWizardDefineProcesses: {
        content: "\
            <div dojoType='dijit.layout.BorderContainer' gutters='false'>\
                <div dojoType='dijit.layout.ContentPane' region='top'>\
                    <h2>Define Processes and Cluster Topology</h2>\
                    <span class='content-tab-sub-title'>Various processes may \
                            be part of a MySQL Cluster configuration. Please \
                            refer to the MySQL Cluster Documentation for a description of the \
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
                            but may execute anywhere.</br></br>\
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
                                        style='height: 28px; padding: 0; overflow: hidden;'>\
                                    <div id='hostTreeHeader'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='center'>\
                                    <div id='hostTree'></div>\
                                </div>\
                                <div dojoType='dijit.layout.ContentPane'\
                                        region='bottom' style='height: 32px; padding: 4px'>\
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
            // If there are hosts not set up completely ("hwResFetch" != "OK"),
            // this could break on, for example, converting paths or could be anywhere
            // so we warn
            var what;
            var clRun = '';
            if (mcc.configuration.determineClusterRunning(mcc.configuration.clServStatus())) {
                what = mcc.userconfig.setCcfgPrGen.apply(this,
                    mcc.userconfig.setMsgForGenPr('clRunning', ['process']));
                if ((what || {}).text) {
                    clRun = what.text;
                };
                console.warn('[WRN]Cluster is running!');
            }
            what = {};
            var badHostsExist = false;
            var alertMsgStarted = '';
            var alertMsg = '';
            if (mcc.userconfig.wasCfgStarted()) {
                what = mcc.userconfig.setCcfgPrGen.apply(this,
                    mcc.userconfig.setMsgForGenPr('clStarted', ['process']));
                if ((what || {}).text) {
                    alertMsgStarted = what.text;
                };
                console.warn('[WRN]Configuration was already started.');
            }
            mcc.storage.hostStorage().getItems().then(function (hosts) {
                console.info('[INF]Checking hosts validity.');
                var what2 = {};
                for (var i in hosts) {
                    if (hosts[i].getValue('name').toString() !== 'Any host') {
                        // This is troubling but not deal breaker.
                        if (hosts[i].getValue('hwResFetch') && String(hosts[i].getValue('hwResFetch')) !== 'OK') {
                            what2 = mcc.userconfig.setCcfgPrGen.apply(this,
                                mcc.userconfig.setMsgForGenPr('failedHWResFetch', [hosts[i].getValue('name')]));
                            if ((what2 || {}).text) {
                                alertMsg += what2.text;
                            } // we already warned for this host
                        }
                        if ((!hosts[i].getValue('datadir') || hosts[i].getValue('datadir').length < 3) ||
                            (!hosts[i].getValue('uname') || hosts[i].getValue('uname').length < 3)) {
                            badHostsExist = true;
                        }
                    }
                }
            });
            var bho = '';
            if (badHostsExist) {
                var what1 = mcc.userconfig.setCcfgPrGen.apply(this,
                    mcc.userconfig.setMsgForGenPr('badHostsExist', ['']));
                if ((what1 || {}).text) {
                    bho = what1.text;
                };
            }
            if (alertMsgStarted || alertMsg || clRun || bho) {
                // there's more than one, add H# tag
                var txt = ((clRun ? clRun + '<br/>' : '') + (alertMsg ? alertMsg + '<br/>' : '') +
                    (bho ? bho + '<br/>' : '') + (alertMsgStarted ? alertMsgStarted + '<br/>' : ''));
                mcc.util.displayModal('I', 0, txt + '<br/>', '', '');
            }
            console.info('[INF]Enter configWizardDefineProcesses');
            mcc.gui.hostTreeSetup();
            mcc.gui.hostTreeSelectionDetailsSetup();
            mcc.gui.updateHostTreeSelectionDetails();
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('process')) {
                mcc.userconfig.setOriginalStore('process');
            }
        },
        exit: function () {
            console.info('[INF]Exit configWizardDefineProcesses');
            mcc.storage.processStorage().save();
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('process')) {
                mcc.userconfig.setOriginalStore('process');
            }
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
                            to the MySQL Cluster Documentation for a description of the \
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
                            parameters based on the hardware resources and the\
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
                                        region='top' style='height: 28px; \
                                        padding:0; overflow: hidden;'>\
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
            // If there are hosts not set up completely ("hwResFetch" != "OK"), this could break on
            // , for example, converting paths or could be anywhere so we warn.
            var badHostsExist = false;
            mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
                mcc.storage.hostStorage().getItems().then(function (hosts) {
                    console.info('[INF]Checking hosts validity.');
                    var alertMsg = '';
                    var what2 = {};
                    for (var i in hosts) {
                        if (hosts[i].getValue('name').toString() !== 'Any host') {
                            // This is troubling but not deal breaker.
                            if (hosts[i].getValue('hwResFetch') && String(hosts[i].getValue('hwResFetch')) !== 'OK') {
                                what2 = mcc.userconfig.setCcfgPrGen.apply(this,
                                    mcc.userconfig.setMsgForGenPr('failedHWResFetch', [hosts[i].getValue('name')]));
                                if ((what2 || {}).text) {
                                    alertMsg += what2.text;
                                } // we already warned for this host
                            }
                        }
                    }
                    if (alertMsg.length > 0) {
                        mcc.util.displayModal('H', 0, '<span style="color:orangered;>' + alertMsg + '</span>',
                            '<span style="font-size:150%;color:orangered">Host(s) incorrectly configured ' +
                            '(failed hwResFetch):</span>');
                    }
                    for (i in hosts) {
                        if (hosts[i].getValue('name').toString() !== 'Any host') {
                            // At least datadir & uname must be there.
                            if ((!hosts[i].getValue('datadir') || hosts[i].getValue('datadir').length < 3) ||
                                (!hosts[i].getValue('uname') || hosts[i].getValue('uname').length < 3)) {
                                badHostsExist = true;
                                break;
                            }
                        }
                    }
                });
            });
            if (badHostsExist) {
                var what1 = mcc.userconfig.setCcfgPrGen(
                    'badHostsExist', '', '', '', '', true);
                if ((what1 || {}).text) {
                    console.warn(what1.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                    mcc.util.displayModal('I', 0, what1.text);
                }
            }
            // First check IF Cluster is running:
            var clRunning = [];
            clRunning = mcc.configuration.clServStatus();
            if (mcc.configuration.determineClusterRunning(clRunning)) {
                var what = mcc.userconfig.setCcfgPrGen.apply(this,
                    mcc.userconfig.setMsgForGenPr('clRunning', ['process']));
                if ((what || {}).text) {
                    console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                    mcc.util.displayModal('I', 3, what.text);
                }
                console.warn('Cluster is running!');
            }

            console.info('[INF]Enter configWizardDefineParameters');
            mcc.gui.processTreeSetup();
            mcc.gui.updateProcessTreeSelectionDetails();
            // it is possible user skipped over Processes page for old configuration and changed
            // say mysqldN.Port on Parameters page... This would leave change undetected if not
            // initialized here.
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('process')) {
                mcc.userconfig.setOriginalStore('process');
            }
            if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('processtype')) {
                mcc.userconfig.setOriginalStore('processtype');
            }
        },
        exit: function () {
            console.info('[INF]Exit configWizardDefineParameters');
            // do not really need to save ProcessTypeStore since each widget onChange triggers save?
            mcc.storage.processTypeStorage().save();
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
                            style=\"position: relative; display:\
                            inline-block; width:17px\">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>\
                            &nbsp;:&nbsp;<i>unknown</i> or \
                            if the management daemon does not reply, \
                            <img src=img/greenlight.gif></img>&nbsp;:&nbsp;\
                            <i>connected</i> or\
                            <i>started</i>, <img src=img/yellowlight.gif></img>&nbsp;:&nbsp;\
                            <i>starting</i> or\
                            <i>shutting down</i>, and <img src=img/redlight.gif></img>&nbsp;:&nbsp;\
                            <i>not connected</i> or\
                            <i>stopped</i>.</br></br> \
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
                                region='top' style='height: 28px; \
                                padding:0;overflow: hidden;'>\
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
                    <div data-dojo-type='dojox.form.BusyButton'\
                        data-dojo-props='iconClass: \"installIcon\", baseClass: \"fbtn\", busyLabel:\"Installing.....\"'\
                        onClick='mcc.configuration.installCluster()'\
                        id='configWizardInstallCluster'>Install cluster\
                    </div>\
                    <div data-dojo-type='dojox.form.BusyButton'\
                        data-dojo-props='iconClass: \"deployIcon\", baseClass: \"fbtn\", busyLabel:\"Deploying.....\"'\
                        onClick='mcc.configuration.deployCluster()'\
                        id='configWizardDeployCluster'>Deploy cluster\
                    </div>\
                    <div data-dojo-type='dojox.form.BusyButton'\
                        data-dojo-props='iconClass: \"startIcon\", baseClass: \"fbtn\", busyLabel:\"Starting.....\"'\
                        onClick='mcc.configuration.startCluster()'\
                        id='configWizardStartCluster'>Start cluster\
                    </div>\
                    <div data-dojo-type='dojox.form.BusyButton'\
                        data-dojo-props='iconClass: \"stopIcon\", baseClass: \"fbtn\", busyLabel:\"Stopping....\"'\
                        onClick='mcc.configuration.stopCluster()'\
                        id='configWizardStopCluster'>Stop cluster\
                    </div>\
                    <div data-dojo-type='dojox.form.BusyButton'\
                        data-dojo-props='iconClass: \"viewIcon\", baseClass: \"fbtn\", busyLabel:\"Saving.......\"'\
                        onClick='mcc.configuration.viewCmds()'\
                        id='configWizardViewCmds'>View commands\
                    </div>\
                </div>\
            </div>",
        enter: function () {
            // If there are hosts not set up completely ("hwResFetch" != "OK"),
            // one has no business here. Warn about page change.
            var badHostsExist = false;
            mcc.storage.clusterStorage().getItem(0).then(function (cluster) {
                mcc.storage.hostStorage().getItems().then(function (hosts) {
                    console.info('[INF]Checking hosts validity.');
                    var alertMsg = '';
                    var what2 = {};
                    for (var i in hosts) {
                        if (hosts[i].getValue('name').toString() !== 'Any host') {
                            // This is troubling but not deal breaker.
                            if (hosts[i].getValue('hwResFetch') && String(hosts[i].getValue('hwResFetch')) !== 'OK') {
                                what2 = mcc.userconfig.setCcfgPrGen.apply(this,
                                    mcc.userconfig.setMsgForGenPr('failedHWResFetch', [hosts[i].getValue('name')]));
                                if ((what2 || {}).text) {
                                    alertMsg += what2.text;
                                } // we already warned for this host
                            }
                        }
                    }
                    if (alertMsg.length > 0) {
                        // no buttons, header
                        mcc.util.displayModal('H', 0, '<span style="color:orangered;>' + alertMsg + '</span>',
                            '<span style="font-size:150%;color:orangered">Host(s) incorrectly configured ' +
                            '(failed hwResFetch):</span>', '');
                    }
                    for (i in hosts) {
                        if (hosts[i].getValue('name').toString() !== 'Any host') {
                            // At least datadir & uname must be there.
                            if ((!hosts[i].getValue('datadir') || hosts[i].getValue('datadir').length < 3) ||
                                (!hosts[i].getValue('uname') || hosts[i].getValue('uname').length < 3)) {
                                badHostsExist = true;
                                break;
                            }
                        }
                    }
                });
            });
            if (badHostsExist) {
                var what1 = mcc.userconfig.setCcfgPrGen(
                    'badHostsExist', '', '', '', '', true);
                if ((what1 || {}).text) {
                    console.warn(what1.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                    mcc.util.displayModal('I', 0, what1.text);
                }
            }
            console.info('[INF]Enter configWizardDeployConfig');
            mcc.configuration.setupContext().then(function () {
                mcc.gui.deploymentTreeSetup();
                // mcc.gui.setPollTimeOut(2000); //There is default timeout variable...
                mcc.gui.startStatusPoll(true);
                mcc.configuration.setupClusterTools();

                // it's not until setupContext recalculates values that original stores reflect new values
                if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('process')) {
                    mcc.userconfig.setOriginalStore('process');
                }
                if (mcc.userconfig.getIsNewConfig() || mcc.userconfig.isShadowEmpty('processtype')) {
                    mcc.userconfig.setOriginalStore('processtype');
                }
            });
        },
        exit: function () {
            console.info('[INF]Exit configWizardDeployConfig');
            // Signal back end to clear MGMTConn array: mcc.gui.stopStatusPoll("STOP");
            // Ordinary stop: mcc.gui.stopStatusPoll();
        }
    }
};

/****************************** Implementation  *******************************/
// Show next page
function nextPage () {
    var current = dojo.indexOf(configWizardPageIds,
        dijit.byId('content-main-tab-container').selectedChildWidget.id);
    if (current.toString() !== 'configWizardDefineCluster') {
        // Ignore changes to Cluster lvl.
        var cs = mcc.storage.clusterStorage();
        var hs = mcc.storage.hostStorage();
        var ps = mcc.storage.processStorage();
        var pts = mcc.storage.processTypeStorage();
        var toWrite = '';
        // _getNewFileContentString generates STRING. I'm assuming it's flat JSON.
        cs.getItem(0).then(function (cluster) {
            // Remove passwords before writing configuration to file.
            var tmpcs = cs.store()._getNewFileContentString();
            var tmphs = hs.store()._getNewFileContentString();
            var tmpcsJSON = JSON.parse(tmpcs);
            var tmphsJSON = JSON.parse(tmphs);
            if (tmpcsJSON.items[0].ssh_pwd && tmpcsJSON.items[0].ssh_pwd.length !== 0) {
                // replace pwd
                tmpcsJSON.items[0].ssh_pwd = '-';
            }
            if (tmpcsJSON.items[0].ssh_ClKeyPass &&
                tmpcsJSON.items[0].ssh_ClKeyPass.length !== 0) {
                // replace pwd
                tmpcsJSON.items[0].ssh_ClKeyPass = '-';
            }
            tmpcs = JSON.stringify(tmpcsJSON, null, '\t'); // To go instead contents of ClusterStore (cs).
            for (var i = 0; i < tmphsJSON.items.length; i++) {
                if (tmphsJSON.items[i].name.toString() !== 'Any host') {
                    if (tmphsJSON.items[i].usrpwd && tmphsJSON.items[i].usrpwd.length !== 0) {
                        tmphsJSON.items[i].usrpwd = '-';
                    }
                    if (tmphsJSON.items[i].key_passp && tmphsJSON.items[i].key_passp.length !== 0) {
                        tmphsJSON.items[i].key_passp = '-';
                    }
                }
            }
            tmphs = JSON.stringify(tmphsJSON, null, '\t'); // To go instead contents of HostStore (hs).
            toWrite = tmpcs + ', ' + tmphs + ', {}, ' +
                ps.store()._getNewFileContentString() + ', ' + pts.store()._getNewFileContentString();
        });
        console.debug('[DBG]Executing cfg save.');
        mcc.userconfig.writeConfigFile(toWrite, mcc.userconfig.getConfigFile());
    }
    dijit.byId('content-main-tab-container').selectChild(dijit.byId(configWizardPageIds[current + 1]));
}

// Show previous page
function prevPage () {
    var current = dojo.indexOf(configWizardPageIds, dijit.byId('content-main-tab-container').selectedChildWidget.id);
    dijit.byId('content-main-tab-container').selectChild(dijit.byId(configWizardPageIds[current - 1]));
}

// Show last page
function lastPage () {
    dijit.byId('content-main-tab-container').selectChild(dijit.byId(configWizardPageIds[configWizardPageIds.length - 1]));
}

// Reload current page
function reloadPage () {
    var wiz = dijit.byId('content-main-tab-container');
    var current = wiz.selectedChildWidget;

    if (current) {
        console.debug('[DBG]Reload page ' + current.id);

        // Keep toplevel child
        for (var i in current.getChildren()) {
            console.debug('[DBG]Destroy ' + current.getChildren()[i].id);
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

/**
 *Setup buttons for previous/next page.
 *
 * @param {Number} page index of page
 */
function configWizardEnableButtons (page) {
    if (page.isFirstChild) {
        dijit.byId('configWizardPrevPage').setAttribute('disabled', true);
    } else {
        dijit.byId('configWizardPrevPage').setAttribute('disabled', false);
    }

    if (page.isLastChild) {
        dijit.byId('configWizardNextPage').setAttribute('disabled', true);
        dijit.byId('configWizardLastPage').setAttribute('disabled', true);
    } else {
        dijit.byId('configWizardNextPage').setAttribute('disabled', false);
        dijit.byId('configWizardLastPage').setAttribute('disabled', false);
    }
}

// Setup help menu for the configuration wizard
function helpMenuSetup () {

    var menu = new dijit.DropDownMenu({ style: 'display: none;' });
    var menuItemContents = new dijit.MenuItem({
        label: 'Contents',
        onClick: function () {
            window.open('hlp/html/help.html',
                'MySQL Cluster Configuration',
                'scrollbars=1, \
                width=1170, \
                height=750, \
                screenX=20, \
                screenY=100, \
                left=20, \
                top=100');
        }
    });
    menu.addChild(menuItemContents);

    var menuItemContext = new dijit.MenuItem({
        label: 'Current page',
        onClick: function () {
            var wiz = dijit.byId('content-main-tab-container');
            var current = wiz.selectedChildWidget;
            var helpLabel = '';

            if (current) {
                helpLabel = '#' + current.id;
            }
            window.open('hlp/html/help_cnt.html' + helpLabel,
                'MySQL Cluster Configuration',
                'scrollbars=1, \
                width=1170, \
                height=750, \
                screenX=20, \
                screenY=100, \
                left=20, \
                top=100');
        }
    });
    menu.addChild(menuItemContext);

    var menuItemAbout = new dijit.MenuItem({
        label: 'About',
        onClick: function () {
            if (!dijit.byId('aboutDialog')) {
                var dlg = new dijit.Dialog({
                    id: 'aboutDialog',
                    title: 'About MySQL Cluster Configuration Tool',
                    content: "\
                        <div><img src='img/content-title.png' </div>\
                        <p id='mcv'></p>\
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
            var version = 'Version: mysql-cluster-8.0';
            if (mcc.util.getClusterUrlRoot().indexOf('57') >= 0) {
                version = 'Version: mysql-5.7-cluster7.6';
            }
            dojo.byId('mcv').innerHTML = version;

            dijit.byId('aboutDialog').show();
        }
    });
    menu.addChild(menuItemAbout);

    var menuButton = new dijit.form.DropDownButton({
        label: 'Help',
        dropDown: menu,
        baseClass: 'menuButton',
        showLabel: true,
        style: 'float: right; ',
        id: 'configWizardHelpButton'
    });
    dojo.byId('content-main-tab-container_tablist').appendChild(menuButton.domNode);
}

// Setup settings menu for the configuration wizard
function configWizardMenuSetup () {
    var menu = new dijit.DropDownMenu({ style: 'display: none;' });
    var menuButton = new dijit.form.DropDownButton({
        label: 'Settings',
        baseClass: 'menuButton',
        dropDown: menu,
        showLabel: true,
        style: 'float: right; display: none;', // Do not show old settings any more.
        id: 'configWizardMenuButton'
    });
    dojo.byId('content-main-tab-container_tablist').appendChild(menuButton.domNode);
}

// Setup a timer to stay at old child
function cancelTabChange (stayAt) {
    setTimeout('dijit.byId("content-main-tab-container").selectChild(dijit.byId("' + stayAt + '"))', 5);
}

// Select and enter first page
function enterFirst () {
    var firstPage = dijit.byId(configWizardPageIds[0]);

    dijit.byId('content-main-tab-container').selectChild(firstPage);
    firstPage.setContent(configWizardPages[firstPage.id].content);
    configWizardPages[firstPage.id].enter();
    mcc.configuration.sendSSHCleanupReq();
    mcc.gui.stopStatusPoll('STOP');
    // In case we're attaching to already running cluster we're out of luck :-/
    // Enable buttons as appropriate
    configWizardEnableButtons(firstPage);
    dojo.style(dijit.byId('fbtnGetClProblems').domNode, 'background', 'green');
    dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'green');
    dojo.style(dijit.byId('fbtnGetPrProblems').domNode, 'background', 'green');
    dojo.style(dijit.byId('fbtnGetGeProblems').domNode, 'background', 'blue');
}
/**
 *Displays all significant changes of parameters in ClusterStore since last sync.
 *
 */
function getClCfgProblems () {
    var reDeploy = '';
    var msg = '';
    if (!mcc.userconfig.getIsNewConfig()) {
        // compare to find differences:
        reDeploy = mcc.userconfig.getConfigProblems('cluster');
        if (!(((((reDeploy || {}).body || {}).error || {}).items || []).length)) {
            // empty
            dojo.style(dijit.byId('fbtnGetClProblems').domNode, 'background', 'green');
        } else {
            if (reDeploy.body.error.items) {
                for (var i in reDeploy.body.error.items) {
                    msg += reDeploy.body.error.items[i].header + reDeploy.body.error.items[i].text;
                }
                dojo.style(dijit.byId('fbtnGetClProblems').domNode, 'background', 'red');
                mcc.util.displayModal('H', 0, msg, '<span style="font-size:150%;' +
                    'color:red">Significant changes in configuration for Cluster:</span>');
            }
        }
    }
    if (!msg) {
        dojo.style(dijit.byId('fbtnGetClProblems').domNode, 'background', 'green');
    }
}
/**
 *Displays all significant changes of parameters in HostStore since last sync.
 *
 */
function getHoCfgProblems () {
    var reDeploy = '';
    var msg = '';
    if (!mcc.userconfig.getIsNewConfig()) {
        reDeploy = mcc.userconfig.getConfigProblems('host');
        if (!(((((reDeploy || {}).body || {}).error || {}).items || []).length)) {
            // empty
            dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'green');
        } else {
            if (reDeploy.body.error.items) {
                for (var i in reDeploy.body.error.items) {
                    msg += reDeploy.body.error.items[i].header + reDeploy.body.error.items[i].text;
                }
                // no buttons, header
                mcc.util.displayModal('H', 0, msg, '<span style="font-size:150%;' +
                    'color:red">Significant changes in configuration for Hosts:</span>');
                dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'red');
            }
        }
    }
    if (!msg) {
        dojo.style(dijit.byId('fbtnGetHoProblems').domNode, 'background', 'green');
    }
}
/**
 *Displays all significant changes of parameters in Process and ProcessTypeStore since last sync.
 *
 */
function getPrCfgProblems () {
    var reDeploy = {};
    var reDeploy1 = {};
    var msg = '';
    var i = -1;
    if (!mcc.userconfig.getIsNewConfig()) {
        // compare to find differences for Process and ProcessType stores. Alert just once.
        reDeploy1 = mcc.userconfig.getConfigProblems('processtype');
        reDeploy = mcc.userconfig.getConfigProblems('process');

        if ((((((reDeploy || {}).body || {}).error || {}).items || []).length) ||
            (((((reDeploy1 || {}).body || {}).error || {}).items || []).length)) {
            // families first
            if ((((((reDeploy1 || {}).body || {}).error || {}).items || []).length)) {
                for (i in reDeploy1.body.error.items) {
                    msg += reDeploy1.body.error.items[i].header + reDeploy1.body.error.items[i].text;
                }
            }
            if (msg) { msg += '<br/><br/>' }; // add some spacing
            if ((((((reDeploy || {}).body || {}).error || {}).items || []).length)) {
                for (i in reDeploy.body.error.items) {
                    msg += reDeploy.body.error.items[i].header + reDeploy.body.error.items[i].text;
                }
            }
            dojo.style(dijit.byId('fbtnGetPrProblems').domNode, 'background', 'red');
            // no buttons, header
            mcc.util.displayModal('H', 0, msg, '<span style="font-size:150%;' +
                'color:red">Significant changes in configuration for Processes and families:</span>');
        }
    }
    if (!msg) {
        dojo.style(dijit.byId('fbtnGetPrProblems').domNode, 'background', 'green');
    }
}
/**
 *Displays all significant changes of parameters in Stores, warnings we have shown and so on.
 *
 */
function getGeCfgProblems () {
    dojo.style(dijit.byId('fbtnGetGeProblems').domNode, 'background', 'blue');
    var rep = mcc.userconfig.getConfigProblems('general');
    if (!(((rep || {}).items || []).length)) {
        // empty
        console.info('General log is empty.');
    } else {
        // show *plain* text, only config change has .count
        var msg = '';
        var s, s1;
        for (var i in rep.items) {
            // display all regardless of Display field status
            s1 = (rep.items[i].key ? '[' + rep.items[i].key + ']:' : '::').replace(/<(?:.|\n)*?>/gm, '');
            msg += (s || '') + rep.items[i].name.replace(/<(?:.|\n)*?>/gm, '') + s1 +
                rep.items[i].text.replace(/<(?:.|\n)*?>/gm, '') +
                '    [' + (rep.items[i].count || '') + ']<br/>';
        }
        mcc.util.displayModal('H', 0, msg, '<span style="font-size:150%;' +
            'color:red">Messages collected so far:</span>');
    }
}

// Initialize the config wizard
function initialize () {
    // Setup the menus
    helpMenuSetup();
    configWizardMenuSetup();

    // Install child selection monitor: Check validity, call exit/enter
    dijit.byId('content-main-tab-container').watch('selectedChildWidget', function (name, prev, next) {
        // Log transition
        console.debug('[DBG]Wizard page: ' + prev.id + ' -> ' + next.id);
        configWizardEnableButtons(next);
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
    });
}

/******************************** Initialize  *********************************/
dojo.ready(function () {
    initialize();
    console.info('[INF]Configuration wizard module initialized');
});
