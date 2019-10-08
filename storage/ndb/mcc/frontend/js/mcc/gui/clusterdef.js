/*
Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

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
 ***                             Cluster definition                         ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.gui.clusterdef
 *
 *  Description:
 *      Define cluster properties
 *
 *  External interface:
 *      mcc.gui.clusterdef.showClusterDefinition: Show/edit stored information
 *      mcc.gui.clusterdef.saveClusterDefinition: Save entered information
 *      mcc.gui.clusterdef.getSSHPwd: Get password for ssh
 *      mcc.gui.clusterdef.getSSHUser: Get ClusterLvL user from variable rather than iterate storage.
 *      mcc.gui.clusterdef.getSSHkeybased: Get ClusterLvL key-based auth from variable rather than iterate storage.
 *      mcc.gui.clusterdef.getOpenFW: Same as above.
 *      mcc.gui.clusterdef.getInstallCl: Same as above.
 *      mcc.gui.clusterdef.getUseVPN: Do we use VPN at all? Important when forming
 *          connect string.
 *      mcc.gui.clusterdef.setLocalHome: Find out HOME for user running back end.
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *     clusterDefinitionSetup: Create all necessary widgets
 *
 *  Internal data:
 *      ssh_pwd: Password for ssh
 *
 *  Unit test interface:
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *
 ******************************************************************************/

/***************************** Import/export **********************************/
dojo.provide('mcc.gui.clusterdef');

dojo.require('dojox.grid.DataGrid');
dojo.require('dijit.form.CheckBox');
dojo.require('dijit.form.TextBox');
dojo.require('dijit.form.FilteringSelect');
dojo.require('dijit.form.NumberSpinner');
dojo.require('dijit.Tooltip');

dojo.require('mcc.util');
dojo.require('mcc.storage');

dojo.require('dojox.form.BusyButton');
dojo.require('dijit.form.Button');

/*************************** External interface  ******************************/
mcc.gui.clusterdef.showClusterDefinition = showClusterDefinition;
mcc.gui.clusterdef.saveClusterDefinition = saveClusterDefinition;
mcc.gui.clusterdef.getSSHPwd = getSSHPwd;
mcc.gui.clusterdef.getSSHUser = getSSHUser;
mcc.gui.clusterdef.getSSHkeybased = getSSHkeybased;
mcc.gui.clusterdef.getOpenFW = getOpenFW;
mcc.gui.clusterdef.getInstallCl = getInstallCl;
mcc.gui.clusterdef.getUseVPN = getUseVPN;
mcc.gui.clusterdef.setLocalHome = setLocalHome;
// New SSH stuff
mcc.gui.clusterdef.getClSSHPwd = getClSSHPwd;
mcc.gui.clusterdef.getClSSHUser = getClSSHUser;
mcc.gui.clusterdef.getClSSHKeyFile = getClSSHKeyFile;

/******************************* Internal data ********************************/
var sshPwd = '';
var sshUser = '';
var sshKeybased = false;
var openFW = false;
var installCl = 'NONE';
var useVPN = false;
var sshClKeyUser = '';
var sshClKeyPass = '';
var sshClKeyFile = '';
// var noAutoSetVPN = false;
var localHome = '';
/****************************** Implementation  *******************************/
/**
 *Provides ClusterLevel user password.
 *
 * @returns {string} ClusterLevel user password.
 */
function getSSHPwd () {
    return sshPwd;
}

/**
 *Provides ClusterLvL user name.
 *
 * @returns {string} ClusterLevel user name.
 * In back end, this will default to local user running BE process.
 */
function getSSHUser () {
    return sshUser;
}

/**
 *Do we use SSH keys for authentication to hosts (ClusterLvL).
 *
 * @returns {boolean}
 */
function getSSHkeybased () {
    return sshKeybased;
}
/**
 *Will we use HostStorage.InternalIP member to form connection srtings.
 *
 * @returns {boolean}
 */
function getUseVPN () {
    return useVPN;
}

/**
 *Provides ClusterLvL OpenFW ports settings.
 *
 * @returns {boolean}
 */
function getOpenFW () {
    return openFW;
}

/**
 *Provides ClusterLvL "Install Cluster" setting.
 *
 * @returns {string} NONE, BOTH, REPO or DOCKER
 */
function getInstallCl () {
    return installCl;
}

/**
 *Provides ClusterLvL SSHPwd when getSSHKeybased (sshKeybased var) is true
 *
 * @returns {string} passphrase for encrypted SSH key
 */
function getClSSHPwd () {
    return sshClKeyPass;
}

/**
 *Provides ClusterLvL SSHUser when getSSHKeybased (sshKeybased var) is true
 * In back end, defaults to user running MCC process.
 *
 * @returns {string} SSH key user
 */
function getClSSHUser () {
    return sshClKeyUser;
}

