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
 ***                                User choices                            ***
 ***                                    IE11                                ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.userconfig.userconfigjsIE
 *      Original: mcc.userconfig.userconfigjs
 *
 *  Description:
 *      Setup global variables based on users choices, IE11.
 *
 ******************************************************************************/

/******************************* Import/export ********************************/
dojo.provide('mcc.userconfig.userconfigjsIE');

/***************************** External interface *****************************/
mcc.userconfig.userconfigjsIE.setConfigFile = setConfigFile;
mcc.userconfig.userconfigjsIE.getConfigFile = getConfigFile;
mcc.userconfig.userconfigjsIE.setConfigFileContents = setConfigFileContents;
mcc.userconfig.userconfigjsIE.getConfigFileContents = getConfigFileContents;
mcc.userconfig.userconfigjsIE.getDefaultCfg = getDefaultCfg;
mcc.userconfig.userconfigjsIE.writeConfigFile = writeConfigFile;
mcc.userconfig.userconfigjsIE.setOriginalStore = setOriginalStore;
mcc.userconfig.userconfigjsIE.setIsNewConfig = setIsNewConfig;
mcc.userconfig.userconfigjsIE.getIsNewConfig = getIsNewConfig;
mcc.userconfig.userconfigjsIE.compareStores = compareStores;
mcc.userconfig.userconfigjsIE.isShadowEmpty = isShadowEmpty;
//-
mcc.userconfig.userconfigjsIE.getConfigProblems = getConfigProblems;
mcc.userconfig.userconfigjsIE.setCcfgPrGen = setCcfgPrGen;
mcc.userconfig.userconfigjsIE.setMsgForGenPr = setMsgForGenPr;
mcc.userconfig.userconfigjsIE.wasCfgStarted = wasCfgStarted;
mcc.userconfig.userconfigjsIE.setCfgStarted = setCfgStarted;

/******************************* Internal data  *******************************/
var configFile = '';
var configFileContents = '';
var defCfg1 = '{\n' +
    '\t"identifier": "id",\n' +
    '\t"label": "name",\n' +
    '\t"items": [\n' +
    '\t\t{\n' +
    '\t\t\t"id": 0,\n' +
    '\t\t\t"ssh_keybased": false,\n' +
    '\t\t\t"ssh_user": "",\n' +
    '\t\t\t"ssh_pwd": "",\n' +
    '\t\t\t"ssh_ClKeyFile": "",\n' +
    '\t\t\t"ssh_ClKeyUser": "",\n' +
    '\t\t\t"ssh_ClKeyPass": "",\n' +
    '\t\t\t"savepwds": false,\n' +
    '\t\t\t"name": "';
var defCfg2 = '",\n' +
    '\t\t\t"apparea": "simple testing",\n' +
    '\t\t\t"writeload": "medium",\n' +
    '\t\t\t"installCluster": "REPO",\n' +
    '\t\t\t"openfw": false,\n' +
    '\t\t\t"ClusterVersion": "8.0",\n' +
    '\t\t\t"FWHostPortPairs": "",\n' +
    '\t\t\t"MasterNode": "",\n' +
    '\t\t\t"Started": false,\n' +
    '\t\t\t"usevpn": false\n' +
    '\t\t}\n' +
    '\t]\n' +
    '},\n' +
    '{},\n' +
    '{},\n' +
    '{},\n' +
    '{}\n';
var defCfg = '';
var shadowClusterStore;
var shadowHostStore;
var shadowProcessStore;
var shadowProcesstypeStore;
var isNewConfig = false;
// to reduce number of pop-ups
var configProblemsGeneral = {
    items: []
};
var cfgStarted;

/******************************* Implementation *******************************/
//-
/**
 *Provides current timestamp.
 *
 * @returns String, in HH:mm:ss format
 */
function getTimeStamp () {
    var d = new Date();
    return '[' + ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2) + ':' +
        ('0' + d.getSeconds()).slice(-2) + ']';
}
/**
 *Insert config changed record into general log as it's never overwritten. If the text is
 * encountered in store, counter is incremented (no timestamp but for 1st entry)
 * If name member is empty, it's a signal entry is coming from configuration change.
 *
 * @param {string} name event name
 * @param {string} key store change occurred in
 * @param {string} text text of change
 * @param {string} lvl level (general, error, warning)
 */
function logConfigChange (name, key, text, lvl) {
    var found = false;
    for (var x in configProblemsGeneral.items) {
        // can't be at the beginning
        if (configProblemsGeneral.items[x].text.indexOf('::' + text) > 0) {
            found = true;
            configProblemsGeneral.items[x].count += 1;
            break;
        }
    }
    if (!found) {
        configProblemsGeneral.items.push({
            name: '', // signal it's from chang-log
            header: '',
            text: (key + ':' + name + '([' + lvl + '])@' + getTimeStamp() + '::' + text),
            key: '',
            level: lvl,
            onetime: false,
            displayed: false,
            count: 1
        });
    }
}

/**
 *Initialization of message structure for shadow stores
 *
 * @param {String} store name of store, cluster, host, process, processtype
 * @returns {JSON} empty message object for calling function to fill in and send to caller
 */
function formMessageBlock (store) {
    return {
        head: store,
        body: {
            general: {
                items: []
            },
            warning: {
                items: []
            },
            error: {
                items: []
            }
        }
    }
}

/**
 *Calls diff on store/shadow, provides result. If store == 'general', just returns what's
 * collected so far (general store does not reset during session).
 *
 * @param {String} store cluster, host, process, processtype or general
 * @returns {String} HTML formatted string with diffs
 */
function getConfigProblems (store) {
    switch (store) {
        case 'cluster':
            return compareStores('cluster')
        case 'host':
            return compareStores('host')
        case 'process':
            return compareStores('process')
        case 'processtype':
            return compareStores('processtype')
        case 'general':
            return configProblemsGeneral;
        default:
            return '';
    }
}
/**
 *When logging request arrives, this routine checks if it has the request already logged and if it
 * should display it or not. If request is new, new record is formed.
 *
 * @param {String} name given to request
 * @param {String} key Key info, say hostName of failing host etc.
 * if item not found, following members will serve to add it
 * @param {String} txt text member value, HTML formatted string
 * @param {String} lvl Level of message (general, warning or error)
 * @param {String} hdr header member value, HTML formatted string, if any
 * @param {String} onet message can be repeated, leave for later
 */
