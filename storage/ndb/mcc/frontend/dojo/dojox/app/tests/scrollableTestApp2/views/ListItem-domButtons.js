define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect", "dijit/registry", "dojox/mvc/at", "dojox/mobile/TransitionEvent", "dojox/mvc/Repeat", 
		"dojox/mvc/getStateful", "dojox/mvc/Output", "dojo/sniff"],
function(dom, domStyle, connect, registry, at, TransitionEvent, Repeat, getStateful, Output, has){
	var _connectResults = []; // events connect result


	return {
		// repeat view init
		init: function(){
		},


		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			if(dom.byId("mli1back") && !has("phone")){ 
				domStyle.set(dom.byId("mli1back"), "visibility", "hidden"); // hide the back button in tablet mode
			}
			if(dom.byId("tab1WrapperA")){ 
				domStyle.set(dom.byId("tab1WrapperA"), "visibility", "visible");  // show the nav view if it being used
				domStyle.set(dom.byId("tab1WrapperB"), "visibility", "visible");  // show the nav view if it being used
			}
		},
		
		
		// view destroy
		destroy: function(){
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		}
	};
});
