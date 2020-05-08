define(["dojo/dom", "dojo/_base/connect", "dijit/registry", "dojo/dom-style"],
function(dom, connect, registry, domStyle){
	var _connectResults = []; // events connect results
	return {
		// simple view init
		init: function(){
			var dir = domStyle.get(this.domNode,"direction");
			dom.byId("headerH1").innerHTML = "APP HEADER dir = "+dir;
		}
	};
});