function setCcfgPrGen (name, key, txt, lvl, hdr, onet) {
    console.debug('Running setCcfgPrGen with %s, %s', name, key);
    if (name) { // key is optional && (key || '')
        for (var i in configProblemsGeneral.items) {
            if (configProblemsGeneral.items[i].name === name &&
                    configProblemsGeneral.items[i].key === key) {
                // we already have that one.
                if (configProblemsGeneral.items[i].displayed && configProblemsGeneral.items[i].onetime) {
                    console.debug('setCcfgPrGen returning {empty}');
                    return {};
                } else {
                    configProblemsGeneral.items[i].displayed = true;
                    console.debug('setCcfgPrGen returning {new}');
                    return {
                        header: configProblemsGeneral.items[i].header,
                        text: configProblemsGeneral.items[i].text,
                        key: configProblemsGeneral.items[i].key,
                        level: configProblemsGeneral.items[i].level
                    };
                }
            }
        }
        // if we're here, this is new entry into errors object
        configProblemsGeneral.items.push({
            name: name,
            header: hdr,
            text: txt,
            key: key,
            level: lvl,
            onetime: onet,
            displayed: true
        });
        // go ahead and display whatever
        console.debug('setCcfgPrGen returning {new object}');
        return {
            header: hdr,
            text: txt,
            key: key,
            level: lvl
        };
    } else {
        // bad call
        console.warn('Bad call to setCcfgPrGen!');
        return {};
    }
}
/**
 *Helper function to reduce clutter in other units. Returns string to be used
 *in setCcfgPrGen for messages that change with key (such as hostName etc).
 *
 * @param {String} msgID which message
 * @param {[String]} key array of replacement values
 * @returns {[String]} proper string to send to setCcfgPrGen
 */
function setMsgForGenPr (msgID, key) {
    switch (msgID) {
        case 'failedHWResFetch':
            return ['failedHWResFetch', key[0], 'Host ' + key[0] + ' is not properly set up!<br/>', 'warning', '', true];
        case 'failedHWResFetchDirect':
            return ['failedHWResFetchDirect', key[0],
                '<span style="color:orangered;>Automatic fetching resources for host ' + key[0] +
                ' with Cluster-wide credentials failed!!<br/></span>', 'warning', '', false];
        case 'pwdMissingHosts':
            return ['pwdMissingHosts', key[0], '\t\tordinary password for \t\t' + key[0] + '<br/>', 'warning', '', true];
        case 'ppMissingHosts':
            return ['ppMissingHosts', key[0], '\t\tSSHkey passphrase for \t\t' + key[0] + '<br/>', 'warning', '', true];
        case 'genHostErrors':
            // this is not in HTML, goes into alert.
            return ['genHostErrors', key[0], 'Host ' + key[0] + ':' + key[1] + '\n', 'warning', '', true];
        case 'clRunning':
            return ['clRunning', key[0], '<span style="font-size:140%;color:orangered">' +
                (key[0] ? '[' + key[0] + ']' : '') + 'Changes not allowed ' +
                'while Cluster is running!<br/>Please stop Cluster first.</span>', 'warning', '', true];
        case 'clStarted':
            return ['clStarted', key[0], '<span style="font-size:135%;color:orangered">Changing processes on ' +
            'already started configuration can lead to unpredictable results!<br/>Please make note to ' +
            '<strong>add DATA nodes in *pairs*</strong>.<br/>Please take care if adding MGMT ' +
            'nodes!<br/></span>', 'warning', '', true];
        case 'useDockerHost':
            return ['useDockerHost', key[0], '<span style="font-size:140%;color:orange">Will use DOCKER ' +
                'installation for host ' + [key] + '.</span>', 'warning', '', true];
        case 'useDockerHostNF':
            return ['useDockerHostNF', key[0], '<span style="font-size:140%;color:red">Docker installation ' +
                'not functional yet for host ' + [key] + '!</span>', 'warning', '', true];
        case 'useRepoHost':
            return ['useRepoHost', key[0], '<span style="font-size:140%;color:orange">' +
                'Will use REPO installation for host ' + [key] + '</span>', 'warning', '', true];
        case 'useRepoHostNF':
            return ['useRepoHostNF', key[0], '<span style="font-size:140%;color:red">Repo installation ' +
                'not functional yet for host ' + [key] + '!</span>', 'warning', '', true];
        case 'unsuppOS':
            return ['unsuppOS', key[0], '<span style="font-size:140%;color:orange">OS ' + [key] +
                ' not supported!</span>', 'warning', '', true];
        case 'unsuppInstPlat':
            return ['unsuppInstPlat', key[0], '<span style="font-size:140%;color:orange"' + [key] +
                ' platform not supported for installation!</span>', 'warning', '', true];
        case 'unsuppInstOS':
            return ['unsuppInstPlat', key[0], '<span style="font-size:140%;color:orange"' + [key] +
                ' not supported for installation!</span>', 'warning', '', true];
        case 'unsuppInstType':
            return ['unsuppInstType', key[0], '<span style="font-size:140%;color:orange"' + [key] +
                ' not supported for installation!</span>', 'warning', '', true];
        case 'hostsNotConn':
            if (key[0] === 'stopCluster') {
                return ['hostsNotConn', key[0], '<span style="font-size:140%;color:red">Proceeding with ' +
                    key[0] + ' although not all Cluster nodes are connected!<br/>Trying to shut down ' +
                    'what can be.</span>', 'warning', '', false];
            } else {
                return ['hostsNotConn', key[0], '<span style="font-size:140%;color:red">Can not proceed with ' +
                    key[0] + ' since not all Cluster nodes are connected!<br/>Please correct the ' +
                    'problem and refresh status either by moving from/to this page or by running' +
                    ' <strong>Connect remote hosts</strong> from Cluster tools.</span>', 'warning', '', false];
            }
        case 'badHostsExist':
            return ['badHostsExist', '', '<span style="font-size:140%;color:red">Some hosts are invalid.' +
                ' Please fetch resources or at least fill in OS and DATADIR manually.</span>', 'warning', '', true];
        case 'delTreeItem':
            return ['delTreeItem', 'processtree', '<span style="font-size:140%;color:orangered">Deleting ' +
                'MGMT/DATA process from Cluster can lead to invalid configuration especially since ' +
                'configuration was already started.</span>', 'info', true];
        case 'addTreeItem':
            return ['addTreeItem', 'processtree', '<span style="font-size:140%;color:orangered">Adding MGMT/DATA ' +
                'process from Cluster can lead to invalid configuration especially since configuration was already ' +
                'started.<br/>Most commonly, NoOfReplicas will change. To avoid failure, always add even number ' +
                'of Data nodes.</span>', 'info', true];
    }
}
/**
 *Writes current state of specified store into its shadow store. This happens after successful
 * Deploy or Start or when Shadow is empty. In each of these cases, it does not matter which
 * problems in configuration ShadowStore noted.
 * OPTIMIZE ME! We're only interested in ITEMS member, no need to fetch all.
 *
 * @param {String} storeName Which store to write to shadow (cluster, host, process, processtype)
 */
