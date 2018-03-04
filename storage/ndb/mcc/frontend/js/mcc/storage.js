/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

