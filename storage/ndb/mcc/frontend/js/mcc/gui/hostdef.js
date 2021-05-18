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

/** ***************************************************************************
 ***                                                                        ***
 ***                              Host definition                           ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.gui.hostdef
 *
 *  Description:
 *      Show and edit host details such as HW resource information
 *
 *  External interface:
 *      mcc.gui.hostdef.hostGridSetup: Setup the grid, connect to storage
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *      addHostsDialogSetup: Setup a dialog for adding new hosts
 *      editHostsDialogSetup: Setup a dialog for editing selected hosts
 *      addHostList: Split hostlist, add individual hosts. Check if host exists
 *      saveSelectedHosts: Save selected hosts after editing
 *      getFieldTT: Get tooltip text for a specific field
 *
 *  Internal data:
 *      hostGrid: The data grid connected to the host storage
 *
 *  Unit test interface:
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *
 ******************************************************************************/

/** **************************** Import/export  ********************************/
dojo.provide('mcc.gui.hostdef');
dojo.require('dojox.grid.EnhancedGrid');
dojo.require('dijit.form.NumberSpinner');
dojo.require('dijit.form.TextBox');
dojo.require('dijit.form.SimpleTextarea');
dojo.require('dojox.grid.cells.dijit');
dojo.require('dijit.Tooltip');
dojo.require('mcc.util');
dojo.require('mcc.storage');
dojo.require('mcc.configuration');

/** ************************** External interface  *****************************/
mcc.gui.hostdef.hostGridSetup = hostGridSetup;

/** ***************************** Internal data ********************************/
var hostGrid = null;
var clusterRunning = false;

/** ********************* Progress bar handling ************************/
// Show progress bar
function updateProgressDialog (title, subtitle, props, indeterminate) {
    // Determine who called update by examining title.
    var firstWord = title.replace(/\s.*/, ''); // You can use " " instead of \s.
    // We know which procedure is running now. Pass the info to dialog setup.
    if (!dijit.byId('progressBarDialog')) {
        progressBarDialogSetup(firstWord, indeterminate);
        dojo.style(dijit.byId('progressBarDialog').closeButtonNode, 'display', 'none');
        dijit.byId('configWizardProgressBar').startup();
    }
    dijit.byId('progressBarDialog').set('title', title);
    dojo.byId('progressBarSubtitle').innerHTML = subtitle;
    dijit.byId('configWizardProgressBar').update(props);
    dijit.byId('progressBarDialog').show();
}

// Setup a dialog for showing progress
function progressBarDialogSetup (procRunning, indeterminate) {
    // Create the dialog if it does not already exist
    if (!dijit.byId('progressBarDialog')) {
        if (!indeterminate) {
            return new dijit.Dialog({
                id: 'progressBarDialog',
                duration: 0,
                content: "\
                    <div id='progressBarSubtitle'></div>\
                    <div id='configWizardProgressBar'\
                        dojoType='dijit.ProgressBar'\
                        progress: '0%',\
                        annotate='true'>\
                    </div>",
                _onKey: function () {}
            });
        } else {
            return new dijit.Dialog({
                id: 'progressBarDialog',
                duration: 0,
                content: "\
                    <div id='progressBarSubtitle'></div>\
                    <div id='configWizardProgressBar'\
                        dojoType='dijit.ProgressBar'\
                        indeterminate: true,\
                        annotate='true'>\
                    </div>",
                _onKey: function () {}
            });
        }
    }
}

function removeProgressDialog () {
    if (dijit.registry.byId('progressBarDialog')) {
        do {
            if (dijit.registry.byId('progressBarDialog').open) {
                dijit.registry.byId('progressBarDialog').destroyRecursive();
                break;
            }
        } while (true);
    }
}

// Indeterminate progress for back-end and parallel operations.
function moveProgress (title, text) {
    updateProgressDialog(title, text,
        { indeterminate: true }, true);
}

/**
 *Determines if value passed is actually empty.
 *
 * @param {*} val   Value to check.
 * @returns
 */
function isEmpty (val) {
    return !!((val === undefined || val == null || val.length <= 0));
}

/**
 *Create tooltips for DOM nodes.
 *
 * @param {*} cId Array of DOM nodes tooltip connects to.
 * @param {*} lbl String representing text to show.
 * @returns fake
 */
function createTT (cId, lbl) {
    return new dijit.Tooltip({
        connectId: cId,
        label: lbl,
        destroyOnHide: true
    });
}
/**
 *Create new DIJIT TextBox and bind to DOM node
 *
 * @param {*} stl       String representing style.
 * @param {*} DOMNd     String representing DOM node.
 * @param {*} valbox    Boolean representing ValidationTextBox
 * @returns fake
 */
function createNewDOMTextBoxDijit (stl, DOMNd, valbox) {
    if (!valbox) {
        if (isEmpty(stl)) {
            return new dijit.form.TextBox({}, DOMNd);
        } else {
            return new dijit.form.TextBox({ style: stl }, DOMNd);
        }
    } else {
        if (isEmpty(stl)) {
            return new dijit.form.ValidationTextBox({}, DOMNd);
        } else {
            return new dijit.form.ValidationTextBox({ style: stl }, DOMNd);
        }
    }
}

// Split the hostlist, add individual hosts. Check if host exists
function addHostList (event) {
    var hosts = dijit.byId('hostlist').getValue().split(',');
    var newHostName = '';

    // Prevent default submit handling
    dojo.stopEvent(event);

    if (hosts.length <= 0) {
        console.info('Nothing in HOSTLIST');
        return;
    }

    // Exclude localhost AND 127.0.0.1 if both in list.
    if (hosts.indexOf('localhost') >= 0 && hosts.indexOf('127.0.0.1') >= 0) {
        mcc.util.displayModal('I', 3, '<span style=""font-size:140%;color:orangered;>localhost is already in the list!</span>');
        return;
    }

    // Check for illegal mix.
    var notProperIP = 0;
    var properIP = 0;
    var ar = [];
    for (var n in hosts) {
        if ((hosts[n].trim()).length > 0) {
            if (hosts[n].trim().toLowerCase() === 'localhost' || hosts[n].trim() === '127.0.0.1') {
                notProperIP += 1;
            } else {
                properIP += 1;
            }
            ar.push(hosts[n].trim());
        }
    }
    if ((notProperIP > 1) || (notProperIP > 0 && properIP > 0)) {
        mcc.util.displayModal('I', 3, '<span style=""font-size:140%;color:orangered;>Mixing localhost ' +
            'with remote hosts is not allowed!<br/>Please change localhost/127.0.0.1 ' +
            'to a proper IP address <br/>if you want to use your box!');
        return;
    }

    // Strip leading/trailing spaces, check multiple occurrences
    // Fetch list of hosts from hostStore to match against input.
    for (var i in hosts) {
        hosts[i] = hosts[i].replace(/^\s*/, '').replace(/\s*$/, '');
        if (hosts[i].length > 0 && dojo.indexOf(hosts, hosts[i]) !== parseInt(i)) {
            mcc.util.displayModal('I', 0, '<span style="font-size:140%;color:orangered;>Hostname ' +
                hosts[i] + ' is entered more than once</span>');
            return;
        }
    }
    // Check now that "localhost"/"127.0.0.1" is not mixed with remote hosts after adding saved hosts to ar[].
    notProperIP = 0;
    properIP = 0;
    mcc.storage.hostStorage().getItems({ anyHost: false }).then(function (items) {
        if (items && items.length > 0) {
            for (var z in items) {
                ar.push(items[z].item['name']);
            }
        }
    });

    for (n in ar) {
        if (String(ar[n]).toLowerCase() === 'localhost' || String(ar[n]) === '127.0.0.1') {
            notProperIP += 1;
        } else {
            properIP += 1;
        }
    }
    if ((notProperIP > 1) || (notProperIP > 0 && properIP > 0)) {
        mcc.util.displayModal('I', 0, '<span style=""font-size:140%;color:orangered;>Mixing localhost ' +
            'with remote hosts is not allowed!<br/>Please change localhost/127.0.0.1 to ' +
            'a proper IP address if you want to use your box.');
        return;
    }

    // Loop over all new hosts and check if name already exists.
    var waitConditions = [];
    for (i in hosts) {
        waitConditions[i] = new dojo.Deferred();
        // Fetch from hostStore to see if the host exists
        (function (host, waitCondition) {
            mcc.storage.hostStorage().getItems({ name: host, anyHost: false }).then(
                function (items) {
                    if (items && items.length > 0) {
                        mcc.util.displayModal('I', 0, '<span style=""font-size:140%;color:orangered;>Host ' +
                            host + ' already exists</span>');
                        waitCondition.resolve(false);
                    } else {
                        waitCondition.resolve(true);
                    }
                }
            );
        })(hosts[i], waitConditions[i]);
    }

    // Wait for all hosts to be checked - add all or none
    var waitList = new dojo.DeferredList(waitConditions);
    waitList.then(function (res) {
        if (res) {
            for (var i in res) {
                if (!res[i][1]) {
                    return;
                }
            }
            // If we haven't returned already, all can be added
            var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();
            for (i in hosts) {
                newHostName = '';
                if (hosts[i].length === 0) continue;
                console.info('Adding new host ' + hosts[i]);
                newHostName = hosts[i];
                // will not call HWResFetch for new item although there might be ClusterLevel credentials present.
                mcc.storage.hostStorage().newItem({
                    name: hosts[i],
                    anyHost: false
                }, false, false);
                // Save credentials:
                var test = newHostName;
                mcc.storage.hostStorage().getItems({ name: test, anyHost: false }).then(function (nhost) {
                    console.info('Updating credentials for new host ' + nhost[0].getValue('name'));
                    if (!clusterHasCreds) {
                        if (dijit.byId('cbKeyauth').get('checked')) {
                            nhost[0].setValue('usr', '');
                            nhost[0].setValue('usrpwd', '');
                            nhost[0].setValue('key_usr', dijit.byId('sd_key_usr').getValue());
                            nhost[0].setValue('key_passp', document.getElementById('passphraseFieldA').value);
                            nhost[0].setValue('key_file', dijit.byId('sd_key_file').getValue());
                        } else {
                            nhost[0].setValue('usr', dijit.byId('sd_usr').getValue());
                            nhost[0].setValue('usrpwd', document.getElementById('passwordFieldA').value);
                            nhost[0].setValue('key_usr', '');
                            nhost[0].setValue('key_passp', '');
                            nhost[0].setValue('key_file', '');
                        }
                        nhost[0].setValue('key_auth', dijit.byId('cbKeyauth').get('checked'));
                    } else {
                        console.warn('Cluster credentials in effect for new host %s!',
                            nhost[0].getValue('name'));
                        nhost[0].setValue('usr', '');
                        nhost[0].setValue('usrpwd', '');
                        nhost[0].setValue('key_usr', '');
                        nhost[0].setValue('key_passp', '');
                        nhost[0].setValue('key_file', '');
                        nhost[0].setValue('key_auth', false);
                    }

                    if (dijit.byId('sd_IntIP').getValue()) {
                        nhost[0].setValue('internalIP', dijit.byId('sd_IntIP').get('value'));
                    }
                    // Save other preferences
                    nhost[0].setValue('openfwhost', dijit.byId('cbOpenfwHost').get('checked'));
                    nhost[0].setValue('installonhost', dijit.byId('cbInstallonhost').get('checked'));
                    console.debug('[DBG]Saving credentials and preferences for new host ' + nhost[0].getValue('name') +
                        '. Override is FALSE');
                    mcc.storage.hostStorage().save();
                    mcc.storage.getHostResourceInfo(
                        nhost[0].getValue('name'), nhost[0].getValue('id'), true, false);
                    if (!nhost[0].getValue('SWInstalled')) {
                        nhost[0].setValue('SWInstalled', false);
                    }
                    // Clean up:
                    dijit.byId('sd_usr').setValue('');
                    document.getElementById('passwordFieldA').value = '';
                    dijit.byId('sd_key_usr').setValue('');
                    document.getElementById('passphraseFieldA').value = '';
                    dijit.byId('sd_key_file').setValue('');
                    dijit.byId('sd_IntIP').setValue('');
                });
            }
            dijit.byId('addHostsDlg').hide();
        }
    });
}

