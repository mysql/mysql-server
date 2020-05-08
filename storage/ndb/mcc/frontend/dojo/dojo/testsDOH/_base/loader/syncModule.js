if (typeof dojoCdnTestLog=="undefined"){
	dojoCdnTestLog= [];
}
dojoCdnTestLog.push("in-dojo.testsDOH._base.loader.syncModule");
dojo.provide("dojo.testsDOH._base.loader.syncModule");
dojo.declare("dojo.testsDOH._base.loader.syncModule", null, {});
dojo.testsDOH._base.loader.syncModule.status= "OK";
dojo.require("dojo.testsDOH._base.loader.syncModuleDep");
dojoCdnTestLog.push("out-dojo.testsDOH._base.loader.syncModule");