/**
 *Provides ClusterLvL SSH key file when getSSHKeybased (sshKeybased var) is true
 * In back end, defaults to ~/.ssh/id_rsa.pub.
 *
 * @returns {string} full path to and name of public SSH key to use
 */
function getClSSHKeyFile () {
    return sshClKeyFile;
}

function setLocalHome (inp) {
    localHome = inp;
    console.debug('[DBG]' + 'Local HOME is ' + localHome);
}

/**
 *Create tooltips for DOM nodes.
 *
 * @param {[String]} cId DOM nodes tooltip connects to.
 * @param {String} lbl text to show.
 * @returns fake
 */
function createTT (cId, lbl) {
    return new dijit.Tooltip({
        connectId: cId,
        label: lbl,
        destroyOnHide: true
    });
}

// Show/hide password/passphrase.
function togglePasswordFieldClicked () {
    var passwordField = document.getElementById('sd_pwd');
    var value = '';
    if (passwordField.value) {
        value = passwordField.value;
    }
    if (passwordField.type === 'password') {
        passwordField.type = 'text';
    } else {
        passwordField.type = 'password';
    }
    passwordField.value = value;
}

function togglePassphraseFieldClicked () {
    var passwordField = document.getElementById('sd_ClkeyPass');
    var value = '';
    if (passwordField.value) {
        value = passwordField.value;
    }
    if (passwordField.type === 'password') {
        passwordField.type = 'text';
    } else {
        passwordField.type = 'password';
    }
    passwordField.value = value;
}
/**
 *Saves data from widgets into the cluster data object, arranges elements depending on selection.
 *
 * @param {String} callerName    'hidden' parameter, the name of widget that triggered the call
 * @returns Nothing, just pops up alerts for dangerous settings
 */
