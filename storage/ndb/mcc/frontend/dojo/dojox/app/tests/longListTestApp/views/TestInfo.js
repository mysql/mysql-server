define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect", "dojo/sniff"],
function(dom, domStyle, connect, has){
		var _connectResults = []; // events connect result
		var backId = 'ti1back1';
		var wrapperIdB = 'tst1WrapperB';
	return {
		init: function(){			
		},


		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
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