function updateHostAuth () {
    var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();
    if (!clusterHasCreds) {
        if (dijit.byId('cbKeyauth').get('checked')) {
            dijit.byId('sd_usr').set('disabled', true);
            dijit.byId('sd_usr').setValue('');
            document.getElementById('passwordFieldA').disabled = true;
            document.getElementById('passwordFieldA').value = '';
            document.getElementById('togglePasswordFieldA').disabled = true;

            dijit.byId('sd_key_usr').set('disabled', false);
            document.getElementById('passphraseFieldA').disabled = false;
            document.getElementById('togglePassphraseFieldA').disabled = false;
            dijit.byId('sd_key_file').set('disabled', false);
        } else {
            dijit.byId('sd_usr').set('disabled', false);
            document.getElementById('passwordFieldA').disabled = false;
            document.getElementById('togglePasswordFieldA').disabled = false;

            dijit.byId('sd_key_usr').set('disabled', true);
            dijit.byId('sd_key_usr').setValue('');
            document.getElementById('passphraseFieldA').disabled = true;
            document.getElementById('passphraseFieldA').value = '';
            document.getElementById('togglePassphraseFieldA').disabled = true;
            dijit.byId('sd_key_file').set('disabled', true);
            dijit.byId('sd_key_file').setValue('');
        }
    } else {
        dijit.byId('sd_key_usr').set('disabled', true);
        dijit.byId('sd_key_usr').setValue('');
        document.getElementById('passphraseFieldA').disabled = true;
        document.getElementById('passphraseFieldA').value = '';
        document.getElementById('togglePassphraseFieldA').disabled = true;
        dijit.byId('sd_key_file').set('disabled', true);
        dijit.byId('sd_key_file').setValue('');
        dijit.byId('sd_usr').set('disabled', true);
        dijit.byId('sd_usr').setValue('');
        document.getElementById('passwordFieldA').disabled = true;
        document.getElementById('passwordFieldA').value = '';
        document.getElementById('togglePasswordFieldA').disabled = true;
    }
}

// Same for EDIT dialog
function updateHostAuthE () {
    var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();
    if (!clusterHasCreds) {
        if (dijit.byId('cbKeyauthEdit').get('checked')) {
            dijit.byId('sd_usredit').set('disabled', true);
            dijit.byId('sd_usredit').setValue('');
            document.getElementById('passwordField').disabled = true;
            document.getElementById('passwordField').value = '';
            document.getElementById('togglePasswordField').disabled = true;
            dijit.byId('sd_key_usredit').set('disabled', false);
            document.getElementById('passphraseField').disabled = false;
            document.getElementById('togglePassphraseField').disabled = false;
            dijit.byId('sd_key_fileedit').set('disabled', false);
        } else {
            dijit.byId('sd_usredit').set('disabled', false);
            document.getElementById('passwordField').disabled = false;
            document.getElementById('togglePasswordField').disabled = false;
            dijit.byId('sd_key_usredit').set('disabled', true);
            dijit.byId('sd_key_usredit').setValue('');
            document.getElementById('passphraseField').disabled = true;
            document.getElementById('passphraseField').value = '';
            document.getElementById('togglePassphraseField').disabled = true;
            dijit.byId('sd_key_fileedit').set('disabled', true);
            dijit.byId('sd_key_fileedit').setValue('');
        }
    } else {
        dijit.byId('sd_usredit').set('disabled', true);
        dijit.byId('sd_usredit').setValue('');
        document.getElementById('passwordField').disabled = true;
        document.getElementById('passwordField').value = '';
        document.getElementById('togglePasswordField').disabled = true;
        dijit.byId('sd_key_usredit').set('disabled', true);
        dijit.byId('sd_key_usredit').setValue('');
        document.getElementById('passphraseField').disabled = true;
        document.getElementById('passphraseField').value = '';
        document.getElementById('togglePassphraseField').disabled = true;
        dijit.byId('sd_key_fileedit').set('disabled', true);
        dijit.byId('sd_key_fileedit').setValue('');
    }
}

function showHideRepoDocker () {
    var div = document.getElementById('RightContainer');
    if (dijit.byId('cbInstallonhostEdit').get('checked')) {
        div.style.visibility = 'visible';
    } else {
        div.style.visibility = 'hidden';
    }
}
function updateHostRepoDocker () {
    var selection = hostGrid.selection.getSelected();
    if (selection && selection.length > 0) {
        for (var i in selection) {
            console.info('Updating selected[' + i + ']');

            console.debug('[DBG]RepoURL is ' + dijit.byId('sd_repoURL').getValue());
            // Update info if necessary.
            if (dijit.byId('sd_repoURL').getValue() || dijit.byId('sd_repoURL').getValue() === '') {
                console.debug('[DBG]Updating REPO URL.');
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'installonhostrepourl', dijit.byId('sd_repoURL').getValue());
            }
            if (dijit.byId('sd_dockerURL').getValue() || dijit.byId('sd_dockerURL').getValue() === '') {
                console.debug('[DBG]Updating DOCKER URL.');
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'installonhostdockerurl', dijit.byId('sd_dockerURL').getValue());
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'installonhostdockernet', dijit.byId('sd_dockerNET').getValue());
            }
        }
        console.info('[INF]Saving host storage, updateHostRepoDocker.');
        mcc.storage.hostStorage().save();
    }
}
/**
 *Creates addHostDialog widget in formID addHostsForm if one doesn't exist.
 *
 * @returns Fake.
 */
function createAddHostDialog () {
    if (!dijit.byId('addHostsDlg')) {
        return new dijit.Dialog({
            id: 'addHostsDlg',
            title: 'Add new host',
            content: "\
                <form id='addHostsForm' data-dojo-type='dijit.form.Form'>\
                    <p>\
                        <br>Host name: \
                        <span class=\"helpIcon\" id=\"hostlist_qm\">[?]</span>\
                        <br /><span id='hostlist'></span>\
                        <br />Host internal IP (VPN): \
                        <span class=\"helpIcon\" id=\"sd_IntIP_qm\">[?]</span>\
                        <br /><span id='sd_IntIP'></span>\
                        <br /><br />Key-based auth: \
                        <span class=\"helpIcon\" id=\"sd_key_auth_qm\">[?]</span>\
                        <span id='cbKeyauth'></span>\
                        <br /><table>\
                            <tr>\
                                <td>User \
                                    <span class=\"helpIcon\" id=\"sd_key_usr_qm\">[?]</span>\
                                </td>\
                                <td>Passphrase&nbsp&nbsp \
                                    <span class=\"helpIcon\" id=\"sd_key_passp_qm\">&nbsp&nbsp[?]&nbsp&nbsp&nbsp&nbsp</span>\
                                    <span>\
                                        <input type='button' class='tglbtn' id='togglePassphraseFieldA' value=''>\
                                    </span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='sd_key_usr'></span></td>\
                                <td>\
                                    <input id='passphraseFieldA' type='password' name='passphraseFieldA' value='' autocomplete='off' style='text-align: left;width:120px;height:100%;display=\"inline\"'\
                                </td>\
                            </tr>\
                        </table>\
                        <table>\
                            <tr>\
                                <td>Key file \
                                    <span class=\"helpIcon\" id=\"sd_key_file_qm\">[?]</span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='sd_key_file'></span></td>\
                            </tr>\
                        </table>\
                        <br /><br />Ordinary login:\
                        <br /><table>\
                            <tr>\
                                <td>User \
                                    <span class=\"helpIcon\" id=\"sd_usr_qm\">[?]</span>\
                                </td>\
                                <td>Password&nbsp&nbsp&nbsp&nbsp&nbsp \
                                    <span class=\"helpIcon\" id=\"sd_usrpwd_qm\">&nbsp&nbsp[?]&nbsp&nbsp&nbsp&nbsp</span>\
                                    <span>\
                                        <input type='button' class='tglbtn' id='togglePasswordFieldA' value=''>\
                                    </span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='sd_usr'></span></td>\
                                <td>\
                                    <input id='passwordFieldA' type='password' name='passwordFieldA' value='' autocomplete='off' style='text-align: left;width:120px;height:100%;display=\"inline\"'\
                                </td>\
                            </tr>\
                        </table>\
                        <br /><table>\
                            <tr>\
                                <td>Open FW ports \
                                    <span class=\"helpIcon\" id=\"sd_openfwhost_qm\">[?]</span>\
                                </td>\
                                <td>Configure installation \
                                    <span class=\"helpIcon\" id=\"sd_installonhost_qm\">[?]</span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='cbOpenfwHost'></span></td>\
                                <td><span id='cbInstallonhost'></span></td>\
                            </tr>\
                        </table>\
                    </p>\
                    <div data-dojo-type='dijit.form.Button' \
                        data-dojo-props=\"baseClass: 'fbtn', onClick:\
                            function() {\
                                dijit.byId('hostlist').setValue('');\
                                dijit.byId('addHostsDlg').hide();\
                            }\">Cancel</div> \
                    \
                    <div data-dojo-type='dijit.form.Button' type='submit'\
                        data-dojo-props='baseClass: \"fbtn\"'\
                        id='saveHosts'>Add\
                    </div>\
                </form>"
        });
    } else { return false; }
}