function saveClusterDefinition (callerName) {
    // Get hold of storage objects
    if (!dijit.byId('cd_hosts')) {
        // we're in trouble, add to general problems list
        console.warn('Page is not properly initialized!');
        return;
    }
    var clusterStorage = mcc.storage.clusterStorage();
    var hostStorage = mcc.storage.hostStorage();
    var clRunning = [];
    // storage initializes config module, still maybe a good idea to do TRY here?.
    clRunning = mcc.configuration.clServStatus();
    if (!mcc.configuration.determineClusterRunning(clRunning)) {
        console.info('[INF]Saving Cluster definition.');
        // makes no sense to have ssh_keybased selected for localhost
        if (callerName === 'cbKeybased') {
            if (dijit.byId('cbKeybased').get('checked')) {
                if (dijit.byId('cd_hosts').getValue().indexOf('localhost') >= 0 ||
                    dijit.byId('cd_hosts').getValue().indexOf('127.0.0.1') >= 0) {
                    console.debug('[DBG]Reverting SSH_keybased to unchecked state.');
                    dijit.byId('cbKeybased').setValue(false);
                }
            }
        }
        // Get the (one and only) cluster item and update it
        clusterStorage.getItem(0).then(function (cluster) {
            // If toggling the keybased ssh on, reset and disable user/pwd
            if (dijit.byId('cbKeybased').get('checked')) {
                dijit.byId('sd_user').set('disabled', true);
                dijit.byId('sd_user').set('value', '');
                document.getElementById('sd_pwd').disabled = true;
                document.getElementById('sd_pwd').value = '';
                document.getElementById('togglePasswordField').disabled = true;

                dijit.byId('sd_ClkeyUser').set('disabled', false);
                document.getElementById('sd_ClkeyPass').disabled = false;
                document.getElementById('togglePassphraseField').disabled = false;
                if (document.getElementById('cd_hosts').value === '') {
                    document.getElementById('cd_hosts').focus();
                } else {
                    // 1st, it could be we just entered something in cd_hosts but config is young
                    if (dijit.byId('cd_clver').textbox.value === '') {
                        document.getElementById('cd_clver').focus();
                    } else {
                        if (dijit.byId('cbKeybased').get('checked')) {
                            if (dijit.byId('sd_ClkeyUser').getValue() === '') {
                                document.getElementById('sd_ClkeyUser').focus();
                            } else {
                                document.getElementById('sd_ClkeyPass').focus();
                            }
                        }
                    }
                }
                dijit.byId('sd_ClkeyFile').set('disabled', false);
            } else {
                dijit.byId('sd_user').set('disabled', false);
                document.getElementById('sd_pwd').disabled = false;
                document.getElementById('togglePasswordField').disabled = false;
                if (document.getElementById('cd_hosts').value === '') {
                    document.getElementById('cd_hosts').focus();
                } else {
                    // 1st, it could be we just entered something in cd_hosts but config is young
                    if (dijit.byId('cd_clver').textbox.value === '') {
                        document.getElementById('cd_clver').focus();
                    } else {
                        if (dijit.byId('sd_user').getValue() === '' &&
                            document.getElementById('sd_pwd').value === '') {
                            document.getElementById('cbKeybased').focus();
                        } else {
                            if (dijit.byId('sd_user').getValue() === '') {
                                document.getElementById('sd_user').focus();
                            } else {
                                document.getElementById('sd_pwd').focus();
                            }
                        }
                    }
                }
                dijit.byId('sd_ClkeyUser').set('disabled', true);
                dijit.byId('sd_ClkeyUser').set('value', '');
                document.getElementById('sd_ClkeyPass').disabled = true;
                document.getElementById('sd_ClkeyPass').value = '';
                document.getElementById('togglePassphraseField').disabled = true;

                dijit.byId('sd_ClkeyFile').set('disabled', true);
                dijit.byId('sd_ClkeyFile').set('value', '');
            }

            if (cluster.getValue('ssh_user') !== dijit.byId('sd_user').getValue() ||
                cluster.getValue('ssh_keybased') !== dijit.byId('cbKeybased').get('checked') ||
                cluster.getValue('ssh_pwd') !== document.getElementById('sd_pwd').value ||
                cluster.getValue('ssh_ClKeyUser') !== dijit.byId('sd_ClkeyUser').getValue() ||
                cluster.getValue('ssh_ClKeyPass') !== document.getElementById('sd_ClkeyPass').value ||
                cluster.getValue('ssh_ClKeyFile') !== dijit.byId('sd_ClkeyFile').getValue() ||
                cluster.getValue('installCluster') !== dijit.byId('sd_installCluster').textbox.value ||
                cluster.getValue('openfw') !== dijit.byId('cbOpenfw').get('checked') ||
                cluster.getValue('usevpn') !== dijit.byId('cbUsevpn').get('checked')) {
                // Store SSH details
                cluster.setValue('ssh_keybased', dijit.byId('cbKeybased').get('checked'));
                cluster.setValue('ssh_user', dijit.byId('sd_user').getValue());

                // Preserve placeholder.
                if (cluster.getValue('ssh_pwd') !== '-' && document.getElementById('sd_pwd').value !== '') {
                    cluster.setValue('ssh_pwd', document.getElementById('sd_pwd').value);
                } else {
                    cluster.setValue('ssh_pwd', '');
                    document.getElementById('sd_pwd').value = '';
                }

                // Check openFW/install actually changed, change old hosts, if any, accordingly.
                var changeFWHostLevel = cluster.getValue('openfw') !== dijit.byId('cbOpenfw').get('checked');
                var changeInstallHostLevel = cluster.getValue('installCluster') !==
                    dijit.byId('sd_installCluster').textbox.value;
                if (changeFWHostLevel || changeInstallHostLevel) {
                    hostStorage.getItems({ anyHost: false }).then(function (hosts) {
                        // Are there any hosts in storage or this is new configuration?
                        for (var i in hosts) {
                            if (changeFWHostLevel) {
                                console.debug('[DBG]Changing OpenFW for host %s to %s',
                                    hosts[i].getValue('name'), dijit.byId('cbOpenfw').get('checked'));
                                hosts[i].setValue('openfwhost', dijit.byId('cbOpenfw').get('checked'));
                            }
                            if (changeInstallHostLevel) {
                                console.debug('[DBG]Changing Install for host %s to %s',
                                    hosts[i].getValue('name'), dijit.byId('sd_installCluster').textbox.value);
                                hosts[i].setValue('installonhost', dijit.byId('sd_installCluster').textbox.value);
                            }
                        }
                        hostStorage.save();
                    });
                }
                cluster.setValue('ssh_ClKeyUser', dijit.byId('sd_ClkeyUser').getValue());

                // Preserve placeholder.
                if (cluster.getValue('ssh_ClKeyPass') !== '-' && document.getElementById('sd_ClkeyPass').value !== '') {
                    cluster.setValue('ssh_ClKeyPass', document.getElementById('sd_ClkeyPass').value);
                } else {
                    cluster.setValue('ssh_ClKeyPass', '');
                }
                cluster.setValue('ssh_ClKeyFile', dijit.byId('sd_ClkeyFile').getValue());

                sshPwd = document.getElementById('sd_pwd').value;
                sshUser = dijit.byId('sd_user').getValue();
                sshKeybased = dijit.byId('cbKeybased').get('checked');

                sshClKeyUser = dijit.byId('sd_ClkeyUser').getValue();
                sshClKeyPass = document.getElementById('sd_ClkeyPass').value;
                sshClKeyFile = dijit.byId('sd_ClkeyFile').getValue();
            }
            // Cluster details
            // can not check if install and openfw make sense for platform since it might be
            // resource info is not yet collected (i.e. new configuration).
            cluster.setValue('installCluster', dijit.byId('sd_installCluster').textbox.value);
            cluster.setValue('openfw', dijit.byId('cbOpenfw').get('checked'));
            // UseVPN with localhost makes no sense.
            if (dijit.byId('cd_hosts').getValue().indexOf('localhost') >= 0 ||
                dijit.byId('cd_hosts').getValue().indexOf('127.0.0.1') >= 0) {
                console.debug('[DBG]Reverting UseVPN to unchecked state.');
                dijit.byId('cbUsevpn').setValue(false);
            }
            cluster.setValue('usevpn', dijit.byId('cbUsevpn').get('checked'));
            // Signal SaveConfig procedure NOT to store passwords into configuration file.
            cluster.setValue('savepwds', false);
            // Update global vars for later.
            openFW = dijit.byId('cbOpenfw').get('checked');
            useVPN = dijit.byId('cbUsevpn').get('checked');
            installCl = dijit.byId('sd_installCluster').textbox.value;
            // Disallow for already started Cluster as it changes FragmentLogFileSize.
            if (cluster.getValue('apparea') !== dijit.byId('cd_apparea').getValue() &&
                dijit.byId('cd_apparea').getValue() !== '') {
                if (!cluster.getValue('Started')) {
                    // Warn if web app or realtime
                    mcc.userconfig.setCfgStarted(false);
                    if (cluster.getValue('apparea') === 'simple testing' &&
                        dijit.byId('cd_apparea').getValue() !== 'simple testing') {
                        mcc.util.displayModal('I', 0, '<span style="font-size:135%;color:orangered;">' +
                            'Please note that with this application area, the ' +
                            'configuration tool will set parameter values ' +
                            'assuming that the hosts are used for running ' +
                            'MySQL Cluster exclusively. The cluster will need ' +
                            'much time for initialization, maybe in the order of ' +
                            'one hour or more, and will consume most of the hosts ' +
                            'RAM, hence leaving the hosts unusable for other applications.</span>');
                    }
                    cluster.setValue('apparea', dijit.byId('cd_apparea').getValue());
                } else {
                    mcc.userconfig.setCfgStarted(true);
                    if (confirm('Changing appArea on already started Cluster will make it not ' +
                    'startable again!\nYou will have to DELETE all the data and files used in previous ' +
                    'deployment and redeploy.\n\nDo you still wish to make this change?')) {
                        cluster.setValue('apparea', dijit.byId('cd_apparea').getValue());
                        mcc.util.displayModal('I', 3, '<span style="font-size:135%;color:orangered;">' +
                            'Please remember to delete all old data files and redeploy!</span>');
                    } else {
                        dijit.byId('cd_apparea').setValue(cluster.getValue('apparea'));
                    }
                }
            }
            cluster.setValue('writeload', dijit.byId('cd_writeload').getValue());
            cluster.setValue('ClusterVersion', dijit.byId('cd_clver').getValue());
            mcc.util.setClusterVersion(dijit.byId('cd_clver').getValue());
            clusterStorage.save();
        });

        // Make array of host list
        var newHosts = dijit.byId('cd_hosts').getValue().split(',');
        console.debug('[DBG]newhosts is ' + JSON.stringify(newHosts));
        // Exclude localhost AND 127.0.0.1
        if (dijit.byId('cd_hosts').getValue().indexOf('localhost') >= 0 &&
            dijit.byId('cd_hosts').getValue().indexOf('127.0.0.1') >= 0) {
            mcc.util.displayModal('I', 0, 'localhost is already in the list!');
            return;
        }
        // Do check:
        var notProperIP = 0;
        var properIP = 0;
        for (var i in newHosts) {
            if ((newHosts[i].trim()).length > 0) {
                if (newHosts[i].trim().toLowerCase() === 'localhost' ||
                    newHosts[i].trim() === '127.0.0.1') {
                    notProperIP += 1;
                } else {
                    properIP += 1;
                }
            }
        }
        if ((notProperIP > 1) || (notProperIP > 0 && properIP > 0)) {
            mcc.util.displayModal('I', 0, 'Mixing localhost with remote hosts is not allowed! Please change ' +
                'localhost/127.0.0.1 to a proper IP address if you want to use your box.');
            document.getElementById('cd_hosts').focus();
            return;
        }

        // Strip leading/trailing spaces
        for (i in newHosts) {
            newHosts[i] = newHosts[i].replace(/^\s*/, '').replace(/\s*$/, '');
            console.debug('[DBG]newhosts[%i] is %s', i, newHosts[i]);
            if (newHosts[i].length > 0) {
                if (dojo.indexOf(newHosts, newHosts[i]) !== parseInt(i)) {
                    mcc.util.displayModal('I', 0, 'Hostname ' + newHosts[i] + ' is entered more than once');
                    return;
                }
            }
        }

        // Update hostStore based on the host list
        hostStorage.getItems({ anyHost: false }).then(function (hosts) {
            var oldHosts = [];
            // Build list of old hosts, check removal, but keep wildcard host
            for (var i in hosts) {
                oldHosts[i] = hosts[i].getValue('name');
                if (dojo.indexOf(newHosts, oldHosts[i]) === -1) {
                    hostStorage.deleteItem(hosts[i], true);
                }
            }
            // Add new hosts
            for (i in newHosts) {
                if (newHosts[i].length > 0 && dojo.indexOf(oldHosts, newHosts[i]) === -1) {
                    // Properly init new host with CLLvl OpenFW and INSTALL values.
                    // Failure to do so will require refetch of host info on HOSTS page
                    // to fetch, say, installation info.
                    hostStorage.newItem({
                        name: newHosts[i],
                        anyHost: false,
                        openfwhost: getOpenFW(),
                        installonhost: getInstallCl() !== 'NONE',
                        SWInstalled: false
                        // If I do not initialize it here, Grid showing HostInfo breaks.
                        // If I initialize it here, I do not know if value comes from user on Host
                        // or Cluster level.
                    }, false, false);
                }
            }
            hostStorage.save();
        });
    } else {
        var what = mcc.userconfig.setCcfgPrGen.apply(this,
            mcc.userconfig.setMsgForGenPr('clRunning', ['cluster']));
        if ((what || {}).text) {
            console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
            // Can not allow poking around parameters while Cluster is alive.
            mcc.util.log.displayModal('I', 3, what.text);
        } else {
            console.warn('Cluster is running!');
        }
    }
}
/**
 *Select cluster object from store, fill in values into the widgets
 *
 * @param {boolean} initialize    is this the first run after starting MCC or we browsed
 * back here
 */
