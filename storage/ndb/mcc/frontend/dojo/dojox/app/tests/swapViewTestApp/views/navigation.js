define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect", "dijit/registry"], function(dom, domStyle, connect, registry){
	var _connectResults = []; // events connect result

	return {
		// view init
		init: function(){
		},
		
		beforeActivate: function(view){
			// summary:
			//		view life cycle beforeActivate()
			//
			this.previousView = view;
			
			// setup code to watch for the navigation pane being visible
			
		},

		beforeDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
		},

		nextClicked: function(/*Event*/ e){
			var activeView = registry.byId("swap1").getShowingView();
			var next = activeView.nextView(activeView.domNode);
			activeView.goTo(1, next);
		},

		previousClicked: function(/*Event*/ e){
			var activeView = registry.byId("swap1").getShowingView();
			var prev = activeView.previousView(activeView.domNode);
			activeView.goTo(-1, prev);
		},

		swap1Clicked: function(/*Event*/ e){
			var activeView = registry.byId("swap1").getShowingView();
			activeView.goTo(-1, "swap1");
		},

		swap2Clicked: function(/*Event*/ e){
			var activeView = registry.byId("swap1").getShowingView();
			var dir = 1;
			if(activeView.id == "swap3"){
				dir = -1;
			}
			activeView.goTo(dir, "swap2");
		//	we could have used .show(), and pageIndicator reset() to do the same thing
		//	registry.byId("swap2").show();
		//	registry.byId("pageIndicatorId").reset();
		},

		swap3Clicked: function(/*Event*/ e){
			var activeView = registry.byId("swap1").getShowingView();
			activeView.goTo(1, "swap3");
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
