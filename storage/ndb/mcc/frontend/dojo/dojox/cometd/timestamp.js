//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/cometd/_base"],function(_1,_2,_3){
_2.provide("dojox.cometd.timestamp");
_2.require("dojox.cometd._base");
_3.cometd._extendOutList.push(function(_4){
_4.timestamp=new Date().toUTCString();
return _4;
});
});