function showClusterDefinition (initialize) {
    // Setup if required
    if (!dijit.byId('clusterDetailsHeader')) {
        clusterDefinitionSetup();
    }
    // Get hold of storage objects
    var clusterStorage = mcc.storage.clusterStorage();
    var hostStorage = mcc.storage.hostStorage();
    //* *//
    clusterStorage.getItem(0).then(function (cluster) {
        console.info('[INF]Looking for CLLvL pwds to add to reminder.');
        // Look for "-"
        if (cluster.getValue('ssh_pwd') && cluster.getValue('ssh_pwd').length === 1) {
            document.getElementById('labelforpwd').style.color = 'red';
            document.getElementById('sd_pwd').placeHolder = 'Password missing here';
            document.getElementById('sd_pwd').focus();
        }
        if (cluster.getValue('ssh_ClKeyPass') && cluster.getValue('ssh_ClKeyPass').length === 1) {
            document.getElementById('labelforpp').style.color = 'red';
            document.getElementById('sd_ClkeyPass').placeHolder = 'Password missing here';
            document.getElementById('sd_ClkeyPass').focus();
        }
    });
    var clRunning = [];
    // storage initializes config module, still maybe a good idea to do TRY here?.
    clRunning = mcc.configuration.clServStatus();
    if (mcc.configuration.determineClusterRunning(clRunning)) {
        // It is running so disable all input components:
        console.debug('[DBG]DefineCluster, disabling input elements.');
        dijit.byId('cbKeybased').set('disabled', true);
        dijit.byId('sd_user').set('disabled', true);
        document.getElementById('sd_pwd').disabled = true;
        document.getElementById('togglePasswordField').disabled = true;
        dijit.byId('sd_ClkeyUser').set('disabled', true);
        document.getElementById('sd_ClkeyPass').disabled = true;
        document.getElementById('togglePassphraseField').disabled = true;
        dijit.byId('sd_ClkeyFile').set('disabled', true);
        dijit.byId('cd_name').set('disabled', true);
        dijit.byId('cd_apparea').set('disabled', true);
        dijit.byId('cd_writeload').set('disabled', true);
        dijit.byId('cd_clver').set('disabled', true);
        dijit.byId('sd_installCluster').set('disabled', true);
        dijit.byId('cbOpenfw').set('disabled', true);
        dijit.byId('cd_masternode').set('disabled', true);
        dijit.byId('cd_hosts').set('disabled', true);
        dijit.byId('cbUsevpn').set('disabled', true);
        dijit.byId('sd_ResetCr').set('disabled', true);
    }

    clusterStorage.getItem(0).then(function (cluster) {
        // SSH details
        if (cluster.getValue('ssh_keybased')) {
            dijit.byId('cbKeybased').setValue(true);
        } else {
            dijit.byId('cbKeybased').setValue(false);
        }
        dijit.byId('sd_user').setValue(cluster.getValue('ssh_user'));
        // Check if just placeholder or we have a password provided.
        var a = '';
        var b = '';
        var storeVal;
        a = getClSSHPwd() + '';
        b = getSSHPwd() + '';
        // Have we been on 1st page and set passwords already?
        if (a.length > 2) {
            document.getElementById('sd_ClkeyPass').value = a;
        } else {
            storeVal = cluster.getValue('ssh_ClKeyPass');
            if (storeVal === undefined || storeVal == null || storeVal.length <= 0) {
                storeVal = '';
                cluster.setValue('ssh_ClKeyPass', '');
            }
            // New run. Check if just placeholder.
            if (storeVal && storeVal.length === 1) {
                document.getElementById('sd_ClkeyPass').value = '';
                sshClKeyPass = '';
            } else {
                sshClKeyPass = storeVal;
                document.getElementById('sd_ClkeyPass').value = sshClKeyPass;
            }
        }

        if (b.length > 2) {
            document.getElementById('sd_pwd').value = b;
        } else {
            storeVal = cluster.getValue('ssh_pwd');
            if (storeVal === undefined || storeVal == null || storeVal.length <= 0) {
                storeVal = '';
            }
            // New run. Check if just placeholder.
            if (storeVal && storeVal.length === 1) {
                document.getElementById('sd_pwd').value = '';
                sshPwd = '';
            } else {
                sshPwd = storeVal;
                document.getElementById('sd_pwd').value = sshPwd;
            }
        }

        sshUser = cluster.getValue('ssh_user');
        sshKeybased = cluster.getValue('ssh_keybased');

        dijit.byId('sd_ClkeyUser').setValue(
            cluster.getValue('ssh_ClKeyUser'));
        sshClKeyUser = cluster.getValue('ssh_ClKeyUser');

        dijit.byId('sd_ClkeyFile').setValue(
            cluster.getValue('ssh_ClKeyFile'));
        sshClKeyFile = cluster.getValue('ssh_ClKeyFile');

        // Cluster details
        dijit.byId('cd_name').setValue(cluster.getValue('name'));
        dijit.byId('cd_apparea').setValue(cluster.getValue('apparea'));
        dijit.byId('cd_writeload').setValue(cluster.getValue('writeload'));
        dijit.byId('cd_clver').setValue(cluster.getValue('ClusterVersion'));
        // If ClusterVersion is empty, it's probably old configuration.
        mcc.util.setClusterVersion(dijit.byId('cd_clver').getValue() || '7.6');

        // Installation details
        if (cluster.getValue('installCluster')) {
            dijit.byId('sd_installCluster').textbox.value = cluster.getValue('installCluster');
        } else {
            // First run, set to NONE//REPO
            dijit.byId('sd_installCluster').textbox.value = 'NONE';
            cluster.setValue('installCluster', 'NONE');
        }
        installCl = cluster.getValue('installCluster');

        if (cluster.getValue('openfw')) {
            dijit.byId('cbOpenfw').setValue(cluster.getValue('openfw'));
        } else {
            // First run, set to NO
            dijit.byId('cbOpenfw').setValue(false);
            cluster.setValue('openfw', false);
        }
        openFW = cluster.getValue('openfw');
        if (cluster.getValue('usevpn')) {
            dijit.byId('cbUsevpn').setValue(cluster.getValue('usevpn'));
        } else {
            // First run, set to NO
            dijit.byId('cbUsevpn').setValue(false);
            cluster.setValue('usevpn', false);
        }
        useVPN = cluster.getValue('usevpn');
        mcc.userconfig.setCfgStarted(cluster.getValue('Started') || false);
        if (!cluster.getValue('Started')) {
            cluster.setValue('Started', false);
        }
        if (cluster.getValue('MasterNode')) {
            dijit.byId('cd_masternode').textbox.value = cluster.getValue('MasterNode');
        } else {
            // Not assigned.
            dijit.byId('cd_masternode').textbox.value = 'UNASSIGNED';
        }
    });
    // If hosts exist, set value
    hostStorage.getItems({ anyHost: false }).then(function (hosts) {
        var hostlist = '';
        var i;
        if ((!hosts || hosts.length === 0) && initialize) {
        } else {
            for (i in hosts) {
                if (i > 0) { hostlist += ', '; }
                hostlist += hosts[i].getValue('name');
            }
        }
        if (hostlist) {
            // Do check:
            var notProperIP = 0;
            var properIP = 0;
            var ar = hostlist.split(',');
            for (i in ar) {
                if ((ar[i].trim()).length > 0) {
                    if (ar[i].trim().toLowerCase() === 'localhost' || ar[i].trim() === '127.0.0.1') {
                        notProperIP += 1;
                    } else {
                        properIP += 1;
                    }
                }
            }
            if ((notProperIP > 1) || (notProperIP > 0 && properIP > 0)) {
                // This is actually invalid configuration read from store.
                mcc.util.displayModal('I', 0, 'Mixing localhost with remote hosts is not allowed!' +
                    ' Invalid configuration read from store.');
                document.getElementById('cd_hosts').focus();
            } else {
                dijit.byId('cd_hosts').setValue(hostlist);
                document.getElementById('cd_hosts').focus();
            }
            mcc.userconfig.setIsNewConfig(false);
            // Since there were hosts originally, fill in shadow:
            if (mcc.userconfig.isShadowEmpty('host')) {
                console.debug('[DBG]Filling in shadows from existing configuration.')
                mcc.userconfig.setOriginalStore('cluster');
                mcc.userconfig.setOriginalStore('host');
                mcc.userconfig.setOriginalStore('process');
                mcc.userconfig.setOriginalStore('processtype');
            };
        } else {
            mcc.userconfig.setIsNewConfig(true);
            document.getElementById('cd_hosts').focus();
        }
    });
}

