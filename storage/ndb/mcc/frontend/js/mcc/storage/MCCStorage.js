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
 ***                      Storage instances for MCC data                    ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.storage.MCCStorage
 *
 *  Description:
 *      Storage instances wrapping the mcc stores
 *
 *  External interface:
 *      mcc.storage.MCCStorage.initialize: Intitialize storages
 *      mcc.storage.MCCStorage.clusterStorage: Get cluster storage;
 *      mcc.storage.MCCStorage.processTypeStorage: Get process type storage
 *      mcc.storage.MCCStorage.processStorage: Get process storage
 *      mcc.storage.MCCStorage.processTreeStorage: Get process tree storage
 *      mcc.storage.MCCStorage.getHostResourceInfo: Fetch resource information
 *      mcc.storage.MCCStorage.getHostDockerInfo: Fetch Docker information
 *      mcc.storage.MCCStorage.hostStorage: Get host storage
 *      mcc.storage.MCCStorage.hostTreeStorage: Get host tree storage
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *      newProcessTypeItem: For new process types: Add to process tree
 *      setProcessValue: If updating process, check if tree should be updated
 *      newProcessItem: For new processes: Add to host tree and process tree
 *      deleteProcessItem: Delete process from host tree and process tree too
 *      debugProcessTree: Special debug output for process tree
 *      setHostValue: If updating value, check if tree should be updated
 *      newHostItem: For new hosts: Get hw details and add to host tree storage
 *      deleteHostItem: Delete host/processes from host- and hostTreeStorage
 *      debugHostTree: Special debug output for host tree
 *      initializeHostTreeStorage: Re-create host tree based on hosts/processes
 *      initializeProcessTreeStorage: Re-create tree based on processes/types
 *      initializeProcessTypeStorage: Add default cluster processes if necessary
 *      mcc.storage.MCCStorage.isOPC: Check if deployment is on Oracle cloud infrastructure.
 *
 *  Internal data:
 *      clusterStorage: Storage for cluster
 *      processTypeStorage: Storage for process types
 *      processStorage: Storage for processes
 *      processTreeStorage: Storage for process tree
 *      hostStorage: Storage for hosts
 *      hostTreeStorage: Storage for host tree
 *      initializeStorageId: Initialize nextId based on storage contents
 *
 *  Unit test interface:
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *
 ******************************************************************************/

/******************************* Import/export ********************************/
dojo.provide('mcc.storage.MCCStorage');

dojo.require('dojo.DeferredList');

dojo.require('mcc.util');
dojo.require('mcc.storage.stores');
dojo.require('mcc.storage.Storage');
dojo.require('mcc.server');
dojo.require('mcc.configuration');

/***************************** External interface *****************************/
mcc.storage.MCCStorage.initialize = initialize;

mcc.storage.MCCStorage.clusterStorage = getClusterStorage;
mcc.storage.MCCStorage.processTypeStorage = getProcessTypeStorage;
mcc.storage.MCCStorage.processStorage = getProcessStorage;
mcc.storage.MCCStorage.processTreeStorage = getProcessTreeStorage;
mcc.storage.MCCStorage.getHostResourceInfo = getHostResourceInfo;
mcc.storage.MCCStorage.getHostDockerInfo = getHostDockerInfo;
mcc.storage.MCCStorage.hostStorage = getHostStorage;
mcc.storage.MCCStorage.hostTreeStorage = getHostTreeStorage;

/******************************** Internal data *******************************/
var clusterStorage = null;
var processTypeStorage = null;
var processStorage = null;
var processTreeStorage = null;
var hostStorage = null;
var hostTreeStorage = null;

/******************************* Implementation *******************************/

/******************************* Cluster storage ******************************/
function getClusterStorage () {
    if (!clusterStorage) {
        clusterStorage = new mcc.storage.Storage({
            name: 'Cluster storage',
            store: mcc.storage.stores.clusterStore
        });
    }
    return clusterStorage;
}

/**************************** Process type storage ****************************/
// Special handling for new process types: Add to process tree
function newProcessTypeItem (item) {
    this.inherited(arguments);
    processTreeStorage.getItems({ name: item.familyLabel }).then(function (items) {
        if (!items || items.length === 0) {
            processTreeStorage.newItem({
                id: item.id,
                type: 'processtype',
                name: item.familyLabel
            });
            processTreeStorage.save();
        }
    });
}

function getProcessTypeStorage () {
    if (!processTypeStorage) {
        processTypeStorage = new mcc.storage.Storage({
            name: 'Process type storage',
            store: mcc.storage.stores.processTypeStore,
            newItem: newProcessTypeItem
        });
    }
    return processTypeStorage;
}

