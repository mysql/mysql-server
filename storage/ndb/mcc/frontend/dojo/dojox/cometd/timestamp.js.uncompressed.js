//>>built
// wrapped by build app
define("dojox/cometd/timestamp", ["dijit","dojo","dojox","dojo/require!dojox/cometd/_base"], function(dijit,dojo,dojox){
dojo.provide("dojox.cometd.timestamp");
dojo.require("dojox.cometd._base");

// A cometd extension that adds a timestamp to every message
dojox.cometd._extendOutList.push(function(msg){
	msg.timestamp = new Date().toUTCString();
	return msg
});

});