function setOriginalStore (storeName) {
    // not to be called too soon!
    console.debug('[DBG]Setting ' + storeName + 'Store shadow from original.');
    switch (storeName) {
        case 'cluster':
            if (!mcc.util.isEmpty(mcc.storage.clusterStorage().store()._getNewFileContentString())) {
                setCcfgPrGen('SHADOW change', 'Cluster', 'Cluster shadow reset at .' + getTimeStamp(),
                    'info', '', false)
                shadowClusterStore = JSON.parse(mcc.storage.clusterStorage().store()._getNewFileContentString());
            } else {
                console.debug('[DBG]NOT Filling in ' + storeName + ' shadow from store as store is empty.');
            }
            break;
        case 'host':
            if (!mcc.util.isEmpty(mcc.storage.hostStorage().store()._getNewFileContentString())) {
                setCcfgPrGen('SHADOW change', 'Host', 'Host shadow reset at .' + getTimeStamp(),
                    'info', '', false)
                shadowHostStore = JSON.parse(mcc.storage.hostStorage().store()._getNewFileContentString());
            } else {
                console.debug('[DBG]NOT Filling in ' + storeName + ' shadow from store as store is empty.');
            }
            break;
        case 'process':
            if (!mcc.util.isEmpty(mcc.storage.processStorage().store()._getNewFileContentString())) {
                setCcfgPrGen('SHADOW change', 'Process', 'Process shadow reset at .' +
                    getTimeStamp(), 'info', '', false)
                shadowProcessStore = JSON.parse(mcc.storage.processStorage().store()._getNewFileContentString());
            } else {
                console.debug('[DBG]NOT Filling in ' + storeName + ' shadow from store as store is empty.');
            }
            break;
        case 'processtype':
            if (!mcc.util.isEmpty(mcc.storage.processTypeStorage().store()._getNewFileContentString())) {
                setCcfgPrGen('SHADOW change', 'Families', 'Process families shadow reset at .' +
                    getTimeStamp(), 'info', '', false)
                shadowProcesstypeStore = JSON.parse(mcc.storage.processTypeStorage().store()._getNewFileContentString());
            } else {
                console.debug('[DBG]NOT Filling in ' + storeName + ' shadow from store as store is empty.');
            }
            break;
    }
}

/**
 *Utility function to reduce clutter in other units and traffic.
 *
 * @param {string} shadowName cluster, host, process, processtype
 * @returns {boolean} shadow empty or not
 */
function isShadowEmpty (shadowName) {
    switch (shadowName) {
        case 'cluster':
            return !(((shadowClusterStore || {}).items || []).length);
        case 'host':
            return !(((shadowHostStore || {}).items || []).length);
        case 'process':
            return !(((shadowProcessStore || {}).items || []).length);
        case 'processtype':
            return !(((shadowProcesstypeStore || {}).items || []).length);
    }
    return false;
}

/**
 * Compare Shadow with Store main routine.
 *
 * @param {string} storeName Store to compare with shadow copy (cluster, host, process, processtype)
 * @returns {JSON} see explanation in diffObjects
 */