/****************************** Process storage *******************************/
// If updating value, check if tree should be updated
function setProcessValue (process, attr, val) {
    this.inherited(arguments);
    attr += '';
    // Name updates should be propagated
    if (attr === 'name' || attr === 'status') {
        hostTreeStorage.getItem(process.getId()).then(function (treeProc) {
            treeProc.setValue('name', process.getValue('name'));
            hostTreeStorage.save();
        });
        processTreeStorage.getItem(process.getId()).then(function (treeProc) {
            treeProc.setValue('name', process.getValue('name'));
            if (process.getValue('status')) {
                treeProc.setValue('status', process.getValue('status'));
            }
            processTreeStorage.save();
        });
    }
}

// Special handling for new processes: Add to host tree and process tree
function newProcessItem (item) {
    // Add the new process instance to the processStorage
    this.inherited(arguments);
    // Fetch the prototypical process type from the process type storage
    processTypeStorage.getItem(item.processtype).then(function (ptype) {
        processTypeStorage.getItems({ family: ptype.getValue('family') }).then(function (pfam) {
            // Update sequence number
            var currSeq = pfam[0].getValue('currSeq');
            pfam[0].setValue('currSeq', ++currSeq);
            processTypeStorage.save();

            // Get the appropriate process type tree item
            processTreeStorage.getItem(pfam[0].getId()).then(function (treeptype) {
                // Add as child in process tree
                processTreeStorage.newItem({ id: item.id, type: 'process', name: item.name },
                    { parent: treeptype.item, attribute: 'processes' });
                processTreeStorage.save();
            });
        });
    });

    // Fetch the host from the host tree
    hostTreeStorage.getItem(item.host).then(function (treehost) {
        // Add the new process to hostTreeStore as host child
        hostTreeStorage.newItem({ id: item.id, type: 'process', name: item.name },
            { parent: treehost.item, attribute: 'processes' });
    });
}

// Delete the given process from processStorage as well as trees
function deleteProcessItem (item) {
    var processId = null;
    // Get process id depending on item type
    if (item.constructor === this.StorageItem) {
        processId = item.getId();
    } else {
        processId = processStorage.store().getValue(item, 'id');
    }
    // Reset all predefined parameter values for this id
    mcc.configuration.resetDefaultValueInstance(processId);
    // Delete process from processStorage
    this.inherited(arguments);
    // Fetch the process entry from the process tree and delete
    processTreeStorage.getItem(processId).then(function (treeItem) {
        treeItem.deleteItem();
        // If selected in the process or deployment tree, reset
        if (mcc.gui.getCurrentProcessTreeItem().storageItem &&
            treeItem.getId() === mcc.gui.getCurrentProcessTreeItem().storageItem.getId()) {
            mcc.gui.resetProcessTreeItem();
        }
        if (mcc.gui.getCurrentDeploymentTreeItem().storageItem &&
            treeItem.getId() === mcc.gui.getCurrentDeploymentTreeItem().storageItem.getId()) {
            mcc.gui.resetDeploymentTreeItem();
        }
    });
    // Fetch the process entry from the host tree and delete
    hostTreeStorage.getItem(processId).then(function (treeItem) {
        treeItem.deleteItem();
        // If selected in the host tree, reset
        if (mcc.gui.getCurrentHostTreeItem().storageItem &&
            treeItem.getId() === mcc.gui.getCurrentHostTreeItem().storageItem.getId()) {
            mcc.gui.resetHostTreeItem();
        }
    });
}

function getProcessStorage () {
    if (!processStorage) {
        processStorage = new mcc.storage.Storage({
            name: 'Process storage',
            store: mcc.storage.stores.processStore,
            setValue: setProcessValue,
            newItem: newProcessItem,
            deleteItem: deleteProcessItem
        });
    }
    return processStorage;
}

/**************************** Process tree storage ****************************/
function debugProcessTree () {
    console.debug('[DBG]' + this.name + ' contents:');
    var that = this;
    this.forItems({}, function (item) {
        processTypeStorage.ifItemId(item.getId(), function () {
            console.debug('   ' + item.getId() + ': process type');
            var processes = item.getValues('processes');
            for (var i in processes) {
                console.debug('    + ' + that.store().getIdentity(processes[i]));
            }
        });
    });
}

function getProcessTreeStorage () {
    if (!processTreeStorage) {
        processTreeStorage = new mcc.storage.Storage({
            name: 'Process tree storage',
            store: mcc.storage.stores.processTreeStore,
            debug: debugProcessTree
        });
    }
    return processTreeStorage;
}

