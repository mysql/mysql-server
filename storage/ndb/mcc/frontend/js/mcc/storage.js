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
 ***              External interface wrapper for MCC storage                ***
 ***                                                                        ***
 ******************************************************************************/

dojo.provide("mcc.storage");

/***************************** Store utilities ********************************/

dojo.require("mcc.storage.stores");

mcc.storage.initializeStores = mcc.storage.stores.initialize;
mcc.storage.setupSaveExtensions = mcc.storage.stores.setupSaveExtensions;
mcc.storage.resetSaveExtensions = mcc.storage.stores.resetSaveExtensions;

/***************************** Storage objects ********************************/

dojo.require("mcc.storage.MCCStorage");

mcc.storage.initializeStorage = mcc.storage.MCCStorage.initialize;
mcc.storage.clusterStorage = mcc.storage.MCCStorage.clusterStorage;
mcc.storage.processTypeStorage = mcc.storage.MCCStorage.processTypeStorage;
mcc.storage.processStorage = mcc.storage.MCCStorage.processStorage;
mcc.storage.processTreeStorage = mcc.storage.MCCStorage.processTreeStorage;
mcc.storage.getHostResourceInfo = mcc.storage.MCCStorage.getHostResourceInfo;
mcc.storage.hostStorage = mcc.storage.MCCStorage.hostStorage;
mcc.storage.hostTreeStorage = mcc.storage.MCCStorage.hostTreeStorage;

/******************************** Initialize  *********************************/

dojo.ready(function() {
    mcc.util.dbg("Storage module initialized");
});