function compareStores (storeName) {
    console.debug('[DBG]Comparing Shadow with ' + storeName + 'Store.');
    // we are just interesed in ITEMS array.
    var totalDif;
    var shadow;
    var working;
    switch (storeName) {
        case 'cluster':
            if (isShadowEmpty('cluster')) {
                console.debug('[DBG]compareStores:Shadow' + storeName + ' empty.');
                return {}
            };
            shadow = shadowClusterStore.items;
            if (!shadow) {
                // init to default
                setOriginalStore('cluster');
                shadow = shadowClusterStore.items;
            }

            // do not know which is faster but at least store went through JSON stuff already...
            working = JSON.parse(mcc.storage.clusterStorage().store()._getNewFileContentString()).items;
            if (!((working || []).length)) {
                console.debug('[DBG]compareStores:Original' + storeName + ' empty.');
                return {}
            };
            // pass by Ref.
            padObject(shadow, working);
            replacePlaceholders(shadow);
            replacePlaceholders(working);
            totalDif = diffObjects(shadow, working);
            return actualFromDiffObj('cluster', totalDif);

        case 'host':
            if (isShadowEmpty('host')) {
                console.debug('[DBG]compareStores:Shadow' + storeName + ' empty.');
                return {}
            };
            shadow = shadowHostStore.items;
            if (!shadow) {
                // init to default
                setOriginalStore('host');
                shadow = shadowHostStore.items;
            }

            working = JSON.parse(mcc.storage.hostStorage().store()._getNewFileContentString()).items;
            if (!((working || []).length)) {
                console.debug('[DBG]compareStores:Original' + storeName + ' empty.');
                return {}
            };
            // pass by Ref.
            padObject(shadow, working);
            replacePlaceholders(shadow);
            replacePlaceholders(working);
            totalDif = diffObjects(shadow, working);
            return actualFromDiffObj('host', totalDif);

        case 'process':
            if (isShadowEmpty('process')) {
                console.debug('[DBG]compareStores:Shadow' + storeName + ' empty.');
                return {}
            };
            shadow = shadowProcessStore.items;
            if (!shadow) {
                // init to default
                setOriginalStore('process');
                shadow = shadowProcessStore.items;
            }
            working = JSON.parse(mcc.storage.processStorage().store()._getNewFileContentString()).items;
            if (!((working || []).length)) {
                console.debug('[DBG]compareStores:Original' + storeName + ' empty.');
                return {}
            };
            // pass by Ref.
            padObject(shadow, working);
            replacePlaceholders(shadow);
            replacePlaceholders(working);
            totalDif = diffObjects(shadow, working);
            return actualFromDiffObj('process', totalDif);

        case 'processtype':
            if (isShadowEmpty('processtype')) {
                console.debug('[DBG]compareStores:Shadow' + storeName + ' empty.');
                return {}
            };
            shadow = shadowProcesstypeStore.items;
            if (!shadow) {
                // init to default
                setOriginalStore('processtype');
                shadow = shadowProcesstypeStore.items;
            }

            working = JSON.parse(mcc.storage.processTypeStorage().store()._getNewFileContentString()).items;
            if (!((working || []).length)) {
                console.debug('[DBG]compareStores:Original' + storeName + ' empty.');
                return {}
            };
            // pass by Ref.
            padObject(shadow, working);
            replacePlaceholders(shadow);
            replacePlaceholders(working);
            totalDif = diffObjects(shadow, working);
            return actualFromDiffObj('processtype', totalDif);
    }
}
/**
 *Get diff from two JSON arrays. In our case, it's two arrays of JSON objects.
 *
 * @param {JSON} obj1
 * @param {JSON} obj2
 * @returns {[JSON]} The difference from diffObjects is in that if item from obj1
 * does not exist in obj2 you will not see that in result. So here, if
 * obj1.items.length !== obj2.items.length, we need two calls with switched parameters.
 *
 */
function diffObjects (obj1, obj2) {
    var result = {};
    for (var key in obj1) {
        if (obj2[key] !== obj1[key]) result[key] = obj2[key];
        // eslint-disable-next-line valid-typeof
        if (typeof obj2[key] == 'array' && typeof obj1[key] == 'array') {
            // eslint-disable-next-line no-caller
            result[key] = arguments.callee(obj1[key], obj2[key]);
        }
        if (typeof obj2[key] == 'object' && typeof obj1[key] == 'object') {
            // eslint-disable-next-line no-caller
            result[key] = arguments.callee(obj1[key], obj2[key]);
        }
    }
    // now the other way around :-/
    for (key in obj2) {
        if (obj1[key] !== obj2[key]) result[key] = obj1[key];
        // eslint-disable-next-line valid-typeof
        if (typeof obj1[key] == 'array' && typeof obj2[key] == 'array') {
            // eslint-disable-next-line no-caller
            result[key] = arguments.callee(obj2[key], obj1[key]);
        }
        if (typeof obj1[key] == 'object' && typeof obj2[key] == 'object') {
            // eslint-disable-next-line no-caller
            result[key] = arguments.callee(obj2[key], obj1[key]);
        }
    }
    return result;
}

/**
 *Parse output from diffObjects to determine the extent and log the change appropriately.
 *
 * @param {string} storeName Store for which comparison with shadow was done (cluster, host, process, processtype)
 * @param {[JSON]} dif Output from diffObjects
 * @returns {String} message object of which we check ERRORS member for serious problems.
 */