/******************************** Host storage ********************************/
// If updating value, check if tree should be updated
function setHostValue (host, attr, val) {
    this.inherited(arguments);
    attr += '';
    // Name updates should be propagated
    if (attr === 'name') {
        hostTreeStorage.getItem(host.getId()).then(function (treeHost) {
            treeHost.setValue(attr, val);
            hostTreeStorage.save();
        });
    }
}

// Utility function for getting predefined directory names
function getPredefinedDirectory (uname, type) {
    var dirs = {
        SunOS: {
            installdir: '/usr/local/bin/',
            datadir: '/var/lib/mysql-cluster/'
        },
        Linux: {
            installdir: '/usr/local/bin/',
            datadir: '/var/lib/mysql-cluster/'
        },
        CYGWIN: {
            installdir: 'C:\\Program Files\\MySQL\\',
            datadir: 'C:\\Program Data\\MySQL\\'
        },
        Windows: {
            installdir: 'C:\\Program Files\\MySQL\\',
            datadir: 'C:\\Program Data\\MySQL\\'
        },
        unknown: {
            installdir: '/usr/local/bin/',
            datadir: '/var/lib/mysql-cluster/'
        }
    };
    if (!uname || !dirs[uname]) {
        uname = 'unknown';
    }
    type += '';
    if (!type || (type !== 'installdir' && type !== 'datadir')) {
        type = 'installdir';
    }
    return dirs[uname][type];
}

// Set default values for installdir etc.
function setDefaultHostDirsUnlessOverridden (hostId, platform, flavor, fetchStatus) {
    hostStorage.getItem(hostId).then(function (host) {
        if (fetchStatus) {
            host.setValue('hwResFetch', fetchStatus);
        }
        platform += '';
        flavor += '';
        if (!host.getValue('installdir')) {
            // Check if Oracle Linux:
            if (platform === 'Linux' && flavor === 'ol') {
                host.setValue('installdir', '/usr/');
            } else {
                host.setValue('installdir', getPredefinedDirectory(platform, 'installdir'));
            }
            host.setValue('installdir_predef', true);
        }
        if (!host.getValue('datadir')) {
            host.setValue('datadir', getPredefinedDirectory(platform, 'datadir'));
            host.setValue('datadir_predef', true);
        }

        var repoDlUrl = '';
        var dockerDlUrl = '';
        var oldInstallCluster = 'NONE';
        var clusterStorage = mcc.storage.clusterStorage();
        clusterStorage.getItem(0).then(function (cluster) {
            oldInstallCluster = cluster.getValue('installCluster');
        });
        var oldOpenFW = mcc.gui.getOpenFW();
        console.debug('[DBG]Getting REPO/DOCKER details.');
        // Get OS details
        var ver = host.getValue('osver');
        console.debug('[DBG]OS details ' + flavor + ', ' + ver);
        if (ver) {
            var array = ver.split('.');
            ver = array[0]; // Take just MAJOR
        }
        console.debug('[DBG]Getting DOCKER details.');
        dockerDlUrl = mcc.util.getClusterDockerUrl();

        console.debug('[DBG]Getting REPO details.');
        switch (platform) {
            case 'WINDOWS':
                break;
            case 'CYGWIN':
                break;
            case 'DARWIN':
                break;
            case 'SunOS':
                break;
            case 'Linux':
                switch (flavor) {
                    case 'ol':
                        // OS=el
                        repoDlUrl = mcc.util.getClusterUrlRoot() + '-el' + ver + '.rpm';
                        // "http://repo.mysql.com/mysql-community-release-"+OS+ver+".rpm";
                        break;
                    case 'fedora':
                        // OS=fc
                        repoDlUrl = mcc.util.getClusterUrlRoot() + '-fc' + ver + '.rpm';
                        break;
                    case 'centos':
                        // OS=el
                        repoDlUrl = mcc.util.getClusterUrlRoot() + '-el' + ver + '.rpm';
                        break;
                    case 'rhel':
                        // OS=el
                        repoDlUrl = mcc.util.getClusterUrlRoot() + '-el' + ver + '.rpm';
                        break;
                    case 'opensuse':
                        // OS=sles
                        repoDlUrl = mcc.util.getClusterUrlRoot() + '-sles' + ver + '.rpm';
                        break;
                    // There is no "latest" for APT repo. Also, there is no way to discover newest
                    // so hard-coding for now.
                    case 'ubuntu': // from APT
                        // OS=ubuntu
                        repoDlUrl = 'http://repo.mysql.com/mysql-apt-config_0.8.7-1_all.deb';
                        break;
                    case 'debian': // from APT
                        // OS=debian
                        repoDlUrl = 'http://repo.mysql.com/mysql-apt-config_0.8.7-1_all.deb';
                        break;
                }
                break;
        }
        if (!host.getValue('openfwhost')) {
            host.setValue('openfwhost', oldOpenFW);
        }
        if (!host.getValue('installonhost')) {
            host.setValue('installonhost', oldInstallCluster !== 'NONE');
        }
        if (host.getValue('installonhostrepourl') === '') {
            host.setValue('installonhostrepourl', repoDlUrl);
        }
        if (host.getValue('installonhostdockerurl') === '') {
            host.setValue('installonhostdockerurl', dockerDlUrl);
        }
        console.debug('[DBG]REPO URL:' + repoDlUrl);
        console.debug('[DBG]DOCKER URL:' + dockerDlUrl);
        hostStorage.save();
    });
}

