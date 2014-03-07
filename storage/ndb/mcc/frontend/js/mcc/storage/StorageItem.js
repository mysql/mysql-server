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
 ***                       Generic storage item class                       ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.storage.StorageItem
 *
 *  Description:
 *      Generic storage item class containing store item and storage ref
 *
 *  External interface: 
 *      mcc.storage.StorageItem: Constructor
 *      isType: Check type attribute
 *      setValue: Set value of an attribute. Delegate to storage
 *      setValues: Set value of a multi valued attribute
 *      getValue: Get attribute value of item
 *      getValues: Get multi valued attribute of item
 *      getId: Get id of an item
 *      deleteAttribute: Delete attribute of an item
 *      delete: Delete the item itself by delegation to storage
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
 *
 *  Internal data: 
 *      item: The store item
 *      storage: Reference to the storage instance
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.storage.StorageItem");

dojo.require("mcc.util");

/****************************** Implementation  *******************************/

dojo.declare("mcc.storage.StorageItem", null, {
    item: null,
    storage: null,
    // Tuck away store item and storage object reference
    constructor: function (storage, item) {
        this.storage = storage; 
        this.item = item; 
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
    },
    // Check type attribute
    isType: function(type) {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        var itemType = this.getValue("type");
        return (itemType && itemType == type);
    },
    // Set value of an attribute. Delegate to storage in order to catch updates
    setValue: function (attr, val) {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        this.storage.setValue(this, attr, val);
    },
    // Set value of a multi valued attribute
    setValues: function (attr, val) {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        this.storage.setValues(this, attr, val);
    },
    // Get attribute value of item
    getValue: function (attr) {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        return this.storage.store().getValue(this.item, attr);
    },
    // Get multi valued attribute of item
    getValues: function (attr) {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        return this.storage.store().getValues(this.item, attr);
    },
    // Get id of an item
    getId: function () {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        return this.storage.store().getIdentity(this.item);
    },
    // Delete attribute of an item
    deleteAttribute: function (attr) {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        this.storage.store().unsetAttribute(this.item, attr);
    },
    // Delete the item itself by delegation to storage
    deleteItem: function () {
        mcc.util.assert(this.storage, "No storage");
        mcc.util.assert(this.item, "No item");
        this.storage.deleteItem(this.item);
    }
});

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("Storage Item class module initialized");
});