// Setup a dialog for adding new hosts
function addHostsDialogSetup () {
    var hostlist = null;
    var oldUser = mcc.gui.getSSHUser();
    var oldKeybased = mcc.gui.getSSHkeybased();
    var oldOpenfw = mcc.gui.getOpenFW(); // Cluster level setting.
    var oldInstallcluster = mcc.gui.getInstallCl();
    var oldPwd = mcc.gui.getSSHPwd();
    var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();
    if (createAddHostDialog()) {
        // already initialized
        // Must connect outside of html to be in scope of function
        dojo.connect(dijit.byId('addHostsForm'), 'onSubmit', addHostList);
        document.getElementById('togglePasswordFieldA').addEventListener('click', togglePasswordFieldClickedA, false);
        document.getElementById('togglePassphraseFieldA').addEventListener('click', togglePassphraseFieldClickedA, false);
        document.getElementById('passphraseFieldA').addEventListener('change', updateHostAuth, false);
        document.getElementById('passphraseFieldA').addEventListener('keyup', function (ev) {
            if (ev.getModifierState('CapsLock') && this.type === 'password') {
                mcc.util.displayModal('H', 2, '', 'CAPSLOCK is ON');
            }
        });
        document.getElementById('passwordFieldA').addEventListener('change', updateHostAuth, false);
        document.getElementById('passwordFieldA').addEventListener('keyup', function (ev) {
            if (ev.getModifierState('CapsLock') && this.type === 'password') {
                mcc.util.displayModal('H', 2, '', 'CAPSLOCK is ON');
            }
        });

        hostlist = new dijit.form.ValidationTextBox({ style: 'width: 245px' }, 'hostlist');
        hostlist.required = true;
        hostlist.missingMessage = 'This field must have value!';
        createTT(['hostlist', 'hostlist_qm'], 'Name or ip addresses of host to use for running MySQL Cluster');

        createNewDOMTextBoxDijit('width: 245px', 'sd_IntIP', true);
        createTT(['sd_IntIP', 'sd_IntIP_qm'], 'If running MySQL Cluster inside VPN, internal ip \
            addresses of host to use for running MySQL Cluster');

        var cbKeyauth = new dijit.form.CheckBox({}, 'cbKeyauth');
        dojo.connect(cbKeyauth, 'onChange', updateHostAuth);
        createTT(['cbKeyauth', 'sd_key_auth_qm'], 'Check this box if key based ssh login is \
            enabled on this host.');
        // "Ordinary" USER/PASSWORD login
        var sdUsr = new dijit.form.TextBox({ style: 'width: 120px' }, 'sd_usr');
        dojo.connect(sdUsr, 'onChange', updateHostAuth);
        createTT(['sd_usr', 'sd_usr_qm'], 'User name for ssh login to the hosts running \
            MySQL Cluster.');
        createTT(['sd_usrpwd_qm', 'passwordFieldA'], 'Password for ssh login to the hosts running MySQL Cluster.');
        // Key based login.
        var sdKeyUsr = new dijit.form.TextBox({ style: 'width: 120px' }, 'sd_key_usr');
        dojo.connect(sdKeyUsr, 'onChange', updateHostAuth);
        createTT(['sd_key_usr', 'sd_key_usr_qm'], 'User name for key login if different than in key.');
        createTT(['sd_key_passp_qm', 'passphraseFieldA'], 'Passphrase for the key.');
        var sdKeyFile = new dijit.form.TextBox({ style: 'width: 245px' }, 'sd_key_file');
        dojo.connect(sdKeyFile, 'onChange', updateHostAuth);
        createTT(['sd_key_file', 'sd_key_file_qm'], 'Path to file containing the key.');
        // Firewall and installation
        var cbOpenfwHost = new dijit.form.CheckBox({}, 'cbOpenfwHost');
        createTT(['cbOpenfwHost', 'sd_openfwhost_qm'], 'Check this box if you need to open firewall \
            ports on this host. Available only for Oracle linux.');
        var cbInstallonhost = new dijit.form.CheckBox({}, 'cbInstallonhost');
        createTT(['cbInstallonhost', 'sd_installonhost_qm'], 'Check this box if you need to \
            install MySQL Cluster on this host.');
    }

    if (!clusterHasCreds) {
        // Init fields that could have changed:
        if (isEmpty(oldKeybased)) {
            dijit.byId('cbKeyauth').setValue(false);
        } else {
            dijit.byId('cbKeyauth').setValue(oldKeybased);
        }
        updateHostAuth();
        if (isEmpty(oldUser)) {
            dijit.byId('sd_usr').set('value', '');
        } else {
            dijit.byId('sd_usr').set('value', oldUser);
        }
        if (isEmpty(oldPwd)) {
            document.getElementById('passwordFieldA').value = '';
        } else {
            document.getElementById('passwordFieldA').value = oldPwd;
        }
    } else {
        console.warn('Cluster credentials in effect for new host!');
        dijit.byId('cbKeyauth').set('checked', false);
        dijit.byId('cbKeyauth').set('disabled', true);
        dijit.byId('sd_usr').setValue('');
        dijit.byId('sd_usr').set('disabled', true);
        document.getElementById('passwordFieldA').value = '';
        document.getElementById('passwordFieldA').disabled = true;
        document.getElementById('togglePasswordFieldA').disabled = true;
        dijit.byId('sd_key_usr').setValue('');
        dijit.byId('sd_key_usr').set('disabled', true);
        document.getElementById('passphraseFieldA').value = '';
        document.getElementById('passphraseFieldA').disabled = true;
        document.getElementById('togglePassphraseFieldA').disabled = true;
        dijit.byId('sd_key_file').setValue('');
        dijit.byId('sd_key_file').set('disabled', true);
    }

    if (isEmpty(oldOpenfw)) {
        dijit.byId('cbOpenfwHost').setValue(false);
    } else {
        dijit.byId('cbOpenfwHost').setValue(oldOpenfw);
    }
    if (isEmpty(oldInstallcluster)) {
        dijit.byId('cbInstallonhost').setValue(false);
    } else {
        dijit.byId('cbInstallonhost').setValue(oldInstallcluster !== 'NONE');
    }
    if (mcc.gui.getUseVPN()) {
        dijit.byId('sd_IntIP').set('disabled', false);
    } else {
        dijit.byId('sd_IntIP').set('disabled', true);
    }
}

// Show/hide password/passphrase for EditHost.
function togglePasswordFieldClicked () {
    var passwordField = document.getElementById('passwordField');
    var value = passwordField.value;
    if (passwordField.type.toLowerCase() === 'password') {
        passwordField.type = 'text';
    } else {
        passwordField.type = 'password';
    }
    passwordField.value = value;
}

function togglePassphraseFieldClicked () {
    var passwordField = document.getElementById('passphraseField');
    var value = passwordField.value;
    if (passwordField.type.toLowerCase() === 'password') {
        passwordField.type = 'text';
    } else {
        passwordField.type = 'password';
    }
    passwordField.value = value;
}

// Show/hide password/passphrase for AddHost.
function togglePasswordFieldClickedA () {
    var passwordField = document.getElementById('passwordFieldA');
    var value = passwordField.value;
    if (passwordField.type.toLowerCase() === 'password') {
        passwordField.type = 'text';
    } else {
        passwordField.type = 'password';
    }
    passwordField.value = value;
}

function togglePassphraseFieldClickedA () {
    var passwordField = document.getElementById('passphraseFieldA');
    var value = passwordField.value;
    if (passwordField.type.toLowerCase() === 'password') {
        passwordField.type = 'text';
    } else {
        passwordField.type = 'password';
    }
    passwordField.value = value;
}