function isOPC (hostname) {
    // It is more likely Cloud cluster will be defined at ClusterLVL.
    // Maybe go through HOSTS and check that host.getValue("osflavor") == "ol" too.
    var clusterStorage = mcc.storage.clusterStorage();
    var res = false;
    clusterStorage.getItem(0).then(function (cluster) {
        if (cluster.getValue('ssh_keybased')) {
            if (cluster.getValue('ssh_ClKeyUser') === 'opc') {
                console.debug('[DBG]inside isOPC, ClusterCreds, res is TRUE');
                res = true;
            }
        }
    });
    if (res) {
        return res;
    } else {
        console.debug('[DBG]isOPC, host has creds?');
        mcc.storage.hostStorage().getItems({ name: hostname }).then(function (hosts) {
            if (hosts[0]) {
                if (hosts[0].getValue('key_auth')) {
                    // Remote host with its own credentials (PK).
                    if (hosts[0].getValue('key_usr') === 'opc') {
                        console.debug('[DBG]isOPC, host has creds, key_usr isOPC returning TRUE');
                        return true;
                    } else {
                        console.debug('[DBG]isOPC, no key_usr returning FALSE');
                        return false;
                    }
                } else {
                    console.debug('[DBG]isOPC, no key_auth returning FALSE');
                    return false;
                }
            }
        });
        // Neither Cluster nor Host creds confirm Oracle cloud deployment.
        console.debug('[DBG]isOPC, no creds anywhere, returning FALSE');
        return false;
    }
}

