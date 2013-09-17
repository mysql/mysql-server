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
 ***                           Generic storage class                        ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.storage.Storage
 *
 *  Description:
 *      Generic storage class wrapping an ItemFileWriteStore
 *
 *  External interface: 
 *      mcc.storage.Storage: Constructor
 *      debug: Report contents
 *      getNextId: Return next available id, increment nextId
 *      setValue: Handle setValue here for easy update propagation to trees
 *      setValues: Handle setValues here for easy update propagation to trees
 *      getItem: Deferred-based item access by id
 *      getItems: Deferred-based item access by query
 *      forItems: Callback-based item access by query
 *      ifItemId: Callback-based id test
 *      isItem: Does item belong in storage?
 *      newItem: Add new item and save
 *      deleteItem: Delete item and save
 *      deleteStorage: Delete all items and save
 *      save: Save store
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      None
 *
 *  Internal data: 
 *      statics.nextId: Id of next store item, shared by all storages
 *      StorageItem: Constructor for storage items
 *      store: Reference to itemFileWriteStore holding data
 *      name: Name used for debugging basically
 *
 *  Unit test interface: 
 *      None
 *
 *  Todo:
 *      Rewrite forItems to take item function as parameter and return deferred
 *      Possibly remove or rewrite ifItemId
 *      Implement unit tests.
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.storage.Storage");

dojo.require("mcc.util");
dojo.require("mcc.storage.StorageItem");

/****************************** Implementation  *******************************/

/************************** Generic storage class *****************************/

dojo.declare("mcc.storage.Storage", null, {
    statics: {nextId: 0},     // Shared data
    StorageItem: mcc.storage.StorageItem,
    store: null,
    name: "Storage", 
    constructor: function (args) {
        dojo.safeMixin(this, args);
        this.debug();
    },
    // Report contents
    debug: function () {
        mcc.util.dbg(this.name + " contents:");
        this.forItems({}, function (item) {
            mcc.util.dbg("   " + item.getId() + ": " + item.getValue("name"));
        });  
    },
    // Return next available id, increment nextId
    getNextId: function () {
        return this.statics.nextId++;
    },
    // Handle setValue here for easy update propagation to trees by overriding
    setValue: function (item, attr, val) {
        this.store().setValue(item.item, attr, val);
    },
    // Handle setValues here for easy update propagation to trees by overriding
    setValues: function (item, attr, val) {
        this.store().setValues(item.item, attr, val);
    },
    // Deferred-based item access by id
    getItem: function (id) {
        var waitCondition = new dojo.Deferred();
        var that = this; // Make this available in callback
        this.store().fetchItemByIdentity({
            identity: id,
            onItem: function (item) {
                if (item) {
                    waitCondition.resolve(new that.StorageItem(that, item));
                } else {
                    waitCondition.resolve(null);
                }
            },
            onError: function (error) {
                waitCondition.reject(error);
            }
        });
        return waitCondition;
    },
    // Deferred-based item access by query
    getItems: function (query) {
        var waitCondition = new dojo.Deferred();
        var that = this; // Make this available in callback
        this.store().fetch({
            query: query,
            onComplete: function (items, request) {
                var storageItems = [];
                for (var i in items) {
                    storageItems[i] = new that.StorageItem(that, items[i]);
                }
                waitCondition.resolve(storageItems);
            },
            onError: function (error) {
                waitCondition.reject(error);
            }
        });
        return waitCondition;
    },
    // Callback-based item access by query
    forItems: function (query, onItem, onComplete, onError) {
        var that = this; // Make this available in callback
        this.store().fetch({
            query: query,
            onItem: function (item, request) {
                if (item) {
                    onItem(new that.StorageItem(that, item));
                } else {
                    onItem(null);
                }
            },
            onComplete: onComplete,
            onError: onError
        });
    },
    // Callback-based id test
    ifItemId: function (id, onTrue, onFalse, onError) {
        var that = this; // Make this available in callback
        this.getItem(id).then(function (item) {
            if (that.isItem(item)) {
                onTrue();
            } else {
                onFalse();
            }
        },
        onError);
    },
    // Does item belong in storage?
    isItem: function (item) {
        var realItem = item;
        if (!realItem) return false;
        // If item is a storage item, get store item
        if (item.constructor == this.StorageItem) {
            realItem = item.item;
        }
        return this.store().isItem(realItem);
    },
    // Add new item and save
    newItem: function (object, parent) {
        // Add id member if it does not exist, increment nextId
        if (!object.hasOwnProperty("id")) {
            object.id = this.getNextId();
        }
        mcc.util.dbg(this.name + ": add id=" + object.id)
        this.store().newItem(object, parent);
        this.save();
    },
    // Delete item and save
    deleteItem: function (item) {
        var realItem = item;
        // If item is a storage item, get store item
        if (item.constructor == this.StorageItem) {
            realItem = item.item;
        }
        mcc.util.dbg(this.name + ": delete id=" + realItem.id)
        this.store().deleteItem(realItem);
        this.save();
    },
    // Delete all items and save
    deleteStorage: function () {
        var waitCondition = new dojo.Deferred();
        this.forItems({}, function (item) {
            item.deleteItem();
        },
        function () {
            waitCondition.resolve(true);
        },
        function (error) {
            waitCondition.reject(error);
        });
        return waitCondition;
    },
    // Save store
    save: function () {
        this.store().save();
    }
});

/******************************* Initialize ***********************************/

dojo.ready(function () {
    mcc.util.dbg("Storage class module initialized");
});


