define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect","dijit/registry", "dojo/sniff", "dojox/mobile/TransitionEvent"],
function(dom, domStyle, connect, registry, has, TransitionEvent){
		var _connectResults = []; // events connect result
		var backId = 'ti1back1';
		var wrapperIdB = 'tst1WrapperB';
		var MODULE = "TestInfo";
	return {
		init: function(){			
		},


		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			if(registry.byId("heading1")){
				registry.byId("heading1").labelDivNode.innerHTML = "Test Information";
			}
			if(dom.byId(backId) && !has("phone")){ 
				domStyle.set(dom.byId(backId), "visibility", "hidden"); // hide the back button in tablet mode
			}
			if(dom.byId("tab1WrapperB") && !has("phone")){ 
				domStyle.set(dom.byId("tab1WrapperB"), "visibility", "visible");  // show the nav view if it being used
			}
			domStyle.set(dom.byId(wrapperIdB), "visibility", "visible");  // show the view when it is ready
		},

		afterActivate: function(){
			// summary:
			//		view life cycle afterActivate()
			if(!this.app.timedAutoFlow){
				return;
			}
			this.app.loopCount++;
			//console.log(MODULE+" afterActivate this.app.loopCount="+this.app.loopCount);
			if(this.app.loopCount === 16){
				console.log("TestInfo:afterActivate loopCount = 15 stop timer");
				console.timeEnd("timing transition loop");
			}
		},
		
		
		destroy: function(){
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		}
	};
});