// Get host resource information
function getHostResourceInfo (hostName, hostId, showAlert, override) {
    hostName += '';
    if (hostName === 'LOCAL') {
        console.debug('getHostResourceInfo(LOCAL)');
        // Special case where we're just interested in data about host back end runs on.
        mcc.server.hostInfoReq('localhost', function (reply) {
            console.info('Hardware resources for ' + hostName + ': ' +
                    'ram = ' + reply.body.hostRes.ram +
                    ', cores = ' + reply.body.hostRes.cores +
                    ', uname = ' + reply.body.hostRes.uname +
                    ', installdir = ' + reply.body.hostRes.installdir +
                    ', datadir = ' + reply.body.hostRes.datadir +
                    ', diskfree = ' + reply.body.hostRes.diskfree +
                    ', OS = ' + reply.body.hostRes.osflavor +
                    ', OS ver = ' + reply.body.hostRes.osver +
                    ', home: ' + reply.body.hostRes.home);
        },
        function (errMsg, reply) {
            // Bail out if new request sent
            if (reply) {
                console.warn('Error fetching local info');
                return '';
            }
            return reply.body.hostRes.home;
        });
    }

    // First, get the host item and see if there are undefined values
    hostStorage.getItem(hostId).then(function (host) {
        var nm = host.getValue('name') + '';
        console.debug('[DBG]Running getHostResourceInfo for host ' + nm);
        // Basically, this IF is to ensure host record was empty.
        if (!host.getValue('uname') || !host.getValue('ram') ||
                !host.getValue('cores') || !host.getValue('installdir') ||
                !host.getValue('datadir') || override) {
            // There are undefined values, try a new fetch if requests allowed.
            // Set fetch status and clear previous values
            host.setValue('hwResFetch', 'Fetching...');
            if (host.getValue('installdir_predef')) {
                host.deleteAttribute('installdir');
            }
            if (host.getValue('datadir_predef')) {
                host.deleteAttribute('datadir');
            }

            console.debug('[DBG]Sending hostInfoReq for host ' + nm);
            var IntIP = 'unknown';
            mcc.server.hostInfoReq(nm, function (reply) {
                if ((nm === '127.0.0.1') || (nm.toLowerCase() === 'localhost')) {
                    IntIP = nm;
                } else {
                    var tmpIntIP = [];
                    console.debug('[DBG]]IntIP from body is: ' + reply.body.hostRes.intip);
                    try {
                        // If Linux, make response into array.
                        tmpIntIP = reply.body.hostRes.intip.split('\n');
                    } catch (e) {
                        // If Windows, response is already an array.
                        tmpIntIP = reply.body.hostRes.intip;
                    }
                    // IF there are more than 1, we can't guarantee it's the
                    // right one so we'll just log it for user to see.
                    if ((typeof tmpIntIP == 'string' || tmpIntIP instanceof String) && tmpIntIP.length > 7) {
                        if (mcc.util.ValidateIPAddress(tmpIntIP)) {
                            console.debug('[DBG]tmpIntIP validation OK, setting InternalIP address to ' + tmpIntIP + '.');
                            IntIP = tmpIntIP;
                        } else {
                            console.debug('[DBG]' + 'Returned InternalIP address ' + tmpIntIP + ' is not valid.');
                        }
                    } else {
                        // variable instanceof Array or Array.isArray(variable) ...
                        if (tmpIntIP.constructor === Array) {
                            if (!!window.MSInputMethodContext && !!document.documentMode) {
                                console.debug('[DBG]]' + tmpIntIP);
                            } else {
                                console.table(tmpIntIP);
                            }
                            for (var x = 0; x < tmpIntIP.length; x++) {
                                if (tmpIntIP[x].length < 7) {
                                    tmpIntIP.splice(x--, 1); // Remove empty entry, reindex.
                                }
                            }
                            if (tmpIntIP.length === 1) {
                                if (mcc.util.ValidateIPAddress(tmpIntIP[0])) {
                                    console.debug('[DBG]tmp IntIP validation OK.');
                                    IntIP = tmpIntIP[0];
                                } else {
                                    console.debug('[DBG]tmp IntIP validation failed.');
                                }
                            }
                        }
                    }
                }
                console.info('Hardware resources for ' + nm + ': ' +
                    'ram = ' + reply.body.hostRes.ram +
                    ', cores = ' + reply.body.hostRes.cores +
                    ', uname = ' + reply.body.hostRes.uname +
                    ', installdir = ' + reply.body.hostRes.installdir +
                    ', datadir = ' + reply.body.hostRes.datadir +
                    ', diskfree = ' + reply.body.hostRes.diskfree +
                    ', fqdn = ' + reply.body.hostRes.fqdn +
                    ', OS = ' + reply.body.hostRes.osflavor +
                    ', OS ver = ' + reply.body.hostRes.osver +
                    ', Docker status: ' + reply.body.hostRes.docker_info +
                    ', INTERNAL IP: ' + IntIP +
                    ', home: ' + reply.body.hostRes.home);
                // Bail out if new request sent
                if (Number(reply.head.rSeq) !== Number(host.getValue('hwResFetchSeq'))) {
                    console.debug('[DBG]Cancel reply to previous request(%s != %s)',
                        reply.head.rSeq, host.getValue('hwResFetchSeq'));
                    return;
                }
                // Delete error message
                host.deleteAttribute('errMsg');
                // Set resource info
                if (!host.getValue('internalIP') || override) {
                    if (mcc.util.ValidateIPAddress(IntIP)) {
                        host.setValue('internalIP', IntIP);
                    }
                }
                if (!host.getValue('ram') || override) {
                    host.setValue('ram', reply.body.hostRes.ram);
                }
                if (!host.getValue('cores') || override) {
                    host.setValue('cores', reply.body.hostRes.cores);
                }
                if (!host.getValue('uname') || override) {
                    host.setValue('uname', reply.body.hostRes.uname);
                }
                if (!host.getValue('osver') || override) {
                    host.setValue('osver', reply.body.hostRes.osver);
                }
                if (!host.getValue('osflavor') || override) {
                    host.setValue('osflavor', reply.body.hostRes.osflavor);
                }
                if (!host.getValue('dockerinfo') || override) {
                    host.setValue('dockerinfo', reply.body.hostRes.docker_info);
                }
                var path;
                if (!host.getValue('installdir') && reply.body.hostRes.installdir) {
                    path = mcc.util.terminatePath(reply.body.hostRes.installdir) + '';
                    if (mcc.util.isWin(host.getValue('uname'))) {
                        path = mcc.util.winPath(path);
                    } else {
                        path = mcc.util.unixPath(path);
                    }
                    host.setValue('installdir', path);
                    host.setValue('installdir_predef', true);
                }
                if (!host.getValue('datadir') && reply.body.hostRes.datadir) {
                    path = reply.body.hostRes.datadir.replace(/(\r\n|\n|\r)/gm, '');
                    path = mcc.util.terminatePath(path);
                    if (mcc.util.isWin(host.getValue('uname'))) {
                        path = mcc.util.winPath(path);
                    } else {
                        path = mcc.util.unixPath(path);
                    }
                    host.setValue('datadir', path);
                    host.setValue('datadir_predef', true);
                }
                if (!host.getValue('diskfree') || override) {
                    host.setValue('diskfree', reply.body.hostRes.diskfree);
                }
                if (!host.getValue('fqdn') || override) {
                    // Not possible to mix local and remote hosts any more.
                    // So, check if HOST is local and IF proper IP address is supplied.
                    // Then decide on value for FQDN.
                    if ((nm === '127.0.0.1') || (nm.toLowerCase() === 'localhost')) {
                        // Ignore FQDN for just localhost. Not important.
                        host.setValue('fqdn', nm);
                    } else {
                        // Was proper IP address provided already?
                        if (mcc.util.ValidateIPAddress(nm)) {
                            // Ignore FQDN for proper IP address.
                            host.setValue('fqdn', nm);
                        } else {
                            // Use whatever web server provided.
                            host.setValue('fqdn', reply.body.hostRes.fqdn);
                        }
                    }
                }

                console.debug('[DBG]isOPC(' + nm + ')');
                if (isOPC(nm)) {
                    if (!host.getValue('internalIP') && mcc.util.ValidateIPAddress(IntIP)) {
                        // Do not override internal IP.
                        console.debug('[DBG]OCIHost ' + nm + ' has no IntIP so setting IntIP to ' + IntIP);
                        host.setValue('internalIP', IntIP);
                    }
                }

                var clusterStorage = mcc.storage.clusterStorage();
                clusterStorage.getItem(0).then(function (cluster) {
                    var oldOpenFW = cluster.getValue('openfw');
                    var oldInstallCluster = cluster.getValue('installCluster');
                    if (!host.getValue('openfwhost') || override) {
                        host.setValue('openfwhost', oldOpenFW);
                    }
                    if (!host.getValue('installonhost') || override) {
                        host.setValue('installonhost', oldInstallCluster !== 'NONE');
                    }
                });
                if ((!host.getValue('installonhostrepourl') || !host.getValue('installonhostdockerurl'))) {
                    // Since this is NEW host, REPO/DOCKER details should be initialized.
                    console.debug('[DBG]Host ' + nm + ' has no URL settings, clearing.');
                    host.setValue('installonhostrepourl', '');
                    host.setValue('installonhostdockerurl', '');
                    host.setValue('installonhostdockernet', '');
                }
                host.setValue('hwResFetch', 'OK');
                // Set predefined OS specific install path and data dir
                setDefaultHostDirsUnlessOverridden(hostId, host.getValue('uname'), host.getValue('osflavor'), 'OK');
            },
            function (errMsg, reply) {
                // Bail out if new request sent
                if (reply) {
                    if (Number(reply.head.rSeq) !== Number(host.getValue('hwResFetchSeq'))) {
                        console.debug('[DBG]Cancel reply to previous request');
                        return;
                    }
                }

                // Update status, set default values
                setDefaultHostDirsUnlessOverridden(hostId, 'unknown', 'unknown', 'Failed');
                if (showAlert) {
                    mcc.util.displayModal('I', 2, '<span style="font-size:135%;color:orangered;">' +
                        'Could not obtain resource information for ' + nm + ': ' + errMsg +
                        '<br/>Please click the appropriate cell in the host definition page to ' +
                        'edit hardware resource information manually</span>');
                    console.warn('[WRN]Could not obtain resource information for ' + nm + ': ' + errMsg)
                }
                // Also save error message, can be viewed in the grid
                host.setValue('errMsg', errMsg);
                hostStorage.save();
            });
        } else {
            console.warn('Error fetching host info with override = %s!', override);
            return '';
        }
    });
}