// Update and save the selected hosts after editing
function saveSelectedHosts (event) {
    // Prevent default submit handling
    dojo.stopEvent(event);
    var dir;
    var val;
    var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();
    var selection = hostGrid.selection.getSelected();
    if (selection && selection.length > 0) {
        for (var i in selection) {
            console.info('Updating selected host[' + i + ']');
            // hwResFetch == undefined, no info was fetched
            var uname = String(mcc.storage.hostStorage().store().getValue(selection[i], 'uname'));
            if (dijit.byId('uname').getValue() && dijit.byId('uname').getValue() !== uname) {
                console.debug('[DBG]Setting new value for UNAME.');
                uname = dijit.byId('uname').getValue();
                mcc.storage.hostStorage().store().setValue(selection[i], 'uname', uname);
                console.debug('[DBG]Updating uname, check predefined directories!');
                // If we changed platform, we need to update predef dirs
                if (mcc.storage.hostStorage().store().getValue(selection[i],
                    'installdir_predef') === true) {
                    dir = mcc.storage.hostStorage().getPredefinedDirectory(uname, 'installdir');
                    console.debug('[DBG]Update predfined installdir to ' + dir);
                    mcc.storage.hostStorage().store().setValue(selection[i], 'installdir', dir);
                }
                if (mcc.storage.hostStorage().store().getValue(selection[i], 'datadir_predef') === true) {
                    dir = mcc.storage.hostStorage().getPredefinedDirectory(uname, 'datadir');
                    console.debug('[DBG]Update predfined datadir to ' + dir);
                    mcc.storage.hostStorage().store().setValue(selection[i], 'datadir', dir);
                }
            }
            var hsval = mcc.storage.hostStorage().store().getValue(selection[i], 'ram');
            if (dijit.byId('ram').getValue() && dijit.byId('ram').getValue() !== hsval) {
                val = dijit.byId('ram').getValue();
                if (val > 0 && val < 90000000) {
                    console.debug('[DBG]Setting new value for RAM.');
                    mcc.storage.hostStorage().store().setValue(selection[i], 'ram', val);
                }
            }
            hsval = mcc.storage.hostStorage().store().getValue(selection[i], 'cores');
            if (dijit.byId('cores').getValue() && dijit.byId('cores').getValue() !== hsval) {
                val = dijit.byId('cores').getValue();
                if (val > 0 && val < 5000) {
                    console.debug('[DBG]Setting new value for CORES.');
                    mcc.storage.hostStorage().store().setValue(selection[i], 'cores', val);
                }
            }

            // make *proper* call about datadir and installdir
            var installdir = String(mcc.storage.hostStorage().store().getValue(selection[i], 'installdir'));
            if (dijit.byId('installdir').getValue()) {
                if (dijit.byId('installdir').getValue() !== installdir ||
                    mcc.util.terminatePath(dijit.byId('installdir').getValue()) !== installdir) {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'installdir',
                        mcc.util.terminatePath(dijit.byId('installdir').getValue()));
                    mcc.storage.hostStorage().store().setValue(selection[i], 'installdir_predef', false);
                    console.debug('[DBG]Setting new value for INSTALLDIR.');
                }
            }

            var datadir = String(mcc.storage.hostStorage().store().getValue(selection[i], 'datadir'));
            if (dijit.byId('datadir').getValue()) {
                if (dijit.byId('datadir').getValue() !== datadir ||
                    mcc.util.terminatePath(dijit.byId('datadir').getValue()) !== datadir) {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'datadir',
                        mcc.util.terminatePath(dijit.byId('datadir').getValue()));
                    mcc.storage.hostStorage().store().setValue(selection[i], 'datadir_predef', false);
                    console.debug('[DBG]Setting new value for DATADIR.');
                }
            }

            hsval = mcc.storage.hostStorage().store().getValue(selection[i], 'diskfree');
            if (dijit.byId('diskfree').getValue() && dijit.byId('diskfree').getValue() !== hsval) {
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'diskfree', dijit.byId('diskfree').getValue());
                console.debug('[DBG]Setting new value for DISKFREE.');
            }
            // Forbid changing the external IP address of host since it's primary key for storage.
            // Delete the host and recreate if needs be.

            // Add invisible SWInstalled if necessary:
            hsval = mcc.storage.hostStorage().store().getValue(selection[i], 'SWInstalled');
            if (!hsval) {
                mcc.storage.hostStorage().store().setValue(selection[i], 'SWInstalled', false);
                console.debug('[DBG]Setting new value for status of INSTALLATION.');
            }
            if (dijit.byId('sd_IntIPedit').getValue()) {
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'internalIP', dijit.byId('sd_IntIPedit').getValue());
                console.debug('[DBG]Setting new value for INTERNALIP.');
            } else {
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'internalIP', '');
                console.debug('[DBG]Clearing value of INTERNALIP.');
            }

            if (!clusterHasCreds) {
                // Update credentials if necessary.
                if (dijit.byId('sd_usredit').getValue()) {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'usr', dijit.byId('sd_usredit').getValue());
                    console.debug('[DBG]Setting new value for USER.');
                } else {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'usr', '');
                    console.debug('[DBG]Clearing value of USER.');
                }
                if (document.getElementById('passwordField').value &&
                        document.getElementById('passwordField').value.length > 1) {
                    mcc.storage.hostStorage().store().setValue(selection[i],
                        'usrpwd', document.getElementById('passwordField').value);
                    console.debug('[DBG]Setting new value for PASSWORD.');
                } else {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'usrpwd', '');
                    console.debug('[DBG]Clearing value of PASSWORD.');
                }

                if (dijit.byId('sd_key_usredit').getValue()) {
                    mcc.storage.hostStorage().store().setValue(selection[i],
                        'key_usr', dijit.byId('sd_key_usredit').getValue());
                    console.debug('[DBG]Setting new value for KEYUSER.');
                } else {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'key_usr', '');
                    console.debug('[DBG]Clearing value of KEYUSER.');
                }

                if (document.getElementById('passphraseField').value &&
                        document.getElementById('passphraseField').value.length > 1) {
                    mcc.storage.hostStorage().store().setValue(selection[i],
                        'key_passp', document.getElementById('passphraseField').value);
                    console.debug('[DBG]Setting new value for KEYPASSPHRASE.');
                } else {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'key_passp', '');
                    console.debug('[DBG]Clearing value of KEYPASSPHRASE.');
                }
                if (dijit.byId('sd_key_fileedit').getValue()) {
                    mcc.storage.hostStorage().store().setValue(selection[i],
                        'key_file', dijit.byId('sd_key_fileedit').getValue());
                    console.debug('[DBG]Setting new value for KEYFILE.');
                } else {
                    mcc.storage.hostStorage().store().setValue(selection[i], 'key_file', '');
                    console.debug('[DBG]Clearing value of KEYFILE.');
                }
                mcc.storage.hostStorage().store().setValue(selection[i],
                    'key_auth', dijit.byId('cbKeyauthEdit').get('checked'));
            } else {
                console.info('Cluster credentials in effect for host[' + i + ']');
                dijit.byId('cbKeyauthEdit').set('checked', false);
                dijit.byId('sd_key_fileedit').setValue('');
                dijit.byId('sd_usredit').setValue('');
                document.getElementById('passwordField').value = '';
                dijit.byId('sd_key_usredit').setValue('');
                document.getElementById('passphraseField').value = '';
                dijit.byId('cbKeyauthEdit').set('checked', false);
                mcc.storage.hostStorage().store().setValue(selection[i], 'key_auth', false);
            }
            // Save other preferences
            console.debug('[DBG]Setting preferences for firewall, installation');
            mcc.storage.hostStorage().store().setValue(selection[i],
                'openfwhost', dijit.byId('cbOpenfwHostEdit').get('checked'));
            mcc.storage.hostStorage().store().setValue(selection[i],
                'installonhost', dijit.byId('cbInstallonhostEdit').get('checked'));
            // Clear IntIP member:
            dijit.byId('sd_IntIPedit').setValue('');

            var hwrf = String(mcc.storage.hostStorage().store().getValue(selection[i], 'hwResFetch'));
            if (typeof hwrf === 'undefined') {
                // We never fetched resources for this host before.
                mcc.storage.getHostResourceInfo(selection[i],
                    mcc.storage.hostStorage().store().getValue(selection[i], 'id'), true, true);
            } else {
                console.debug('[DBG]Setting host docker default preferences.');
                updateHostRepoDocker();
            }
            console.info('[INF]Saving host storage, saveSelectedHosts.');
            mcc.storage.hostStorage().save();
        }
        hostGrid.startup();
    }
    dijit.byId('editHostsDlg').hide();
}

// Get tooltip texts for various host attributes
function getFieldTT (field) {
    var fieldTT = {
        name: '<i>Host</i> is the name or ip address of the host as ' +
                    'known by the operating system',
        hwResFetch: '<i>Resource info</i> indicates the status of automatic ' +
                    'retrieval of hardware resource information. <i>N/A</i> ' +
                    'means that automatic fetching of information is turned ' +
                    'off, <i>Fetching</i> means that a request for ' +
                    'information has been sent to the host, <i>OK</i> means ' +
                    'that information has been fetched, and <i>Failed</i> ' +
                    'means that information could not be obtained',
        uname: '<i>Platform</i> is given by the type of hardware, ' +
                    'operating system and system software running on the host',
        ram: '<i>Memory</i> is the size of the internal memory of ' +
                    'the host, expressed in <b>M</b>ega<b>B</b>ytes',
        cores: "<i>CPU cores</i> is the number of cores of host's CPU. " +
                    'A multi core CPU can do several things simultaneously.',
        installdir: '<i>Installation directory</i> is the directory where ' +
                'the MySQL Cluster software is installed. ',
        datadir: '<i>Data directory</i> is the directory for storing ' +
                'data, log files, etc. for MySQL Cluster. Data directories ' +
                'for individual processes are defined automatically by ' +
                'appending process ids to this root path. If you want to ' +
                'have different data directories for different processes, ' +
                'this can be overridden for each process later in this wizard.',
        diskfree: '<i>DiskFree</i> Amount of free space (in GB) available on ' +
                  'chosen Data directory disk. In case of failure to fetch,' +
                  'unknown is displayed.',
        fqdn: '<i>FQDN</i> is fully qualified domain name.',
        internalIP: '<i>Internal IP</i> is host IP internal to VPN. Default is FQDN',
        openfwhost: '<i>Open FW</i> means try to open necessary firewall ports on host.',
        installonhostrepourl: '<i>REPO URL</i> is URL to repository configuration file.' +
                'Default is ' + mcc.util.getClusterUrlRoot() + '-PLATFORMMAJOR.rpm/deb...',
        installonhostdockerurl: '<i>DOCKER URL</i> is URL to docker image file.',
        installonhost: '<i>Install</i> means try to install MySQL NDB Cluster and Server on host.'
    };
    return fieldTT[field];
}

/**
 *Creates editHostsDlg widget in formID editHostsForm if one doesn't exist.
 *
 * @returns Fake.
 */
