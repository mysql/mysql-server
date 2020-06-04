// wrapped by build app
define("dojox/storage/_common", ["dojo","dijit","dojox","dojo/require!dojox/storage/Provider,dojox/storage/manager,dojox/storage/LocalStorageProvider,dojox/storage/GearsStorageProvider,dojox/storage/WhatWGStorageProvider,dojox/storage/FlashStorageProvider,dojox/storage/BehaviorStorageProvider,dojox/storage/CookieStorageProvider"], function(dojo,dijit,dojox){
dojo.provide("dojox.storage._common");
dojo.require("dojox.storage.Provider");
dojo.require("dojox.storage.manager");

/*
  Note: if you are doing Dojo Offline builds you _must_
  have offlineProfile=true when you run the build script:
  ./build.sh action=release profile=offline offlineProfile=true
*/
dojo.require("dojox.storage.LocalStorageProvider");
dojo.require("dojox.storage.GearsStorageProvider");
dojo.require("dojox.storage.WhatWGStorageProvider");
dojo.require("dojox.storage.FlashStorageProvider");
dojo.require("dojox.storage.BehaviorStorageProvider");
dojo.require("dojox.storage.CookieStorageProvider");

// now that we are loaded and registered tell the storage manager to
// initialize itself
dojox.storage.manager.initialize();

});