// Get host Docker information. Not used ATM.
function getHostDockerInfo (hostName, hostId, showAlert, override) {
    // First, get the host item and see if there are undefined values
    hostStorage.getItem(hostId).then(function (host) {
        var nm = host.getValue('name') + '';
        console.debug('[DBG]Running getHostDockerInfo for host ' + nm);
        mcc.server.hostDockerReq(nm, function (reply) {
            console.debug('[DBG]Docker status for ' + nm + ': ' + reply.body.hostRes.docker_info);
            host.setValue('docker_info', reply.body.hostRes.docker_info);
            hostStorage.save();
        },
        function (errMsg, reply) {
            if (showAlert) {
                mcc.util.displayModal('I', 2, '<span style="font-size:135%;color:orangered;">' +
                    'Could not obtain Docker information for ' + hostName + ': ' + errMsg + '</span>');
            }
            console.warn('Could not obtain Docker information for ' + hostName + ': ' + errMsg);
            // Also save error message, can be viewed in the grid
            host.setValue('errMsg', errMsg);
            hostStorage.save();
        });
    });
}

// Special handling for new hosts: Get hw details and add to host tree storage
function newHostItem (item, showAlert, fetchRes) {
    this.inherited(arguments);
    var nm = item.name + '';
    console.debug('[DBG]New host name is ' + nm + ' and fetchRes is ' + fetchRes);
    if (!item.anyHost && nm !== '') {
        hostTreeStorage.newItem({ id: item.id, type: 'host', name: nm });
        hostTreeStorage.save();
    } else {
        hostTreeStorage.newItem({ id: item.id, type: 'anyHost', name: nm });
    }

    // Get hardware resources unless this is a wildcard host
    if (!item.anyHost && nm !== '') {
        if (fetchRes) {
            console.debug('[DBG]getHostResourceInfo for ' + nm + ' with override set.');
            getHostResourceInfo(nm, item.id, showAlert, true); // Only place where we have (..., TRUE, TRUE)
        } else {
            console.debug('[DBG]Skipped getHostResourceInfo for ' + nm + ' as requested.');
        }
    }
}