function createEditHostDialog () {
    if (!dijit.byId('editHostsDlg')) {
        return new dijit.Dialog({
            id: 'editHostsDlg',
            title: 'Edit selected host(s)',
            style: { width: '720px' },
            content: "\
                <form id='editHostsForm' data-dojo-type='dijit.form.Form'>\
                    <div id='TableContainer' style='align:top;height:20%;'>\
                        <p>Please edit the fields you want to change. The changes \
                            will be applied to all selected hosts. Fields that \
                            are not edited in the form below will be left \
                            unchanged.\
                        </p>\
                        <p>\
                        <table>\
                            <tr>\
                                <td>Platform \
                                    <span class=\"helpIcon\"\
                                        id=\"uname_qm\">[?]\
                                    </span>\
                                </td>\
                                <td>Memory (MB) \
                                    <span class=\"helpIcon\"\
                                        id=\"ram_qm\">[?]\
                                    </span>\
                                </td>\
                                <td>CPU cores \
                                    <span class=\"helpIcon\"\
                                        id=\"cores_qm\">[?]\
                                    </span>\
                                </td>\
                                <td>MySQL Cluster install directory \
                                    <span class=\"helpIcon\"\
                                        id=\"installdir_qm\">[?]\
                                    </span>\
                                </td>\
                                <td>MySQL Cluster data directory \
                                    <span class=\"helpIcon\"\
                                        id=\"datadir_qm\">[?]\
                                    </span>\
                                </td>\
                                <td>DiskFree \
                                    <span class=\"helpIcon\"\
                                        id=\"diskfree_qm\">[?]\
                                    </span>\
                                </td>\
                            </tr>\
                            <tr>\
                                <td><span id='uname'></span></td>\
                                <td><span id='ram'></span></td>\
                                <td><span id='cores'></span></td>\
                                <td><span id='installdir'></span></td>\
                                <td><span id='datadir'></span></td>\
                                <td><span id='diskfree'></span></td>\
                            </tr>\
                        </table>\
                        </p>\
                    </div>\
                    <div id='RestContainer' style='display: flex; flex-direction: row; justify-content: space-around;width:100%;height:70%;'>\
                        <div id='LeftContainer' style='text-align: left;width:50%;height:100%;'>\
                            <br />Host external IP: \
                            <span class=\"helpIcon\" id=\"sd_ExtIPedit_qm\">[?]</span>\
                            <br /><span id='sd_ExtIPedit'></span>\
                            <br />Host internal IP (VPN): \
                            <span class=\"helpIcon\" id=\"sd_IntIPedit_qm\">[?]</span>\
                            <br /><span id='sd_IntIPedit'></span>\
                            <br /><br />Key-based auth: \
                            <span class=\"helpIcon\" id=\"sd_key_authedit_qm\">[?]</span>\
                            <span id='cbKeyauthEdit'></span>\
                            <br /><table>\
                                <tr>\
                                    <td>User \
                                        <span class=\"helpIcon\" id=\"sd_key_usredit_qm\">[?]</span>\
                                    </td>\
                                    <td>Passphrase \
                                        <span class=\"helpIcon\" id=\"sd_key_passpedit_qm\">[?]</span>\
                                    </td>\
                                </tr>\
                                <tr>\
                                    <td><span id='sd_key_usredit'></span></td>\
                                    <td>\
                                        <input id='passphraseField' type='password' name='passphraseField' value='' autocomplete='off' style='text-align: left;width:120px;height:100%;display=\"inline\"'\
                                        >\
                                    </td>\
                                    <td>\
                                        <input type='button' class='tglbtn' id='togglePassphraseField' value=''>\
                                    </td>\
                                </tr>\
                            </table>\
                            <table>\
                                <tr>\
                                    <td>Key file \
                                        <span class=\"helpIcon\" id=\"sd_key_fileedit_qm\">[?]</span>\
                                    </td>\
                                </tr>\
                                <tr>\
                                    <td><span id='sd_key_fileedit'></span></td>\
                                </tr>\
                            </table>\
                            <br /><br />Ordinary login:\
                            <br /><table>\
                                <tr>\
                                    <td>User \
                                        <span class=\"helpIcon\" id=\"sd_usredit_qm\">[?]</span>\
                                    </td>\
                                    <td>Password \
                                        <span class=\"helpIcon\" id=\"sd_usrpwdedit_qm\">[?]</span>\
                                    </td>\
                                </tr>\
                                <tr>\
                                    <td><span id='sd_usredit'></span></td>\
                                    <td>\
                                        <input id='passwordField' type='password' name='passwordField' value='' autocomplete='off' style='text-align: left;width:120px;height:100%;display=\"inline\"'\
                                        >\
                                    </td>\
                                    <td>\
                                        <input type='button' class='tglbtn' id='togglePasswordField' value=''>\
                                    </td>\
                                </tr>\
                            </table>\
                            <br />Open ports in firewall: \
                            <span class=\"helpIcon\" id=\"sd_openfwhostedit_qm\" style='margin: 3px;'>[?]</span>\
                            <span id='cbOpenfwHostEdit'></span>\
                            <br />Configure installation: \
                            <span class=\"helpIcon\" id=\"sd_installonhostedit_qm\" style='margin: 3px;'>[?]</span>\
                            <span id='cbInstallonhostEdit'></span>\
                        </div>\
                        <div id='RightContainer' style='text-align: left;width:50%;height:100%;'>\
                            Please edit the fields you want to change. The changes \
                            will be applied to all selected hosts. Fields that \
                            are not edited in the form below will be left \
                            unchanged. If both URLs are provided, we favor REPO.\
                            <br />REPOSITORY URL: \
                            <span class=\"helpIcon\" id=\"sd_repoURL_qm\">[?]</span>\
                            <br /><span id='sd_repoURL'></span>\
                            <br />DOCKER URL: \
                            <span class=\"helpIcon\" id=\"sd_dockerURL_qm\">[?]</span>\
                            <br /><span id='sd_dockerURL'></span>\
                            <br />DOCKER network create: \
                            <span class=\"helpIcon\" id=\"sd_dockerNET_qm\">[?]</span>\
                            <br /><span id='sd_dockerNET'></span>\
                            <br />DOCKER status: \
                            <span id='sd_dockerSTATUS'></span>\
                        </div>\
                    </div>\
                    <div id='BottomContainer' style='width:100%;height:10%;display: flex; flex-direction: row-reverse;'>\
                        <div data-dojo-type='dijit.form.Button' style='margin-top: 20px;margin-right: 10px;' \
                            data-dojo-props=\"baseClass: 'fbtn', onClick:\
                                function() {\
                                    dijit.byId('editHostsDlg').hide();\
                                }\">Cancel</div> \
                        \
                        <div data-dojo-type='dijit.form.Button' type='submit' style='margin-top: 20px;'\
                            data-dojo-props='baseClass: \"fbtn\"'\
                            id='saveSelection'>&nbspSave&nbsp\
                        </div>\
                    </div>\
                </form>"
        });
    }
}

// Setup a dialog for editing hosts
function editHostsDialogSetup () {
    if (createEditHostDialog()) {
        // Must connect outside of html to be in scope of function
        dojo.connect(dijit.byId('editHostsForm'), 'onSubmit', saveSelectedHosts);
        document.getElementById('togglePasswordField').addEventListener('click', togglePasswordFieldClicked, false);
        document.getElementById('togglePassphraseField').addEventListener('click', togglePassphraseFieldClicked, false);
        document.getElementById('passphraseField').addEventListener('change', updateHostAuthE, false);
        document.getElementById('passphraseField').addEventListener('keyup', function (ev) {
            if (ev.getModifierState('CapsLock') && this.type === 'password') {
                mcc.util.displayModal('H', 2, '', 'CAPSLOCK is ON');
            }
        });
        document.getElementById('passwordField').addEventListener('change', updateHostAuthE, false);
        document.getElementById('passwordField').addEventListener('keyup', function (ev) {
            if (ev.getModifierState('CapsLock') && this.type === 'password') {
                mcc.util.displayModal('H', 2, '', 'CAPSLOCK is ON');
            }
        });

        // Define widgets
        createNewDOMTextBoxDijit('width: 60px', 'uname', false);
        var ram = new dijit.form.NumberSpinner({
            style: 'width: 80px',
            constraints: { min: 1, max: 90000000, places: 0 }
        }, 'ram');
        var cores = new dijit.form.NumberSpinner({
            style: 'width: 80px',
            constraints: { min: 1, max: 5000, places: 0, format: '####' }
        }, 'cores');
        createNewDOMTextBoxDijit('width: 170px', 'installdir', false);
        createNewDOMTextBoxDijit('width: 170px', 'datadir', false);
        createNewDOMTextBoxDijit('width: 100px', 'diskfree', false);

        // Define grid tooltips
        createTT(['uname_qm'], getFieldTT('uname'));
        createTT(['ram_qm'], getFieldTT('ram'));
        createTT(['cores_qm'], getFieldTT('cores'));
        createTT(['installdir_qm'], getFieldTT('installdir'));
        createTT(['datadir_qm'], getFieldTT('datadir'));
        createTT(['diskfree_qm'], getFieldTT('diskfree'));

        createNewDOMTextBoxDijit('width: 245px', 'sd_ExtIPedit', true);
        createTT(['sd_ExtIPedit', 'sd_ExtIPedit_qm'], 'External ip addresses of host meaning the \
            address at which MCC can access this host.');

        createNewDOMTextBoxDijit('width: 245px', 'sd_IntIPedit', true);
        createTT(['sd_IntIPedit', 'sd_IntIPedit_qm'], 'If running MySQL Cluster inside VPN, \
            internal ip addresses of host to use for running MySQL Cluster');

        // Credentials definition
        var cbKeyauthEdit = new dijit.form.CheckBox({}, 'cbKeyauthEdit');
        dojo.connect(cbKeyauthEdit, 'onChange', updateHostAuthE);
        createTT(['cbKeyauthEdit', 'sd_key_authedit_qm'],
            'Check this box if key based ssh login is enabled on this host.');

        // "Ordinary" USER/PASSWORD login
        var sdUsredit = new dijit.form.TextBox({ style: 'width: 120px' }, 'sd_usredit');
        dojo.connect(sdUsredit, 'onChange', updateHostAuthE);
        createTT(['sd_usredit', 'sd_usredit_qm'],
            'User name for ssh login to the hosts running MySQL Cluster.');
        createTT(['sd_usrpwdedit_qm', 'passwordField'],
            'Password for ssh login to the hosts running MySQL Cluster.');

        // Key based login.
        var sdKeyusrEdit = new dijit.form.TextBox({ style: 'width: 120px' }, 'sd_key_usredit');
        dojo.connect(sdKeyusrEdit, 'onChange', updateHostAuthE);
        createTT(['sd_key_usredit', 'sd_key_usredit_qm'], 'User name for key login \
                    if different than in key.');
        createTT(['sd_key_passpedit_qm', 'passphraseField'], 'Passphrase for the key.');

        var sdKeyfileEdit = new dijit.form.TextBox({ style: 'width: 245px' }, 'sd_key_fileedit');
        dojo.connect(sdKeyfileEdit, 'onChange', updateHostAuthE);
        createTT(['sd_key_fileedit', 'sd_key_fileedit_qm'], 'Path to file containing the key.');

        // Firewall and installation
        var cbOpenfwHostEdit = new dijit.form.CheckBox({}, 'cbOpenfwHostEdit');
        createTT(['cbOpenfwHostEdit', 'sd_openfwhostedit_qm'],
            'Check this box if you need to open firewall ports on this host.');
        var cbInstallonhostEdit = new dijit.form.CheckBox({}, 'cbInstallonhostEdit');
        dojo.connect(cbInstallonhostEdit, 'onChange', showHideRepoDocker);
        createTT(['cbInstallonhostEdit', 'sd_installonhostedit_qm'], 'Check this box if you need \
            to install MySQL Cluster on this host.');

        // REPO-DOCKER stuff
        // Define widgets
        createNewDOMTextBoxDijit('width: 345px', 'sd_repoURL', false);
        createTT(['sd_repoURL', 'sd_repoURL_qm'], 'URL for repository.');

        createNewDOMTextBoxDijit('width: 345px', 'sd_dockerURL', false);
        createTT(['sd_dockerURL', 'sd_dockerURL_qm'], 'URL for docker image.');
        createNewDOMTextBoxDijit('width: 345px', 'sd_dockerNET', false);
        createTT(['sd_dockerNET', 'sd_dockerNET_qm'], 'Docker network definition. If nothing is \
            entered, --net=host will be used.');
        var sdDockerStatus = new dijit.form.SimpleTextarea({
            disabled: true,
            rows: '1',
            style: 'width: 254px; resize : none' },
        'sd_dockerSTATUS');
    }

    if (mcc.gui.getUseVPN()) {
        dijit.byId('sd_IntIPedit').set('disabled', false);
    } else {
        dijit.byId('sd_IntIPedit').set('disabled', true);
    }
}