function actualFromDiffObj (storeName, dif) {
    var result = '';
    var i;
    var key;
    var pType = -1;
    var name = '';
    var origVal = '';
    // remove passwords from logging
    var tmp = dif;
    for (i in tmp) {
        for (key in tmp[i]) {
            if (key === 'ssh_ClKeyPass' || key === 'ssh_pwd' ||
                key === 'key_passp' || key === 'usrpwd') {
                tmp[i][key] = '***';
            }
        }
    }
    // enable only in case below code fails console.info('[INF]Total diff between stores (%s) is %o', storeName, tmp);
    switch (storeName) {
        case 'cluster':
            var msg = formMessageBlock('cluster');
            if (dif) {
                // we can do without this FOR as there can be only one item in ClusterStore, 0.
                for (i in dif) {
                    // now fetch CHANGED values as they will have K/V set so iterable
                    for (key in dif[i]) {
                        if (dif[i].hasOwnProperty(key)) {
                            origVal = shadowClusterStore.items[i][key] || 'default value';
                            if (key === 'ssh_ClKeyPass' || key === 'ssh_pwd') {
                                // do not put passwords in plain sight
                                if (dif[i][key] !== undefined) {
                                    msg.body.general.items.push(
                                        {
                                            name: configFile.slice(0, -4),
                                            header: '',
                                            text: 'For ' + storeName.toUpperCase() + ' ' + configFile.slice(0, -4) +
                                                ', ' + key + ' changed to -> ***.<br/>',
                                            key: key,
                                            from: '*', // can't log passwords
                                            to: '*',
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        });
                                } else {
                                    msg.body.general.items.push(
                                        {
                                            name: configFile.slice(0, -4),
                                            header: '',
                                            text: 'For ' + storeName.toUpperCase() + ' ' + configFile.slice(0, -4) +
                                                ' value for ' + key + ' was deleted.<br/>',
                                            key: key,
                                            from: '*',
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        });
                                }
                            } else {
                                if (dif[i][key] !== undefined) {
                                    msg.body.general.items.push(
                                        {
                                            name: configFile.slice(0, -4),
                                            header: '',
                                            text: 'For ' + storeName.toUpperCase() + ' ' + configFile.slice(0, -4) +
                                                ', ' + key + ' changed from ' + origVal + ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    msg.body.general.items.push(
                                        {
                                            name: configFile.slice(0, -4),
                                            header: '',
                                            text: 'For ' + storeName.toUpperCase() + ' ' + configFile.slice(0, -4) +
                                                ' ' + key + ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                                logConfigChange('config change', 'Cluster store',
                                    msg.body.general.items.slice(-1)[0].text, 'info');
                            }
                            // only props we're interested in
                            if (['apparea', 'writeload', 'ClusterVersion', 'usevpn'].indexOf(key) >= 0) {
                                if (dif[i][key] !== undefined) {
                                    msg.body.error.items.push(
                                        {   // error because we probably need new deployment
                                            name: configFile.slice(0, -4),
                                            header: '',
                                            text: 'For Cluster:' + configFile.slice(0, -4) + ', ' +
                                                key + ' changed from ' + origVal + ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    // can't really be deleted but keep conformity with stores where that can happen
                                    msg.body.error.items.push(
                                        {
                                            name: configFile.slice(0, -4),
                                            header: '',
                                            text: 'For Cluster: ' + configFile.slice(0, -4) + ' ' + key +
                                                ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                            }
                        }
                    }
                }
            } else {
                msg.body.error.items.push(
                    {   // error because we probably need new deployment
                        name: configFile.slice(0, -4),
                        header: '',
                        text: '<span style="color:red;font-size:150%">We failed to compare ClusterStore and ' +
                            'shadow.</span><br/><br/>',
                        key: '',
                        from: '',
                        to: '',
                        added: false,
                        removed: false,
                        displayed: false
                    }
                );
            }
            break;
        case 'host':
            msg = formMessageBlock('host');
            if (dif) {
                for (i in dif) {
                    // now fetch CHANGED values as they will have K/V set so iterable
                    name = dif[i]['name'] || shadowHostStore.items[i]['name']
                    for (key in dif[i]) {
                        if (dif[i].hasOwnProperty(key)) {
                            origVal = shadowHostStore.items[i][key] || 'default value';
                            if (key === 'id' && dif[i][key] === undefined) {
                                msg.body.error.items.push(
                                    {
                                        name: 'HOST:' + name,
                                        // header: 'Host ' + name + ' deleted from configuration!<br/>',
                                        header: '',
                                        text: 'Deleting host <strong>' + name + '</strong> also means removing ' +
                                            'processes running on that hos.!<br/><br/>',
                                        key: '',
                                        from: origVal,
                                        to: '',
                                        added: false,
                                        removed: true,
                                        displayed: false
                                    }
                                );
                                logConfigChange('config change', 'Host store',
                                    msg.body.error.items.slice(-1)[0].text, 'info');
                                break;
                            }
                            if (key === 'id') {
                                msg.body.error.items.push(
                                    {
                                        name: 'HOST:' + name,
                                        header: '',
                                        text: 'Host id:' + dif[i][key] + ' ' + name + ' added to configuration!<br/>',
                                        key: 'id',
                                        from: '',
                                        to: dif[i][key],
                                        added: true,
                                        removed: false,
                                        displayed: false
                                    }
                                );
                                logConfigChange('config change', 'Host store',
                                    msg.body.error.items.slice(-1)[0].text, 'info');
                                break;
                            }
                            if (key === 'key_passp' || key === 'usrpwd') {
                                if (dif[i][key] !== undefined) {
                                    msg.body.general.items.push(
                                        {
                                            name: 'HOST:' + name,
                                            header: '',
                                            text: 'For host ' + name + ', ' + key + ' changed to -> ***.<br/>',
                                            key: key,
                                            from: '*', // can't log passwords
                                            to: '*',
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        });
                                } else {
                                    msg.body.general.items.push(
                                        {
                                            name: 'HOST:' + name,
                                            header: '',
                                            text: 'For host ' + name + ' value for ' + key + ' was deleted.<br/>',
                                            key: key,
                                            from: '*',
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        });
                                }
                            } else {
                                if (dif[i][key] !== undefined) {
                                    msg.body.general.items.push(
                                        {
                                            name: 'HOST:' + name,
                                            header: '',
                                            text: 'For host ' + name + ' value for ' + key +
                                                ' changed from ' + origVal + ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    msg.body.general.items.push(
                                        {
                                            name: 'HOST:' + name,
                                            header: '',
                                            text: 'For host ' + name + ' value for ' + key +
                                                ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                            }
                            logConfigChange('config change', 'Host store',
                                msg.body.general.items.slice(-1)[0].text, 'info');
                            // interesting props
                            if (['name', 'anyHost', 'internalIP', 'uname', 'datadir'].indexOf(key) >= 0) {
                                if (dif[i][key] !== undefined) {
                                    msg.body.error.items.push(
                                        {   // error because we probably need new deployment
                                            name: 'HOST:' + name,
                                            header: '',
                                            text: 'For host:' + name + ', ' + key + ' changed from ' + origVal +
                                                ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    // can't really be deleted but keep conformity with stores where that can happen
                                    msg.body.error.items.push(
                                        {
                                            name: 'HOST:' + name,
                                            header: '',
                                            text: 'For host: ' + name + ' ' + key + ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                            }
                        }
                    }
                }
            } else {
                msg.body.error.items.push(
                    {   // error because we probably need new deployment
                        name: configFile.slice(0, -4),
                        header: '',
                        text: '<span style="color:red;font-size:150%">We failed to compare HostStore and ' +
                            'shadow.</span><br/><br/>',
                        key: '',
                        from: '',
                        to: '',
                        added: false,
                        removed: false,
                        displayed: false
                    }
                );
            }
            break;
        case 'process':
            msg = formMessageBlock('process');
            if (dif) {
                for (i in dif) {
                    // now fetch CHANGED values as they will have K/V set so iterable
                    pType = -1;
                    name = dif[i]['name'] || shadowProcessStore.items[i]['name']
                    for (key in dif[i]) {
                        // If DELETED, dif[i][key] will just be 'undefined'.
                        if (dif[i].hasOwnProperty(key)) {
                            origVal = shadowProcessStore.items[i][key] || 'default value';
                            // deleted process
                            if (key === 'id') {
                                if (dif[i][key] === undefined) {
                                    pType = Number(shadowProcessStore.items[i]['processtype']);
                                } else {
                                    // so it's added process since all is defined
                                    pType = Number(dif[i]['processtype']);
                                }
                            }

                            if (key === 'id' && pType !== 4) {
                                result = ((dif[i][key] === undefined) ? 'Process id:' + dif[i][key] + ' ' +
                                    name + ' removed from' : 'Process id:' + dif[i][key] + ' ' + name + ' added to');
                                result += ' configuration.<br/><br/>';
                                if (dif[i][key] !== undefined) {
                                    msg.body.error.items.push(
                                        {   // error because we probably need new deployment
                                            name: 'PROCESS:' + name,
                                            header: '',
                                            text: result,
                                            key: key,
                                            from: '',
                                            to: dif[i][key],
                                            added: true,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    // can't really be deleted but keep conformity with stores where that can happen
                                    msg.body.error.items.push(
                                        {
                                            name: 'PROCESS:' + name,
                                            header: '',
                                            text: result,
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                                logConfigChange('config change', 'Process store',
                                    msg.body.error.items.slice(-1)[0].text, 'info');
                                break;
                            } else {
                                if (key === 'id' && pType === 4) {
                                    if (dif[i][key] !== undefined) {
                                        msg.body.general.items.push(
                                            {
                                                name: 'PROCESS:' + name,
                                                header: '',
                                                text: 'API process ' + name + ' added.<br/>',
                                                key: key,
                                                from: '',
                                                to: dif[i][key],
                                                added: true,
                                                removed: false,
                                                displayed: false
                                            }
                                        );
                                    } else {
                                        // can't really be deleted but keep conformity with stores where that can happen
                                        msg.body.general.items.push(
                                            {
                                                name: 'PROCESS:' + name,
                                                header: '',
                                                text: 'API process ' + name + ' deleted.<br/>',
                                                key: key,
                                                from: origVal,
                                                to: '',
                                                added: false,
                                                removed: true,
                                                displayed: false
                                            }
                                        );
                                    }
                                    // no need to list all members of deleted process item
                                    logConfigChange('config change', 'Process store',
                                        msg.body.general.items.slice(-1)[0].text, 'info');
                                    break;
                                };
                            }

                            // log everything else now (deleted/added process is dealt with)
                            if (dif[i][key] !== undefined) {
                                msg.body.general.items.push(
                                    {
                                        name: 'PROCESS:' + name,
                                        header: '',
                                        text: 'For process:' + name + ', ' + key + ' changed from ' +
                                            origVal + ' to -> ' + dif[i][key] + '<br/>',
                                        key: key,
                                        from: origVal,
                                        to: dif[i][key],
                                        added: false,
                                        removed: false,
                                        displayed: false
                                    }
                                );
                            } else {
                                // can't really be deleted but keep conformity with stores where that can happen
                                msg.body.general.items.push(
                                    {
                                        name: 'PROCESS:' + name,
                                        header: '',
                                        text: 'For process: ' + name + ', ' + key +
                                            ' is deleted (was ' + origVal + ')<br/>',
                                        key: key,
                                        from: origVal,
                                        to: '',
                                        added: false,
                                        removed: true,
                                        displayed: false
                                    }
                                );
                            }
                            // plain text to general log.
                            logConfigChange('config change', 'Process store',
                                msg.body.general.items.slice(-1)[0].text, 'info');
                            // if _name_ has changed but processtype didn't, it means someone edited
                            // configuration by hand!
                            if (['host', 'processtype', 'NodeId'].indexOf(key) >= 0 && pType !== 4) {
                                if (dif[i][key] !== undefined) {
                                    msg.body.error.items.push(
                                        {   // error because we probably need new deployment
                                            name: 'PROCESS:' + name,
                                            header: '',
                                            text: 'Process:' + name + ', ' + key + ' changed from ' + origVal + ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    // can't really be deleted but keep conformity with stores where that can happen
                                    msg.body.error.items.push(
                                        {
                                            name: 'PROCESS:' + name,
                                            header: '',
                                            text: 'Process: ' + name + ' ' + key + ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                            }
                            // And now definitions on *process* level, like Port for mysqld process.
                            // Thus excluding automatic properties. There is no additional props for API.
                            if (['id', 'name', 'host', 'processtype', 'NodeId', 'seqno'].indexOf(key) < 0) {
                                if (dif[i][key] !== undefined) {
                                    msg.body.error.items.push(
                                        {   // error because we probably need new deployment
                                            name: 'PROCESS:' + name,
                                            header: '',
                                            text: 'For process:' + name + ', ' + key + ' changed from ' + origVal +
                                                ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    msg.body.error.items.push(
                                        {
                                            name: 'PROCESS:' + name,
                                            header: '',
                                            text: 'For process: ' + name + ', ' + key + ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                            }
                        } else {
                            console.warn('[WRN]processStore dif, can\'t find key ' + key);
                        }
                    }
                }
            } else {
                msg.body.error.items.push(
                    {   // error because we can't tell if we need new deployment
                        name: configFile.slice(0, -4),
                        text: '<span style="color:red;font-size:150%">We failed to compare ' +
                            'ProcessStore and shadow.</span><br/><br/>',
                        key: '',
                        from: '',
                        to: '',
                        added: false,
                        removed: false,
                        displayed: false
                    }
                );
            }
            break;
        case 'processtype':
            msg = formMessageBlock('processtype');
            if (dif) {
                pType = -1;
                for (i in dif) {
                    name = dif[i]['familyLabel'] || shadowProcesstypeStore.items[i]['familyLabel']
                    // now fetch CHANGED values as they will have K/V set so iterable
                    for (key in dif[i]) {
                        if (dif[i].hasOwnProperty(key)) {
                            origVal = shadowProcesstypeStore.items[i][key] || 'default value';
                            if (key === 'id') {
                                if (dif[i][key] === undefined) {
                                    pType = Number(shadowProcesstypeStore.items[i]['id']);
                                } else {
                                    // so it's added key since all is defined
                                    pType = Number(dif[i]['id']);
                                }
                            }
                            if (dif[i][key] !== undefined) {
                                msg.body.general.items.push(
                                    {
                                        name: 'FAMILY:' + name,
                                        header: '',
                                        text: 'For process family:' + name + ', ' + key +
                                            ' changed from ' + origVal + ' to -> ' + dif[i][key] + '<br/>',
                                        key: key,
                                        from: origVal,
                                        to: dif[i][key],
                                        added: false,
                                        removed: false,
                                        displayed: false
                                    }
                                );
                            } else {
                                // can't really be deleted but keep conformity with stores where that can happen
                                msg.body.general.items.push(
                                    {
                                        name: 'FAMILY:' + name,
                                        header: '',
                                        text: 'For process family:' + name + ', ' + key +
                                            ' is deleted (was ' + origVal + ')<br/>',
                                        key: key,
                                        from: origVal,
                                        to: '',
                                        added: false,
                                        removed: true,
                                        displayed: false
                                    }
                                );
                            }
                            logConfigChange('config change', 'Process family store',
                                msg.body.general.items.slice(-1)[0].text, 'info');
                            // None of these options change apart from currSeq which is irrelevant.
                            // There is no additional props for API but there might be later.
                            // However, it might be someone changed it by hand *during* MCC run so
                            // probably needs some sort of safeguard...
                            if (['id', 'name', 'family', 'familyLabel', 'nodeLabel', 'minNodeId',
                                'maxNodeId', 'currSeq'].indexOf(key) < 0 && pType !== 4) {
                                if (dif[i][key] !== undefined) {
                                    msg.body.error.items.push(
                                        {   // error because we need new deployment
                                            name: 'FAMILY:' + name,
                                            header: '',
                                            text: 'For process family:' + name + ', ' + key +
                                                ' changed from ' + origVal + ' to -> ' + dif[i][key] + '<br/>',
                                            key: key,
                                            from: origVal,
                                            to: dif[i][key],
                                            added: false,
                                            removed: false,
                                            displayed: false
                                        }
                                    );
                                } else {
                                    msg.body.error.items.push(
                                        {
                                            name: 'FAMILY:' + name,
                                            header: '',
                                            text: 'For process family: ' + name + ' ' + key +
                                                ' is deleted (was ' + origVal + ')<br/>',
                                            key: key,
                                            from: origVal,
                                            to: '',
                                            added: false,
                                            removed: true,
                                            displayed: false
                                        }
                                    );
                                }
                            }
                        }
                    }
                }
            } else {
                msg.body.error.items.push(
                    {   // error because we can't tell if we need new deployment
                        name: configFile.slice(0, -4),
                        text: '<span style="color:red;font-size:150%">We failed to compare ProcessTypeStore and shadow.</span><br/><br/>',
                        key: '',
                        from: '',
                        to: '',
                        added: false,
                        removed: false,
                        displayed: false
                    }
                );
            }
    }
    return msg;
}
/**
 *Finds index of property:value pair in array of JSON objects.
 *
 * @param {[JSON]} data Array of JSON objects to scan.
 * @param {string} property Keyname to look for.
 * @param {any} value Value to look for.
 * @returns {number} Index of K/V pair in [JSON] or -1 if not found
 */
function findIndexInData (data, property, value) {
    var result = -1;
    data.some(function (item, i) {
        if (item[property] === value) {
            result = i;
            return true;
        }
    });
    return result;
}
/**
 *During padding of [JSON] objects we need actual key field to be present. After padding is done,
 *we replace entries {id: keyvalue} with {} to emphasize entire item was removed from configuration.
 *
 * @param {[JSON]} obj
 */
function replacePlaceholders (obj) {
    // replace {id: nn} with {}
    for (var wIndex in obj) {
        if (!obj[wIndex].hasOwnProperty('name')) {
            obj.splice(wIndex, 1, {});
        }
    }
}

/**
 *Pads two arrays of JSON objects so they are the same length minding the key value. Arrays are
 *passed by reference so there is no return value as they are modified here.
 *
 * @param {[JSON]} obj1 Shadow.items
 * @param {[JSON]} obj2 Store.items
 */
function padObject (obj1, obj2) {
    var sID = -1;
    var wID = -1;
    var witem;
    var index = 0;
    var ndx = -1;
    while (true) {
        if (index > obj1.length - 1 && index <= obj2.length - 1) {
            // item from the bottom of obj2 missing in obj1
            wID = obj2[index]['id'];
            console.debug('[DBG]OBJ1::Adding ID:' + wID + ' at ' + index);
            obj1.splice(index, 0, { id: wID });
            continue; // for same index!
        }

        if (index <= obj1.length - 1 && index > obj2.length - 1) {
            // item from the bottom of obj1 missing in obj2
            sID = obj1[index]['id'];
            console.debug('[DBG]OBJ2::Adding ID:' + sID + ' at ' + index);
            obj2.splice(index, 0, { id: sID });
            continue; // for same index!
        }

        if (index > obj1.length - 1 || index > obj2.length - 1) {
            break;
        }

        sID = obj1[index]['id'];
        wID = obj2[index]['id'];
        if (sID === wID) {
            index++;
            continue;
        } else {
            if (sID < wID) {
                ndx = findIndexInData(obj2, 'id', sID);
                if (ndx === -1) {
                    console.debug('[DBG]Adding placeholder to obj2 at index ' + index + ', ID:' + sID);
                    obj2.splice(index, 0, { id: sID });
                    continue; // for same index!
                } else {
                    console.debug('[DBG]OBJ2, moving item at index ' + ndx + ', ID:' + wID +
                        ' to position ' + index);
                    witem = obj2[ndx];
                    obj2.splice(ndx, 1);
                    obj2.splice(index, 0, witem);
                    continue; // for same index!
                }
            } else {
                ndx = findIndexInData(obj1, 'id', wID);
                if (ndx === -1) {
                    console.debug('[DBG]Adding placeholder to obj1 at index ' + index + ', ID:' + wID);
                    obj1.splice(index, 0, { id: wID });
                    continue; // for same index!
                } else {
                    console.debug('[DBG]OBJ1, moving item at index ' + ndx + ', ID:' + sID +
                        ' to position ' + index);
                    witem = obj1[ndx];
                    obj1.splice(ndx, 1);
                    obj1.splice(index, 0, witem);
                    continue; // for same index!
                }
            }
        }
    };
}

/**
 *set status of loaded config
 *
 * @param {boolean} setTo new status of loaded config
 */
function setIsNewConfig (setTo) {
    isNewConfig = setTo;
}
/**
 *Is loaded config new?
 *
 * @returns {boolean} loaded config status
 */
function getIsNewConfig () {
    return isNewConfig;
}

/**
 *Saves user's choice of config file in globally available place which is then used
 *to set ClusterName and to pass config name to back end for saving the changes.
 *
 * @param {string} fnm   chosen file name
 */
function setConfigFile (fnm) {
    configFile = fnm;
    console.info('[INF]CF set to: ' + configFile);
    if (defCfg === '') {
        defCfg = defCfg1 + configFile.slice(0, -4) + defCfg2; // Remove .mcc part.
    }
}

/**
 *Utility function to provide configuration read from back end.
 *
 * @returns {string} configuration read from back end in JSON format
 */
function getConfigFile () {
    return configFile;
}

/**
 *Utility function to provide default configuration. Called when new configuration is requested.
 *
 * @returns {string} default configuration in JSON format
 */
function getDefaultCfg () {
    return defCfg;
}

/**
 *Utility function to fill global variable with contents of configuration file in JSON format.
 *
 * @param {JSON} cfc   configuration contents to set
 */
function setConfigFileContents (cfc) {
    configFileContents = cfc;
    console.info('[SET]CFC.length is: ' + configFileContents.length);
}

/**
 *Utility function to provide contents of configuration file to other units.
 *
 * @returns {string} contents of configuration file in JSON format
 */
function getConfigFileContents () {
    console.info('[GET]CFC.length is: ' + configFileContents.length);
    return configFileContents;
}

/**
 *XHRPost
 *
 * @param {JSON} msg   POST message block to pass to back end.
 * @returns {dojo.Deferred}
 */
function doPost (msg) {
    // Convert to json string
    var jsonMsg = dojo.toJson(msg);
    // Return deferred from xhrPost
    return dojo.xhrPost({
        url: '/cmd',
        headers: { 'Content-Type': 'application/json' },
        postData: jsonMsg,
        handleAs: 'json'
    });
}

/**
 *Generic error handler closure.
 *
 * @param {JSON} req request message block
 * @param {function} onError
 * @returns
 */
function errorHandler (req, onError) {
    if (onError) {
        return onError;
    } else {
        return function (error) {
            console.error('[ERR]An error occurred while executing \'' + req.cmd + ' (' + req.seq + ')\': ' + error);
        };
    }
}

/**
 *Generic reploy handler to XHRPost message. Generally a good place to reconsider what's failure.
 *
 * @param {function} onReply   Reply response function
 * @param {function} onError   Error response function
 * @returns
 */
function replyHandler (onReply, onError) {
    return function (reply) {
        var ErrM = '';
        if (reply && reply.stat) {
            // reply.stat member is there only in case of an error.
            ErrM = reply.stat.errMsg + '';
        } else { ErrM = 'OK'; }
        if (reply && reply.stat && ErrM !== 'OK') {
            if (onError) {
                onError(reply.stat.errMsg, reply);
            } else {
                mcc.util.displayModal('I', 3, '<span style="font-size:135%;color:orangered;">' +
                    ErrM + '</span>');
            }
        } else {
            onReply(reply);
        }
    };
}

/**
 *Sends POST message composed in writeConfigFile to execution.
 *
 * @param {JSON} message POST message
 * @param {function} onReply Reply handler
 * @param {function} onError Error handler
 */
function createCfgFileReq (message, onReply, onError) {
    doPost(message).then(replyHandler(onReply, onError), errorHandler(message.head, onError));
}

/**
 *Forms command POST message to write configuration file to [HOME]/.mcc on box where back end runs.
 *
 * @param {String} contents
 * @param {String} fileName
 */
function writeConfigFile (contents, fileName) {
    var res2 = new dojo.Deferred();
    console.debug('[DBG]Writing to config file.');
    // Fill new config with predefined defaults:
    var msg = {
        head: { cmd: 'createCfgFileReq', seq: 1 },
        body: {
            ssh: { keyBased: false, user: '', pwd: '' },
            hostName: 'localhost',
            path: '~',
            fname: fileName,
            contentString: contents,
            phr: ''
        }
    };
    createCfgFileReq(
        msg,
        function () {
            console.info('[INF]Configuration written.');
            res2.resolve(true);
            return res2;
        },
        function (errMsgFNC) {
            mcc.util.displayModal('I', 0, '<span style="font-size:135%;color:orangered;">' +
                'Unable to write config file ' + fileName + ' in HOME directory: ' + errMsgFNC + '</span>');
            // This is bad, bail out and just work from tree.
            fileName = '';
            res2.resolve(false);
            return res2;
        }
    );
    res2.then(function (success1) {
        console.info('[INF]createCfgFileReq success: ', success1);
        return true;
    }, function (error1) {
        console.error('[ERR]createCfgFileReq error: ', error1);
        return false;
    }).then(function (info1) {
        console.debug('[DBG]createCfgFileReq has finished with success status: ', info1);
        if (info1) {
            setCcfgPrGen('CONFIG file', '', ('Configuration written to disk at ' + getTimeStamp()), 'info', '', false)
            console.debug('[DBG]writeConfigFile done.');
            return 'OK';
        } else {
            return 'FAILED';
        }
    });
}
/**
 *Returns ClusterLvL.Started value from configuration file.
 *
 * @returns {Boolean} global cfgStarted
 */
function wasCfgStarted () {
    return cfgStarted;
}
/**
 *Returns ClusterStore.Started state
 *
 * @param {Boolean} res
 */
function setCfgStarted (res) {
    cfgStarted = res;
}
/********************************* Initialize *********************************/
dojo.ready(function () {
    console.info('[INF]UserconfigIE class module initialized');
});