// Delete a host and its processes from the hostStorage and hostTreeStorage
function deleteHostItem (item) {
    var hostId = null;
    // Get host id depending on item type
    if (item.constructor === this.StorageItem) {
        hostId = item.getId();
    } else {
        hostId = hostStorage.store().getValue(item, 'id');
    }

    // Delete the host from hostStorage, save
    this.inherited(arguments);

    // Fetch the host tree item
    hostTreeStorage.getItem(hostId).then(function (treeHost) {
        // Delete the host from hostTreeStorage, save
        treeHost.deleteItem();
        // If selected in the host tree, reset
        if (mcc.gui.getCurrentHostTreeItem().storageItem &&
            treeHost.getId() === mcc.gui.getCurrentHostTreeItem().storageItem.getId()) {
            console.debug('[DBG]Deleting selected host, reset current host tree item');
            mcc.gui.resetHostTreeItem();
        }
    });

    // Fetch all processes for this host and delete them
    processStorage.forItems({ host: hostId }, function (process) {
        process.deleteItem();
    });
}

function getHostStorage () {
    if (!hostStorage) {
        console.debug('[DBG]No hostStorage, initializing.');
        hostStorage = new mcc.storage.Storage({
            name: 'Host storage',
            store: mcc.storage.stores.hostStore,
            setValue: setHostValue,
            newItem: newHostItem,
            deleteItem: deleteHostItem,
            getPredefinedDirectory: getPredefinedDirectory
        });
    }
    return hostStorage;
}

/***************************** Host tree storage ******************************/
function debugHostTree () {
    console.debug(this.name + ' contents:');
    var that = this;
    this.forItems({}, function (item) {
        hostStorage.ifItemId(item.getId(), function () {
            console.debug('   ' + item.getId() + ': host');
            var processes = item.getValues('processes');
            for (var i in processes) {
                console.debug('    + ' + that.store().getIdentity(processes[i]));
            }
        });
    });
}

function getHostTreeStorage () {
    if (!hostTreeStorage) {
        hostTreeStorage = new mcc.storage.Storage({
            name: 'Host tree storage',
            store: mcc.storage.stores.hostTreeStore,
            debug: debugHostTree
        });
    }
    return hostTreeStorage;
}

/********************************* Initialize *********************************/
// Initialize nextId based on storage contents
function initializeStorageId () {
    mcc.storage.Storage.prototype.statics.nextId = 0;
    var waitList = [new dojo.Deferred(), new dojo.Deferred(), new dojo.Deferred()];
    var waitCondition = new dojo.DeferredList(waitList);

    function updateId (waitCondition) {
        return function (items) {
            for (var i in items) {
                if (items[i].getId() >= mcc.storage.Storage.prototype.statics.nextId) {
                    mcc.storage.Storage.prototype.statics.nextId = items[i].getId() + 1;
                }
            }
            waitCondition.resolve();
        };
    }

    processTypeStorage.getItems().then(updateId(waitList[0]));
    processStorage.getItems().then(updateId(waitList[1]));
    hostStorage.getItems().then(updateId(waitList[2]));

    waitCondition.then(function () {
        console.debug('[DBG]Initialized storage id to: ' + mcc.storage.Storage.prototype.statics.nextId);
    });
    return waitCondition;
}

// Add wildcard host if host storage is empty
function initializeHostStorage () {
    var waitCondition = new dojo.Deferred();
    hostStorage.getItems().then(function (items) {
        if (!items || items.length === 0) {
            console.debug('[DBG]Add wildcard host');
            hostStorage.newItem({ name: 'Any host', anyHost: true });
            hostStorage.save();
        }
        waitCondition.resolve();
    });
    return waitCondition;
}