// Setup the host grid with support for adding, deleting and editing hosts
function hostGridSetup () {
    clusterRunning = mcc.configuration.determineClusterRunning(mcc.configuration.clServStatus());
    var what = {};
    if (dijit.byId('hostGrid')) {
        console.info('[INF]Setup host definition widgets');
        if (clusterRunning) {
            what = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('clRunning', ['hosts']));
            if ((what || {}).text) {
                console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                // Can not allow poking around parameters while Cluster is alive.
                mcc.util.displayModal('I', 3, what.text);
            } else {
                console.warn('Cluster is running!');
            }
            // disable important components
            dijit.byId('hostGrid').setAttribute('disabled', true);
            dijit.byId('addHostsButton').setAttribute('disabled', true);
            dojo.style(dijit.byId('addHostsButton').domNode, 'display', 'none');
            dijit.byId('removeHostsButton').setAttribute('disabled', true);
            dojo.style(dijit.byId('removeHostsButton').domNode, 'display', 'none');
            dijit.byId('refreshHostsButton').setAttribute('disabled', true);
            dojo.style(dijit.byId('refreshHostsButton').domNode, 'display', 'none');
            dijit.byId('editHostsButton').setAttribute('disabled', true);
            dojo.style(dijit.byId('editHostsButton').domNode, 'display', 'none');
        }
        return;
    }
    // Button for adding a host. Show add hosts dialog on click
    var addButton = new dijit.form.Button({
        label: 'Add host',
        iconClass: 'dijitIconAdd',
        baseClass: 'fbtn'
    }, 'addHostsButton');
    dojo.connect(addButton, 'onClick', function () {
        // storage initializes config module, still maybe a good idea to do TRY here?.
        if (clusterRunning) {
            // Can not allow poking HOSTS while Cluster is alive.
            what = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('clRunning', ['hosts']));
            if ((what || {}).text) {
                console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                // Can not allow poking around parameters while Cluster is alive.
                mcc.util.displayModal('I', 3, what.text);
            }
            return;
        }
        // mcc.gui.stopStatusPoll('STOP');
        dijit.byId('hostlist').setValue('');
        var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();
        // We are at NEW host dialog so set everything up according to ClusterLvL defaults.
        if (isEmpty(mcc.gui.getOpenFW())) {
            dijit.byId('cbOpenfwHost').setValue(false);
        } else {
            dijit.byId('cbOpenfwHost').setValue(mcc.gui.getOpenFW());
        }
        if (isEmpty(mcc.gui.getInstallCl())) {
            dijit.byId('cbInstallonhost').setValue(false);
        } else {
            dijit.byId('cbInstallonhost').setValue(mcc.gui.getInstallCl().toUpperCase() !== 'NONE');
        }
        if (!clusterHasCreds) {
            var oldKeybased = mcc.gui.getSSHkeybased();
            if (isEmpty(oldKeybased)) {
                dijit.byId('cbKeyauth').setValue(false);
            } else {
                dijit.byId('cbKeyauth').setValue(oldKeybased);
            }
            if (isEmpty(mcc.gui.getSSHUser())) {
                dijit.byId('sd_usr').setValue('');
            } else {
                dijit.byId('sd_usr').setValue(mcc.gui.getSSHUser());
            }
            if (isEmpty(mcc.gui.getSSHPwd())) {
                document.getElementById('passwordFieldA').value = '';
            } else {
                document.getElementById('passwordFieldA').value = mcc.gui.getSSHPwd();
            }
        } else {
            console.warn('Cluster credentials in effect for new host!');
            dijit.byId('cbKeyauth').set('checked', false);
            dijit.byId('cbKeyauth').set('disabled', true);
            dijit.byId('sd_usr').setValue('');
            dijit.byId('sd_usr').set('disabled', true);
            document.getElementById('passwordFieldA').value = '';
            document.getElementById('passwordFieldA').disabled = true;
            document.getElementById('togglePasswordFieldA').disabled = true;
            dijit.byId('sd_key_usr').setValue('');
            dijit.byId('sd_key_usr').set('disabled', true);
            document.getElementById('passphraseFieldA').value = '';
            document.getElementById('passphraseFieldA').disabled = true;
            document.getElementById('togglePassphraseFieldA').disabled = true;
            dijit.byId('sd_key_file').setValue('');
            dijit.byId('sd_key_file').set('disabled', true);
        }
        dijit.byId('addHostsDlg').show();
    });
    dojo.connect(addButton, 'onMouseEnter', function (event) {
        // If LOCALHOST in list, disable.
        if (!clusterRunning) {
            dijit.byId('addHostsButton').setAttribute('disabled', false);
            mcc.storage.hostStorage().getItems({ anyHost: false }).then(function (items) {
                if (items && items.length > 0) {
                    for (var z in items) {
                        if (String(items[z].item['name']).toLowerCase() === 'localhost' ||
                        String(items[z].item['name']) === '127.0.0.1') {
                            dijit.byId('addHostsButton').setAttribute('disabled', true)
                            break;
                        }
                    }
                } else {
                    dijit.byId('addHostsButton').setAttribute('disabled', false);
                }
            });
        }
    });

    // Button for removing a host. Connect to storeDeleteHost function
    var removeButton = new dijit.form.Button({
        label: 'Remove selected host(s)',
        iconClass: 'dijitIconDelete',
        baseClass: 'fbtn'
    }, 'removeHostsButton');
    dojo.connect(removeButton, 'onClick', function () {
        // storage initializes config module, still maybe a good idea to do TRY here?.
        if (clusterRunning) {
            var msg = '';
            var what = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('clRunning', ['hosts']));
            if ((what || {}).text) {
                msg = what.text;
            }
            if (msg) {
                mcc.util.displayModal('I', 3, msg);
            } else {
                console.warn('Cluster is running, please stop it first!');
            }
            return;
        }
        // mcc.gui.stopStatusPoll('STOP');
        var selection = hostGrid.selection.getSelected();
        if (selection && selection.length > 0) {
            // Get the row index of the last item
            var lastIdx = 0;
            for (var i in selection) {
                lastIdx = hostGrid.getItemIndex(selection[i]);
                mcc.storage.hostStorage().deleteItem(selection[i]);
            }
            // If there is an item at lastIdx, select it, otherwise select first
            if (hostGrid.getItem(lastIdx)) {
                hostGrid.selection.setSelected(lastIdx, true);
            } else if (lastIdx > 0) {
                hostGrid.selection.setSelected(lastIdx - 1, true);
            }
        }
    });

    // Button for refreshing host information.
    var refreshButton = new dijit.form.Button({
        label: 'Refresh selected host(s)',
        iconClass: 'dijitIconNewTask',
        baseClass: 'fbtn'
    }, 'refreshHostsButton');
    dojo.connect(refreshButton, 'onClick', function () {
        // storage initializes config module, still maybe a good idea to do TRY here?.
        if (clusterRunning) {
            what = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('clRunning', ['hosts']));
            if ((what || {}).text) {
                console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                // Can not allow poking around parameters while Cluster is alive.
                mcc.util.displayModal('I', 3, what.text);
            }
            return;
        }
        var selection = hostGrid.selection.getSelected();
        var timeout;
        var stillFetching = false;
        function onTimeout () {
            for (var i in selection) {
                stillFetching = false;
                if (mcc.storage.hostStorage().store().getValue(selection[i], 'hwResFetch') === 'Fetching...') {
                    // still fetching
                    stillFetching = true;
                    break;
                }
            }
            if (stillFetching) {
                timeout = setTimeout(onTimeout, 500);
            } else {
                clearTimeout(timeout);
                removeProgressDialog();
            };
        }

        if (selection && selection.length > 0) {
            // Get the row index of the last item
            console.info('[INF]Running refresh on selection.');
            moveProgress('Refreshing hosts', 'Fetching host info.');
            for (var i in selection) {
                console.debug('[DBG]Refresh override is TRUE.');
                mcc.storage.getHostResourceInfo(selection[i],
                    mcc.storage.hostStorage().store().getValue(selection[i], 'id'),
                    false, true);
            }
            onTimeout();
        }
    });

    // Button for showing/hiding extended host information.
    var toggleInfoButton = new dijit.form.ToggleButton({
        label: 'Show extended info',
        iconClass: 'dijitIconUndo',
        baseClass: 'fbtn'
    }, 'toggleHostInfoButton');
    dojo.connect(toggleInfoButton, 'onChange', function (val) {
        if (val) {
            this.set('label', 'Hide extended info');
            hostGrid.layout.setColumnVisibility(8, true);
            hostGrid.layout.setColumnVisibility(9, true);
            hostGrid.layout.setColumnVisibility(10, true);
            hostGrid.layout.setColumnVisibility(11, true);
            hostGrid.layout.setColumnVisibility(12, true);
            hostGrid.layout.setColumnVisibility(13, true);
        } else {
            this.set('label', 'Show extended info');
            hostGrid.layout.setColumnVisibility(8, false);
            hostGrid.layout.setColumnVisibility(9, false);
            hostGrid.layout.setColumnVisibility(10, false);
            hostGrid.layout.setColumnVisibility(11, false);
            hostGrid.layout.setColumnVisibility(12, false);
            hostGrid.layout.setColumnVisibility(13, false);
        }
    });

    // Button for editing the host. Show Edit hosts dialog on click
    var editButton = new dijit.form.Button({
        label: 'Edit selected host',
        iconClass: 'dijitIconEdit',
        baseClass: 'fbtn'
    }, 'editHostsButton');
    dojo.connect(editButton, 'onClick', function () {
        // storage initializes config module, still maybe a good idea to do TRY here?.
        if (clusterRunning) {
            what = mcc.userconfig.setCcfgPrGen.apply(this,
                mcc.userconfig.setMsgForGenPr('clRunning', ['hosts']));
            if ((what || {}).text) {
                console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
                // Can not allow poking around parameters while Cluster is alive.
                mcc.util.displayModal('I', 3, what.text);
            }
            console.warn('[WRN]Cluster is running!');
            return;
        }
        var oldOpenfw = mcc.gui.getOpenFW();
        var oldInstallcluster = mcc.gui.getInstallCl();
        var clusterHasCreds = mcc.gui.getSSHPwd() || mcc.gui.getSSHUser() || mcc.gui.getSSHkeybased();

        // Check if selected Host has configured credentials already,
        // potentially, from using "Add Host" button.
        var selection = hostGrid.selection.getSelected();
        if (selection && selection.length > 1) {
            mcc.util.displayModal('I', 3, '<span style=""font-size:140%;color:orangered;>Please select ' +
                'one host at the time!</span>');
            return;
        }
        if (selection && selection.length > 0) {
            for (var i in selection) {
                dijit.byId('uname').setValue(mcc.storage.hostStorage().store().getValue(selection[i], 'uname'));
                dijit.byId('ram').setValue(mcc.storage.hostStorage().store().getValue(selection[i], 'ram'));
                dijit.byId('cores').setValue(mcc.storage.hostStorage().store().getValue(selection[i], 'cores'));
                // show installdir and datadir for editing
                dijit.byId('installdir').setValue(String(mcc.storage.hostStorage().store().getValue(selection[i], 'installdir')));
                dijit.byId('datadir').setValue(String(mcc.storage.hostStorage().store().getValue(selection[i], 'datadir')));
                dijit.byId('diskfree').setValue(mcc.storage.hostStorage().store().getValue(selection[i], 'diskfree'));

                var val = mcc.storage.hostStorage().store().getValue(selection[i], 'name');
                var hName = val.toLowerCase();
                if (val) {
                    dijit.byId('sd_ExtIPedit').setValue(val);
                    // No change in host ExternalIP!
                    dijit.byId('sd_ExtIPedit').set('disabled', true);
                } else {
                    dijit.byId('sd_ExtIPedit').setValue('');
                }

                val = mcc.storage.hostStorage().store().getValue(selection[i], 'internalIP');
                if (val) {
                    dijit.byId('sd_IntIPedit').setValue(val);
                } else {
                    dijit.byId('sd_IntIPedit').setValue('');
                }

                if (hName === 'localhost' || hName === '127.0.0.1' || !mcc.gui.getUseVPN()) {
                    // Forbid IntIP edit for localhost.
                    dijit.byId('sd_IntIPedit').set('disabled', true);
                    dijit.byId('sd_IntIPedit').setValue('');
                } else {
                    dijit.byId('sd_IntIPedit').set('disabled', false);
                }

                if (!clusterHasCreds) {
                    var hasCreds = false;
                    val = mcc.storage.hostStorage().store().getValue(selection[i], 'usr');
                    if (val) {
                        dijit.byId('sd_usredit').setValue(val);
                        hasCreds = true;
                    } else {
                        dijit.byId('sd_usredit').setValue('');
                    }
                    val = mcc.storage.hostStorage().store().getValue(selection[i], 'usrpwd');
                    if (val) {
                        document.getElementById('passwordField').value = val;
                        hasCreds = true;
                    } else {
                        document.getElementById('passwordField').value = '';
                    }

                    var valku = mcc.storage.hostStorage().store().getValue(selection[i], 'key_usr');
                    if (valku) {
                        dijit.byId('sd_key_usredit').setValue(valku);
                        hasCreds = true;
                    } else {
                        dijit.byId('sd_key_usredit').setValue('');
                    }
                    var valkp = mcc.storage.hostStorage().store().getValue(selection[i], 'key_passp');
                    if (valkp) {
                        document.getElementById('passphraseField').value = valkp;
                        hasCreds = true;
                    } else {
                        document.getElementById('passphraseField').value = '';
                    }
                    var valkf = mcc.storage.hostStorage().store().getValue(selection[i], 'key_file');
                    if (valkf) {
                        dijit.byId('sd_key_fileedit').setValue(valkf);
                        hasCreds = true;
                    } else {
                        dijit.byId('sd_key_fileedit').setValue('');
                    }
                    val = mcc.storage.hostStorage().store().getValue(selection[i], 'key_auth');
                    if (val || valku || valkp || valkf) {
                        dijit.byId('cbKeyauthEdit').setValue(val);
                        hasCreds = true;
                    } else {
                        dijit.byId('cbKeyauthEdit').setValue(false);
                    }
                    // Check if there are credentials at host level.
                    if (!hasCreds) {
                        console.info('[INF]No saved credentials for host %s. please enter.', hName);
                        dijit.byId('cbKeyauthEdit').setValue(false);
                        dijit.byId('sd_usredit').setValue('');
                        document.getElementById('passwordField').value = '';
                    }
                } else {
                    console.info('Cluster credentials in effect for host %s.', hName);
                    dijit.byId('cbKeyauthEdit').set('checked', false);
                    dijit.byId('cbKeyauthEdit').set('disabled', true);
                    dijit.byId('sd_key_fileedit').setValue('');
                    dijit.byId('sd_key_fileedit').set('disabled', true);
                    dijit.byId('sd_usredit').setValue('');
                    dijit.byId('sd_usredit').set('disabled', true);
                    document.getElementById('passwordField').value = '';
                    document.getElementById('passwordField').disabled = true;
                    dijit.byId('sd_key_usredit').setValue('');
                    dijit.byId('sd_key_usredit').set('disabled', true);
                    document.getElementById('passphraseField').value = '';
                    document.getElementById('passphraseField').disabled = true;
                    dijit.byId('cbKeyauthEdit').set('checked', false);
                    dijit.byId('cbKeyauthEdit').set('disabled', true);
                }
                // Set FW and installation from storage.
                // Looks redundant; check changes in MCCstorage.getHostResourceInfo.
                if (mcc.storage.hostStorage().store().getValue(selection[i], 'openfwhost')) {
                    dijit.byId('cbOpenfwHostEdit').setValue(
                        mcc.storage.hostStorage().store().getValue(selection[i], 'openfwhost'));
                } else {
                    if (isEmpty(oldOpenfw)) {
                        dijit.byId('cbOpenfwHostEdit').setValue(false);
                        oldOpenfw = false;
                    } else {
                        dijit.byId('cbOpenfwHostEdit').setValue(oldOpenfw);
                    }
                    mcc.storage.hostStorage().store().setValue(selection[i], 'openfwhost', oldOpenfw);
                }

                if (hName === 'localhost' || hName === '127.0.0.1') {
                    // Forbid OpenFW for localhost.
                    dijit.byId('cbOpenfwHostEdit').set('disabled', true);
                    dijit.byId('cbOpenfwHostEdit').setValue(false);
                    mcc.storage.hostStorage().store().setValue(selection[i], 'openfwhost', false);
                } else {
                    dijit.byId('cbOpenfwHostEdit').set('disabled', false);
                }

                if (mcc.storage.hostStorage().store().getValue(selection[i], 'installonhost')) {
                    dijit.byId('cbInstallonhostEdit').setValue(
                        mcc.storage.hostStorage().store().getValue(selection[i], 'installonhost'));
                } else {
                    if (isEmpty(oldInstallcluster)) {
                        dijit.byId('cbInstallonhostEdit').setValue(false);
                        oldInstallcluster = 'NONE';
                    } else {
                        dijit.byId('cbInstallonhost').setValue(oldInstallcluster !== 'NONE');
                    }
                    mcc.storage.hostStorage().store().setValue(selection[i], 'installonhost',
                        dijit.byId('cbInstallonhostEdit').get('checked'));
                }
                dijit.byId('sd_repoURL').setValue(mcc.storage.hostStorage().store().getValue(selection[i],
                    'installonhostrepourl'));
                dijit.byId('sd_dockerURL').setValue(mcc.storage.hostStorage().store().getValue(selection[i],
                    'installonhostdockerurl'));
                dijit.byId('sd_dockerNET').setValue(mcc.storage.hostStorage().store().getValue(selection[i],
                    'installonhostdockernet'));
                dijit.byId('sd_dockerSTATUS').setValue(mcc.storage.hostStorage().store().getValue(selection[i],
                    'dockerinfo'));
            }
        }

        var div = document.getElementById('RightContainer');
        if (dijit.byId('cbInstallonhostEdit').get('checked')) {
            div.style.visibility = 'visible';
        } else {
            div.style.visibility = 'hidden';
        }
        dijit.byId('editHostsDlg').show();
    });

    // Layout for the host grid. elasticView does not work!
    var hostGridDefinitions = [
        [{
            width: '12%',
            field: 'name',
            editable: false,
            name: 'Host'
        },
        {
            width: '7%',
            field: 'hwResFetch',
            formatter: function (value) {
                if (value !== 'OK') {
                    if (value === 'Failed') {
                        return '<div style="color:red;">' + value + '</div>';
                    } else {
                        return '<div style="color:royalblue;">' + value + '</div>';
                    }
                } else {
                    return value;
                }
            },
            editable: false,
            name: 'Res.info'
        },
        {
            width: '8%',
            field: 'uname',
            editable: !clusterRunning,
            name: 'Platform'
        },
        {
            width: '10%',
            field: 'ram',
            name: 'Memory (MB)',
            editable: !clusterRunning,
            type: dojox.grid.cells._Widget,
            widgetClass: 'dijit.form.NumberSpinner',
            constraint: { min: 1, max: 90000000, places: 0 }
        },
        {
            width: '6%',
            field: 'cores',
            name: 'Cores',
            editable: !clusterRunning,
            type: dojox.grid.cells._Widget,
            widgetClass: 'dijit.form.NumberSpinner',
            constraint: { min: 1, max: 5000, places: 0 }
        },
        {
            width: '24%',
            field: 'installdir',
            name: 'MySQL Cluster install directory',
            editable: !clusterRunning
        },
        {
            width: '24%',
            field: 'datadir',
            name: 'MySQL Cluster data directory',
            editable: !clusterRunning
        },
        {
            width: '9%',
            field: 'diskfree',
            name: 'DiskFree',
            editable: !clusterRunning
        }],
        [
            { name: 'Internal IP', field: 'internalIP', editable: mcc.gui.getUseVPN() && !clusterRunning, hidden: false, width: '10%' },
            { name: 'OS details',
                fields: ['osflavor', 'osver'],
                formatter: function (fields) {
                    var first = fields[0]; var last = fields[1];
                    return first + ', ver. ' + last;
                },
                editable: false,
                colSpan: 2,
                hidden: false,
                width: '16%' },
            { name: 'Open FW', field: 'openfwhost', type: dojox.grid.cells.Bool, editable: !clusterRunning, hidden: false, width: '10%' },
            { name: 'REPO URL', field: 'installonhostrepourl', editable: !clusterRunning, hidden: false, colSpan: '2', width: '30%' },
            { name: 'DOCKER URL', field: 'installonhostdockerurl', editable: !clusterRunning, hidden: false, width: '24%' },
            { name: 'Install', field: 'installonhost', type: dojox.grid.cells.Bool, editable: !clusterRunning, hidden: false, width: '10%' }
        ]
    ];// type: dojox.grid.cells.Bool/dijit.form.Checkbox NO
    // Validate user input. Needed if user types illegal values
    function applyCellEdit (inValue, inRowIndex, inAttrName) {
        // Enable V8 to show VM for this unit.
        if (clusterRunning) { return; };
        var rowItem = hostGrid.getItem(inRowIndex);
        for (var colId in rowItem) { // colId is field name.
            if (colId === inAttrName) {
                // Possibly add other checks as well
                mcc.storage.hostStorage().getItem(hostGrid._by_idx[inRowIndex].idty).then(function (host) {
                    // If updating directories, set predef flag to false
                    if (inAttrName.toLowerCase() === 'installdir' || inAttrName.toLowerCase() === 'datadir') {
                        console.debug('[DBG]Overriding ' + inAttrName + ', set flag');
                        host.setValue(inAttrName + '_predef', false);
                        inValue = mcc.util.terminatePath(inValue);
                    }
                    // If updating platform, update predef dirs too
                    if (inAttrName.toLowerCase() === 'uname') {
                        console.debug('[DBG]Update platform, check predef dirs');
                        var dir;
                        if (host.getValue('installdir_predef') === true) {
                            dir = mcc.storage.hostStorage().getPredefinedDirectory(inValue, 'installdir');
                            console.debug('[DBG]Update predfined installdir to ' + dir);
                            host.setValue('installdir', dir);
                        }
                        if (host.getValue('datadir_predef') === true) {
                            dir = mcc.storage.hostStorage().getPredefinedDirectory(inValue, 'datadir');
                            console.debug('[DBG]Update predfined datadir to ' + dir);
                            host.setValue('datadir', dir);
                        }
                    }

                    if (inAttrName.toLowerCase() === 'openfwhost' && inValue && (
                        host.getValue('name').toLowerCase() === 'localhost' ||
                        host.getValue('name') === '127.0.0.1')) {
                        // Forbid OpenFW for localhost.
                        console.debug('[DBG]Setting OpenFW to false for localhost.');
                        inValue = false;
                    }
                    host.setValue(inAttrName, inValue);
                    mcc.storage.hostStorage().save();
                    hostGrid.onApplyCellEdit(inValue, inRowIndex, inAttrName);
                });
                break;
            }
        }
    }
    // Define the host grid, don't show wildcard host
    hostGrid = new dojox.grid.EnhancedGrid({ // dojox.grid.DataGrid({
        autoHeight: true,
        query: { anyHost: false },
        queryOptions: {},
        canSort: function (col) { return false; },
        store: mcc.storage.hostStorage().store(),
        keepSelection: true,
        singleClickEdit: true,
        escapeHTMLInData: false,
        doApplyCellEdit: applyCellEdit,
        doStartEdit: function (cell, idx) {
            // Avoid inheriting previously entered value as default
            cell.widget = null; hostGrid.onStartEdit(cell, idx);
        },
        onBlur: function () {
            // Apply unsaved edits if leaving page
            if (hostGrid.edit.isEditing()) {
                console.info('Applying unsaved edit...');
                hostGrid.edit.apply();
            }
        },
        structure: hostGridDefinitions
    }, 'hostGrid');

    function showTT (event) {
        var editMsg = null;
        var colMsg = getFieldTT(event.cell.field);
        var msg;
        if (event.cell.editable) {
            editMsg = 'Click the cell for editing, or select one or more rows and press the edit button below the grid.';
        } else {
            editMsg = 'This cell cannot be edited.';
        }
        if (event.rowIndex < 0) {
            msg = colMsg;
        } else {
            msg = editMsg;
            // Since we introduced concatenated field (osflavor + osversion),
            // event.cell.field will fail (expectedly). So guarding it.
            try {
                if (!event.grid.store.getValue(event.grid.getItem(event.rowIndex), event.cell.field)) {
                    msg = 'Ellipsis (\'...\') means the value was not retrieved automatically. ' + msg;
                }
            } catch (e) {
                // Concatenated field is NOT editable anyway.
                msg = 'Ellipsis (\'...\') means the value was not retrieved automatically. ' + msg;
            }
        }
        if (msg) { dijit.showTooltip(msg, event.cellNode, ['after']); }
    }
    function hideTT (event) {
        dijit.hideTooltip(event.cellNode);
    }

    dojo.connect(hostGrid, 'onCellClick', function (e) {
        if (e.cell.field === 'hwResFetch') {
            mcc.storage.hostStorage().getItem(hostGrid._by_idx[e.rowIndex].idty).then(function (host) {
                var errMsg = host.getValue('errMsg');
                if (errMsg) {
                    mcc.util.displayModal('I', 0, '<span style=""font-size:140%;color:orangered;>Error: ' + errMsg);
                }
            });
        }
    });
    dojo.connect(hostGrid, 'onCellMouseOver', showTT);
    dojo.connect(hostGrid, 'onCellMouseOut', hideTT);
    dojo.connect(hostGrid, 'onHeaderCellMouseOver', showTT);
    dojo.connect(hostGrid, 'onHeaderCellMouseOut', hideTT);

    createTT(['hostGrid'], 'Click a cell in the grid to edit, or select one or more rows\
        and press the edit button below the grid. All cells except \
        <i>Host</i> and <i>Resource info</i> are editable.');

    hostGrid.startup();
    addHostsDialogSetup();
    editHostsDialogSetup();
    hostGrid.scrollToRow(0)
    // If there is an item at lastIdx, select it, otherwise select first
    if (hostGrid.getItem(0)) {
        hostGrid.selection.setSelected(0, true);
    }
    // Did not help with rendering :-/
    hostGrid.layout.setColumnVisibility(8, false);
    hostGrid.layout.setColumnVisibility(9, false);
    hostGrid.layout.setColumnVisibility(10, false);
    hostGrid.layout.setColumnVisibility(11, false);
    hostGrid.layout.setColumnVisibility(12, false);
    hostGrid.layout.setColumnVisibility(13, false);

    // disable widgets if cluster is running
    if (clusterRunning) {
        what = mcc.userconfig.setCcfgPrGen.apply(this,
            mcc.userconfig.setMsgForGenPr('clRunning', ['hosts']));
        if ((what || {}).text) {
            console.warn(what.text.replace(/<(?:.|\n)*?>/gm, '')); // plain text for console
            mcc.util.displayModal('I', 3, what.text);
        }
        // disable important components
        dijit.byId('hostGrid').setAttribute('disabled', true);
        dijit.byId('addHostsButton').setAttribute('disabled', true);
        dojo.style(dijit.byId('addHostsButton').domNode, 'display', 'none');
        dijit.byId('removeHostsButton').setAttribute('disabled', true);
        dojo.style(dijit.byId('removeHostsButton').domNode, 'display', 'none');
        dijit.byId('refreshHostsButton').setAttribute('disabled', true);
        dojo.style(dijit.byId('refreshHostsButton').domNode, 'display', 'none');
        dijit.byId('editHostsButton').setAttribute('disabled', true);
        dojo.style(dijit.byId('editHostsButton').domNode, 'display', 'none');
    }
}

/** ****************************** Initialize  *********************************/

dojo.ready(function initialize () {
    console.info('[INF]Host definition module initialized');
});