/**
 *Setup the page with widgets for the cluster definition
 *
 */
function clusterDefinitionSetup () {
    // Setup the required headers
    var clusterDetailsHeader = new dojox.grid.DataGrid({
        baseClass: 'content-grid-header',
        autoHeight: true,
        structure: [{
            name: 'Cluster property',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, 'clusterDetailsHeader');
    clusterDetailsHeader.canSort = function () { return false };
    clusterDetailsHeader.startup();

    var sshDetailsHeader = new dojox.grid.DataGrid({
        baseClass: 'content-grid-header',
        autoHeight: true,
        structure: [{
            name: 'SSH property (Cluster-wide)',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, 'sshDetailsHeader');
    sshDetailsHeader.canSort = function () { return false };
    sshDetailsHeader.startup();

    var installDetailsHeader = new dojox.grid.DataGrid({
        baseClass: 'content-grid-header',
        autoHeight: true,
        structure: [{
            name: 'Install properties (Cluster-wide)',
            width: '30%'
        },
        {
            name: 'Value',
            width: '70%'
        }]
    }, 'installDetailsHeader');
    installDetailsHeader.canSort = function () { return false };
    installDetailsHeader.startup();

    // Setup all the required widgets and connect them to the save function
    var cdName = new dijit.form.TextBox({ style: 'width: 120px' }, 'cd_name');
    // dojo.connect(cd_name, "onChange", saveClusterDefinition);
    cdName.set('disabled', 'disabled');
    createTT(['cd_name', 'cd_name_qm'], 'Cluster name');

    var cdHosts = new dijit.form.TextBox({ style: 'width: 250px' }, 'cd_hosts');
    dojo.connect(cdHosts, 'onChange', dojo.partial(saveClusterDefinition, 'cd_hosts'));
    createTT(['cd_hosts', 'cd_hosts_qm'], 'Comma separated list of names or ip addresses of the \
        hosts to use for running MySQL Cluster');

    var cdApparea = new dijit.form.FilteringSelect({ style: 'width: 120px',
        store: mcc.storage.stores.appAreaStore() }, 'cd_apparea');
    dojo.connect(cdApparea, 'onChange', dojo.partial(saveClusterDefinition, 'cd_apparea'));
    createTT(['cd_apparea', 'cd_apparea_qm'], 'Intended use of the application. \
                This information is used for determining the appropriate \
                value of various configuration parameters.');

    var cdWriteload = new dijit.form.FilteringSelect({ style: 'width: 120px',
        store: mcc.storage.stores.loadStore() }, 'cd_writeload');
    dojo.connect(cdWriteload, 'onChange', dojo.partial(saveClusterDefinition, 'cd_writeload'));
    createTT(['cd_writeload', 'cd_writeload_qm'], 'Write load for the application. \
                This information is used for determining the appropriate \
                value of various configuration parameters.');

    var cdClver = new dijit.form.FilteringSelect({ style: 'width: 120px',
        store: mcc.storage.stores.clusterVersionStore() }, 'cd_clver');
    dojo.connect(cdClver, 'onChange', dojo.partial(saveClusterDefinition, 'cd_clver'));
    createTT(['cd_clver', 'cd_clver_qm'], 'Tells which Cluster version to install on hosts  \
                if and where so configured.');

    var cdMasternode = new dijit.form.FilteringSelect({ style: 'width: 120px' }, 'cd_masternode');
    createTT(['cd_masternode', 'cd_masternode_qm'], 'Node that has SSH connection to all other nodes. \
                This information is set while deploying Cluster.');

    var cbKeybased = new dijit.form.CheckBox({}, 'cbKeybased');
    dojo.connect(cbKeybased, 'onChange', dojo.partial(saveClusterDefinition, 'cbKeybased'));
    createTT(['cbKeybased', 'sd_keybased_qm'], 'Check this box if key based ssh login is enabled \
                on the hosts running MySQL Cluster.');

    var sdUser = new dijit.form.TextBox({ style: 'width: 120px' }, 'sd_user');
    dojo.connect(sdUser, 'onChange', dojo.partial(saveClusterDefinition, 'sd_user'));
    createTT(['sd_user', 'sd_user_qm'], 'User name for ssh login \
                to the hosts running MySQL Cluster.');

    var sdPwd = document.getElementById('sd_pwd');
    sdPwd.addEventListener('change', dojo.partial(saveClusterDefinition, 'sd_pwd'), false);
    sdPwd.addEventListener('keyup', function (ev) {
        if (ev.getModifierState('CapsLock') && this.type === 'password') {
            mcc.util.displayModal('I', 2, 'CAPSLOCK is ON');
        }
    });
    createTT(['sd_pwd', 'sd_pwd_qm'], 'Password for ssh login \
                to the hosts running MySQL Cluster.');

    // NEW SSH stuff:
    var sdClkeyUser = new dijit.form.TextBox({ style: 'width: 120px' }, 'sd_ClkeyUser');
    dojo.connect(sdClkeyUser, 'onChange', dojo.partial(saveClusterDefinition, 'sd_ClkeyUser'));
    sdClkeyUser.setAttribute('disabled', true);
    createTT(['sd_ClkeyUser', 'sd_ClkeyUser_qm'], 'User name for key login \
                if different than in key.');

    var sdClkeyPass = document.getElementById('sd_ClkeyPass');
    sdClkeyPass.addEventListener('change', dojo.partial(saveClusterDefinition, 'sd_ClkeyPass'), false);
    sdClkeyPass.addEventListener('keyup', function (ev) {
        if (ev.getModifierState('CapsLock') && this.type === 'password') {
            mcc.util.displayModal('I', 2, 'CAPSLOCK is ON');
        }
    });
    createTT(['sd_ClkeyPass', 'sd_ClkeyPass_qm'], 'Passphrase for the key.');

    var sdClkeyFile = new dijit.form.TextBox({ style: 'width: 245px' }, 'sd_ClkeyFile');
    dojo.connect(sdClkeyFile, 'onChange', dojo.partial(saveClusterDefinition, 'sd_ClkeyFile'));
    sdClkeyFile.setAttribute('disabled', true);
    createTT(['sd_ClkeyFile', 'sd_ClkeyFile_qm'], 'Path to file containing the key.');

    var sdInstallCluster = new dijit.form.FilteringSelect({}, 'sd_installCluster');
    var options = [];
    options.push({ label: 'BOTH', value: 'BOTH', selected: false });
    options.push({ label: 'DOCKER', value: 'DOCKER', selected: false });
    options.push({ label: 'REPO', value: 'REPO', selected: false });
    options.push({ label: 'NONE', value: 'NONE', selected: true });
    sdInstallCluster.set('labelAttr', 'label');
    sdInstallCluster.set('searchAttr', 'value');
    sdInstallCluster.set('idProperty', 'value');
    sdInstallCluster.store.setData(options);
    dojo.connect(sdInstallCluster, 'onChange', dojo.partial(saveClusterDefinition, 'sd_installCluster'));
    createTT(['sd_installCluster', 'sd_installCluster_qm'], 'Try installing MySQL Cluster \
        on hosts using repo/docker or both.');

    var cbOpenfw = new dijit.form.CheckBox({}, 'cbOpenfw');
    dojo.connect(cbOpenfw, 'onChange', dojo.partial(saveClusterDefinition, 'cbOpenfw'));
    createTT(['cbOpenfw', 'sd_openfw_qm'], 'Open necessary firewall ports for running MySQL Cluster.');

    var cbUsevpn = new dijit.form.CheckBox({}, 'cbUsevpn');
    dojo.connect(cbUsevpn, 'onChange', dojo.partial(saveClusterDefinition, 'cbUsevpn'));
    createTT(['cbUsevpn', 'sd_usevpn_qm'], 'Only for running MySQL Cluster inside VPN.');

    // Button for clearing credentials: sd_ResetCr
    var sdResetCr = new dijit.form.Button({
        label: 'RESET credentials for this cluster',
        baseClass: 'fbtn'
    }, 'sd_ResetCr');
    sdResetCr.tabIndex = '-1';
    dojo.connect(sdResetCr, 'onClick', function () {
        var clusterStorage = mcc.storage.clusterStorage();
        // Get the (one and only) cluster item and update it
        clusterStorage.getItem(0).then(function (cluster) {
            cluster.setValue('ssh_user', '');
            cluster.setValue('ssh_pwd', '');
            cluster.setValue('ssh_ClKeyUser', '');
            cluster.setValue('ssh_ClKeyPass', '');
            cluster.setValue('ssh_ClKeyFile', '');
            sshPwd = '';
            sshUser = '';
            sshClKeyUser = '';
            sshClKeyPass = '';
            sshClKeyFile = '';
            dijit.byId('sd_user').setValue('');
            document.getElementById('sd_pwd').value = '';
            dijit.byId('sd_ClkeyUser').setValue('');
            document.getElementById('sd_ClkeyPass').value = '';
            dijit.byId('sd_ClkeyFile').setValue('');
        });
    });
    document.getElementById('togglePasswordField').addEventListener('click', togglePasswordFieldClicked, false);
    document.getElementById('togglePassphraseField').addEventListener('click', togglePassphraseFieldClicked, false);
}

/******************************** Initialize  *********************************/

dojo.ready(function initialize () {
    console.info('[INF]Cluster definition module initialized');
});