// Re-create host tree based on hosts and processes
function initializeHostTreeStorage () {
    var waitCondition = new dojo.Deferred();
    hostStorage.forItems({}, function (host) {
        hostTreeStorage.newItem({
            id: host.getId(),
            type: host.getValue('anyHost') ? 'anyHost' : 'host',
            name: host.getValue('name')
        });
        // Get the recently added host tree item
        hostTreeStorage.getItem(host.getId(host)).then(function (treeitem) {
            processStorage.forItems({ host: +host.getId() }, function (process) {
                hostTreeStorage.newItem({
                    id: process.getId(),
                    type: 'process',
                    name: process.getValue('name')
                }, { parent: treeitem.item, attribute: 'processes' });
            });
        });
    },
    function (items, request) {
        hostTreeStorage.save();
        console.debug('[DBG]Re-created host tree');
        hostTreeStorage.debug();
        waitCondition.resolve();
    });
    return waitCondition;
}

// Re-create process tree based on process types and processes
function initializeProcessTreeStorage () {
    var waitCondition = new dojo.Deferred();
    processTypeStorage.forItems({}, function (ptype) {
        mcc.util.assert(ptype, 'No process type');
        // Add item unless family already existing
        processTreeStorage.getItems({ name: ptype.getValue('familyLabel') }).then(function (treeitems) {
            var treeitem = treeitems[0];
            if (!treeitem || !processTreeStorage.isItem(treeitem)) {
                processTreeStorage.newItem({
                    id: ptype.getId(),
                    type: 'processtype',
                    name: ptype.getValue('familyLabel')
                });
            }
            // Fetch the appropriate item
            processTreeStorage.getItems({ name: ptype.getValue('familyLabel') }).then(function (treeitems) {
                var treeitem = treeitems[0];
                processStorage.forItems({ processtype: ptype.getId(ptype) }, function (process) {
                    processTreeStorage.newItem({
                        id: +process.getId(),
                        type: 'process',
                        name: process.getValue('name')
                    },
                    {
                        parent: treeitem.item,
                        attribute: 'processes'
                    });
                });
            });
        });
    },
    function () {
        processTreeStorage.save();
        console.debug('[DBG]Re-created process tree');
        processTreeStorage.debug();
        waitCondition.resolve();
    });
    return waitCondition;
}

// Add default cluster processes if necessary
function initializeProcessTypeStorage () {
    var waitCondition = new dojo.Deferred();

    processTypeStorage.getItems().then(function (processTypes) {
        if (processTypes.length === 0) {
            processTypeStorage.newItem({
                name: 'ndb_mgmd',
                family: 'management',
                familyLabel: 'Management layer',
                nodeLabel: 'Management node',
                minNodeId: 49,
                maxNodeId: 50,
                currSeq: 1
            });
            processTypeStorage.newItem({
                name: 'ndbd',
                family: 'data',
                familyLabel: 'Data layer',
                nodeLabel: 'Single threaded data node',
                minNodeId: 1,
                maxNodeId: 48,
                currSeq: 1
            });
            processTypeStorage.newItem({
                name: 'ndbmtd',
                family: 'data',
                familyLabel: 'Data layer',
                nodeLabel: 'Multi threaded data node',
                minNodeId: 1,
                maxNodeId: 48,
                currSeq: 1
            });
            processTypeStorage.newItem({
                name: 'mysqld',
                family: 'sql',
                familyLabel: 'SQL layer',
                nodeLabel: 'SQL node',
                minNodeId: 53,
                maxNodeId: 230,
                currSeq: 1
            });
            processTypeStorage.newItem({
                name: 'api',
                family: 'api',
                familyLabel: 'API layer',
                nodeLabel: 'API node',
                minNodeId: 231,
                maxNodeId: 255,
                currSeq: 1
            });
        } else {
            console.debug('[DBG]Process types already exist, not adding defaults');
        }
        waitCondition.resolve();
    });
    return waitCondition;
}

// Coordinate all initialization, return wait condition
function initialize () {
    var initId = initializeStorageId().then(initializeProcessTypeStorage);

    return new dojo.DeferredList([
        initId.then(initializeProcessTreeStorage),
        initId.then(initializeHostTreeStorage).then(initializeHostStorage)
    ]);
}

/********************************* Initialize *********************************/
dojo.ready(function () {
    getClusterStorage();
    getProcessTypeStorage();
    getProcessStorage();
    getProcessTreeStorage();
    getHostStorage();
    getHostTreeStorage();
    console.info('[INF]MCC storage class module initialized');
});
